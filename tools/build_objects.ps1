# Compiles everything the app links except main.cpp into one folder of objects,
# incrementally: the engine (src/engine), Dear ImGui + its GLFW/OpenGL backends
# + the text editor (third_party/imgui), and the icon/version resource.
#
# The app itself is built at -g with no optimisation so that main.cpp stays
# quick to rebuild and easy to step through -- but the engine is an interpreter
# running hundreds of millions of instructions per competition, and at -O0 it is
# several times slower than it needs to be. So everything here is -O2, and only
# a unit whose source (or a header it depends on) is newer than its object is
# recompiled: a normal build of the app costs nothing here.
#
#   powershell -File tools/build_objects.ps1                 # -> build/
#   powershell -File tools/build_objects.ps1 -Out build/rel  # a separate, fresh set

param([string]$Out = 'build')

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
if (-not [System.IO.Path]::IsPathRooted($Out)) { $Out = Join-Path $root $Out }
if (-not (Test-Path $Out)) { New-Item -ItemType Directory $Out | Out-Null }

$imgui = Join-Path $root 'third_party\imgui'
$glfw  = Join-Path $root 'third_party\glfw\include'

# name of the object, source, include flags, headers that invalidate it
$engineHeaders = Get-ChildItem (Join-Path $root 'src\engine') -Filter *.h
$imguiHeaders  = Get-ChildItem $imgui -Filter *.h
$units = @()
# the engine translation units the app links -- test harnesses are not here
foreach ($u in 'cpu', 'war', 'replay', 'disasm', 'asm', 'listing', 'session', 'compete', 'netpack', 'netlink') {
    $units += ,@("eng_$u", (Join-Path $root "src\engine\$u.cpp"), @(), $engineHeaders)
}
$inc = @("-I$imgui", "-I$imgui\backends", "-I$glfw")
foreach ($u in 'imgui', 'imgui_draw', 'imgui_tables', 'imgui_widgets',
               'imgui_TextEditor', 'imgui_impl_opengl3') {
    # imgui_impl_opengl3.cpp is the locally modified copy at the imgui root
    $units += ,@($u, (Join-Path $imgui "$u.cpp"), $inc, $imguiHeaders)
}
$units += ,@('imgui_impl_glfw', (Join-Path $imgui 'backends\imgui_impl_glfw.cpp'), $inc, $imguiHeaders)

$built = 0
foreach ($unit in $units) {
    $name, $src, $flags, $deps = $unit
    $obj = Join-Path $Out "$name.o"
    if (-not (Test-Path $src)) { Write-Host "missing $src"; exit 1 }
    $stale = $true
    if (Test-Path $obj) {
        $t = (Get-Item $obj).LastWriteTimeUtc
        $stale = (Get-Item $src).LastWriteTimeUtc -gt $t
        foreach ($h in $deps) { if ($h.LastWriteTimeUtc -gt $t) { $stale = $true } }
    }
    if ($stale) {
        Write-Host "  $(Split-Path -Leaf $src) -> $name.o"
        & g++ -O2 -std=c++17 @flags -c $src -o $obj
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        $built++
    }
}

# The icon and version block. app.ico itself comes from tools/makeicon.py.
$rc  = Join-Path $root 'assets\app.rc'
$ico = Join-Path $root 'assets\app.ico'
$res = Join-Path $Out 'app_res.o'
if (-not (Test-Path $res) -or
    (Get-Item $rc).LastWriteTimeUtc -gt (Get-Item $res).LastWriteTimeUtc -or
    (Get-Item $ico).LastWriteTimeUtc -gt (Get-Item $res).LastWriteTimeUtc) {
    Write-Host "  app.rc -> app_res.o"
    & windres --include-dir (Join-Path $root 'assets') $rc -O coff -o $res
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $built++
}

if ($built -eq 0) { Write-Host "objects up to date" }
else { Write-Host "$built object(s) rebuilt in $Out" }
