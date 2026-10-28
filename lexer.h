#ifndef CLARIFIX_LEXER_H
#define CLARIFIX_LEXER_H

#include <stddef.h>
#include <stdio.h>

/* Shared token vocabulary for a future parser. */
#define CF_TOKEN_TYPES(X) \
    X(EOF) X(IDENT) X(INT) X(DECIMAL) X(STRING) X(CHAR) X(BOOL) \
    X(SET) X(SHOW) X(ASK) X(IF) X(ELIF) X(ELSE) X(WHILE) X(FOR) \
    X(IN) X(FUNCTION) X(RETURN) X(BREAK) X(CONTINUE) X(NOISE) \
    X(PLUS) X(MINUS) X(STAR) X(SLASH) X(FLOOR_DIV) X(MOD) X(POWER) \
    X(INCREMENT) X(DECREMENT) X(AND) X(OR) X(NOT) X(IS) \
    X(EQ) X(NE) X(LT) X(GT) X(LE) X(GE) X(ASSIGN) \
    X(PLUS_ASSIGN) X(MINUS_ASSIGN) X(STAR_ASSIGN) X(SLASH_ASSIGN) \
    X(MOD_ASSIGN) X(POWER_ASSIGN) X(LPAREN) X(RPAREN) \
    X(LBRACKET) X(RBRACKET) X(LBRACE) X(RBRACE) X(COMMA) \
    X(PERIOD) X(COLON) X(NEWLINE) X(INDENT) X(DEDENT) \
    X(LINE_COMMENT) X(BLOCK_COMMENT)

typedef enum {
#define CF_ENUM(name) CF_##name,
    CF_TOKEN_TYPES(CF_ENUM)
#undef CF_ENUM
    CF_TOKEN_COUNT
} CfTokenType;

typedef struct {
    CfTokenType type;
    size_t offset, length;       /* Original spelling in the source buffer. */
    size_t line, column;         /* One-based; columns count UTF-8 bytes. */
    size_t symbol;               /* One-based symbol ID; 0 if not interned. */
} CfToken;

typedef struct {
    char *lexeme;
    CfTokenType type;
    char *value;                 /* Decoded text or exact normalized decimal value. */
    size_t length;              /* Unicode-scalar count of the source lexeme. */
    size_t line, column, occurrences;
    size_t identity, next;       /* Trie terminal key; one-based collision-chain link. */
} CfSymbol;

#define CF_SYMBOL_BUCKETS 211
typedef struct {
    size_t offset, length, line, column;
    int code;
    const char *reason;
} CfInvalid;

typedef struct {
    CfToken *tokens;
    size_t token_count, token_capacity;
    CfToken *report;             /* Source lexemes, including comments and noise. */
    size_t report_count, report_capacity;
    CfSymbol *symbols;
    size_t symbol_count, symbol_capacity;
    size_t buckets[CF_SYMBOL_BUCKETS];
    CfInvalid *invalid;
    size_t invalid_count, invalid_capacity;
    size_t errors;
} CfResult;

const char *cf_token_name(CfTokenType type);
/* Source must outlive the result. Use LF newlines; main.c normalizes files.
   braces = 0 generates layout tokens; braces = 1 uses explicit { } blocks.
   Diagnostics go to diagnostics. Each call starts a fresh symbol table. */
int cf_lex(const char *source, size_t length, int braces,
           FILE *diagnostics, CfResult *result);
int cf_write_tokens(FILE *out, const char *source, const CfResult *result);
int cf_write_symbols(FILE *out, const CfResult *result);
int cf_write_report(FILE *out, const char *source, const char *source_name, const CfResult *result);
int cf_write_table(FILE *out, const char *source, const CfResult *result);
void cf_free(CfResult *result);

#endif
