param([string]$ClangCl = "")
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw "Visual Studio C++ build tools are required." }
if ($env:VSCMD_ARG_TGT_ARCH -ne "x64") {
    & (Join-Path $vsPath "Common7\Tools\Launch-VsDevShell.ps1") -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null
}
if (-not $ClangCl) { $ClangCl = Join-Path $vsPath "VC\Tools\Llvm\x64\bin\clang-cl.exe" }
$output = Join-Path $root "build\standalone-mod"
New-Item -ItemType Directory -Force -Path $output | Out-Null
Push-Location $output
try {
    & $ClangCl /nologo /std:c++20 /EHsc /W4 /WX /wd4201 /O2 /MT /LD /Brepro "/I$root\lib\N64ModernRuntime\N64Recomp\include" "/clang:-ffile-prefix-map=$root=." (Join-Path $PSScriptRoot "native\plugin.cpp") /FeDinoTale32x9.dll /link /INCREMENTAL:NO
    if ($LASTEXITCODE -ne 0) { throw "Building native mod failed." }
    & $ClangCl /nologo /std:c++20 /EHsc /W4 /WX /UNDEBUG /MT (Join-Path $PSScriptRoot "native\patch_plan_test.cpp") /FePatchPlanTest.exe
    if ($LASTEXITCODE -ne 0) { throw "Building patch-plan tests failed." }
    & .\PatchPlanTest.exe
    if ($LASTEXITCODE -ne 0) { throw "Patch-plan tests failed." }
    & $ClangCl /nologo /std:c++20 /EHsc /W4 /WX /UNDEBUG /MT (Join-Path $PSScriptRoot "native\tile_request_pool_test.cpp") /FeTileRequestPoolTest.exe
    if ($LASTEXITCODE -ne 0) { throw "Building tile-request regression test failed." }
    & .\TileRequestPoolTest.exe
    if ($LASTEXITCODE -ne 0) { throw "Tile-request regression test failed." }
    & $ClangCl /nologo /std:c++20 /EHsc /W4 /WX /UNDEBUG /MT (Join-Path $PSScriptRoot "native\visibility_test.cpp") /FeVisibilityTest.exe
    if ($LASTEXITCODE -ne 0) { throw "Building visibility tests failed." }
    & .\VisibilityTest.exe
    if ($LASTEXITCODE -ne 0) { throw "Visibility tests failed." }
    foreach ($test in @("background_repeat", "background_lookup", "background_cover", "camera_smoothing", "ground_collision", "ground_extension", "ground_runtime")) {
        & $ClangCl /nologo /std:c++20 /EHsc /W4 /WX /UNDEBUG /MT (Join-Path $PSScriptRoot "native\${test}_test.cpp") "/Fe${test}_test.exe"
        if ($LASTEXITCODE -ne 0) { throw "Building $test tests failed." }
        & ".\${test}_test.exe"
        if ($LASTEXITCODE -ne 0) { throw "$test tests failed." }
    }
    & $ClangCl /nologo /std:c++20 /EHsc /W4 /WX /MT (Join-Path $PSScriptRoot "native\verify_plugin.cpp") /FeVerifyPlugin.exe
    if ($LASTEXITCODE -ne 0) { throw "Building plugin verification tool failed." }
    & .\VerifyPlugin.exe --reject (Join-Path $output "DinoTale32x9.dll")
    if ($LASTEXITCODE -ne 0) { throw "Unsupported-host rejection test failed." }
    Write-Host "Native mod and patch-plan tests passed"
}
finally { Pop-Location }
