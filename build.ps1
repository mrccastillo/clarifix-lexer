param(
    [string]$Compiler = 'gcc',
    [switch]$Test,
    [switch]$Examples,
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    $compilerCommand = Get-Command $Compiler -ErrorAction SilentlyContinue
    if (-not $compilerCommand -and $Compiler -eq 'gcc') {
        foreach ($candidate in @('C:\msys64\ucrt64\bin\gcc.exe', 'C:\msys64\mingw64\bin\gcc.exe', 'C:\MinGW\bin\gcc.exe')) {
            if (Test-Path -LiteralPath $candidate) {
                $compilerCommand = Get-Command $candidate
                break
            }
        }
    }
    if (-not $compilerCommand) {
        throw 'A desktop C compiler is required. Put GCC on PATH or use .\build.ps1 -Compiler C:\path\to\gcc.exe (or clang.exe).'
    }
    New-Item -ItemType Directory -Path build -Force | Out-Null
    & $compilerCommand.Source -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 main.c lexer.c -o build/clarifix.exe
    if ($LASTEXITCODE -ne 0) { throw 'C compilation failed.' }
    Write-Host 'Built build\clarifix.exe'
    if ($Test) {
        python tests/test_lexer.py build/clarifix.exe
        if ($LASTEXITCODE -ne 0) { throw 'Lexer tests failed.' }
    }
    if ($Examples) {
        if (-not $OutputDirectory) { throw '-Examples requires a fresh -OutputDirectory.' }
        python tools/generate_samples.py --executable build/clarifix.exe --output $OutputDirectory
        if ($LASTEXITCODE -ne 0) { throw 'Example report generation failed.' }
    }
} finally {
    Pop-Location
}
