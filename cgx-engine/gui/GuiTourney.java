import org.apache.commons.math3.random.RandomDataGenerator;

import java.io.File;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;
import java.util.concurrent.atomic.DoubleAdder;

/**
 * GUI-side tournament driver.  A FRONT END over the proven engine, not a
 * reimplementation of it.
 *
 * ================== WHAT THIS CLASS IS AND IS NOT =========================
 *
 * Everything that can move a digit of the answer is CALLED, not copied:
 *
 *   - group construction   -> Field.groupsOf          (unchanged)
 *   - zombie construction  -> Tourney.zombieGroup     (unchanged)
 *   - the battle itself    -> Tourney.runBattle       (unchanged)
 *   - score accumulation   -> Tourney.Acc             (unchanged)
 *   - the csv layout       -> Tourney.writeScores     (unchanged)
 *   - the pairing draw     -> the same seeded RandomDataGenerator.nextPermutation
 *
 * No file under src/ is edited by the GUI, and the GUI is compiled against the
 * promoted buildE class files, so the classes that carry the G1/G2/G3/OpSweep
 * proof are the exact bytes that run here.
 *
 * ================== THE ORDER-DEPENDENCE, AND HOW IT IS KEPT ==============
 *
 * Tourney's whole determinism argument is that battles run in parallel but the
 * float32 score accumulation is REPLAYED SEQUENTIALLY IN BATTLE ORDER once the
 * pool has joined.  That is what makes --threads unable to change scores.csv.
 *
 * A live display that added each battle's award to a running total as the
 * battle FINISHED would accumulate in COMPLETION order, which is not battle
 * order, and float32 addition is not associative -- so it would land on a
 * different final number.
 *
 * Therefore this class keeps the two things strictly apart:
 *
 *   PREVIEW  (previewSnapshot()) -- fed to the bars while the run is in flight.
 *            Accumulated in completion order, in DOUBLE, in DoubleAdder.  It is
 *            deliberately a different type from the canonical path so it can
 *            never be mistaken for it, and it is never written to a file.
 *
 *   FINAL    (Result.groupScore / warriorScore) -- produced ONLY after every
 *            worker has joined, by replaying slots 0..N-1 in battle order
 *            through Tourney.Acc.  Bit-for-bit what `Tourney -o scores.csv`
 *            would have written for the same field, seeds and battle count.
 *
 * Result.maxPreviewDelta reports how far the preview drifted, so the UI can
 * state the difference rather than assert it is zero.
 *
 * ================== STOPPING ==============================================
 *
 * stop() sets a flag the workers poll between battles.  Workers are daemon
 * threads, so even a wedged battle cannot outlive the window, and run() always
 * joins them before touching the award arrays.
 *
 * A stopped run still produces a CANONICAL result: it replays the longest
 * CONTIGUOUS PREFIX of completed battles, slots 0..N-1.  That is exactly the
 * result a full run of N battles would have produced -- not a ragged set of
 * whichever battles happened to land -- so a partial number is still a real
 * number, just of a shorter tournament.
 */
public final class GuiTourney {

    // ---------------------------------------------------------------- config

    public static final class Config {
        public File survivors;
        public File zombies;            // null or non-directory => no zombies
        public File out = new File("scores.csv");
        public int  battles     = 5000; // <=0 => binomial(n,k) * 100, as Tourney
        public int  combo       = 4;    // <=0 => all groups
        public int  threads     = 0;    // <=0 => Tourney.autoThreads()
        public long seedBase    = "guru".hashCode();
        public long pickSeed    = 20260731L;
        public int  maxRound    = 200000;
        public int  zombieSpeed = 2;
    }

    // ---------------------------------------------------------------- result

    /** The CANONICAL outcome.  Only ever built by the sequential replay. */
    public static final class Result {
        public String[]  groupNames;
        public float[]   groupScore;      // float32, battle-order accumulation
        public String[][] warriorNames;
        public float[][] warriorScore;
        public int       battlesRun;      // battles replayed (== requested unless partial)
        public int       battlesRequested;
        public boolean   partial;         // true if STOP cut the run short
        public String    csvPath;
        public double    maxPreviewDelta; // |preview - canonical|, max over groups; -1 if partial
        public long      elapsedNanos;
        public List<String> problems = new ArrayList<>();
    }

    // ------------------------------------------------------------ live state
    // Read by the Swing timer at ~25 Hz.  Nothing here is on the battle hot
    // path except one incrementAndGet and one DoubleAdder.add per battle.

