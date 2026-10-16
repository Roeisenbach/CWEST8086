# Driving the CGX engine

This folder is a self-contained CodeGuru Extreme (corewars8086 6.0.0) setup:
the official engine jar, a fast re-implementation of the simulator, a
tournament window, and two headless runners. Nothing here needs installing and
nothing phones home — every path below is relative to this folder.

Everything in this file was run on the engine as shipped here, not inferred
from the sources.

## Requirement

A JDK 17 or newer. `java -version` must answer. `javac` is needed only to
compile `tools/` or to edit `gui/` — both ship pre-compiled, so a plain JRE
runs everything as-is.

If `javac` is missing from PATH on Windows it is usually still installed:
`C:\Program Files\Java\jdk-*\bin\javac.exe`.

## The classpath

Every headless command uses the same two entries, in this order:

    engine                            the fast simulator (44 classes)
    lib/corewars8086-6.0.0.jar        the official engine

Windows separates them with `;`, Linux/macOS with `:`.

```bash
CP="engine:lib/corewars8086-6.0.0.jar"            # Linux / macOS
```
```powershell
$CP = "engine;lib\corewars8086-6.0.0.jar"          # Windows
```

Add `tools/build` to the classpath for `Duel` and `Mass`.

## How a field is laid out

The two field directories are **not** in git, because of the rule below. Make
them once after cloning:

```powershell
mkdir survivors, zombies
```

A *field directory* holds warrior files. Each file is the **raw binary** of one
warrior — no header, no extension needed, at most **512 bytes**.

**Every file in the directory is loaded as a warrior.** There is no extension
filter and no skipping of dotfiles: a `.gitkeep` in `survivors/` loads as a
0-byte warrior and shows up in the scores, and one in `zombies/` becomes a
third zombie. Keep these two folders holding nothing but warriors — no README,
no placeholder, no `.DS_Store`.

**Group naming is the thing to get right.** The group is the filename with its
trailing digits stripped, so two files named `TOM1` and `TOM2` are the two
warriors of one group `TOM`. `PR1`/`PR2` → group `PR`. A file named
`warrior.bin` is its own group `warrior.bin`. Groups are what score; warriors
inside a group are teammates and share a data segment.

Zombies live in a **separate** directory and are one unnamed group that never
scores. A zombie executes **two instructions per turn**; a survivor executes
one.

    survivors/        TOM1  TOM2  OTHER1  OTHER2      -> groups TOM, OTHER
    zombies/          zomA  zomB

The arena is one 64 KB segment filled with `0xCC`, warriors land at least 1024
bytes apart, and a battle runs 200,000 rounds unless everyone dies first.

## Running a tournament — `Tourney`

```powershell
java -cp $CP Tourney --warriors survivors --zombies zombies `
     --battles 200 --max-round 200000 --seed 1 --threads 6
```

Flags (these are the complete set the binary accepts):

| flag | meaning |
|---|---|
| `--warriors` / `-w` | survivors directory (**required**) |
| `--zombies` | zombies directory |
| `--battles` | battles per combo |
| `--totalBattles`, `--battles-per-combo` | the other two ways to say how many |
| `--combo` | how many groups fight at once |
| `--max-round` | rounds per battle (competition value: 200000) |
| `--seed` | base seed; battle *n* uses seed+*n* |
| `--pick-seed` | seed for choosing which groups meet |
| `--threads` | worker threads |
| `--zombie-speed` | instructions per turn for zombies (default 2) |
| `--out` | where to write the score file |
| `--quiet` | less chatter |

It prints a summary and writes `scores.csv`:

```
Tourney: 1 groups, combo 1, 20 battles, seed base 1, pick seed 20260731, threads 4, zombies 2 files
Writing scores to file scores.csv
20 battles in 0.02 s = 1231.56 battles/s
```

```
Groups:
TOM,20.0

Warriors:
TOM1,10.0
TOM2,10.0
```

Threads change speed only, never a result: `--threads 1` and `--threads 6`
produce identical scores.

## Tracing one battle — `Dump`

`Dump` runs **one group** and simulates the same battle twice — once on the
official jar, once on the fast engine — then writes both traces. Comparing them
is the correctness gate: if they diverge, the fast engine is wrong.

```powershell
java -cp $CP Dump --dir survivors --group TOM --max-round 2000 --seed 1 --out trace
```

| flag | meaning |
|---|---|
| `--dir` | field directory |
| `--group` | which group to run |
| `--max-round`, `--seed` | as above |
| `--full` | full per-instruction trace rather than digests |
| `--isover` | stop as soon as one group is left |
| `--out` | output prefix |

It writes `<out>.jar.txt` and `<out>.eng.txt`.

## Tracing a battle with several groups — `tools/Duel.java`

`Dump` only runs one group, which is useless once a warrior works and the
question becomes *what killed it*. `Duel` calls the runner directly with as
many groups as you name, in the same trace format.

```powershell
javac -cp $CP -d tools\build tools\Duel.java tools\Mass.java
java -cp "$CP;tools\build" Duel --dir survivors --zombies zombies `
     --out trace --max-round 200000 --seed 1 --full
```

