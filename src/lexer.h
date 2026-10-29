#ifndef CLARIFIX_LEXER_H
#define CLARIFIX_LEXER_H

#include <stddef.h>
#include <stdio.h>

/* Fixed storage limits. No heap allocation is used by the analyzer. */
#define CF_MAX_SOURCE 262144
#define CF_MAX_TOKENS 32768
#define CF_MAX_SYMBOLS 8192
#define CF_MAX_DEPTH 256
#define CF_MAX_ERRORS 5
#define CF_TEXT_SIZE (2 * CF_MAX_SOURCE + 2 * CF_MAX_SYMBOLS)
#define CF_SYMBOL_BUCKETS 211

typedef enum {
    CF_EOF,
    CF_IDENT,
    CF_INT,
    CF_DECIMAL,
    CF_STRING,
    CF_CHAR,
    CF_BOOL,
    CF_SET,
    CF_SHOW,
    CF_ASK,
    CF_IF,
    CF_ELIF,
    CF_ELSE,
    CF_WHILE,
    CF_FOR,
    CF_IN,
    CF_FUNCTION,
    CF_RETURN,
    CF_BREAK,
    CF_CONTINUE,
    CF_NOISE,
    CF_PLUS,
    CF_MINUS,
    CF_STAR,
    CF_SLASH,
    CF_FLOOR_DIV,
    CF_MOD,
    CF_POWER,
    CF_INCREMENT,
    CF_DECREMENT,
    CF_AND,
    CF_OR,
    CF_NOT,
    CF_IS,
    CF_EQ,
    CF_NE,
    CF_LT,
    CF_GT,
    CF_LE,
    CF_GE,
    CF_ASSIGN,
    CF_PLUS_ASSIGN,
    CF_MINUS_ASSIGN,
    CF_STAR_ASSIGN,
    CF_SLASH_ASSIGN,
    CF_MOD_ASSIGN,
    CF_POWER_ASSIGN,
    CF_LPAREN,
    CF_RPAREN,
    CF_LBRACKET,
    CF_RBRACKET,
    CF_LBRACE,
    CF_RBRACE,
    CF_COMMA,
    CF_PERIOD,
    CF_COLON,
    CF_NEWLINE,
    CF_INDENT,
    CF_DEDENT,
    CF_LINE_COMMENT,
    CF_BLOCK_COMMENT,
    CF_TOKEN_COUNT
} CfTokenType;

typedef struct {
    CfTokenType type;
    size_t offset, length;       /* Location of the lexeme in the source text. */
    size_t line, column;         /* One-based source location. */
    size_t symbol;               /* Symbol ID, or 0 for keywords/punctuation. */
} CfToken;

typedef struct {
    const char *lexeme;          /* Points into the fixed text array below. */
    CfTokenType type;
    const char *value;           /* NULL for identifiers: we do not execute code. */
    size_t length, line, column, occurrences;
    size_t next;                /* Next symbol ID in the same hash bucket. */
} CfSymbol;

typedef struct {
    size_t offset, length, line, column;
    int code;
    const char *reason;
} CfInvalid;

typedef struct {
    CfToken tokens[CF_MAX_TOKENS];  /* Parser-oriented tokens. */
    size_t token_count;
    CfToken report[CF_MAX_TOKENS];  /* Source lexemes, including comments/noise. */
    size_t report_count;
    CfSymbol symbols[CF_MAX_SYMBOLS];
    size_t symbol_count;
    size_t buckets[CF_SYMBOL_BUCKETS];
    CfInvalid invalid[CF_MAX_ERRORS];
    size_t invalid_count, errors;
    char text[CF_TEXT_SIZE];       /* Saved symbol spellings and decoded values. */
    size_t text_used;
} CfResult;

const char *cf_token_name(CfTokenType type);
/* Source must remain alive until all outputs are written. LF newlines expected.
   braces = 0: four-space indentation. braces = 1: explicit { } blocks.
   trace = NULL: normal output. Otherwise show state transitions on that stream.
   Each call resets result. Use static CfResult storage to avoid a large stack. */
int cf_lex(const char *source, size_t length, int braces,
           FILE *diagnostics, FILE *trace, CfResult *result);
int cf_write_tokens(FILE *out, const char *source, const CfResult *result);
int cf_write_token_table(FILE *out, const char *source, const CfResult *result);
int cf_write_symbols(FILE *out, const CfResult *result);
int cf_write_report(FILE *out, const char *source, const char *source_name, const CfResult *result);
int cf_write_table(FILE *out, const char *source, const CfResult *result);

#endif