    public static final int IDLE = 0, LOADING = 1, PLANNING = 2, RUNNING = 3,
                            ACCUMULATING = 4, DONE = 5, FAILED = 6, STOPPED = 7;

    public volatile int      phase        = IDLE;
    public volatile String   phaseText    = "idle";
    public volatile String[] groupNames   = new String[0];
    public volatile int      totalBattles = 0;
    public volatile int      threadsUsed  = 0;
    public volatile int      comboUsed    = 0;
    public volatile int      zombieFiles  = 0;
    public volatile long     startNanos   = 0;
    public volatile long     endNanos     = 0;
    public final AtomicInteger completed  = new AtomicInteger();

    private volatile DoubleAdder[] preview = new DoubleAdder[0];
    private volatile boolean stopRequested = false;

    public void stop() { stopRequested = true; }
    public boolean isStopRequested() { return stopRequested; }

    /** Elapsed wall time of the battle phase, in seconds. */
    public double elapsedSeconds() {
        long s = startNanos;
        if (s == 0) return 0;
        long e = endNanos;
        return ((e == 0 ? System.nanoTime() : e) - s) / 1e9;
    }

    /**
     * PREVIEW ONLY.  Completion-order double accumulation.  Never written to a
     * file, never compared against the jar, never used as an answer.
     */
    public double[] previewSnapshot() {
        DoubleAdder[] p = preview;
        double[] out = new double[p.length];
        for (int i = 0; i < p.length; i++) out[i] = p[i].sum();
        return out;
    }

    // ------------------------------------------------------------------- run

