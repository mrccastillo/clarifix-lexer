# ClariFix Lexical Analyzer

ClariFix reads your code, identifies tokens, and reports lexical errors. Save your programs with the `.clx` extension.

This is a lexical analyzer. Running a file shows its tokens and errors; executing the program is a later phase.

## Basic running

Open the **PPL folder** in VS Code, then select **Terminal > New Terminal**. GCC or Clang must be installed.

### 1. Build the analyzer

| System | Command |
| --- | --- |
| Windows (PowerShell) | `.\build.ps1` |
| Mac / Linux | `./build` |

Wait for the `Built` message. The executable is created in `bin/`.

### 2. Run an example

| System | Command |
| --- | --- |
| Windows | `.\bin\clarifix.exe examples\sample.clx` |
| Mac / Linux | `./bin/clarifix examples/sample.clx` |

Tokens and diagnostics appear directly in the terminal. Look for `0 error(s)` in the summary for a successful analysis.

### 3. Try another example

```powershell
.\bin\clarifix.exe examples\average.clx
```

On Mac/Linux, use `./bin/clarifix examples/average.clx`.

Build once, then run as many source files as you want. Rebuild when you change the C source files; editing a `.clx` file only requires saving it and running the analyzer again. The commands below use Windows PowerShell; on Mac/Linux use `./bin/clarifix` and forward slashes in paths.

## Create your own example

In VS Code, right-click the **examples** folder, select **New File**, and name it **my-program.clx**.

Write this code and save the file:

```text
set name to "Marc".
set age to 20.
show name.
show age.
```

Run it:

```powershell
.\bin\clarifix.exe examples\my-program.clx
```

Use straight quotes and end simple statements with a period. Default blocks use four-space indentation.

## Included examples

| File in `examples/` | Purpose |
| --- | --- |
| `sample.clx` | Short sample from the paper. |
| `average.clx` | Class-average program. |
| `features.clx` | Literals, operators, and noise words. |
| `checklist.clx` | Required token categories. |
| `errors.clx` | Five intentional lexical errors. |
| `braces.clx` | Explicit blocks; run with `--braces`. |

For the braces example:

```powershell
.\bin\clarifix.exe --braces examples\braces.clx
```

## Optional: save a Symbol Table report

Run these commands in order. The first creates the report; the second displays it:

```powershell
.\bin\clarifix.exe --report bin\sample-report.txt examples\sample.clx
Get-Content bin\sample-report.txt
```

The report contains the token list, unique symbols, invalid tokens, and category summary. Use a **new report filename each time** because existing files are protected against overwriting. Reports after lexical errors are partial.

## Optional: run the automated tests

Python 3 is required for tests:

```powershell
.\build.ps1 -Test
```

On Mac/Linux:

```sh
./build --test
```

The shell script automatically finds `cc`, `clang`, or `gcc`. To choose a compiler, use `CC=clang ./build`. Python 3 is only needed when running tests or generating reports.

All 38 tests should pass. The `errors.clx` example is expected to report five errors.

For more command options:

```powershell
.\bin\clarifix.exe --help
```
