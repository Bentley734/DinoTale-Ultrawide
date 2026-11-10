# Build only the .nrm carrier. Its companion DLL is built separately.
param([string]$RecompModTool = "")

$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$output = Join-Path $root "build\standalone-mod"
New-Item -ItemType Directory -Force -Path $output | Out-Null
if (-not $RecompModTool) {
    $RecompModTool = Join-Path $root "lib\N64ModernRuntime\N64Recomp\build\RecompModTool.exe"
}
if (-not (Test-Path -LiteralPath $RecompModTool)) {
    throw "Build the RecompModTool target before packaging."
}
python (Join-Path $PSScriptRoot "make_bootstrap.py") (Join-Path $output "bootstrap.elf")
if ($LASTEXITCODE -ne 0) { throw "Creating bootstrap ELF failed." }
$nrm = Join-Path $output "DinoTale-32x9.nrm"
# RecompModTool cannot overwrite an existing .nrm when it renames its ZIP.
if (Test-Path -LiteralPath $nrm) { Remove-Item -LiteralPath $nrm -Force }
& $RecompModTool (Join-Path $PSScriptRoot "mod.toml") $output
if ($LASTEXITCODE -ne 0) { throw "Packaging the standalone mod failed." }
Write-Host "Packaged $nrm"
