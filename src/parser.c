#include "parser.h"
#include "internal.h"

#include <string.h>

void cf_parser_init(CfParser *p, CfParserInput *in,
                    FILE *diagnostics, CfParseResult *result)
{
    if (!p) return;
    memset(p, 0, sizeof(*p));
    p->in = in;
    p->diagnostics = diagnostics;
    p->result = result;
    if (result) memset(result, 0, sizeof(*result));
}

const CfAstNode *cf_ast_node(const CfParseResult *result, uint32_t id)
{
    if (!result || id == CF_NODE_NONE || id > result->node_count) return NULL;
    return &result->nodes[id - 1];
}

const char *cf_node_kind_name(CfNodeKind kind)
{
    switch (kind) {
    case CF_NODE_PROGRAM: return "Program";
    case CF_NODE_SHOW_STMT: return "ShowStmt";
    case CF_NODE_LITERAL: return "Literal";
    case CF_NODE_IDENT: return "Ident";
    case CF_NODE_GROUP: return "Group";
    case CF_NODE_ARRAY: return "Array";
    case CF_NODE_UNARY: return "Unary";
    case CF_NODE_POSTFIX: return "Postfix";
    case CF_NODE_BINARY: return "Binary";
    case CF_NODE_CALL: return "Call";
    case CF_NODE_INDEX: return "Index";
    }
    return "Unknown";
}

const char *cf_ast_op_name(CfTokenType op, int negated)
{
    switch (op) {
    case CF_PLUS: return "+";
    case CF_MINUS: return "-";
    case CF_STAR: return "*";
    case CF_SLASH: return "/";
    case CF_FLOOR_DIV: return "//";
    case CF_MOD: return "%";
    case CF_POWER: return "^";
    case CF_INCREMENT: return "++";
    case CF_DECREMENT: return "--";
    case CF_AND: return "and";
    case CF_OR: return "or";
    case CF_NOT: return "not";
    case CF_IS: return negated ? "is not" : "is";
    case CF_EQ: return "==";
    case CF_NE: return "!=";
    case CF_LT: return "<";
    case CF_GT: return ">";
    case CF_LE: return "<=";
    case CF_GE: return ">=";
    default: return cf_token_name(op);
    }
}

static uint32_t alloc_node(CfParser *p, CfNodeKind kind, const CfToken *tok)
{
    cf_require_room(p->result->node_count, CF_MAX_AST_NODES, "AST nodes");
    uint32_t id = ++p->result->node_count;
    CfAstNode *node = &p->result->nodes[id - 1];
    memset(node, 0, sizeof(*node));
    node->kind = kind;
    if (tok) node->token = *tok;
    return id;
}

static void append_child(CfParser *p, uint32_t parent_id, uint32_t child_id)
{
    if (parent_id == CF_NODE_NONE || child_id == CF_NODE_NONE) return;
    CfAstNode *parent = &p->result->nodes[parent_id - 1];
    CfAstNode *child = &p->result->nodes[child_id - 1];
    child->next_sibling = CF_NODE_NONE;
    if (parent->first_child == CF_NODE_NONE) {
        parent->first_child = child_id;
        parent->last_child = child_id;
    } else {
        p->result->nodes[parent->last_child - 1].next_sibling = child_id;
        parent->last_child = child_id;
    }
    parent->child_count++;
}

static int is_stmt_boundary(const CfToken *tok)
{
    switch (tok->type) {
    case CF_NEWLINE:
    case CF_EOF:
    case CF_DEDENT:
    case CF_RBRACE:
    case CF_SHOW:
    case CF_SET:
    case CF_ASK:
    case CF_IF:
    case CF_ELIF:
    case CF_ELSE:
    case CF_WHILE:
    case CF_FOR:
    case CF_FUNCTION:
    case CF_RETURN:
    case CF_BREAK:
    case CF_CONTINUE:
        return 1;
    default:
        return 0;
    }
}

/* Format a short display spelling of a token for diagnostic messages. */
static void token_display(const CfParserInput *in, const CfToken *tok,
                          char *buf, size_t cap)
{
    if (!tok || cap == 0) return;
    if (tok->type == CF_EOF) {
        snprintf(buf, cap, "end of file");
        return;
    }
    if (tok->type == CF_NEWLINE) {
        snprintf(buf, cap, "end of line");
        return;
    }
    if (tok->type == CF_INDENT) {
        snprintf(buf, cap, "indentation");
        return;
    }
    if (tok->type == CF_DEDENT) {
        snprintf(buf, cap, "dedent");
        return;
    }
    size_t len = 0;
    const char *lex = parser_input_lexeme(in, tok, &len);
    if (len == 0) {
        snprintf(buf, cap, "%s", cf_token_name(tok->type));
        return;
    }
    if (len > 32) len = 32;
    size_t pos = 0;
    if (pos + 1 < cap) buf[pos++] = '"';
    for (size_t i = 0; i < len && pos + 2 < cap; i++) {
        unsigned char c = (unsigned char)lex[i];
        if (c < 32 || c == 127) buf[pos++] = '?';
        else buf[pos++] = (char)c;
    }
    if (pos + 1 < cap) buf[pos++] = '"';
    buf[pos] = '\0';
}

