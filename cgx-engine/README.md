# CGX engine + GUI — portable

The fast engine and the tournament window, and nothing else. Everything is
relative to this folder, so it can live anywhere on any machine and be renamed.

## Moving it

Copy the folder. That is the whole procedure — nothing to install, nothing to
download, no absolute paths inside.

## Running it

**Windows** — double-click `RUN.bat`.
**Linux / macOS** — `./run.sh`.

Drop survivor files into `survivors/` and zombie files into `zombies/`, then
press START in the window.

## Requirement

A **JDK 17 or newer**. `java -version` should answer.

`RUN.bat` finds it by itself: `JAVA_HOME` first, then whatever is on `PATH`,
then the usual install locations (`C:\Program Files\Java\jdk*`, Adoptium,
Microsoft). Only if all of those fail does it ask you to set `JAVA_HOME`.

`javac` is needed **only** if you edit a file in `gui/`. The window is shipped
already compiled, so a plain JRE runs it as-is.

## What is in here

    RUN.bat / run.sh   the launcher
    engine/            44 compiled classes - the fast engine, ~4x the official
                       jar and proven bit-identical to it by the G1/G2/G3 and
                       OpSweep gates. Never rebuilt by the launcher.
    gui/               the window: 8 java sources + buildGui/ already compiled
    lib/               the official corewars8086 6.0.0 jar. The engine needs it
                       for commons-math3, which supplies the pairing draw.
    survivors/         put your warriors here
    zombies/           put zombies here

## Editing the window

Change any `.java` in `gui/` and just run `RUN.bat` again — it notices the file
is newer than the last build and recompiles that folder only. It never touches
`engine/`. If you DELETE or rename a source, delete `gui/buildGui/` once so the
orphaned class goes.

## Cores

The engine parallelises across battles and sizes itself from the machine:
physical cores, asked of the OS. Thread count is speed only and never changes a
result — `--threads 1` and `--threads 6` give identical rankings and identical
scores.

Measured on a 6-core machine, 8,000 battles on a 12-group field: 1 thread 20.2 s,
3 threads 8.6 s, 6 threads 6.9 s. Returns fall off well before the cores run
out, so a 16-core box is faster than a 6-core one but not by 2.7x.
