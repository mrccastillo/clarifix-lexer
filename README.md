# ClariFix Lexical Analyzer

ClariFix reads your code, identifies tokens, and reports lexical errors. Use `.clx` files for new programs; existing `.clf` examples also work.

This is a lexical analyzer. Running a file shows its tokens and errors; executing the program is a later phase.

## Basic running

Open the **PPL folder** in VS Code, then select **Terminal > New Terminal**. Use PowerShell for the commands below.

### 1. Build the analyzer

```powershell
.\build.ps1
```

Wait for `Built build\clarifix.exe`. GCC or Clang must be installed.

### 2. Run an example

```powershell
.\build\clarifix.exe examples\sample.clx
```

Tokens and diagnostics appear directly in the terminal. Look for `0 error(s)` in the summary for a successful analysis.

### 3. Try another example

```powershell
.\build\clarifix.exe examples\average.clf
```

Build once, then run as many source files as you want. Rebuild when you change the C source files; editing a `.clx` file only requires saving it and running the analyzer again.

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
.\build\clarifix.exe examples\my-program.clx
```

Use straight quotes and end simple statements with a period. Default blocks use four-space indentation.

## Included examples

| File in `examples/` | Purpose |
| --- | --- |
| `sample.clx` | Short sample from the paper. |
| `average.clf` | Class-average program. |
| `features.clf` | Literals, operators, and noise words. |
| `checklist.clf` | Required token categories. |
| `errors.clf` | Five intentional lexical errors. |
| `braces.clf` | Explicit blocks; run with `--braces`. |

For the braces example:

```powershell
.\build\clarifix.exe --braces examples\braces.clf
```

## Optional: save a Symbol Table report

Run these commands in order. The first creates the report; the second displays it:

```powershell
.\build\clarifix.exe --report build\sample-report.txt examples\sample.clx
Get-Content build\sample-report.txt
```

The report contains the token list, unique symbols, invalid tokens, and category summary. Use a **new report filename each time** because existing files are protected against overwriting. Reports after lexical errors are partial.

## Optional: run the automated tests

Python 3 is required for tests:

```powershell
.\build.ps1 -Test
```

All 38 tests should pass. The `errors.clf` example is expected to report five errors.

For more command options:

```powershell
.\build\clarifix.exe --help
```
