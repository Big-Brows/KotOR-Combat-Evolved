# Apply the requested keyboard layout to this checkout's initialized test profile only.
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$profileDir = Join-Path $repo 'platforms/out/k1-test-user'
$ini = Join-Path $profileDir 'swkotor.ini'
$exe = Join-Path $repo 'platforms/out/k1-host-pc/windows/k1.exe'
if (!(Test-Path -LiteralPath $ini)) { throw 'Launch the isolated test profile once before applying this layout.' }
if (Get-Process -Name k1 -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe }) {
    throw 'Close this checkout''s running K1 before changing its bindings.'
}
$part = Get-Item -LiteralPath $ini
while ($part) {
    if ($part.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw 'Reparse point in test profile path.' }
    if ($part -is [IO.FileInfo]) { $part = $part.Directory } else { $part = $part.Parent }
}
$before = [IO.File]::ReadAllText($ini)
$saveAlias = (Join-Path $profileDir 'saves').Replace('\', '/')
if ($before -notmatch ('(?im)^SAVES=' + [regex]::Escape($saveAlias) + '\s*$')) {
    throw 'Expected isolated SAVES alias missing; review the profile before editing.'
}
# CExoInput key IDs: A=51, D=54, Z=76, C=53, Left Shift=24 (not SDL or DirectInput scancodes).
# Action281 is the movement left/right axis; Action284 is camera rotation.
# Action265 is K1's held walk modifier; release restores the normal movement speed.
# Action221 previously claimed Left Shift before Action265 could register. Swap it to B (52),
# the former walk key; Right Shift's Action222 remains the other action-bar modifier.
$bindings = [ordered]@{Action281A=51; Action281B=54; Action284A=76; Action284B=53; Action265=24; Action221=52}
$sections = [regex]::Matches($before, '(?ims)(^\[Keymapping\]\r?\n)(.*?)(?=^\[|\z)')
if ($sections.Count -ne 1) { throw 'Expected exactly one initialized Keymapping section.' }
$section = $sections[0]
$body = $section.Groups[2].Value
foreach ($key in $bindings.Keys) {
    $pattern = '(?im)^(' + $key + '=)[^\r\n]*'
    if ([regex]::Matches($body, $pattern).Count -ne 1) { throw "Expected exactly one $key binding." }
    $body = [regex]::Replace($body, $pattern, ('${1}' + $bindings[$key]))
}
$after = $before.Substring(0, $section.Index) + $section.Groups[1].Value + $body + $before.Substring($section.Index + $section.Length)
if ($after -eq $before) { Write-Output 'Requested layout already applied; no files changed.'; return }
$backup = Join-Path $profileDir ('keymapping-before-' + [guid]::NewGuid().ToString('N') + '.ini')
Copy-Item -LiteralPath $ini -Destination $backup
[IO.File]::WriteAllText($ini, $after, [Text.UTF8Encoding]::new($false))
if ([IO.File]::ReadAllText($ini) -cne $after) { throw 'Keymapping write verification failed; backup preserved.' }
Write-Output 'Applied A/D movement, Z/C camera rotation and Left Shift to walk to the isolated test profile.'
Write-Output ('Previous configuration preserved under platforms/out/k1-test-user/' + [IO.Path]::GetFileName($backup))
