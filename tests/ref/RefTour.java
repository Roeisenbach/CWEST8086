// Reference-engine mirror of tests/tourdiff.cpp: runs a whole TOURNAMENT --
// many wars, each on its own combination of groups -- through the real
// il.co.codeguru.corewars8086 classes, and prints one digest line per war so
// the C++ engine can be diffed against it in bulk.
//
// RefWar checks one fight in detail; this checks that thousands of fights, the
// group draw, the loading and the scoring all still agree.
//
//   javac -d classes -cp "classes;<jar>" RefTour.java
//   java  -cp "classes;<jar>" RefTour <survDir> <zombDir|""> <games> <combo> <zspeed> <firstSeed>

import il.co.codeguru.corewars8086.war.*;
import il.co.codeguru.corewars8086.memory.MemoryEventListener;
import il.co.codeguru.corewars8086.memory.RealModeAddress;
import il.co.codeguru.corewars8086.cli.Options;
import com.google.devtools.common.options.OptionsParser;

import java.io.*;
import java.nio.file.*;
import java.util.*;

public class RefTour {
    static final int MAX_WARRIOR_SIZE = 512;
    static final long FNV_OFF = -3750763034362895579L;
    static final long FNV_PRIME = 1099511628211L;

    static long jHash, jWrites;
    static StringBuilder deaths = new StringBuilder();

    static long mix64(long z) {
        z += 0x9E3779B97F4A7C15L;
        z = (z ^ (z >>> 30)) * 0xBF58476D1CE4E5B9L;
        z = (z ^ (z >>> 27)) * 0x94D049BB133111EBL;
        return z ^ (z >>> 31);
    }

    static byte[] readAll(Path p) throws IOException {
        byte[] b = Files.readAllBytes(p);
        return (b.length > MAX_WARRIOR_SIZE) ? Arrays.copyOf(b, MAX_WARRIOR_SIZE) : b;
    }

    static List<WarriorGroup> loadSurvivors(Path dir) throws IOException {
        List<Path> files = new ArrayList<>();
        if (Files.isDirectory(dir))
            for (Path p : (Iterable<Path>) Files.list(dir)::iterator)
                if (Files.isRegularFile(p) && !p.getFileName().toString().contains("."))
                    files.add(p);
        files.sort((a, b) -> a.getFileName().toString().compareToIgnoreCase(b.getFileName().toString()));

        List<WarriorGroup> groups = new ArrayList<>();
        WarriorGroup cur = null;
        for (Path p : files) {
            String name = p.getFileName().toString();
            byte[] code = readAll(p);
            char last = name.isEmpty() ? 0 : name.charAt(name.length() - 1);
            if (last == '1') {
                cur = new WarriorGroup(name.substring(0, name.length() - 1));
                cur.addWarrior(new WarriorData(name, code, WarriorType.SURVIVOR_1));
                groups.add(cur);
            } else if (last == '2' && cur != null) {
                cur.addWarrior(new WarriorData(name, code, WarriorType.SURVIVOR_2));
                cur = null;
            } else {
                WarriorGroup g = new WarriorGroup(name);
                g.addWarrior(new WarriorData(name, code, WarriorType.SURVIVOR));
                groups.add(g);
                cur = null;
            }
        }
        return groups;
    }

    static WarriorGroup loadZombies(Path dir) throws IOException {
        WarriorGroup z = new WarriorGroup("ZoMbIeS");
        if (!Files.isDirectory(dir)) return z;
        List<Path> files = new ArrayList<>();
        for (Path p : (Iterable<Path>) Files.list(dir)::iterator)
            if (Files.isRegularFile(p)) files.add(p);
        files.sort(Comparator.comparing(p -> p.getFileName().toString()));
        for (Path p : files) {
            String name = p.getFileName().toString();
            WarriorType t = name.toLowerCase().endsWith("h") ? WarriorType.ZOMBIE_H : WarriorType.ZOMBIE;
            z.addWarrior(new WarriorData(name, readAll(p), t));
        }
        return z;
    }

    // name -> 0 none / 1 CPU / 2 memory, for the --gpu digest
    static Map<String, Integer> deathReason = new HashMap<>();

