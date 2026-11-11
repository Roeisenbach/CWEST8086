import java.io.File;

/**
 * Headless self-check for the GUI's tournament driver.
 *
 * The window is a front end, so the one thing that has to be proven about it is
 * that IT DID NOT CHANGE THE ANSWER.  This runs GuiTourney with the same field,
 * seeds, combo size and battle count that `Tourney` would use, and writes a
 * scores.csv the caller can diff byte-for-byte against Tourney's own.
 *
 * Usage:
 *   java -cp buildGui;..\buildE;<jar> GuiSelfCheck -w <dir> -z <dir|-> -o out.csv
 *        [-c 4] [--battles 400] [--threads 6] [--stop-after <n>]
 *
 * --stop-after exercises the STOP path: it calls stop() once n battles have
 * finished, so the result is the canonical replay of the contiguous prefix.
 */
public final class GuiSelfCheck {

    public static void main(String[] args) throws Exception {
        GuiTourney.Config cfg = new GuiTourney.Config();
        int stopAfter = -1;
        for (int i = 0; i < args.length; i++) {
            switch (args[i]) {
                case "-w": cfg.survivors = new File(args[++i]); break;
                case "-z": { String z = args[++i]; cfg.zombies = z.equals("-") ? null : new File(z); break; }
                case "-o": cfg.out = new File(args[++i]); break;
                case "-c": cfg.combo = Integer.parseInt(args[++i]); break;
                case "-s": cfg.seedBase = Tourney.parseSeed(args[++i]); break;
                case "--battles": cfg.battles = Integer.parseInt(args[++i]); break;
                case "--threads": cfg.threads = Integer.parseInt(args[++i]); break;
                case "--pick-seed": cfg.pickSeed = Long.parseLong(args[++i]); break;
                case "--stop-after": stopAfter = Integer.parseInt(args[++i]); break;
                default: throw new IllegalArgumentException("unknown arg " + args[i]);
            }
        }

        final GuiTourney t = new GuiTourney();
        if (stopAfter > 0) {
            final int n = stopAfter;
            Thread stopper = new Thread(() -> {
                try {
                    while (t.completed.get() < n) Thread.sleep(1);
                    t.stop();
                } catch (InterruptedException ignored) { }
            }, "stopper");
            stopper.setDaemon(true);
            stopper.start();
        }

        GuiTourney.Result r = t.run(cfg);
        System.out.printf("GuiSelfCheck: %d groups, combo %d, %d/%d battles%s, %d threads, %.2f s%n",
                r.groupNames.length, t.comboUsed, r.battlesRun, r.battlesRequested,
                r.partial ? " (PARTIAL, stopped)" : "", t.threadsUsed, r.elapsedNanos / 1e9);
        System.out.printf("preview-vs-canonical max delta: %s%n",
                r.maxPreviewDelta < 0 ? "n/a (partial run)" : String.valueOf(r.maxPreviewDelta));
        for (String p : r.problems) System.out.println("INPUT-PROBLEM " + p);
        System.out.println("wrote " + r.csvPath);
    }
}
