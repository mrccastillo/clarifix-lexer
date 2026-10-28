# ClariFix

A handwritten C11 lexical analyzer for ClariFix `.clx` files. It recognizes tokens, builds a symbol table, and reports lexical errors. Existing `.clf` files are also supported.

This project covers lexical analysis; parsing and program execution belong to later phases.

## Requirements

- GCC or Clang with C11 support.
- Python 3 for tests and batch report generation.

## Quick start

Run these commands from the project folder in PowerShell:

```powershell
.\build.ps1 -Test
.\build\clarifix.exe --report build\sample-report.txt examples\sample.clx
Get-Content build\sample-report.txt
```

The first command builds the analyzer and runs all 38 tests. The sample produces nine source-token rows and two symbols: `total` and `10`.

**Use a new output filename each time.** Existing files are protected against overwriting.

If the compiler cannot be found, provide its location:

```powershell
.\build.ps1 -Compiler C:\path\to\gcc.exe -Test
```

## Example code

Save your code in a `.clx` file:

```text
# Class score
set name to "Marc".
set score to 90.
show name.
show score.
```

Analyze it by replacing `examples\sample.clx` in the quick-start command with your file's path.

## Output options

| Option | Output |
| --- | --- |
| `--report FILE` | Four-section report: token list, unique symbols, invalid tokens, and category summary. |
| `--symbols FILE` | Symbol attributes as TSV. |
| `--table FILE` | Two-column token type and lexeme TSV. |
| `--tokens FILE` | Detailed tokens with locations and symbol IDs. Defaults to the terminal. |
| `--braces` | Use explicit `{ }` blocks instead of indentation. |

Diagnostics appear in the terminal. Exit codes are `0` for success, `1` for lexical errors, and `2` for usage or I/O failures. Output after a lexical error is partial.

## Included examples

| File | Purpose |
| --- | --- |
| `sample.clx` | Sample from the paper. |
| `average.clf` | Class-average program. |
| `features.clf` | Literals, operators, and noise words. |
| `braces.clf` | Explicit blocks; requires `--braces`. |
| `errors.clf` | Five intentional lexical errors. |
| `checklist.clf` | Coverage of the required token categories. |

Generate reports for all examples in a fresh directory:

```powershell
.\build.ps1 -Examples -OutputDirectory build\example-reports
```

## Language rules

- Identifiers are case-sensitive, start with an ASCII letter or underscore, and have a maximum length of 32 characters.
- Constants include integers, decimals, characters, strings, and Booleans. Arrays are recognized through their brackets and elements.
- Use straight quotes for literals. Supported escapes are `\n`, `\"`, `\'`, and `\\`.
- Comments use `#` or non-nesting `/* ... */`. The `//` operator means floor division.
- Default blocks use four-space indentation. Select `--braces` for free-field blocks.
- Noise words are `to`, `the`, `of`, and `then`. Simple statements end with a period.

Recognition uses character automata. Unique identifiers and constants are stored in a manual djb2 hash table with 211 buckets and separate chaining. Reports retain original spellings, converted constant values, and source locations.

## Direct compilation

On Linux/macOS, or without the PowerShell script:

```sh
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 main.c lexer.c -o clarifix
python3 tests/test_lexer.py ./clarifix
./clarifix --report sample-report.txt examples/sample.clx
```

Windows is the verified environment. For coursework hand-in, see the [submission checklist](submission/CHECKLIST.md). The [published release](https://github.com/mrccastillo/clarifix-lexer/releases/tag/paper-alignment-2) contains the original verified ZIP and paper PDFs.