/* Locate 1-based line number in raw source text. Returns 1 if found. */
static int find_source_line(const char *source, size_t length, size_t target_line,
                            size_t *begin_out, size_t *end_out)
{
    size_t line = 1, pos = 0;
    while (pos < length && line < target_line) {
        if (source[pos++] == '\n') line++;
    }
    if (line != target_line) return 0;
    size_t begin = pos;
    while (pos < length && source[pos] != '\n') pos++;
    *begin_out = begin;
    *end_out = pos;
    return 1;
}

/* Render the source line (from raw source or reconstructed from loaded tokens)
   and a caret pointing to the offending column. */
static void print_error_excerpt(FILE *out, const CfParserInput *in, const CfToken *tok)
{
    size_t line = tok ? tok->line : 1;
    size_t col = tok ? tok->column : 1;
    if (col == 0) col = 1;

    /* Raw source buffer from cf_lex(): render the exact line and caret. */
    if (in && in->source && in->result && in->source != in->result->text) {
        size_t begin = 0, end = 0;
        if (find_source_line(in->source, in->source_length, line, &begin, &end)) {
            fputs("  ", out);
            size_t caret = 0;
            for (size_t i = begin; i < end;) {
                unsigned char c = (unsigned char)in->source[i];
                size_t bytes = cf_utf8_span(in->source + i, end - i);
                size_t width = 1;
                if (!bytes || c < 32 || c == 127) {
                    fprintf(out, "\\x%02X", c);
                    bytes = 1;
                    width = 4;
                } else {
                    fwrite(in->source + i, 1, bytes, out);
                }
                if (i - begin < col - 1) caret += width;
                i += bytes;
            }
            fputs("\n  ", out);
            for (size_t i = 0; i < caret; i++) fputc(' ', out);
            fputs("^\n", out);
            return;
        }
    }

    /* Loaded token table: reconstruct the line from tokens sharing tok->line. */
    fputs("  ", out);
    size_t caret = 0;
    int placed_caret = 0;
    size_t rendered = 0;
    if (in && in->result && tok) {
        for (size_t i = 0; i < in->result->token_count; i++) {
            const CfToken *t = &in->result->tokens[i];
            if (t->line != line) continue;
            size_t len = 0;
            const char *lex = parser_input_lexeme(in, t, &len);
            int is_target = (t->type == tok->type && t->column == tok->column &&
                             t->offset == tok->offset);
            if (len == 0) {
                if (is_target && !placed_caret) {
                    caret = rendered;
                    placed_caret = 1;
                }
                continue;
            }
            if (rendered > 0) {
                fputc(' ', out);
                rendered++;
            }
            if (is_target && !placed_caret) {
                caret = rendered;
                placed_caret = 1;
            }
            fwrite(lex, 1, len, out);
            rendered += cf_scalar_count(lex, len);
        }
    }
    if (rendered == 0) {
        fputs("<end of input>", out);
        caret = 0;
    } else if (!placed_caret) {
        caret = rendered;
    }
    fputs("\n  ", out);
    for (size_t i = 0; i < caret; i++) fputc(' ', out);
    fputs("^\n", out);
}

static void syntax_error_at(CfParser *p, int code, const CfToken *tok,
                            const char *what, const char *why, const char *fix)
{
    if (!p || !p->result || p->result->errors >= CF_MAX_ERRORS) return;
    size_t line = tok ? tok->line : 1;
    size_t col = tok ? tok->column : 1;

    CfSyntaxError *err = &p->result->error_list[p->result->errors];
    err->line = line;
    err->column = col;
    err->code = code;
    snprintf(err->what, sizeof(err->what), "%s", what ? what : "Syntax error.");

    if (p->diagnostics) {
        fprintf(p->diagnostics, "Error E%d (Syntax Error) at line %zu, column %zu\n",
                code, line, col);
        print_error_excerpt(p->diagnostics, p->in, tok);
        fprintf(p->diagnostics, "What happened: %s\nWhy: %s\nHow to fix: %s\n\n",
                what, why, fix);
    }

    p->result->errors++;
    if (p->result->errors == CF_MAX_ERRORS && p->diagnostics) {
        fputs("Fix these first, then run again\n", p->diagnostics);
    }
}

