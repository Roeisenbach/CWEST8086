// Mass - run many battles in one JVM and report only who died.
// Duel writes the whole trace to disk to get the death records; at 100 MB a
// battle that is the only reason 1000 battles would be slow. This feeds the
// engine an OutputStream that keeps nothing: it scans the bytes for lines
// beginning with 'D' and throws the rest away, so the cost is the simulation
// and nothing else.
import java.io.*;
import java.util.*;
import java.util.concurrent.*;

public final class Mass {
    static final class DeathSink extends OutputStream {
        final StringBuilder line = new StringBuilder(96);
        final List<String> deaths = new ArrayList<>();
        @Override public void write(int b) {
            if (b == '\n') {
                if (line.length() > 0 && line.charAt(0) == 'D') deaths.add(line.toString());
                line.setLength(0);
            } else if (line.length() < 90) line.append((char) b);
        }
        @Override public void write(byte[] b, int off, int len) {
            for (int i = 0; i < len; i++) write(b[off + i]);
        }
    }

    public static void main(String[] args) throws Exception {
        String dir = args[0], zdir = args[1];
        int maxRound = Integer.parseInt(args[2]);
        int from = Integer.parseInt(args[3]), to = Integer.parseInt(args[4]);
        int threads = args.length > 5 ? Integer.parseInt(args[5])
                                      : Runtime.getRuntime().availableProcessors();

        List<String> warn = new ArrayList<>();
        List<EngGroup> base = Field.groupsOf(new File(dir), warn);
        EngGroup zg = Tourney.zombieGroup(new File(zdir));

        ExecutorService ex = Executors.newFixedThreadPool(threads);
        List<Future<String>> fs = new ArrayList<>();
        for (int s = from; s <= to; s++) {
            final int seed = s;
            fs.add(ex.submit(() -> {
                List<EngGroup> pick = new ArrayList<>();
                for (EngGroup g : Field.groupsOf(new File(dir), new ArrayList<>())) pick.add(g);
                pick.add(Tourney.zombieGroup(new File(zdir)));
                EngGroup[] arr = pick.toArray(new EngGroup[0]);
                int n = 0; for (EngGroup g : arr) n += g.size();
                float[] sc = new float[n];
                DeathSink sink = new DeathSink();
                EngRunner.run(sink, arr, seed, 2, maxRound, false, false, "m", sc);
                if (sink.deaths.isEmpty()) return null;
                StringBuilder sb = new StringBuilder("seed " + seed + ":");
                for (String d : sink.deaths) {
                    String[] f = d.trim().split("\s+");
                    sb.append(' ').append(f[3]).append('@').append(Integer.parseInt(f[1], 16));
                }
                return sb.toString();
            }));
        }
        int bad = 0, total = 0;
        List<String> msgs = new ArrayList<>();
        for (Future<String> f : fs) {
            String r = f.get(); total++;
            if (r != null) { bad++; if (msgs.size() < 25) msgs.add(r); }
        }
        ex.shutdown();
        for (String m : msgs) System.out.println("  " + m);
        System.out.println();
        System.out.println(total + " battles at " + maxRound + " rounds; "
                + (total - bad) + " with every warrior alive, " + bad + " with a death.");
    }
}
