// TideNotifier.java
import java.io.*;
import java.nio.file.*;
import java.time.*;
import java.time.format.*;
import java.util.*;
import com.google.gson.*;

class Location {
    String name;
    double phase_offset;
    int threshold;
}

class TideModel {
    public static double level(double hours, double phaseOffset) {
        final double M2_PERIOD = 12.42;
        final double S2_PERIOD = 12.0;
        double radM2 = (hours / M2_PERIOD + phaseOffset / M2_PERIOD) * 2 * Math.PI;
        double radS2 = (hours / S2_PERIOD + phaseOffset / S2_PERIOD) * 2 * Math.PI;
        return 0.0 + 1.2 * Math.sin(radM2) + 0.4 * Math.sin(radS2);
    }

    public static Map<String, Object> nextEvent(double hours, double phaseOffset) {
        double step = 0.05;
        List<Double> times = new ArrayList<>();
        List<Double> levels = new ArrayList<>();
        for (int i = 0; i <= (int)(12*60/(step*60)); i++) {
            double t = hours + i * step;
            times.add(t);
            levels.add(level(t, phaseOffset));
        }
        List<Map<String, Object>> highTimes = new ArrayList<>();
        List<Map<String, Object>> lowTimes = new ArrayList<>();
        for (int i = 1; i < times.size()-1; i++) {
            if (levels.get(i) > levels.get(i-1) && levels.get(i) > levels.get(i+1)) {
                Map<String, Object> m = new HashMap<>();
                m.put("t", times.get(i));
                m.put("lvl", levels.get(i));
                highTimes.add(m);
            } else if (levels.get(i) < levels.get(i-1) && levels.get(i) < levels.get(i+1)) {
                Map<String, Object> m = new HashMap<>();
                m.put("t", times.get(i));
                m.put("lvl", levels.get(i));
                lowTimes.add(m);
            }
        }
        Map<String, Object> nextHigh = null, nextLow = null;
        for (Map<String, Object> h : highTimes) {
            if ((double)h.get("t") >= hours) {
                nextHigh = h;
                break;
            }
        }
        if (nextHigh == null && !highTimes.isEmpty()) {
            Map<String, Object> h = highTimes.get(0);
            nextHigh = new HashMap<>();
            nextHigh.put("t", (double)h.get("t") + 24);
            nextHigh.put("lvl", h.get("lvl"));
        }
        for (Map<String, Object> l : lowTimes) {
            if ((double)l.get("t") >= hours) {
                nextLow = l;
                break;
            }
        }
        if (nextLow == null && !lowTimes.isEmpty()) {
            Map<String, Object> l = lowTimes.get(0);
            nextLow = new HashMap<>();
            nextLow.put("t", (double)l.get("t") + 24);
            nextLow.put("lvl", l.get("lvl"));
        }
        Map<String, Object> result = new HashMap<>();
        result.put("high", nextHigh);
        result.put("low", nextLow);
        return result;
    }
}

public class TideNotifier {
    private List<Location> locations = new ArrayList<>();
    private final String configFile = "tide_notifier_config.json";
    private final Gson gson = new GsonBuilder().setPrettyPrinting().create();
    private static final int DEFAULT_INTERVAL = 60;
    private static final int DEFAULT_THRESHOLD = 30;

    public TideNotifier() {
        load();
    }

    private void load() {
        try {
            Path path = Paths.get(configFile);
            if (Files.exists(path)) {
                String json = new String(Files.readAllBytes(path));
                Location[] arr = gson.fromJson(json, Location[].class);
                locations = Arrays.asList(arr);
            }
        } catch (Exception e) {}
    }

    private void save() {
        try {
            Files.write(Paths.get(configFile), gson.toJson(locations).getBytes());
        } catch (Exception e) {}
    }

    public void addLocation(String name, double phaseOffset, int threshold) {
        locations.removeIf(l -> l.name.equals(name));
        Location loc = new Location();
        loc.name = name;
        loc.phase_offset = phaseOffset;
        loc.threshold = threshold == 0 ? DEFAULT_THRESHOLD : threshold;
        locations.add(loc);
        save();
        System.out.printf("✅ Location '%s' added.\n", name);
    }

    public void removeLocation(String name) {
        locations.removeIf(l -> l.name.equals(name));
        save();
        System.out.printf("✅ Location '%s' removed.\n", name);
    }

    public void listLocations() {
        if (locations.isEmpty()) {
            System.out.println("No locations.");
            return;
        }
        System.out.println("\n📍 Monitored locations:");
        for (Location loc : locations) {
            System.out.printf("  %s (phase: %.2fh, threshold: %d min)\n", loc.name, loc.phase_offset, loc.threshold);
        }
    }

