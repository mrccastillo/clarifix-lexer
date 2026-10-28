#include "lexer.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ERROR_LIMIT 5

typedef struct { const char *word; CfTokenType type; int noise; } Fixed;
static const Fixed fixed[] = {
    {"set", CF_SET, 0}, {"show", CF_SHOW, 0}, {"ask", CF_ASK, 0},
    {"if", CF_IF, 0}, {"elif", CF_ELIF, 0}, {"else", CF_ELSE, 0},
    {"while", CF_WHILE, 0}, {"for", CF_FOR, 0}, {"in", CF_IN, 0},
    {"function", CF_FUNCTION, 0}, {"return", CF_RETURN, 0},
    {"break", CF_BREAK, 0}, {"continue", CF_CONTINUE, 0},
    {"and", CF_AND, 0}, {"or", CF_OR, 0}, {"not", CF_NOT, 0}, {"is", CF_IS, 0},
    {"to", CF_NOISE, 1}, {"the", CF_NOISE, 3}, {"of", CF_NOISE, 3}, {"then", CF_NOISE, 2},
    {"+", CF_PLUS, 0}, {"-", CF_MINUS, 0}, {"*", CF_STAR, 0}, {"/", CF_SLASH, 0},
    {"//", CF_FLOOR_DIV, 0}, {"%", CF_MOD, 0}, {"^", CF_POWER, 0},
    {"++", CF_INCREMENT, 0}, {"--", CF_DECREMENT, 0},
    {"&&", CF_AND, 0}, {"||", CF_OR, 0}, {"!", CF_NOT, 0},
    {"==", CF_EQ, 0}, {"!=", CF_NE, 0}, {"<", CF_LT, 0}, {">", CF_GT, 0},
    {"<=", CF_LE, 0}, {">=", CF_GE, 0}, {"=", CF_ASSIGN, 0},
    {"+=", CF_PLUS_ASSIGN, 0}, {"-=", CF_MINUS_ASSIGN, 0},
    {"*=", CF_STAR_ASSIGN, 0}, {"/=", CF_SLASH_ASSIGN, 0},
    {"%=", CF_MOD_ASSIGN, 0}, {"^=", CF_POWER_ASSIGN, 0},
    {"(", CF_LPAREN, 0}, {")", CF_RPAREN, 0}, {"[", CF_LBRACKET, 0},
    {"]", CF_RBRACKET, 0}, {"{", CF_LBRACE, 0}, {"}", CF_RBRACE, 0},
    {",", CF_COMMA, 0}, {".", CF_PERIOD, 0}, {":", CF_COLON, 0}
};

typedef struct { char close; size_t offset, line, column; } Bracket;
/* Reserved/operator acceptance is separate from dynamic symbol identity. */
typedef struct { unsigned char c; size_t child, sibling, fixed; int boolean; } Node;
typedef struct {
    const char *s;
    size_t length, pos, line, column;
    size_t indent_width, statement;
    int physical_started, logical_started, indent_tab, braces;
    size_t *indents, indent_count, indent_capacity;
    Bracket *brackets;
    size_t bracket_count, bracket_capacity;
    Node *nodes;
    size_t node_count, node_capacity;
    FILE *diagnostics;
    CfResult *r;
} Lexer;

static void *resize(void *p, size_t count, size_t item_size)
{
    void *q;
    if (count > SIZE_MAX / item_size) {
        fputs("ClariFix: allocation is too large.\n", stderr);
        exit(2);
    }
    q = realloc(p, count * item_size);
    if (!q) {
        fputs("ClariFix: out of memory.\n", stderr);
        exit(2);
    }
    return q;
}

#define GROW(ptr, count, capacity) do { \
    if ((count) == (capacity)) { \
        size_t new_capacity = (capacity) ? (capacity) * 2 : 32; \
        if (new_capacity < (capacity)) exit(2); \
        (ptr) = resize((ptr), new_capacity, sizeof(*(ptr))); \
        (capacity) = new_capacity; \
    } \
} while (0)

const char *cf_token_name(CfTokenType type)
{
    static const char *names[] = {
#define CF_NAME(name) #name,
        CF_TOKEN_TYPES(CF_NAME)
#undef CF_NAME
    };
    return (unsigned)type < CF_TOKEN_COUNT ? names[type] : "UNKNOWN";
}

