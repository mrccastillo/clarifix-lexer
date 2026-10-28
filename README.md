# ClariFix Lexical Analyzer

ClariFix reads `.clx` files and shows their tokens, symbols, and lexical errors. It does not execute programs.

## 1. Build

Open the project folder in VS Code and open a terminal. Install GCC or Clang first.

| Windows PowerShell | Mac / Linux |
| --- | --- |
| `.\build.ps1` | `./build` |

Build once. Rebuild only after changing the C source files.

## 2. Run

| Windows PowerShell | Mac / Linux |
| --- | --- |
| `.\lex.ps1 sample` | `./lex sample` |

The token table and any errors appear in the terminal. Look for `0 error(s)`.

Replace `sample` with `average`, `features`, or `checklist`. The `errors` example intentionally contains errors. To run the braces example, use `.\lex.ps1 braces --braces` on Windows or `./lex braces --braces` on Mac/Linux.

**On Marc's computer:** the shortcuts `build` and `lex sample` also work in a new PowerShell terminal. Other computers should use the commands above.

## 3. Write your own example

Create `examples/my-program.clx`, add the following, and save:

```text
set name to "Marc".
show name.
```

Run it with `.\lex.ps1 my-program` on Windows or `./lex my-program` on Mac/Linux. After editing a `.clx` file, save and run it again; no rebuild is needed.

## Save a report (optional)

| Windows PowerShell | Mac / Linux |
| --- | --- |
| `.\lex.ps1 sample --report bin/report.txt` | `./lex sample --report bin/report.txt` |

Open `bin/report.txt` in VS Code to see the tokens, symbol table, errors, and summary. Use a new filename or delete the old report before running the same command again. Reports are created only when requested.

## Run tests (optional)

Python 3 is required for tests.

| Windows PowerShell | Mac / Linux |
| --- | --- |
| `.\build.ps1 -Test` | `./build --test` |