/* Line- and statement-level recovery: advance past the damaged statement so
   subsequent statements can still be parsed without cascading diagnostics. */
static void recover_statement(CfParser *p, size_t stmt_line)
{
    int progressed = 0;
    while (!parser_input_at_end(p->in)) {
        const CfToken *tok = peek(p->in);
        if (tok->type == CF_PERIOD) {
            next_token(p->in);
            return;
        }
        if (tok->type == CF_NEWLINE) {
            next_token(p->in);
            return;
        }
        if (tok->type == CF_DEDENT || tok->type == CF_RBRACE) {
            return;
        }
        if (tok->line > stmt_line) {
            return;
        }
        if (progressed && is_stmt_boundary(tok)) {
            return;
        }
        next_token(p->in);
        progressed = 1;
    }
}

/* Check whether the current lookahead token is specifically the word "not"
   (and not the symbolic "!" operator), used for "is not". */
static int is_word_not(const CfParserInput *in)
{
    const CfToken *tok = peek(in);
    if (tok->type != CF_NOT) return 0;
    size_t len = 0;
    const char *lex = parser_input_lexeme(in, tok, &len);
    return len == 3 && memcmp(lex, "not", 3) == 0;
}

static int enter_depth(CfParser *p, const CfToken *tok)
{
    if (p->depth >= CF_MAX_PARSE_DEPTH) {
        syntax_error_at(p, 114, tok,
                        "Expression is nested too deeply.",
                        "ClariFix limits expression nesting to 256 levels.",
                        "Simplify the expression or split it across variables.");
        return 0;
    }
    p->depth++;
    return 1;
}

static void leave_depth(CfParser *p)
{
    if (p->depth > 0) p->depth--;
}

/* Forward declarations for mutually recursive expression rules. */
static uint32_t parse_or(CfParser *p);
static uint32_t parse_unary(CfParser *p);

/* primary = INT | DECIMAL | STRING | CHAR | BOOL
           | IDENT | "(" expr ")" | "[" [ args ] "]" ; */
static uint32_t parse_primary(CfParser *p)
{
    const CfToken *tok = peek(p->in);

    switch (tok->type) {
    case CF_INT:
    case CF_DECIMAL:
    case CF_STRING:
    case CF_CHAR:
    case CF_BOOL:
        tok = next_token(p->in);
        return alloc_node(p, CF_NODE_LITERAL, tok);

    case CF_IDENT:
        tok = next_token(p->in);
        return alloc_node(p, CF_NODE_IDENT, tok);

    case CF_LPAREN: {
        CfToken open = *next_token(p->in);
        if (peek(p->in)->type == CF_RPAREN) {
            syntax_error_at(p, 114, peek(p->in),
                            "Empty parentheses \"()\" are not a valid expression.",
                            "Parentheses used for grouping must contain an expression.",
                            "Place an expression inside the parentheses, or remove \"()\".");
            return CF_NODE_NONE;
        }
        uint32_t inner = parse_expr(p);
        if (inner == CF_NODE_NONE) return CF_NODE_NONE;
        if (!match(p->in, CF_RPAREN)) {
            syntax_error_at(p, 114, peek(p->in),
                            "Expected \")\" to close parenthesized expression.",
                            "Every opening \"(\" must have a matching \")\".",
                            "Add \")\" after the grouped expression.");
            return CF_NODE_NONE;
        }
        uint32_t node_id = alloc_node(p, CF_NODE_GROUP, &open);
        p->result->nodes[node_id - 1].left = inner;
        return node_id;
    }

    case CF_LBRACKET: {
        CfToken open = *next_token(p->in);
        uint32_t arr_id = alloc_node(p, CF_NODE_ARRAY, &open);
        if (match(p->in, CF_RBRACKET)) {
            return arr_id;
        }
        for (;;) {
            uint32_t elem = parse_expr(p);
            if (elem == CF_NODE_NONE) return CF_NODE_NONE;
            append_child(p, arr_id, elem);
            if (match(p->in, CF_COMMA)) {
                if (peek(p->in)->type == CF_RBRACKET) {
                    syntax_error_at(p, 114, peek(p->in),
                                    "Expected an element expression after \",\" in array literal.",
                                    "Each comma in an array literal must be followed by an element.",
                                    "Add an expression after \",\" or remove the trailing comma.");
                    return CF_NODE_NONE;
                }
                continue;
            }
            if (match(p->in, CF_RBRACKET)) {
                return arr_id;
            }
            char found[48];
            char msg[160];
            token_display(p->in, peek(p->in), found, sizeof(found));
            snprintf(msg, sizeof(msg),
                     "Expected \",\" or \"]\" in array literal, but found %s.", found);
            syntax_error_at(p, 114, peek(p->in), msg,
                            "Array elements must be separated by commas and closed with \"]\".",
                            "Add \",\" between elements or \"]\" to close the array.");
            return CF_NODE_NONE;
        }
    }

    case CF_RPAREN:
        syntax_error_at(p, 114, tok,
                        "Expected an expression before \")\".",
                        "An expression cannot end with a closing parenthesis here.",
                        "Provide the missing expression before \")\".");
        return CF_NODE_NONE;

    case CF_RBRACKET:
        syntax_error_at(p, 114, tok,
                        "Expected an expression before \"]\".",
                        "An expression cannot end with a closing bracket here.",
                        "Provide the missing expression before \"]\".");
        return CF_NODE_NONE;

    case CF_COMMA:
        syntax_error_at(p, 114, tok,
                        "Expected an expression before \",\".",
                        "A comma separates expressions and cannot appear without a preceding value.",
                        "Provide an expression before \",\" or remove the stray comma.");
        return CF_NODE_NONE;

    case CF_PERIOD:
    case CF_NEWLINE:
    case CF_EOF:
    case CF_DEDENT:
    case CF_RBRACE:
        syntax_error_at(p, 114, tok,
                        "Expected an expression before end of statement.",
                        "An operator or grouping construct is missing its operand expression.",
                        "Add the missing value, variable, or function call.");
        return CF_NODE_NONE;

    default: {
        char found[48];
        char msg[160];
        token_display(p->in, tok, found, sizeof(found));
        snprintf(msg, sizeof(msg),
                 "Unexpected token %s where an expression was expected.", found);
        syntax_error_at(p, 114, tok, msg,
                        "An expression must begin with a literal, variable name, unary operator, \"(\", or \"[\".",
                        "Replace or remove the unexpected token so a valid expression appears here.");
        return CF_NODE_NONE;
    }
    }
}

