# Builds the release package: release/CWEST8086/ and release/CWEST8086-<version>-win64.zip.
#
# Everything is compiled fresh, at -O2, into build/release/ -- the release never
# links the development objects in build/. The exe is a static, stripped GUI
# program: no console window, and no MinGW runtime DLLs to ship beside it.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File tools/make_release.ps1
#
# Needs MSYS2 UCRT64 g++/windres on PATH (or at C:\msys64\ucrt64\bin). The GPU
# engine (gpu-engine/engine.exe + shaders/compute.spv) and the CGX Java engine
# are copied as already built; rebuild those first if their sources changed.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root
if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
    $env:PATH = "C:\msys64\ucrt64\bin;$env:PATH"
}

# version from the resource script, so the zip and the exe always agree
$rc = Get-Content (Join-Path $root 'assets\app.rc') -Raw
if ($rc -notmatch 'VALUE "ProductVersion",\s*"(\d+)\.(\d+)\.(\d+)') { throw 'no ProductVersion in assets/app.rc' }
$version = "$($Matches[1]).$($Matches[2]).$($Matches[3])"
$name    = 'CWEST8086'
$objDir  = Join-Path $root 'build\release'
$outDir  = Join-Path $root "release\$name"
$zip     = Join-Path $root "release\$name-$version-win64.zip"
Write-Host "== $name $version"

# A copy running out of the release folder would be gutted by the clean-out
# below (its exe is locked, everything around it is not) -- refuse up front.
$running = Get-CimInstance Win32_Process |
    Where-Object { $_.ExecutablePath -and $_.ExecutablePath.StartsWith($outDir, [StringComparison]::OrdinalIgnoreCase) }
if ($running) {
    $running | ForEach-Object { Write-Host "  running: $($_.ExecutablePath) (pid $($_.ProcessId))" }
    throw "close the copy running from $outDir first"
}

# ---- compile -------------------------------------------------------------------
if (Test-Path $objDir) { Remove-Item -Recurse -Force $objDir }
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'build_objects.ps1') -Out $objDir
if ($LASTEXITCODE -ne 0) { throw 'object build failed' }

if (Test-Path $outDir) { Remove-Item -Recurse -Force $outDir }
New-Item -ItemType Directory $outDir | Out-Null

Write-Host "  linking $name.exe"
$objs = Get-ChildItem $objDir -Filter *.o | ForEach-Object { $_.FullName }
& g++ -O2 -s -std=c++17 src/main.cpp @objs `
    -Isrc -Ithird_party/imgui -Ithird_party/imgui/backends -Ithird_party/glfw/include `
    -Lthird_party/glfw/lib-mingw-w64 -lglfw3 -lopengl32 -lgdi32 -lcomdlg32 -lshell32 -lole32 -lws2_32 -lwinhttp -lbcrypt `
    -mwindows -static -o (Join-Path $outDir "$name.exe")
if ($LASTEXITCODE -ne 0) { throw 'link failed' }

Write-Host "  linking cwnode.exe"
& g++ -O2 -s -std=c++17 -static src/engine/cwnode.cpp `
    (Join-Path $objDir 'eng_cpu.o') (Join-Path $objDir 'eng_war.o') `
    (Join-Path $objDir 'eng_compete.o') (Join-Path $objDir 'eng_netpack.o') `
    (Join-Path $objDir 'eng_netlink.o') `
    -lws2_32 -lwinhttp -lbcrypt -o (Join-Path $outDir 'cwnode.exe')
if ($LASTEXITCODE -ne 0) { throw 'cwnode link failed' }

# ---- runtime files -------------------------------------------------------------
function Copy-Into([string]$src, [string]$dstDir) {
    if (-not (Test-Path $src)) { throw "missing $src" }
    if (-not (Test-Path $dstDir)) { New-Item -ItemType Directory $dstDir -Force | Out-Null }
    Copy-Item $src $dstDir -Recurse -Force
}

# fonts -- every one in the folder, since Options can pick any of them
Get-ChildItem assets\fonts -File | Where-Object { $_.Extension -in '.ttf', '.otf' } |
    ForEach-Object { Copy-Into $_.FullName "$outDir\fonts" }

# GPU lane: the host and its shader; the pipeline cache is per-driver and is
# rebuilt on the first run
Copy-Into gpu-engine\engine.exe          "$outDir\gpu-engine"
Copy-Into gpu-engine\shaders\compute.spv "$outDir\gpu-engine\shaders"

# Java lane: the CGX engine classes, the official jar, the lane driver (source
# and build) and the CGX tournament window with its launchers
$cgx = "$outDir\CGX-engine"
Copy-Into CGX-engine\engine                     $cgx
Copy-Into CGX-engine\lib                        $cgx
Copy-Into CGX-engine\gui                        $cgx
Copy-Into CGX-engine\lane\JavaLane.java         "$cgx\lane"
Copy-Into CGX-engine\lane\build\JavaLane.class  "$cgx\lane\build"
foreach ($f in 'README.md', 'RUN.bat', 'run.sh') { Copy-Into "CGX-engine\$f" $cgx }
New-Item -ItemType Directory "$cgx\survivors", "$cgx\zombies" -Force | Out-Null

# licences of what is linked in
Copy-Item third_party\imgui\LICENSE.txt (New-Item -ItemType Directory "$outDir\licenses" -Force).FullName
Rename-Item "$outDir\licenses\LICENSE.txt" 'imgui-LICENSE.txt'
Copy-Item third_party\glfw\LICENSE.md "$outDir\licenses\glfw-LICENSE.md"
# the fonts are all from Google Fonts, under the SIL Open Font License 1.1 --
# each family's own OFL.txt from github.com/google/fonts
Get-ChildItem assets\fonts\licenses -Filter *.txt | ForEach-Object { Copy-Into $_.FullName "$outDir\licenses\fonts" }

Copy-Item assets\README-release.txt "$outDir\README.txt"

# server mode's tunnel program (Cloudflare's, Apache-2.0), when it sits next to
# the dev build -- without it the release's server mode says where to get it.
# Apache-2.0 asks for its licence to go wherever the program goes, so the two
# ship together or not at all (cloudflared has no NOTICE file to add).
if (Test-Path cloudflared.exe) {
    if (-not (Test-Path third_party\cloudflared\LICENSE)) { throw 'cloudflared.exe without third_party\cloudflared\LICENSE' }
    Copy-Item cloudflared.exe $outDir
    Copy-Item third_party\cloudflared\LICENSE "$outDir\licenses\cloudflared-LICENSE.txt"
}

# ---- zip -------------------------------------------------------------------------
# bsdtar keeps the empty survivors/ and zombies/ folders, which Compress-Archive drops
if (Test-Path $zip) { Remove-Item -Force $zip }
& "$env:SystemRoot\System32\tar.exe" -a -c -f $zip -C (Join-Path $root 'release') $name
if ($LASTEXITCODE -ne 0) { throw 'zip failed' }

$size = (Get-ChildItem $outDir -Recurse -File | Measure-Object Length -Sum).Sum / 1MB
Write-Host ("== {0}  ({1:N1} MB unpacked, {2:N1} MB zipped)" -f $outDir, $size, ((Get-Item $zip).Length / 1MB))
