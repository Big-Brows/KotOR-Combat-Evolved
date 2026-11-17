param()
$ErrorActionPreference = 'Stop'
$repo = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$destination = Join-Path $repo 'platforms/out/k1-engage-prototype/dismemberment/textures'
& git -C $repo check-ignore --quiet -- $destination
if ($LASTEXITCODE -ne 0) { throw 'Dismemberment textures must remain ignored.' }
$archive = Join-Path ${env:ProgramFiles(x86)} 'Steam/steamapps/common/Jedi Academy/GameData/base/assets1.pk3'
if (!(Test-Path -LiteralPath $archive -PathType Leaf)) { throw 'Owned JKA assets1.pk3 required.' }
New-Item -ItemType Directory -Force $destination | Out-Null
Add-Type -AssemblyName System.IO.Compression.FileSystem
Add-Type -AssemblyName System.Drawing
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
try {
    $records = @()
    foreach ($pair in @(@('textures/common/caps.jpg','jkcap'), @('textures/common/caps_glow.jpg','jkcapgl'))) {
        $entry = $zip.GetEntry($pair[0])
        if (!$entry) { throw "Missing owned cap texture: $($pair[0])" }
        $source = Join-Path $destination ($pair[1] + '-source.jpg')
        $target = Join-Path $destination ($pair[1] + '.tga')
        [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$source,$true)
        # Decode without recoloring/resizing. Native K12 textures use TGA.
        $bitmap = [Drawing.Bitmap]::new($source)
        try {
            $writer = [IO.BinaryWriter]::new([IO.File]::Create($target))
            try {
                $writer.Write([byte[]]@(0,0,2,0,0,0,0,0,0,0,0,0))
                $writer.Write([uint16]$bitmap.Width); $writer.Write([uint16]$bitmap.Height)
                $writer.Write([byte]24); $writer.Write([byte]0)
                for ($y=$bitmap.Height-1; $y -ge 0; $y--) {
                    for ($x=0; $x -lt $bitmap.Width; $x++) {
                        $pixel=$bitmap.GetPixel($x,$y)
                        $writer.Write([byte]$pixel.B); $writer.Write([byte]$pixel.G); $writer.Write([byte]$pixel.R)
                    }
                }
            } finally { $writer.Dispose() }
            $records += @{entry=$entry.FullName;alias=($pair[1]+'.tga');width=$bitmap.Width;height=$bitmap.Height;
                sourceSha256=(Get-FileHash -LiteralPath $source -Algorithm SHA256).Hash.ToLowerInvariant();
                outputSha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLowerInvariant()}
        } finally { $bitmap.Dispose() }
    }
    @{archive='JKA base/assets1.pk3';shader='shaders/common.shader: models/players/stormtrooper/caps';
      files=$records;installed=$false} | ConvertTo-Json -Depth 5 |
        Set-Content -LiteralPath (Join-Path $destination 'provenance.json')
} finally { $zip.Dispose() }
"blending additive`r`n" | Set-Content -LiteralPath (Join-Path $destination 'jkcapgl.txi') -Encoding ascii
Write-Output 'Prepared original JKA cap textures in ignored candidate output; not installed.'
