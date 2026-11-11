#!/bin/sh
# CGX Tournament on Linux / macOS. Same folder layout as RUN.bat.
HERE=$(cd "$(dirname "$0")" && pwd)
JAR="$HERE/lib/corewars8086-6.0.0.jar"
CLASSES="$HERE/engine"
GUIB="$HERE/gui/buildGui"
[ -f "$GUIB/CgxGuiFrame.class" ] || {
  echo "building the window ..."
  mkdir -p "$GUIB"
  javac -nowarn -cp "$CLASSES:$JAR" -d "$GUIB" "$HERE"/gui/*.java || exit 1
}
exec java -Xmx2g -Dcgx.gui.dir="$HERE" -cp "$CLASSES:$GUIB:$JAR" CgxGuiFrame "$@"
