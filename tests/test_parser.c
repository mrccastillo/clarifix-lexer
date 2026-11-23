#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/parser_input.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int tests_run = 0;
static int tests_failed = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        tests_failed++; \
        return; \
    } \
} while (0)

#define RUN_TEST(fn) do { \
    int before = tests_failed; \
    tests_run++; \
    fn(); \
    if (tests_failed == before) { \
        printf("PASS %s\n", #fn); \
    } else { \
        printf("FAIL %s\n", #fn); \
    } \
} while (0)

static CfResult g_lex;
static CfParseResult g_parse;
static CfParserInput g_in;

static int lex_and_parse(const char *src, int braces, char *diag_buf, size_t diag_cap)
{
    FILE *diag = tmpfile();
    if (!diag) { perror("tmpfile"); exit(2); }

    int lex_status = cf_lex(src, strlen(src), braces, diag, NULL, &g_lex);
    if (lex_status != 0) {
        if (diag_buf && diag_cap > 0) {
            rewind(diag);
            size_t n = fread(diag_buf, 1, diag_cap - 1, diag);
            diag_buf[n] = '\0';
        }
        fclose(diag);
        return -1;
    }

    parser_input_init(&g_in, &g_lex, src, strlen(src));
    int parse_status = cf_parse(&g_in, diag, &g_parse);

    if (diag_buf && diag_cap > 0) {
        rewind(diag);
        size_t n = fread(diag_buf, 1, diag_cap - 1, diag);
        diag_buf[n] = '\0';
    }
    fclose(diag);
    return parse_status;
}

static int lexeme_eq(const CfParserInput *in, const CfToken *tok, const char *expected)
{
    size_t len = 0;
    const char *lex = parser_input_lexeme(in, tok, &len);
    return strlen(expected) == len && memcmp(lex, expected, len) == 0;
}

static void test_basic_show_literals_ident_and_noise(void)
{
    const char *src =
        "show 42.\n"
        "show 19.99.\n"
        "show \"Hello\".\n"
        "show 'A'.\n"
        "show true.\n"
        "show false.\n"
        "show total.\n"
        "show the total.\n"
        "show of name.\n";
    char diag[4096];
    CHECK(lex_and_parse(src, 0, diag, sizeof(diag)) == 0);
    CHECK(g_parse.errors == 0);
    CHECK(diag[0] == '\0');

    const CfAstNode *prog = cf_ast_node(&g_parse, g_parse.root);
    CHECK(prog != NULL && prog->kind == CF_NODE_PROGRAM);
    CHECK(prog->child_count == 9);

    uint32_t cur = prog->first_child;
    const CfAstNode *s1 = cf_ast_node(&g_parse, cur);
    CHECK(s1->kind == CF_NODE_SHOW_STMT);
    const CfAstNode *e1 = cf_ast_node(&g_parse, s1->left);
    CHECK(e1->kind == CF_NODE_LITERAL && e1->token.type == CF_INT);
    CHECK(lexeme_eq(&g_in, &e1->token, "42"));

    cur = s1->next_sibling;
    const CfAstNode *s2 = cf_ast_node(&g_parse, cur);
    const CfAstNode *e2 = cf_ast_node(&g_parse, s2->left);
    CHECK(e2->kind == CF_NODE_LITERAL && e2->token.type == CF_DECIMAL);

    cur = s2->next_sibling;
    const CfAstNode *s3 = cf_ast_node(&g_parse, cur);
    const CfAstNode *e3 = cf_ast_node(&g_parse, s3->left);
    CHECK(e3->kind == CF_NODE_LITERAL && e3->token.type == CF_STRING);
    CHECK(lexeme_eq(&g_in, &e3->token, "\"Hello\""));

    cur = s3->next_sibling;
    const CfAstNode *s4 = cf_ast_node(&g_parse, cur);
    CHECK(cf_ast_node(&g_parse, s4->left)->token.type == CF_CHAR);

    cur = s4->next_sibling;
    const CfAstNode *s5 = cf_ast_node(&g_parse, cur);
    CHECK(cf_ast_node(&g_parse, s5->left)->token.type == CF_BOOL);

    cur = s5->next_sibling;
    const CfAstNode *s6 = cf_ast_node(&g_parse, cur);
    CHECK(cf_ast_node(&g_parse, s6->left)->token.type == CF_BOOL);

    cur = s6->next_sibling;
    const CfAstNode *s7 = cf_ast_node(&g_parse, cur);
    CHECK(cf_ast_node(&g_parse, s7->left)->kind == CF_NODE_IDENT);
    CHECK(lexeme_eq(&g_in, &cf_ast_node(&g_parse, s7->left)->token, "total"));

    cur = s7->next_sibling;
    const CfAstNode *s8 = cf_ast_node(&g_parse, cur);
    CHECK(cf_ast_node(&g_parse, s8->left)->kind == CF_NODE_IDENT);
    CHECK(lexeme_eq(&g_in, &cf_ast_node(&g_parse, s8->left)->token, "total"));

    cur = s8->next_sibling;
    const CfAstNode *s9 = cf_ast_node(&g_parse, cur);
    CHECK(cf_ast_node(&g_parse, s9->left)->kind == CF_NODE_IDENT);
    CHECK(lexeme_eq(&g_in, &cf_ast_node(&g_parse, s9->left)->token, "name"));
    CHECK(s9->next_sibling == CF_NODE_NONE);
}

static void test_string_concatenation(void)
{
    const char *src = "show \"A: \" + a + \", B: \" + b.\n";
    char diag[4096];
    CHECK(lex_and_parse(src, 0, diag, sizeof(diag)) == 0);

    const CfAstNode *prog = cf_ast_node(&g_parse, g_parse.root);
    CHECK(prog->child_count == 1);
    const CfAstNode *stmt = cf_ast_node(&g_parse, prog->first_child);
    CHECK(stmt->kind == CF_NODE_SHOW_STMT);

    /* Left-associative: ((("A: " + a) + ", B: ") + b) */
    const CfAstNode *top = cf_ast_node(&g_parse, stmt->left);
    CHECK(top->kind == CF_NODE_BINARY && top->op == CF_PLUS);
    CHECK(lexeme_eq(&g_in, &cf_ast_node(&g_parse, top->right)->token, "b"));

    const CfAstNode *mid = cf_ast_node(&g_parse, top->left);
    CHECK(mid->kind == CF_NODE_BINARY && mid->op == CF_PLUS);
    CHECK(lexeme_eq(&g_in, &cf_ast_node(&g_parse, mid->right)->token, "\", B: \""));

    const CfAstNode *bot = cf_ast_node(&g_parse, mid->left);
    CHECK(bot->kind == CF_NODE_BINARY && bot->op == CF_PLUS);
    CHECK(lexeme_eq(&g_in, &cf_ast_node(&g_parse, bot->left)->token, "\"A: \""));
    CHECK(lexeme_eq(&g_in, &cf_ast_node(&g_parse, bot->right)->token, "a"));
}

static void test_show_concat_with_function_calls(void)
{
    const char *src =
        "show \"Average: \" + to_text(avg).\n"
        "show \"Average: \" + to_text(average(total, count)).\n"
        "show greet().\n"
        "show format(\"Hello, \" + name, \"Score: \" + to_text(score + 1), [1, 2]).\n";
    char diag[4096];
    CHECK(lex_and_parse(src, 0, diag, sizeof(diag)) == 0);

    const CfAstNode *prog = cf_ast_node(&g_parse, g_parse.root);
    CHECK(prog->child_count == 4);

    /* 1: show "Average: " + to_text(avg). */
    const CfAstNode *s1 = cf_ast_node(&g_parse, prog->first_child);
    const CfAstNode *b1 = cf_ast_node(&g_parse, s1->left);
    CHECK(b1->kind == CF_NODE_BINARY && b1->op == CF_PLUS);
    CHECK(lexeme_eq(&g_in, &cf_ast_node(&g_parse, b1->left)->token, "\"Average: \""));
    const CfAstNode *c1 = cf_ast_node(&g_parse, b1->right);
    CHECK(c1->kind == CF_NODE_CALL && c1->child_count == 1);
    CHECK(lexeme_eq(&g_in, &cf_ast_node(&g_parse, c1->left)->token, "to_text"));
    CHECK(lexeme_eq(&g_in, &cf_ast_node(&g_parse, c1->first_child)->token, "avg"));

    /* 2: show "Average: " + to_text(average(total, count)). */
    const CfAstNode *s2 = cf_ast_node(&g_parse, s1->next_sibling);
    const CfAstNode *b2 = cf_ast_node(&g_parse, s2->left);
    const CfAstNode *outer = cf_ast_node(&g_parse, b2->right);
    CHECK(outer->kind == CF_NODE_CALL && outer->child_count == 1);
    const CfAstNode *inner = cf_ast_node(&g_parse, outer->first_child);
    CHECK(inner->kind == CF_NODE_CALL && inner->child_count == 2);
    CHECK(lexeme_eq(&g_in, &cf_ast_node(&g_parse, inner->left)->token, "average"));
    const CfAstNode *arg0 = cf_ast_node(&g_parse, inner->first_child);
    const CfAstNode *arg1 = cf_ast_node(&g_parse, arg0->next_sibling);
    CHECK(lexeme_eq(&g_in, &arg0->token, "total"));
    CHECK(lexeme_eq(&g_in, &arg1->token, "count"));

    /* 3: show greet(). */
    const CfAstNode *s3 = cf_ast_node(&g_parse, s2->next_sibling);
    const CfAstNode *c3 = cf_ast_node(&g_parse, s3->left);
    CHECK(c3->kind == CF_NODE_CALL && c3->child_count == 0);
    CHECK(c3->first_child == CF_NODE_NONE);

    /* 4: show format("Hello, " + name, "Score: " + to_text(score + 1), [1, 2]). */
    const CfAstNode *s4 = cf_ast_node(&g_parse, s3->next_sibling);
    const CfAstNode *c4 = cf_ast_node(&g_parse, s4->left);
    CHECK(c4->kind == CF_NODE_CALL && c4->child_count == 3);
    const CfAstNode *f0 = cf_ast_node(&g_parse, c4->first_child);
    const CfAstNode *f1 = cf_ast_node(&g_parse, f0->next_sibling);
    const CfAstNode *f2 = cf_ast_node(&g_parse, f1->next_sibling);
    CHECK(f0->kind == CF_NODE_BINARY && f0->op == CF_PLUS);
    CHECK(f1->kind == CF_NODE_BINARY && f1->op == CF_PLUS);
    CHECK(f2->kind == CF_NODE_ARRAY && f2->child_count == 2);
}

static void test_postfix_chaining_and_multiline_grouping(void)
{
    const char *src =
        "show get_fn()(1, 2).\n"
        "show funcs[0](x, y).\n"
        "show get_list(n)[i + 1].\n"
        "show (\n"
        "    \"Hello, \" +\n"
        "    to_text(name)\n"
        ").\n"
        "show combine(\n"
        "    \"A\" + \"B\",\n"
        "    to_text(count)\n"
        ").\n";
    char diag[4096];
    CHECK(lex_and_parse(src, 0, diag, sizeof(diag)) == 0);
    const CfAstNode *prog = cf_ast_node(&g_parse, g_parse.root);
    CHECK(prog->child_count == 5);

    const CfAstNode *s1 = cf_ast_node(&g_parse, prog->first_child);
    const CfAstNode *outer = cf_ast_node(&g_parse, s1->left);
    CHECK(outer->kind == CF_NODE_CALL && outer->child_count == 2);
    const CfAstNode *callee = cf_ast_node(&g_parse, outer->left);
    CHECK(callee->kind == CF_NODE_CALL && callee->child_count == 0);

    const CfAstNode *s2 = cf_ast_node(&g_parse, s1->next_sibling);
    const CfAstNode *c2 = cf_ast_node(&g_parse, s2->left);
    CHECK(c2->kind == CF_NODE_CALL && c2->child_count == 2);
    CHECK(cf_ast_node(&g_parse, c2->left)->kind == CF_NODE_INDEX);

    const CfAstNode *s3 = cf_ast_node(&g_parse, s2->next_sibling);
    const CfAstNode *idx3 = cf_ast_node(&g_parse, s3->left);
    CHECK(idx3->kind == CF_NODE_INDEX);
    CHECK(cf_ast_node(&g_parse, idx3->left)->kind == CF_NODE_CALL);
    CHECK(cf_ast_node(&g_parse, idx3->right)->kind == CF_NODE_BINARY);
}

static void test_precedence_and_associativity(void)
{
    char diag[4096];

    /* 1 + 2 * 3 -> 1 + (2 * 3) */
    CHECK(lex_and_parse("show 1 + 2 * 3.\n", 0, diag, sizeof(diag)) == 0);
    const CfAstNode *e = cf_ast_node(&g_parse, cf_ast_node(&g_parse, cf_ast_node(&g_parse, g_parse.root)->first_child)->left);
    CHECK(e->kind == CF_NODE_BINARY && e->op == CF_PLUS);
    CHECK(cf_ast_node(&g_parse, e->right)->kind == CF_NODE_BINARY && cf_ast_node(&g_parse, e->right)->op == CF_STAR);

    /* 2 ^ 3 ^ 2 -> 2 ^ (3 ^ 2) */
    CHECK(lex_and_parse("show 2 ^ 3 ^ 2.\n", 0, diag, sizeof(diag)) == 0);
    e = cf_ast_node(&g_parse, cf_ast_node(&g_parse, cf_ast_node(&g_parse, g_parse.root)->first_child)->left);
    CHECK(e->kind == CF_NODE_BINARY && e->op == CF_POWER);
    CHECK(cf_ast_node(&g_parse, e->right)->kind == CF_NODE_BINARY && cf_ast_node(&g_parse, e->right)->op == CF_POWER);

    /* 2 ^ -3 -> 2 ^ (-3) */
    CHECK(lex_and_parse("show 2 ^ -3.\n", 0, diag, sizeof(diag)) == 0);
    e = cf_ast_node(&g_parse, cf_ast_node(&g_parse, cf_ast_node(&g_parse, g_parse.root)->first_child)->left);
    CHECK(e->kind == CF_NODE_BINARY && e->op == CF_POWER);
    CHECK(cf_ast_node(&g_parse, e->right)->kind == CF_NODE_UNARY && cf_ast_node(&g_parse, e->right)->op == CF_MINUS);

    /* -2 ^ 2 -> -(2 ^ 2) */
    CHECK(lex_and_parse("show -2 ^ 2.\n", 0, diag, sizeof(diag)) == 0);
    e = cf_ast_node(&g_parse, cf_ast_node(&g_parse, cf_ast_node(&g_parse, g_parse.root)->first_child)->left);
    CHECK(e->kind == CF_NODE_UNARY && e->op == CF_MINUS);
    CHECK(cf_ast_node(&g_parse, e->left)->kind == CF_NODE_BINARY && cf_ast_node(&g_parse, e->left)->op == CF_POWER);

    /* age is not 10 or false */
    CHECK(lex_and_parse("show age is not 10 or false.\n", 0, diag, sizeof(diag)) == 0);
    e = cf_ast_node(&g_parse, cf_ast_node(&g_parse, cf_ast_node(&g_parse, g_parse.root)->first_child)->left);
    CHECK(e->kind == CF_NODE_BINARY && e->op == CF_OR);
    const CfAstNode *is_not = cf_ast_node(&g_parse, e->left);
    CHECK(is_not->kind == CF_NODE_BINARY && is_not->op == CF_IS && is_not->op_negated == 1);

    /* Complex features.clx expressions */
    CHECK(lex_and_parse(
        "show (2 + 3) * 4 ^ 2 // 2 % 7.\n"
        "show true and !false || age != 0.\n"
        "show not not flag.\n"
        "show [].\n"
        "show age++.\n"
        "show age--.\n", 0, diag, sizeof(diag)) == 0);
    CHECK(cf_ast_node(&g_parse, g_parse.root)->child_count == 6);
}

static void test_same_line_and_braces_mode(void)
{
    char diag[4096];
    CHECK(lex_and_parse("show 1. show \"a\" + \"b\". show f(1, 2).", 0, diag, sizeof(diag)) == 0);
    CHECK(cf_ast_node(&g_parse, g_parse.root)->child_count == 3);

    CHECK(lex_and_parse("show 1.\nshow \"a\" + \"b\".\nshow f(1, 2).", 1, diag, sizeof(diag)) == 0);
    CHECK(cf_ast_node(&g_parse, g_parse.root)->child_count == 3);
}

static void test_loaded_from_all_four_lexer_table_formats(void)
{
    const char *src =
        "# comment\n"
        "show \"Average: \" + to_text(average(total, count)).\n"
        "show the name.\n";

    static CfResult lex_res;
    static CfResult loaded_res;
    static CfParseResult parse_res;
    CfParserInput loaded_in;

    CHECK(cf_lex(src, strlen(src), 0, stderr, NULL, &lex_res) == 0);

    for (int fmt = 0; fmt < 4; fmt++) {
        FILE *tmp = tmpfile();
        CHECK(tmp != NULL);
        if (fmt == 0) CHECK(cf_write_tokens(tmp, src, &lex_res) == 0);
        else if (fmt == 1) CHECK(cf_write_table(tmp, src, &lex_res) == 0);
        else if (fmt == 2) CHECK(cf_write_token_table(tmp, src, &lex_res) == 0);
        else CHECK(cf_write_report(tmp, src, "test.clx", &lex_res) == 0);

        rewind(tmp);
        CHECK(parser_input_load_stream(&loaded_in, &loaded_res, tmp) == 0);
        fclose(tmp);

        FILE *diag = tmpfile();
        CHECK(cf_parse(&loaded_in, diag, &parse_res) == 0);
        fclose(diag);

        const CfAstNode *prog = cf_ast_node(&parse_res, parse_res.root);
        CHECK(prog && prog->child_count == 2);
        const CfAstNode *s1 = cf_ast_node(&parse_res, prog->first_child);
        CHECK(s1->kind == CF_NODE_SHOW_STMT);
        const CfAstNode *plus = cf_ast_node(&parse_res, s1->left);
        CHECK(plus->kind == CF_NODE_BINARY && plus->op == CF_PLUS);
        CHECK(lexeme_eq(&loaded_in, &cf_ast_node(&parse_res, plus->left)->token, "\"Average: \""));
        const CfAstNode *call = cf_ast_node(&parse_res, plus->right);
        CHECK(call->kind == CF_NODE_CALL && call->child_count == 1);
        const CfAstNode *inner = cf_ast_node(&parse_res, call->first_child);
        CHECK(inner->kind == CF_NODE_CALL && inner->child_count == 2);

        const CfAstNode *s2 = cf_ast_node(&parse_res, s1->next_sibling);
        CHECK(s2->kind == CF_NODE_SHOW_STMT);
        CHECK(lexeme_eq(&loaded_in, &cf_ast_node(&parse_res, s2->left)->token, "name"));
    }
}

static void test_missing_period_e113_and_recovery(void)
{
    char diag[4096];

    /* Missing period at EOF */
    CHECK(lex_and_parse("show \"Hello\"", 0, diag, sizeof(diag)) == 1);
    CHECK(g_parse.errors == 1);
    CHECK(g_parse.error_list[0].code == 113);
    CHECK(strstr(diag, "Error E113 (Syntax Error)") != NULL);
    CHECK(strstr(diag, "What happened:") != NULL);
    CHECK(strstr(diag, "Why:") != NULL);
    CHECK(strstr(diag, "How to fix:") != NULL);
    CHECK(strstr(diag, "^") != NULL);

    /* Missing period on line 1, valid show on line 2 */
    CHECK(lex_and_parse("show \"Average: \" + to_text(avg)\nshow \"Done\".\n",
                        0, diag, sizeof(diag)) == 1);
    CHECK(g_parse.errors == 1);
    CHECK(g_parse.error_list[0].code == 113);
    CHECK(cf_ast_node(&g_parse, g_parse.root)->child_count == 1);

    /* Same in --braces mode (no NEWLINE token between statements) */
    CHECK(lex_and_parse("show \"Hello\"\nshow \"World\".\n", 1, diag, sizeof(diag)) == 1);
    CHECK(g_parse.errors == 1);
    CHECK(g_parse.error_list[0].code == 113);
    CHECK(cf_ast_node(&g_parse, g_parse.root)->child_count == 1);
}

static void test_invalid_show_concat_and_call_arguments(void)
{
    const char *bad_cases[] = {
        "show.\n",
        "show\n",
        "show \"Hello\" + .\n",
        "show + \"Hello\".\n",
        "show \"A\" + + \"B\".\n",
        "show \"Hello\" \"World\".\n",
        "show \"Average: \" to_text(avg).\n",
        "show average(total, ).\n",
        "show average(, count).\n",
        "show average(total count).\n",
        "show average(total,, count).\n",
        "show \"Hello\", name.\n",
        "show x = 5.\n",
        "show ().\n",
        "show arr[].\n",
        "show [1, ].\n",
        "show [1 2].\n",
        "show if.\n",
        "show show.\n",
    };
    size_t count = sizeof(bad_cases) / sizeof(bad_cases[0]);
    for (size_t i = 0; i < count; i++) {
        char diag[4096];
        int status = lex_and_parse(bad_cases[i], 0, diag, sizeof(diag));
        CHECK(status == 1);
        CHECK(g_parse.errors >= 1);
        CHECK(strstr(diag, "Error E114 (Syntax Error)") != NULL);
        CHECK(strstr(diag, "What happened:") != NULL);
        CHECK(strstr(diag, "Why:") != NULL);
        CHECK(strstr(diag, "How to fix:") != NULL);
        CHECK(cf_ast_node(&g_parse, g_parse.root)->child_count == 0);
    }
}

static void test_unclosed_delimiters_in_token_stream(void)
{
    /* Simulate pre-tokenized streams missing closing ')' or ']' (bypassing lexer E102). */
    const char *tables[] = {
        "TOKEN TYPE\tLEXEME\nSHOW\tshow\nIDENTIFIER\tto_text\nLPAREN\t(\nIDENTIFIER\tavg\nSTATEMENT_TERMINATOR\t.\n",
        "TOKEN TYPE\tLEXEME\nSHOW\tshow\nLPAREN\t(\nCONST_INT\t1\nSTATEMENT_TERMINATOR\t.\n",
        "TOKEN TYPE\tLEXEME\nSHOW\tshow\nIDENTIFIER\tarr\nLBRACKET\t[\nCONST_INT\t0\nSTATEMENT_TERMINATOR\t.\n",
    };
    static CfResult loaded_res;
    static CfParseResult parse_res;
    CfParserInput loaded_in;

    for (size_t i = 0; i < 3; i++) {
        FILE *tmp = tmpfile();
        CHECK(tmp != NULL);
        fputs(tables[i], tmp);
        rewind(tmp);
        CHECK(parser_input_load_stream(&loaded_in, &loaded_res, tmp) == 0);
        fclose(tmp);

        FILE *diag = tmpfile();
        CHECK(cf_parse(&loaded_in, diag, &parse_res) == 1);
        CHECK(parse_res.errors == 1);
        CHECK(parse_res.error_list[0].code == 114);
        fclose(diag);
    }
}

static void test_recovery_and_five_error_limit(void)
{
    char diag[8192];
    const char *mixed =
        "show \"a\" + .\n"
        "show f(x, ).\n"
        "show \"no dot\"\n"
        "show \"Recovered: \" + to_text(ok).\n";
    CHECK(lex_and_parse(mixed, 0, diag, sizeof(diag)) == 1);
    CHECK(g_parse.errors == 3);
    CHECK(cf_ast_node(&g_parse, g_parse.root)->child_count == 1);

    const char *many =
        "show.\nshow.\nshow.\nshow.\nshow.\nshow.\nshow.\nshow.\n";
    CHECK(lex_and_parse(many, 0, diag, sizeof(diag)) == 1);
    CHECK(g_parse.errors == CF_MAX_ERRORS);
    CHECK(strstr(diag, "Fix these first, then run again") != NULL);
}

static void test_direct_parse_show_and_ast_output(void)
{
    const char *src = "show \"Average: \" + to_text(average(total, count)).\n";
    CHECK(cf_lex(src, strlen(src), 0, stderr, NULL, &g_lex) == 0);
    parser_input_init(&g_in, &g_lex, src, strlen(src));

    CfParser p;
    cf_parser_init(&p, &g_in, stderr, &g_parse);
    uint32_t stmt = parse_show_stmt(&p);
    CHECK(stmt != CF_NODE_NONE);
    CHECK(g_parse.errors == 0);
    g_parse.root = stmt;

    FILE *out = tmpfile();
    CHECK(cf_write_ast(out, &g_in, &g_parse) == 0);
    rewind(out);
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, out);
    buf[n] = '\0';
    fclose(out);

    CHECK(strstr(buf, "ShowStmt") != NULL);
    CHECK(strstr(buf, "Binary +") != NULL);
    CHECK(strstr(buf, "Literal STRING \"Average: \"") != NULL);
    CHECK(strstr(buf, "Call (1 arg") != NULL);
    CHECK(strstr(buf, "Ident to_text") != NULL);
    CHECK(strstr(buf, "Call (2 args") != NULL);
    CHECK(strstr(buf, "Ident average") != NULL);
    CHECK(strstr(buf, "Ident total") != NULL);
    CHECK(strstr(buf, "Ident count") != NULL);
}