static int letter(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
static int digit(unsigned char c) { return c >= '0' && c <= '9'; }
static unsigned char peek(const Lexer *l, size_t ahead)
{
    return ahead < l->length - l->pos ? (unsigned char)l->s[l->pos + ahead] : 0;
}
static unsigned char advance(Lexer *l)
{
    unsigned char c = (unsigned char)l->s[l->pos++];
    if (c == '\n') {
        l->line++;
        l->column = 1;
        l->physical_started = 0;
        l->indent_width = 0;
        l->indent_tab = 0;
    } else l->column++;
    return c;
}

static size_t transition(const Lexer *l, size_t state, unsigned char c)
{
    for (size_t i = l->nodes[state].child; i; i = l->nodes[i].sibling)
        if (l->nodes[i].c == c) return i;
    return 0;
}
static size_t extend(Lexer *l, size_t state, unsigned char c)
{
    size_t next = transition(l, state, c);
    if (next) return next;
    GROW(l->nodes, l->node_count, l->node_capacity);
    next = l->node_count++;
    l->nodes[next] = (Node){c, 0, l->nodes[state].child, 0, 0};
    l->nodes[state].child = next;
    return next;
}
static size_t utf8_span(const char *s, size_t n);
static char *copy_text(const char *s, size_t n)
{
    char *text = resize(NULL, n + 1, 1);
    memcpy(text, s, n); text[n] = '\0';
    return text;
}
static size_t scalar_count(const char *s, size_t n)
{
    size_t count = 0;
    for (size_t i = 0; i < n; count++) {
        size_t bytes = utf8_span(s + i, n - i);
        i += bytes ? bytes : 1;
    }
    return count;
}
static char *constant_value(const char *s, size_t n, CfTokenType type)
{
    if (type == CF_IDENT) return NULL;
    if (type == CF_INT || type == CF_DECIMAL) {
        size_t first = 0, end = n;
        while (first + 1 < n && s[first] == '0' && s[first + 1] != '.') first++;
        if (type == CF_DECIMAL) {
            while (end > first && s[end - 1] == '0') end--;
            if (s[end - 1] == '.') end++; /* An integral decimal retains .0. */
        }
        return copy_text(s + first, end - first);
    }
    if (type == CF_BOOL) return copy_text(s, n);
    char *value = resize(NULL, n + 1, 1);
    size_t dest = 0;
    for (size_t i = 1; i + 1 < n; i++) {
        char c = s[i];
        if (c == '\\') { c = s[++i]; if (c == 'n') c = '\n'; }
        value[dest++] = c;
    }
    value[dest] = '\0';
    return value;
}
static size_t symtab_insert(Lexer *l, const char *s, size_t n, CfTokenType type,
                            size_t line, size_t column)
{
    size_t state = 0;
    uint64_t hash = 5381;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        state = extend(l, state, c);
        hash = hash * UINT64_C(33) + c;
    }
    CfResult *r = l->r;
    size_t bucket = (size_t)(hash % CF_SYMBOL_BUCKETS);
    for (size_t id = r->buckets[bucket]; id; id = r->symbols[id - 1].next) {
        CfSymbol *entry = &r->symbols[id - 1];
        if (entry->identity == state && entry->type == type) {
            entry->occurrences++;
            return id;
        }
    }
    GROW(r->symbols, r->symbol_count, r->symbol_capacity);
    CfSymbol *symbol = &r->symbols[r->symbol_count++];
    memset(symbol, 0, sizeof(*symbol));
    symbol->lexeme = copy_text(s, n);
    symbol->type = type;
    symbol->value = constant_value(s, n, type);
    symbol->length = scalar_count(s, n);
    symbol->line = line; symbol->column = column; symbol->occurrences = 1;
    symbol->identity = state;
    symbol->next = r->buckets[bucket];
    r->buckets[bucket] = r->symbol_count;
    return r->symbol_count;
}
static void report(Lexer *l, CfTokenType type, size_t start, size_t n,
                   size_t line, size_t col, size_t symbol)
{
    CfResult *r = l->r;
    GROW(r->report, r->report_count, r->report_capacity);
    r->report[r->report_count++] = (CfToken){type, start, n, line, col, symbol};
}
static void emit_parser(Lexer *l, CfTokenType type, size_t start, size_t n,
                 size_t line, size_t col, size_t symbol)
{
    CfResult *r = l->r;
    GROW(r->tokens, r->token_count, r->token_capacity);
    r->tokens[r->token_count++] = (CfToken){type, start, n, line, col, symbol};
    if (type == CF_PERIOD || type == CF_NEWLINE || type == CF_LBRACE || type == CF_RBRACE)
        l->statement = r->token_count;
}
static void emit(Lexer *l, CfTokenType type, size_t start, size_t n,
                 size_t line, size_t col, size_t symbol)
{
    if (type == CF_IDENT || (type >= CF_INT && type <= CF_BOOL))
        symbol = symtab_insert(l, l->s + start, n, type, line, col);
    emit_parser(l, type, start, n, line, col, symbol);
    if (n) report(l, type, start, n, line, col, symbol);
}
static void structural(Lexer *l, CfTokenType type)
{
    emit(l, type, l->pos, 0, l->line, l->column, 0);
}

/* Zero means an invalid UTF-8 byte; ASCII (including NUL) has length one. */
static size_t utf8_span(const char *s, size_t n)
{
    unsigned char a = n ? (unsigned char)s[0] : 0;
    unsigned char b = n > 1 ? (unsigned char)s[1] : 0;
    unsigned char c = n > 2 ? (unsigned char)s[2] : 0;
    unsigned char d = n > 3 ? (unsigned char)s[3] : 0;
    if (!n) return 0;
    if (a < 128) return 1;
    if (a >= 0xc2 && a <= 0xdf && b >= 0x80 && b <= 0xbf) return 2;
    if (a >= 0xe0 && a <= 0xef && b >= 0x80 && b <= 0xbf && c >= 0x80 && c <= 0xbf &&
        !(a == 0xe0 && b < 0xa0) && !(a == 0xed && b >= 0xa0)) return 3;
    if (a >= 0xf0 && a <= 0xf4 && b >= 0x80 && b <= 0xbf && c >= 0x80 && c <= 0xbf &&
        d >= 0x80 && d <= 0xbf && !(a == 0xf0 && b < 0x90) && !(a == 0xf4 && b >= 0x90)) return 4;
    return 0;
}

