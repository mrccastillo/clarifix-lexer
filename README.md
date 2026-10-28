# ClariFix Lexical Analyzer

ClariFix reads `.clx` source files, identifies tokens, builds a symbol table, and reports lexical errors. It analyzes the code; program execution is a later phase.

## 1. Build

Open the **PPL folder** in VS Code, then select **Terminal > New Terminal**. A C11 compiler such as GCC or Clang must be installed.

| System | Command |
| --- | --- |
| Windows PowerShell | `.\build.ps1` |
| Mac / Linux | `./build` |

Wait for the `Built` message. The executable is saved in `bin/`.

**PowerShell shortcuts:** on this computer, open a new terminal and type `build` to compile or `lex sample` to analyze an example. These shortcuts are saved in your local profile; other Windows users can use the project scripts directly.

## 2. Run an example

| System | Command |
| --- | --- |
| Windows | `.\lex.ps1 sample` |
| Mac / Linux | `./lex sample` |

Tokens and errors appear directly in the terminal. Look for `0 error(s)` in the summary for a successful analysis.

Pass the example name without the folder or extension: use `average` for `examples/average.clx`. Full file paths also work. To analyze another example, replace `sample` with its name. Build once, then run as many source files as you want. Rebuild after changing the C files; after editing a `.clx` file, save it and run the analyzer again.

## 3. Create your own example

In VS Code, right-click **examples**, select **New File**, and name it **my-program.clx**. Add this code and save:

```text
set name to "Marc".
set age to 20.
show name.
show age.
```

Then run it:

| System | Command |
| --- | --- |
| Windows | `.\lex.ps1 my-program` |
| Mac / Linux | `./lex my-program` |

Use straight quotes, end simple statements with a period, and use four spaces for indentation.

## Included examples

| File in `examples/` | Purpose |
| --- | --- |
| `sample.clx` | Short sample from the paper. |
| `average.clx` | Class-average program. |
| `features.clx` | Literals, operators, and noise words. |
| `checklist.clx` | Required token categories. |
| `errors.clx` | Five intentional lexical errors. |
| `braces.clx` | Explicit blocks; add `--braces` when running. |

For example, on Windows:

```powershell
.\lex.ps1 braces --braces
```

## Optional: save a Symbol Table report

A normal build or run does not save a report. Add `--report` when you want one, then open the file after it has been created.

Windows:

```powershell
.\lex.ps1 sample --report bin\sample-report.txt
Get-Content bin\sample-report.txt
```

Mac / Linux:

```sh
./lex sample --report bin/sample-report.txt
cat bin/sample-report.txt
```

The report contains four sections: token list, unique symbols, invalid tokens, and summary. The filename is your choice; `submission-symbol-table.txt` is not a required name.

Use a **new report filename each time**, or delete the previous report before reusing its name. Existing files are protected against overwriting. Reports after lexical errors are partial.

## Optional: run the tests

Python 3 is required for automated tests, but not for building the analyzer or saving a report with `--report`.

| System | Command |
| --- | --- |
| Windows | `.\build.ps1 -Test` |
| Mac / Linux | `./build --test` |

If your PowerShell shortcut is configured, `build -Test` also works. All 38 tests should pass.

## Generated files

- `bin/clarifix.exe` on Windows, or `bin/clarifix` on Mac/Linux, is created by building. Delete it whenever needed and rebuild to restore it.
- Report files are created when explicitly requested. You can delete them and generate new ones later. Batch reports can also be requested through `build.ps1 -Examples` with a fresh `-OutputDirectory`; that optional helper requires Python 3.
- Builds do not generate a release ZIP or a `paper-release` folder.

For all analyzer options, run `.\bin\clarifix.exe --help` on Windows or `./bin/clarifix --help` on Mac/Linux.
