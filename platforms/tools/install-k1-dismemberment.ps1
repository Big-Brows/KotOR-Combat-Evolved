param([Parameter(Mandatory=$true)][string]$Profile)
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$output=[IO.Path]::GetFullPath((Join-Path $repo 'platforms/out'))+[IO.Path]::DirectorySeparatorChar
$profilePath=[IO.Path]::GetFullPath($Profile)
if(!$profilePath.StartsWith($output,[StringComparison]::OrdinalIgnoreCase)){throw 'Only an ignored test profile may receive cut resources.'}
& git -C $repo check-ignore --quiet -- $profilePath
if($LASTEXITCODE -ne 0){throw 'Test profile is not ignored.'}
$part=Get-Item -LiteralPath $profilePath
while($part){if($part.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Redirected profile path.'};$part=$part.Parent}
$work=Join-Path $repo 'platforms/out/k1-engage-prototype/dismemberment'
$report=Get-Content -LiteralPath "$work/geometry-preparation.json" -Raw | ConvertFrom-Json
if(!$report.complete){throw 'Cut geometry incomplete.'}
$manifest=Get-Content -LiteralPath "$work/resume/runtime-manifest.json" -Raw | ConvertFrom-Json
$lock=[IO.File]::Open((Join-Path $profilePath 'engage-launch.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None)
try {
    $override=Join-Path $profilePath 'override'
    $rows=@();$index=0
    foreach($actor in $manifest.actors.PSObject.Properties){
        foreach($cut in $actor.Value.cuts.PSObject.Properties){
            $d=$cut.Value
            foreach($side in 'retained','detached'){
                $name=$d.$side;$hashes=$d.($side+'_sha256')
                foreach($extension in '.mdl','.mdx'){
                    $file=Join-Path $work "candidate/$name$extension"
                    if((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hashes.$extension){throw "Candidate identity mismatch: $name$extension"}
                    Copy-Item -LiteralPath $file -Destination (Join-Path $override "$name$extension")
                }
            }
            # User tuning: all supported parts/actors use nominal90%.
            # The native-compatible roll gate doubles this stored threshold.
            $probability=45
            $values=@($actor.Value.native_body, $(if($d.source_head){$d.source_head}else{'****'}),$d.anchor,$cut.Name,$d.retained,$d.detached,$d.model_slot)+@($d.local_origin)+@($d.local_axis)+@($probability)
            $strings=@($values | ForEach-Object {if($_ -is [double] -or $_ -is [float]){$_.ToString('R',[Globalization.CultureInfo]::InvariantCulture)}else{[string]$_}})
            $rows+=([string]$index+"`t"+($strings -join "`t"));$index++
        }
    }
    @('2DA V2.0','','body source anchor cut retained detached slot px py pz ax ay az probability')+$rows |
        Set-Content -LiteralPath (Join-Path $override 'k1cuts.2da') -Encoding ascii
    foreach($texture in 'jkcap.tga','jkcapgl.tga','jkcapgl.txi'){Copy-Item -LiteralPath (Join-Path $work "textures/$texture") -Destination (Join-Path $override $texture)}
    Write-Output "Installed $index cut pairs in ignored test output. Runtime remains gated by K1_ENGAGE_DISMEMBERMENT."
} finally {$lock.Dispose()}
