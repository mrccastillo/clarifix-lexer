#include "internal.h"
#include "dfa.h"

#include <stdlib.h>
#include <string.h>

/* Every write into a fixed array is checked before it happens. */
void cf_require_room(size_t count, size_t limit, const char *name)
{
    if (count >= limit) {
        fprintf(stderr, "Capacity limit: %s (maximum %zu).\n", name, limit);
        exit(2);
    }
}

int cf_letter(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
int cf_digit(unsigned char c) { return c >= '0' && c <= '9'; }
unsigned char cf_peek(const Lexer *l, size_t ahead)
{
    return ahead < l->length - l->pos ? (unsigned char)l->s[l->pos + ahead] : 0;
}
unsigned char cf_advance(Lexer *l)
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

static void report(Lexer *l, CfTokenType type, size_t start, size_t n,
                   size_t line, size_t col, size_t symbol)
{
    CfResult *r = l->r;
    cf_require_room(r->report_count, CF_MAX_TOKENS, "source tokens");
    r->report[r->report_count++] = (CfToken){type, start, n, line, col, symbol};
    if (l->trace) fprintf(l->trace, "TRACE %zu:%zu ACCEPT %s length=%zu\n", line, col, cf_token_name(type), n);
}
static void emit_parser(Lexer *l, CfTokenType type, size_t start, size_t n,
                 size_t line, size_t col, size_t symbol)
{
    CfResult *r = l->r;
    cf_require_room(r->token_count, CF_MAX_TOKENS, "parser tokens");
    r->tokens[r->token_count++] = (CfToken){type, start, n, line, col, symbol};
    if (type == CF_PERIOD || type == CF_NEWLINE || type == CF_LBRACE || type == CF_RBRACE)
        l->statement = r->token_count;
}
static void emit(Lexer *l, CfTokenType type, size_t start, size_t n,
                 size_t line, size_t col, size_t symbol)
{
    if (type == CF_IDENT || (type >= CF_INT && type <= CF_BOOL))
        symbol = cf_symbol_insert(l, l->s + start, n, type, line, col);
    emit_parser(l, type, start, n, line, col, symbol);
    if (n) report(l, type, start, n, line, col, symbol);
}
static void structural(Lexer *l, CfTokenType type)
{
    if (l->trace) fprintf(l->trace, "TRACE %zu:%zu LAYOUT EMIT %s\n",
                          l->line, l->column, cf_token_name(type));
    emit(l, type, l->pos, 0, l->line, l->column, 0);
}

size_t cf_scalar_count(const char *s, size_t n)
{
    size_t count = 0;
    for (size_t i = 0; i < n; count++) {
        size_t bytes = cf_utf8_span(s + i, n - i);
        i += bytes ? bytes : 1;
    }
    return count;
}

/* Zero means an invalid UTF-8 byte; ASCII (including NUL) has length one. */
size_t cf_utf8_span(const char *s, size_t n)
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

static void newline(Lexer *l)
{
    if (!l->braces && !l->bracket_count && l->logical_started) {
        structural(l, CF_NEWLINE);
        l->logical_started = 0;
    }
    cf_advance(l);
}
static int begin_code(Lexer *l)
{
    if (l->physical_started) return 1;
    l->physical_started = 1;
    if (l->braces || l->bracket_count) return 1;
    if (l->indent_tab) {
        cf_error_at(l, 107, l->pos, l->line, 1, "A tab is used in indentation.",
                 "Indentation uses groups of four spaces.", "Replace indentation tabs with four spaces.");
        cf_recover(l); return 0;
    }
    size_t width = l->indent_width;
    size_t current = l->indents[l->indent_count - 1];
    if (width % 4 || (width > current && width - current != 4)) {
        cf_error_at(l, 108, l->pos, l->line, 1, "The indentation does not match a four-space level.",
                 "Each new indentation level adds four spaces.", "Align this line with a previous level or add exactly four spaces.");
        cf_recover(l); return 0;
    }
    if (width > current) {
        cf_require_room(l->indent_count, CF_MAX_DEPTH, "indentation levels");
        l->indents[l->indent_count++] = width;
        structural(l, CF_INDENT);
        l->statement = l->r->token_count;
    } else if (width < current) {
        size_t target = l->indent_count;
        while (target && l->indents[target - 1] > width) target--;
        if (!target || l->indents[target - 1] != width) {
            cf_error_at(l, 108, l->pos, l->line, 1, "The line dedents to an unknown level.",
                     "A dedent must match an earlier indentation level.", "Align the line with its surrounding block.");
            cf_recover(l); return 0;
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
    WordState state = W_START;
    do {
        unsigned char c = cf_peek(l, 0);
        WordState next = word_next(state, c);
        cf_trace_step(l, "WORD", word_state_name(state), c, word_state_name(next));
        state = next;
        cf_advance(l);
    } while (cf_letter(cf_peek(l, 0)) || cf_digit(cf_peek(l, 0)));
    size_t n = l->pos - start;
    if (n > 32) {
        cf_error_at(l, 104, start, line, col, "This identifier is longer than 32 characters.",
                 "ClariFix limits identifier length to 32 characters.", "Choose a shorter name.");
        cf_recover(l); return;
    }
    CfTokenType type = word_token(state);
    cf_trace_step(l, "WORD", word_state_name(state), cf_peek(l, 0), "BOUNDARY (not consumed)");
    if (type == CF_NOISE) {
        int noise;
        switch (state) {
        case W_TO: noise = 1; break;
        case W_THEN: noise = 2; break;
        default: noise = 3; break; /* the, of */
        }
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
            cf_error_at(l, 105, start, line, col, "A noise word is used in an unsupported position.",
                     "Noise words are reserved and only allowed in specific statement positions.",
                     "Use 'to' after a set target, 'then' before an if/elif colon, or 'the'/'of' before a set/show/ask operand.");
            cf_recover(l);
        } else {
            report(l, CF_NOISE, start, n, line, col, 0);
        }
        return;
    }
    emit(l, type, start, n, line, col, 0);
}
/* Number DFA: START -> INTEGER -> DOT -> DECIMAL.
   A dot enters DOT only if the next character is a digit, so the last
   dot in "19.99." is left for the statement-terminator recognizer. */
typedef enum { N_START, N_INTEGER, N_DOT, N_DECIMAL, N_DONE } NumberState;
static const char *number_state_name(NumberState state)
{
    switch (state) {
    case N_START: return "START";
    case N_INTEGER: return "INTEGER";
    case N_DOT: return "DOT";
    case N_DECIMAL: return "DECIMAL";
    case N_DONE: return "BOUNDARY (not consumed)";
    }
    return "UNKNOWN";
}
static void number(Lexer *l)
{
    size_t start = l->pos, line = l->line, col = l->column;
    NumberState state = N_START;
    CfTokenType type = CF_INT;
    while (state != N_DONE) {
        unsigned char c = cf_peek(l, 0);
        NumberState next;
        switch (state) {
        case N_START: next = N_INTEGER; break; /* Caller already checked a digit. */
        case N_INTEGER:
            if (cf_digit(c)) next = N_INTEGER;
            else if (c == '.' && cf_digit(cf_peek(l, 1))) next = N_DOT;
            else next = N_DONE;
            break;
        case N_DOT: next = N_DECIMAL; break; /* Lookahead guaranteed a digit. */
        case N_DECIMAL: next = cf_digit(c) ? N_DECIMAL : N_DONE; break;
        default: next = N_DONE; break;
        }
        if (next == N_DONE && cf_letter(c)) {
            cf_trace_step(l, "NUMBER", number_state_name(state), c, "ERROR E110");
            cf_error_at(l, 110, start, line, col, "A name begins with a number, or a number touches a name.",
                     "Identifiers must start with an ASCII letter or underscore.", "Start the name with a letter, or separate the number and name with an operator.");
            cf_recover(l); return;
        }
        cf_trace_step(l, "NUMBER", number_state_name(state), c, number_state_name(next));
        state = next;
        if (state != N_DONE) cf_advance(l);
        if (state == N_DECIMAL) type = CF_DECIMAL;
    }
    emit(l, type, start, l->pos - start, line, col, 0);
}

/* Size of one valid UTF-8 scalar; rejects overlong forms and surrogate values. */
static size_t utf8_size(const Lexer *l)
{
    return cf_utf8_span(l->s + l->pos, l->length - l->pos);
}
/* Quoted literal DFA. Escape processing is visible as its own state. */
typedef enum { Q_START, Q_BODY, Q_ESCAPE, Q_DONE } QuoteState;
static void literal(Lexer *l)
{
    size_t start = l->pos, line = l->line, col = l->column, characters = 0;
    size_t escape = 0, escape_col = 0;
    unsigned char quote = cf_peek(l, 0);
    QuoteState state = Q_START;
    while (state != Q_DONE) {
        unsigned char c = cf_peek(l, 0);
        if (l->pos == l->length || c == '\n') {
            cf_trace_step(l, "QUOTE", state == Q_ESCAPE ? "ESCAPE" : "BODY", c, "ERROR E102");
            cf_error_at(l, 102, start, line, col, "A quoted literal is not closed on this line.",
                     "Literals need matching straight quotes and cannot contain raw newlines.", "Close the literal, or use \\n for a newline inside it.");
            cf_recover(l); return;
        }
        switch (state) {
        case Q_START:
            cf_trace_step(l, "QUOTE", "START", c, "BODY");
            cf_advance(l); state = Q_BODY;
            break;
        case Q_BODY:
            if (c == quote) {
                cf_trace_step(l, "QUOTE", "BODY", c, "DONE");
                cf_advance(l); state = Q_DONE;
            } else if (c == '\\') {
                escape = l->pos; escape_col = l->column;
                cf_trace_step(l, "QUOTE", "BODY", c, "ESCAPE");
                cf_advance(l); state = Q_ESCAPE;
            } else {
                size_t bytes = utf8_size(l);
                if (!bytes || c == 0) {
                    cf_trace_step(l, "QUOTE", "BODY", c, "ERROR E103");
                    cf_error_at(l, 103, l->pos, l->line, l->column, "The literal contains an invalid character encoding.",
                             "Source files use UTF-8 without embedded NUL bytes.", "Save the file as UTF-8 and remove NUL bytes.");
                    cf_recover(l); return;
                }
                while (bytes--) {
                    cf_trace_step(l, "QUOTE", "BODY", cf_peek(l, 0), "BODY");
                    cf_advance(l);
                }
                characters++;
            }
            break;
        case Q_ESCAPE:
            if (c != 'n' && c != '\\' && c != '\'' && c != '"') {
                cf_trace_step(l, "QUOTE", "ESCAPE", c, "ERROR E111");
                cf_advance(l);
                cf_error_at(l, 111, escape, l->line, escape_col, "The literal contains an unknown escape sequence.",
                         "Only newline, quote, and backslash escapes are supported.", "Use a supported escape or double the backslash.");
                cf_recover(l); return;
            }
            cf_trace_step(l, "QUOTE", "ESCAPE", c, "BODY");
            cf_advance(l); characters++; state = Q_BODY;
            break;
        case Q_DONE: break;
        }
    }
    if (quote == '\'' && characters != 1) {
        cf_error_at(l, 112, start, line, col, "A character literal does not contain exactly one character.",
                 "Single quotes hold one Unicode character or one supported escape.", "Use one character, or double quotes for a string.");
        cf_recover(l); return;
    }
    emit(l, quote == '\'' ? CF_CHAR : CF_STRING, start, l->pos - start, line, col, 0);
}

typedef enum { C_START, C_SLASH, C_BODY, C_STAR, C_DONE } CommentState;
static const char *comment_state_name(CommentState state)
{
    switch (state) {
    case C_START: return "START";
    case C_SLASH: return "SLASH";
    case C_BODY: return "BODY";
    case C_STAR: return "STAR";
    case C_DONE: return "DONE";
    }
    return "UNKNOWN";
}
static void line_comment(Lexer *l)
{
    size_t start = l->pos, line = l->line, col = l->column;
    CommentState state = C_START;
    while (l->pos < l->length && cf_peek(l, 0) != '\n') {
        cf_trace_step(l, "LINE_COMMENT", comment_state_name(state), cf_peek(l, 0), "BODY");
        cf_advance(l); state = C_BODY;
    }
    cf_trace_step(l, "LINE_COMMENT", "BODY", cf_peek(l, 0), "BOUNDARY (not consumed)");
    report(l, CF_LINE_COMMENT, start, l->pos - start, line, col, 0);
}
static void comment(Lexer *l)
{
    size_t start = l->pos, line = l->line, col = l->column;
    CommentState state = C_START;
    while (l->pos < l->length && state != C_DONE) {
        unsigned char c = cf_peek(l, 0);
        CommentState next;
        switch (state) {
        case C_START: next = C_SLASH; break;
        case C_SLASH: next = C_BODY; break; /* Opening marker was checked by caller. */
        case C_BODY: next = c == '*' ? C_STAR : C_BODY; break;
        case C_STAR:
            if (c == '/') next = C_DONE;
            else next = c == '*' ? C_STAR : C_BODY;
            break;
        default: next = C_DONE; break;
        }
        cf_trace_step(l, "BLOCK_COMMENT", comment_state_name(state), c, comment_state_name(next));
        if (c == '\n') newline(l);
        else cf_advance(l);
        state = next;
    }
    if (state == C_DONE) {
        report(l, CF_BLOCK_COMMENT, start, l->pos - start, line, col, 0);
        return;
    }
    cf_error_at(l, 106, start, line, col, "A multi-line comment is not closed.",
             "Every /* comment needs a closing */ marker.", "Add */ at the end of the comment.");
}
/* Operator DFA. Remember the last accepting state for longest match. */
typedef enum {
    O_START, O_DEAD,
    O_PLUS,
    O_MINUS,
    O_STAR,
    O_SLASH,
    O_MOD,
    O_POWER,
    O_NOT,
    O_ASSIGN,
    O_LT,
    O_GT,
    O_AMP,
    O_PIPE,
    O_LPAREN,
    O_RPAREN,
    O_LBRACKET,
    O_RBRACKET,
    O_LBRACE,
    O_RBRACE,
    O_COMMA,
    O_PERIOD,
    O_COLON,
    O_INCREMENT,
    O_DECREMENT,
    O_FLOOR_DIV,
    O_AND,
    O_OR,
    O_EQ,
    O_NE,
    O_LE,
    O_GE,
    O_PLUS_ASSIGN,
    O_MINUS_ASSIGN,
    O_STAR_ASSIGN,
    O_SLASH_ASSIGN,
    O_MOD_ASSIGN,
    O_POWER_ASSIGN,
} OperatorState;
static OperatorState operator_next(OperatorState state, unsigned char c)
{
    switch (state) {
    case O_START:
        switch (c) {
        case '+': return O_PLUS;
        case '-': return O_MINUS;
        case '*': return O_STAR;
        case '/': return O_SLASH;
        case '%': return O_MOD;
        case '^': return O_POWER;
        case '!': return O_NOT;
        case '=': return O_ASSIGN;
        case '<': return O_LT;
        case '>': return O_GT;
        case '&': return O_AMP;
        case '|': return O_PIPE;
        case '(': return O_LPAREN;
        case ')': return O_RPAREN;
        case '[': return O_LBRACKET;
        case ']': return O_RBRACKET;
        case '{': return O_LBRACE;
        case '}': return O_RBRACE;
        case ',': return O_COMMA;
        case '.': return O_PERIOD;
        case ':': return O_COLON;
        default: return O_DEAD;
        }
    case O_PLUS:
        if (c == '+') return O_INCREMENT;
        if (c == '=') return O_PLUS_ASSIGN;
        return O_DEAD;
    case O_MINUS:
        if (c == '-') return O_DECREMENT;
        if (c == '=') return O_MINUS_ASSIGN;
        return O_DEAD;
    case O_STAR:
        if (c == '=') return O_STAR_ASSIGN;
        return O_DEAD;
    case O_SLASH:
        if (c == '/') return O_FLOOR_DIV;
        if (c == '=') return O_SLASH_ASSIGN;
        return O_DEAD;
    case O_MOD:
        if (c == '=') return O_MOD_ASSIGN;
        return O_DEAD;
    case O_POWER:
        if (c == '=') return O_POWER_ASSIGN;
        return O_DEAD;
    case O_NOT:
        if (c == '=') return O_NE;
        return O_DEAD;
    case O_ASSIGN:
        if (c == '=') return O_EQ;
        return O_DEAD;
    case O_LT:
        if (c == '=') return O_LE;
        return O_DEAD;
    case O_GT:
        if (c == '=') return O_GE;
        return O_DEAD;
    case O_AMP:
        if (c == '&') return O_AND;
        return O_DEAD;
    case O_PIPE:
        if (c == '|') return O_OR;
        return O_DEAD;
    default: return O_DEAD;
    }
}
static CfTokenType operator_token(OperatorState state)
{
    switch (state) {
    case O_PLUS: return CF_PLUS;
    case O_MINUS: return CF_MINUS;
    case O_STAR: return CF_STAR;
    case O_SLASH: return CF_SLASH;
    case O_MOD: return CF_MOD;
    case O_POWER: return CF_POWER;
    case O_NOT: return CF_NOT;
    case O_ASSIGN: return CF_ASSIGN;
    case O_LT: return CF_LT;
    case O_GT: return CF_GT;
    case O_LPAREN: return CF_LPAREN;
    case O_RPAREN: return CF_RPAREN;
    case O_LBRACKET: return CF_LBRACKET;
    case O_RBRACKET: return CF_RBRACKET;
    case O_LBRACE: return CF_LBRACE;
    case O_RBRACE: return CF_RBRACE;
    case O_COMMA: return CF_COMMA;
    case O_PERIOD: return CF_PERIOD;
    case O_COLON: return CF_COLON;
    case O_INCREMENT: return CF_INCREMENT;
    case O_DECREMENT: return CF_DECREMENT;
    case O_FLOOR_DIV: return CF_FLOOR_DIV;
    case O_AND: return CF_AND;
    case O_OR: return CF_OR;
    case O_EQ: return CF_EQ;
    case O_NE: return CF_NE;
    case O_LE: return CF_LE;
    case O_GE: return CF_GE;
    case O_PLUS_ASSIGN: return CF_PLUS_ASSIGN;
    case O_MINUS_ASSIGN: return CF_MINUS_ASSIGN;
    case O_STAR_ASSIGN: return CF_STAR_ASSIGN;
    case O_SLASH_ASSIGN: return CF_SLASH_ASSIGN;
    case O_MOD_ASSIGN: return CF_MOD_ASSIGN;
    case O_POWER_ASSIGN: return CF_POWER_ASSIGN;
    default: return CF_TOKEN_COUNT; /* Incomplete/invalid operator. */
    }
}
static const char *operator_state_name(OperatorState state)
{
    switch (state) {
    case O_START: return "START";
    case O_DEAD: return "BOUNDARY (not consumed)";
    case O_PLUS: return "PLUS";
    case O_MINUS: return "MINUS";
    case O_STAR: return "STAR";
    case O_SLASH: return "SLASH";
    case O_MOD: return "MOD";
    case O_POWER: return "POWER";
    case O_NOT: return "NOT";
    case O_ASSIGN: return "ASSIGN";
    case O_LT: return "LT";
    case O_GT: return "GT";
    case O_AMP: return "AMP";
    case O_PIPE: return "PIPE";
    case O_LPAREN: return "LPAREN";
    case O_RPAREN: return "RPAREN";
    case O_LBRACKET: return "LBRACKET";
    case O_RBRACKET: return "RBRACKET";
    case O_LBRACE: return "LBRACE";
    case O_RBRACE: return "RBRACE";
    case O_COMMA: return "COMMA";
    case O_PERIOD: return "PERIOD";
    case O_COLON: return "COLON";
    case O_INCREMENT: return "INCREMENT";
    case O_DECREMENT: return "DECREMENT";
    case O_FLOOR_DIV: return "FLOOR_DIV";
    case O_AND: return "AND";
    case O_OR: return "OR";
    case O_EQ: return "EQ";
    case O_NE: return "NE";
    case O_LE: return "LE";
    case O_GE: return "GE";
    case O_PLUS_ASSIGN: return "PLUS_ASSIGN";
    case O_MINUS_ASSIGN: return "MINUS_ASSIGN";
    case O_STAR_ASSIGN: return "STAR_ASSIGN";
    case O_SLASH_ASSIGN: return "SLASH_ASSIGN";
    case O_MOD_ASSIGN: return "MOD_ASSIGN";
    case O_POWER_ASSIGN: return "POWER_ASSIGN";
    }
    return "UNKNOWN";
}
static int symbol(Lexer *l)
{
    OperatorState state = O_START;
    CfTokenType type = CF_TOKEN_COUNT;
    size_t width = 0;
    for (size_t ahead = 0; ahead < l->length - l->pos; ahead++) {
        unsigned char c = cf_peek(l, ahead);
        OperatorState next = operator_next(state, c);
        cf_trace_step_at(l, "OPERATOR", operator_state_name(state), c, operator_state_name(next), ahead);
        if (next == O_DEAD) break;
        state = next;
        CfTokenType candidate = operator_token(state);
        if (candidate != CF_TOKEN_COUNT) { type = candidate; width = ahead + 1; }
    }
    if (type == CF_TOKEN_COUNT) return 0;
    size_t start = l->pos, line = l->line, col = l->column;
    char c = (char)cf_peek(l, 0);
    if (!l->braces && (c == '{' || c == '}')) {
        cf_error_at(l, 103, start, line, col, "A brace appears in indentation mode.",
                 "This mode uses colons and indentation for blocks.", "Use indentation, or run the lexer with --braces.");
        cf_recover(l); return 1;
    }
    if (width == 1 && (c == '(' || c == '[' || c == '{')) {
        cf_require_room(l->bracket_count, CF_MAX_DEPTH, "nested brackets");
        l->brackets[l->bracket_count++] = (Bracket){c == '(' ? ')' : c == '[' ? ']' : '}', start, line, col};
    } else if (width == 1 && (c == ')' || c == ']' || c == '}')) {
        if (!l->bracket_count || l->brackets[l->bracket_count - 1].close != c) {
            cf_error_at(l, 102, start, line, col, "A closing bracket has no matching opener.",
                     "Brackets must close in reverse opening order.", "Match each (, [, or { with its corresponding closing bracket.");
            cf_recover(l); return 1;
        }
        l->bracket_count--;
    }
    for (size_t j = 0; j < width; j++) cf_advance(l);
    emit(l, type, start, width, line, col, 0);
    return 1;
}

int cf_lex(const char *source, size_t length, int braces, FILE *diagnostics, FILE *trace, CfResult *result)
{
    cf_require_room(length, CF_MAX_SOURCE + 1, "source bytes");
    Lexer l;
    memset(result, 0, sizeof(*result));
    memset(&l, 0, sizeof(l));
    l.s = source; l.length = length; l.line = l.column = 1;
    l.braces = braces; l.diagnostics = diagnostics; l.trace = trace; l.r = result;
    l.indents[l.indent_count++] = 0;
    while (l.pos < length && result->errors < CF_MAX_ERRORS) {
        unsigned char c = cf_peek(&l, 0);
        if (c == ' ' || c == '\t') {
            cf_trace_step(&l, "LAYOUT", "START", c, "SKIP");
            if (!l.physical_started) {
                if (c == '\t') l.indent_tab = 1;
                else l.indent_width++;
            }
            cf_advance(&l); continue;
        }
        if (c == '\n') {
            cf_trace_step(&l, "LAYOUT", "START", c, "NEXT_LINE");
            newline(&l); continue;
        }
        if (c == '#') { line_comment(&l); continue; }
        if (c == '/' && cf_peek(&l, 1) == '*') { comment(&l); continue; }
        if (!begin_code(&l)) continue;
        l.logical_started = 1;
        if (cf_letter(c)) word(&l);
        else if (cf_digit(c)) number(&l);
        else if (c == '\'' || c == '"') literal(&l);
        else if (c == 0xe2 && cf_peek(&l, 1) == 0x80 &&
                 (cf_peek(&l, 2) == 0x98 || cf_peek(&l, 2) == 0x99 || cf_peek(&l, 2) == 0x9c || cf_peek(&l, 2) == 0x9d)) {
            cf_error_at(&l, 109, l.pos, l.line, l.column, "A curly quote is used as a literal boundary.",
                     "ClariFix literal boundaries use straight quotes.", "Replace smart quotes with ' or \".");
            cf_recover(&l);
        } else if (!symbol(&l)) {
            cf_error_at(&l, 103, l.pos, l.line, l.column, "This character is not recognized outside a literal or comment.",
                     "Names use ASCII letters, digits and underscores; operators have defined spellings.", "Remove the character or place intended text inside quotes.");
            cf_recover(&l);
        }
    }
    while (l.bracket_count && result->errors < CF_MAX_ERRORS) {
        Bracket b = l.brackets[--l.bracket_count];
        cf_error_at(&l, 102, b.offset, b.line, b.column, "An opening bracket is never closed.",
                 "Every opening bracket needs a matching closing bracket.", "Add the matching ), ], or }.");
    }
    if (!braces && l.logical_started) structural(&l, CF_NEWLINE);
    while (l.indent_count > 1) { l.indent_count--; structural(&l, CF_DEDENT); }
    structural(&l, CF_EOF);
    return result->errors ? 1 : 0;
}