static void error_at(Lexer *l, int code, size_t offset, size_t line, size_t col,
                     const char *what, const char *why, const char *fix)
{
    if (l->r->errors >= ERROR_LIMIT) return;
    size_t begin = offset, end = offset;
    while (begin && l->s[begin - 1] != '\n') begin--;
    while (end < l->length && l->s[end] != '\n') end++;
    size_t bad_start = offset, bad_end = offset;
    if (code == 107 || code == 108) {
        bad_start = begin; bad_end = begin;
        while (bad_end < end && (l->s[bad_end] == ' ' || l->s[bad_end] == '\t')) bad_end++;
    } else if (code == 104 || code == 105 || code == 110) {
        while (bad_end < end && (letter((unsigned char)l->s[bad_end]) ||
               digit((unsigned char)l->s[bad_end]) ||
               (code == 110 && l->s[bad_end] == '.' && bad_end + 1 < end && digit((unsigned char)l->s[bad_end + 1])))) bad_end++;
    } else if (code == 106) bad_end = l->length;
    else if ((code == 102 || code == 112) && offset < end &&
             (l->s[offset] == '\'' || l->s[offset] == '"')) {
        bad_end = code == 112 ? l->pos : end;
    } else if (code == 111) bad_end = offset + 2 <= end ? offset + 2 : end;
    else if (offset < end) {
        size_t bytes = utf8_span(l->s + offset, end - offset);
        bad_end = offset + (bytes ? bytes : 1);
    }
    if (bad_end == bad_start && bad_start < end) bad_end++;
    GROW(l->r->invalid, l->r->invalid_count, l->r->invalid_capacity);
    l->r->invalid[l->r->invalid_count++] = (CfInvalid){bad_start, bad_end - bad_start, line, col, code, what};
    fprintf(l->diagnostics, "Error E%d (Lexical Error) at line %zu, column %zu\n  ", code, line, col);
    /* Escape invalid bytes and controls; track the rendered caret position. */
    size_t caret = 0;
    for (size_t i = begin; i < end;) {
        unsigned char c = (unsigned char)l->s[i];
        size_t bytes = utf8_span(l->s + i, end - i), width = 1;
        if (!bytes || c < 32 || c == 127) {
            fprintf(l->diagnostics, "\\x%02X", c);
            bytes = 1; width = 4;
        } else {
            fwrite(l->s + i, 1, bytes, l->diagnostics);
        }
        if (i - begin < col - 1) caret += width;
        i += bytes;
    }
    fputs("\n  ", l->diagnostics);
    for (size_t i = 0; i < caret; i++) fputc(' ', l->diagnostics);
    fprintf(l->diagnostics, "^\nWhat happened: %s\nWhy: %s\nHow to fix: %s\n\n", what, why, fix);
    if (++l->r->errors == ERROR_LIMIT)
        fputs("Fix these first, then run again\n", l->diagnostics);
}
static void recover(Lexer *l)
{
    while (l->pos < l->length && peek(l, 0) != '\n') advance(l);
    /* A damaged line must not leave its own openers on the bracket stack. */
    while (l->bracket_count && l->brackets[l->bracket_count - 1].line == l->line)
        l->bracket_count--;
    l->statement = l->r->token_count;
}
static void newline(Lexer *l)
{
    if (!l->braces && !l->bracket_count && l->logical_started) {
        structural(l, CF_NEWLINE);
        l->logical_started = 0;
    }
    advance(l);
}
static int begin_code(Lexer *l)
{
    if (l->physical_started) return 1;
    l->physical_started = 1;
    if (l->braces || l->bracket_count) return 1;
    if (l->indent_tab) {
        error_at(l, 107, l->pos, l->line, 1, "A tab is used in indentation.",
                 "Indentation uses groups of four spaces.", "Replace indentation tabs with four spaces.");
        recover(l); return 0;
    }
    size_t width = l->indent_width;
    size_t current = l->indents[l->indent_count - 1];
    if (width % 4 || (width > current && width - current != 4)) {
        error_at(l, 108, l->pos, l->line, 1, "The indentation does not match a four-space level.",
                 "Each new indentation level adds four spaces.", "Align this line with a previous level or add exactly four spaces.");
        recover(l); return 0;
    }
    if (width > current) {
        GROW(l->indents, l->indent_count, l->indent_capacity);
        l->indents[l->indent_count++] = width;
        structural(l, CF_INDENT);
        l->statement = l->r->token_count;
    } else if (width < current) {
        size_t target = l->indent_count;
        while (target && l->indents[target - 1] > width) target--;
        if (!target || l->indents[target - 1] != width) {
            error_at(l, 108, l->pos, l->line, 1, "The line dedents to an unknown level.",
                     "A dedent must match an earlier indentation level.", "Align the line with its surrounding block.");
            recover(l); return 0;
        }
        while (l->indent_count > target) { l->indent_count--; structural(l, CF_DEDENT); }
        l->statement = l->r->token_count;
    }
    return 1;
}

