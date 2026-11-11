# CWEST8086

Core Wars 8086: an editor, simulator/debugger and tournament runner for
CodeGuru Extreme 8086 survivors. Dear ImGui + GLFW + OpenGL, Windows.

## Layout

```
src/
  main.cpp            the app
  gui/                the panels (#included by main.cpp)
  engine/             the 8086 war engine, assembler/disassembler, competition
                      runner, network packs, and cwnode.cpp (headless joiner)
assets/               fonts/ (Google Fonts; fonts/licenses/ has each OFL), app.ico +
                      app.rc (icon, version), README-release.txt
third_party/          imgui/ (docking branch, locally modified -- see
                      imgui-local-changes.patch), glfw/ (MinGW libs only)
gpu-engine/           the Vulkan competition engine: src/, shaders/, engine.exe
CGX-engine/           the CGX Java engine (the "Java" competition engine) and
                      its own tournament window; lane/ is the app's driver for it
tests/                engine test harnesses, ref/ (the Java reference and the
                      Java side of the cross-checks), data/ (warrior corpora)
tools/                build_objects.ps1, make_release.ps1, makeicon.py, shot.ps1
build/                objects and test binaries (generated)
release/              the packaged release (generated)
```

`app.exe`, `cwnode.exe`, `cwest.ini` and `corewars_layout.ini` in the root are
the development build and its saved settings; the app looks for `assets/`,
`gpu-engine/` and `CGX-engine/` beside itself.

## Building

MSYS2 UCRT64 (`C:\msys64\ucrt64\bin` on PATH for g++ and windres).

- **App (development):** VS Code task *Build ImGui App* (the default build).
  It runs `tools/build_objects.ps1` -- the engine, imgui and the icon resource,
  compiled once at -O2 into `build/` -- then compiles `src/main.cpp` at -g and
  links `app.exe` as a static GUI program (no console window, no MinGW DLLs).
- **cwnode:** task *Build cwnode*.
- **Release:** task *Make Release*, or
  `powershell -NoProfile -ExecutionPolicy Bypass -File tools/make_release.ps1`.
  Rebuilds everything from source at -O2 into `build/release/`, and writes
  `release/CWEST8086/` and `release/CWEST8086-<version>-win64.zip`. The version
  comes from `assets/app.rc`.
- **GPU engine:** open `gpu-engine/` in VS Code and run *Build-Vulkan* (needs
  the Vulkan SDK; it compiles the shader first).
- **Java lane driver:** the app compiles `CGX-engine/lane/JavaLane.java` itself
  when its class is missing or stale (needs a JDK); the release ships it built.

## Tests

`powershell -NoProfile -ExecutionPolicy Bypass -File tests/build.ps1` builds
every harness into `build/tests/` (pass names to build only some). Run them from
the project root. Each file's header says what it checks; `tests/ref/` has the
Java reference and `VALIDATION.txt`, the recorded C++-vs-Java results.
