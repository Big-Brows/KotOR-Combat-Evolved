param([Parameter(Mandatory=$true)][string]$Profile)
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$root = [IO.Path]::GetFullPath((Join-Path $repo 'platforms/out')) + [IO.Path]::DirectorySeparatorChar
$destination = [IO.Path]::GetFullPath((Join-Path $Profile 'jka-marks'))
if (!$destination.StartsWith($root,[StringComparison]::OrdinalIgnoreCase)) { throw 'JKA marks must remain in ignored project output.' }
& git -C $repo check-ignore --quiet -- $destination
if ($LASTEXITCODE -ne 0) { throw 'JKA mark output is not ignored.' }
$archive = Join-Path ${env:ProgramFiles(x86)} 'Steam/steamapps/common/Jedi Academy/GameData/base/assets1.pk3'
if (!(Test-Path -LiteralPath $archive -PathType Leaf)) { throw 'Owned JKA base/assets1.pk3 required for original saber marks.' }
function File-Sha256([string]$path) {
    $stream=[IO.File]::OpenRead($path); $sha=[Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','').ToLowerInvariant() }
    finally { $sha.Dispose(); $stream.Dispose() }
}
$audioArchive = Join-Path (Split-Path $archive) 'assets0.pk3'
$audioReady = Test-Path "$destination/audio-provenance.json"
if($audioReady) { $audioReady = ((Get-Content "$destination/audio-provenance.json" -Raw | ConvertFrom-Json).selection -eq 'single-contact-cue') }
$audioReady = $audioReady -and (Test-Path "$Profile/override/jkscorch1.wav")
if (!$audioReady) {
    if (!(Test-Path -LiteralPath $audioArchive -PathType Leaf)) { throw 'Owned JKA base/assets0.pk3 required for original saber contact sounds.' }
    New-Item -ItemType Directory -Force $destination,(Join-Path $Profile 'override') | Out-Null
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $audio = [IO.Compression.ZipFile]::OpenRead($audioArchive)
    try {
        $records=@()
        foreach($i in 1..1) {
            $entry=$audio.GetEntry("sound/weapons/saber/saberhitwall$i.mp3")
            if (!$entry) { throw "Original JKA saberhitwall$i sound missing." }
            # Preserve the MP3 bytes. Native WAV resources already sniff/decode
            # MP3 payloads; the short alias only supplies K12's resource name.
            $file=Join-Path $Profile "override/jkscorch$i.wav"
            & git -C $repo check-ignore --quiet -- $file
            if ($LASTEXITCODE -ne 0) { throw 'JKA contact sound destination is not ignored.' }
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$file,$true)
            $records+=@{ entry=$entry.FullName;alias="jkscorch$i.wav";sha256=(File-Sha256 $file) }
        }
        @{ source='JKA base/assets0.pk3';selection='single-contact-cue';archiveSha256=(File-Sha256 $audioArchive);files=$records } |
            ConvertTo-Json -Depth 5 | Set-Content "$destination/audio-provenance.json"
    } finally { $audio.Dispose() }
}
if ((Test-Path "$destination/rivet.tga") -and (Test-Path "$destination/glow.tga") -and (Test-Path "$destination/provenance.json")) { return }
New-Item -ItemType Directory -Force $destination | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.Drawing
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {
    $rivet = $zip.GetEntry('gfx/damage/rivetmark.tga')
    $glow = $zip.GetEntry('gfx/effects/saberdamageglow.jpg')
    if (!$rivet -or !$glow) { throw 'Original JKA mark textures missing.' }
    [IO.Compression.ZipFileExtensions]::ExtractToFile($rivet,"$destination/rivet.tga",$true)
    [IO.Compression.ZipFileExtensions]::ExtractToFile($glow,"$destination/glow-source.jpg",$true)
    # Decode the owned JPEG without color/shape editing; K12's image reader uses
    # uncompressed bottom-up TGA. No KotOR textures or authored substitute.
    $image = [Drawing.Bitmap]::new("$destination/glow-source.jpg")
    try {
        $stream = [IO.File]::Create("$destination/glow.tga")
        $writer = [IO.BinaryWriter]::new($stream)
        try {
            $writer.Write([byte[]]@(0,0,2,0,0,0,0,0,0,0,0,0))
            $writer.Write([uint16]$image.Width); $writer.Write([uint16]$image.Height)
            $writer.Write([byte]24); $writer.Write([byte]0)
            for($y=$image.Height-1;$y -ge 0;$y--) { for($x=0;$x -lt $image.Width;$x++) {
                $p=$image.GetPixel($x,$y)
                $writer.Write([byte]$p.B); $writer.Write([byte]$p.G); $writer.Write([byte]$p.R)
            }}
        } finally { $writer.Dispose() }
    } finally { $image.Dispose() }
    [ordered]@{ source='JKA base/assets1.pk3'; archiveSha256=(File-Sha256 $archive);
        rivetEntry=$rivet.FullName; glowEntry=$glow.FullName;
        files=@(Get-ChildItem $destination -File | ForEach-Object { @{ name=$_.Name;sha256=(File-Sha256 $_.FullName) } })
    } | ConvertTo-Json -Depth 5 | Set-Content "$destination/provenance.json"
} finally { $zip.Dispose() }
