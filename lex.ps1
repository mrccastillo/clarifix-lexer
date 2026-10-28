$ErrorActionPreference = 'Stop'

if ($args.Count -eq 0 -or $args[0] -in @('--help', '-h')) {
    Write-Output 'Usage: .\lex.ps1 NAME [analyzer options]'
    Write-Output 'Examples: .\lex.ps1 sample | .\lex.ps1 average | .\lex.ps1 braces --braces'
    Write-Output 'Use a name from examples, or a file path. The .clx extension is optional.'
    exit 0
}

$analyzer = Join-Path $PSScriptRoot 'bin\clarifix.exe'
if (-not (Test-Path -LiteralPath $analyzer -PathType Leaf)) {
    [Console]::Error.WriteLine('Build the analyzer first with .\build.ps1.')
    exit 2
}

$sourceName = [string]$args[0]
if (-not [IO.Path]::GetExtension($sourceName)) { $sourceName += '.clx' }
$candidates = @($sourceName)
if (-not [IO.Path]::IsPathRooted($sourceName)) {
    $candidates += Join-Path $PSScriptRoot $sourceName
    $candidates += Join-Path (Join-Path $PSScriptRoot 'examples') $sourceName
}
$sourcePath = $null
foreach ($candidate in $candidates) {
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        $sourcePath = (Resolve-Path -LiteralPath $candidate).Path
        break
    }
}
if (-not $sourcePath) {
    [Console]::Error.WriteLine("Cannot find '$sourceName'. Save it in examples or provide its path.")
    exit 2
}

$analyzerArgs = @()
if ($args.Count -gt 1) { $analyzerArgs = @($args[1..($args.Count - 1)]) }
& $analyzer @analyzerArgs $sourcePath
exit $LASTEXITCODE
