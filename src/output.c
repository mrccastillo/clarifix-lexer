#include "internal.h"

#include <string.h>

const char *cf_token_name(CfTokenType type)
{
    static const char *names[] = {
        "EOF",
        "IDENT",
        "INT",
        "DECIMAL",
        "STRING",
        "CHAR",
        "BOOL",
        "SET",
        "SHOW",
        "ASK",
        "IF",
        "ELIF",
        "ELSE",
        "WHILE",
        "FOR",
        "IN",
        "FUNCTION",
        "RETURN",
        "BREAK",
        "CONTINUE",
        "NOISE",
        "PLUS",
        "MINUS",
        "STAR",
        "SLASH",
        "FLOOR_DIV",
        "MOD",
        "POWER",
        "INCREMENT",
        "DECREMENT",
        "AND",
        "OR",
        "NOT",
        "IS",
        "EQ",
        "NE",
        "LT",
        "GT",
        "LE",
        "GE",
        "ASSIGN",
        "PLUS_ASSIGN",
        "MINUS_ASSIGN",
        "STAR_ASSIGN",
        "SLASH_ASSIGN",
        "MOD_ASSIGN",
        "POWER_ASSIGN",
        "LPAREN",
        "RPAREN",
        "LBRACKET",
        "RBRACKET",
        "LBRACE",
        "RBRACE",
        "COMMA",
        "PERIOD",
        "COLON",
        "NEWLINE",
        "INDENT",
        "DEDENT",
        "LINE_COMMENT",
        "BLOCK_COMMENT",
    };
    return (unsigned)type < CF_TOKEN_COUNT ? names[type] : "UNKNOWN";
}

