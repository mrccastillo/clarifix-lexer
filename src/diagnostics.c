#include "internal.h"

/* Trace uses the same state transitions as recognition. No second scanner. */
void cf_trace_step_at(const Lexer *l, const char *machine, const char *from,
                          unsigned char c, const char *to, size_t ahead)
{
    if (!l->trace) return;
    fprintf(l->trace, "TRACE %zu:%zu %s %s -- ", l->line, l->column + ahead, machine, from);
    switch (c) {
    case 0: fputs(l->pos + ahead == l->length ? "EOF" : "NUL", l->trace); break;
    case ' ': fputs("SPACE", l->trace); break;
    case '\n': fputs("NEWLINE", l->trace); break;
    case '\t': fputs("TAB", l->trace); break;
    default:
        if (c < 32 || c >= 127) fprintf(l->trace, "0x%02X", c);
        else fprintf(l->trace, "'%c'", c);
    }
    fprintf(l->trace, " --> %s\n", to);
}

void cf_trace_step(const Lexer *l, const char *machine, const char *from,
                       unsigned char c, const char *to)
{
    cf_trace_step_at(l, machine, from, c, to, 0);
}

void cf_error_at(Lexer *l, int code, size_t offset, size_t line, size_t col,
                     const char *what, const char *why, const char *fix)
{
    if (l->r->errors >= CF_MAX_ERRORS) return;
    if (l->trace) fprintf(l->trace, "TRACE %zu:%zu REJECT E%d\n", line, col, code);
    size_t begin = offset, end = offset;
    while (begin && l->s[begin - 1] != '\n') begin--;
    while (end < l->length && l->s[end] != '\n') end++;
    size_t bad_start = offset, bad_end = offset;
    if (code == 107 || code == 108) {
        bad_start = begin; bad_end = begin;
        while (bad_end < end && (l->s[bad_end] == ' ' || l->s[bad_end] == '\t')) bad_end++;
    } else if (code == 104 || code == 105 || code == 110) {
        while (bad_end < end && (cf_letter((unsigned char)l->s[bad_end]) ||
               cf_digit((unsigned char)l->s[bad_end]) ||
               (code == 110 && l->s[bad_end] == '.' && bad_end + 1 < end && cf_digit((unsigned char)l->s[bad_end + 1])))) bad_end++;
    } else if (code == 106) bad_end = l->length;
    else if ((code == 102 || code == 112) && offset < end &&
             (l->s[offset] == '\'' || l->s[offset] == '"')) {
        bad_end = code == 112 ? l->pos : end;
    } else if (code == 111) bad_end = offset + 2 <= end ? offset + 2 : end;
    else if (offset < end) {
        size_t bytes = cf_utf8_span(l->s + offset, end - offset);
        bad_end = offset + (bytes ? bytes : 1);
    }
    if (bad_end == bad_start && bad_start < end) bad_end++;
    cf_require_room(l->r->invalid_count, CF_MAX_ERRORS, "errors");
    l->r->invalid[l->r->invalid_count++] = (CfInvalid){bad_start, bad_end - bad_start, line, col, code, what};
    fprintf(l->diagnostics, "Error E%d (Lexical Error) at line %zu, column %zu\n  ", code, line, col);
    /* Escape invalid bytes and controls; track the rendered caret position. */
    size_t caret = 0;
    for (size_t i = begin; i < end;) {
        unsigned char c = (unsigned char)l->s[i];
        size_t bytes = cf_utf8_span(l->s + i, end - i), width = 1;
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
    if (++l->r->errors == CF_MAX_ERRORS)
        fputs("Fix these first, then run again\n", l->diagnostics);
}
void cf_recover(Lexer *l)
{
    if (l->trace) fprintf(l->trace, "TRACE %zu:%zu RECOVER skip rest of line\n", l->line, l->column);
    while (l->pos < l->length && cf_peek(l, 0) != '\n') cf_advance(l);
    /* A damaged line must not leave its own openers on the bracket stack. */
    while (l->bracket_count && l->brackets[l->bracket_count - 1].line == l->line)
        l->bracket_count--;
    l->statement = l->r->token_count;
}
