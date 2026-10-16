import java.io.*;
import java.nio.file.*;
import java.util.*;

/**
 * ZombieSelf - if our own zombies carry our own survivors' code, does anybody die?
 *
 * THE QUESTION. In CGX you submit survivors AND zombies. Zombies run
 * zombieSpeed x (filename ends in 'h' ? 2 : 1) opcodes per turn - the only
 * mechanism in the game that exceeds one opcode per turn, since a survivor is
 * capped at two and cannot reach even that. Last year the user's own zombie and
 * survivors killed each other. The proposal is to hand the zombies the
 * survivors' code so the whole submission is one family.
 *
 * WHY IT IS NOT OBVIOUS EITHER WAY:
 *  FOR  - the champion is already immune to its own family's trail. The
 *         TempoProbe self-collision control FAILED to fire: champ1 vs champ1 gave
 *         0% deaths, because the trail cell starts A5 = MOVSW, which is a REBUILD
 *         TRIGGER, not a kill. Same-family fire heals.
 *  AGAINST - every zombie in the battle is forced into ONE hardcoded group named
 *         "ZoMbIeS" (Tourney.zombieGroup), so a zombie cannot read OUR group's
 *         shared memory. The handshake that keeps champ2 a safe distance from
 *         champ1 - m1 publishes its load offset with stosw on turn 1, m2 reads it
 *         with lodsw - is unavailable across that boundary. Survivor pair and
 *         zombie pair therefore land at INDEPENDENT random offsets, which is
 *         exactly the configuration the 0x25->0x33 fix was made to eliminate
 *         inside the survivor pair, where it was killing one battle in 25.
 *  AND    - relocation is the real hazard, not the trail. A rebuild that lands on
 *         top of live bytes overwrites them whatever family wrote it.
 *
 * So: run it alone - our two survivors plus our two zombies, no opponents at all -
 * and count deaths. Alone is the right test: it isolates self-inflicted death from
 * everything an opponent could do.
 *
 * Usage: java ZombieSelf <rounds> <trials> <zombieSpeed> <s1> <s2> [z1] [z2]
 *        z1/z2 default to s1/s2. Append 'h' to a zombie NAME (not the path) to
 *        make it type 2 - handled by the --h1/--h2 flags below.
 */
public final class ZombieSelf {

    public static void main(String[] args) throws Exception {
        List<String> a = new ArrayList<>(Arrays.asList(args));
        boolean h1 = a.remove("--h1"), h2 = a.remove("--h2");

        int rounds = Integer.parseInt(a.get(0));
        int trials = Integer.parseInt(a.get(1));
        int zspeed = Integer.parseInt(a.get(2));
        byte[] s1 = read(a.get(3)), s2 = read(a.get(4));
        byte[] z1 = a.size() > 5 ? read(a.get(5)) : s1;
        byte[] z2 = a.size() > 6 ? read(a.get(6)) : s2;

        System.out.printf("survivors %d+%d B, zombies %d+%d B, zombieSpeed %d "
                        + "(z1 type %d = %dx, z2 type %d = %dx), alone, %d rounds x %d trials%n%n",
                s1.length, s2.length, z1.length, z2.length, zspeed,
                h1 ? 2 : 1, zspeed * (h1 ? 2 : 1), h2 ? 2 : 1, zspeed * (h2 ? 2 : 1),
                rounds, trials);

        int sDied = 0, zDied = 0, cleanRuns = 0;
        long roundsRun = 0;
        Map<String, Integer> firstDeath = new TreeMap<>();

        for (int t = 0; t < trials; t++) {
            EngGroup mine = new EngGroup("MINE", new String[]{"m1", "m2"},
                    new byte[][]{s1, s2}, new int[]{0, 0});
            // Every zombie in a battle is ONE group, whoever submitted it.
            EngGroup zom = new EngGroup("ZoMbIeS", new String[]{"z1", "z2"},
                    new byte[][]{z1, z2}, new int[]{h1 ? 2 : 1, h2 ? 2 : 1});

            EngWar war = new EngWar(zspeed < 0 ? 0 : zspeed, null);
            war.setSeed(3184561 + t);
            // zombieSpeed < 0 means NO ZOMBIE GROUP AT ALL - the true control.
            // Passing speed 0 is NOT a control: the zombies still load, they just
            // run one opcode a turn like everyone else, so their bodies are still
            // in the arena and still collide. That mistake made the first control
            // report ten zombie deaths in a run that was supposed to have none.
            war.loadWarriorGroups(zspeed < 0 ? new EngGroup[]{mine}
                                             : new EngGroup[]{mine, zom});

            boolean[] wasAlive = new boolean[war.numWarriors];
            for (int i = 0; i < war.numWarriors; i++) wasAlive[i] = war.warriors[i].alive;

            // NOT `!war.isOver()`. With one survivor group isOver() is TRUE at
            // round 0 - `numSurvivorsAlive == 2 && countAliveSurvivorGroups() == 1`
            // holds the moment we load - so an isOver-guarded loop runs ZERO rounds
            // and reports a clean sheet without simulating anything. Run the full
            // count and stop only if our own side is actually gone.
            int ran = 0;
            for (int r = 0; r < rounds; r++) {
                if (war.numSurvivorsAlive == 0) break;
                ran++;
                war.nextRound(r);
                for (int i = 0; i < war.numWarriors; i++) {
                    if (wasAlive[i] && !war.warriors[i].alive) {
                        wasAlive[i] = false;
                        String who = war.warriors[i].name;
                        firstDeath.merge(who + " @r" + bucket(r), 1, Integer::sum);
                        if (war.warriors[i].isZombie()) zDied++; else sDied++;
                    }
                }
            }
            roundsRun += ran;
            boolean clean = true;
            for (int i = 0; i < war.numWarriors; i++) if (!war.warriors[i].alive) clean = false;
            if (clean) cleanRuns++;
        }

        System.out.printf("rounds actually simulated: %,d (%,d per trial)%n", roundsRun, roundsRun / trials);
        System.out.printf("trials with NOBODY dead : %d / %d%n", cleanRuns, trials);
        System.out.printf("survivor deaths         : %d%n", sDied);
        System.out.printf("zombie deaths           : %d%n", zDied);
        if (!firstDeath.isEmpty()) {
            System.out.println("\nwho died, and when:");
            firstDeath.forEach((k, v) -> System.out.printf("   %-22s %d%n", k, v));
        }
    }

    static String bucket(int r) {
        if (r < 100) return "<100";
        if (r < 1000) return "100-1k";
        if (r < 10000) return "1k-10k";
        return ">10k";
    }

    static byte[] read(String p) throws IOException {
        byte[] b = Files.readAllBytes(Paths.get(p));
        return b.length > Field.MAX_WARRIOR_SIZE ? Arrays.copyOf(b, Field.MAX_WARRIOR_SIZE) : b;
    }
}
