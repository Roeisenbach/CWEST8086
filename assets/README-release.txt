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
    Java        Java 17 or newer (JAVA_HOME, PATH or a standard install location).
    Busy (C++)  the C++ engine plus the graphics card together. Off until
                Options > Competition allows it -- it runs the machine hot.
                Needs a Vulkan-capable driver; the first run compiles the
                shader and caches it in gpu-engine\, which can take a minute.
  If one of them is missing the C++ engine plays the battles instead and the
  panel says why.

Workspace
---------
  Everything lives in a workspace: a folder you pick (Ctrl+O), with assembly\,
  survivors\ and zombies\ inside it, and folders of groups as directories in
  both. Pick an empty folder to start a new one. The last workspace reopens on
  the next start. Files added to it from outside show up on their own.

int 0x89 and the smart check
----------------------------
  Writing "int 0x89" in a survivor's source assembles to INT 87h (the smart
  bomb), so in every real war it behaves exactly like INT 87h with the AX:DX
  your code sets. What it adds is the Smart check button on the Competition
  toolbar: it plays the group against the active groups once for every AX:DX
  the bomb could look for, and ranks them by the share of the points the group
  takes. The candidates are every 4 bytes of every other warrior's code, plus
  every 4 bytes memory actually holds at the moment your int 0x89 runs in the
  first games -- code as the war has changed it, bombs, and partly-changed
  windows like "CC CC CC 5A". Double-click a row to copy its "mov ax / mov dx".

Playing a tournament together over the internet
-----------------------------------------------
  Server mode needs cloudflared.exe next to CWEST8086.exe -- Cloudflare's free
  tunnel program, no account needed (if it is missing, the Competition panel
  says where to get it: download cloudflared-windows-amd64.exe and rename it).
  Press Listen: the panel shows an https://....trycloudflare.com address. The
  others pick Join, paste it and press Connect -- from anywhere, campus Wi-Fi
  included, with no router or firewall setup. The address is new every time
  you start listening.
  Who may join: anyone (the default), whoever types your password (every
  message is then sealed with it, so the tunnel cannot read the tournament),
  or whoever you accept by hand ("ask me").

What is in this folder
----------------------
  CWEST8086.exe   the app
  cwnode.exe      headless tournament joiner for machines with no display:
                    cwnode <address> [--password p] [--name n] [--threads t] [--once]
  cloudflared.exe Cloudflare's tunnel program, for server mode (when included;
                  Apache License 2.0, in licenses\cloudflared-LICENSE.txt)
  fonts\          UI and code fonts (Google Fonts). Pick them in Options >
                  Fonts; any .ttf or .otf you drop in here is listed too.
  gpu-engine\     the graphics-card half of the Busy (C++) competition engine
  CGX-engine\     the CGX Java engine, used by the "Java" competition engine.
                  RUN.bat in there also opens CGX's own tournament window.
  licenses\       licences of the libraries built into the app and of
                  cloudflared.exe, and in licenses\fonts\ the SIL Open Font
                  License of each font

Command line
------------
  CWEST8086.exe [workspace folder | files to bring in] [options]
    --folder <dir>               open this workspace (instead of the last one)
    --serve [port]               host a tournament and run it straight away
    --join <address>             play a share of a tournament someone hosts
    --password <p>               the password for a protected host (or for hosting)
    --games <n>                  battles for the run (0 = until Stop)
    --threads <n>                worker threads (0 = every core)
    --engine c++|java|busy       which competition engine to use (busy only when allowed)
    --mode local|server|join
    --run                        press Run once the field is loaded
