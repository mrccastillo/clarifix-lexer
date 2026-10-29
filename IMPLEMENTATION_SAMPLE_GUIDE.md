# VI. Implementation Sample — Writing Guide and Draft

This guide explains what to include in Sections 6.1–6.4 of the ClariFix paper and provides text you can adapt. Here, **SRC Overview** is interpreted as **source code overview**.

The descriptions below are based on the current repository. The sample report in 6.4.1 comes from the saved test fixture, not a new run. A new build could not be completed while preparing this guide because no C compiler was found. Generate the remaining reports and screenshots using the commands below before submitting the paper.

## 6.1 Implementation Overview

### What to put here

Explain the implementation language, how the analyzer receives input, its main processing steps, and what it produces. Include the command used to build and run it. Keep detailed token rules and automaton tables in their earlier sections.

### Suggested paper text

The ClariFix lexical analyzer is implemented in C using the C11 standard. It accepts a source file with the `.clx` extension and scans its contents to recognize keywords, identifiers, constants, operators, delimiters, comments, and noise words. Each recognized source lexeme is recorded with its classification and line and column location.

The implementation separates file handling from lexical analysis. The main program reads the source file, normalizes line endings, and calls the lexical analyzer. The analyzer processes the source through character-based recognition routines and records identifiers and constants in a symbol table. The symbol table uses a djb2 hash with 211 buckets and separate chaining to handle collisions. Repeated occurrences of the same identifier or constant refer to its existing symbol entry.

The analyzer maintains both a source-oriented report and a token stream intended for a future parser. Comments and permitted noise words remain visible in the source report. Comments are omitted from the parser token stream; permitted filler words are omitted, while `to` in an assignment is represented as an assignment token. In the default mode, indentation is represented using layout tokens such as `INDENT` and `DEDENT`. An optional brace mode supports explicit `{ }` blocks.

Lexical errors include a source location, an explanation of what happened, a reason, and a suggested correction. After an error, the analyzer skips the remainder of the affected line and continues, stopping when five errors have been reported. Reports containing errors are marked as partial.

The analyzer displays a token table in the terminal and can also save a report containing the token list, unique symbol table, invalid tokens, and category summary. Its current implementation performs lexical analysis; expression evaluation, program execution, and complete syntax and semantic analysis belong to later stages.

### Build and run commands

From the project folder in Windows PowerShell, with GCC or Clang installed:

```powershell
.\build.ps1
.\lex.ps1 sample
```

For Clang, use `.\build.ps1 -Compiler clang`. The build produces `bin/clarifix.exe`.

Processing flow:

```text
.clx source file
    -> Read file and normalize line endings
    -> Recognize and classify lexemes
    -> Record symbols and lexical errors
    -> Display token table / save requested report
```

## 6.2 SRC Overview

### What to put here

Describe the actual source files and their responsibilities. Include a short explanation of the central functions and data structures. You do not need to paste the complete source code into this section.

### Suggested paper text

The source code is organized into three main C files: `main.c`, `lexer.c`, and `lexer.h`. Supporting scripts build and run the analyzer, while example programs and automated tests demonstrate its behavior.

| File or directory | Purpose |
| --- | --- |
| `main.c` | Handles command-line options, checks the `.clx` extension, reads and normalizes source text, invokes analysis, writes requested outputs, and returns an exit status. |
| `lexer.c` | Implements lexeme recognition, indentation handling, symbol storage, lexical diagnostics, error recovery, and output formatting. |
| `lexer.h` | Declares shared token types, result structures, symbol structures, and public functions. |
| `build.ps1` / `build` | Compile the C source on Windows or macOS/Linux, respectively. |
| `lex.ps1` / `lex` | Run the analyzer using an example name or source path. |
| `examples/` | Contains valid demonstration programs and an intentionally invalid sample. |
| `tests/test_lexer.py` | Contains automated checks for token recognition, reports, symbols, and error handling. |

The main entry point for lexical analysis is `cf_lex()`. It receives the source text, its length, a block-mode flag, a diagnostic output stream, and a result structure. It initializes the analyzer, scans the input, records results, and returns whether lexical analysis succeeded.

