CWEST8086 - Core Wars 8086: Editor, Simulator and Tournaments
=============================================================

Unzip anywhere and run CWEST8086.exe. Nothing is installed; the app keeps its
settings (cwest.ini) and window layout (corewars_layout.ini) next to the exe,
so put the folder somewhere you can write to (not Program Files).

Requirements
------------
  Windows 10 or 11, 64-bit.
  OpenGL 3 graphics (any current driver).

  Optional, for the extra competition engines (Competition tab > engine):
    GPU   a Vulkan-capable graphics driver. The first run compiles the shader
          and caches it in gpu-engine\, which can take a minute.
    Java  Java 17 or newer (JAVA_HOME, PATH or a standard install location).
  If one of them is missing the CPU engine plays the battles instead and the
  panel says why.

What is in this folder
----------------------
  CWEST8086.exe   the app
  cwnode.exe      headless tournament joiner for machines with no display:
                    cwnode <host[:port]> [--name n] [--threads t] [--once]
  fonts\          UI and code fonts (Google Fonts). Pick them in Options >
                  Fonts; any .ttf or .otf you drop in here is listed too.
  gpu-engine\     the GPU (Vulkan) competition engine
  CGX-engine\     the CGX Java engine, used by the "Java" competition engine.
                  RUN.bat in there also opens CGX's own tournament window.
  licenses\       licences of the libraries built into the app, and in
                  licenses\fonts\ the SIL Open Font License of each font

Command line
------------
  CWEST8086.exe [files or folders to open] [options]
    --folder <dir>               open a workspace folder
    --serve [port]               host a tournament and run it straight away
    --join host[:port]           play a share of a tournament someone hosts
    --games <n>                  battles for the run (0 = until Stop)
    --threads <n>                worker threads (0 = every core)
    --engine cpu|gpu|both|java   which competition engine(s) to use
    --mode local|offline|server|join
    --run                        press Run once the field is loaded
