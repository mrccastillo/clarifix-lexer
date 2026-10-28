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
    {"true", CF_BOOL, 0}, {"false", CF_BOOL, 0},
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
/* Sparse character transitions. Node 0 is the root; terminal IDs are stable. */
typedef struct { unsigned char c; size_t child, sibling, symbol; } Node;
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
    l->nodes[next] = (Node){c, 0, l->nodes[state].child, 0};
    l->nodes[state].child = next;
    return next;
}
static size_t accept(Lexer *l, size_t state, const char *s, size_t n,
                     CfTokenType type, int is_fixed)
{
    if (l->nodes[state].symbol) return l->nodes[state].symbol;
    CfResult *r = l->r;
    GROW(r->symbols, r->symbol_count, r->symbol_capacity);
    CfSymbol *symbol = &r->symbols[r->symbol_count++];
    memset(symbol, 0, sizeof(*symbol));
    symbol->lexeme = resize(NULL, n + 1, 1);
    memcpy(symbol->lexeme, s, n);
    symbol->lexeme[n] = '\0';
    symbol->type = type;
    symbol->fixed = is_fixed;
    l->nodes[state].symbol = r->symbol_count;
    return r->symbol_count;
}
static void report(Lexer *l, CfTokenType type, size_t start, size_t n,
                   size_t line, size_t col, size_t symbol)
{
    CfResult *r = l->r;
    GROW(r->report, r->report_count, r->report_capacity);
    r->report[r->report_count++] = (CfToken){type, start, n, line, col, symbol};
}
static void seen(Lexer *l, size_t id, size_t line, size_t column)
{
    CfSymbol *s = &l->r->symbols[id - 1];
    if (!s->occurrences) { s->line = line; s->column = column; }
    s->occurrences++;
}
static void emit(Lexer *l, CfTokenType type, size_t start, size_t n,
                 size_t line, size_t col, size_t symbol)
{
    CfResult *r = l->r;
    GROW(r->tokens, r->token_count, r->token_capacity);
    r->tokens[r->token_count++] = (CfToken){type, start, n, line, col, symbol};
    if (n && !(type == CF_ASSIGN && symbol && r->symbols[symbol - 1].type == CF_NOISE))
        report(l, type, start, n, line, col, symbol);
    if (type == CF_PERIOD || type == CF_NEWLINE || type == CF_LBRACE || type == CF_RBRACE)
        l->statement = r->token_count;
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
    size_t id = accept(l, state, l->s + start, n, CF_IDENT, 0);
    CfTokenType type = l->r->symbols[id - 1].type;
    seen(l, id, line, col);
    if (type == CF_NOISE) {
        int noise = l->r->symbols[id - 1].noise;
        size_t count = l->r->token_count - l->statement;
        CfTokenType head = count ? l->r->tokens[l->statement].type : CF_EOF;
        int allowed = 0;
        if (noise == 1) {
            allowed = head == CF_SET && target_complete(l);
            if (allowed) {
                report(l, CF_NOISE, start, n, line, col, id);
                emit(l, CF_ASSIGN, start, n, line, col, id); return;
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
            report(l, CF_NOISE, start, n, line, col, id);
        }
        return;
    }
    emit(l, type, start, n, line, col, id);
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
        size_t candidate = l->nodes[state].symbol;
        if (candidate && l->r->symbols[candidate - 1].fixed) {
            id = candidate; width = ahead + 1;
        }
    }
    if (!id) return 0;
    size_t start = l->pos, line = l->line, col = l->column;
    CfTokenType type = l->r->symbols[id - 1].type;
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
    seen(l, id, line, col);
    emit(l, type, start, width, line, col, id);
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
    l.nodes[l.node_count++] = (Node){0, 0, 0, 0};
    for (size_t i = 0; i < sizeof(fixed) / sizeof(fixed[0]); i++) {
        size_t state = 0, n = 0;
        while (fixed[i].word[n]) {
            state = extend(&l, state, (unsigned char)fixed[i].word[n]);
            n++;
        }
        size_t id = accept(&l, state, fixed[i].word, n, fixed[i].type, 1);
        /* Accepting actions are assigned during construction, never by matching input words. */
        result->symbols[id - 1].noise = fixed[i].noise;
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
int cf_write_symbols(FILE *out, const CfResult *r)
{
    fputs("ID\tLEXEME\tTOKEN\tORIGIN\tFIRST_LINE\tFIRST_COLUMN\tOCCURRENCES\n", out);
    for (size_t i = 0; i < r->symbol_count; i++) {
        const CfSymbol *s = &r->symbols[i];
        fprintf(out, "%zu\t", i + 1);
        escaped(out, s->lexeme, strlen(s->lexeme));
        fprintf(out, "\t%s\t%s\t%zu\t%zu\t%zu\n", cf_token_name(s->type),
                s->fixed ? "FIXED" : "IDENTIFIER", s->line, s->column, s->occurrences);
    }
    return ferror(out) ? 2 : 0;
}
int cf_write_report(FILE *out, const char *source, const CfResult *r)
{
    fputs("TOKEN TYPE\tLEXEME\n", out);
    for (size_t i = 0; i < r->report_count; i++) {
        const CfToken *t = &r->report[i];
        const char *name;
        switch (t->type) {
        case CF_IDENT: name = "IDENTIFIER"; break;
        case CF_INT: name = "INT_LITERAL"; break;
        case CF_DECIMAL: name = "DECIMAL_LITERAL"; break;
        case CF_CHAR: name = "CHAR_LITERAL"; break;
        case CF_STRING: name = "STRING_LITERAL"; break;
        case CF_BOOL: name = "BOOL_LITERAL"; break;
        case CF_NOISE: name = "NOISE_WORD"; break;
        default: name = cf_token_name(t->type); break;
        }
        fprintf(out, "%s\t", name);
        escaped(out, source + t->offset, t->length);
        fputc('\n', out);
    }
    return ferror(out) ? 2 : 0;
}
void cf_free(CfResult *r)
{
    for (size_t i = 0; i < r->symbol_count; i++) free(r->symbols[i].lexeme);
    free(r->symbols); free(r->tokens); free(r->report);
    memset(r, 0, sizeof(*r));
}