/* Look past trivia without consuming it (used only for the optional 'then'). */
static unsigned char next_significant(const Lexer *l)
{
    size_t p = l->pos;
    while (p < l->length) {
        unsigned char c = (unsigned char)l->s[p];
        if (c == ' ' || c == '\t' || c == '\n') { p++; continue; }
        if (c == '#') {
            while (p < l->length && l->s[p] != '\n') p++;
            continue;
        }
        if (c == '/' && p + 1 < l->length && l->s[p + 1] == '*') {
            p += 2;
            while (p + 1 < l->length && !(l->s[p] == '*' && l->s[p + 1] == '/')) p++;
            if (p + 1 >= l->length) return 0;
            p += 2; continue;
        }
        return c;
    }
    return 0;
}

/* Has the token prefix recognized one assignment target, including indices? */
static int target_complete(const Lexer *l)
{
    const CfResult *r = l->r;
    size_t i = l->statement + 1;
    if (i >= r->token_count || r->tokens[i++].type != CF_IDENT) return 0;
    while (i < r->token_count) {
        if (r->tokens[i++].type != CF_LBRACKET) return 0;
        size_t depth = 1, start = i;
        while (i < r->token_count && depth) {
            CfTokenType t = r->tokens[i++].type;
            if (t == CF_LBRACKET) depth++;
            if (t == CF_RBRACKET) depth--;
        }
        if (depth || i == start + 1) return 0;
    }
    return 1;
}
static void word(Lexer *l)
{
    size_t start = l->pos, line = l->line, col = l->column;
    size_t state = 0;
    do {
        /* Bound trie growth for overlong invalid identifiers. */
        if (l->pos - start < 32) state = extend(l, state, peek(l, 0));
        advance(l);
    } while (letter(peek(l, 0)) || digit(peek(l, 0)));
    size_t n = l->pos - start;
    if (n > 32) {
        error_at(l, 104, start, line, col, "This identifier is longer than 32 characters.",
                 "ClariFix limits identifier length to 32 characters.", "Choose a shorter name.");
        recover(l); return;
    }
    size_t fixed_id = l->nodes[state].fixed;
    CfTokenType type = fixed_id ? fixed[fixed_id - 1].type :
                       l->nodes[state].boolean ? CF_BOOL : CF_IDENT;
    if (type == CF_NOISE) {
        int noise = fixed[fixed_id - 1].noise;
        size_t count = l->r->token_count - l->statement;
        CfTokenType head = count ? l->r->tokens[l->statement].type : CF_EOF;
        int allowed = 0;
        if (noise == 1) {
            allowed = head == CF_SET && target_complete(l);
            if (allowed) {
                report(l, CF_NOISE, start, n, line, col, 0);
                emit_parser(l, CF_ASSIGN, start, n, line, col, 0); return;
            }
        } else if (noise == 2) {
            allowed = (head == CF_IF || head == CF_ELIF) && count > 1 && next_significant(l) == ':';
        } else {
            /* Filler before a target/output/input, or an assignment value. */
            allowed = count == 1 && (head == CF_SET || head == CF_SHOW || head == CF_ASK);
            if (head == CF_SET && count > 2 && l->r->tokens[l->r->token_count - 1].type == CF_ASSIGN)
                allowed = 1;
        }
        if (!allowed) {
            error_at(l, 105, start, line, col, "A noise word is used in an unsupported position.",
                     "Noise words are reserved and only allowed in specific statement positions.",
                     "Use 'to' after a set target, 'then' before an if/elif colon, or 'the'/'of' before a set/show/ask operand.");
            recover(l);
        } else {
            report(l, CF_NOISE, start, n, line, col, 0);
        }
        return;
    }
    emit(l, type, start, n, line, col, 0);
}
static void number(Lexer *l)
{
    size_t start = l->pos, line = l->line, col = l->column;
    CfTokenType type = CF_INT;
    while (digit(peek(l, 0))) advance(l);
    /* A dot belongs to a decimal only when followed by a digit: 19.99. */
    if (peek(l, 0) == '.' && digit(peek(l, 1))) {
        type = CF_DECIMAL; advance(l);
        while (digit(peek(l, 0))) advance(l);
    }
    if (letter(peek(l, 0))) {
        error_at(l, 110, start, line, col, "A name begins with a number, or a number touches a name.",
                 "Identifiers must start with an ASCII letter or underscore.", "Start the name with a letter, or separate the number and name with an operator.");
        recover(l); return;
    }
    emit(l, type, start, l->pos - start, line, col, 0);
}

