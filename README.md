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

The terminal table shows only `TOKEN TYPE` and `LEXEME`, including source comments and noise words. Any errors and the analysis summary also appear in the terminal. Look for `0 error(s)`.

To save the two-column output as a text file, run `lex sample --table bin/token-table.txt`. Use a fresh filename for each export. The optional `--tokens FILE` export retains detailed positions and symbol IDs.

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

Open `bin/report.txt` in VS Code to see the tokens, symbol table, errors, and summary. Use a new filename or delete the old report before running the same command again. Reports are created only when requested.

## Run tests (optional)

Python 3 is required for tests.

| Windows PowerShell | Mac / Linux |
| --- | --- |
| `build -Test` | `build --test` |

## Run without shortcuts

If `build` or `lex` is not recognized, run setup and open a new terminal, or use the scripts directly:

| Windows PowerShell | Mac / Linux |
| --- | --- |
| `.\build.ps1` | `sh ./build` |
| `.\lex.ps1 sample` | `sh ./lex sample` |
