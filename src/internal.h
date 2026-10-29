#ifndef CLARIFIX_INTERNAL_H
#define CLARIFIX_INTERNAL_H

#include "lexer.h"

/* Private scanner data shared by lexer.c, symbols.c, and diagnostics.c.
   main.c uses only the public interface in lexer.h. */
typedef struct {
    char close;
    size_t offset, line, column;
} Bracket;

typedef struct {
    const char *s;
    size_t length, pos, line, column;
    size_t indent_width, statement;
    int physical_started, logical_started, indent_tab, braces;
    size_t indents[CF_MAX_DEPTH], indent_count;
    Bracket brackets[CF_MAX_DEPTH];
    size_t bracket_count;
    FILE *diagnostics, *trace;
    CfResult *r;
} Lexer;

/* lexer.c: cursor movement, character checks, and fixed-storage checks. */
void cf_require_room(size_t count, size_t limit, const char *name);
int cf_letter(unsigned char c);
int cf_digit(unsigned char c);
unsigned char cf_peek(const Lexer *l, size_t ahead);
unsigned char cf_advance(Lexer *l);
size_t cf_utf8_span(const char *s, size_t n);
size_t cf_scalar_count(const char *s, size_t n);

/* symbols.c: add a new symbol or return the existing ID. */
size_t cf_symbol_insert(Lexer *l, const char *s, size_t n, CfTokenType type,
                        size_t line, size_t column);

/* diagnostics.c: state tracing, lexical errors, and line recovery. */
void cf_trace_step_at(const Lexer *l, const char *machine, const char *from,
                       unsigned char c, const char *to, size_t ahead);
void cf_trace_step(const Lexer *l, const char *machine, const char *from,
                    unsigned char c, const char *to);
void cf_error_at(Lexer *l, int code, size_t offset, size_t line, size_t col,
                  const char *what, const char *why, const char *fix);
void cf_recover(Lexer *l);

#endif
