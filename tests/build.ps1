# Builds the engine test harnesses into build/tests/, against the -O2 engine
# objects tools/build_objects.ps1 makes in build/. Run from anywhere:
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests/build.ps1            # all of them
#   powershell -NoProfile -ExecutionPolicy Bypass -File tests/build.ps1 selftest  # just one
#
# The harnesses run from the project root: their default data paths
# (tests/data/..., CGX-engine/...) are relative to it. What each one checks is
# in the comment at its top; tests/ref/ holds the Java side of the
# cross-checks.

param([string[]]$Only)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) { $env:PATH = "C:\msys64\ucrt64\bin;$env:PATH" }

& powershell -NoProfile -ExecutionPolicy Bypass -File tools\build_objects.ps1 | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'object build failed' }

$out = Join-Path $root 'build\tests'
if (-not (Test-Path $out)) { New-Item -ItemType Directory $out | Out-Null }
$engine = Get-ChildItem build -Filter 'eng_*.o' | ForEach-Object { $_.FullName }

$failed = @()
foreach ($t in Get-ChildItem tests -Filter *.cpp) {
    if ($Only -and ($Only -notcontains $t.BaseName)) { continue }
    $exe = Join-Path $out "$($t.BaseName).exe"
    & g++ -O2 -std=c++17 -static -Isrc/engine $t.FullName @engine -lws2_32 -o $exe 2>&1 | Out-Host
    if ($LASTEXITCODE -eq 0) { Write-Host "  ok    $($t.BaseName)" }
    else { Write-Host "  FAIL  $($t.BaseName)"; $failed += $t.BaseName }
}
if ($failed) { Write-Host "failed: $($failed -join ', ')"; exit 1 }