`--groups A,B` picks specific groups (default: all of them). `--isover` ends
the battle when one group is left; without it the battle runs the full round
count, so a warrior that died at round 900 still shows the rest of the field
carrying on around its corpse.

## Counting survivors over many battles — `tools/Mass.java`

The reason `Duel` is slow at scale is that it writes the whole trace to disk —
100 MB for one long battle. `Mass` feeds the engine a sink that keeps only the
death lines, so the cost is the simulation and nothing else.

Positional arguments, in this order:

```powershell
java -cp "$CP;tools\build" Mass <fieldDir> <zombieDir> <rounds> <fromSeed> <toSeed> <threads>
java -cp "$CP;tools\build" Mass survivors zombies 200000 1 1000 16
```

It prints one line per battle that had a death, then a tally:

```
  seed 576: zomB@69
  seed 579: zomB@77
1000 battles at 200000 rounds; 987 with every warrior alive, 13 with a death.
```

This is the workhorse for "does my pair survive": 1000 full-length battles take
about three minutes on six cores.

## Reading a trace

Each line starts with a one-letter tag. The fields below are verified; the two
trailing numbers on `T` lines are run metadata I did not pin down.

**`W`** — a warrior loading, one per warrior at the start:

```
W 02 PR1   PR   37C1 00F9 S 1000 37C1 1000 2140 2180 0800 37C1
  |  |     |    |    |    | |    |    |    |    |    |
  |  name  grp  load size kind CS  IP   DS   ES   SS   SP
  index                   S=survivor Z=zombie
```

The **load offset** (`37C1`) is the one number everything else is measured
from: a warrior's own code computes its addresses from it.

**`T`** — one turn. Fields after the name are `FF`, an alive flag, then the
registers:

```
T 0000041B 00 zomA  FF 1 0FFD 4EA3 18FF 0100 0000 0000 4EDB E000 0000 4EA4 2040 0FFD 1000 0044 ...
  round    ix name     |  CS   IP   AX   BX   CX   DX   SP   BP   SI   DI   DS   ES   SS  flags
                       alive
```

**`M`** — one byte written: `M <warrior index> <linear address> <byte>`. This is
how you find out who bombed what:

```
M 00 1F227 A5      warrior 00 wrote A5 at linear 0x1F227
```

Linear address, not segment:offset — convert with `CS*16 + IP`.

**`D`** — a death: `D <round> <index> <name> <cause>`, e.g.
`D 0000004C 01 zomB memory_exception`. `H` lines are the header, `S` lines the
final scores.

A worked example of the loop: find the dying warrior's last `T` line, compute
`CS*16+IP` to get the linear address it died on, then grep the `M` lines for
that address to see who wrote the byte that killed it.

## The window

Double-click `RUN.bat` (Windows) or run `./run.sh` (Linux/macOS). Drop files
into `survivors/` and `zombies/`, press START.

`RUN.bat` finds a JDK by itself — `JAVA_HOME`, then PATH, then the usual
install locations. It recompiles `gui/` only when you have edited a source
there, and it **never** rebuilds `engine/`: those class files carry the
equivalence proof against the official jar and are meant to stay byte-for-byte
as shipped.

## Things that will bite

- **Trailing digits decide teammates.** `a1` and `a2` are one group. Rename a
  file and you may silently have merged or split a team.
- **Anything in the field directory is a warrior**, including dotfiles. A stray
  `.gitkeep` costs you a phantom group; a stray zombie file changes the fight.
  Check with `--battles 1`: the banner prints the group and zombie counts it
  actually loaded, and that number is the cheapest sanity check there is.
- **Zombies run at double speed.** Two instructions a turn. Any timing you tune
  against a survivor is off by 2x for a zombie.
- **`scores.csv` is overwritten** by every `Tourney` run in the working
  directory. Pass `--out` if you care.
- **`--threads` never changes a result.** If a run is not reproducible at a
  fixed seed, something else is wrong.
- **`Dump` is one group only.** Reach for `Duel` the moment a second group
  matters.

## Provenance

`lib/corewars8086-6.0.0.jar` is the official CodeGuru Extreme 6.0.0 engine
(`il.co.codeguru.corewars8086`), shipped here unmodified; it bundles
commons-math3, Guava and jline, whose licences are inside the jar under
`META-INF/`. `engine/` is a faster re-implementation that the G1/G2/G3 and
OpSweep gates certify as bit-identical to it. `tools/Duel.java` and
`tools/Mass.java` are headless runners added on top; they are not part of the
original package.
