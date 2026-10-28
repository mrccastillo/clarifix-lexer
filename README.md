# ClariFix lexical analyzer

A handwritten C11 lexical analyzer aligned with pages 18–20 of the [updated Group 4 paper](https://github.com/mrccastillo/clarifix-lexer/releases/download/paper-alignment-2/ClariFix-Updated-Paper.pdf). Source is hosted in the private [mrccastillo/clarifix-lexer](https://github.com/mrccastillo/clarifix-lexer) repository.

The analyzer recognizes source lexemes using character automata, stores unique identifiers/constants in a manually implemented djb2 hash table, and generates the paper's four-section Symbol Table report. It does not execute ClariFix programs or perform parsing, semantic analysis, or runtime evaluation.

## Build and run

Use desktop GCC or Clang with C11 support. Python 3 is needed for tests, report-generation scripts, and packaging; it does not tokenize source.

Windows PowerShell:

```powershell
.\build.ps1 -Test
.\build\clarifix.exe --report build\sample-report.txt examples\sample.clx
Get-Content build\sample-report.txt
.\build\clarifix.exe --braces examples\braces.clf
.\build.ps1 -Examples -OutputDirectory build\fresh-reports
```

Output files and report directories must be new. The analyzer protects source files and previous outputs through exclusive creation. Choose a different filename/directory for a later run. Reports and parser tokens from invalid input are partial; inspect stderr and section C.

`build.ps1` refreshes missing compiler paths from the saved Windows PATH, so it works in a VS Code terminal opened before GCC was installed. An explicit compiler path is also supported:

```powershell
.\build.ps1 -Compiler C:\path\to\gcc.exe -Test
```

Direct compilation:

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 main.c lexer.c -o clarifix.exe
python tests\test_lexer.py .\clarifix.exe
```

Linux/macOS:

```sh
make test
./clarifix --report sample-report.txt examples/sample.clx
```

Windows is the verified environment. All 38 regression tests and six example runs pass.

## Paper example

`examples/sample.clx` reproduces the embedded page-20 input:

```text
# avg calc
set total to 10.
show total.
```

Its report has nine source-token rows and exactly two unique symbol entries:

| ID | Lexeme | Token Type | Value | Length | First Seen |
| --- | --- | --- | --- | --- | --- |
| 1 | total | IDENTIFIER | - | 5 | 2:5 |
| 2 | 10 | CONST_INT | 10 | 2 | 2:14 |

Summary: identifiers 2, keywords 2, noise words 1, constants 1, delimiters 2, comments 1; no invalid tokens. Generate the report with the command above; saved report copies are excluded from the source tree.

## Outputs

| Option | Output |
| --- | --- |
| `--report FILE` | Paper report: A source token list, B unique symbols, C invalid records, D category summary. Fixed-width columns and `(none)` in empty sections. |
| `--symbols FILE` | Unique identifiers/constants TSV with type, category, converted value, length, first location, and occurrence count. |
| `--table FILE` | Simple two-column `TOKEN TYPE` / `LEXEME` TSV for the original instructor checklist. |
| `--tokens FILE` | Detailed parser-token TSV, retaining internal token enums, locations, and symbol IDs. Defaults to stdout when the flag is absent. |

Public report names include IDENTIFIER, CONST_INT, CONST_DECIMAL, CONST_CHAR, CONST_STRING, CONST_BOOLEAN, NOISE, SINGLE_COMMENT, MULTI_COMMENT, and STATEMENT_TERMINATOR. Original lexemes remain unchanged. Quotes and comment markers are retained; controls and invalid bytes are escaped.

Symbol IDs start at 1 in first-occurrence order. Only identifiers and constants have symbol IDs; other tokens use 0. Repeated names/constants reuse IDs. The table starts empty, with 211 djb2 buckets and separate chains. Reserved words, noise, operators, delimiters, and comments are not symbol entries. Boolean constants are inserted only when encountered.

Numeric values use exact normalized base-10 text, avoiding machine overflow or rounding in the lexical phase. String/character values decode supported escapes. Source lexeme length counts Unicode scalars, including quotes and escape characters; first columns count UTF-8 bytes. A variable's runtime value/type remains a later-phase concern.

## Language rules and paper ambiguities

- `.clx` is canonical, following the updated sample; `.clf` remains a compatibility alias.
- Identifiers use ASCII `[A-Za-z_][A-Za-z0-9_]*`, are case-sensitive, and have a 32-character limit.
- Five scalar literal types are recognized; arrays are compound bracket/element/comma syntax. A decimal point requires a following digit: `19.99.` is DECIMAL then PERIOD; `20.` is INT then PERIOD. Minus is separate. Exponents and leading-dot decimals are unsupported.
- Literals use straight quotes. Character literals contain exactly one UTF-8 scalar or supported escape. Supported escapes are backslash followed by `n`, double quote, single quote, or backslash. Smart quotes, raw newlines, NUL, invalid UTF-8, and unknown escapes are rejected in literals.
- `#` begins a line comment; `/* ... */` begins a non-nesting block comment. `//` is floor division. Comments appear in source reports and are omitted from parser tokens.
- Default mode follows four-space indentation and generates layout tokens. `--braces` explicitly selects free-field braces and suppresses layout. Newlines inside grouping are suppressed in default mode.
- Allowed `to` remains NOISE in the source report and becomes ASSIGN in parser tokens. Allowed `the`, `of`, and `then` remain report rows but are omitted from parser tokens. Misplaced noise receives E105.
- Use ASCII `--` for decrement. `int` is an ordinary identifier; the optional declaration example is not implemented grammar. Use `set score = 90.`.

The choices above resolve conflicting passages in the paper. PDFs and the detailed alignment documents remain available in Git history and the [original alignment release](https://github.com/mrccastillo/clarifix-lexer/releases/tag/paper-alignment-2). They are excluded from the current source tree and newly generated packages at the user's request.

## Recognition and diagnostics

Character transitions classify source; trie terminal identity allows exact hash-chain key comparisons by integer identity and token type. No premade lexer or whole-word string matching is used. CLI comparisons in `main.c` parse options/extensions rather than source words. Layout and bracket stacks are separate from finite lexeme automata.

Diagnostics explain where, what, why, and how to fix a problem. E100 and E102–E112 are lexical checks. Recovery skips a damaged physical line, removes its bracket openers, and stops after five errors. Invalid records retain the offending fragment, code, and reason. Invalid UTF-8 and controls are escaped in diagnostics and reports. Numeric source columns remain authoritative where a terminal displays combining/wide glyphs differently.

Exit codes: 0 lexical success, 1 lexical errors, 2 usage/memory/I/O failure. Missing colons/periods, reserved words used in variable positions, undefined names, type checks, and execution belong to later phases. The paper's deliberately wrong program can pass the lexer because its demonstrated errors belong to those phases.

## Verification and submission

```powershell
.\build.ps1 -Test
python tools\package_submission.py --output build\paper-release
python tools\verify_submission.py build\paper-release\clarifix-paper-alignment-2.zip
```

Use a fresh directory for packaging. Build the analyzer first. The packager generates fresh reports in a temporary directory and includes them in the ZIP with sources, the current specification, tests, and examples. No saved reports are required in the repository. Binaries, caches, and temporary logs are excluded. The manifest identifies every payload file. Verification rebuilds in a fresh temporary directory, runs the full suite and all six examples, and compares regenerated reports against packaged samples.

Report samples are generated with `tools/generate_samples.py` or `build.ps1 -Examples`; report, parser-token, two-column, unique-symbol, diagnostic, and JSON-summary outputs are produced together.

Current specification: [updated-paper alignment](SPEC-UPDATED-PAPER-ALIGNMENT.md). Release label: `paper-alignment-2`. The [submission checklist](submission/CHECKLIST.md) tracks remaining course hand-in, deadline, access, printing, and index-card steps. Publishing a private GitHub release does not perform course hand-in.

## Parser integration

Keep the normalized source buffer alive, call `cf_lex`, and inspect `CfResult.tokens` after lexical success. `CfResult.report` stores source occurrences; `CfResult.symbols` stores unique identifiers/constants; `CfResult.invalid` stores diagnostic fragments. `CfResult.buckets` and symbol `next` indices contain the collision chains. Call `cf_free` before reusing a result. Constants have decoded/canonical values, not evaluated expressions or variable assignments.
