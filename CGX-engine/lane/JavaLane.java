// =============================================================================
//  JavaLane -- the CGX Java engine as a competition lane for the CWEST8086 app.
//
//  The app runs this as a child process, the way it runs the GPU host, and
//  reads the standings back from a scores file in the same format. Nothing here
//  plays a war itself: every battle goes through Tourney.runBattle, the exact
//  class files in ../engine that carry the G1/G2/G3/OpSweep proof.
//
//  What this file does decide is WHICH battles are played, and that follows the
//  app's contract, not Tourney's: game g of the whole tournament is fought by
//  the groups cw::drawCombo(pairSeed, g) picks, on seed firstSeed + g, and a
//  lane plays g = phase + k * stride. Same draw, same seed, same order of
//  groups in the war -- so this lane plays the same battles the C++ lane would,
//  and its result adds to any other lane's without overlap.
//
//    java -cp "<engine>;<lane build>;<corewars8086 jar>" JavaLane
//         --field f.txt --scores s.txt --seed S --pair-seed P --combo C
//         --stride N --phase K --games G --rounds R --zspeed Z --threads T
//         [--parent PID]
//
//  --games 0 is open-ended: play until the process is killed. --parent makes
//  the lane exit on its own if the app that started it goes away.
//
//  Field file (written by the app):
//    cwest-java-field 1
//    g <label> <warriorCount>
//    w <name> <type 0 survivor / 1 zombie / 2 heavy zombie> <hex code>
//
//  Scores file (rewritten atomically a few times a second):
//    games <run> <failed> <seconds>
//    w <name> <score> <survived>
// =============================================================================

import java.io.*;
import java.nio.charset.StandardCharsets;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.atomic.AtomicInteger;

public final class JavaLane {

    // ---- cw::drawCombo, transcribed. Java long wraps like the C++ u64 and
    //      >>> is its shift, so the two agree bit for bit.
    static long mix64(long z) {
        z += 0x9E3779B97F4A7C15L;
        z = (z ^ (z >>> 30)) * 0xBF58476D1CE4E5B9L;
        z = (z ^ (z >>> 27)) * 0x94D049BB133111EBL;
        return z ^ (z >>> 31);
    }

    static void drawCombo(long pairSeed, int k, int fieldSize, int combo, int[] out) {
        if (combo > fieldSize) combo = fieldSize;
        Random rnd = new Random(mix64(pairSeed + (long) k * 0x9E3779B97F4A7C15L));
        for (int i = 0; i < fieldSize; i++) out[i] = i;
        for (int i = 0; i < combo; i++) {
            int j = i + rnd.nextInt(fieldSize - i);
            int t = out[i]; out[i] = out[j]; out[j] = t;
        }
    }

    // ---- the field
    static final List<EngGroup> field  = new ArrayList<>();   // drawn into battles
    static final List<EngGroup> always = new ArrayList<>();   // zombies: in every battle
    static final List<String>   scored = new ArrayList<>();   // warrior names, in slot order
    static final Map<String, Integer> slotOf = new HashMap<>();

    static byte[] unhex(String s) {
        byte[] b = new byte[s.length() / 2];
        for (int i = 0; i < b.length; i++)
            b[i] = (byte) Integer.parseInt(s.substring(2 * i, 2 * i + 2), 16);
        return b;
    }

    static void loadField(Path p) throws IOException {
        List<String> lines = Files.readAllLines(p, StandardCharsets.US_ASCII);
        int i = 0;
        if (lines.isEmpty() || !lines.get(i++).startsWith("cwest-java-field"))
            throw new IOException("not a field file: " + p);
        while (i < lines.size()) {
            String[] h = lines.get(i++).trim().split(" ");
            if (h.length < 3 || !h[0].equals("g")) continue;
            int n = Integer.parseInt(h[2]);
            String[] names = new String[n];
            byte[][] code = new byte[n][];
            int[] types = new int[n];
            boolean zombie = n > 0;
            for (int w = 0; w < n; w++) {
                String[] f = lines.get(i++).trim().split(" ");
                names[w] = f[1];
                types[w] = Integer.parseInt(f[2]);
                code[w]  = f.length > 3 ? unhex(f[3]) : new byte[0];
                if (types[w] == 0) {
                    zombie = false;
                    slotOf.put(names[w], scored.size());
                    scored.add(names[w]);
                }
            }
            EngGroup g = new EngGroup(h[1], names, code, types);
            (zombie ? always : field).add(g);
        }
    }

