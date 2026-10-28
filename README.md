# ClariFix lexical analyzer

GitHub repository: [mrccastillo/clarifix-lexer](https://github.com/mrccastillo/clarifix-lexer).

A C11 lexer for the language in the [revised proposal](docs/ClariFix-Revised-Proposal.md). It reads a `.clf` source file, produces tokens with source locations, interns identifiers through character-trie transitions, and reports beginner-friendly lexical errors.

This is the first stage of the language implementation. It does not execute programs, check statement grammar, enforce operator precedence, resolve variables, or infer runtime types. For example, `show missing.` is lexically valid; a later semantic checker must report E201.

## Build and run

Use a **desktop GCC or Clang compiler** with C11 support. Python 3 is needed for tests, example report generation, and packaging tools.

Windows PowerShell:

```powershell
.\build.ps1 -Test
.\build\clarifix.exe --report build\average-symbol-table.txt examples\average.clf
.\build\clarifix.exe --tokens tokens.txt --symbols symbol_table.txt examples\average.clf
.\build\clarifix.exe --braces examples\braces.clf
.\build.ps1 -Examples -OutputDirectory build\fresh-reports
```

If GCC is not on PATH, pass its location:

```powershell
.\build.ps1 -Compiler C:\msys64\ucrt64\bin\gcc.exe -Test
```

If your VS Code terminal was open before GCC was installed, `build.ps1` refreshes missing compiler paths from the saved Windows PATH automatically. Run `.\build.ps1 -Test` again. Restart VS Code to refresh PATH for commands you run directly, such as `gcc --version`.

Or compile directly:

```powershell
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 main.c lexer.c -o clarifix.exe
.\clarifix.exe examples\features.clf
python tests\test_lexer.py .\clarifix.exe
```

Linux/macOS:

```sh
make
./clarifix examples/average.clf
make test
```

Output files must **not already exist**. Exclusive creation protects the source and previous results. Choose new output filenames or remove old result files before another export. Diagnostics and a summary go to stderr. Tokens go to stdout unless `--tokens` is provided. Use `--report` for the instructor-format Symbol Table and `--symbols` for the internal symbol inventory. Use `--help` for usage. Exit status is 0 for lexical success, 1 for lexical errors, and 2 for usage, memory, or I/O failures. Output following a lexical error is partial and must not be executed.

## Example

```text
set age to 20.
show age.
```

The token sequence is:

```text
SET IDENT ASSIGN INT PERIOD NEWLINE
SHOW IDENT PERIOD NEWLINE EOF
```

`to` produces an `ASSIGN` token whose original lexeme is `to`. The report and internal inventory retain it as a noise word. Each token's original spelling, line, column, and symbol ID remain available to a future parser.

The instructor-format Symbol Table (`--report`) has exactly two tab-separated columns:

```text
TOKEN TYPE    LEXEME
SET           set
IDENTIFIER    age
NOISE_WORD    to
INT_LITERAL   20
PERIOD        .
SHOW          show
IDENTIFIER    age
PERIOD        .
```

The report retains recognized lexemes in source order, including repeated identifiers, literals, comments, and allowed noise words. It excludes synthetic layout/EOF tokens and unused vocabulary. See [the report contract](docs/report-format.md) for token names and escaping. The instructor's example uses another language's spellings; ClariFix retains its own vocabulary and period terminator.

The developer token file (`--tokens`) is tab-separated:

```text
LINE    COLUMN    TOKEN    LEXEME    SYMBOL_ID
```

The internal symbol inventory (`--symbols`) is tab-separated:

```text
ID    LEXEME    TOKEN    ORIGIN    FIRST_LINE    FIRST_COLUMN    OCCURRENCES
```

Backslashes and control characters are escaped in exports. Literal spellings include their surrounding quotes; literal values are not decoded or converted to machine numbers yet. `NEWLINE`, `INDENT`, `DEDENT`, and `EOF` have empty lexemes and symbol ID 0. Numeric/string/character literals also have ID 0; only fixed spellings and identifiers are interned. A variable's dynamic value/type belongs to later phases, not this lexical table.

## Implemented rules

- ASCII identifiers match `[A-Za-z_][A-Za-z0-9_]*`, are case-sensitive, and have a maximum length of 32 characters.
- Integers and decimals follow the proposal. A decimal dot requires a following digit: `19.99.` becomes `DECIMAL` and `PERIOD`; `20.` becomes `INT` and `PERIOD`. Negative values start with a separate `MINUS` token. Exponents, leading-dot decimals, and digit separators are not supported.
- Strings use straight double quotes. Character literals use straight single quotes and contain exactly one UTF-8 scalar or supported escape. Supported escapes are `\n`, `\"`, `\'`, and `\\`. Raw newlines in literals are rejected. UTF-8 text is allowed inside literals; identifiers remain ASCII.
- `#` starts a line comment. `/* ... */` starts a non-nesting block comment. Comments produce no parser-facing tokens but appear in the submission report. `//` is floor division, as in the formal grammar.
- All listed arithmetic, relational, logical, and assignment operators are recognized using longest match. Both `&&`/`||`/`!` and `and`/`or`/`not` use the same token kinds. `is` has its own token, and `is not` remains two tokens.
- Delimiters include parentheses, square brackets, commas, periods, and colons. Braces are accepted in `--braces` mode. Mismatched and unclosed brackets are detected.
- The 13 keywords in the proposal, Boolean literals, word operators, noise words, and operator/delimiter spellings initialize a character trie with 57 fixed spellings (23 word spellings and 34 symbolic spellings). New identifiers are interned once, with occurrence counts and their first location. Builtins such as `range`, `to_text`, `to_int`, and `to_decimal` remain identifiers.

## Decisions for conflicting draft rules

These decisions are incorporated into the [revised proposal](docs/ClariFix-Revised-Proposal.md). The original root-level draft Markdown and PDF are preserved as historical references:

| Draft ambiguity | Choice in this lexer |
| --- | --- |
| Indented blocks versus free-field braces | Default to the formal grammar and correct sample: four-space indentation. Select `--braces` for free-field code. |
| Layout tokens | Default mode emits `NEWLINE` for nonempty logical lines, plus `INDENT`/`DEDENT`. Blank and comment-only lines do not change indentation. Parentheses and square brackets suppress layout while open. A final logical newline and outstanding dedents are emitted at EOF. |
| Indentation errors | A new level adds exactly four spaces; dedents match an existing level. Tabs in code indentation cause E107. Tabs between tokens, in continuation lines, in comments, and in braces mode are whitespace. Whether indentation follows an actual block header is a parser check. |
| Curly braces with indentation | Braces require `--braces`. That mode emits brace tokens and suppresses all layout tokens; spaces/tabs/newlines are separators, and periods delimit simple statements. |
| Logical spellings and floor division | Recognize both the operator tables and the grammar: symbolic and word logical operators, `is`, and `//`. |
| Printed decrement en dash | Use ASCII `--`. An en dash outside text is illegal. |
| Noise words | Normalize `to` after a `set` target (including indexed targets) to `ASSIGN`. Drop `then` after an `if`/`elif` condition immediately before a colon, ignoring intervening trivia. Drop `the`/`of` immediately after `set`/`show`/`ask`, or just after a `set` assignment separator. Multiple fillers at those boundaries are allowed. Other uses produce E105. Full expression validity is a parser check. |
| Reserved identifiers | Keywords/noise words are never emitted as `IDENT`. Detecting attempts to use a keyword as a variable (E202) belongs to the parser/semantic checker. Noise misuse is caught as E105 here. |
| Number of preloaded entries | Preload 57 fixed spellings derived from the supported rules rather than inventing entries to match the draft's unreconciled count of 52. Quotes are literal boundaries, not standalone interned delimiters. |
| Source extension | Use lowercase `.clf`; other extensions produce E100. |
| Source encoding and columns | Read UTF-8, strip an optional BOM, normalize CRLF/CR to LF. Locations are one-based and columns count UTF-8 bytes. |

Comments are whitespace. A physical newline inside a block comment still ends the preceding logical line when outside grouping. To continue a default-mode expression across lines, enclose it in parentheses or square brackets. In braces mode, statements may span lines freely.

## Diagnostics

Implemented lexical codes: **E100** and **E102–E112**. E101 (missing colon) and E113 (missing period) need a parser and are intentionally not emitted here. E202 and all name/type/runtime errors also need later phases.

```text
Error E110 (Lexical Error) at line 1, column 5
  set 1name = 2.
      ^
What happened: A name begins with a number, or a number touches a name.
Why: Identifiers must start with an ASCII letter or underscore.
How to fix: Start the name with a letter, or separate the number and name with an operator.
```

After a recoverable lexical error, scanning skips the rest of the physical line. Brackets opened on the damaged line are discarded from delimiter tracking. The scanner reports at most five errors, then prints `Fix these first, then run again`. Unclosed brackets and block comments point to their opening marker.

## Files and tests

| File | Purpose |
| --- | --- |
| `lexer.h` | Shared token vocabulary, result structures, and API for a future parser |
| `lexer.c` | Character-trie recognizer and interning, diagnostics, layout, and TSV exporters |
| `main.c` | Command-line options, file reading/newline normalization, and outputs |
| `build.ps1` / `Makefile` | Build and test commands |
| `examples/average.clf` | Correct sample from the proposal |
| `examples/features.clf` | Token families, literals, operators, and noise words |
| `examples/braces.clf` | Alternate explicit-block mode |
| `examples/errors.clf` | Five deliberate lexical errors |
| `examples/checklist.clf` | Instructor checklist coverage, including all nine Boolean-expression operators |
| `docs/lexer-automata.md` | States, transitions, accepting actions, and vocabulary |
| `docs/ClariFix-Revised-Proposal.pdf` | Printable revised proposal |
| `submission/reports/` | Generated two-column report examples and diagnostics |
| `tests/test_lexer.py` | Black-box regression tests against the compiled executable |

The tests cover exact token sequences and locations, number/period boundaries, all operators, comments, Unicode literals, noise normalization, both block styles, error codes, five-error recovery, symbol reuse, large tables, safe output creation, CLI failures, the examples, and seeded malformed inputs.

The strict desktop GCC build and all 29 regression test methods pass, including invalid UTF-8 diagnostics and the new report tests. All five examples produce their expected exit statuses. See [validation evidence](docs/validation.md) for compiler details, commands, counts, PDF checks, and clean-package verification.

## Recognition design

The handwritten scanner follows individual character edges and accepting states; identifier interning uses stable trie terminal IDs. It does not classify source with a premade lexer, regular expressions, or whole-word string comparisons. [Automata documentation](docs/lexer-automata.md) supplies transition tables. Layout and matching brackets use separate stacks. Command-line option and filename comparisons in `main.c` do not tokenize source.

Invalid UTF-8/control bytes in diagnostics are escaped as `\xHH`; valid UTF-8 remains intact. Source locations use byte columns, while the caret accounts for escaped bytes and Unicode scalars. Terminal display width of combining/wide glyphs varies; the numeric byte location remains authoritative.

## Submission package

The release includes sources, examples, tests, reports, the revised proposal PDF, and validation evidence. It excludes binaries, compiler tools, caches, machine-specific logs, and the superseded original proposal. The originals remain unchanged in the workspace.

```powershell
python tools\package_submission.py --output build\release
python tools\verify_submission.py build\release\clarifix-lexer-compliance-1.zip
```

Use a fresh output directory for each package. The verifier checks every SHA-256 manifest entry, extracts to a fresh temporary directory, follows the PowerShell build/test/example commands, and compares regenerated reports with packaged samples. It requires the same desktop GCC and Python setup as the normal build.

The optional PDF regeneration tool requires ReportLab and Markdown in a separate environment; they are not required to compile the lexer or run its tests. Run `python tools/render_proposal.py` in that environment to regenerate the printable revised proposal.

[The submission checklist](submission/CHECKLIST.md) records the remaining course steps: repository URL, confirmed calendar deadline, hand-in, index card, and printed proposal. The GitHub repository hosts the source and the `lexer-compliance-1` release provides the verified ZIP. Course hand-in is a separate group action.

To integrate a parser, keep the normalized source buffer alive, call `cf_lex`, inspect `CfResult.tokens` only when the call succeeds, then call `cf_free`. `cf_token_name` returns a printable name for each enum value. Literal tokens keep source slices so a later phase can decode their values without losing the original spelling. Free a result before reusing it for a new scan.
