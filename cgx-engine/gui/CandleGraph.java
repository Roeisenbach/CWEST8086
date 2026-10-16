import javax.swing.JComponent;
import javax.swing.ToolTipManager;
import java.awt.BasicStroke;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.RenderingHints;
import java.awt.event.MouseEvent;

/**
 * The candles.  One vertical bar per group, colour per group (per school,
 * really -- see GroupColors), the place number in gold above each bar, and the
 * group name written up the bottom band.
 *
 * Matched to the FEEL of il.co.codeguru.corewars8086.gui.ColumnGraph, not to
 * its code: black field, 3D-shaded columns, score printed just above the bar
 * top, gold rank numeral above that, name rotated 90 degrees.  What is
 * different, and deliberately so, because the brief asks for a 22-group field
 * to be readable:
 *
 *   - bars are SORTED BY SCORE, leader on the left, so the standing is the
 *     shape of the picture rather than something you have to read off;
 *   - EVERY group gets a place number, not just the top half;
 *   - heights, the vertical scale and the horizontal slot positions are all
 *     eased frame to frame, so a group overtaking another slides past it
 *     instead of teleporting;
 *   - the scale grows and shrinks continuously instead of halving.
 *
 * The values it is handed are PREVIEW values while a run is in flight and
 * CANONICAL values once it finishes; it does not care which, and it never
 * computes a score itself.  setFinal() only changes the label at the top.
 */
public final class CandleGraph extends JComponent {

    private static final long serialVersionUID = 1L;

    private static final Color BG        = Color.BLACK;
    private static final Color GOLD      = new Color(255, 215, 0);
    private static final Color AXIS      = new Color(60, 60, 60);
    private static final Color NOTE      = new Color(150, 150, 150);
    private static final Color NOTE_LIVE = new Color(120, 190, 255);
    private static final Color NOTE_FIN  = new Color(120, 235, 140);

    private String[] names   = new String[0];
    private double[] target  = new double[0];   // where each bar is heading
    private double[] shown   = new double[0];   // where each bar is now
    private double[] slotNow = new double[0];   // animated horizontal slot
    private int[]    rank    = new int[0];      // 1-based place per group
    private int[]    order   = new int[0];      // group indices, best first
    private GroupColors colors = new GroupColors(new String[0]);

    private double scaleTarget = 1, scaleNow = 1;
    private boolean finalMode = false;
    private String  headline  = "";

    /** last painted bar rectangles, for tooltips */
    private int[] hitX = new int[0], hitW = new int[0];

    public CandleGraph() {
        setOpaque(true);
        setPreferredSize(new Dimension(900, 520));
        setMinimumSize(new Dimension(420, 260));
        ToolTipManager.sharedInstance().registerComponent(this);
    }

    /** New field.  Resets every animation to zero so the run visibly starts empty. */
    public void setGroups(String[] groupNames) {
        int n = groupNames.length;
        names   = groupNames.clone();
        target  = new double[n];
        shown   = new double[n];
        slotNow = new double[n];
        rank    = new int[n];
        order   = new int[n];
        hitX    = new int[n];
        hitW    = new int[n];
        for (int i = 0; i < n; i++) { slotNow[i] = i; order[i] = i; rank[i] = i + 1; }
        colors = new GroupColors(groupNames);
        scaleTarget = scaleNow = 1;
        finalMode = false;
        recomputeOrder();
        repaint();
    }

    public void setHeadline(String s) { headline = s == null ? "" : s; }

    /** Live preview values.  Animated. */
    public void setValues(double[] v) {
        if (v.length != target.length) return;
        System.arraycopy(v, 0, target, 0, v.length);
        recomputeOrder();
        repaint();
    }

    /** Canonical values.  Animated to, then held; flips the header label. */
    public void setFinalValues(double[] v) {
        finalMode = true;
        setValues(v);
    }

    public boolean isFinalMode() { return finalMode; }
    public int[] orderBestFirst() { return order.clone(); }
    public int[] ranks() { return rank.clone(); }
    public GroupColors colors() { return colors; }