    /** Runs the whole tournament on the CALLING thread (use a worker, not the EDT). */
    public Result run(Config cfg) throws Exception {
        stopRequested = false;
        completed.set(0);
        startNanos = 0;
        endNanos = 0;
        phase = LOADING;
        phaseText = "loading warriors";

        // ---- field, exactly as Tourney builds it
        List<String> problems = new ArrayList<>();
        if (cfg.survivors == null || !cfg.survivors.isDirectory())
            throw new IllegalArgumentException("Survivors folder not found:\n" + cfg.survivors);
        EngGroup[] field = Field.groupsOf(cfg.survivors, problems).toArray(new EngGroup[0]);
        if (field.length == 0)
            throw new IllegalArgumentException(
                    "No warrior files in\n" + cfg.survivors.getAbsolutePath()
                  + "\n\nDrop the survivor files straight into that folder.");

        EngGroup zombies = null;
        if (cfg.zombies != null && cfg.zombies.isDirectory())
            zombies = Tourney.zombieGroup(cfg.zombies);
        zombieFiles = zombies == null ? 0 : zombies.size();

        String[] names = new String[field.length];
        for (int g = 0; g < field.length; g++) names[g] = field[g].name;
        groupNames = names;

        DoubleAdder[] pv = new DoubleAdder[field.length];
        for (int g = 0; g < pv.length; g++) pv[g] = new DoubleAdder();
        preview = pv;

        // ---- combo / battle count, exactly as Tourney resolves them
        int combo = cfg.combo <= 0 ? field.length : cfg.combo;
        final int k = Math.max(1, Math.min(combo, field.length));
        comboUsed = k;
        int battles = cfg.battles;
        if (battles <= 0)
            battles = (int) Math.min(Integer.MAX_VALUE, Tourney.binomial(field.length, k) * 100L);
        final int nb = battles;
        totalBattles = nb;

        int threads = cfg.threads;
        if (threads <= 0) threads = Tourney.autoThreads();
        threads = Math.max(1, Math.min(threads, nb));
        threadsUsed = threads;

        // ---- the battle list.  Drawn on ONE thread, up front, from a SEEDED
        //      RandomDataGenerator -- identical to Tourney, so the same seeds
        //      give the same tournament whether it was launched from the GUI or
        //      from the command line.
        phase = PLANNING;
        phaseText = "planning " + nb + " battles";
        RandomDataGenerator pick = new RandomDataGenerator();
        pick.reSeed(cfg.pickSeed);
        final int[][] plan = new int[nb][];
        for (int b = 0; b < nb; b++) plan[b] = pick.nextPermutation(field.length, k);

        // ---- preview routing table: the SAME rule Tourney.Acc uses to decide
        //      which group an award lands on (built in group order, a LATER
        //      group with the same warrior name overwrites an earlier one).
        //      Used for the preview bars only.
        final Map<String, Integer> nameToGroup = new HashMap<>();
        for (int g = 0; g < field.length; g++)
            for (String n : field[g].warriorNames) nameToGroup.put(n, g);

        // ---- award record, one slot per battle, indexed by BATTLE NUMBER.
        final String[][] awardNames = new String[nb][];
        final float[]    awardScore = new float[nb];

        final EngGroup[] ffield = field;
        final EngGroup   fz     = zombies;
        final int  fmax  = cfg.maxRound, fzs = cfg.zombieSpeed;
        final long fseed = cfg.seedBase;
        final DoubleAdder[] fpv = pv;

        phase = RUNNING;
        phaseText = "running";
        startNanos = System.nanoTime();

        final AtomicInteger cursor = new AtomicInteger(0);
        final AtomicReference<Throwable> failure = new AtomicReference<>();
        Thread[] pool = new Thread[threads];
        for (int t = 0; t < threads; t++) {
            pool[t] = new Thread(() -> {
                try {
                    int b;
                    while ((b = cursor.getAndIncrement()) < nb) {
                        if (stopRequested || failure.get() != null) return;
                        Tourney.runBattle(ffield, plan[b], fz, fseed + b, fmax, fzs,
                                          awardNames, awardScore, b);
                        // ---- PREVIEW ONLY.  Completion order, double.  The
                        //      canonical float32 total is computed below, from
                        //      the same slots, in BATTLE order.
                        String[] won = awardNames[b];
                        float v = awardScore[b];
                        for (int i = 0; i < won.length; i++) {
                            Integer gi = nameToGroup.get(won[i]);
                            if (gi != null) fpv[gi].add(v);
                        }
                        completed.incrementAndGet();
                    }
                } catch (Throwable e) {
                    failure.compareAndSet(null, e);
                }
            }, "cgx-battle-" + t);
            pool[t].setDaemon(true);          // cannot outlive the window
        }
        for (Thread th : pool) th.start();
        for (Thread th : pool) th.join();     // ALWAYS joined before the replay
        endNanos = System.nanoTime();

        Throwable e = failure.get();
        if (e != null) { phase = FAILED; throw new RuntimeException("battle failed", e); }

        // ---- the longest CONTIGUOUS prefix of finished battles.  On a normal
        //      run this is every battle; after STOP it is the prefix, which is
        //      exactly the battle list a shorter run would have used.
        int n = 0;
        while (n < nb && awardNames[n] != null) n++;
        final boolean partial = n < nb;

        // ================= THE CANONICAL ACCUMULATION =====================
        // Sequential, in battle order, float32, through Tourney.Acc -- the same
        // class G3 proved bit-identical to the jar's accumulator over 7,500
        // battles.  Nothing above this line has touched these numbers.
        phase = ACCUMULATING;
        phaseText = "canonical replay";
        Tourney.Acc acc = new Tourney.Acc(field);
        for (int b = 0; b < n; b++) {
            String[] won = awardNames[b];
            float v = awardScore[b];
            for (int i = 0; i < won.length; i++) acc.addScore(won[i], v);
        }
        // =================================================================

        Result r = new Result();
        r.groupNames = names;
        r.groupScore = acc.groupScore.clone();
        r.warriorNames = new String[field.length][];
        r.warriorScore = new float[field.length][];
        for (int g = 0; g < field.length; g++) {
            r.warriorNames[g] = field[g].warriorNames.clone();
            r.warriorScore[g] = acc.warriorScore[g].clone();
        }
        r.battlesRun = n;
        r.battlesRequested = nb;
        r.partial = partial;
        r.elapsedNanos = endNanos - startNanos;
        r.problems = problems;

        // How far did the preview drift from the truth?  Only meaningful on a
        // complete run -- after a STOP the preview also contains battles beyond
        // the replayed prefix, so the comparison is apples to oranges.
        if (!partial) {
            double worst = 0;
            for (int g = 0; g < field.length; g++)
                worst = Math.max(worst, Math.abs(fpv[g].sum() - acc.groupScore[g]));
            r.maxPreviewDelta = worst;
        } else {
            r.maxPreviewDelta = -1;
        }

        if (cfg.out != null) {
            File parent = cfg.out.getAbsoluteFile().getParentFile();
            if (parent != null) parent.mkdirs();
            Tourney.writeScores(cfg.out.getAbsolutePath(), field, acc);
            r.csvPath = cfg.out.getAbsolutePath();
        }

        phase = partial ? STOPPED : DONE;
        phaseText = partial ? "stopped" : "done";
        return r;
    }
}