/* Size of one valid UTF-8 scalar; rejects overlong forms and surrogate values. */
static size_t utf8_size(const Lexer *l)
{
    return utf8_span(l->s + l->pos, l->length - l->pos);
}
static void literal(Lexer *l)
{
    size_t start = l->pos, line = l->line, col = l->column, characters = 0;
    unsigned char quote = advance(l);
    while (l->pos < l->length && peek(l, 0) != quote && peek(l, 0) != '\n') {
        if (peek(l, 0) == '\\') {
            size_t escape = l->pos, escape_col = l->column;
            advance(l);
            if (l->pos == l->length || peek(l, 0) == '\n') break;
            unsigned char c = advance(l);
            if (c != 'n' && c != '\\' && c != '\'' && c != '"') {
                error_at(l, 111, escape, l->line, escape_col, "The literal contains an unknown escape sequence.",
                         "Only newline, quote, and backslash escapes are supported.", "Use a supported escape or double the backslash.");
                recover(l); return;
            }
        } else {
            size_t bytes = utf8_size(l);
            if (!bytes || peek(l, 0) == 0) {
                error_at(l, 103, l->pos, l->line, l->column, "The literal contains an invalid character encoding.",
                         "Source files use UTF-8 without embedded NUL bytes.", "Save the file as UTF-8 and remove NUL bytes.");
                recover(l); return;
            }
            while (bytes--) advance(l);
        }
        characters++;
    }
    if (l->pos == l->length || peek(l, 0) != quote) {
        error_at(l, 102, start, line, col, "A quoted literal is not closed on this line.",
                 "Literals need matching straight quotes and cannot contain raw newlines.", "Close the literal, or use \\n for a newline inside it.");
        recover(l); return;
    }
    advance(l);
    if (quote == '\'' && characters != 1) {
        error_at(l, 112, start, line, col, "A character literal does not contain exactly one character.",
                 "Single quotes hold one Unicode character or one supported escape.", "Use one character, or double quotes for a string.");
        recover(l); return;
    }
    emit(l, quote == '\'' ? CF_CHAR : CF_STRING, start, l->pos - start, line, col, 0);
}
static void comment(Lexer *l)
{
    size_t start = l->pos, line = l->line, col = l->column;
    advance(l); advance(l);
    while (l->pos < l->length) {
        if (peek(l, 0) == '*' && peek(l, 1) == '/') {
            advance(l); advance(l);
            report(l, CF_BLOCK_COMMENT, start, l->pos - start, line, col, 0);
            return;
        }
        if (peek(l, 0) == '\n') newline(l);
        else advance(l);
    }
    error_at(l, 106, start, line, col, "A multi-line comment is not closed.",
             "Every /* comment needs a closing */ marker.", "Add */ at the end of the comment.");
}
static int symbol(Lexer *l)
{
    size_t state = 0, id = 0, width = 0;
    for (size_t ahead = 0; ahead < l->length - l->pos; ahead++) {
        state = transition(l, state, peek(l, ahead));
        if (!state) break;
        size_t candidate = l->nodes[state].fixed;
        if (candidate) {
            id = candidate; width = ahead + 1;
        }
    }
    if (!id) return 0;
    size_t start = l->pos, line = l->line, col = l->column;
    CfTokenType type = fixed[id - 1].type;
    char c = (char)peek(l, 0);
    if (!l->braces && (c == '{' || c == '}')) {
        error_at(l, 103, start, line, col, "A brace appears in indentation mode.",
                 "This mode uses colons and indentation for blocks.", "Use indentation, or run the lexer with --braces.");
        recover(l); return 1;
    }
    if (width == 1 && (c == '(' || c == '[' || c == '{')) {
        GROW(l->brackets, l->bracket_count, l->bracket_capacity);
        l->brackets[l->bracket_count++] = (Bracket){c == '(' ? ')' : c == '[' ? ']' : '}', start, line, col};
    } else if (width == 1 && (c == ')' || c == ']' || c == '}')) {
        if (!l->bracket_count || l->brackets[l->bracket_count - 1].close != c) {
            error_at(l, 102, start, line, col, "A closing bracket has no matching opener.",
                     "Brackets must close in reverse opening order.", "Match each (, [, or { with its corresponding closing bracket.");
            recover(l); return 1;
        }
        l->bracket_count--;
    }
    for (size_t j = 0; j < width; j++) advance(l);
    emit(l, type, start, width, line, col, 0);
    return 1;
}