    /**
     * One animation step.  Driven by the window's Swing timer rather than an
     * internal one, so there is exactly one timer in the process and it stops
     * when the window stops caring.  Returns true if anything moved.
     */
    public boolean tick() {
        boolean moving = false;
        final double EASE = 0.28;

        // HEIGHTS are eased -- that is the motion the eye follows.
        //
        // HORIZONTAL SLOTS ARE NOT.  Sliding a bar past its neighbour looks
        // lovely in a bar-chart race and is wrong here: mid-slide the two bars
        // occupy the same column, and their names, scores and place numbers
        // print on top of each other.  At 22 groups early in a run the order
        // churns on nearly every frame, so that smear would be the normal state
        // of the window rather than a rare artefact.  Readability at ~22 groups
        // is the requirement; an overtake is therefore a clean swap.
        for (int i = 0; i < shown.length; i++) {
            double d = target[i] - shown[i];
            if (Math.abs(d) > 1e-9) { shown[i] += d * EASE; moving = true; }
            for (int j = 0; j < order.length; j++)
                if (order[j] == i) { if (slotNow[i] != j) { slotNow[i] = j; moving = true; } break; }
        }
        double dsc = scaleTarget - scaleNow;
        if (Math.abs(dsc) > 1e-9) { scaleNow += dsc * EASE; moving = true; }
        if (moving) repaint();
        return moving;
    }

    private void recomputeOrder() {
        int n = target.length;
        Integer[] idx = new Integer[n];
        for (int i = 0; i < n; i++) idx[i] = i;
        // score descending; ties broken by group name so the picture is stable
        java.util.Arrays.sort(idx, (a, b) -> {
            int c = Double.compare(target[b], target[a]);
            if (c != 0) return c;
            return names[a].compareToIgnoreCase(names[b]);
        });
        for (int i = 0; i < n; i++) order[i] = idx[i];
        // standard competition ranking: place = 1 + how many scored strictly more
        for (int i = 0; i < n; i++) {
            int better = 0;
            for (int j = 0; j < n; j++) if (target[j] > target[i]) better++;
            rank[i] = better + 1;
        }
    }

    // ------------------------------------------------------------------ paint