/* postfix = primary { "(" [ args ] ")" | "[" expr "]" | "++" | "--" } ;
   args    = expr { "," expr } ; */
static uint32_t parse_postfix(CfParser *p)
{
    uint32_t node_id = parse_primary(p);
    if (node_id == CF_NODE_NONE) return CF_NODE_NONE;

    for (;;) {
        const CfToken *tok = peek(p->in);
        if (tok->type == CF_LPAREN) {
            CfToken open = *next_token(p->in);
            uint32_t call_id = alloc_node(p, CF_NODE_CALL, &open);
            p->result->nodes[call_id - 1].left = node_id;

            if (!match(p->in, CF_RPAREN)) {
                for (;;) {
                    uint32_t arg = parse_expr(p);
                    if (arg == CF_NODE_NONE) return CF_NODE_NONE;
                    append_child(p, call_id, arg);

                    if (match(p->in, CF_COMMA)) {
                        if (peek(p->in)->type == CF_RPAREN) {
                            syntax_error_at(p, 114, peek(p->in),
                                            "Expected an argument expression after \",\" in function call.",
                                            "Each comma in a function call must be followed by another argument.",
                                            "Add an argument expression after \",\" or remove the trailing comma.");
                            return CF_NODE_NONE;
                        }
                        continue;
                    }
                    if (match(p->in, CF_RPAREN)) {
                        break;
                    }

                    const CfToken *bad = peek(p->in);
                    if (bad->type == CF_PERIOD || is_stmt_boundary(bad)) {
                        syntax_error_at(p, 114, bad,
                                        "Expected \")\" to close function-call argument list.",
                                        "Every opening \"(\" in a function call must have a matching \")\".",
                                        "Add \")\" after the last argument.");
                    } else {
                        char found[48];
                        char msg[160];
                        token_display(p->in, bad, found, sizeof(found));
                        snprintf(msg, sizeof(msg),
                                 "Expected \",\" or \")\" after function-call argument, but found %s.",
                                 found);
                        syntax_error_at(p, 114, bad, msg,
                                        "Function-call arguments must be separated by commas and closed with \")\".",
                                        "Add \",\" between arguments or \")\" to close the call.");
                    }
                    return CF_NODE_NONE;
                }
            }
            node_id = call_id;
        } else if (tok->type == CF_LBRACKET) {
            CfToken open = *next_token(p->in);
            if (peek(p->in)->type == CF_RBRACKET) {
                syntax_error_at(p, 114, peek(p->in),
                                "Missing index expression inside \"[]\".",
                                "Array indexing requires an index expression between \"[\" and \"]\".",
                                "Provide an index expression, for example: items[0].");
                return CF_NODE_NONE;
            }
            uint32_t idx = parse_expr(p);
            if (idx == CF_NODE_NONE) return CF_NODE_NONE;
            if (!match(p->in, CF_RBRACKET)) {
                syntax_error_at(p, 114, peek(p->in),
                                "Expected \"]\" to close index expression.",
                                "Every opening \"[\" in an index expression must have a matching \"]\".",
                                "Add \"]\" after the index expression.");
                return CF_NODE_NONE;
            }
            uint32_t index_id = alloc_node(p, CF_NODE_INDEX, &open);
            p->result->nodes[index_id - 1].left = node_id;
            p->result->nodes[index_id - 1].right = idx;
            node_id = index_id;
        } else if (tok->type == CF_INCREMENT || tok->type == CF_DECREMENT) {
            CfToken op = *next_token(p->in);
            uint32_t post_id = alloc_node(p, CF_NODE_POSTFIX, &op);
            p->result->nodes[post_id - 1].op = op.type;
            p->result->nodes[post_id - 1].left = node_id;
            node_id = post_id;
        } else {
            break;
        }
    }
    return node_id;
}