| Function | Responsibility |
| --- | --- |
| `cf_lex()` | Coordinates scanning and fills the analysis result. |
| `word()` | Recognizes identifiers, keywords, Boolean literals, and noise words. |
| `number()` | Recognizes numeric literals. |
| `literal()` | Recognizes strings and character literals and checks supported escapes. |
| `symbol()` | Recognizes operators, punctuation, and comment openings. |
| `error_at()` / `recover()` | Report an error and skip the damaged line. |
| `cf_write_token_table()` | Produces the terminal token table. |
| `cf_write_report()` | Produces the four-section report used in the paper. |
| `cf_free()` | Releases memory allocated for analysis results. |

The shared structures include `CfToken`, which stores a token type, source offset, length, location, and symbol reference; `CfSymbol`, which stores a unique identifier or constant and its attributes; and `CfInvalid`, which records an invalid source segment and its error information. `CfResult` groups the token stream, source-report entries, symbol table, invalid records, and error count.

If a source-code screenshot is required, use a readable excerpt of `cf_lex()` or the structures in `lexer.h`, with a caption explaining its role. Choose a short excerpt that supports this explanation.

## 6.3 Sample Input

### What to put here

Include three samples: a simple valid program, a more detailed valid program, and an invalid program. State what each sample demonstrates and identify its source file. Preserve the exact code and indentation so the output locations match.

### Sample 1: Basic assignment and output statement

Source file: `examples/sample.clx`

```text
# avg calc
set total to 10.
show total.
```

This sample demonstrates a single-line comment, the keywords `set` and `show`, the identifier `total`, the noise word `to`, an integer constant, and statement terminators. The repeated identifier also demonstrates reuse of a symbol-table entry.

### Sample 2: Class average program

Source file: `examples/average.clx`

```text
# Class average calculator from the proposal.
function average(total, count):
    return total / count.
set total to 0.
set count to 0.
ask answer "How many scores? ".
set n = to_int(answer).
for i in range(0, n):
    ask entry "Enter score: ".
    set total = total + to_decimal(entry).
    set count = count + 1.
if count > 0 then:
    set avg = average(total, count).
    show "Average: " + to_text(avg).
else:
    show "No scores entered.".
```

This sample demonstrates function-related keywords, input statements, a loop, conditional blocks, arithmetic and relational operators, strings, parentheses, commas, and four-space indentation. Names such as `range`, `to_int`, `to_decimal`, and `to_text` are recognized as identifiers. The analyzer does not call these functions or calculate an average.

### Sample 3: Intentionally invalid source

Source file: `examples/errors.clx`

```text
# Each bad line demonstrates one lexical diagnostic.
set 1name = 2.
set this_identifier_is_longer_than_thirty_two_characters = 1.
show "An unsupported escape: \q".
set grade = 'AB'.
show @.
# Analysis stops after five errors; fix these before scanning further.
```

This sample demonstrates an invalid name beginning with a digit, an identifier exceeding 32 characters, an unsupported escape sequence, a character literal containing more than one character, and an unrecognized character. It also demonstrates the five-error limit and partial reporting.

## 6.4 Sample Output

### What to put here

Pair the output with the same sample numbers used in 6.3. Include readable report excerpts or screenshots and a short explanation of what each result demonstrates. Use the analyzer's token and symbol output; a displayed average would require program execution beyond the current implementation.

Generate the reports from the project folder after a successful build:

```powershell
.\lex.ps1 sample --report bin/implementation-sample-report.txt
.\lex.ps1 average --report bin/implementation-average-report.txt
.\lex.ps1 errors --report bin/implementation-errors-report.txt
```

The final command intentionally returns a lexical-error status. Report files are created only when requested, and existing files are protected against overwriting. If a report name already exists, choose a fresh filename.

### 6.4.1 Output for Sample 1

The following tables reproduce the content of `tests/fixtures/paper-sample.expected.txt`, reformatted as Markdown for the paper.

**[A] Token List — in order of appearance**

