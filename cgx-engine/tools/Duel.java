// Duel - trace a battle with MORE THAN ONE group in the arena.
//
// The engine's own Dump runs exactly one group, which is what the solo gate
// needs and useless for the only question that matters once a warrior works:
// what is the thing that kills it. This calls EngRunner directly with as many
// groups as you name, and writes the same trace format, so watch.py and
// writes.py read its output unchanged.
//
//   javac -cp "<engine>;<jar>" -d tools/build tools/Duel.java
//   java  -cp "<engine>;<jar>;tools/build" Duel --dir <field> \
//         --groups A,B --out trace --max-round 4000 --seed 1 --full
//
// --isover stops when one group is left, which is what a real battle does.
// Without it the battle runs the full round count, so a warrior that dies at
// round 900 still shows the rest of the field carrying on around its corpse.
import java.io.*;
import java.util.*;

public final class Duel {
    public static void main(String[] args) throws Exception {
        String dir = null, out = "duel", groupsArg = null, zdir = null;
        int maxRound = 4000, zombieSpeed = 2;
        long seed = 1;
        boolean full = false, isOver = false;

        for (int i = 0; i < args.length; i++) {
            switch (args[i]) {
                case "--dir":       dir = args[++i]; break;
                case "--out":       out = args[++i]; break;
                case "--groups":    groupsArg = args[++i]; break;
                case "--zombies":   zdir = args[++i]; break;
                case "--max-round": maxRound = Integer.parseInt(args[++i]); break;
                case "--seed":      seed = Long.parseLong(args[++i]); break;
                case "--zombie-speed": zombieSpeed = Integer.parseInt(args[++i]); break;
                case "--full":      full = true; break;
                case "--isover":    isOver = true; break;
                default: throw new IllegalArgumentException(args[i]);
            }
        }
        if (dir == null) throw new IllegalArgumentException("need --dir");

        List<String> warn = new ArrayList<>();
        List<EngGroup> all = Field.groupsOf(new File(dir), warn);

        List<EngGroup> pick = new ArrayList<>();
        if (groupsArg == null) {
            pick.addAll(all);
        } else {
            for (String want : groupsArg.split(",")) {
                EngGroup hit = null;
                for (EngGroup g : all) if (g.name.equals(want.trim())) hit = g;
                if (hit == null) {
                    StringBuilder sb = new StringBuilder();
                    for (EngGroup g : all) sb.append(g.name).append(' ');
                    throw new IllegalArgumentException(
                        "no group '" + want.trim() + "' in " + dir + "; have: " + sb);
                }
                pick.add(hit);
            }
        }
        // Zombies are not warriors: they need Tourney's own constructor so
        // they get the zombie type, the fixed instructions per turn and no
        // score. A field without them is not the game - measured on
        // cgx2024/05-final2, adding them alone moved one warrior from 3rd to
        // 7th and another from 7th to 3rd.
        if (zdir != null) {
            // zombieGroup takes the DIRECTORY: every zombie in the field is
            // one group under one fixed name, and none of them scores.
            EngGroup zg = Tourney.zombieGroup(new File(zdir));
            if (zg == null)
                throw new IllegalArgumentException("not a directory: " + zdir);
            pick.add(zg);
        }
        EngGroup[] arr = pick.toArray(new EngGroup[0]);

        StringBuilder names = new StringBuilder();
        for (EngGroup g : arr) names.append(g.name).append(' ');
        System.out.println("groups: " + names.toString().trim());

        float[] scores = new float[countWarriors(arr)];
        // suppress is the inverse of --full: the engine writes per-warrior
        // state and death records only when it is off.
        try (OutputStream os = new FileOutputStream(out + ".eng.txt")) {
            int end = EngRunner.run(os, arr, seed, zombieSpeed, maxRound,
                                    !full, isOver, "engine", scores);
            System.out.println("endRound=" + end);
        }

        int k = 0;
        for (EngGroup g : arr)
            for (String w : g.warriorNames)
                System.out.printf("  %-32s %.4f%n", w, scores[k++]);
        System.out.println("wrote " + out + ".eng.txt");
    }

    static int countWarriors(EngGroup[] gs) {
        int n = 0;
        for (EngGroup g : gs) n += g.size();
        return n;
    }
}
