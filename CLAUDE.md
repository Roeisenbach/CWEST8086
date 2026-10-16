# CWEST8086

Two separate things live in this repository.

**The native CWEST tool** — `main.cpp`, the `gui_*.inl` panels, `app.exe` and
friends at the top level. Not documented here.

**`cgx-engine/`** — a self-contained CodeGuru Extreme (corewars8086 6.0.0)
setup: the official engine jar, a fast simulator that is bit-identical to it, a
tournament window, and headless runners for scoring and tracing battles.

If you are about to run battles, score warriors, or work out what killed one,
**read `cgx-engine/CLAUDE.md` first** — it has the verified command lines, the
field-directory and group-naming rules, and the trace format.

The short version, from inside `cgx-engine/`:

```powershell
$CP = "engine;lib\corewars8086-6.0.0.jar"
mkdir survivors, zombies          # not in git: every file in them loads as a warrior

# score a field
java -cp $CP Tourney --warriors survivors --zombies zombies --battles 200 --max-round 200000 --seed 1 --threads 6

# 1000 full-length battles, report only who died
javac -cp $CP -d tools\build tools\Duel.java tools\Mass.java
java -cp "$CP;tools\build" Mass survivors zombies 200000 1 1000 16
```

Warrior files are raw binaries of at most 512 bytes, dropped in
`cgx-engine/survivors/`. The group is the filename with trailing digits
stripped, so `TOM1` and `TOM2` are one team. Zombies go in
`cgx-engine/zombies/` and run at two instructions per turn. Every file in those
two folders is loaded as a warrior, dotfiles included, so keep them clean.

Requires a JDK 17 or newer. Nothing needs installing beyond that.