    private List<Map<String, Object>> getStatus(double hours) {
        List<Map<String, Object>> statuses = new ArrayList<>();
        for (Location loc : locations) {
            double lvl = TideModel.level(hours, loc.phase_offset);
            Map<String, Object> events = TideModel.nextEvent(hours, loc.phase_offset);
            String nextType = null;
            Double nextTime = null;
            Double nextLevel = null;
            @SuppressWarnings("unchecked")
            Map<String, Object> high = (Map<String, Object>) events.get("high");
            Map<String, Object> low = (Map<String, Object>) events.get("low");
            if (high != null && low != null) {
                if ((double)high.get("t") < (double)low.get("t")) {
                    nextType = "High";
                    nextTime = (double)high.get("t");
                    nextLevel = (double)high.get("lvl");
                } else {
                    nextType = "Low";
                    nextTime = (double)low.get("t");
                    nextLevel = (double)low.get("lvl");
                }
            } else if (high != null) {
                nextType = "High";
                nextTime = (double)high.get("t");
                nextLevel = (double)high.get("lvl");
            } else if (low != null) {
                nextType = "Low";
                nextTime = (double)low.get("t");
                nextLevel = (double)low.get("lvl");
            }
            Map<String, Object> status = new HashMap<>();
            status.put("loc", loc);
            status.put("lvl", lvl);
            status.put("nextType", nextType);
            status.put("nextTime", nextTime);
            status.put("nextLevel", nextLevel);
            statuses.add(status);
        }
        return statuses;
    }

    public void monitor(int interval) {
        System.out.printf("🌊 Tide Notifier (checking every %ds)\n", interval);
        System.out.println("Press Ctrl+C to stop.\n");
        try {
            while (true) {
                LocalDateTime now = LocalDateTime.now();
                double hours = now.getHour() + now.getMinute()/60.0 + now.getSecond()/3600.0;
                List<Map<String, Object>> statuses = getStatus(hours);
                for (Map<String, Object> st : statuses) {
                    String nextType = (String) st.get("nextType");
                    if (nextType != null) {
                        double nextTime = (double) st.get("nextTime");
                        double nextLevel = (double) st.get("nextLevel");
                        Location loc = (Location) st.get("loc");
                        double minutesUntil = (nextTime - hours) * 60;
                        if (minutesUntil < 0) minutesUntil += 24*60;
                        if (minutesUntil <= loc.threshold && minutesUntil > 0) {
                            System.out.printf("[%s] %s – %s tide in %.0f min (level: %.2f m)\n",
                                now.format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm")),
                                loc.name, nextType, minutesUntil, nextLevel);
                        }
                    }
                }
                Thread.sleep(interval * 1000L);
            }
        } catch (InterruptedException e) {}
    }

    public void showStatus() {
        LocalDateTime now = LocalDateTime.now();
        double hours = now.getHour() + now.getMinute()/60.0 + now.getSecond()/3600.0;
        List<Map<String, Object>> statuses = getStatus(hours);
        System.out.printf("\n🌊 Tide Status (%s)\n", now.format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm")));
        for (Map<String, Object> st : statuses) {
            Location loc = (Location) st.get("loc");
            double lvl = (double) st.get("lvl");
            String nextType = (String) st.get("nextType");
            Double nextTime = (Double) st.get("nextTime");
            Double nextLevel = (Double) st.get("nextLevel");
            System.out.printf("\n📍 %s\n", loc.name);
            System.out.printf("  Phase offset: %.2fh\n", loc.phase_offset);
            System.out.printf("  Current level: %.2f m\n", lvl);
            if (nextType != null) {
                double minutesUntil = (nextTime - hours) * 60;
                if (minutesUntil < 0) minutesUntil += 24*60;
                int hour = (int) Math.floor(nextTime);
                int min = (int) ((nextTime - hour) * 60);
                System.out.printf("  Next event: %s tide at %02d:%02d (%.2f m) – in %.0f min\n",
                    nextType, hour, min, nextLevel, minutesUntil);
            }
            System.out.printf("  Alert threshold: %d min\n", loc.threshold);
        }
    }

    public static void main(String[] args) throws Exception {
        if (args.length < 1) {
            System.out.println("Usage: TideNotifier <command> [options]");
            return;
        }
        TideNotifier app = new TideNotifier();
        String cmd = args[0];
        switch (cmd) {
            case "add":
                if (args.length < 3) { System.out.println("add <name> <phase_offset> [--threshold <minutes>]"); return; }
                String name = args[1];
                double phase = Double.parseDouble(args[2]);
                int threshold = DEFAULT_THRESHOLD;
                for (int i=3; i<args.length; i++) {
                    if (args[i].equals("--threshold") && i+1 < args.length) {
                        threshold = Integer.parseInt(args[++i]);
                    }
                }
                app.addLocation(name, phase, threshold);
                break;
            case "list":
                app.listLocations();
                break;
            case "remove":
                if (args.length < 2) { System.out.println("remove <name>"); return; }
                app.removeLocation(args[1]);
                break;
            case "start":
                int interval = DEFAULT_INTERVAL;
                for (int i=1; i<args.length; i++) {
                    if (args[i].equals("--interval") && i+1 < args.length) {
                        interval = Integer.parseInt(args[++i]);
                    }
                }
                app.monitor(interval);
                break;
            case "status":
                app.showStatus();
                break;
            default:
                System.out.println("Unknown command. Available: add, list, remove, start, status");
        }
    }
}
