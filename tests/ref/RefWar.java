// Reference-engine mirror of tests/selftest.cpp: runs ONE war through the
// real il.co.codeguru.corewars8086 classes (compiled from tests/ref/java) with the
// same group set / seed / zombie speed, and prints the same deterministic
// summary so the two can be diffed.
//
//   javac -d classes -cp <corewars jar> $(find java -name '*.java')
//   javac -d classes -cp "classes;<jar>" RefWar.java
//   java  -cp "classes;<jar>" RefWar <survDir> <zombDir|""> <seed> <combo> <zspeed>

import il.co.codeguru.corewars8086.war.*;
import il.co.codeguru.corewars8086.memory.MemoryEventListener;
import il.co.codeguru.corewars8086.memory.RealModeAddress;
import il.co.codeguru.corewars8086.cli.Options;
import com.google.devtools.common.options.OptionsParser;

import java.io.*;
import java.nio.file.*;
import java.util.*;

public class RefWar {
    static final int MAX_WARRIOR_SIZE = 512;
    static final long FNV_OFF = -3750763034362895579L;   // 1469598103934665603 unsigned
    static final long FNV_PRIME = 1099511628211L;

    static long jHash, jWrites;

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

    public static void main(String[] args) throws Exception {
        Path survDir = Paths.get(args.length > 0 ? args[0] : "tests/data/nightly-2014/survivors");
        String zomb  = args.length > 1 ? args[1] : "";
        long seed    = args.length > 2 ? Long.parseLong(args[2]) : 0;
        int combo    = args.length > 3 ? Integer.parseInt(args[3]) : -1;
        int zspeed   = args.length > 4 ? Integer.parseInt(args[4]) : 2;

        List<WarriorGroup> groups = loadSurvivors(survDir);
        WarriorGroup zombies = loadZombies(Paths.get(zomb.isEmpty() ? "___none___" : zomb));
        if (combo < 0 || combo > groups.size()) combo = groups.size();

        List<WarriorGroup> chosen = new ArrayList<>(groups.subList(0, combo));
        if (!zombies.getWarriors().isEmpty()) chosen.add(zombies);

        System.out.printf("survivors dir : %s  (%d groups, using %d)%n", survDir, groups.size(), combo);
        System.out.printf("zombies       : %d%n", zombies.getWarriors().size());
        System.out.printf("seed          : %d   zombieSpeed %d%n", seed, zspeed);

        Options options = OptionsParser.newOptionsParser(Options.class).getOptions(Options.class);
        options.zombieSpeed = zspeed;

        jHash = FNV_OFF; jWrites = 0;
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
                System.out.printf("  death: %-28s (%s)%n", n, r);
            }
            public void onCompetitionStart() {}
            public void onCompetitionEnd() {}
            public void onEndRound() {}
        };

        War war = new War(mem, wl, false, options);
        war.setSeed(seed);
        war.loadWarriorGroups(chosen.toArray(new WarriorGroup[0]));

        System.out.printf("warriors      : %d%n", war.getNumWarriors());

        int round = 0;
        for (; round < Competition.MAX_ROUND; ++round) {
            if (war.isOver()) break;
            war.nextRound(round);
        }

        long coreHash = FNV_OFF;
        for (int i = 0; i < RealModeAddress.MEMORY_SIZE; ++i)
            coreHash = (coreHash ^ (war.getMemory().readByte(new RealModeAddress(i)) & 0xFF)) * FNV_PRIME;

        int surv = war.getNumRemainingSurvivors();
        System.out.printf("%n--- result ---%n");
        System.out.printf("rounds run    : %d%n", round);
        System.out.printf("survivors     : %d alive%n", surv);
        System.out.printf("mem writes     : %d%n", jWrites);
        System.out.printf("write journal  : %016x%n", jHash);
        System.out.printf("core checksum  : %016x%n", coreHash);
        System.out.printf("scoring share  : %n");
        float score = 1.0f / surv;
        List<String> lines = new ArrayList<>();
        for (int i = 0; i < war.getNumWarriors(); ++i) {
            Warrior w = war.getWarrior(i);
            if (w.isAlive() && !w.isZombie())
                lines.add(String.format("  %-28s %.6f", w.getName(), score));
        }
        for (String s : lines) System.out.println(s);
    }
}