    // ---- the standings, as the lane's workers fold them in
    static final Object lock = new Object();
    static double[] score;
    static int[]    survived;
    static int      gamesRun, gamesFailed;
    static long     t0;

    static void writeScores(Path out) {
        StringBuilder sb = new StringBuilder();
        synchronized (lock) {
            double secs = (System.nanoTime() - t0) / 1e9;
            sb.append("games ").append(gamesRun).append(' ').append(gamesFailed)
              .append(' ').append(secs).append('\n');
            for (int i = 0; i < scored.size(); i++)
                sb.append("w ").append(scored.get(i)).append(' ').append(score[i])
                  .append(' ').append(survived[i]).append('\n');
        }
        // write and rename, so the reader sees one whole update or the last one
        try {
            Path tmp = out.resolveSibling(out.getFileName() + ".tmp");
            Files.write(tmp, sb.toString().getBytes(StandardCharsets.US_ASCII));
            Files.move(tmp, out, StandardCopyOption.REPLACE_EXISTING, StandardCopyOption.ATOMIC_MOVE);
        } catch (IOException e) {
            // the next update tries again
        }
    }

    public static void main(String[] args) throws Exception {
        Path fieldPath = null, scoresPath = null;
        long seed = 0, pairSeed = 0, parent = -1;
        int combo = 0, stride = 1, phase = 0, games = 0, rounds = 200000, zspeed = 2, threads = 0;
        for (int i = 0; i + 1 < args.length; i += 2) {
            String a = args[i], v = args[i + 1];
            switch (a) {
                case "--field":     fieldPath = Paths.get(v); break;
                case "--scores":    scoresPath = Paths.get(v); break;
                case "--seed":      seed = Long.parseLong(v); break;
                case "--pair-seed": pairSeed = Long.parseLong(v); break;
                case "--combo":     combo = Integer.parseInt(v); break;
                case "--stride":    stride = Math.max(1, Integer.parseInt(v)); break;
                case "--phase":     phase = Integer.parseInt(v); break;
                case "--games":     games = Integer.parseInt(v); break;
                case "--rounds":    rounds = Integer.parseInt(v); break;
                case "--zspeed":    zspeed = Integer.parseInt(v); break;
                case "--threads":   threads = Integer.parseInt(v); break;
                case "--parent":    parent = Long.parseLong(v); break;
                default: System.err.println("unknown option " + a); System.exit(2);
            }
        }
        if (fieldPath == null || scoresPath == null) {
            System.err.println("need --field and --scores");
            System.exit(2);
        }

        loadField(fieldPath);
        score = new double[scored.size()];
        survived = new int[scored.size()];
        if (field.isEmpty()) {
            System.err.println("no scoring groups -- only zombies");
            System.exit(1);
        }

        final int fieldSize = field.size();
        final int k = (combo > 0 && combo < fieldSize) ? combo : fieldSize;
        final EngGroup[] fieldArr  = field.toArray(new EngGroup[0]);
        final EngGroup[] alwaysArr = always.toArray(new EngGroup[0]);
        final boolean unlimited = games <= 0;
        final int cap = unlimited ? Integer.MAX_VALUE / 2 : games;
        if (threads <= 0) threads = Runtime.getRuntime().availableProcessors();
        threads = Math.max(1, Math.min(threads, cap));

        System.out.printf("JavaLane: %d groups (+%d zombie), %d per battle, stride %d phase %d, %s, %d threads%n",
                          fieldSize, alwaysArr.length, k, stride, phase,
                          unlimited ? "open-ended" : games + " games", threads);

        t0 = System.nanoTime();
        final Path out = scoresPath;
        writeScores(out);

        // the app went away: nobody is reading, so stop
        if (parent > 0) {
            final Optional<ProcessHandle> ph = ProcessHandle.of(parent);
            Thread watch = new Thread(() -> {
                for (;;) {
                    if (ph.isEmpty() || !ph.get().isAlive()) Runtime.getRuntime().halt(3);
                    try { Thread.sleep(1000); } catch (InterruptedException e) { return; }
                }
            }, "parent-watch");
            watch.setDaemon(true);
            watch.start();
        }

        final long fSeed = seed, fPair = pairSeed;
        final int fStride = stride, fPhase = phase, fRounds = rounds, fZs = zspeed;
        final AtomicInteger next = new AtomicInteger();
        final int CHUNK = 4;

        Thread[] pool = new Thread[threads];
        for (int t = 0; t < threads; t++) {
            pool[t] = new Thread(() -> {
                int[] order = new int[fieldSize];
                int[] plan = new int[k + alwaysArr.length];
                for (int i = 0; i < plan.length; i++) plan[i] = i;
                EngGroup[] war = new EngGroup[k + alwaysArr.length];
                for (int i = 0; i < alwaysArr.length; i++) war[k + i] = alwaysArr[i];
                String[][] awardNames = new String[1][];
                float[] awardScore = new float[1];

                double[] myScore = new double[score.length];
                int[] mySurv = new int[score.length];
                int myRun = 0, myFailed = 0;
                long lastFold = System.nanoTime();

                for (;;) {
                    int base = next.getAndAdd(CHUNK);
                    if (base >= cap) break;
                    int end = Math.min(base + CHUNK, cap);
                    for (int kk = base; kk < end; kk++) {
                        long g = (long) fPhase + (long) kk * fStride;
                        drawCombo(fPair, (int) g, fieldSize, k, order);
                        for (int i = 0; i < k; i++) war[i] = fieldArr[order[i]];
                        try {
                            Tourney.runBattle(war, plan, null, fSeed + g, fRounds, fZs,
                                              awardNames, awardScore, 0);
                        } catch (Exception e) {
                            myFailed++;              // could not be placed: void, not a loss
                            continue;
                        }
                        myRun++;
                        double share = awardScore[0];
                        for (String n : awardNames[0]) {
                            Integer s = slotOf.get(n);
                            if (s == null) continue;
                            myScore[s] += share;
                            mySurv[s]++;
                        }
                    }
                    long now = System.nanoTime();
                    if (now - lastFold > 150_000_000L || end >= cap) {
                        synchronized (lock) {
                            for (int i = 0; i < myScore.length; i++) {
                                score[i] += myScore[i]; myScore[i] = 0;
                                survived[i] += mySurv[i]; mySurv[i] = 0;
                            }
                            gamesRun += myRun; myRun = 0;
                            gamesFailed += myFailed; myFailed = 0;
                        }
                        lastFold = now;
                    }
                }
                synchronized (lock) {
                    for (int i = 0; i < myScore.length; i++) {
                        score[i] += myScore[i];
                        survived[i] += mySurv[i];
                    }
                    gamesRun += myRun;
                    gamesFailed += myFailed;
                }
            }, "lane-" + t);
            pool[t].setDaemon(true);
            pool[t].start();
        }

        // report a few times a second until every worker is done
        for (;;) {
            boolean alive = false;
            for (Thread th : pool) {
                th.join(200);
                if (th.isAlive()) { alive = true; break; }
            }
            writeScores(out);
            if (!alive) break;
        }
        writeScores(out);
        System.out.println("JavaLane: done, " + gamesRun + " games");
    }
}