/* power = postfix [ "^" unary ] ; (right-associative) */
static uint32_t parse_power(CfParser *p)
{
    uint32_t lhs = parse_postfix(p);
    if (lhs == CF_NODE_NONE) return CF_NODE_NONE;

    if (peek(p->in)->type == CF_POWER) {
        CfToken op = *next_token(p->in);
        uint32_t rhs = parse_unary(p);
        if (rhs == CF_NODE_NONE) return CF_NODE_NONE;
        uint32_t bin = alloc_node(p, CF_NODE_BINARY, &op);
        p->result->nodes[bin - 1].op = op.type;
        p->result->nodes[bin - 1].left = lhs;
        p->result->nodes[bin - 1].right = rhs;
        return bin;
    }
    return lhs;
}

/* unary = "-" unary | power ; */
static uint32_t parse_unary(CfParser *p)
{
    if (peek(p->in)->type == CF_MINUS) {
        CfToken op = *next_token(p->in);
        if (!enter_depth(p, &op)) return CF_NODE_NONE;
        uint32_t operand = parse_unary(p);
        leave_depth(p);
        if (operand == CF_NODE_NONE) return CF_NODE_NONE;
        uint32_t node_id = alloc_node(p, CF_NODE_UNARY, &op);
        p->result->nodes[node_id - 1].op = op.type;
        p->result->nodes[node_id - 1].left = operand;
        return node_id;
    }
    return parse_power(p);
}

/* term = unary { ( "*" | "/" | "//" | "%" ) unary } ; */
static uint32_t parse_term(CfParser *p)
{
    uint32_t lhs = parse_unary(p);
    if (lhs == CF_NODE_NONE) return CF_NODE_NONE;

    while (peek(p->in)->type == CF_STAR || peek(p->in)->type == CF_SLASH ||
           peek(p->in)->type == CF_FLOOR_DIV || peek(p->in)->type == CF_MOD) {
        CfToken op = *next_token(p->in);
        uint32_t rhs = parse_unary(p);
        if (rhs == CF_NODE_NONE) return CF_NODE_NONE;
        uint32_t bin = alloc_node(p, CF_NODE_BINARY, &op);
        p->result->nodes[bin - 1].op = op.type;
        p->result->nodes[bin - 1].left = lhs;
        p->result->nodes[bin - 1].right = rhs;
        lhs = bin;
    }
    return lhs;
}

/* sum = term { ( "+" | "-" ) term } ;
   "+" handles both arithmetic addition and string concatenation. */
static uint32_t parse_sum(CfParser *p)
{
    uint32_t lhs = parse_term(p);
    if (lhs == CF_NODE_NONE) return CF_NODE_NONE;

    while (peek(p->in)->type == CF_PLUS || peek(p->in)->type == CF_MINUS) {
        CfToken op = *next_token(p->in);
        uint32_t rhs = parse_term(p);
        if (rhs == CF_NODE_NONE) return CF_NODE_NONE;
        uint32_t bin = alloc_node(p, CF_NODE_BINARY, &op);
        p->result->nodes[bin - 1].op = op.type;
        p->result->nodes[bin - 1].left = lhs;
        p->result->nodes[bin - 1].right = rhs;
        lhs = bin;
    }
    return lhs;
}

/* relation = sum { ( "<" | ">" | "<=" | ">=" ) sum } ; */
static uint32_t parse_relation(CfParser *p)
{
    uint32_t lhs = parse_sum(p);
    if (lhs == CF_NODE_NONE) return CF_NODE_NONE;

    while (peek(p->in)->type == CF_LT || peek(p->in)->type == CF_GT ||
           peek(p->in)->type == CF_LE || peek(p->in)->type == CF_GE) {
        CfToken op = *next_token(p->in);
        uint32_t rhs = parse_sum(p);
        if (rhs == CF_NODE_NONE) return CF_NODE_NONE;
        uint32_t bin = alloc_node(p, CF_NODE_BINARY, &op);
        p->result->nodes[bin - 1].op = op.type;
        p->result->nodes[bin - 1].left = lhs;
        p->result->nodes[bin - 1].right = rhs;
        lhs = bin;
    }
    return lhs;
}

