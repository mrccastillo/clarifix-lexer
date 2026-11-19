#include "parser_input.h"
#include "internal.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void build_eof(CfParserInput *in)
{
    size_t line = 1, column = 1, offset = in->source_length;
    if (in->result && in->result->token_count > 0) {
        const CfToken *last = &in->result->tokens[in->result->token_count - 1];
        if (last->type == CF_EOF) {
            in->eof_token = *last;
            return;
        }
        line = last->line;
        column = last->column + last->length;
        offset = last->offset + last->length;
    }
    in->eof_token = (CfToken){CF_EOF, offset, 0, line, column, 0};
}

void parser_input_init(CfParserInput *in, const CfResult *result,
                       const char *source, size_t source_length)
{
    if (!in) return;
    memset(in, 0, sizeof(*in));
    in->result = result;
    if (source) {
        in->source = source;
        in->source_length = source_length;
    } else if (result) {
        in->source = result->text;
        in->source_length = result->text_used;
    }
    build_eof(in);
}

void parser_input_reset(CfParserInput *in)
{
    if (!in) return;
    in->index = 0;
}

const CfToken *peek_ahead(const CfParserInput *in, size_t ahead)
{
    static const CfToken empty_eof = {CF_EOF, 0, 0, 1, 1, 0};
    if (!in) return &empty_eof;
    if (!in->result || in->result->token_count == 0) return &in->eof_token;
    if (ahead >= in->result->token_count - in->index) return &in->eof_token;
    return &in->result->tokens[in->index + ahead];
}

const CfToken *peek(const CfParserInput *in)
{
    return peek_ahead(in, 0);
}

const CfToken *next_token(CfParserInput *in)
{
    const CfToken *tok = peek(in);
    if (!in || !in->result || in->index >= in->result->token_count) return tok;
    if (tok->type == CF_EOF && in->index + 1 == in->result->token_count) return tok;
    in->index++;
    return tok;
}

int match(CfParserInput *in, CfTokenType type)
{
    const CfToken *tok = peek(in);
    if (tok->type != type) return 0;
    next_token(in);
    return 1;
}

int parser_input_at_end(const CfParserInput *in)
{
    return peek(in)->type == CF_EOF;
}

const CfSymbol *parser_input_symbol(const CfParserInput *in, const CfToken *token)
{
    if (!in || !in->result || !token || token->symbol == 0) return NULL;
    if (token->symbol > in->result->symbol_count) return NULL;
    return &in->result->symbols[token->symbol - 1];
}

const char *parser_input_lexeme(const CfParserInput *in, const CfToken *token,
                                size_t *length)
{
    if (length) *length = 0;
    if (!in || !token) return "";
    const CfSymbol *sym = parser_input_symbol(in, token);
    if (sym && sym->lexeme) {
        if (length) *length = strlen(sym->lexeme);
        return sym->lexeme;
    }
    if (in->source && token->offset + token->length <= in->source_length) {
        if (length) *length = token->length;
        return in->source + token->offset;
    }
    return "";
}

/* Map either internal token names ("IDENT", "PERIOD") or public report names
   ("IDENTIFIER", "CONST_INT", "STATEMENT_TERMINATOR") to CfTokenType. */
