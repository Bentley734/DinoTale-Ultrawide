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
if (-not (Test-Path -LiteralPath $ClangCl)) { throw "Pass -ClangCl with your clang-cl.exe path." }
$testDir = Join-Path $root "build\ultrawide-tests"
New-Item -ItemType Directory -Force -Path $testDir | Out-Null
foreach ($name in @("viewport_math", "bg_arena", "bg_arena_hooks")) {
    $source = Join-Path $root "tests\${name}_test.cpp"
    $exe = Join-Path $testDir "${name}_test.exe"
    $extra = @()
    if ($name -eq "bg_arena_hooks") {
        $extra = @((Join-Path $root "src\game\bg_arena.cpp"))
    }
    Push-Location $testDir
    try {
        & $ClangCl /nologo /std:c++20 /EHsc /W4 /WX /wd4201 /UNDEBUG "/I$root\include" "/I$root\lib\N64ModernRuntime\N64Recomp\include" "/Fe$exe" $source @extra
    }
    finally { Pop-Location }
    if ($LASTEXITCODE -ne 0) { throw "Compiling $name tests failed." }
    & $exe
    if ($LASTEXITCODE -ne 0) { throw "$name tests failed." }
    Write-Host "$name tests passed"
}
