/* Lexer functions, result structures, storage limits, and file I/O definitions. */
#include "lexer.h"

/* String helpers used to compare options and find the filename extension. */
#include <string.h>

/* Print help to the terminal's normal output or error output, as requested. */
static void usage(FILE *out)
{
    fputs("ClariFix lexical analyzer\n"
          "Usage: clarifix [--trace] [--braces] [--report FILE] [--table FILE] [--tokens FILE] [--symbols FILE] SOURCE.clx\n"
          "       clarifix --help\n\n"
          "Default: four-space indentation and a token table on stdout.\n"
          "Each analysis replaces Symbol Table.txt in the current directory.\n"
          "--trace        Show character-by-character states on stderr.\n"
          "--braces       Use { } blocks; ignore indentation and newlines.\n"
          "--tokens FILE  Write token TSV to FILE instead of stdout.\n"
          "--symbols FILE Write unique identifiers/constants and attributes as TSV.\n"
          "--report FILE  Write the paper's four-section Symbol Table report.\n"
          "--table FILE   Write a two-column token-type/lexeme TSV.\n"
          "Source type: .clx\n"
          "Exit codes: 0 = lexical success, 1 = lexical errors, 2 = usage/I/O error.\n",
          out);
}

/* Load and clean the source text; give its final byte count back through length. */
static char *read_source(const char *path, size_t *length)
{
    /* Binary mode preserves the original bytes so we can handle line endings ourselves. */
    FILE *file = fopen(path, "rb");
    /* Static storage stays valid after this function returns; +1 leaves room for '\0'. */
    static char source[CF_MAX_SOURCE + 1];
    size_t count = 0;
    /* Use int so ch can hold either a byte or the special end-of-file value EOF. */
    int ch;
    if (!file) { perror(path); return NULL; }
    /* Read one byte at a time, stopping if the file is larger than our buffer. */
    while ((ch = fgetc(file)) != EOF) {
        if (count == CF_MAX_SOURCE) {
            fprintf(stderr, "Capacity limit: source bytes (maximum %d).\n", CF_MAX_SOURCE);
            fclose(file);
            return NULL;
        }
        source[count++] = (char)ch;
    }
    /* EOF can also signal a read failure, so check before closing the file. */
    if (ferror(file)) { perror(path); fclose(file); return NULL; }
    fclose(file);
    /* Skip the optional three-byte UTF-8 marker that some editors add at the start. */
    size_t start = count >= 3 && (unsigned char)source[0] == 0xef &&
                   (unsigned char)source[1] == 0xbb && (unsigned char)source[2] == 0xbf ? 3 : 0;
    /* Rewrite the text in place, turning CRLF and CR line endings into one LF. */
    size_t dest = 0;
    for (size_t i = start; i < count; i++) {
        if (source[i] == '\r') {
            source[dest++] = '\n';
            /* For CRLF, skip the existing LF so we do not add two newlines. */
            if (i + 1 < count && source[i + 1] == '\n') i++;
        } else source[dest++] = source[i];
    }
    /* End the C string and return both the cleaned text and its length. */
    source[dest] = '\0';
    *length = dest;
    return source;
}

/* Named choices tell write_output which format to produce. */
typedef enum {
    OUTPUT_TOKENS,
    OUTPUT_SYMBOLS,
    OUTPUT_REPORT,
    OUTPUT_TSV_TABLE,
    OUTPUT_AUTOMATIC_TABLE
} OutputKind;

/* Write lexer results to a file, or to normal terminal output when path is NULL. */
static int write_output(const char *path, OutputKind kind, const char *source,
                        const char *source_name, const CfResult *r)
{
    /* "w" replaces the automatic table; "wx" prevents overwriting explicit exports. */
    FILE *out = path ? fopen(path, kind == OUTPUT_AUTOMATIC_TABLE ? "w" : "wx") : stdout;
    if (!out) { perror(path); return 2; }
    int status = 0;
    /* Choose the writer that matches the requested output format. */
    switch (kind) {
    case OUTPUT_AUTOMATIC_TABLE:
        status = cf_write_token_table(out, source, r); break;
    case OUTPUT_TSV_TABLE:
        status = cf_write_table(out, source, r); break;
    case OUTPUT_REPORT:
        status = cf_write_report(out, source, source_name, r); break;
    case OUTPUT_SYMBOLS:
        status = cf_write_symbols(out, r); break;
    case OUTPUT_TOKENS:
        /* A file gets tab-separated tokens; the terminal gets a formatted table. */
        status = path ? cf_write_tokens(out, source, r) : cf_write_token_table(out, source, r);
        break;
    }
    /* Finish buffered writes and catch errors that only appear when finishing. */
    if (path) { if (fclose(out) != 0) status = 2; }
    else if (fflush(out) != 0) status = 2;
    if (status) fprintf(stderr, "Could not write %s.\n", path ? path : "stdout");
    return status;
}