/* equality = relation { ( "==" | "!=" | "is" [ "not" ] ) relation } ; */
static uint32_t parse_equality(CfParser *p)
{
    uint32_t lhs = parse_relation(p);
    if (lhs == CF_NODE_NONE) return CF_NODE_NONE;

    while (peek(p->in)->type == CF_EQ || peek(p->in)->type == CF_NE ||
           peek(p->in)->type == CF_IS) {
        CfToken op = *next_token(p->in);
        int negated = 0;
        if (op.type == CF_IS && is_word_not(p->in)) {
            next_token(p->in);
            negated = 1;
        }
        uint32_t rhs = parse_relation(p);
        if (rhs == CF_NODE_NONE) return CF_NODE_NONE;
        uint32_t bin = alloc_node(p, CF_NODE_BINARY, &op);
        p->result->nodes[bin - 1].op = op.type;
        p->result->nodes[bin - 1].op_negated = negated;
        p->result->nodes[bin - 1].left = lhs;
        p->result->nodes[bin - 1].right = rhs;
        lhs = bin;
    }
    return lhs;
}

/* not_expr = ( "not" | "!" ) not_expr | equality ; */
static uint32_t parse_not(CfParser *p)
{
    if (peek(p->in)->type == CF_NOT) {
        CfToken op = *next_token(p->in);
        if (!enter_depth(p, &op)) return CF_NODE_NONE;
        uint32_t operand = parse_not(p);
        leave_depth(p);
        if (operand == CF_NODE_NONE) return CF_NODE_NONE;
        uint32_t node_id = alloc_node(p, CF_NODE_UNARY, &op);
        p->result->nodes[node_id - 1].op = op.type;
        p->result->nodes[node_id - 1].left = operand;
        return node_id;
    }
    return parse_equality(p);
}

/* and_expr = not_expr { ( "and" | "&&" ) not_expr } ; */
static uint32_t parse_and(CfParser *p)
{
    uint32_t lhs = parse_not(p);
    if (lhs == CF_NODE_NONE) return CF_NODE_NONE;

    while (peek(p->in)->type == CF_AND) {
        CfToken op = *next_token(p->in);
        uint32_t rhs = parse_not(p);
        if (rhs == CF_NODE_NONE) return CF_NODE_NONE;
        uint32_t bin = alloc_node(p, CF_NODE_BINARY, &op);
        p->result->nodes[bin - 1].op = op.type;
        p->result->nodes[bin - 1].left = lhs;
        p->result->nodes[bin - 1].right = rhs;
        lhs = bin;
    }
    return lhs;
}

/* or_expr = and_expr { ( "or" | "||" ) and_expr } ; */
static uint32_t parse_or(CfParser *p)
{
    uint32_t lhs = parse_and(p);
    if (lhs == CF_NODE_NONE) return CF_NODE_NONE;

    while (peek(p->in)->type == CF_OR) {
        CfToken op = *next_token(p->in);
        uint32_t rhs = parse_and(p);
        if (rhs == CF_NODE_NONE) return CF_NODE_NONE;
        uint32_t bin = alloc_node(p, CF_NODE_BINARY, &op);
        p->result->nodes[bin - 1].op = op.type;
        p->result->nodes[bin - 1].left = lhs;
        p->result->nodes[bin - 1].right = rhs;
        lhs = bin;
    }
    return lhs;
}

uint32_t parse_expr(CfParser *p)
{
    if (!p || !p->in || !p->result) return CF_NODE_NONE;
    if (!enter_depth(p, peek(p->in))) return CF_NODE_NONE;
    uint32_t id = parse_or(p);
    leave_depth(p);
    return id;
}

