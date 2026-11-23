#ifndef CLARIFIX_PARSER_H
#define CLARIFIX_PARSER_H

#include "parser_input.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/* Fixed storage limits for syntax analysis. No heap allocation is used. */
#define CF_MAX_AST_NODES 32768
#define CF_MAX_PARSE_DEPTH 256

/* Index 0 represents "no AST node". Valid node IDs are 1..node_count. */
#define CF_NODE_NONE 0u

typedef enum {
    CF_NODE_PROGRAM = 1,      /* Top-level statement list (first_child..next_sibling). */
    CF_NODE_SHOW_STMT,        /* Output statement: show <expr> . (left = expression). */
    CF_NODE_LITERAL,          /* INT, DECIMAL, STRING, CHAR, or BOOL constant. */
    CF_NODE_IDENT,            /* Identifier reference. */
    CF_NODE_GROUP,            /* Parenthesized expression: ( <expr> ) (left = inner). */
    CF_NODE_ARRAY,            /* Array literal: [ <args> ] (first_child..next_sibling). */
    CF_NODE_UNARY,            /* Unary prefix: -, not, ! (op, left = operand). */
    CF_NODE_POSTFIX,          /* Postfix operator: ++, -- (op, left = operand). */
    CF_NODE_BINARY,           /* Binary operator: left op right (includes + concat). */
    CF_NODE_CALL,             /* Function call: callee(args) (left = callee, children = args). */
    CF_NODE_INDEX             /* Indexing: target[index] (left = target, right = index). */
} CfNodeKind;

typedef struct {
    CfNodeKind kind;
    CfToken token;            /* Representative token (keyword, operator, literal, opener). */
    CfTokenType op;           /* Operator token type for UNARY, BINARY, and POSTFIX nodes. */
    int op_negated;           /* Non-zero for the two-token "is not" equality operator. */
    uint32_t left;            /* LHS / operand / callee / show expr / group expr / target. */
    uint32_t right;           /* RHS / index expression. */
    uint32_t first_child;     /* Head of intrusive child list (statements, call args, elements). */
    uint32_t last_child;      /* Tail of intrusive child list for O(1) append. */
    uint32_t next_sibling;    /* Next sibling in parent's child list. */
    uint32_t child_count;     /* Number of nodes in the child list. */
} CfAstNode;

typedef struct {
    size_t line, column;
    int code;
    char what[160];
} CfSyntaxError;

typedef struct {
    CfAstNode nodes[CF_MAX_AST_NODES];
    uint32_t node_count;
    uint32_t root;                          /* CF_NODE_PROGRAM node ID, or CF_NODE_NONE. */
    CfSyntaxError error_list[CF_MAX_ERRORS];
    size_t errors;
} CfParseResult;

typedef struct {
    CfParserInput *in;
    FILE *diagnostics;
    CfParseResult *result;
    size_t depth;                           /* Current expression recursion depth. */
} CfParser;

/* Initialize a parser over an existing CfParserInput token stream.
   Resets result. Use static CfParseResult storage to avoid a large stack. */
void cf_parser_init(CfParser *p, CfParserInput *in,
                    FILE *diagnostics, CfParseResult *result);

/* Look up an AST node by 1-based node ID (returns NULL for CF_NODE_NONE or out of range). */
const CfAstNode *cf_ast_node(const CfParseResult *result, uint32_t id);

/* Human-readable names for AST node kinds and operators. */
const char *cf_node_kind_name(CfNodeKind kind);
const char *cf_ast_op_name(CfTokenType op, int negated);

/* Parse an expression following the ClariFix precedence hierarchy:
   or -> and -> not -> equality -> relation -> sum -> term ->
   unary -> power -> postfix (calls, indexing, ++/--) -> primary. */
uint32_t parse_expr(CfParser *p);

/* Parse a single output statement: show <expr> .
   Supports string concatenation (+) and function-call arguments. */
uint32_t parse_show_stmt(CfParser *p);

/* Parse a single statement at the current cursor position. */
uint32_t parse_statement(CfParser *p);

/* Parse all statements in the token stream until CF_EOF.
   Returns the CF_NODE_PROGRAM root node ID. */
uint32_t parse_program(CfParser *p);

/* Convenience driver: initialize a parser and parse the entire token stream.
   Returns 0 on syntax success, or 1 if one or more syntax errors occurred. */
int cf_parse(CfParserInput *in, FILE *diagnostics, CfParseResult *result);

/* Print a human-readable indented representation of the parsed AST.
   Returns 0 on success, or 2 on write error. */
int cf_write_ast(FILE *out, const CfParserInput *in, const CfParseResult *result);

#endif