    public static void main(String[] args) throws Exception {
        // --gpu makes this print the line format that `engine.exe --digest`
        // emits, so the Vulkan engine can be diffed straight against the real
        // Java classes rather than only against the C++ port.
        boolean gpuFmt = false;
        List<String> pos = new ArrayList<>();
        for (String a : args) {
            if (a.equals("--gpu")) gpuFmt = true;
            else pos.add(a);
        }
        args = pos.toArray(new String[0]);

        Path survDir = Paths.get(args.length > 0 ? args[0] : "CGX-engine/survivors");
        String zomb  = args.length > 1 ? args[1] : "";
        int games    = args.length > 2 ? Integer.parseInt(args[2]) : 100;
        int combo    = args.length > 3 ? Integer.parseInt(args[3]) : 4;
        int zspeed   = args.length > 4 ? Integer.parseInt(args[4]) : 2;
        long first   = args.length > 5 ? Long.parseLong(args[5]) : 0;
        long pairSeed = args.length > 6 ? Long.parseLong(args[6]) : 0;
        int maxRounds = args.length > 7 ? Integer.parseInt(args[7]) : Competition.MAX_ROUND;

        List<WarriorGroup> field = loadSurvivors(survDir);
        WarriorGroup zombies = loadZombies(Paths.get(zomb.isEmpty() ? "___none___" : zomb));
        if (combo > field.size()) combo = field.size();

        Options options = OptionsParser.newOptionsParser(Options.class).getOptions(Options.class);
        options.zombieSpeed = zspeed;

        MemoryEventListener mem = (RealModeAddress a) -> {
            jHash ^= (a.getLinearAddress() & 0xFFFFFFFFL);
            jHash *= FNV_PRIME;
            jWrites++;
        };
        CompetitionEventListener wl = new CompetitionEventListener() {
            public void onWarStart(long s) {}
            public void onWarEnd(int r, String w) {}
            public void onRound(int r) {}
            public void onWarriorBirth(String n) {}
            public void onWarriorDeath(String n, String r) {
                deaths.append(n).append('/').append(r.startsWith("mem") ? 'M' : 'C').append(';');
                deathReason.put(n, r.startsWith("mem") ? 2 : 1);
            }
            public void onCompetitionStart() {}
            public void onCompetitionEnd() {}
            public void onEndRound() {}
        };

        System.out.printf("field %d combo %d games %d zspeed %d zombies %d%n",
                          field.size(), combo, games, zspeed, zombies.getWarriors().size());

        for (int k = 0; k < games; ++k) {
            // cw::drawCombo, transcribed: splitmix64 over (pairingSeed, k) then
            // a partial Fisher-Yates with java.util.Random. Long arithmetic
            // wraps the same way C++ unsigned does, and >>> is the u64 shift.
            Random pick = new Random(mix64(pairSeed + (long) k * 0x9E3779B97F4A7C15L));
            int[] idx = new int[field.size()];
            for (int i = 0; i < idx.length; ++i) idx[i] = i;
            for (int i = 0; i < combo; ++i) {
                int j = i + pick.nextInt(idx.length - i);
                int t = idx[i]; idx[i] = idx[j]; idx[j] = t;
            }
            List<WarriorGroup> chosen = new ArrayList<>();
            for (int i = 0; i < combo; ++i) chosen.add(field.get(idx[i]));
            if (!zombies.getWarriors().isEmpty()) chosen.add(zombies);

            jHash = FNV_OFF; jWrites = 0;
            deaths.setLength(0);
            deathReason.clear();

            long seed = first + k;
            War war = new War(mem, wl, false, options);
            war.setSeed(seed);
            int round = 0;
            int surv = 0;
            String alive = "";
            try {
                war.loadWarriorGroups(chosen.toArray(new WarriorGroup[0]));
                for (; round < maxRounds; ++round) {
                    if (war.isOver()) break;
                    war.nextRound(round);
                }
                surv = war.getNumRemainingSurvivors();
                List<String> names = new ArrayList<>();
                for (int i = 0; i < war.getNumWarriors(); ++i) {
                    Warrior w = war.getWarrior(i);
                    if (w.isAlive() && !w.isZombie()) names.add(w.getName());
                }
                alive = String.join(",", names);
            } catch (Exception e) {
                System.out.printf("war %d seed %d VOID%n", k, seed);
                continue;
            }

            if (gpuFmt) {
                // exactly what gpu-engine/engine.exe --digest prints
                int mask = 0;
                for (int i = 0; i < war.getNumWarriors(); ++i)
                    if (war.getWarrior(i).isAlive()) mask |= 1 << i;
                int end = (round >= maxRounds) ? (surv == 1 ? 1 : 2)
                                               : (surv == 1 ? 1 : 3);
                StringBuilder sb = new StringBuilder();
                sb.append(String.format("war %d seed %d end %d rounds %d alive %d mask %05x",
                                        k, seed, end, round, surv, mask));
                sb.append(" load");
                for (int i = 0; i < war.getNumWarriors(); ++i)
                    sb.append(String.format(" %04x", war.getWarrior(i).getLoadOffset() & 0xFFFF));
                sb.append(" death");
                for (int i = 0; i < war.getNumWarriors(); ++i)
                    sb.append(' ').append(deathReason.getOrDefault(war.getWarrior(i).getName(), 0));
                System.out.println(sb);
                continue;
            }

            System.out.printf("war %d seed %d rounds %d alive %d [%s] deaths %s writes %d wj %016x%n",
                              k, seed, round, surv, alive, deaths.toString(), jWrites, jHash);
        }
    }
}