int cf_lex(const char *source, size_t length, int braces, FILE *diagnostics, CfResult *result)
{
    Lexer l;
    memset(result, 0, sizeof(*result));
    memset(&l, 0, sizeof(l));
    l.s = source; l.length = length; l.line = l.column = 1;
    l.braces = braces; l.diagnostics = diagnostics; l.r = result;
    GROW(l.indents, l.indent_count, l.indent_capacity);
    l.indents[l.indent_count++] = 0;
    GROW(l.nodes, l.node_count, l.node_capacity);
    l.nodes[l.node_count++] = (Node){0, 0, 0, 0, 0};
    for (size_t i = 0; i < sizeof(fixed) / sizeof(fixed[0]); i++) {
        size_t state = 0, n = 0;
        while (fixed[i].word[n]) {
            state = extend(&l, state, (unsigned char)fixed[i].word[n]);
            n++;
        }
        l.nodes[state].fixed = i + 1;
    }
    /* Boolean DFA accepting states are separate from the reserved vocabulary. */
    const char *booleans[] = {"true", "false"};
    for (size_t i = 0; i < 2; i++) {
        size_t state = 0;
        for (size_t j = 0; booleans[i][j]; j++)
            state = extend(&l, state, (unsigned char)booleans[i][j]);
        l.nodes[state].boolean = (int)i + 1;
    }
    while (l.pos < length && result->errors < ERROR_LIMIT) {
        unsigned char c = peek(&l, 0);
        if (c == ' ' || c == '\t') {
            if (!l.physical_started) {
                if (c == '\t') l.indent_tab = 1;
                else l.indent_width++;
            }
            advance(&l); continue;
        }
        if (c == '\n') { newline(&l); continue; }
        if (c == '#') {
            size_t start = l.pos, line = l.line, col = l.column;
            while (l.pos < length && peek(&l, 0) != '\n') advance(&l);
            report(&l, CF_LINE_COMMENT, start, l.pos - start, line, col, 0);
            continue;
        }
        if (c == '/' && peek(&l, 1) == '*') { comment(&l); continue; }
        if (!begin_code(&l)) continue;
        l.logical_started = 1;
        if (letter(c)) word(&l);
        else if (digit(c)) number(&l);
        else if (c == '\'' || c == '"') literal(&l);
        else if (c == 0xe2 && peek(&l, 1) == 0x80 &&
                 (peek(&l, 2) == 0x98 || peek(&l, 2) == 0x99 || peek(&l, 2) == 0x9c || peek(&l, 2) == 0x9d)) {
            error_at(&l, 109, l.pos, l.line, l.column, "A curly quote is used as a literal boundary.",
                     "ClariFix literal boundaries use straight quotes.", "Replace smart quotes with ' or \".");
            recover(&l);
        } else if (!symbol(&l)) {
            error_at(&l, 103, l.pos, l.line, l.column, "This character is not recognized outside a literal or comment.",
                     "Names use ASCII letters, digits and underscores; operators have defined spellings.", "Remove the character or place intended text inside quotes.");
            recover(&l);
        }
    }
    while (l.bracket_count && result->errors < ERROR_LIMIT) {
        Bracket b = l.brackets[--l.bracket_count];
        error_at(&l, 102, b.offset, b.line, b.column, "An opening bracket is never closed.",
                 "Every opening bracket needs a matching closing bracket.", "Add the matching ), ], or }.");
    }
    if (!braces && l.logical_started) structural(&l, CF_NEWLINE);
    while (l.indent_count > 1) { l.indent_count--; structural(&l, CF_DEDENT); }
    structural(&l, CF_EOF);
    free(l.indents); free(l.brackets); free(l.nodes);
    return result->errors ? 1 : 0;
}

