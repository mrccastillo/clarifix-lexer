#ifndef CLARIFIX_PARSER_INPUT_H
#define CLARIFIX_PARSER_INPUT_H

#include "lexer.h"

/* Cursor over a token stream produced by cf_lex() or loaded from a
   lexer token/symbol table file. */
typedef struct {
    const CfResult *result;      /* Lexer result holding tokens and symbols. */
    const char *source;          /* Source buffer or loaded lexeme text buffer. */
    size_t source_length;        /* Byte length of source buffer. */
    size_t index;                /* Index of the current lookahead token. */
    CfToken eof_token;           /* Fallback EOF token when past the end. */
} CfParserInput;

/* Initialize a parser input stream from an in-memory lexer result.
   source may be NULL if raw lexeme slices by offset are not needed. */
void parser_input_init(CfParserInput *in, const CfResult *result,
                       const char *source, size_t source_length);

/* Reset the cursor back to the first token. */
void parser_input_reset(CfParserInput *in);

/* Return the current lookahead token without advancing.
   Never returns NULL; returns an EOF token at or past the end of the stream. */
const CfToken *peek(const CfParserInput *in);

/* Return the token at ahead positions after the current token without advancing.
   peek_ahead(in, 0) is equivalent to peek(in). */
const CfToken *peek_ahead(const CfParserInput *in, size_t ahead);

/* Return the current token and advance the cursor by one token.
   Once at CF_EOF, stays at CF_EOF and returns the EOF token. */
const CfToken *next_token(CfParserInput *in);

/* If the current lookahead token has the given type, advance the cursor
   and return 1; otherwise leave the cursor unchanged and return 0. */
int match(CfParserInput *in, CfTokenType type);

/* Return 1 if the current lookahead token is CF_EOF, or 0 otherwise. */
int parser_input_at_end(const CfParserInput *in);

/* Look up the symbol table entry for a token (or NULL if token->symbol == 0). */
const CfSymbol *parser_input_symbol(const CfParserInput *in, const CfToken *token);

/* Copy or point to a token's lexeme text; sets *length if length is non-NULL.
   For symbol-backed tokens, returns the symbol's NUL-terminated lexeme.
   For other tokens, returns a pointer into source (if available) or "". */
const char *parser_input_lexeme(const CfParserInput *in, const CfToken *token,
                                size_t *length);

/* Parse a lexer token/symbol table stream (--tokens TSV, --table TSV,
   Symbol Table.txt bordered table, or --report four-section report) into
   result and initialize the parser input cursor over it.
   Use static CfResult storage to avoid a large stack frame.
   Returns 0 on success, or 2 on I/O or format error. */
int parser_input_load_stream(CfParserInput *in, CfResult *result, FILE *stream);

/* Load a lexer token/symbol table file by path into result and initialize in.
   Returns 0 on success, or 2 on I/O or format error. */
int parser_input_load_file(CfParserInput *in, CfResult *result, const char *path);

#endif
