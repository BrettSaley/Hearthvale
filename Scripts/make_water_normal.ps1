# Writes SourceArt/Water/T_Water_Ripples_N.png: a tileable normal map of small water ripples.
# The height field is a sum of sine waves with whole-number frequencies across the tile (so it wraps
# seamlessly), and the normal comes from its slope. Made here, so no licence to worry about.
param([int]$Size = 512, [string]$Out = "$PSScriptRoot\..\SourceArt\Water\T_Water_Ripples_N.png")
Add-Type -AssemblyName System.Drawing

# (x frequency, y frequency, amplitude, phase): a few long swells and many short wavelets.
$rng = [Random]::new(7)
$waves = @()
foreach ($i in 1..24) {
    $fx = $rng.Next(-9, 10); $fy = $rng.Next(-9, 10)
    if ($fx -eq 0 -and $fy -eq 0) { continue }
    $len = [Math]::Sqrt($fx * $fx + $fy * $fy)
    $waves += , @($fx, $fy, (1.0 / $len), ($rng.NextDouble() * 2 * [Math]::PI))
}

$bmp = New-Object System.Drawing.Bitmap $Size, $Size
$strength = 0.9
for ($y = 0; $y -lt $Size; $y++) {
    for ($x = 0; $x -lt $Size; $x++) {
        $u = 2 * [Math]::PI * $x / $Size; $v = 2 * [Math]::PI * $y / $Size
        $dx = 0.0; $dy = 0.0
        foreach ($w in $waves) {
            $c = [Math]::Cos($w[0] * $u + $w[1] * $v + $w[3]) * $w[2]
            $dx += $w[0] * $c; $dy += $w[1] * $c
        }
        $nx = -$dx * $strength / $waves.Count; $ny = -$dy * $strength / $waves.Count; $nz = 1.0
        $l = [Math]::Sqrt($nx * $nx + $ny * $ny + $nz * $nz)
        $r = [int](($nx / $l * 0.5 + 0.5) * 255); $g = [int](($ny / $l * 0.5 + 0.5) * 255); $b = [int](($nz / $l * 0.5 + 0.5) * 255)
        $bmp.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $r, $g, $b))
    }
}
New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null
$bmp.Save([IO.Path]::GetFullPath($Out), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
"wrote $Out"