    @Override
    protected void paintComponent(Graphics g0) {
        Graphics2D g = (Graphics2D) g0.create();
        try {
            g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING,
                               RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
            g.setRenderingHint(RenderingHints.KEY_ANTIALIASING,
                               RenderingHints.VALUE_ANTIALIAS_ON);

            int W = getWidth(), H = getHeight();
            g.setColor(BG);
            g.fillRect(0, 0, W, H);

            int n = names.length;
            if (n == 0) {
                g.setColor(NOTE);
                g.setFont(new Font(Font.SANS_SERIF, Font.PLAIN, 15));
                String msg = "Drop warrior files into the survivors folder, then press START.";
                FontMetrics fm = g.getFontMetrics();
                g.drawString(msg, (W - fm.stringWidth(msg)) / 2, H / 2);
                return;
            }

            // --- header line
            int headH = 0;
            if (!headline.isEmpty()) {
                g.setFont(new Font(Font.SANS_SERIF, Font.BOLD, 12));
                g.setColor(finalMode ? NOTE_FIN : NOTE_LIVE);
                g.drawString(headline, 8, 16);
                headH = 24;
            }

            // --- side margins.  Without these the leader's score and place
            //     numeral sit flush against the left edge of the panel with no
            //     air at all, which reads like the label has been clipped.
            final int PADL = 10, PADR = 6;
            int plotW = Math.max(n * 6, W - PADL - PADR);

            // --- how much room the rotated names need at the bottom
            int colW = Math.max(6, plotW / n);
            int nameFont = clamp((int) (colW * 0.62), 8, 14);
            g.setFont(new Font(Font.SANS_SERIF, Font.BOLD, nameFont));
            FontMetrics nfm = g.getFontMetrics();
            int longest = 0;
            for (String s : names) longest = Math.max(longest, nfm.stringWidth(s));
            int nameBand = clamp(longest + 10, 30, Math.max(40, (int) (H * 0.42)));

            int base = H - nameBand;                 // y of the baseline
            int barTop = headH + 6;
            int barArea = Math.max(20, base - barTop);

            // --- vertical scale, eased.  85% of the bar area for the leader.
            double maxV = 0;
            for (double v : target) maxV = Math.max(maxV, v);
            scaleTarget = maxV <= 1e-12 ? 1 : (barArea * 0.85) / maxV;
            if (scaleNow <= 0) scaleNow = scaleTarget;

            // --- baseline
            g.setColor(AXIS);
            g.setStroke(new BasicStroke(1f));
            g.drawLine(0, base, W, base);

            int gap = colW >= 18 ? 3 : (colW >= 10 ? 2 : 1);
            int bw  = Math.max(2, colW - gap);

            Font nameF = new Font(Font.SANS_SERIF, Font.BOLD, nameFont);

            // paint right-to-left so the leader's labels sit on top of its neighbours'
            for (int slot = n - 1; slot >= 0; slot--) {
                int gi = order[slot];
                int x = PADL + (int) Math.round(slotNow[gi] * colW) + gap / 2;
                int h = (int) Math.round(Math.max(0, shown[gi]) * scaleNow);
                if (h > barArea) h = barArea;
                int y = base - h;

                hitX[gi] = x; hitW[gi] = bw;

                // the candle
                g.setColor(colors.get(gi));
                g.fill3DRect(x, y, bw, Math.max(1, h), true);
                g.setColor(colors.darker(gi));
                g.drawRect(x, y, bw, Math.max(1, h));

                // score, just above the bar top.  The point size is derived from
                // the string LENGTH, not just the column width, so "1234.5" over
                // a narrow column shrinks instead of running into its neighbour.
                //
                // The label is drawn in colors.label(gi) -- the group's hue
                // LIFTED toward white -- not in the bar's own colour.  A dark
                // blue or dark green bar's true colour is close to unreadable
                // as 9 pt text on a black field.
                String sv = fmt(target[gi]);
                g.setFont(new Font(Font.SANS_SERIF, Font.BOLD,
                                   clamp((int) (bw * 1.9 / Math.max(1, sv.length())), 9, 13)));
                FontMetrics sfm = g.getFontMetrics();
                int svw = sfm.stringWidth(sv);
                int sy = Math.max(barTop + sfm.getAscent(), y - 4);
                if (svw <= colW - 2) {      // never let two scores touch
                    g.setColor(colors.label(gi));
                    g.drawString(sv, x + (bw - svw) / 2, sy);
                }

                // the place number, in gold, above the score.  Same rule: a
                // two-digit place gets a smaller numeral so it still fits its
                // own column, which matters a lot when two neighbouring bars are
                // nearly level and their numerals sit at the same height.
                // A SHARED place gets a trailing '=' (the league-table
                // convention).  Two zero-height bars both numbered 21, with no
                // 22 anywhere, is correct standard competition ranking and
                // reads exactly like a rendering fault without this mark.
                String rk = String.valueOf(rank[gi]) + (isTied(gi) ? "=" : "");
                g.setFont(new Font(Font.MONOSPACED, Font.BOLD,
                                   clamp((int) (bw * 1.5 / rk.length()),
                                         9, Math.max(11, Math.min(30, colW)))));
                FontMetrics rfm = g.getFontMetrics();
                int ry = Math.max(barTop + rfm.getAscent(), sy - sfm.getHeight() - 1);
                g.setColor(GOLD);
                g.drawString(rk, x + (bw - rfm.stringWidth(rk)) / 2, ry);

                // the group name, written up the bottom band -- lifted colour
                // again, for the same reason the score is
                g.setFont(nameF);
                g.setColor(colors.label(gi));
                drawRotated(g, x + bw / 2 + nameFont / 2 - 1, base + 5, names[gi], nameBand - 8);
            }
        } finally {
            g.dispose();
        }
    }

    /** name drawn reading bottom-to-top, clipped to maxLen pixels with an ellipsis */
    private void drawRotated(Graphics2D g, int x, int y, String text, int maxLen) {
        FontMetrics fm = g.getFontMetrics();
        String s = text;
        if (fm.stringWidth(s) > maxLen) {
            while (s.length() > 1 && fm.stringWidth(s + "…") > maxLen) s = s.substring(0, s.length() - 1);
            s = s + "…";
        }
        g.translate(x, y);
        g.rotate(Math.PI / 2);
        g.drawString(s, 0, 0);
        g.rotate(-Math.PI / 2);
        g.translate(-x, -y);
    }

    private static String fmt(double v) {
        if (v >= 1000) return String.format("%.0f", v);
        if (v >= 100)  return String.format("%.1f", v);
        return String.format("%.2f", v);
    }

    private static int clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

    /** true if another group holds the same place (standard competition ranking) */
    public boolean isTied(int gi) {
        for (int j = 0; j < rank.length; j++) if (j != gi && rank[j] == rank[gi]) return true;
        return false;
    }

    @Override
    public String getToolTipText(MouseEvent e) {
        for (int gi = 0; gi < names.length; gi++)
            if (e.getX() >= hitX[gi] && e.getX() <= hitX[gi] + hitW[gi])
                return "#" + rank[gi] + (isTied(gi) ? " (tied — shared place, next place skipped)" : "")
                        + "  " + names[gi] + "  =  " + fmt(target[gi])
                        + (finalMode ? "  (final)" : "  (live preview)");
        return null;
    }
}
