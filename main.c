#include "lexer.h"

#include <stdlib.h>
#include <string.h>

static void usage(FILE *out)
{
    fputs("ClariFix lexical analyzer\n"
          "Usage: clarifix [--braces] [--report FILE] [--tokens FILE] [--symbols FILE] SOURCE.clf\n"
          "       clarifix --help\n\n"
          "Default: four-space indentation and token output on stdout.\n"
          "--braces       Use { } blocks; ignore indentation and newlines.\n"
          "--tokens FILE  Write token TSV to FILE instead of stdout.\n"
          "--symbols FILE Write the internal trie-backed symbol inventory as TSV.\n"
          "--report FILE  Write the instructor's two-column Symbol Table as TSV.\n"
          "Exit codes: 0 = lexical success, 1 = lexical errors, 2 = usage/I/O error.\n",
          out);
}

static char *read_source(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    char *source = NULL;
    size_t count = 0, capacity = 0;
    int ch;
    if (!file) { perror(path); return NULL; }
    while ((ch = fgetc(file)) != EOF) {
        if (count + 1 >= capacity) {
            size_t next = capacity ? capacity * 2 : 4096;
            if (next <= capacity) { fputs("Source file is too large.\n", stderr); free(source); fclose(file); return NULL; }
            char *grown = realloc(source, next);
            if (!grown) { fputs("Out of memory reading source.\n", stderr); free(source); fclose(file); return NULL; }
            source = grown; capacity = next;
        }
        source[count++] = (char)ch;
    }
    if (ferror(file)) { perror(path); free(source); fclose(file); return NULL; }
    fclose(file);
    if (!source) {
        source = malloc(1);
        if (!source) { fputs("Out of memory.\n", stderr); return NULL; }
    }
    /* Normalize CRLF/CR and remove an optional UTF-8 BOM. */
    size_t start = count >= 3 && (unsigned char)source[0] == 0xef &&
                   (unsigned char)source[1] == 0xbb && (unsigned char)source[2] == 0xbf ? 3 : 0;
    size_t dest = 0;
    for (size_t i = start; i < count; i++) {
        if (source[i] == '\r') {
            source[dest++] = '\n';
            if (i + 1 < count && source[i + 1] == '\n') i++;
        } else source[dest++] = source[i];
    }
    source[dest] = '\0';
    *length = dest;
    return source;
}

static int write_output(const char *path, int symbols, const char *source, const CfResult *r)
{
    /* C11 exclusive creation protects source files and previous results. */
    FILE *out = path ? fopen(path, "wx") : stdout;
    if (!out) { perror(path); return 2; }
    int status = symbols == 2 ? cf_write_report(out, source, r) :
                 symbols == 1 ? cf_write_symbols(out, r) : cf_write_tokens(out, source, r);
    if (path) { if (fclose(out) != 0) status = 2; }
    else if (fflush(out) != 0) status = 2;
    if (status) fprintf(stderr, "Could not write %s.\n", path ? path : "stdout");
    return status;
}

int main(int argc, char **argv)
{
    const char *input = NULL, *tokens = NULL, *symbols = NULL, *report = NULL;
    int braces = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) { usage(stdout); return 0; }
        if (strcmp(argv[i], "--braces") == 0) braces = 1;
        else if (strcmp(argv[i], "--tokens") == 0 || strcmp(argv[i], "--symbols") == 0 ||
                 strcmp(argv[i], "--report") == 0) {
            int is_symbols = strcmp(argv[i], "--symbols") == 0;
            int is_report = strcmp(argv[i], "--report") == 0;
            if (++i == argc || argv[i][0] == '-') { usage(stderr); return 2; }
            if (is_report) report = argv[i];
            else if (is_symbols) symbols = argv[i]; else tokens = argv[i];
        } else if (argv[i][0] == '-' || input) { usage(stderr); return 2; }
        else input = argv[i];
    }
    if (!input) { usage(stderr); return 2; }
    const char *extension = strrchr(input, '.');
    if (!extension || strcmp(extension, ".clf") != 0) {
        fputs("Error E100 (Lexical Error)\n"
              "What happened: The source filename does not end in .clf.\n"
              "Why: This implementation uses .clf for ClariFix source files.\n"
              "How to fix: Save your source with the .clf extension.\n", stderr);
        return 1;
    }
    size_t length;
    char *source = read_source(input, &length);
    if (!source) return 2;
    CfResult result;
    int status = cf_lex(source, length, braces, stderr, &result);
    if (write_output(tokens, 0, source, &result)) status = 2;
    if (symbols && write_output(symbols, 1, source, &result)) status = 2;
    if (report && write_output(report, 2, source, &result)) status = 2;
    fprintf(stderr, "Lexical analysis: %zu token(s), %zu symbol(s), %zu error(s).%s\n",
            result.token_count, result.symbol_count, result.errors,
            result.errors ? " Token output is partial; do not execute it. Report output is partial." : "");
    cf_free(&result);
    free(source);
    return status;
}