static void test_deep_unary_nesting_does_not_crash(void)
{
    char src[2048];
    size_t pos = 0;
    memcpy(src + pos, "show ", 5); pos += 5;
    for (int i = 0; i < 300; i++) {
        src[pos++] = '-';
        src[pos++] = ' ';
    }
    memcpy(src + pos, "1.\n", 3); pos += 3;
    src[pos] = '\0';

    char diag[4096];
    CHECK(lex_and_parse(src, 0, diag, sizeof(diag)) == 1);
    CHECK(g_parse.errors >= 1);
    CHECK(strstr(diag, "nested too deeply") != NULL);
}

int main(void)
{
    RUN_TEST(test_basic_show_literals_ident_and_noise);
    RUN_TEST(test_string_concatenation);
    RUN_TEST(test_show_concat_with_function_calls);
    RUN_TEST(test_postfix_chaining_and_multiline_grouping);
    RUN_TEST(test_precedence_and_associativity);
    RUN_TEST(test_same_line_and_braces_mode);
    RUN_TEST(test_loaded_from_all_four_lexer_table_formats);
    RUN_TEST(test_missing_period_e113_and_recovery);
    RUN_TEST(test_invalid_show_concat_and_call_arguments);
    RUN_TEST(test_unclosed_delimiters_in_token_stream);
    RUN_TEST(test_recovery_and_five_error_limit);
    RUN_TEST(test_direct_parse_show_and_ast_output);
    RUN_TEST(test_deep_unary_nesting_does_not_crash);

    printf("\nRan %d tests, %d failed.\n", tests_run, tests_failed);
    return tests_failed ? 1 : 0;
}
