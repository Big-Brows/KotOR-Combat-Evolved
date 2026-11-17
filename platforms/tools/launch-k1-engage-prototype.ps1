# Modified for KotOR:CE on 2026-11-16; see docs/LOCAL_CHANGES.md.
param([switch]$SaberSwitchProbe, [switch]$FontProbe, [switch]$Sandbox, [switch]$SandboxProbe, [ValidateSet("dark-jedi","dark-jedi-staff")][string]$NpcType="dark-jedi", [switch]$StaffForceContactProbe, [switch]$StaffArcProbe, [switch]$StaffContactProbe, [switch]$CursorProbe, [switch]$StaffPrototype, [switch]$StaffProbe, [switch]$Smoke, [switch]$DarkJediProbe, [switch]$CorpseProbe, [switch]$Probe, [switch]$Integration, [switch]$Arena, [switch]$ArenaProbe, [switch]$ControlProbe, [switch]$RigDiagnosis, [switch]$RigCandidate, [switch]$NativeGrip, [switch]$RigSourceOnly, [switch]$AnatomicalRig, [switch]$MissingRigProbe, [switch]$ElbowRig, [switch]$AimPitch, [switch]$EnemyAI, [switch]$Gunner, [switch]$NpcProbe, [switch]$Rifle, [switch]$NpcNavProbe, [switch]$NpcCrashProbe, [switch]$ForceTargetProbe, [switch]$UtilityProbe, [switch]$PoiseProbe, [switch]$PoiseRenderProbe, [switch]$PoiseMeterProbe, [switch]$PoiseRegenProbe, [switch]$ClashProbe, [switch]$ParryProbe, [switch]$Dismember, [switch]$DismemberModelProbe, [switch]$DismemberProbe, [switch]$DismemberContactProbe, [switch]$DismemberLethalProbe)
$ErrorActionPreference = 'Stop'
if ($SaberSwitchProbe) { $SandboxProbe=$true }
if ($FontProbe) { $SandboxProbe=$true }
if ($SandboxProbe) { $Sandbox=$true; $Smoke=$true }
if ($Sandbox) { $Arena=$true; $AnatomicalRig=$true; $ElbowRig=$true; $AimPitch=$true; $EnemyAI=$true; $Dismember=$true }
if ($NpcType -eq 'dark-jedi-staff') {
    $Arena=$true; $AnatomicalRig=$true; $ElbowRig=$true; $AimPitch=$true; $EnemyAI=$true; $Dismember=$true
    if ($Smoke -and !$Sandbox -and !$StaffContactProbe -and !$StaffArcProbe -and !$UtilityProbe -and !$PoiseProbe -and !$ParryProbe -and !$ClashProbe -and !$ForceTargetProbe) { $DarkJediProbe=$true }
}
if ($StaffForceContactProbe) { $StaffContactProbe=$true }
if ($CursorProbe) { $Smoke=$true }
if ($StaffArcProbe -or $StaffContactProbe) { $StaffPrototype=$true; $Smoke=$true }
if ($StaffProbe) { $StaffPrototype=$true; $Probe=$true; $Smoke=$true }
if ($StaffPrototype -and !$Probe) {
    $Arena=$true; $AnatomicalRig=$true; $ElbowRig=$true; $AimPitch=$true
    $EnemyAI=$true; $Dismember=$true
    if ($Smoke -and !$Sandbox -and !$StaffArcProbe -and !$StaffContactProbe -and !$ClashProbe -and !$ParryProbe -and !$PoiseProbe -and !$UtilityProbe -and !$ForceTargetProbe -and !$NpcNavProbe -and !$NpcCrashProbe -and !$CorpseProbe) { $DarkJediProbe=$true }
}