static int parse_token_type(const char *name, CfTokenType *out)
{
    if (strcmp(name, "IDENTIFIER") == 0) { *out = CF_IDENT; return 1; }
    if (strcmp(name, "CONST_INT") == 0) { *out = CF_INT; return 1; }
    if (strcmp(name, "CONST_DECIMAL") == 0) { *out = CF_DECIMAL; return 1; }
    if (strcmp(name, "CONST_CHAR") == 0) { *out = CF_CHAR; return 1; }
    if (strcmp(name, "CONST_STRING") == 0) { *out = CF_STRING; return 1; }
    if (strcmp(name, "CONST_BOOLEAN") == 0) { *out = CF_BOOL; return 1; }
    if (strcmp(name, "SINGLE_COMMENT") == 0) { *out = CF_LINE_COMMENT; return 1; }
    if (strcmp(name, "MULTI_COMMENT") == 0) { *out = CF_BLOCK_COMMENT; return 1; }
    if (strcmp(name, "STATEMENT_TERMINATOR") == 0) { *out = CF_PERIOD; return 1; }
    for (int t = 0; t < (int)CF_TOKEN_COUNT; t++) {
        if (strcmp(name, cf_token_name((CfTokenType)t)) == 0) {
            *out = (CfTokenType)t;
            return 1;
        }
    }
    return 0;
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

/* Decode an escaped cell string (\n, \t, \r, \\, \xHH) in place. */
static size_t unescape_cell(char *s)
{
    size_t src = 0, dst = 0;
    while (s[src]) {
        if (s[src] == '\\' && s[src + 1]) {
            char next = s[src + 1];
            if (next == 'n') { s[dst++] = '\n'; src += 2; continue; }
            if (next == 't') { s[dst++] = '\t'; src += 2; continue; }
            if (next == 'r') { s[dst++] = '\r'; src += 2; continue; }
            if (next == '\\') { s[dst++] = '\\'; src += 2; continue; }
            if (next == 'x' && hex_value(s[src + 2]) >= 0 && hex_value(s[src + 3]) >= 0) {
                int hi = hex_value(s[src + 2]);
                int lo = hex_value(s[src + 3]);
                s[dst++] = (char)((hi << 4) | lo);
                src += 4;
                continue;
            }
        }
        s[dst++] = s[src++];
    }
    s[dst] = '\0';
    return dst;
}

static size_t save_result_text(CfResult *r, const char *s, size_t n)
{
    cf_require_room(n, CF_TEXT_SIZE - r->text_used, "symbol text bytes");
    size_t offset = r->text_used;
    memcpy(r->text + offset, s, n);
    r->text[offset + n] = '\0';
    r->text_used += n + 1;
    return offset;
}

static int parse_size(const char *s, size_t *out)
{
    if (!s || !*s) return 0;
    size_t val = 0;
    for (const char *p = s; *p; p++) {
        if (*p < '0' || *p > '9') return 0;
        val = val * 10 + (size_t)(*p - '0');
    }
    *out = val;
    return 1;
}

static int append_loaded_token(CfResult *r, CfTokenType type,
                               const char *lexeme, size_t len,
                               size_t line, size_t col)
{
    /* Skip comments and pure noise words when loading from report/table exports,
       while turning 'to' noise words into CF_ASSIGN just like cf_lex(). */
    if (type == CF_LINE_COMMENT || type == CF_BLOCK_COMMENT) return 0;
    if (type == CF_NOISE) {
        if (len == 2 && memcmp(lexeme, "to", 2) == 0) type = CF_ASSIGN;
        else return 0;
    }

    size_t offset = save_result_text(r, lexeme, len);
    size_t symbol = 0;
    if (type == CF_IDENT || (type >= CF_INT && type <= CF_BOOL)) {
        Lexer dummy;
        memset(&dummy, 0, sizeof(dummy));
        dummy.r = r;
        symbol = cf_symbol_insert(&dummy, r->text + offset, len, type, line, col);
    }
    cf_require_room(r->token_count, CF_MAX_TOKENS, "parser tokens");
    r->tokens[r->token_count++] = (CfToken){type, offset, len, line, col, symbol};
    if (len) {
        cf_require_room(r->report_count, CF_MAX_TOKENS, "source tokens");
        r->report[r->report_count++] = (CfToken){type, offset, len, line, col, symbol};
    }
    return 0;
}

static char *trim_spaces(char *s)
{
    while (*s == ' ') s++;
    size_t n = strlen(s);
    while (n > 0 && s[n - 1] == ' ') s[--n] = '\0';
    return s;
}

#define LINE_BUFFER_SIZE (4 * CF_MAX_SOURCE + 256)

int parser_input_load_stream(CfParserInput *in, CfResult *result, FILE *stream)
{
    static char line_buf[LINE_BUFFER_SIZE];
    if (!in || !result || !stream) return 2;
    memset(result, 0, sizeof(*result));

    typedef enum {
        FORMAT_UNKNOWN,
        FORMAT_TOKENS_TSV,
        FORMAT_TABLE_TSV,
        FORMAT_BORDERED,
        FORMAT_REPORT
    } TableFormat;

    TableFormat format = FORMAT_UNKNOWN;
    int in_report_section_a = 0;
    size_t col_starts[6] = {0};
    size_t col_ends[6] = {0};
    int have_report_columns = 0;
    size_t auto_line = 1;

    while (fgets(line_buf, (int)sizeof(line_buf), stream)) {
        size_t len = strlen(line_buf);
        while (len > 0 && (line_buf[len - 1] == '\n' || line_buf[len - 1] == '\r'))
            line_buf[--len] = '\0';

        if (format == FORMAT_UNKNOWN) {
            if (len == 0) continue;
            if (strcmp(line_buf, "LINE\tCOLUMN\tTOKEN\tLEXEME\tSYMBOL_ID") == 0) {
                format = FORMAT_TOKENS_TSV;
                continue;
            }
            if (strcmp(line_buf, "TOKEN TYPE\tLEXEME") == 0) {
                format = FORMAT_TABLE_TSV;
                continue;
            }
            if (line_buf[0] == '+' && strchr(line_buf, '-') != NULL) {
                format = FORMAT_BORDERED;
                continue;
            }
            if (strncmp(line_buf, "======", 6) == 0 ||
                strncmp(line_buf, "[A] TOKEN LIST", 14) == 0) {
                format = FORMAT_REPORT;
                if (strncmp(line_buf, "[A] TOKEN LIST", 14) == 0)
                    in_report_section_a = 1;
                continue;
            }
            return 2;
        }

        if (format == FORMAT_TOKENS_TSV) {
            if (len == 0) continue;
            char *c1 = line_buf;
            char *c2 = strchr(c1, '\t'); if (!c2) return 2; *c2++ = '\0';
            char *c3 = strchr(c2, '\t'); if (!c3) return 2; *c3++ = '\0';
            char *c4 = strchr(c3, '\t'); if (!c4) return 2; *c4++ = '\0';
            char *c5 = strchr(c4, '\t'); if (!c5) return 2; *c5++ = '\0';

            size_t line_no = 0, col_no = 0, sym_id = 0;
            CfTokenType type;
            if (!parse_size(c1, &line_no) || !parse_size(c2, &col_no) ||
                !parse_token_type(c3, &type) || !parse_size(c5, &sym_id))
                return 2;
            size_t lex_len = unescape_cell(c4);
            append_loaded_token(result, type, c4, lex_len, line_no, col_no);
        } else if (format == FORMAT_TABLE_TSV) {
            if (len == 0) continue;
            char *c1 = line_buf;
            char *c2 = strchr(c1, '\t'); if (!c2) return 2; *c2++ = '\0';
            CfTokenType type;
            if (!parse_token_type(c1, &type)) return 2;
            size_t lex_len = unescape_cell(c2);
            append_loaded_token(result, type, c2, lex_len, auto_line++, 1);
        } else if (format == FORMAT_BORDERED) {
            if (len == 0 || line_buf[0] == '+') continue;
            if (strcmp(line_buf, "(none)") == 0) continue;
            if (line_buf[0] != '|') return 2;
            char *first_bar = line_buf;
            char *mid_bar = strchr(first_bar + 1, '|');
            if (!mid_bar) return 2;
            char *last_bar = strrchr(mid_bar + 1, '|');
            if (!last_bar) return 2;
            *mid_bar = '\0';
            *last_bar = '\0';
            char *type_str = trim_spaces(first_bar + 1);
            char *lex_str = trim_spaces(mid_bar + 1);
            if (strcmp(type_str, "TOKEN TYPE") == 0 && strcmp(lex_str, "LEXEME") == 0)
                continue;
            CfTokenType type;
            if (!parse_token_type(type_str, &type)) return 2;
            size_t lex_len = unescape_cell(lex_str);
            append_loaded_token(result, type, lex_str, lex_len, auto_line++, 1);
        } else if (format == FORMAT_REPORT) {
            if (strncmp(line_buf, "[A] TOKEN LIST", 14) == 0) {
                in_report_section_a = 1;
                continue;
            }
            if (strncmp(line_buf, "[B] SYMBOL TABLE", 16) == 0) {
                break;
            }
            if (!in_report_section_a || len == 0) continue;
            if (strncmp(line_buf, "No.", 3) == 0) continue;
            if (strncmp(line_buf, "---", 3) == 0) {
                size_t col = 0, i = 0;
                while (i < len && col < 6) {
                    while (i < len && line_buf[i] == ' ') i++;
                    if (i >= len) break;
                    col_starts[col] = i;
                    while (i < len && line_buf[i] == '-') i++;
                    col_ends[col] = i;
                    col++;
                }
                if (col != 6) return 2;
                have_report_columns = 1;
                continue;
            }
            if (strcmp(line_buf, "(none)") == 0) continue;
            if (!have_report_columns || len < col_starts[5]) return 2;

            line_buf[col_ends[0]] = '\0';
            line_buf[col_ends[1]] = '\0';
            line_buf[col_ends[2]] = '\0';
            line_buf[col_ends[3]] = '\0';
            line_buf[col_ends[4]] = '\0';

            char *line_str = trim_spaces(line_buf + col_starts[1]);
            char *col_str = trim_spaces(line_buf + col_starts[2]);
            char *lex_str = trim_spaces(line_buf + col_starts[3]);
            char *type_str = trim_spaces(line_buf + col_starts[4]);

            size_t line_no = 0, col_no = 0;
            CfTokenType type;
            if (!parse_size(line_str, &line_no) || !parse_size(col_str, &col_no) ||
                !parse_token_type(type_str, &type))
                return 2;
            size_t lex_len = unescape_cell(lex_str);
            append_loaded_token(result, type, lex_str, lex_len, line_no, col_no);
        }
    }

    if (ferror(stream) || format == FORMAT_UNKNOWN) return 2;

    if (result->token_count == 0 ||
        result->tokens[result->token_count - 1].type != CF_EOF) {
        size_t eof_line = 1, eof_col = 1;
        if (result->token_count > 0) {
            const CfToken *last = &result->tokens[result->token_count - 1];
            eof_line = last->line;
            eof_col = last->column + last->length;
        }
        cf_require_room(result->token_count, CF_MAX_TOKENS, "parser tokens");
        result->tokens[result->token_count++] =
            (CfToken){CF_EOF, result->text_used, 0, eof_line, eof_col, 0};
    }

    parser_input_init(in, result, result->text, result->text_used);
    return 0;
}

int parser_input_load_file(CfParserInput *in, CfResult *result, const char *path)
{
    if (!in || !result || !path) return 2;
    FILE *file = fopen(path, "rb");
    if (!file) { perror(path); return 2; }
    int status = parser_input_load_stream(in, result, file);
    fclose(file);
    return status;
}
