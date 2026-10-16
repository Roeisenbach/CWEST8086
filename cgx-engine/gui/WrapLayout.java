import java.awt.Component;
import java.awt.Container;
import java.awt.Dimension;
import java.awt.FlowLayout;
import java.awt.Insets;
import javax.swing.JScrollPane;
import javax.swing.SwingUtilities;

/**
 * FlowLayout that reports the height it will actually need once it has wrapped.
 *
 * Plain FlowLayout wraps its rows but still reports a single row's height, so a
 * toolbar in a BorderLayout.NORTH silently CLIPS its second line when the window
 * gets narrow -- which is how the START button disappears at 800 px wide.  This
 * makes the toolbar grow taller instead, so every control stays reachable at any
 * window size the user drags to.
 */
public final class WrapLayout extends FlowLayout {

    private static final long serialVersionUID = 1L;

    public WrapLayout(int align, int hgap, int vgap) { super(align, hgap, vgap); }

    @Override public Dimension preferredLayoutSize(Container target) { return layoutSize(target, true); }
    @Override public Dimension minimumLayoutSize(Container target) {
        Dimension d = layoutSize(target, false);
        d.width -= (getHgap() + 1);
        return d;
    }

    private Dimension layoutSize(Container target, boolean preferred) {
        synchronized (target.getTreeLock()) {
            int targetWidth = target.getSize().width;
            if (targetWidth == 0) targetWidth = Integer.MAX_VALUE;

            int hgap = getHgap(), vgap = getVgap();
            Insets in = target.getInsets();
            int horizontalInsets = in.left + in.right + hgap * 2;
            int maxWidth = targetWidth - horizontalInsets;

            Dimension dim = new Dimension(0, 0);
            int rowWidth = 0, rowHeight = 0;

            for (int i = 0; i < target.getComponentCount(); i++) {
                Component m = target.getComponent(i);
                if (!m.isVisible()) continue;
                Dimension d = preferred ? m.getPreferredSize() : m.getMinimumSize();
                if (rowWidth + d.width > maxWidth && rowWidth != 0) {
                    dim.width = Math.max(dim.width, rowWidth);
                    dim.height += rowHeight + vgap;
                    rowWidth = 0;
                    rowHeight = 0;
                }
                if (rowWidth != 0) rowWidth += hgap;
                rowWidth += d.width;
                rowHeight = Math.max(rowHeight, d.height);
            }
            dim.width = Math.max(dim.width, rowWidth);
            dim.height += rowHeight;

            dim.width += horizontalInsets;
            dim.height += in.top + in.bottom + vgap * 2;

            // a wrapping toolbar inside a viewport needs one extra pixel of slack
            Container scrollPane = SwingUtilities.getAncestorOfClass(JScrollPane.class, target);
            if (scrollPane != null && target.isValid()) dim.width -= (hgap + 1);
            return dim;
        }
    }
}
