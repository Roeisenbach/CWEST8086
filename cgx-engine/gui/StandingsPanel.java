import javax.swing.JComponent;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.FontMetrics;
import java.awt.Graphics;
import java.awt.Graphics2D;
import java.awt.RenderingHints;

/**
 * The same standing as the candles, in words: place, colour chip, group name,
 * score.  A 22-group field's bars are readable but its rotated names are small,
 * and "which one is 7th" should not require counting columns.
 *
 * Purely a view of numbers handed to it.  Never computes a score.
 */
public final class StandingsPanel extends JComponent {

    private static final long serialVersionUID = 1L;

    private static final Color BG   = new Color(18, 18, 18);
    private static final Color GOLD = new Color(255, 215, 0);
    private static final Color TXT  = new Color(228, 228, 228);
    private static final Color DIM  = new Color(130, 130, 130);
    private static final Color RULE = new Color(45, 45, 45);

    private String[] names  = new String[0];
    private double[] values = new double[0];
    private int[]    order  = new int[0];
    private int[]    rank   = new int[0];
    private GroupColors colors = new GroupColors(new String[0]);
    private boolean finalMode = false;

    public StandingsPanel() {
        setOpaque(true);
        setPreferredSize(new Dimension(250, 400));
        setMinimumSize(new Dimension(170, 120));
    }

    public void setGroups(String[] groupNames, GroupColors c) {
        names  = groupNames.clone();
        values = new double[names.length];
        order  = new int[names.length];
        rank   = new int[names.length];
        for (int i = 0; i < names.length; i++) { order[i] = i; rank[i] = i + 1; }
        colors = c;
        finalMode = false;
        repaint();
    }

    public void update(double[] v, int[] orderBestFirst, int[] ranks, boolean isFinal) {
        if (v.length != names.length) return;
        values = v.clone();
        order  = orderBestFirst.clone();
        rank   = ranks.clone();
        finalMode = isFinal;
        repaint();
    }

    @Override
    protected void paintComponent(Graphics g0) {
        Graphics2D g = (Graphics2D) g0.create();
        try {
            g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING,
                               RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
            int W = getWidth(), H = getHeight();
            g.setColor(BG);
            g.fillRect(0, 0, W, H);

            int n = names.length;
            g.setFont(new Font(Font.SANS_SERIF, Font.BOLD, 11));
            g.setColor(DIM);
            boolean anyTie = false;
            for (int i = 0; i < names.length && !anyTie; i++) anyTie = isTied(i);
            g.drawString((finalMode ? "STANDING  (final)" : "STANDING  (live preview)")
                    + (anyTie ? "   = tie" : ""), 8, 15);
            g.setColor(RULE);
            g.drawLine(6, 21, W - 6, 21);

            if (n == 0) return;

            int top = 26;
            int rowH = Math.max(11, Math.min(22, (H - top - 4) / n));
            int font = Math.max(9, Math.min(14, rowH - 5));
            Font rowF = new Font(Font.SANS_SERIF, Font.PLAIN, font);
            Font numF = new Font(Font.MONOSPACED, Font.BOLD, font);

            for (int slot = 0; slot < n; slot++) {
                int gi = order[slot];
                int y = top + slot * rowH;
                if (y + rowH > H) break;
                int baseline = y + rowH - Math.max(2, (rowH - font) / 2);

                if ((slot & 1) == 1) {
                    g.setColor(new Color(26, 26, 26));
                    g.fillRect(0, y, W, rowH);
                }

                // Standard competition ranking: groups on the same score SHARE a
                // place and the next place is skipped, so a 22-group field whose
                // last two both score 0 ends "21, 21" with no 22.  That is
                // correct and it looks like a bug, so a shared place is marked
                // with '=' -- the usual league-table convention.
                g.setFont(numF);
                FontMetrics nfm = g.getFontMetrics();
                String rk = String.valueOf(rank[gi]) + (isTied(gi) ? "=" : "");
                g.setColor(GOLD);
                g.drawString(rk, 26 - nfm.stringWidth(rk), baseline);

                g.setColor(colors.get(gi));
                g.fillRect(30, y + (rowH - font) / 2 + 1, font - 2, font - 2);

                g.setFont(rowF);
                FontMetrics rfm = g.getFontMetrics();
                String sv = fmt(values[gi]);
                int svW = rfm.stringWidth(sv);
                int nameX = 30 + font + 4;
                int nameMax = W - 8 - svW - 6 - nameX;
                String nm = names[gi];
                if (rfm.stringWidth(nm) > nameMax) {
                    while (nm.length() > 1 && rfm.stringWidth(nm + "…") > nameMax)
                        nm = nm.substring(0, nm.length() - 1);
                    nm = nm + "…";
                }
                g.setColor(TXT);
                g.drawString(nm, nameX, baseline);
                g.setColor(finalMode ? TXT : DIM);
                g.drawString(sv, W - 8 - svW, baseline);
            }
        } finally {
            g.dispose();
        }
    }

    /** true if another group holds the same place */
    private boolean isTied(int gi) {
        for (int j = 0; j < rank.length; j++) if (j != gi && rank[j] == rank[gi]) return true;
        return false;
    }

    private static String fmt(double v) {
        if (v >= 1000) return String.format("%.1f", v);
        return String.format("%.2f", v);
    }
}
