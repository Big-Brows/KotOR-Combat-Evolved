param([switch]$Smoke)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$assets = 'C:\Program Files (x86)\Steam\steamapps\common\swkotor'
$userDir = Join-Path $repo 'platforms/out/k1-test-user'
$exe = Join-Path $repo 'platforms/out/k1-host-pc/windows/k1.exe'
$saveDir = Join-Path $userDir 'saves'
$ini = Join-Path $userDir 'swkotor.ini'
$logDir = Join-Path $repo 'platforms/out/baseline'
foreach ($path in @($exe, "$assets/chitin.key", "$assets/data", "$assets/modules", $userDir, $saveDir)) {
    if (!(Test-Path -LiteralPath $path)) { throw "Required path missing: $path" }
}
# Reject redirected test directories before writing configuration or launching.
$part = Get-Item -LiteralPath $saveDir
while ($part) {
    if ($part.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse point in test path: $($part.FullName)" }
    $part = $part.Parent
}
$saveAlias = $saveDir.Replace('\', '/')
if (!(Test-Path -LiteralPath $ini)) {
    @"
[Alias]
SAVES=$saveAlias
[Display Options]
FullScreen=0
"@ | Set-Content -LiteralPath $ini -Encoding ascii
}
$section = ''
$foundSaves = $false
foreach ($line in Get-Content -LiteralPath $ini) {
    if ($line -match '^\s*\[([^]]+)\]') { $section = $Matches[1]; continue }
    if ($section -ieq 'Alias' -and $line -match '^\s*([^;#=]+)=(.*)$') {
        if ($Matches[1].Trim() -ine 'SAVES' -or $Matches[2].Trim() -ine $saveAlias) {
            throw "Unexpected alias in isolated ini; review before launch: $line"
        }
        $foundSaves = $true
    }
}
if (!$foundSaves) { throw 'Isolated SAVES alias missing' }
$probe = Join-Path $userDir 'write-probe.tmp'
'preflight' | Set-Content -LiteralPath $probe
Remove-Item -LiteralPath $probe
Write-Output "Assets (read): $assets"
Write-Output "Configuration and output: $userDir"
Write-Output "SAVES alias: $saveAlias"
# Scope environment changes to this launch and restore the caller's environment.
$prior = @{}
Get-ChildItem Env: | Where-Object { $_.Name -match '^(K1_|K2_)' } | ForEach-Object { $prior[$_.Name] = $_.Value }
try {
    Get-ChildItem Env: | Where-Object { $_.Name -match '^(K1_|K2_)' } | ForEach-Object { Remove-Item -LiteralPath "Env:$($_.Name)" }
    $env:K2_ASSET_DIR = $assets
    $env:K2_USER_DIR = $userDir
    $env:K2_CLOUDSAVES = '0'
    $env:K2_WORKSHOP = '0'
    $env:K1_WINDOW = '1280x720'
    if ($Smoke) {
        $env:K2_HIDDEN = '1'
        $env:K2_SKIP_MOVIES = '1'
        $env:K2_INPUT = '300:quit:0'
        # Scripted quit uses _exit; bypass the asynchronous log tee so its final line persists.
        $env:K2_LOGFILE = '0'
    }
    $startArgs = @{FilePath=$exe; WorkingDirectory=$userDir; WindowStyle='Normal'; PassThru=$true}
    if ($Smoke) {
        $startArgs.WindowStyle = 'Hidden'
        $startArgs.RedirectStandardError = Join-Path $logDir 'smoke-stderr.log'
        $startArgs.RedirectStandardOutput = Join-Path $logDir 'smoke-stdout.log'
    }
    $game = Start-Process @startArgs
    # Windows PowerShell 5.1 needs the process handle retained to report ExitCode after waiting.
    $null = $game.Handle
    if ($Smoke -and !$game.WaitForExit(45000)) {
        Stop-Process -Id $game.Id
        throw 'Startup check timed out after 45 seconds; owned process stopped'
    }
    if (!$Smoke) { $game.WaitForExit() }
    $game.Refresh()
    Write-Output "Exit code: $($game.ExitCode)"
    if ($game.ExitCode -ne 0) { throw "K1 exited with code $($game.ExitCode)" }
} finally {
    Get-ChildItem Env: | Where-Object { $_.Name -match '^(K1_|K2_)' } | ForEach-Object { Remove-Item -LiteralPath "Env:$($_.Name)" }
    foreach ($name in $prior.Keys) { Set-Item -LiteralPath "Env:$name" -Value $prior[$name] }
}
