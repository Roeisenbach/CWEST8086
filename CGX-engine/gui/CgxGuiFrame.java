import javax.swing.BorderFactory;
import javax.swing.Box;
import javax.swing.BoxLayout;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JComponent;
import javax.swing.JFileChooser;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JProgressBar;
import javax.swing.JScrollPane;
import javax.swing.JSpinner;
import javax.swing.JSplitPane;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.SpinnerNumberModel;
import javax.swing.SwingUtilities;
import javax.swing.Timer;
import javax.swing.UIManager;
import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Desktop;
import java.awt.Dimension;
import java.awt.FlowLayout;
import java.awt.Font;
import java.awt.event.ComponentAdapter;
import java.awt.event.ComponentEvent;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.io.File;
import java.io.PrintStream;
import java.text.ParseException;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * The window.
 *
 * ONE folder to drop warriors into (survivors/, with zombies/ beside it, the
 * jar's convention), ONE file to double-click (RUN.bat), a START button, live
 * candles with a place number on each, a progress read-out and a STOP that
 * actually stops.  No arena view -- deliberately; it was not asked for.
 *
 * THE UI THREAD NEVER RUNS A BATTLE.  The tournament runs on a daemon worker,
 * which itself spawns the daemon battle pool; the EDT only reads counters and
 * a preview snapshot off a 40 ms Swing timer.  Nothing here can wedge the
 * window, and nothing here can outlive it.
 *
 * THE NUMBERS.  While a run is in flight the bars show GuiTourney's PREVIEW,
 * accumulated in completion order.  When it finishes the bars are replaced by
 * the CANONICAL result -- re-accumulated sequentially in battle order, float32,
 * through Tourney.Acc -- which is what scores.csv contains.  The two can differ
 * in the last digits and the window says so, in the header of the graph, in the
 * standing panel's title and in the footer.
 */
public final class CgxGuiFrame extends JFrame {

    private static final long serialVersionUID = 1L;

    private static final Color PANEL_BG = new Color(32, 32, 32);
    private static final Color TEXT     = new Color(225, 225, 225);
    private static final Color WARN     = new Color(255, 190, 90);
    private static final Color NOTE_OK  = new Color(130, 225, 150);

    private static final String PREVIEW_NOTE =
            "Bars during a run are a LIVE PREVIEW (added in completion order). "
          + "The final numbers are re-accumulated sequentially in battle order, float32 — "
          + "the last digits can differ.";

    private final File guiDir;

    private final JTextField survivorsField = new JTextField(38);
    private final JTextField zombiesField   = new JTextField(24);
    private final JSpinner battlesSpin = new JSpinner(new SpinnerNumberModel(5000, 1, 100000000, 500));
    private final JSpinner comboSpin   = new JSpinner(new SpinnerNumberModel(4, 1, 64, 1));
    private final JSpinner threadsSpin = new JSpinner(new SpinnerNumberModel(0, 0, 128, 1));
    private final JTextField seedField = new JTextField("guru", 6);
    private final JTextField pickField = new JTextField("20260731", 9);
    private final JCheckBox reshuffle  = new JCheckBox("reshuffle each run");

    private final JButton startBtn = new JButton("START");
    private final JButton stopBtn  = new JButton("STOP");
    private final JButton browseBtn = new JButton("Choose folder…");
    private final JButton openBtn   = new JButton("Open folder");
    private final JButton helpBtn   = new JButton("?");

    private final CandleGraph    graph     = new CandleGraph();
    private final StandingsPanel standings = new StandingsPanel();
    private final JProgressBar progress = new JProgressBar(0, 1000);
    private final JLabel status  = new JLabel(" ");
    private final JLabel caveat  = new JLabel(" ");

    private GuiTourney runner;
    private Thread worker;
    private String[] boundNames;          // group names currently bound to the views
    private boolean  showingFinal;
    private String   lastProblems = "";
    private GuiTourney.Config lastCfg;    // exactly what the running run was given
    private String   lastSeedText = "guru";

    // ------------------------------------------------------------------ setup

    public CgxGuiFrame(File guiDir, File survivors, File zombies) {
        super("CGX Tournament — fast engine");
        this.guiDir = guiDir;
        survivorsField.setText(survivors.getAbsolutePath());
        zombiesField.setText(zombies.getAbsolutePath());

        setDefaultCloseOperation(DO_NOTHING_ON_CLOSE);
        addWindowListener(new WindowAdapter() {
            @Override public void windowClosing(WindowEvent e) { shutdown(); }
        });

        setLayout(new BorderLayout(0, 0));
        add(buildControls(), BorderLayout.NORTH);

        final JSplitPane split = new JSplitPane(JSplitPane.HORIZONTAL_SPLIT, graph, standings);
        split.setResizeWeight(1.0);          // shrinking the window shrinks the graph
        split.setDividerLocation(880);
        split.setBorder(null);
        split.setContinuousLayout(true);
        add(split, BorderLayout.CENTER);
        // ...but never let the standing list collapse to a sliver, and never let
        // it eat the graph, whatever width the window is dragged to.
        addComponentListener(new ComponentAdapter() {
            @Override public void componentResized(ComponentEvent e) {
                int w = split.getWidth();
                if (w <= 0) return;
                int loc = split.getDividerLocation();
                int max = w - 190, min = Math.min(240, w / 2);
                if (loc > max) split.setDividerLocation(Math.max(min, w - 270));
                else if (loc < min) split.setDividerLocation(min);
            }
        });

        add(buildFooter(), BorderLayout.SOUTH);

        startBtn.addActionListener(e -> start());
        stopBtn.addActionListener(e -> requestStop());
        browseBtn.addActionListener(e -> browse());
        openBtn.addActionListener(e -> openFolder());
        helpBtn.addActionListener(e -> showHelp());
        stopBtn.setEnabled(false);

        // ONE timer in the process: animation, progress and preview all ride it.
        new Timer(40, e -> pulse()).start();

        setMinimumSize(new Dimension(760, 480));
        setSize(1180, 760);
        setLocationRelativeTo(null);
    }

    private JComponent buildControls() {
        JPanel root = new JPanel();
        root.setLayout(new BoxLayout(root, BoxLayout.Y_AXIS));
        root.setBackground(PANEL_BG);
        root.setBorder(BorderFactory.createEmptyBorder(6, 8, 6, 8));

        JPanel r1 = row();
        r1.add(lbl("Survivors folder"));
        r1.add(w(survivorsField, 320));
        r1.add(browseBtn);
        r1.add(openBtn);
        r1.add(lbl("   Zombies"));
        r1.add(w(zombiesField, 190));
        r1.add(helpBtn);

        JPanel r2 = row();
        r2.add(lbl("Battles"));
        r2.add(w(battlesSpin, 90));
        r2.add(lbl("  Combo size"));
        r2.add(w(comboSpin, 55));
        r2.add(lbl("  Threads"));
        r2.add(w(threadsSpin, 55));
        r2.add(lbl("  Seed"));
        r2.add(w(seedField, 70));
        r2.add(lbl("  Pairing seed"));
        r2.add(w(pickField, 90));
        reshuffle.setBackground(PANEL_BG);
        reshuffle.setForeground(TEXT);
        reshuffle.setToolTipText("<html>Off: the same folder and the two seeds always produce the same tournament."
                + "<br>On: a fresh pairing seed is drawn at START and <b>written into the Pairing seed box</b>,"
                + "<br>so the run you just watched can still be repeated exactly.</html>");
        r2.add(reshuffle);
        r2.add(Box.createHorizontalStrut(14));
        startBtn.setFont(startBtn.getFont().deriveFont(Font.BOLD, 14f));
        stopBtn.setFont(stopBtn.getFont().deriveFont(Font.BOLD, 14f));
        r2.add(startBtn);
        r2.add(stopBtn);

        battlesSpin.setToolTipText("How many battles to run. 5,000 is a few seconds; 25,000 is the usual measurement.");
        comboSpin.setToolTipText("How many groups fight in each battle. Clamped to the number of groups found.");
        threadsSpin.setToolTipText("0 = auto (half the logical cores = the physical core count). "
                + "Thread count cannot change the final scores.");
        seedField.setToolTipText("Battle seed base. A word (like guru) or a number.");
        pickField.setToolTipText("<html>Seed of the pairing draw — <b>which</b> groups meet in each battle."
                + "<br>Together with Seed, Battles and Combo size it names the tournament exactly."
                + "<br>Printed in the status line and in last-run.txt after every run.</html>");
        helpBtn.setToolTipText("How this window works");

        root.add(r1);
        root.add(r2);
        return root;
    }

    private JComponent buildFooter() {
        JPanel p = new JPanel(new BorderLayout(8, 2));
        p.setBackground(PANEL_BG);
        p.setBorder(BorderFactory.createEmptyBorder(4, 8, 6, 8));

        progress.setStringPainted(true);
        progress.setString("");
        progress.setPreferredSize(new Dimension(260, 18));

        status.setForeground(TEXT);
        status.setFont(new Font(Font.SANS_SERIF, Font.PLAIN, 12));
        status.setText("Ready.");

        caveat.setForeground(WARN);
        caveat.setFont(new Font(Font.SANS_SERIF, Font.PLAIN, 11));
        caveat.setText(PREVIEW_NOTE);

        JPanel line = new JPanel(new BorderLayout(8, 0));
        line.setBackground(PANEL_BG);
        line.add(progress, BorderLayout.WEST);
        line.add(status, BorderLayout.CENTER);

        p.add(line, BorderLayout.NORTH);
        p.add(caveat, BorderLayout.SOUTH);
        return p;
    }

    private static JPanel row() {
        // WrapLayout, not FlowLayout: a narrow window must make the toolbar
        // taller, never hide the START button below the clip.
        JPanel p = new JPanel(new WrapLayout(FlowLayout.LEFT, 5, 3));
        p.setBackground(PANEL_BG);
        return p;
    }

    private static <T extends JComponent> T w(T c, int width) {
        Dimension d = c.getPreferredSize();
        c.setPreferredSize(new Dimension(width, d.height));
        return c;
    }

    private static JLabel lbl(String s) {
        JLabel l = new JLabel(s);
        l.setForeground(TEXT);
        return l;
    }

    // ------------------------------------------------------------------- run

    private void start() {
        if (worker != null && worker.isAlive()) return;

        // ------------------------------------------------------------------
        // COMMIT THE SPINNER EDITORS FIRST.  A JSpinner's formatted-text editor
        // only writes its typed text into the model on focus-lost, and on
        // Windows that focus-lost lands AFTER this button's actionPerformed.
        // Without this the window would read the PREVIOUS value while the field
        // on screen showed the new one -- the numbers would still be correct
        // for whatever ran, but the window would be labelling them with a
        // tournament that did not happen.  Nothing below may read getValue()
        // before this block.
        // ------------------------------------------------------------------
        try { battlesSpin.commitEdit(); } catch (ParseException e) { battlesSpin.setValue(battlesSpin.getValue()); }
        try { comboSpin.commitEdit();   } catch (ParseException e) { comboSpin.setValue(comboSpin.getValue()); }
        try { threadsSpin.commitEdit(); } catch (ParseException e) { threadsSpin.setValue(threadsSpin.getValue()); }

        GuiTourney.Config cfg = new GuiTourney.Config();
        cfg.survivors = new File(survivorsField.getText().trim());
        File z = new File(zombiesField.getText().trim());
        cfg.zombies = z.isDirectory() ? z : null;
        cfg.out = new File(guiDir, "scores.csv");
        cfg.battles = (Integer) battlesSpin.getValue();
        cfg.combo   = (Integer) comboSpin.getValue();
        cfg.threads = (Integer) threadsSpin.getValue();
        lastSeedText = seedField.getText().trim().isEmpty() ? "guru" : seedField.getText().trim();
        cfg.seedBase = Tourney.parseSeed(lastSeedText);

        // The pairing seed is a VISIBLE, EDITABLE value, never a hidden one.
        // "reshuffle" draws a fresh one and writes it straight back into the box,
        // so even a reshuffled run can be repeated afterwards.
        if (reshuffle.isSelected()) {
            cfg.pickSeed = System.nanoTime();
            pickField.setText(Long.toString(cfg.pickSeed));
        } else {
            try {
                cfg.pickSeed = Long.parseLong(pickField.getText().trim());
            } catch (NumberFormatException e) {
                cfg.pickSeed = 20260731L;
                pickField.setText("20260731");
            }
        }

        if (!cfg.survivors.isDirectory()) {
            JOptionPane.showMessageDialog(this,
                    "That survivors folder does not exist:\n" + cfg.survivors.getAbsolutePath(),
                    "No such folder", JOptionPane.ERROR_MESSAGE);
            return;
        }
        if (!confirmDropFolders(cfg)) return;
        lastCfg = cfg;

        boundNames = null;
        showingFinal = false;
        lastProblems = "";
        graph.setGroups(new String[0]);
        standings.setGroups(new String[0], new GroupColors(new String[0]));
        progress.setValue(0);
        progress.setString("");
        status.setText("Loading warriors…");
        status.setToolTipText(null);
        caveat.setForeground(WARN);
        caveat.setText(PREVIEW_NOTE);
        setInputsEnabled(false);

        final GuiTourney r = new GuiTourney();
        runner = r;
        worker = new Thread(() -> {
            try {
                GuiTourney.Result res = r.run(cfg);
                SwingUtilities.invokeLater(() -> onFinished(res));
            } catch (Throwable t) {
                SwingUtilities.invokeLater(() -> onFailed(t));
            }
        }, "cgx-tourney");
        worker.setDaemon(true);          // a run can never keep the JVM alive
        worker.start();
    }

    /**
     * The drop folders hold COMPETITORS, not documents.  Field.groupsOf skips
     * exactly one filename -- "README.md" -- and Tourney.zombieGroup skips none
     * at all, so a stray note, spreadsheet or Thumbs.db dropped in beside the
     * warriors silently becomes an extra group (or an extra zombie) and moves
     * every score in the table.  That is a trap worth one dialog.
     *
     * Reports and asks; never deletes, never renames, never silently drops a
     * file -- the field handed to the engine is still exactly what is on disk.
     */
    private boolean confirmDropFolders(GuiTourney.Config cfg) {
        List<String> bad = new ArrayList<>();
        scanForJunk(cfg.survivors, true, "survivors", bad);
        if (cfg.zombies != null) scanForJunk(cfg.zombies, false, "zombies", bad);
        if (bad.isEmpty()) return true;

        StringBuilder sb = new StringBuilder();
        sb.append("These files are in the drop folders but do not look like warriors.\n")
          .append("The loader does not know that: every one of them will be entered\n")
          .append("into the tournament and will change the scores.\n\n");
        for (String s : bad) sb.append("    ").append(s).append('\n');
        sb.append("\n(The only filename skipped automatically is README.md, and only in\n")
          .append("the survivors folder. The zombies folder skips nothing at all.)\n\n")
          .append("Run anyway?");

        JTextArea ta = new JTextArea(sb.toString());
        ta.setEditable(false);
        ta.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 12));
        int r = JOptionPane.showConfirmDialog(this, new JScrollPane(ta),
                "Non-warrior files in the drop folder", JOptionPane.YES_NO_OPTION,
                JOptionPane.WARNING_MESSAGE);
        return r == JOptionPane.YES_OPTION;
    }

    private static final String[] JUNK_EXT = {
        ".txt", ".md", ".csv", ".json", ".log", ".bat", ".cmd", ".ps1", ".sh",
        ".py", ".ini", ".db", ".lnk", ".url", ".exe", ".dll", ".class", ".jar",
        ".zip", ".rar", ".7z", ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".pdf",
        ".doc", ".docx", ".xls", ".xlsx", ".htm", ".html", ".xml", ".yml", ".yaml"
    };

    private static void scanForJunk(File dir, boolean readmeIsSkipped, String label, List<String> out) {
        File[] fs = dir.listFiles();
        if (fs == null) return;
        for (File f : fs) {
            if (f.isDirectory()) continue;
            String n = f.getName();
            if (readmeIsSkipped && n.equals("README.md")) continue;   // Field skips this one, exactly
            String low = n.toLowerCase(Locale.ROOT);
            boolean junk = low.equals("desktop.ini") || low.equals("thumbs.db");
            for (String e : JUNK_EXT) if (low.endsWith(e)) { junk = true; break; }
            if (junk) out.add(label + "\\" + n);
        }
    }

    private void requestStop() {
        GuiTourney r = runner;
        if (r == null) return;
        r.stop();
        stopBtn.setEnabled(false);
        status.setText("Stopping — letting the battles in flight finish, then replaying canonically…");
    }

    private void setInputsEnabled(boolean on) {
        startBtn.setEnabled(on);
        stopBtn.setEnabled(!on);
        battlesSpin.setEnabled(on);
        comboSpin.setEnabled(on);
        threadsSpin.setEnabled(on);
        seedField.setEnabled(on);
        pickField.setEnabled(on);
        reshuffle.setEnabled(on);
        survivorsField.setEnabled(on);
        zombiesField.setEnabled(on);
        browseBtn.setEnabled(on);
    }

    // ------------------------------------------------------------ the pulse

    /** 25 Hz.  Reads counters, pushes the preview, steps the animation. */
    private void pulse() {
        GuiTourney r = runner;
        if (r == null) { graph.tick(); return; }

        String[] names = r.groupNames;
        if (names != boundNames && names.length > 0) {
            boundNames = names;
            graph.setGroups(names);
            standings.setGroups(names, graph.colors());
        }

        if (!showingFinal && boundNames != null) {
            double[] pv = r.previewSnapshot();
            if (pv.length == boundNames.length) {
                graph.setValues(pv);
                standings.update(pv, graph.orderBestFirst(), graph.ranks(), false);
            }
        }
        graph.tick();

        int phase = r.phase;
        if (phase == GuiTourney.RUNNING || phase == GuiTourney.ACCUMULATING) {
            int done = r.completed.get();
            int tot  = Math.max(1, r.totalBattles);
            double secs = r.elapsedSeconds();
            progress.setValue((int) (1000.0 * done / tot));
            progress.setString(String.format("%.0f%%", 100.0 * done / tot));
            double rate = secs > 0.05 ? done / secs : 0;
            String eta = (rate > 1 && done < tot)
                    ? String.format(" · ~%s left", clock((tot - done) / rate)) : "";
            graph.setHeadline(String.format(
                    "LIVE PREVIEW — %s / %s battles · %s%s   (final numbers come from the canonical sequential replay)",
                    fmtInt(done), fmtInt(tot), clock(secs), eta));
            status.setText(String.format(
                    "%s / %s battles · %s elapsed · %s battles/s · %d groups, combo %d, %d threads, %s · seed %s, pairing seed %s",
                    fmtInt(done), fmtInt(tot), clock(secs),
                    rate >= 10 ? fmtInt((int) rate) : String.format("%.1f", rate),
                    boundNames == null ? 0 : boundNames.length,
                    r.comboUsed, r.threadsUsed,
                    r.zombieFiles == 0 ? "no zombies" : (r.zombieFiles + " zombie files"),
                    lastSeedText, lastCfg == null ? "?" : Long.toString(lastCfg.pickSeed))
                    + (phase == GuiTourney.ACCUMULATING ? " · canonical replay…" : ""));
        } else if (phase == GuiTourney.LOADING || phase == GuiTourney.PLANNING) {
            status.setText(r.phaseText + "…");
            progress.setString("");
        }
    }

    // ------------------------------------------------------------- end of run

    private void onFinished(GuiTourney.Result res) {
        showingFinal = true;
        setInputsEnabled(true);
        stopBtn.setEnabled(false);

        if (res.groupNames != boundNames) {
            boundNames = res.groupNames;
            graph.setGroups(res.groupNames);
            standings.setGroups(res.groupNames, graph.colors());
        }
        double[] fin = new double[res.groupScore.length];
        for (int i = 0; i < fin.length; i++) fin[i] = res.groupScore[i];
        graph.setFinalValues(fin);
        standings.update(fin, graph.orderBestFirst(), graph.ranks(), true);

        double secs = res.elapsedNanos / 1e9;
        progress.setValue(1000);
        progress.setString(res.partial ? "stopped" : "100%");

        String repro = reproduceCommand(res);
        String reproFile = writeLastRun(res, repro);

        StringBuilder sb = new StringBuilder();
        sb.append(res.partial ? "STOPPED — " : "DONE — ");
        sb.append(fmtInt(res.battlesRun));
        if (res.partial) sb.append(" of ").append(fmtInt(res.battlesRequested));
        sb.append(" battles in ").append(clock(secs));
        if (secs > 0.01) sb.append(String.format(" (%s battles/s)", fmtInt((int) (res.battlesRun / secs))));
        sb.append("  ·  seed ").append(lastSeedText)
          .append(", pairing seed ").append(lastCfg == null ? "?" : Long.toString(lastCfg.pickSeed));
        // The "these are the canonical numbers" statement lives in the graph
        // header and the footer note; keeping it out of here stops the status
        // line from running past the edge of a narrow window.
        if (res.maxPreviewDelta >= 0)
            sb.append(String.format("  ·  live preview drifted at most %.4g", res.maxPreviewDelta));
        else
            // Say why the figure is missing rather than just dropping it: after a
            // STOP the preview holds battles the canonical replay deliberately
            // excluded, so the two are not comparable and the drift is UNMEASURED,
            // not zero.
            sb.append("  ·  preview drift not measurable after STOP");
        if (res.csvPath != null) sb.append("  ·  scores.csv written");
        if (reproFile != null) sb.append("  ·  last-run.txt");
        status.setText(sb.toString());
        status.setToolTipText("<html>" + (res.csvPath == null ? "" : "scores.csv &rarr; " + esc(res.csvPath) + "<br><br>")
                + "Repeat this exact tournament from the command line:<br><tt>"
                + esc(repro) + "</tt></html>");

        graph.setHeadline((res.partial ? "FINAL (partial, " + fmtInt(res.battlesRun) + " battles)"
                                       : "FINAL — " + fmtInt(res.battlesRun) + " battles")
                + " · canonical sequential float32 replay, identical to scores.csv");

        List<String> probs = res.problems;
        String note = "The numbers above are FINAL — the canonical sequential float32 replay, "
                + "the same values that went into scores.csv.";
        if (res.maxPreviewDelta < 0)
            note += "   The live-preview drift figure is missing because the run was STOPPED: "
                  + "the preview also counted battles the canonical replay left out, so the two are "
                  + "not comparable — the drift is unmeasured, not zero.";
        if (probs != null && !probs.isEmpty()) {
            lastProblems = String.join("\n", probs);
            note += "   " + probs.size() + " note(s) about the input files — see the ? button.";
        }
        caveat.setForeground(NOTE_OK);
        caveat.setText(note);
        runner = null;
    }

    /**
     * The command line that reproduces exactly what was just run.  Every value
     * in it is one the window showed, including the pairing seed -- so a run
     * with "reshuffle" ticked is still repeatable afterwards.
     *
     * For a STOPPED run --battles is the number that ACTUALLY ran, because the
     * canonical replay used the contiguous prefix; `Tourney --battles <that>`
     * is the run whose scores.csv this matches.
     */
    private String reproduceCommand(GuiTourney.Result res) {
        GuiTourney.Config c = lastCfg;
        if (c == null) return "";
        StringBuilder b = new StringBuilder("java -cp \"");
        b.append(new File(guiDir.getParentFile(), "buildE").getAbsolutePath())
         .append(";<corewars8086-6.0.0.jar>\" Tourney");
        b.append(" -w \"").append(c.survivors.getAbsolutePath()).append('"');
        if (c.zombies != null) b.append(" -z \"").append(c.zombies.getAbsolutePath()).append('"');
        b.append(" -o \"").append(c.out.getAbsolutePath()).append('"');
        b.append(" -s ").append(lastSeedText);
        b.append(" -c ").append(c.combo);
        b.append(" --battles ").append(res.battlesRun);
        b.append(" --pick-seed ").append(c.pickSeed);
        return b.toString();
    }

    /** Written beside scores.csv.  scores.csv itself is left byte-for-byte as
     *  Tourney.writeScores emits it -- no header, no comment line, nothing that
     *  would make it differ from the command-line tool's output. */
    private String writeLastRun(GuiTourney.Result res, String repro) {
        File f = new File(guiDir, "last-run.txt");
        try (PrintStream ps = new PrintStream(new java.io.FileOutputStream(f), true, "UTF-8")) {
            ps.println("CGX tournament — what the window last ran");
            ps.println("finished          : " + new java.util.Date());
            ps.println("survivors         : " + lastCfg.survivors.getAbsolutePath());
            ps.println("zombies           : " + (lastCfg.zombies == null ? "(none)"
                        : lastCfg.zombies.getAbsolutePath()));
            ps.println("groups            : " + res.groupNames.length);
            ps.println("combo size        : " + lastCfg.combo);
            ps.println("battles requested : " + res.battlesRequested);
            ps.println("battles run       : " + res.battlesRun + (res.partial ? "   (STOPPED early)" : ""));
            ps.println("battle seed       : " + lastSeedText + "   (= " + lastCfg.seedBase + ")");
            ps.println("pairing seed      : " + lastCfg.pickSeed);
            ps.println("scores written to : " + (res.csvPath == null ? "(none)" : res.csvPath));
            ps.println("preview drift     : " + (res.maxPreviewDelta >= 0
                        ? String.format("%.6g", res.maxPreviewDelta)
                        : "not measurable (run was stopped)"));
            ps.println();
            ps.println("Thread count cannot change these scores, so it is not part of the identity");
            ps.println("of the run and is not repeated here.");
            ps.println();
            ps.println("Repeat exactly:");
            ps.println("  " + repro);
            return f.getAbsolutePath();
        } catch (Exception e) {
            return null;
        }
    }

    private static String esc(String s) {
        return s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;");
    }

    private void onFailed(Throwable t) {
        setInputsEnabled(true);
        stopBtn.setEnabled(false);
        progress.setValue(0);
        progress.setString("");
        Throwable c = t.getCause() != null ? t.getCause() : t;
        status.setText("FAILED: " + c);
        JTextArea ta = new JTextArea(String.valueOf(c.getMessage() == null ? c : c.getMessage()));
        ta.setEditable(false);
        ta.setLineWrap(true);
        ta.setWrapStyleWord(true);
        ta.setColumns(50);
        ta.setRows(6);
        JOptionPane.showMessageDialog(this, new JScrollPane(ta),
                "Tournament failed", JOptionPane.ERROR_MESSAGE);
        runner = null;
    }

    // ------------------------------------------------------------------ misc

    private void browse() {
        JFileChooser fc = new JFileChooser(new File(survivorsField.getText().trim()));
        fc.setFileSelectionMode(JFileChooser.DIRECTORIES_ONLY);
        fc.setDialogTitle("Pick the folder holding the survivor files");
        if (fc.showOpenDialog(this) == JFileChooser.APPROVE_OPTION) {
            File d = fc.getSelectedFile();
            survivorsField.setText(d.getAbsolutePath());
            File sib = new File(d.getParentFile(), "zombies");
            if (sib.isDirectory()) zombiesField.setText(sib.getAbsolutePath());
        }
    }

    private void openFolder() {
        File d = new File(survivorsField.getText().trim());
        try {
            if (d.isDirectory() && Desktop.isDesktopSupported()) Desktop.getDesktop().open(d);
        } catch (Exception ignored) { }
    }

    private void showHelp() {
        String s =
            "HOW TO USE\n"
          + "  1. Put the survivor files in:   " + survivorsField.getText() + "\n"
          + "     Pairing follows the usual rule: a file whose name ends in 1 opens a group,\n"
          + "     the next file ending in 2 closes it, anything else is a group of one.\n"
          + "  2. Zombies, if you want them, go in:   " + zombiesField.getText() + "\n"
          + "     NOTHING ELSE goes in these folders.  README.md is the ONE filename the\n"
          + "     loader skips (and only in survivors); every other file becomes a\n"
          + "     competitor.  The window warns before it runs if it spots one.\n"
          + "  3. Press START.  Battles, combo size, threads and the two seeds all have\n"
          + "     working defaults — you never have to touch them.\n"
          + "  4. STOP ends the run cleanly and still reports real numbers (see below).\n"
          + "\n"
          + "THE BARS\n"
          + "  One candle per group, sorted so the leader is on the left, the place number\n"
          + "  in gold above each bar, the score just under it, the group name up the\n"
          + "  bottom edge.  Groups whose names share the first three letters share a\n"
          + "  colour, the same way the official window does it.\n"
          + "  Places are STANDARD COMPETITION RANKING: groups on the same score share a\n"
          + "  place and the next place is skipped, so a field can legitimately read\n"
          + "  1,2,3,3,5 — or end 21,21 with no 22.  A shared place is marked with '='\n"
          + "  in the standing list on the right.  It is not a rendering fault.\n"
          + "\n"
          + "REPEATING A RUN\n"
          + "  Two seeds decide a tournament: 'Seed' seeds each battle, 'Pairing seed'\n"
          + "  decides which groups meet.  Both are on screen, both are editable, and\n"
          + "  both are printed in the status line when the run ends.\n"
          + "  'reshuffle each run' draws a fresh pairing seed and WRITES IT BACK into the\n"
          + "  box, so even a reshuffled run can be repeated afterwards.\n"
          + "  last-run.txt, next to scores.csv, holds the whole configuration plus the\n"
          + "  exact command line that reproduces it.  (scores.csv itself is left exactly\n"
          + "  as the command-line tool writes it — no header, no comment line.)\n"
          + "\n"
          + "COMPARING AGAINST tools/tournament.py\n"
          + "  If the folder contains an UNPAIRED warrior — a lone file ending in 1 or 2,\n"
          + "  or any file that has no partner — this window and tournament.py will\n"
          + "  disagree.  tournament.py's discover_groups lists paired warriors first and\n"
          + "  solo warriors last; the engine (here AND in the Tourney command line) keeps\n"
          + "  plain alphabetical order.  Different order = different field indices =\n"
          + "  a genuinely different tournament.  Neither is wrong; they are not the same\n"
          + "  question.  Folders with no solo warriors agree to the last float32 digit.\n"
          + "\n"
          + "PREVIEW vs FINAL — this matters\n"
          + "  The engine runs battles in parallel, but its score accumulation is\n"
          + "  ORDER-DEPENDENT ON PURPOSE: the float32 totals are replayed sequentially in\n"
          + "  BATTLE order once every worker has finished, and that is exactly why the\n"
          + "  thread count cannot change the answer.\n"
          + "  A live bar necessarily adds scores in COMPLETION order instead, which is a\n"
          + "  different order, and float32 addition is not associative.  So while a run is\n"
          + "  in flight the bars are a PREVIEW (accumulated in double).  When it ends they\n"
          + "  are replaced by the canonical result, and that is what goes into scores.csv.\n"
          + "  The footer reports how far the preview had drifted.\n"
          + "\n"
          + "STOP\n"
          + "  Stopping replays the longest unbroken run of finished battles, 1..N — so a\n"
          + "  stopped run gives exactly the numbers a full run of N battles would have\n"
          + "  given, not a ragged sample.\n"
          + "  After a STOP the preview-drift figure disappears from the footer.  That is\n"
          + "  because the preview also counted battles past the replayed prefix, so the\n"
          + "  two totals are not comparable — the drift is UNMEASURED, not zero.\n"
          + "\n"
          + "OUTPUT\n"
          + "  scores.csv is written next to this window's folder, in the same layout the\n"
          + "  official tool writes, so every existing script reads it unchanged.\n"
          + "  last-run.txt beside it records the configuration and the reproducing\n"
          + "  command line.\n";
        if (!lastProblems.isEmpty()) s += "\nINPUT NOTES FROM THE LAST RUN\n  " + lastProblems.replace("\n", "\n  ") + "\n";
        JTextArea ta = new JTextArea(s);
        ta.setEditable(false);
        ta.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 12));
        JScrollPane sp = new JScrollPane(ta);
        sp.setPreferredSize(new Dimension(720, 520));
        JOptionPane.showMessageDialog(this, sp, "CGX Tournament — help", JOptionPane.INFORMATION_MESSAGE);
    }

    private void shutdown() {
        GuiTourney r = runner;
        if (r != null) r.stop();          // battle threads are daemons anyway
        setVisible(false);
        dispose();
        System.exit(0);
    }

    private static String fmtInt(long v) { return String.format("%,d", v); }

    private static String clock(double secs) {
        if (secs < 60) return String.format("%.1f s", secs);
        long m = (long) (secs / 60);
        return String.format("%d m %02.0f s", m, secs - m * 60);
    }

    // ------------------------------------------------------------------ main

    public static void main(String[] args) {
        File guiDir = new File(System.getProperty("cgx.gui.dir",
                               System.getProperty("user.dir", "."))).getAbsoluteFile();
        File survivors = new File(guiDir, "survivors");
        File zombies   = new File(guiDir, "zombies");
        for (int i = 0; i < args.length; i++) {
            if (("-w".equals(args[i]) || "--warriors".equals(args[i])) && i + 1 < args.length)
                survivors = new File(args[++i]).getAbsoluteFile();
            else if (("-z".equals(args[i]) || "--zombies".equals(args[i])) && i + 1 < args.length)
                zombies = new File(args[++i]).getAbsoluteFile();
        }
        survivors.mkdirs();
        final File fs = survivors, fz = zombies, fg = guiDir;

        SwingUtilities.invokeLater(() -> {
            try { UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName()); }
            catch (Exception ignored) { }
            new CgxGuiFrame(fg, fs, fz).setVisible(true);
        });
    }
}
