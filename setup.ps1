param(
    [string]$ProfilePath = $PROFILE.CurrentUserAllHosts
)

$ErrorActionPreference = 'Stop'

$buildScript = Join-Path $PSScriptRoot 'build.ps1'
$lexScript = Join-Path $PSScriptRoot 'lex.ps1'
foreach ($scriptPath in @($buildScript, $lexScript)) {
    if (-not (Test-Path -LiteralPath $scriptPath -PathType Leaf)) {
        throw "Missing project script: $scriptPath"
    }
}

$startMarker = '# BEGIN ClariFix shortcuts'
$endMarker = '# END ClariFix shortcuts'
$shortcutBlock = @(
    $startMarker
    "Set-Alias -Name build -Value '$($buildScript.Replace("'", "''"))' -Scope Global"
    "Set-Alias -Name lex -Value '$($lexScript.Replace("'", "''"))' -Scope Global"
    $endMarker
) -join [Environment]::NewLine

$profileFile = [IO.Path]::GetFullPath($ProfilePath)
$profileDirectory = [IO.Path]::GetDirectoryName($profileFile)
[IO.Directory]::CreateDirectory($profileDirectory) | Out-Null
$profileText = ''
if (Test-Path -LiteralPath $profileFile -PathType Leaf) {
    $profileText = [IO.File]::ReadAllText($profileFile)
}

$blockPattern = '(?ms)^' + [regex]::Escape($startMarker) + '\r?$.*?^' + [regex]::Escape($endMarker) + '(?=\r?$)'
if ([regex]::IsMatch($profileText, $blockPattern)) {
    $profileText = [regex]::Replace($profileText, $blockPattern, [System.Text.RegularExpressions.MatchEvaluator]{ param($match) $shortcutBlock })
} else {
    if ($profileText.Length -gt 0 -and -not $profileText.EndsWith("`n")) {
        $profileText += [Environment]::NewLine
    }
    $profileText += $shortcutBlock + [Environment]::NewLine
}

[IO.File]::WriteAllText($profileFile, $profileText, [Text.UTF8Encoding]::new($true))
Set-Alias -Name build -Value $buildScript -Scope Global
Set-Alias -Name lex -Value $lexScript -Scope Global

Write-Host 'Shortcuts installed. Run: build'
Write-Host 'Then analyze an example: lex sample'
Write-Host 'Run setup again if you move the project folder.'
