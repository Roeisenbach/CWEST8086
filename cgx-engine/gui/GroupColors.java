import java.awt.Color;
import java.util.HashMap;
import java.util.Map;

/**
 * Colour per group, matching the official ColumnGraph's convention so the
 * picture reads the same way to someone who has watched the jar run:
 *
 *   - the key is the group name's first 3 characters, upper-cased, so every
 *     team from the same school gets ONE colour (HRZ_SHRek, HRZ_JMP2HELL and
 *     HRZ_Code_Killers are all the same hue);
 *   - hues are walked by the golden-ratio conjugate, saturation 0.8,
 *     brightness 0.95 -- the same recipe TeamColorHolder uses, so adjacent
 *     schools never land on neighbouring hues;
 *   - assignment order is group order, which is the sorted file order, so the
 *     colours are stable from run to run for the same folder.
 */
public final class GroupColors {

    private static final int   PREFIX_LEN = 3;
    private static final float SATURATION = 0.8f;
    private static final float BRIGHTNESS = 0.95f;
    private static final float GOLDEN     = 0.618033988749895f;

    private final Map<String, Color> byPrefix = new HashMap<>();
    private final Color[] perGroup;

    public GroupColors(String[] groupNames) {
        perGroup = new Color[groupNames.length];
        float x = 0;
        for (int i = 0; i < groupNames.length; i++) {
            String key = prefix(groupNames[i]);
            Color c = byPrefix.get(key);
            if (c == null) {
                c = Color.getHSBColor(x % 1f, SATURATION, BRIGHTNESS);
                byPrefix.put(key, c);
                x += GOLDEN;
            }
            perGroup[i] = c;
        }
    }

    private static String prefix(String name) {
        return name.substring(0, Math.min(PREFIX_LEN, name.length())).toUpperCase();
    }

    public Color get(int groupIndex) {
        if (groupIndex < 0 || groupIndex >= perGroup.length) return Color.GRAY;
        return perGroup[groupIndex];
    }

    /**
     * The same hue, lifted far enough to be legible as small text on black.
     *
     * The candle itself must stay the group's true colour, but a 7-12 pt score
     * printed in that colour is unreadable when the golden-ratio walk lands on
     * a dark blue, a dark purple or a dark green -- exactly the hues that sit
     * closest to the black field.  So the LABEL is a lightened version: blended
     * ~62% toward white, which keeps it recognisably the same colour as its bar
     * while putting it well clear of the background.
     */
    public Color label(int groupIndex) {
        Color c = get(groupIndex);
        final float k = 0.62f;
        return new Color(mix(c.getRed(), k), mix(c.getGreen(), k), mix(c.getBlue(), k));
    }

    private static int mix(int v, float k) {
        return Math.min(255, Math.round(v + (255 - v) * k));
    }

    public Color darker(int groupIndex) {
        Color c = get(groupIndex);
        return new Color(Math.max(0, (int) (c.getRed()   * 0.55)),
                         Math.max(0, (int) (c.getGreen() * 0.55)),
                         Math.max(0, (int) (c.getBlue()  * 0.55)));
    }

    public int size() { return perGroup.length; }
}