/* Program entry point: argc counts command arguments; argv holds their text. */
int main(int argc, char **argv)
{
    /* No filenames are selected yet, and brace mode and tracing start disabled. */
    const char *input = NULL, *tokens = NULL, *symbols = NULL, *report = NULL, *table = NULL;
    int braces = 0, trace = 0;
    /* Start at 1 because argv[0] is the program name. strcmp returns 0 for a match. */
    for (int i = 1; i < argc; i++) {
        /* Help exits immediately; the next two options simply enable a setting. */
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) { usage(stdout); return 0; }
        if (strcmp(argv[i], "--braces") == 0) braces = 1;
        else if (strcmp(argv[i], "--trace") == 0) trace = 1;
        else if (strcmp(argv[i], "--tokens") == 0 || strcmp(argv[i], "--symbols") == 0 ||
                 strcmp(argv[i], "--report") == 0 || strcmp(argv[i], "--table") == 0) {
            /* Remember the option before advancing to its required output filename. */
            int is_symbols = strcmp(argv[i], "--symbols") == 0;
            int is_report = strcmp(argv[i], "--report") == 0;
            int is_table = strcmp(argv[i], "--table") == 0;
            /* Reject a missing filename or another option where a filename belongs. */
            if (++i == argc || argv[i][0] == '-') { usage(stderr); return 2; }
            /* Save the filename for the matching export. */
            if (is_table) table = argv[i];
            else if (is_report) report = argv[i];
            else if (is_symbols) symbols = argv[i]; else tokens = argv[i];
        /* Reject unknown options and extra source files; accept one input path. */
        } else if (argv[i][0] == '-' || input) { usage(stderr); return 2; }
        else input = argv[i];
    }
    /* A source file is required, and its extension must be exactly .clx. */
    if (!input) { usage(stderr); return 2; }
    const char *extension = strrchr(input, '.');
    if (!extension || strcmp(extension, ".clx") != 0) {
        fputs("Error E100 (Lexical Error)\n"
              "What happened: The source filename does not end in .clx.\n"
              "Why: ClariFix source files use the .clx extension.\n"
              "How to fix: Save your source with the .clx extension.\n", stderr);
        return 1;
    }
    /* Load the file; &length lets read_source fill in the source byte count. */
    size_t length;
    char *source = read_source(input, &length);
    if (!source) return 2;
    /* Keep the large result structure in static storage to avoid a large stack use. */
    static CfResult result;
    /* Analyze the text, send errors to stderr, and enable tracing only if requested. */
    int status = cf_lex(source, length, braces, stderr, trace ? stderr : NULL, &result);
    /* Point to the filename after the last slash, supporting Windows and Unix paths. */
    const char *source_name = input;
    for (const char *p = input; *p; p++) if (*p == '/' || *p == '\\') source_name = p + 1;
    /* Always write the automatic table and tokens, even if lexical errors were found. */
    if (write_output("Symbol Table.txt", OUTPUT_AUTOMATIC_TABLE, source, source_name, &result)) status = 2;
    if (write_output(tokens, OUTPUT_TOKENS, source, source_name, &result)) status = 2;
    /* Write optional exports only when their filenames were supplied. */
    if (symbols && write_output(symbols, OUTPUT_SYMBOLS, source, source_name, &result)) status = 2;
    if (report && write_output(report, OUTPUT_REPORT, source, source_name, &result)) status = 2;
    if (table && write_output(table, OUTPUT_TSV_TABLE, source, source_name, &result)) status = 2;
    /* Exit with 0 for success, 1 for lexical errors, or 2 for usage/I/O errors. */
    return status;
}