/* show_stmt = "show" expr "." ; */
uint32_t parse_show_stmt(CfParser *p)
{
    if (!p || !p->in || !p->result) return CF_NODE_NONE;

    if (peek(p->in)->type != CF_SHOW) {
        syntax_error_at(p, 114, peek(p->in),
                        "Expected \"show\" to start an output statement.",
                        "Output statements in ClariFix begin with the keyword \"show\".",
                        "Start the statement with \"show\", for example: show \"Hello\".");
        return CF_NODE_NONE;
    }

    CfToken show_tok = *next_token(p->in);
    const CfToken *after_show = peek(p->in);

    if (after_show->type == CF_PERIOD || is_stmt_boundary(after_show)) {
        syntax_error_at(p, 114, after_show,
                        "Expected an expression after \"show\".",
                        "The show statement requires a value or expression to display.",
                        "Provide an expression after \"show\", for example: show \"Hello\".");
        if (after_show->type == CF_PERIOD || after_show->type == CF_NEWLINE) {
            next_token(p->in);
        }
        return CF_NODE_NONE;
    }

    uint32_t expr_id = parse_expr(p);
    if (expr_id == CF_NODE_NONE) {
        recover_statement(p, show_tok.line);
        return CF_NODE_NONE;
    }

    if (match(p->in, CF_PERIOD)) {
        uint32_t stmt_id = alloc_node(p, CF_NODE_SHOW_STMT, &show_tok);
        p->result->nodes[stmt_id - 1].left = expr_id;
        return stmt_id;
    }

    const CfToken *next = peek(p->in);
    if (is_stmt_boundary(next)) {
        syntax_error_at(p, 113, next,
                        "A period \".\" is missing at the end of the show statement.",
                        "Every simple statement in ClariFix must end with a period (\".\").",
                        "Add \".\" at the end of the show statement.");
        if (next->type == CF_NEWLINE) next_token(p->in);
        return CF_NODE_NONE;
    }

    if (next->type == CF_COMMA) {
        syntax_error_at(p, 114, next,
                        "Unexpected \",\" in show statement.",
                        "show displays a single expression and does not take comma-separated values.",
                        "Join values with \"+\" (using to_text() for non-strings), or use separate show statements.");
        recover_statement(p, show_tok.line);
        return CF_NODE_NONE;
    }

    if (next->type == CF_ASSIGN) {
        syntax_error_at(p, 114, next,
                        "Unexpected \"=\" in show statement.",
                        "show displays an expression and cannot perform variable assignment.",
                        "Use \"==\" to compare values, or use a \"set\" statement to assign a variable.");
        recover_statement(p, show_tok.line);
        return CF_NODE_NONE;
    }

    char found[48];
    char msg[160];
    token_display(p->in, next, found, sizeof(found));
    snprintf(msg, sizeof(msg),
             "Unexpected token %s after expression in show statement; expected \".\".",
             found);
    syntax_error_at(p, 114, next, msg,
                    "A show statement displays a single expression followed by a period (\".\").",
                    "Use \"+\" to combine values, or end the statement with \".\".");
    recover_statement(p, show_tok.line);
    return CF_NODE_NONE;
}

uint32_t parse_statement(CfParser *p)
{
    if (!p || !p->in || !p->result) return CF_NODE_NONE;

    const CfToken *tok = peek(p->in);
    switch (tok->type) {
    case CF_SHOW:
        return parse_show_stmt(p);

    default: {
        char found[48];
        char msg[160];
        token_display(p->in, tok, found, sizeof(found));
        snprintf(msg, sizeof(msg),
                 "Unexpected token %s at the start of a statement.", found);
        syntax_error_at(p, 114, tok, msg,
                        "This statement could not be matched to a supported ClariFix syntax rule.",
                        "Check the statement spelling and syntax (for example: show <expr>.).");
        recover_statement(p, tok->line);
        return CF_NODE_NONE;
    }
    }
}

uint32_t parse_program(CfParser *p)
{
    if (!p || !p->in || !p->result) return CF_NODE_NONE;
    const CfToken *start = peek(p->in);
    uint32_t prog = alloc_node(p, CF_NODE_PROGRAM, start);
    p->result->root = prog;

    while (!parser_input_at_end(p->in) && p->result->errors < CF_MAX_ERRORS) {
        if (match(p->in, CF_NEWLINE)) continue;
        size_t before = p->in->index;
        uint32_t stmt = parse_statement(p);
        if (stmt != CF_NODE_NONE) {
            append_child(p, prog, stmt);
        } else if (p->in->index == before && !parser_input_at_end(p->in)) {
            next_token(p->in);
        }
    }
    return prog;
}

int cf_parse(CfParserInput *in, FILE *diagnostics, CfParseResult *result)
{
    CfParser parser;
    cf_parser_init(&parser, in, diagnostics, result);
    parse_program(&parser);
    return result && result->errors == 0 ? 0 : 1;
}

static void write_indent(FILE *out, size_t level)
{
    for (size_t i = 0; i < level; i++) fputs("  ", out);
}

static void write_lexeme_escaped(FILE *out, const char *s, size_t n)
{
    for (size_t i = 0; i < n;) {
        unsigned char c = (unsigned char)s[i];
        size_t bytes = cf_utf8_span(s + i, n - i);
        if (c == '\n') fputs("\\n", out);
        else if (c == '\t') fputs("\\t", out);
        else if (c == '\r') fputs("\\r", out);
        else if (c < 32 || c == 127 || !bytes) fprintf(out, "\\x%02X", c);
        else { fwrite(s + i, 1, bytes, out); i += bytes; continue; }
        i++;
    }
}