if ($DismemberLethalProbe) { $DismemberContactProbe=$true }
if ($DismemberContactProbe) { $DarkJediProbe=$true; $Dismember=$true }
if ($DismemberProbe) { $DarkJediProbe=$true; $Dismember=$true }
if ($Dismember -and (!$Arena -or !$AnatomicalRig -or !$ElbowRig)) { throw 'Dismember requires the isolated anatomical elbow arena.' }
if ($DismemberModelProbe -and (!$Smoke -or !$Probe -or !$NpcProbe -or !$RigCandidate -or !$ElbowRig)) { throw 'DismemberModelProbe requires its isolated NPC component smoke profile.' }
$UseDarkJedi = $EnemyAI -and !$Gunner -and ($StaffPrototype -or $Sandbox -or $NpcType -eq 'dark-jedi-staff' -or !$Smoke -or $DarkJediProbe)
if ($DarkJediProbe -and (!$Smoke -or !$Arena -or !$EnemyAI -or $Gunner -or $Rifle -or $ControlProbe -or $UtilityProbe -or $CorpseProbe -or $ForceTargetProbe -or $PoiseProbe -or $ClashProbe -or $ParryProbe -or $NpcNavProbe -or $NpcCrashProbe -or $Probe)) { throw 'DarkJediProbe requires its isolated melee EnemyAI arena smoke profile.' }
if ($CorpseProbe -and (!$Smoke -or !$Arena -or !$EnemyAI -or $Gunner -or $Rifle -or $ControlProbe -or $UtilityProbe -or $ForceTargetProbe -or $PoiseProbe -or $ClashProbe -or $ParryProbe -or $NpcNavProbe -or $NpcCrashProbe)) { throw 'CorpseProbe requires its isolated melee EnemyAI arena smoke profile.' }
$SurfaceMarkProbe = !$StaffArcProbe -and !$StaffContactProbe -and $Smoke -and $EnemyAI -and !$DarkJediProbe -and !$Gunner -and !$PoiseProbe -and !$ClashProbe -and !$ParryProbe -and !$ForceTargetProbe -and !$UtilityProbe -and !$NpcNavProbe -and !$NpcCrashProbe -and !$Probe
if ($PoiseRegenProbe -and (!$PoiseProbe -or $PoiseRenderProbe)) { throw 'PoiseRegenProbe requires the real PoiseProbe and cannot use capture-only modes.' }
if ($ClashProbe -and (!$Arena -or !$EnemyAI -or !$Smoke -or $Gunner -or $Rifle -or $PoiseProbe -or $ControlProbe -or $NpcNavProbe -or $UtilityProbe -or $ForceTargetProbe)) { throw 'ClashProbe requires the isolated melee EnemyAI arena smoke test without other runtime probes.' }
if ($ParryProbe -and (!$Arena -or !$EnemyAI -or !$Smoke -or $Gunner -or $Rifle -or $PoiseProbe -or $ControlProbe -or $NpcNavProbe -or $NpcCrashProbe -or $UtilityProbe -or $ForceTargetProbe -or $ClashProbe)) { throw 'ParryProbe requires the isolated melee EnemyAI arena smoke test without other runtime probes.' }
if ($PoiseMeterProbe -and !$PoiseRenderProbe) { throw 'PoiseMeterProbe requires PoiseRenderProbe and its isolated fixture.' }
if ($PoiseRenderProbe -and !$PoiseProbe) { throw 'PoiseRenderProbe requires the isolated PoiseProbe smoke fixture.' }
if ($PoiseProbe -and (!$Smoke -or !$EnemyAI -or $Gunner -or $ControlProbe -or $NpcNavProbe -or $NpcCrashProbe -or $ForceTargetProbe -or $UtilityProbe)) { throw 'PoiseProbe requires the isolated saber EnemyAI smoke profile.' }
if ($UtilityProbe -and (!$Smoke -or !$EnemyAI -or $Gunner -or $ControlProbe -or $NpcNavProbe -or $NpcCrashProbe -or $ForceTargetProbe)) { throw 'UtilityProbe requires its isolated saber EnemyAI smoke profile.' }
if ($ForceTargetProbe -and (!$Smoke -or !$EnemyAI -or $Gunner -or $NpcNavProbe -or $NpcCrashProbe)) { throw 'ForceTargetProbe requires the isolated saber EnemyAI smoke profile.' }
if ($NpcCrashProbe -and (!$Smoke -or !$EnemyAI -or $Gunner -or $NpcNavProbe)) { throw 'NpcCrashProbe requires the isolated saber EnemyAI smoke profile.' }
if ($NpcProbe -and (!$Smoke -or !$Probe -or !$RigCandidate -or !$ElbowRig -or $NativeGrip -or $RigSourceOnly)) { throw 'NpcProbe requires the isolated elbow component smoke profile.' }
if ($NpcNavProbe -and (!$Smoke -or !$EnemyAI -or $Gunner)) { throw 'NpcNavProbe requires the isolated saber EnemyAI smoke fixture.' }
if ($AimPitch -and !$ElbowRig) { throw 'AimPitch is limited to the isolated ElbowRig test profile.' }
if ($AnatomicalRig -and !$Arena) { throw 'AnatomicalRig is limited to the isolated arena.' }
if ($EnemyAI -and (!$Arena -or !$AnatomicalRig -or !$ElbowRig -or $ArenaProbe -or $ControlProbe)) { throw 'EnemyAI requires -Arena -AnatomicalRig -ElbowRig and uses its own isolated probe.' }
if ($Rifle -and !$Gunner) { throw 'Rifle requires the isolated Gunner fixture.' }
if ($Gunner -and !$EnemyAI) { throw 'Gunner requires the isolated EnemyAI arena.' }
if ($MissingRigProbe -and (!$Smoke -or !$Arena -or !$AnatomicalRig -or !$ControlProbe)) { throw 'MissingRigProbe requires the isolated anatomical arena smoke/control probe.' }
if ($RigSourceOnly -and (!$RigCandidate -or $NativeGrip)) { throw 'RigSourceOnly requires RigCandidate and cannot be combined with NativeGrip.' }
if ($NativeGrip -and !$RigCandidate) { throw 'NativeGrip requires the isolated RigCandidate probe.' }
if ($RigCandidate -and (!$Probe -or $Arena -or $Integration)) {
    throw 'RigCandidate is a component-only probe: use -Smoke -Probe -RigCandidate. It does not enable a runtime body swap.'
}
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$assets = Join-Path ${env:ProgramFiles(x86)} 'Steam/steamapps/common/swkotor'
$profile = Join-Path $repo 'platforms/out/k1-engage-prototype/profile'
if ($ElbowRig) {
    if (!(($Arena -and $AnatomicalRig) -or ($Probe -and $RigCandidate))) { throw 'ElbowRig requires the isolated anatomical arena or candidate component probe.' }
    $profile = Join-Path $repo 'platforms/out/k1-engage-prototype/elbow-arena/profile'
}
if ($StaffPrototype -or $CursorProbe) { $profile = Join-Path $repo 'platforms/out/k1-engage-prototype/elbow-arena/profile' }
$exe = Join-Path $repo 'platforms/out/k1-host-pc/windows/k1.exe'
$saves = Join-Path $profile 'saves'
if ($Dismember -or $DismemberModelProbe) {
    foreach ($resource in @('k1cuts.2da','jkcap.tga','jkcapgl.tga','jkcapgl.txi')) {
        if (!(Test-Path -LiteralPath (Join-Path $profile "override/$resource") -PathType Leaf)) {
            throw "Missing local dismemberment resource $resource; run the documented ignored-profile preparation/install first."
        }
    }
}
foreach ($required in @($exe, "$assets/chitin.key", "$assets/data", "$assets/modules",
                       "$profile/override/k1yellow.mdl", "$profile/yellow-expected.txt")) {
    if (!(Test-Path -LiteralPath $required)) { throw "Missing prototype dependency: $required. See STATUS.md." }
}
function Assert-LocalDirectory([string]$path) {
    $part = Get-Item -LiteralPath $path
    while ($part) {
        if ($part.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Redirected prototype path: $($part.FullName)" }
        $part = $part.Parent
    }
}
Assert-LocalDirectory $profile
if ($StaffPrototype) {
    $identityFile = Join-Path $repo 'platforms/out/k1-engage-prototype/staff-prototype/generation-report.json'
    if (!(Test-Path -LiteralPath $identityFile)) { throw 'Prepare the isolated staff assets first.' }
    $identity = Get-Content -LiteralPath $identityFile -Raw | ConvertFrom-Json
    foreach ($clip in @('s_top','s_tlbr','s_trbl','s_lr','s_rl','s_bltr','s_brtl',
                        'j_block_up','j_block_ur','j_block_ul','j_block_lr','j_block_ll')) {
        if (!$identity.clips.PSObject.Properties[$clip]) { throw "This staff build requires generated attack/block clip $clip; run its preparation recipe first." }
    }
    $hasher = [Security.Cryptography.SHA256]::Create()
    $assetStream = [IO.File]::OpenRead((Join-Path $profile 'override/k1jastaff.mdl'))
    try { $installedHash = [BitConverter]::ToString($hasher.ComputeHash($assetStream)).Replace('-', '') }
    finally { $assetStream.Dispose(); $hasher.Dispose() }
    if (!$identity.pass -or $installedHash -ine $identity.mdl_sha256) { throw 'Staff profile asset identity does not match its generation checks.' }
}
if (Get-ChildItem -LiteralPath $profile -Recurse -Force | Where-Object { $_.Attributes -band [IO.FileAttributes]::ReparsePoint }) {
    throw 'Redirected content inside the writable prototype profile.'
}
& (Join-Path $PSScriptRoot 'prepare-jka-saber-marks.ps1') -Profile $profile
if (!(Test-Path -LiteralPath $saves)) { New-Item -ItemType Directory -Path $saves | Out-Null }
Assert-LocalDirectory $saves
if (($Integration -or $Arena) -and !(Test-Path -LiteralPath (Join-Path $saves '000003 - Game2/SAVEGAME.sav'))) {
    throw 'Integration requires the documented isolated fixture save. See STATUS.md.'
}
$saveAlias = $saves.Replace('\', '/')
$ini = Join-Path $profile 'swkotor.ini'
if (!(Test-Path -LiteralPath $ini)) {
    @"
[Alias]
SAVES=$saveAlias
[Display Options]
FullScreen=0
"@ | Set-Content -LiteralPath $ini -Encoding ascii
}
$section = ''; $foundSaves = $false
foreach ($line in Get-Content -LiteralPath $ini) {
    if ($line -match '^\s*\[([^]]+)\]') { $section = $Matches[1]; continue }
    if ($section -ieq 'Alias' -and $line -match '^\s*([^;#=]+)=(.*)$') {
        if ($Matches[1].Trim() -ine 'SAVES' -or $Matches[2].Trim() -ine $saveAlias) {
            throw 'Unexpected prototype alias; review the isolated profile before launching.'
        }
        $foundSaves = $true
    }
}
if (!$foundSaves) { throw 'Isolated prototype SAVES alias missing.' }
$qol = Join-Path $profile 'k12.ini'
if (!(Test-Path -LiteralPath $qol)) { "[Gameplay]`r`nFasterMovement=0`r`n" | Set-Content -LiteralPath $qol -Encoding ascii }
Write-Output "Read-only assets: $assets"
if ($EnemyAI) {
    $npcAssets = if ($UseDarkJedi) { @('k1jadark.mdl','k1jadark.mdx','k1darktest.utc') } else { @('k1jasith.mdl','k1jasith.mdx') }
    if ($Sandbox -or $NpcType -eq 'dark-jedi-staff') { $npcAssets+=@('k1jadstaff.mdl','k1jadstaff.mdx','k1darkstaff.utc') }
    foreach ($asset in $npcAssets) {
        if (!(Test-Path -LiteralPath (Join-Path $profile "override/$asset") -PathType Leaf)) {
            throw "Missing generated opponent asset $asset in the isolated profile; NPC animation ownership cannot start."
        }
    }
}
Write-Output "Prototype output/configuration: $profile"
Write-Output "Verified SAVES alias: $saveAlias"
$prior = @{}
Get-ChildItem Env: | Where-Object { $_.Name -match '^(K1_|K2_)' } | ForEach-Object { $prior[$_.Name] = $_.Value }
$profileLock = $null
try {
    try {
        $profileLock = [IO.File]::Open((Join-Path $profile 'engage-launch.lock'),
            [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
    } catch {
        throw 'This prototype profile is already in use. Close its game and launcher before starting another run.'
    }
    Get-ChildItem Env: | Where-Object { $_.Name -match '^(K1_|K2_)' } | ForEach-Object { Remove-Item -LiteralPath "Env:$($_.Name)" }
    $env:K2_ASSET_DIR = $assets; $env:K2_USER_DIR = $profile
    $env:K2_CLOUDSAVES = '0'; $env:K2_WORKSHOP = '0'; $env:K2_SKIP_MOVIES = '1'; $env:K2_LOGFILE = '0'
    $env:K1_WINDOW = '1280x720'; $env:K1_ENGAGE_PROTOTYPE = '1'
    if ($CursorProbe) { $env:K1_CURSOR_ART_TEST='1' }
    if ($SurfaceMarkProbe) { $env:K1_ENGAGE_SURFACE_MARK_TEST = '1' }
    if ($Sandbox) { $env:K1_ENGAGE_SANDBOX='1' }
    if ($SaberSwitchProbe) { $env:K1_ENGAGE_SABER_SWITCH_TEST='1' }
    if ($FontProbe) { $env:K1_ENGAGE_FONT_TEST='1' }
    if ($SandboxProbe) { $env:K1_ENGAGE_SANDBOX_TEST='1'; $env:K2_CTL=Join-Path $profile 'sandbox-input.txt' }
    if ($StaffPrototype) { $env:K1_ENGAGE_STAFF_PROTOTYPE = '1' }
    if ($StaffArcProbe) { $env:K1_ENGAGE_STAFF_ARC_TEST='1'; $env:K2_CTL=Join-Path $profile 'staff-arc-input.txt'; Remove-Item -LiteralPath $env:K2_CTL -ErrorAction SilentlyContinue }
    if ($StaffForceContactProbe) { $env:K1_ENGAGE_FORCE_CONTACT_TEST='1' }
    if ($StaffContactProbe) { $env:K1_ENGAGE_STAFF_CONTACT_TEST='1'; $env:K2_CTL=Join-Path $profile 'staff-contact-input.txt'; Remove-Item -LiteralPath $env:K2_CTL -ErrorAction SilentlyContinue }
    if ($Probe) { $env:K1_ENGAGE_PROBE = '1' }
    if ($Dismember -or $DismemberModelProbe) { $env:K1_ENGAGE_DISMEMBERMENT = '1' }
    if ($DismemberModelProbe) { $env:K1_ENGAGE_DISMEMBER_MODEL_TEST = '1' }
    if ($DismemberContactProbe) { $env:K1_ENGAGE_DISMEMBER_CONTACT_TEST = '1' }
    if ($DismemberLethalProbe) { $env:K1_ENGAGE_DISMEMBER_LETHAL_TEST = '1' }
    if ($DismemberProbe) { $env:K1_ENGAGE_DISMEMBER_TEST = '1' }
    if ($RigDiagnosis) { $env:K1_ENGAGE_RIG_DIAG = '1' }
    if ($AimPitch) { $env:K1_ENGAGE_VIEW_PITCH = '1' }
    if ($RigCandidate) { $env:K1_ENGAGE_RIG_CANDIDATE = '1' }
    if ($NativeGrip) { $env:K1_ENGAGE_NATIVE_GRIP = '1' }
    if ($NpcProbe) { $env:K1_ENGAGE_NPC_PROBE = '1' }
    if ($RigSourceOnly) { $env:K1_ENGAGE_RIG_SOURCE_ONLY = '1' }
    if ($Integration) { $env:K1_ENGAGE_INTEGRATION = '1'; $env:K2_INPUT = '100:key:40;1800:quit:0' }
    if ($AnatomicalRig) { $env:K1_ENGAGE_ANATOMICAL_RIG = '1' }
    if ($MissingRigProbe) { $env:K1_ENGAGE_EXPECT_MISSING_RIG = '1' }
    if ($Arena) {
        if ($Integration) { throw 'Run the controlled fixture and arena checks separately.' }
        if (!(Test-Path -LiteralPath "$profile/modules/k1arena.mod")) { throw 'Missing isolated arena module.' }
        $env:K1_ENGAGE_ARENA = '1'; $env:K2_INPUT = '100:key:40'

        if ($EnemyAI) {
            $env:K1_ENGAGE_ENEMY_AI = '1'
            if ($UseDarkJedi) { $env:K1_ENGAGE_DARK_JEDI = '1'; $env:K1_ENGAGE_NPC_TYPE=$NpcType }
            if ($DarkJediProbe) { $env:K1_ENGAGE_DARK_JEDI_TEST = '1'; $env:K2_CTL = Join-Path $profile 'dark-jedi-input.txt' }
            if ($Gunner) { $env:K1_ENGAGE_ARENA_GUNNER = '1' }
            if ($Rifle) { $env:K1_ENGAGE_ARENA_RIFLE = '1' }
            if ($Smoke) { $env:K1_ENGAGE_ENEMY_AI_TEST = '1'; $env:K1_ENGAGE_SOUND_TEST = '1' }
            if ($ClashProbe) {
                $env:K1_ENGAGE_CLASH_TEST = '1'; $env:K2_CTL = Join-Path $profile 'clash-input.txt'
                if (Test-Path -LiteralPath $env:K2_CTL) { Remove-Item -LiteralPath $env:K2_CTL }
            }
            if ($ParryProbe) {
                $env:K1_ENGAGE_PARRY_TEST = '1'; $env:K2_CTL = Join-Path $profile 'parry-input.txt'
                if (Test-Path -LiteralPath $env:K2_CTL) { Remove-Item -LiteralPath $env:K2_CTL }
            }
            if ($ForceTargetProbe) { $env:K1_ENGAGE_FORCE_TARGET_TEST = '1' }
            if ($PoiseProbe) {
                if ($PoiseRegenProbe) { $env:K1_ENGAGE_POISE_REGEN_TEST = '1' }
                if ($PoiseRenderProbe) { $env:K1_ENGAGE_POISE_RENDER_TEST = '1' }
                if ($PoiseMeterProbe) { $env:K1_ENGAGE_POISE_METER_TEST = '1' }
                $env:K1_ENGAGE_POISE_TEST = '1'; $env:K2_CTL = Join-Path $profile 'poise-input.txt'
                if (Test-Path -LiteralPath $env:K2_CTL) { Remove-Item -LiteralPath $env:K2_CTL }
            }
            if ($CorpseProbe) { $env:K1_ENGAGE_CORPSE_TEST = '1' }
            if ($UtilityProbe) {
                $env:K1_ENGAGE_UTILITY_TEST = '1'; $env:K2_CTL = Join-Path $profile 'utility-input.txt'
                if (Test-Path -LiteralPath $env:K2_CTL) { Remove-Item -LiteralPath $env:K2_CTL }
            }
            if ($NpcNavProbe) { $env:K1_ENGAGE_NPC_NAV_TEST = '1' }
            if ($NpcCrashProbe) { $env:K1_JKA_TRACE_QUERY = '1'; $env:K1_JKA_DEFAULT_PROFILE_PROBE = '1' }
        }
        if ($ArenaProbe) { $env:K1_ENGAGE_ARENA_TEST = '1' }
        if ($ControlProbe) {
            $env:K1_ENGAGE_ARENA_TEST = '1'; $env:K1_ENGAGE_CONTROL_TEST = '1'
            if ($Smoke) { $env:K1_ENGAGE_SOUND_TEST = '1' }
            $env:K2_CTL = Join-Path $profile 'control-input.txt'
            if (Test-Path -LiteralPath $env:K2_CTL) { Remove-Item -LiteralPath $env:K2_CTL }
        }
        if ($Smoke) { $env:K2_INPUT += $(if($StaffForceContactProbe -or $SandboxProbe){';12000:quit:0'}elseif($DismemberProbe -or $DismemberLethalProbe){';7500:quit:0'}elseif($ForceTargetProbe -or $UtilityProbe){';6400:quit:0'}elseif($NpcCrashProbe){';4800:quit:0'}elseif($StaffPrototype){';3600:quit:0'}elseif($ControlProbe){';3800:quit:0'}elseif($EnemyAI -and !$NpcNavProbe){';4000:quit:0'}elseif($EnemyAI){';2400:quit:0'}else{';1200:quit:0'}) }
    }
    if ($SandboxProbe) { $env:K2_CTL=Join-Path $profile 'sandbox-input.txt'; Remove-Item -LiteralPath $env:K2_CTL -ErrorAction SilentlyContinue }
    $startArgs = @{FilePath=$exe; WorkingDirectory=$profile; WindowStyle='Normal'; PassThru=$true;
        RedirectStandardError=(Join-Path $profile 'engage-stderr.log'); RedirectStandardOutput=(Join-Path $profile 'engage-stdout.log')}
    if ($Smoke) {
        $env:K2_HIDDEN = '1'; if (!$Integration -and !$Arena) { $env:K2_INPUT = '300:quit:0' }
        $startArgs.WindowStyle = 'Hidden'
    }
    $game = Start-Process @startArgs; $null = $game.Handle
    # Source NPC pose/graph probes can render below60fps. Preserve the required
    # frame sequence and assertions; give it enough wall time to finish them.
    if ($Smoke -and !$game.WaitForExit($(if($ForceTargetProbe -or $UtilityProbe -or $StaffForceContactProbe -or $SandboxProbe){240000}elseif($StaffPrototype -and $Arena){120000}elseif($NpcCrashProbe){180000}elseif($EnemyAI){120000}elseif($ControlProbe){65000}else{45000}))) { Stop-Process -Id $game.Id; throw 'Prototype smoke timed out; owned process stopped.' }
    if (!$Smoke) { $game.WaitForExit() }
    $game.Refresh()
    if ($game.ExitCode -ne 0) { throw "Prototype exited with code $($game.ExitCode)." }
    if ($CursorProbe) {
        $cursorLog=Get-Content -LiteralPath $startArgs.RedirectStandardError -Raw
        if ($cursorLog -notmatch 'K1_CURSOR_ART_PASS tested=276 bad=0' -or $cursorLog -match 'K1_CURSOR_\w+_FAIL') { throw 'Native-art software cursor pixel/hotspot/visibility check failed; inspect isolated log.' }
        Write-Output 'PASS: original PC cursor pixels, hotspots, transparency and relative-look hiding; OBS visual check pending.'
    }
    if ($Probe) {
        $log = Get-Content -LiteralPath $startArgs.RedirectStandardError -Raw
        if ($StaffPrototype) {
            if ($log -notmatch 'K1_STAFF_COMPONENT_PASS' -or $log -match 'K1_STAFF_\w+_FAIL') { throw 'Staff animation components failed; inspect isolated log.' }
            Write-Output 'PASS: staff component checks; visual acceptance pending.'
            return
        }
        if ($DismemberModelProbe -and ($log -notmatch 'K1_DISMEMBER_REGIONS_PASS cases=138 chance_boundaries=7' -or [regex]::Matches($log,'K1_DISMEMBER_TERMINAL_PASS').Count -ne 288 -or $log -notmatch 'K1_DISMEMBER_DIRECTIONS_PASS cases=42' -or [regex]::Matches($log,'K1_DISMEMBER_EYES_PASS').Count -ne 16 -or $log -notmatch 'K1_DISMEMBER_MODELS_PASS models=48 native_death_samples=1440' -or $log -notmatch 'K1_DISMEMBER_PAYLOAD_PASS native_fields=22 copied=1 mismatched_target=1' -or $log -match 'K1_DISMEMBER_\w+_FAIL')) { throw 'Dismemberment native-model probe failed; see engage-stderr.log.' }
        if ($RigCandidate) {
            if ($NpcProbe) {
                if ($log -notmatch 'K1_NPC_RIG_SOURCE_PASS' -or $log -notmatch 'K1_NPC_PLAYBACK_PASS' -or $log -match 'K1_NPC_\w+_FAIL') { throw 'NPC source/playback component check failed.' }
                Write-Output 'PASS: NPC source/playback components; live combat and skin acceptance not established.'
            } elseif ($NativeGrip) {
                if ($log -notmatch 'K1_NATIVE_GRIP_PASS') { throw 'Native grip sampling failed.' }
                Write-Output 'PASS: isolated native grip reference capture.'
            } elseif ($RigSourceOnly) {
                if ($log -notmatch 'K1_RIG_SOURCE_PASS' -or $log -match 'K1_(RIG|ENGAGE)_\w+_FAIL') { throw 'Candidate rig source/bind/palette-only check failed.' }
                Write-Output 'PASS: candidate source/bind/palette only; connected and gameplay checks not run.'
            } else {
            if ($log -notmatch 'K1_RIG_BIND_PASS' -or $log -notmatch 'K1_RIG_PALETTE_PASS' -or $log -notmatch 'K1_RIG_MODEL_PASS' -or $log -notmatch 'K1_ENGAGE_CONNECTED_PASS' -or $log -match 'K1_(RIG|ENGAGE)_\w+_FAIL') { throw 'Candidate rig component/connected check failed.' }
            Write-Output 'PASS: candidate rig bind/palette/source-pose component checks.'
            }
        } else {
        if ($log -notmatch 'K1_ENGAGE_MODEL_PASS' -or $log -notmatch 'K1_ENGAGE_LAYER_PASS' -or $log -notmatch 'K1_ENGAGE_CONNECTED_PASS' -or $log -notmatch 'K1_ENGAGE_RESTORE_PASS' -or $log -match 'K1_ENGAGE_\w+_FAIL') { throw 'Prototype numeric probe failed; see engage-stderr.log.' }
        Write-Output 'PASS: yellow stance native engine pose probe.'
        }
    }
    if ($Integration) {
        $log = Get-Content -LiteralPath $startArgs.RedirectStandardError -Raw
        if ($log -notmatch 'K1_ENGAGE_INTEGRATION_PASS' -or $log -match 'K1_ENGAGE_\w+_FAIL') { throw 'Loaded-game integration did not pass; see engage-stderr.log.' }
        Write-Output 'PASS: isolated loaded-game combat integration.'
    }
    if ($Arena -and $Smoke) {
        $log = Get-Content -LiteralPath $startArgs.RedirectStandardError -Raw

        if ($StaffArcProbe) {
            if ($log -notmatch 'K1_STAFF_ARC_PASS trials=21' -or $log -match 'K1_STAFF_ARC_FAIL') { throw 'Native staff camera-space arc capture failed; inspect isolated log.' }
            Write-Output 'PASS: recorded21 complete native staff attacks, standing, Shift-walking and running, through the real camera.'
            return
        }
        if ($SandboxProbe) {
            if ($SaberSwitchProbe) {
                if ($log -notmatch 'K1_SABER_SWITCH_PASS switches=4 single=2 staff=2 outfit=1 head=1 native_restore=1' -or $log -match 'K1_SANDBOX_FAIL') { throw 'Saber switch/appearance probe failed; see isolated log.' }
                Write-Output 'PASS: four saber switches, matching stances, outfit/head attachments and native restoration.'
                return
            }
            if ($FontProbe) {
                if ($log -notmatch 'K1_FONT_ATLASES_PASS count=18' -or $log -notmatch 'K1_FONT_UI_PASS' -or $log -match 'K1_FONT_\w+_FAIL') { throw 'HD font load/layout probe failed; see isolated log.' }
                Write-Output 'PASS: all18 HD font resources load; native menu hit/layout and AI-off idle checks pass.'
                return
            }
            if ($log -notmatch 'K1_SABER_SWITCH_PASS switches=4 single=2 staff=2 outfit=1 head=1 native_restore=1' -or $log -notmatch 'K1_SANDBOX_PASS' -or $log -match 'K1_SANDBOX_FAIL') { throw 'Sandbox saber-switch/native UI/AI/combat probe failed; see isolated log.' }
            Write-Output 'PASS: sandbox native menu, spawn/factions/stance, idle AI toggle, NPC combat and cleanup.'
            return
        }
        if ($StaffContactProbe) {
            if ($log -notmatch 'K1_STAFF_CONTACT_PASS beams=2 repeated=1 unpowered=1 windup=1 recovery=1 native_damage=1 directions=3 extended_tail=1' -or
                $log -notmatch 'K1_MELEE_CONTACT_TIMING_PASS' -or $log -match 'K1_STAFF_\w+_FAIL') { throw 'Native staff contact/damage probe failed; inspect isolated log.' }
            if ($StaffForceContactProbe -and $log -notmatch 'K1_STAFF_FORCE_CONTACT_PASS effects=3 caster=1 native_hits=4 health_loss=1') { throw 'Staff contact during Force effects/casting did not pass; see engage-stderr.log.' }
            Write-Output 'PASS: both blades, per-blade guard poise, one health hit, staff chain/speed/recovery, timing and equipment eligibility.'
            return
        }
        if ($DismemberLethalProbe) {
            if ([regex]::Matches($log,'K1_VICTORY_CAMERA_TARGET .*follows_player=1').Count -ne 21 -or [regex]::Matches($log,'K1_VICTORY_CAMERA_MOVE_PASS').Count -ne 2 -or [regex]::Matches($log,'K1_VICTORY_CAMERA_TURN_PASS').Count -ne 2 -or [regex]::Matches($log,'K1_VICTORY_SWING_PASS').Count -ne 21 -or $log -notmatch 'K1_DISMEMBER_LETHAL_DONE kills=21 cuts=[1-9]' -or $log -notmatch 'K1_DISMEMBER_ROLL_ACCEPT' -or $log -match 'K1_DISMEMBER_LIVE_FAIL') { throw 'Real killing-contact region/chance test failed.' }
            Write-Output 'PASS: immediate native killing contact selects a local region and rolls once.'
            return
        }
        if ($DismemberContactProbe) {
            if ($log -notmatch 'K1_DISMEMBER_LIVE_DONE swings=21' -or $log -match 'K1_DISMEMBER_LIVE_FAIL') { throw 'Live contact diagnostic did not complete.' }
            Write-Output 'Recorded 21 production swings with original eligibility; inspect contact rejection reasons.'
            return
        }
        if ($DismemberProbe) {
            if ($log -notmatch 'K1_DISMEMBER_ARENA_PASS native_cut_deaths=17 retained=16 revived=1 unmarked_deaths=1 cleanup=1 parts=24' -or
                [regex]::Matches($log,'K1_DISMEMBER_NATIVE_DEATH_PASS').Count -ne 17 -or
                [regex]::Matches($log,'K1_DISMEMBER_MATERIAL_PASS').Count -ne 17 -or
                [regex]::Matches($log,'K1_DISMEMBER_FROZEN_PASS').Count -ne 16 -or
                [regex]::Matches($log,'K1_DISMEMBER_OUTWARD_PASS').Count -ne 16 -or
                [regex]::Matches($log,'K1_DISMEMBER_FLAT_PASS').Count -ne 16 -or
                [regex]::Matches($log,'K1_DISMEMBER_PLACEMENT_PASS').Count -ne 17 -or
                $log -notmatch 'K1_DISMEMBER_SURFACE_PASS' -or $log -notmatch 'K1_DISMEMBER_NONLETHAL_PASS' -or
                $log -notmatch 'K1_DISMEMBER_STUN_FIXTURE_PASS' -or $log -notmatch 'K1_DISMEMBER_REVIVAL_PASS' -or
                $log -notmatch 'K1_DISMEMBER_UNMARKED_PASS' -or $log -match 'K1_DISMEMBER_\w+_FAIL') { throw 'Dismemberment native-death arena probe failed.' }
            Write-Output 'PASS: posed surface selection, native deaths, textures, stun/revival, retained corpses and area cleanup; visual acceptance pending.'
            return
        }
        if ($DarkJediProbe) {
            if ($log -notmatch 'K1_DARK_JEDI_PASS rounds=2 native_victory=1' -or
                [regex]::Matches($log,'K1_NPC_HANDOFF_SEED_PASS').Count -ne 4 -or
                [regex]::Matches($log,"K1_NPC_TYPE_POLICY_PASS id=$NpcType ").Count -ne 2 -or
                $log -match 'K1_NPC_HANDOFF_SEED_FAIL|K1_NPC_TYPE_POLICY_FAIL' -or
                [regex]::Matches($log,'K1_DARK_JEDI_CAST_PASS').Count -ne 4 -or
                [regex]::Matches($log,'K1_NPC_FORCE_RECOVERY_PASS').Count -ne 4 -or
                [regex]::Matches($log,'K1_DARK_JEDI_PLANNING_PASS').Count -ne 2 -or
                $log -notmatch 'K1_DARK_JEDI_CAST_PASS .*spell=9 ' -or
                $log -notmatch 'K1_DARK_JEDI_CAST_PASS .*spell=23 ' -or $log -match 'K1_DARK_JEDI_FAIL') {
                throw 'Dark Jedi autonomous Force/melee/replacement probe failed; see engage-stderr.log.'
            }
            if ($NpcType -eq 'dark-jedi-staff' -and $log -notmatch 'K1_NPC_STAFF_CONTRACT_PASS') { throw 'Staff NPC contract did not pass.' }
            Write-Output 'PASS: Dark Jedi casts native Force powers, resumes JKA melee and repeats with native restoration.'
            return
        }
        if ($env:K1_ENGAGE_CORPSE_TEST) {
            if ($log -notmatch 'K1_ARENA_CORPSES_PASS retained=6' -or
                $log -notmatch 'K1_CORPSE_AREA_UNLOAD released=6 remaining=0 tracked=6' -or
                [regex]::Matches($log,'K1_CORPSE_UNLOAD_OBJECT id=\d+ found=1 remaining=0').Count -ne 6 -or
                $log -match 'K1_CORPSE_UNLOAD_OBJECT .*remaining=1' -or
                $log -notmatch 'K1_CORPSE_RELOAD_PASS old_bodies=6' -or
                $log -notmatch 'K1_CORPSE_RETURN_PASS count=0' -or $log -match 'K1_\w*CORPSE\w*_FAIL') {
                throw 'Isolated corpse retention/reload probe failed; see engage-stderr.log.'
            }
            Write-Output 'PASS: six corpses retained, unloaded and absent on arena return.'
            return
        }
        if ($log -notmatch 'K1_ENGAGE_ARENA_PASS' -or $log -match 'K1_ENGAGE_\w+_FAIL') { throw 'Arena initialization did not pass; see engage-stderr.log.' }
        Write-Output 'PASS: separate Taris arena loaded and initialized.'
        if ($SurfaceMarkProbe) {
            if ($log -notmatch 'K1_JKA_SABER_MARK_INSTALLED_PASS' -or
                $log -notmatch 'K1_JKA_SABER_MARK_SURFACES_PASS wall=1 prop=1' -or
                $log -notmatch 'K1_JKA_SABER_MARK_PROP_PASS .*attached=1 owner_cleared=1' -or
                $log -notmatch 'K1_JKA_SABER_MARK_GLOW_EXPIRED_PASS' -or
                $log -notmatch 'K1_JKA_SABER_MARK_SCORCH_EXPIRED_PASS' -or
                $log -notmatch 'K1_JKA_SABER_MARK_MISS_RESET_PASS fixture=0' -or
                $log -notmatch 'K1_JKA_SABER_MARK_WEAPON_PASS swords=4 sabers=3 unpowered=1 native_item_restored=1' -or
                $log -notmatch 'K1_JKA_SABER_MARK_CONTACT .*fixture=0' -or
                $log -match 'K1_JKA_SABER_MARK_\w+_FAIL') {
                throw 'JKA surface mark projection/live-contact/render-state check failed; see engage-stderr.log.'
            }
            $pattern='(?ms)^K1_JKA_SCORCH_SOUND_BEGIN actor=(?<actor>\d+) resource=(?<resource>jkscorch[123]) volume=(?<volume>\d+) now=(?<now>\d+) due=(?<due>\d+) fixture=(?<fixture>\d+)\r?\n(?<body>.*?)^K1_JKA_SCORCH_SOUND_(?<status>PASS|FAIL) actor=\k<actor> resource=\k<resource> length_ms=(?<length>-?\d+) next=(?<next>\d+)\r?$'
            $cues=[regex]::Matches($log,$pattern);$seen=@{};$next=@{}
            if (!$cues.Count -or $cues.Count -ne [regex]::Matches($log,'(?m)^K1_JKA_SCORCH_SOUND_BEGIN').Count) { throw 'Missing/unpaired surface scorch audio scopes.' }
            foreach($cue in $cues) {
                $g=$cue.Groups;$actor=$g['actor'].Value;$resource=$g['resource'].Value
                $now=[long]$g['now'].Value;$length=[int]$g['length'].Value
                $native=[regex]::Matches($g['body'].Value,'K1_COMBAT_AUDIO_PLAY resource=(\w+) result=(\d+) group=15 position=[^\r\n]* volume=20')
                if ($g['status'].Value -ne 'PASS' -or [int]$g['volume'].Value -ne 20 -or $length -le 0 -or
                    $now -lt [long]$g['due'].Value -or ($next.ContainsKey($actor) -and $now -lt $next[$actor]) -or
                    [long]$g['next'].Value -ne ($now+[Math]::Max([long]100000,[long]$length*1000)) -or
                    $native.Count -ne 1 -or $native[0].Groups[1].Value -ne $resource -or [int]$native[0].Groups[2].Value -ne $length) {
                    throw 'Surface audio must use a positive native spatial cue at volume20, with no per-actor overlap.'
                }
                $seen[$resource]=$true;$next[$actor]=[long]$g['next'].Value
            }
            if($seen.Count -ne 1 -or !$seen.ContainsKey('jkscorch1') -or
                $log -notmatch 'K1_JKA_SCORCH_PAYLOAD resource=jkscorch1 .*bytes=7283 cached=0') { throw 'The original JKA contact payload must be decoded by the native audio backend.' }
            Write-Output "PASS: quiet original JKA scorch audio cues=$($cues.Count), verified payload, native volume20/127 and non-overlapping cooldown."
            Write-Output 'PASS: original JKA surface marks, native floor projection/live blade contacts and isolated renderer state.'
        }
        if ($EnemyAI) {
            $graphCount = [regex]::Matches($log, 'K1_JKA_NAV_GRAPH_PASS').Count
            $preloadCount = [regex]::Matches($log, 'K1_JKA_NAV_PRELOAD_PASS').Count
            $expectedGraphs = if ($NpcCrashProbe) { 2 } else { 1 }
            if ($graphCount -ne $expectedGraphs -or $preloadCount -ne $expectedGraphs -or
                $log -notmatch 'K1_JKA_NAV_CACHE_HIT' -or
                $log -match 'K1_JKA_(NAV_\w+_FAIL|ADJ_QUERY)' -or
                $log.IndexOf('K1_JKA_NAV_PRELOAD_PASS') -gt $log.IndexOf('K1_ENGAGE_ARENA_PASS')) {
                throw 'Navigation must prepare once per loaded area before gameplay, reuse its cache, and avoid invalid adjacency.'
            }
            if ($NpcCrashProbe -and ($log -notmatch 'K1_JKA_BOUNDARY_REGRESSION_PASS' -or $log -notmatch 'K1_JKA_NAV_RELOAD_PASS')) {
                throw 'Boundary/default-profile/reload crash regression did not pass.'
            }
        }
        if ($ArenaProbe -and $log -notmatch 'K1_ENGAGE_ARENA_GUARD_PASS') { throw 'Arena live-opponent guard probe did not pass.' }
        if ($ControlProbe -and $log -notmatch 'K1_ENGAGE_CONTROL_PASS') { throw 'Grounded movement/input probe did not pass.' }
        if ($NpcNavProbe -and ($log -notmatch 'K1_JKA_NAV_SOURCE_PASS' -or $log -notmatch 'K1_JKA_JEDI_SOURCE_PASS' -or $log -notmatch 'K1_NPC_NAV_PASS' -or $log -notmatch 'K1_NPC_VICTORY_RESTORE_PASS')) { throw 'NPC source/moving-target/obstacle navigation probe failed.' }
        if ($Smoke -and ($EnemyAI -or $ControlProbe)) {
            $audioPattern = '(?ms)^K1_ENGAGE_SOUND_BEGIN kind=(?<kind>\w+) actor=(?<actor>\d+)\r?\n(?<body>.*?)^K1_ENGAGE_SOUND_END kind=\k<kind> actor=\k<actor> result=(?<result>-?\d+)\r?$'
            $audioBlocks = [regex]::Matches($log, $audioPattern)
            if ($audioBlocks.Count -ne [regex]::Matches($log,'(?m)^K1_ENGAGE_SOUND_BEGIN').Count) { throw 'Unpaired combat audio trace.' }
            $audioKinds = @{}
            foreach ($audioBlock in $audioBlocks) {
                $nativePlays = [regex]::Matches($audioBlock.Groups['body'].Value,'K1_COMBAT_AUDIO_PLAY resource=(\S+) result=(-?\d+)')
                if ($nativePlays.Count -ne 1 -or [int]$nativePlays[0].Groups[2].Value -le 0 -or [int]$audioBlock.Groups['result'].Value -le 0) {
                    throw 'Combat sound failed native resource/playback verification; see engage-stderr.log.'
                }
                $audioKinds[$audioBlock.Groups['kind'].Value] = $true
            }
            if ($ParryProbe) {
                foreach ($kind in @('swing','guard','parry','hit')) { if (!$audioKinds.ContainsKey($kind)) { throw "Missing real combat sound category: $kind" } }
                if ($log -notmatch 'K1_ENGAGE_SOUND_GATE_PASS') { throw 'Swing sound timing/deduplication check failed.' }
            }
            if ($ClashProbe -and (!$audioKinds.ContainsKey('swing') -or !$audioKinds.ContainsKey('clash'))) { throw 'Missing real reciprocal-clash sound.' }
            if ($ClashProbe -and $audioKinds.ContainsKey('hit')) { throw 'Reciprocal clash produced a false body-hit sound.' }
            Write-Output "PASS: native combat audio requests=$($audioBlocks.Count), categories=$($audioKinds.Keys -join ',')."
        }
        if ($PoiseProbe -and !$PoiseRenderProbe -and ($log -notmatch 'K1_POISE_PASS trials=4' -or $log -match 'K1_ENGAGE_POISE_FAIL')) { throw 'Melee poise/native stun probe failed.' }
        if ($PoiseRegenProbe -and ($log -notmatch 'K1_POISE_REGEN_CLOCK_PASS' -or $log -notmatch 'K1_POISE_REGEN_LIVE_PASS' -or $log -match 'K1_POISE_REGEN_FAIL')) { throw 'Poise regeneration timing/input/damage probe failed.' }
        if ($ClashProbe -and ($log -notmatch 'K1_MELEE_CLASH_LIVE_PASS trials=2' -or $log -match 'K1_ENGAGE_CLASH_FAIL')) { throw 'Real posed-swing clash test failed; see engage-stderr.log.' }
        if ($ParryProbe -and ($log -notmatch 'K1_DIRECTIONAL_PARRY_LIVE_PASS matched=7' -or $log -notmatch 'K1_PARRY_RULES_PASS' -or
            $log -notmatch 'K1_NPC_CUT_WEIGHTS_PASS' -or $log -notmatch 'K1_PARRY_NPC_COOLDOWN_PASS' -or
            $log -notmatch 'K1_PARRY_PLAYER_COOLDOWN_PASS' -or $log -notmatch 'K1_PARRY_AIM_AREA_PASS' -or $log -notmatch 'K1_PARRY_RENDER_PASS' -or
            $log -notmatch 'K1_BLOCK_REACTION_PLAYER_PASS trial=6' -or $log -notmatch 'K1_BLOCK_REACTION_NPC_PASS' -or
            $log -notmatch 'K1_BLOCK_REACTION_WALK_PASS' -or $log -match 'K1_BLOCK_REACTION_MISSING' -or
            $log -notmatch 'K1_PARRY_REWARD_PLAYER_PASS before=50 after=60' -or
            $log -notmatch 'K1_PARRY_REWARD_PLAYER_PASS before=95 after=100' -or $log -notmatch 'K1_PARRY_REWARD_NPC_PASS' -or
            $log -notmatch 'K1_NPC_PARRY_POLICY_PASS' -or $log -notmatch 'K1_NPC_PARRY_ZERO_CONTACT_PASS' -or
            $log -notmatch 'K1_PLAYER_SWING_RATE_PASS' -or $log -notmatch 'K1_NPC_SWING_RATE_PASS' -or
            $log -match 'K1_\w*PARRY\w*_FAIL')) { throw 'Directional parry/contact/reticle probe failed; see engage-stderr.log.' }
        if ($PoiseRenderProbe -and !$PoiseMeterProbe -and ($log -notmatch 'K1_POISE_RENDER_CAPTURE_PASS epochs=6' -or [regex]::Matches($log,'K1_POISE_RENDER_EPOCH').Count -ne 6)) { throw 'Poise preview on/off capture probe failed.' }
        if ($PoiseMeterProbe -and ($log -notmatch 'K1_POISE_METER_CAPTURE_PASS stages=10' -or $log -notmatch 'K1_POISE_RENDER_CAPTURE_PASS epochs=10' -or [regex]::Matches($log,'K1_POISE_METER_EQUIP_PASS').Count -ne 10 -or $log -match 'K1_ENGAGE_POISE_FAIL')) { throw 'Poise saber selection/depletion capture probe failed.' }
        if ($UtilityProbe -and ($log -notmatch 'K1_NATIVE_UTILITY_PASS trials=8' -or $log -match 'K1_NATIVE_UTILITY_FAIL' -or
            [regex]::Matches($log,'K1_NATIVE_UTILITY_RETURN_PASS').Count -ne 8 -or
            $log -notmatch 'K1_LAST_MEDKIT_MOVEMENT_PASS before_count=2' -or
            $log -notmatch 'K1_LAST_MEDKIT_MOVEMENT_PASS before_count=1' -or
            $log -match 'K1_LAST_MEDKIT_MOVEMENT_FAIL')) { throw 'Native planning/player/NPC utility animation and depletion movement probe failed.' }
        if ($EnemyAI -and !$NpcNavProbe -and !$UtilityProbe -and !$PoiseProbe -and !$ClashProbe -and !$ParryProbe) {
            if ($ForceTargetProbe) {
                if ($log -notmatch 'K1_PLAYER_NATIVE_EFFECT_PASS' -or $log -match 'K1_PLAYER_NATIVE_EFFECT_FAIL') { throw 'Player effect animation ownership did not return to KotOR.' }
                foreach ($spell in @(9,29,27)) {
                    if ($log -notmatch "K1_NPC_FORCE_REACTION_PASS spell=$spell .*native=1") { throw "Native target reaction missing for spell $spell." }
                }
                if ([regex]::Matches($log,'K1_NPC_FORCE_RECOVER_PASS').Count -ne 3) { throw 'Native target effects did not recover three times.' }
            }
            # The old bespoke melee_evade_swing branch was replaced by the
            # source Jedi commands. Source saber-evasion parity remains pending.
            $decision = if ($Gunner) { 'gunner_make_space' } else { 'jka_jedi_advance' }
            $ranged = [int][bool]$Gunner
            if ($log -notmatch 'K1_JKA_JEDI_SOURCE_PASS' -or $log -notmatch "K1_ENEMY_AI .*decision=$decision" -or
                ($Gunner -and ($log -notmatch 'K1_NPC_SHOT ' -or $log -notmatch 'K1_NPC_BOLT_IMPACT .*damage=1')) -or
                (!$Gunner -and ($log -notmatch 'K1_NPC_ATTACK ' -or $log -notmatch 'K1_NPC_CONTACT .*damage=1' -or $log -notmatch 'K1_NPC_RIG_READY .*engage=0' -or $log -notmatch 'K1_NPC_GUARD_PASS' -or $log -notmatch 'K1_NPC_FORCE_REACTION_PASS' -or $log -notmatch 'K1_NPC_FORCE_RECOVER_PASS')) -or
                $log -notmatch "K1_ENEMY_AI_FIXTURE ranged=$ranged" -or
                $log -notmatch "K1_ENEMY_AI_DAMAGE_PASS ranged=$ranged" -or
                $log -notmatch 'K1_ENEMY_AI_UNSEEN_PASS' -or
                $log -notmatch 'K1_NPC_PURSUIT_LIFETIME_PASS held=1' -or
                $log -notmatch "K1_ENEMY_AI_LIFECYCLE_PASS ranged=$ranged" -or
                $log -notmatch "K1_NPC_VICTORY_RESTORE_PASS ranged=$ranged" -or
                $log -notmatch 'K1_NPC_PLANNING_PASS' -or
                (!$NpcCrashProbe -and ($log -notmatch 'K1_ARENA_REPEAT_PASS replacements=2 final_restored=1' -or
                    $log -notmatch 'K1_ARENA_REPEAT_DAMAGE_PASS rounds=2')) -or
                $log -match 'K1_ARENA_CORPSES_FAIL' -or
                $log -match 'K1_ARENA_ROUND_CHECK_FAIL' -or
                (!$Gunner -and ($log -notmatch 'K1_NPC_UTILITY_NATIVE_PASS clip=' -or $log -notmatch 'K1_NPC_UTILITY_RESUME_PASS')) -or
                ($Rifle -and $log -notmatch 'K1_NPC_SHOT .*rifle=1') -or
                (!$Gunner -and ($log -notmatch 'K1_NPC_PARITY_PASS' -or $log -notmatch 'K1_NPC_ATTACK_WALK_PASS' -or $log -notmatch 'K1_NPC_WALK_BAND_PASS')) -or
                ($Gunner -and ($log -notmatch 'decision=gunner_find_shot' -or $log -notmatch 'K1_ENEMY_AI_LANE_PASS'))) {
                throw "Enemy AI arena probe missed a required decision. See engage-stderr.log."
            }
        }
    }
} finally {
    Get-ChildItem Env: | Where-Object { $_.Name -match '^(K1_|K2_)' } | ForEach-Object { Remove-Item -LiteralPath "Env:$($_.Name)" }
    foreach ($name in $prior.Keys) { Set-Item -LiteralPath "Env:$name" -Value $prior[$name] }
    if ($profileLock) { $profileLock.Dispose() }
}