static void escaped(FILE *out, const char *s, size_t n)
{
    for (size_t i = 0; i < n;) {
        unsigned char c = (unsigned char)s[i];
        size_t bytes = utf8_span(s + i, n - i);
        if (c == '\\') fputs("\\\\", out);
        else if (c == '\n') fputs("\\n", out);
        else if (c == '\t') fputs("\\t", out);
        else if (c == '\r') fputs("\\r", out);
        else if (c < 32 || c == 127 || !bytes) fprintf(out, "\\x%02X", c);
        else { fwrite(s + i, 1, bytes, out); i += bytes; continue; }
        i++;
    }
}
int cf_write_tokens(FILE *out, const char *source, const CfResult *r)
{
    fputs("LINE\tCOLUMN\tTOKEN\tLEXEME\tSYMBOL_ID\n", out);
    for (size_t i = 0; i < r->token_count; i++) {
        const CfToken *t = &r->tokens[i];
        fprintf(out, "%zu\t%zu\t%s\t", t->line, t->column, cf_token_name(t->type));
        escaped(out, source + t->offset, t->length);
        fprintf(out, "\t%zu\n", t->symbol);
    }
    return ferror(out) ? 2 : 0;
}
static const char *public_name(CfTokenType type)
{
    switch (type) {
    case CF_IDENT: return "IDENTIFIER";
    case CF_INT: return "CONST_INT";
    case CF_DECIMAL: return "CONST_DECIMAL";
    case CF_CHAR: return "CONST_CHAR";
    case CF_STRING: return "CONST_STRING";
    case CF_BOOL: return "CONST_BOOLEAN";
    case CF_LINE_COMMENT: return "SINGLE_COMMENT";
    case CF_BLOCK_COMMENT: return "MULTI_COMMENT";
    case CF_PERIOD: return "STATEMENT_TERMINATOR";
    default: return cf_token_name(type);
    }
}
static const char *category(CfTokenType type)
{
    if (type == CF_IDENT) return "Identifier";
    if (type >= CF_INT && type <= CF_BOOL) return "Constant";
    if (type >= CF_SET && type <= CF_CONTINUE) return "Keyword";
    if (type == CF_NOISE) return "Noise word";
    if (type == CF_LINE_COMMENT || type == CF_BLOCK_COMMENT) return "Comment";
    if (type >= CF_LPAREN && type <= CF_COLON) return "Delimiter";
    return "Operator";
}
static char *escaped_text(const char *s, size_t n)
{
    if (n > (SIZE_MAX - 1) / 4) exit(2);
    char *text = resize(NULL, 4 * n + 1, 1);
    const char hex[] = "0123456789ABCDEF";
    size_t dest = 0;
    for (size_t i = 0; i < n;) {
        unsigned char c = (unsigned char)s[i];
        size_t bytes = utf8_span(s + i, n - i);
        if (c == '\\' || c == '\n' || c == '\t' || c == '\r') {
            text[dest++] = '\\';
            text[dest++] = c == '\\' ? '\\' : c == '\n' ? 'n' : c == '\t' ? 't' : 'r';
        } else if (!bytes || c < 32 || c == 127) {
            text[dest++] = '\\'; text[dest++] = 'x';
            text[dest++] = hex[c >> 4]; text[dest++] = hex[c & 15];
        } else {
            memcpy(text + dest, s + i, bytes); dest += bytes; i += bytes; continue;
        }
        i++;
    }
    text[dest] = '\0';
    return text;
}
int cf_write_symbols(FILE *out, const CfResult *r)
{
    fputs("ID\tLEXEME\tTOKEN\tCATEGORY\tVALUE\tLENGTH\tFIRST_LINE\tFIRST_COLUMN\tOCCURRENCES\n", out);
    for (size_t i = 0; i < r->symbol_count; i++) {
        const CfSymbol *s = &r->symbols[i];
        fprintf(out, "%zu\t", i + 1);
        escaped(out, s->lexeme, strlen(s->lexeme));
        fprintf(out, "\t%s\t%s\t", public_name(s->type), category(s->type));
        if (s->value) escaped(out, s->value, strlen(s->value));
        else fputc('-', out);
        fprintf(out, "\t%zu\t%zu\t%zu\t%zu\n", s->length, s->line, s->column, s->occurrences);
    }
    return ferror(out) ? 2 : 0;
}
int cf_write_table(FILE *out, const char *source, const CfResult *r)
{
    fputs("TOKEN TYPE\tLEXEME\n", out);
    for (size_t i = 0; i < r->report_count; i++) {
        const CfToken *t = &r->report[i];
        fprintf(out, "%s\t", public_name(t->type));
        escaped(out, source + t->offset, t->length);
        fputc('\n', out);
    }
    return ferror(out) ? 2 : 0;
}
static char *size_text(size_t value)
{
    char text[32];
    snprintf(text, sizeof(text), "%zu", value);
    return copy_text(text, strlen(text));
}
static void pad(FILE *out, size_t width, char c)
{
    for (size_t i = 0; i < width; i++) fputc(c, out);
}
static char **cells_new(size_t rows, size_t columns)
{
    if (rows > SIZE_MAX / columns) exit(2);
    return resize(NULL, rows ? rows * columns : 1, sizeof(char *));
}
static void grid_rule(FILE *out, const size_t *widths, size_t columns)
{
    for (size_t c = 0; c < columns; c++) { fputc('+', out); pad(out, widths[c] + 2, '-'); }
    fputs("+\n", out);
}
static void print_table(FILE *out, const char **headers, char **cells, size_t rows, size_t columns,
                        int bordered)
{
    size_t *widths = resize(NULL, columns, sizeof(size_t));
    for (size_t c = 0; c < columns; c++) widths[c] = strlen(headers[c]);
    for (size_t i = 0; i < rows * columns; i++) {
        size_t width = scalar_count(cells[i], strlen(cells[i]));
        if (width > widths[i % columns]) widths[i % columns] = width;
    }
    if (bordered) { grid_rule(out, widths, columns); fputs("| ", out); }
    for (size_t c = 0; c < columns; c++) {
        fputs(headers[c], out);
        if (bordered) {
            pad(out, widths[c] - strlen(headers[c]), ' ');
            fputs(c + 1 < columns ? " | " : " |", out);
        } else if (c + 1 < columns) { pad(out, widths[c] - strlen(headers[c]), ' '); fputs("  ", out); }
    }
    fputc('\n', out);
    if (bordered) grid_rule(out, widths, columns);
    else {
        for (size_t c = 0; c < columns; c++) {
            pad(out, widths[c], '-'); if (c + 1 < columns) fputs("  ", out);
        }
        fputc('\n', out);
    }
    if (!rows) fputs("(none)\n", out);
    for (size_t row = 0; row < rows; row++) {
        if (bordered) fputs("| ", out);
        for (size_t c = 0; c < columns; c++) {
            char *text = cells[row * columns + c];
            fputs(text, out);
            if (bordered) {
                pad(out, widths[c] - scalar_count(text, strlen(text)), ' ');
                fputs(c + 1 < columns ? " | " : " |", out);
            } else if (c + 1 < columns) { pad(out, widths[c] - scalar_count(text, strlen(text)), ' '); fputs("  ", out); }
            free(text);
        }
        fputc('\n', out);
    }
    if (bordered) grid_rule(out, widths, columns);
    free(cells); free(widths);
}
int cf_write_token_table(FILE *out, const char *source, const CfResult *r)
{
    const char *headers[] = {"LINE", "COLUMN", "TOKEN", "LEXEME", "SYMBOL_ID"};
    char **cells = cells_new(r->token_count, 5);
    for (size_t i = 0; i < r->token_count; i++) {
        const CfToken *t = &r->tokens[i];
        size_t j = i * 5;
        cells[j] = size_text(t->line); cells[j + 1] = size_text(t->column);
        cells[j + 2] = copy_text(cf_token_name(t->type), strlen(cf_token_name(t->type)));
        cells[j + 3] = escaped_text(source + t->offset, t->length);
        cells[j + 4] = size_text(t->symbol);
    }
    print_table(out, headers, cells, r->token_count, 5, 1);
    return ferror(out) ? 2 : 0;
}
int cf_write_report(FILE *out, const char *source, const char *source_name, const CfResult *r)
{
    fputs("======================================================================\n"
          "                        CLARIFIX SYMBOL TABLE\n"
          "======================================================================\nSource file : ", out);
    escaped(out, source_name, strlen(source_name));
    fputs("\n----------------------------------------------------------------------\n\n"
          "[A] TOKEN LIST (in order of appearance)\n", out);
    const char *a_headers[] = {"No.", "Line", "Col", "Lexeme", "Token Type", "Category"};
    char **cells = cells_new(r->report_count, 6);
    /* Category counts: identifiers, keywords, noise, constants, delimiters, comments, operators. */
    size_t counts[7] = {0};
    for (size_t i = 0; i < r->report_count; i++) {
        const CfToken *t = &r->report[i];
        size_t j = i * 6;
        cells[j] = size_text(i + 1); cells[j + 1] = size_text(t->line); cells[j + 2] = size_text(t->column);
        cells[j + 3] = escaped_text(source + t->offset, t->length);
        cells[j + 4] = copy_text(public_name(t->type), strlen(public_name(t->type)));
        cells[j + 5] = copy_text(category(t->type), strlen(category(t->type)));
        size_t group = t->type == CF_IDENT ? 0 :
                       t->type >= CF_SET && t->type <= CF_CONTINUE ? 1 :
                       t->type == CF_NOISE ? 2 :
                       t->type >= CF_INT && t->type <= CF_BOOL ? 3 :
                       t->type >= CF_LPAREN && t->type <= CF_COLON ? 4 :
                       t->type == CF_LINE_COMMENT || t->type == CF_BLOCK_COMMENT ? 5 : 6;
        counts[group]++;
    }
    print_table(out, a_headers, cells, r->report_count, 6, 0);
    fputs("\n[B] SYMBOL TABLE (unique identifiers and constants)\n", out);
    const char *b_headers[] = {"ID", "Lexeme", "Token Type", "Value", "Length", "First Seen"};
    cells = cells_new(r->symbol_count, 6);
    for (size_t i = 0; i < r->symbol_count; i++) {
        const CfSymbol *s = &r->symbols[i];
        size_t j = i * 6;
        cells[j] = size_text(i + 1); cells[j + 1] = escaped_text(s->lexeme, strlen(s->lexeme));
        cells[j + 2] = copy_text(public_name(s->type), strlen(public_name(s->type)));
        char *value = s->value ? escaped_text(s->value, strlen(s->value)) : copy_text("-", 1);
        if (s->type == CF_CHAR || s->type == CF_STRING) {
            size_t n = strlen(value);
            cells[j + 3] = resize(NULL, n + 3, 1);
            char quote = s->type == CF_CHAR ? '\'' : '"';
            cells[j + 3][0] = quote; memcpy(cells[j + 3] + 1, value, n);
            cells[j + 3][n + 1] = quote; cells[j + 3][n + 2] = '\0'; free(value);
        } else cells[j + 3] = value;
        cells[j + 4] = size_text(s->length);
        char first[70]; snprintf(first, sizeof(first), "%zu:%zu", s->line, s->column);
        cells[j + 5] = copy_text(first, strlen(first));
    }
    print_table(out, b_headers, cells, r->symbol_count, 6, 0);
    fputs("\n[C] INVALID / UNRECOGNIZED TOKENS\n", out);
    const char *c_headers[] = {"Line", "Col", "Lexeme", "Error", "Reason"};
    cells = cells_new(r->invalid_count, 5);
    for (size_t i = 0; i < r->invalid_count; i++) {
        const CfInvalid *invalid = &r->invalid[i];
        size_t j = i * 5;
        cells[j] = size_text(invalid->line); cells[j + 1] = size_text(invalid->column);
        cells[j + 2] = escaped_text(source + invalid->offset, invalid->length);
        char code[32]; snprintf(code, sizeof(code), "E%d", invalid->code);
        cells[j + 3] = copy_text(code, strlen(code));
        cells[j + 4] = copy_text(invalid->reason, strlen(invalid->reason));
    }
    print_table(out, c_headers, cells, r->invalid_count, 5, 0);
    fprintf(out, "\n[D] SUMMARY\nIdentifiers: %zu | Keywords: %zu | Noise words: %zu | Constants: %zu\n"
            "Delimiters: %zu | Comments: %zu | Operators: %zu | Invalid: %zu | Total tokens: %zu\n"
            "======================================================================\n",
            counts[0], counts[1], counts[2], counts[3], counts[4], counts[5], counts[6], r->invalid_count, r->report_count);
    return ferror(out) ? 2 : 0;
}
void cf_free(CfResult *r)
{
    for (size_t i = 0; i < r->symbol_count; i++) { free(r->symbols[i].lexeme); free(r->symbols[i].value); }
    free(r->symbols); free(r->tokens); free(r->report); free(r->invalid);
    memset(r, 0, sizeof(*r));
}
