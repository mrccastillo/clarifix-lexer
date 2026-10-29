# ClariFix Lexical Analyzer

ClariFix reads `.clx` files and shows their tokens, symbols, and lexical errors. It does not execute programs.

## 1. Set up once

Open the project folder in VS Code and open a terminal. Install GCC or Clang first.

Run the setup command for your computer:

Windows PowerShell:

```powershell
.\setup.ps1
```

Mac / Linux (Zsh or Bash):

```sh
sh ./setup
```

Setup saves the `build` and `lex` shortcuts for your account. Open a new terminal in the project folder before continuing.

## 2. Build and run

Use these commands on Windows, Mac, or Linux after setup:

```text
build
lex sample
```

Build once. Rebuild only after changing the C source files.

The terminal table shows only `TOKEN TYPE` and `LEXEME`, including source comments and noise words. Individual error messages appear when lexical errors are found.

Every analysis automatically creates or replaces `Symbol Table.txt` in the terminal's current folder. It contains the same bordered table as the terminal, with aligned `TOKEN TYPE` and `LEXEME` columns. For invalid source code, it contains the recognized lexemes from the partial analysis; read the terminal diagnostics for the errors.

To save an additional copy, run `lex sample --table bin/token-table.txt`. Use a fresh filename for each explicit export. The optional `--tokens FILE` export retains detailed positions and symbol IDs.

Use `lex programname`, replacing `programname` with your file's name without `.clx`. For example, `lex average` analyzes `examples/average.clx`.

You can also try `features` or `checklist`. The `errors` example intentionally contains errors. For braces, use `lex braces --braces`.

Run setup again if you move the project folder or change shells.

## 3. Write your own example

Create `examples/my-program.clx`, add the following, and save:

```text
set name to "Marc".
show name.
```

Run it with `lex my-program`. After editing a `.clx` file, save and run it again; no rebuild is needed.

## Save a report (optional)

```text
lex sample --report bin/report.txt
```

Open `bin/report.txt` in VS Code to see the tokens, symbol table, errors, and summary. Use a new filename or delete the old report before running the same command again. This detailed report is optional; `Symbol Table.txt` is generated automatically.

## Run tests (optional)

Python 3 is required for tests.

| Windows PowerShell | Mac / Linux |
| --- | --- |
| `build -Test` | `build --test` |

## Source code folder

All C source files and headers are in `src/`:

```text
src/
  main.c          File input, command-line options, and output coordination
  lexer.c         Scanning loop and token recognition
  dfa.c           Hardcoded word-state transitions
  symbols.c       Unique identifiers, constants, and symbol IDs
  diagnostics.c   State tracing, errors, and recovery
  output.c        Terminal tables and saved reports
  lexer.h         Public types, results, limits, and functions
  dfa.h           Word-state definitions
  internal.h      Private scanner data and module interfaces
```

Run `build` and `lex` from the project folder as before. Both build scripts compile the files in `src/` and place the executable in `bin/`.

## Follow the states for a demonstration

```text
lex trace --trace
```

This runs `examples/trace.clx` and shows character transitions, token acceptance, symbol insertion or reuse, and indentation events. The trace goes to the diagnostic stream; the terminal table and `Symbol Table.txt` keep their usual format. Omit `--trace` for normal output.

For the keyword `if`, the trace includes:

```text
TRACE 2:1 WORD START -- 'i' --> I
TRACE 2:2 WORD I -- 'f' --> IF
TRACE 2:3 WORD IF -- SPACE --> BOUNDARY (not consumed)
TRACE 2:1 ACCEPT IF length=2
```

The space ends the word and is handled separately. With `ifx`, the next letter moves from `IF` to `IDENT`, so the whole word becomes an identifier. A keyword is accepted only after the complete word has been read.

Start reading the code in this order:

| File / function | What to explain |
| --- | --- |
| `src/main.c`: `main()` and `read_source()` | Read a `.clx` file into a fixed array, call the lexer, and write output. |
| `src/lexer.c`: `cf_lex()` | Look at the next character and select the appropriate recognizer. |
| `src/dfa.c`: `word_next()` | Hardcoded `switch` cases move between named word states. |
| `src/dfa.c`: `word_token()` | Classify the final state at a word boundary. |
| `src/lexer.c`: `number()`, `literal()`, `comment()`, `operator_next()` | Follow explicit states for other token families. |
| `src/lexer.c`: `emit()` | Record each recognized token. |
| `src/symbols.c`: `cf_symbol_insert()` | Insert or reuse unique identifiers and constants. |
| `src/diagnostics.c`: `cf_error_at()` and `cf_recover()` | Explain an invalid lexeme and skip the affected line. |
| `src/output.c` | Format the terminal table and saved reports. |
| `src/lexer.h` | Token definitions and fixed storage limits. |
| `src/internal.h` | Scanner data and declarations shared by implementation files. |

For example, `word_next()` contains these transitions:

```c
case W_I:
    if (c == 'f') return W_IF;
    if (c == 'n') return W_IN;
    if (c == 's') return W_IS;
    return W_IDENT;
```

`word_token()` then maps `W_IF` to `CF_IF`. This is the same code used in normal and traced runs. States are written directly in the source; no trie or transition graph is built at runtime.

The C analyzer uses fixed arrays, with no calls to `malloc`, `calloc`, `realloc`, or `free`. The internal symbol table still uses djb2 hashing and 211 buckets; collision chains use array indices. Indentation and bracket matching use fixed stacks alongside the token DFAs.

| Storage | Limit |
| --- | --- |
| Source file | 262,144 bytes before newline/BOM normalization |
| Parser tokens | 32,768, including layout and EOF tokens |
| Source-report entries | 32,768 |
| Unique identifiers and constants | 8,192 |
| Indentation stack / bracket stack | 256 entries each; indentation includes its initial level |
| Saved symbol text | 540,672 bytes, including string terminators |
| Lexical diagnostics | 5 per run |

Exceeding a storage limit stops the analyzer with a capacity message and exit code 2. These limits are declared in `src/lexer.h`. The trace explains recognition; it does not execute the ClariFix program.

## Run without shortcuts

If `build` or `lex` is not recognized, run setup and open a new terminal, or use the scripts directly:

| Windows PowerShell | Mac / Linux |
| --- | --- |
| `.\build.ps1` | `sh ./build` |
| `.\lex.ps1 sample` | `sh ./lex sample` |