static void write_ast_node(FILE *out, const CfParserInput *in,
                           const CfParseResult *result, uint32_t id, size_t level)
{
    const CfAstNode *node = cf_ast_node(result, id);
    if (!node) return;

    write_indent(out, level);
    switch (node->kind) {
    case CF_NODE_PROGRAM:
        fprintf(out, "Program (%u %s)\n", node->child_count,
                node->child_count == 1 ? "statement" : "statements");
        for (uint32_t c = node->first_child; c != CF_NODE_NONE;
             c = cf_ast_node(result, c)->next_sibling) {
            write_ast_node(out, in, result, c, level + 1);
        }
        break;

    case CF_NODE_SHOW_STMT:
        fprintf(out, "ShowStmt (line %zu, col %zu)\n",
                node->token.line, node->token.column);
        write_ast_node(out, in, result, node->left, level + 1);
        break;

    case CF_NODE_LITERAL: {
        size_t len = 0;
        const char *lex = parser_input_lexeme(in, &node->token, &len);
        fprintf(out, "Literal %s ", cf_token_name(node->token.type));
        write_lexeme_escaped(out, lex, len);
        fprintf(out, " (line %zu, col %zu)\n",
                node->token.line, node->token.column);
        break;
    }

    case CF_NODE_IDENT: {
        size_t len = 0;
        const char *lex = parser_input_lexeme(in, &node->token, &len);
        fputs("Ident ", out);
        write_lexeme_escaped(out, lex, len);
        fprintf(out, " (line %zu, col %zu)\n",
                node->token.line, node->token.column);
        break;
    }

    case CF_NODE_GROUP:
        fprintf(out, "Group (line %zu, col %zu)\n",
                node->token.line, node->token.column);
        write_ast_node(out, in, result, node->left, level + 1);
        break;

    case CF_NODE_ARRAY:
        fprintf(out, "Array (%u %s, line %zu, col %zu)\n",
                node->child_count, node->child_count == 1 ? "element" : "elements",
                node->token.line, node->token.column);
        for (uint32_t c = node->first_child; c != CF_NODE_NONE;
             c = cf_ast_node(result, c)->next_sibling) {
            write_ast_node(out, in, result, c, level + 1);
        }
        break;

    case CF_NODE_UNARY:
        fprintf(out, "Unary %s (line %zu, col %zu)\n",
                cf_ast_op_name(node->op, node->op_negated),
                node->token.line, node->token.column);
        write_ast_node(out, in, result, node->left, level + 1);
        break;

    case CF_NODE_POSTFIX:
        fprintf(out, "Postfix %s (line %zu, col %zu)\n",
                cf_ast_op_name(node->op, node->op_negated),
                node->token.line, node->token.column);
        write_ast_node(out, in, result, node->left, level + 1);
        break;

    case CF_NODE_BINARY:
        fprintf(out, "Binary %s (line %zu, col %zu)\n",
                cf_ast_op_name(node->op, node->op_negated),
                node->token.line, node->token.column);
        write_ast_node(out, in, result, node->left, level + 1);
        write_ast_node(out, in, result, node->right, level + 1);
        break;

    case CF_NODE_CALL: {
        fprintf(out, "Call (%u %s, line %zu, col %zu)\n",
                node->child_count, node->child_count == 1 ? "arg" : "args",
                node->token.line, node->token.column);
        write_indent(out, level + 1);
        fputs("callee:\n", out);
        write_ast_node(out, in, result, node->left, level + 2);
        uint32_t arg_index = 0;
        for (uint32_t c = node->first_child; c != CF_NODE_NONE;
             c = cf_ast_node(result, c)->next_sibling) {
            write_indent(out, level + 1);
            fprintf(out, "arg[%u]:\n", arg_index++);
            write_ast_node(out, in, result, c, level + 2);
        }
        break;
    }

    case CF_NODE_INDEX:
        fprintf(out, "Index (line %zu, col %zu)\n",
                node->token.line, node->token.column);
        write_indent(out, level + 1);
        fputs("target:\n", out);
        write_ast_node(out, in, result, node->left, level + 2);
        write_indent(out, level + 1);
        fputs("index:\n", out);
        write_ast_node(out, in, result, node->right, level + 2);
        break;
    }
}

int cf_write_ast(FILE *out, const CfParserInput *in, const CfParseResult *result)
{
    if (!out || !result) return 2;
    if (result->root == CF_NODE_NONE) {
        fputs("(empty AST)\n", out);
        return ferror(out) ? 2 : 0;
    }
    write_ast_node(out, in, result, result->root, 0);
    return ferror(out) ? 2 : 0;
}