| No. | Line | Col | Lexeme | Token Type | Category |
| --- | --- | --- | --- | --- | --- |
| 1 | 1 | 1 | `# avg calc` | SINGLE_COMMENT | Comment |
| 2 | 2 | 1 | `set` | SET | Keyword |
| 3 | 2 | 5 | `total` | IDENTIFIER | Identifier |
| 4 | 2 | 11 | `to` | NOISE | Noise word |
| 5 | 2 | 14 | `10` | CONST_INT | Constant |
| 6 | 2 | 16 | `.` | STATEMENT_TERMINATOR | Delimiter |
| 7 | 3 | 1 | `show` | SHOW | Keyword |
| 8 | 3 | 6 | `total` | IDENTIFIER | Identifier |
| 9 | 3 | 11 | `.` | STATEMENT_TERMINATOR | Delimiter |

**[B] Symbol Table — unique identifiers and constants**

| ID | Lexeme | Token Type | Value | Length | First Seen |
| --- | --- | --- | --- | --- | --- |
| 1 | `total` | IDENTIFIER | - | 5 | 2:5 |
| 2 | `10` | CONST_INT | 10 | 2 | 2:14 |

**[C] Invalid / Unrecognized Tokens**

```text
(none)
```

**[D] Summary**

```text
Identifiers: 2 | Keywords: 2 | Noise words: 1 | Constants: 1
Delimiters: 2 | Comments: 1 | Operators: 0 | Invalid: 0 | Total tokens: 9
```

Suggested explanation: The report contains nine source lexeme occurrences and two unique symbols. Although `total` appears twice, it occupies only one symbol-table entry. The identifier's value is shown as `-` because the analyzer records its name without executing its assignment. Comments and noise words appear in the source report for documentation. No invalid lexemes are reported.

The report's source-token count can differ from the terminal's token-stream count: the parser-oriented stream omits comments, handles noise words, and includes generated layout and end-of-file tokens.

### 6.4.2 Output for Sample 2

Insert the actual output from `bin/implementation-average-report.txt`. For a long report, include a clearly labeled excerpt of the token list, the symbol table, the invalid-token section, and the summary. Show the terminal status in a screenshot if required.

Suggested explanation to use after confirming the run: The analyzer recognizes the function, loop, conditional, arithmetic, and input/output lexemes in the class average source. Repeated identifiers share symbol-table entries. The report records the source text and its lexical categories; it does not prompt for scores or compute an average.

Suggested caption: **Figure 6.2. Lexical analysis report for the class average sample.**

### 6.4.3 Output for Sample 3

Insert the actual diagnostic output and the invalid-token section from `bin/implementation-errors-report.txt`. The following is an expected-error guide based on the source implementation, not a captured terminal transcript:

| Source line | Invalid item | Expected error | Explanation |
| --- | --- | --- | --- |
| 2 | `1name` | E110 | A number is immediately followed by identifier characters. |
| 3 | `this_identifier_is_longer_than_thirty_two_characters` | E104 | Identifier exceeds the 32-character limit. |
| 4 | `\q` | E111 | Unsupported escape sequence in a literal. |
| 5 | `'AB'` | E112 | Character literal does not contain exactly one character. |
| 6 | `@` | E103 | Unrecognized character outside a literal or comment. |

Suggested explanation: Each diagnostic identifies the source location, explains the error, and suggests a correction. The analyzer recovers by skipping the remainder of the affected line. Upon reaching five errors, it stops scanning and asks the user to fix the reported issues before running again. Any token list or report generated for this input is partial.

Suggested caption: **Figure 6.3. Lexical diagnostics and partial report for intentionally invalid source.**

## Before submitting

- Keep the paper headings as 6.1 Implementation Overview, 6.2 SRC Overview, 6.3 Sample Input, and 6.4 Sample Output.
- Adapt the suggested paragraphs into your group's writing style and remove the guide instructions from the submitted paper.
- Build the current source and run all three samples.
- Replace the output instructions for Samples 2 and 3 with actual reports or readable screenshots.
- Match each output to the exact input shown, including comments and indentation.
- Label any shortened output as an excerpt and add figure or table captions.
- Confirm that the paper describes lexical analysis without claiming that the current analyzer executes ClariFix programs.
