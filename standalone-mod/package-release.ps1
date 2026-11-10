param([string]$Version = "0.2.2")
$ErrorActionPreference = "Stop"
if ($Version -notmatch '^[0-9]+\.[0-9]+\.[0-9]+$') { throw "Invalid mod version." }
$root = Split-Path $PSScriptRoot -Parent
$built = Join-Path $root "build\standalone-mod"
$dist = [IO.Path]::GetFullPath((Join-Path $root "dist"))
$name = "DinoTale-32x9-v$Version-windows"
$stage = [IO.Path]::GetFullPath((Join-Path $dist $name))
if (-not $stage.StartsWith($dist + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Release folder must stay inside dist."
}
foreach ($file in @("DinoTale-32x9.nrm", "DinoTale32x9.dll")) {
    if (-not (Test-Path -LiteralPath (Join-Path $built $file))) { throw "Build the mod first: $file missing." }
}
if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
Copy-Item -LiteralPath (Join-Path $built "DinoTale-32x9.nrm"), (Join-Path $built "DinoTale32x9.dll") -Destination $stage
Copy-Item -LiteralPath (Join-Path $PSScriptRoot "README.md"), (Join-Path $root "COPYING") -Destination $stage
$zip = Join-Path $dist "$name.zip"
if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip -Force }
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash.ToLowerInvariant()
"$hash  $name.zip" | Set-Content -LiteralPath (Join-Path $dist "DinoTale-32x9-v$Version-SHA256SUMS.txt") -Encoding ascii
Write-Host "Packaged $zip"