static void escaped(FILE *out, const char *s, size_t n)
{
    for (size_t i = 0; i < n;) {
        unsigned char c = (unsigned char)s[i];
        size_t bytes = cf_utf8_span(s + i, n - i);
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
/* The longest escaped source lexeme is four bytes per input byte.
   The caller supplies a fixed cell buffer; this function never allocates. */
static void escaped_text(char *text, const char *s, size_t n)
{
    const char hex[] = "0123456789ABCDEF";
    size_t dest = 0;
    for (size_t i = 0; i < n;) {
        unsigned char c = (unsigned char)s[i];
        size_t bytes = cf_utf8_span(s + i, n - i);
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
/* Table formatting uses one reusable, fixed-size row, not an allocated
   string for every cell. Two passes measure the columns, then print rows. */
#define CELL_SIZE (4 * CF_MAX_SOURCE + 128)
typedef enum { TABLE_TERMINAL, TABLE_TOKENS, TABLE_SYMBOLS, TABLE_INVALID } TableKind;

static void table_row(TableKind kind, size_t row, const char *source, const CfResult *r,
                      char cells[6][CELL_SIZE])
{
    switch (kind) {
    case TABLE_TERMINAL: {
        const CfToken *t = &r->report[row];
        snprintf(cells[0], CELL_SIZE, "%s", public_name(t->type));
        escaped_text(cells[1], source + t->offset, t->length);
        break;
    }
    case TABLE_TOKENS: {
        const CfToken *t = &r->report[row];
        snprintf(cells[0], CELL_SIZE, "%zu", row + 1);
        snprintf(cells[1], CELL_SIZE, "%zu", t->line);
        snprintf(cells[2], CELL_SIZE, "%zu", t->column);
        escaped_text(cells[3], source + t->offset, t->length);
        snprintf(cells[4], CELL_SIZE, "%s", public_name(t->type));
        snprintf(cells[5], CELL_SIZE, "%s", category(t->type));
        break;
    }
    case TABLE_SYMBOLS: {
        const CfSymbol *s = &r->symbols[row];
        snprintf(cells[0], CELL_SIZE, "%zu", row + 1);
        escaped_text(cells[1], s->lexeme, strlen(s->lexeme));
        snprintf(cells[2], CELL_SIZE, "%s", public_name(s->type));
        if (s->type == CF_CHAR || s->type == CF_STRING) {
            char quote = s->type == CF_CHAR ? '\'' : '"';
            cells[3][0] = quote;
            escaped_text(cells[3] + 1, s->value, strlen(s->value));
            size_t n = strlen(cells[3]);
            cells[3][n] = quote; cells[3][n + 1] = '\0';
        } else {
            const char *value = s->value ? s->value : "-";
            escaped_text(cells[3], value, strlen(value));
        }
        snprintf(cells[4], CELL_SIZE, "%zu", s->length);
        snprintf(cells[5], CELL_SIZE, "%zu:%zu", s->line, s->column);
        break;
    }
    case TABLE_INVALID: {
        const CfInvalid *bad = &r->invalid[row];
        snprintf(cells[0], CELL_SIZE, "%zu", bad->line);
        snprintf(cells[1], CELL_SIZE, "%zu", bad->column);
        escaped_text(cells[2], source + bad->offset, bad->length);
        snprintf(cells[3], CELL_SIZE, "E%d", bad->code);
        snprintf(cells[4], CELL_SIZE, "%s", bad->reason);
        break;
    }
    }
}
static void pad(FILE *out, size_t width, char c)
{
    for (size_t i = 0; i < width; i++) fputc(c, out);
}
static void grid_rule(FILE *out, const size_t *widths, size_t columns)
{
    for (size_t c = 0; c < columns; c++) { fputc('+', out); pad(out, widths[c] + 2, '-'); }
    fputs("+\n", out);
}
static void print_table(FILE *out, const char **headers, size_t rows, size_t columns,
                        int bordered, TableKind kind, const char *source, const CfResult *r)
{
    static char cells[6][CELL_SIZE];
    size_t widths[6] = {0};
    for (size_t c = 0; c < columns; c++) widths[c] = strlen(headers[c]);
    for (size_t row = 0; row < rows; row++) {
        table_row(kind, row, source, r, cells);
        for (size_t c = 0; c < columns; c++) {
            size_t width = cf_scalar_count(cells[c], strlen(cells[c]));
            if (width > widths[c]) widths[c] = width;
        }
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
        table_row(kind, row, source, r, cells);
        if (bordered) fputs("| ", out);
        for (size_t c = 0; c < columns; c++) {
            fputs(cells[c], out);
            size_t width = cf_scalar_count(cells[c], strlen(cells[c]));
            if (bordered) {
                pad(out, widths[c] - width, ' ');
                fputs(c + 1 < columns ? " | " : " |", out);
            } else if (c + 1 < columns) { pad(out, widths[c] - width, ' '); fputs("  ", out); }
        }
        fputc('\n', out);
    }
    if (bordered) grid_rule(out, widths, columns);
}
int cf_write_token_table(FILE *out, const char *source, const CfResult *r)
{
    const char *headers[] = {"TOKEN TYPE", "LEXEME"};
    print_table(out, headers, r->report_count, 2, 1, TABLE_TERMINAL, source, r);
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
    /* Category counts: identifiers, keywords, noise, constants, delimiters, comments, operators. */
    size_t counts[7] = {0};
    for (size_t i = 0; i < r->report_count; i++) {
        const CfToken *t = &r->report[i];
        size_t group = t->type == CF_IDENT ? 0 :
                       t->type >= CF_SET && t->type <= CF_CONTINUE ? 1 :
                       t->type == CF_NOISE ? 2 :
                       t->type >= CF_INT && t->type <= CF_BOOL ? 3 :
                       t->type >= CF_LPAREN && t->type <= CF_COLON ? 4 :
                       t->type == CF_LINE_COMMENT || t->type == CF_BLOCK_COMMENT ? 5 : 6;
        counts[group]++;
    }
    print_table(out, a_headers, r->report_count, 6, 0, TABLE_TOKENS, source, r);
    fputs("\n[B] SYMBOL TABLE (unique identifiers and constants)\n", out);
    const char *b_headers[] = {"ID", "Lexeme", "Token Type", "Value", "Length", "First Seen"};
    print_table(out, b_headers, r->symbol_count, 6, 0, TABLE_SYMBOLS, source, r);
    fputs("\n[C] INVALID / UNRECOGNIZED TOKENS\n", out);
    const char *c_headers[] = {"Line", "Col", "Lexeme", "Error", "Reason"};
    print_table(out, c_headers, r->invalid_count, 5, 0, TABLE_INVALID, source, r);
    fprintf(out, "\n[D] SUMMARY\nIdentifiers: %zu | Keywords: %zu | Noise words: %zu | Constants: %zu\n"
            "Delimiters: %zu | Comments: %zu | Operators: %zu | Invalid: %zu | Total tokens: %zu\n"
            "======================================================================\n",
            counts[0], counts[1], counts[2], counts[3], counts[4], counts[5], counts[6], r->invalid_count, r->report_count);
    return ferror(out) ? 2 : 0;
}
