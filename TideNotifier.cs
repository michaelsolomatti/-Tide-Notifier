// TideNotifier.cs
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;

class Location
{
    [JsonPropertyName("name")] public string Name { get; set; }
    [JsonPropertyName("phase_offset")] public double PhaseOffset { get; set; }
    [JsonPropertyName("threshold")] public int Threshold { get; set; }
}

class TideModel
{
    public static double Level(double hours, double phaseOffset)
    {
        const double M2_PERIOD = 12.42;
        const double S2_PERIOD = 12.0;
        double radM2 = (hours / M2_PERIOD + phaseOffset / M2_PERIOD) * 2 * Math.PI;
        double radS2 = (hours / S2_PERIOD + phaseOffset / S2_PERIOD) * 2 * Math.PI;
        return 0.0 + 1.2 * Math.Sin(radM2) + 0.4 * Math.Sin(radS2);
    }

    public static (double? t, double? lvl)[] NextEvent(double hours, double phaseOffset)
    {
        double step = 0.05;
        var times = new List<double>();
        var levels = new List<double>();
        for (int i = 0; i <= (int)(12*60/(step*60)); i++)
        {
            double t = hours + i * step;
            times.Add(t);
            levels.Add(Level(t, phaseOffset));
        }
        var highTimes = new List<(double t, double lvl)>();
        var lowTimes = new List<(double t, double lvl)>();
        for (int i = 1; i < times.Count-1; i++)
        {
            if (levels[i] > levels[i-1] && levels[i] > levels[i+1])
                highTimes.Add((times[i], levels[i]));
            else if (levels[i] < levels[i-1] && levels[i] < levels[i+1])
                lowTimes.Add((times[i], levels[i]));
        }
        (double t, double lvl)? nextHigh = null, nextLow = null;
        foreach (var h in highTimes)
            if (h.t >= hours) { nextHigh = h; break; }
        if (!nextHigh.HasValue && highTimes.Any())
            nextHigh = (highTimes[0].t + 24, highTimes[0].lvl);
        foreach (var l in lowTimes)
            if (l.t >= hours) { nextLow = l; break; }
        if (!nextLow.HasValue && lowTimes.Any())
            nextLow = (lowTimes[0].t + 24, lowTimes[0].lvl);
        return new (double?, double?)[] { (nextHigh?.t, nextHigh?.lvl), (nextLow?.t, nextLow?.lvl) };
    }
}

class Notifier
{
    private List<Location> locations = new List<Location>();
    private readonly string configFile = "tide_notifier_config.json";
    private readonly JsonSerializerOptions options = new JsonSerializerOptions { WriteIndented = true };
    private const int DEFAULT_INTERVAL = 60;
    private const int DEFAULT_THRESHOLD = 30;

    public Notifier() => Load();

    private void Load()
    {
        if (!File.Exists(configFile)) return;
        string json = File.ReadAllText(configFile);
        locations = JsonSerializer.Deserialize<List<Location>>(json) ?? new List<Location>();
    }

    private void Save()
    {
        string json = JsonSerializer.Serialize(locations, options);
        File.WriteAllText(configFile, json);
    }

    public void AddLocation(string name, double phaseOffset, int threshold)
    {
        locations.RemoveAll(l => l.Name == name);
        locations.Add(new Location { Name = name, PhaseOffset = phaseOffset, Threshold = threshold == 0 ? DEFAULT_THRESHOLD : threshold });
        Save();
        Console.WriteLine($"✅ Location '{name}' added.");
    }

    public void RemoveLocation(string name)
    {
        locations.RemoveAll(l => l.Name == name);
        Save();
        Console.WriteLine($"✅ Location '{name}' removed.");
    }

    public void ListLocations()
    {
        if (!locations.Any()) { Console.WriteLine("No locations."); return; }
        Console.WriteLine("\n📍 Monitored locations:");
        foreach (var loc in locations)
            Console.WriteLine($"  {loc.Name} (phase: {loc.PhaseOffset:F2}h, threshold: {loc.Threshold} min)");
    }

    private List<dynamic> GetStatus(double hours)
    {
        var statuses = new List<dynamic>();
        foreach (var loc in locations)
        {
            double lvl = TideModel.Level(hours, loc.PhaseOffset);
            var events = TideModel.NextEvent(hours, loc.PhaseOffset);
            string nextType = null;
            double? nextTime = null, nextLevel = null;
            if (events[0].t.HasValue && events[1].t.HasValue)
            {
                if (events[0].t < events[1].t)
                {
                    nextType = "High";
                    nextTime = events[0].t;
                    nextLevel = events[0].lvl;
                }
                else
                {
                    nextType = "Low";
                    nextTime = events[1].t;
                    nextLevel = events[1].lvl;
                }
            }
            else if (events[0].t.HasValue)
            {
                nextType = "High";
                nextTime = events[0].t;
                nextLevel = events[0].lvl;
            }
            else if (events[1].t.HasValue)
            {
                nextType = "Low";
                nextTime = events[1].t;
                nextLevel = events[1].lvl;
            }
            statuses.Add(new { loc, lvl, nextType, nextTime, nextLevel });
        }
        return statuses;
    }

    public void Monitor(int interval = DEFAULT_INTERVAL)
    {
        Console.WriteLine($"🌊 Tide Notifier (checking every {interval}s)");
        Console.WriteLine("Press Ctrl+C to stop.\n");
        while (true)
        {
            var now = DateTime.Now;
            double hours = now.Hour + now.Minute/60.0 + now.Second/3600.0;
            var statuses = GetStatus(hours);
            foreach (var st in statuses)
            {
                if (st.nextType != null)
                {
                    double minutesUntil = (st.nextTime - hours) * 60;
                    if (minutesUntil < 0) minutesUntil += 24*60;
                    if (minutesUntil <= st.loc.Threshold && minutesUntil > 0)
                    {
                        Console.WriteLine($"[{now:yyyy-MM-dd HH:mm}] {st.loc.Name} – {st.nextType} tide in {(int)minutesUntil} min (level: {st.nextLevel:F2} m)");
                    }
                }
            }
            System.Threading.Thread.Sleep(interval * 1000);
        }
    }

    public void ShowStatus()
    {
        var now = DateTime.Now;
        double hours = now.Hour + now.Minute/60.0 + now.Second/3600.0;
        var statuses = GetStatus(hours);
        Console.WriteLine($"\n🌊 Tide Status ({now:yyyy-MM-dd HH:mm})");
        foreach (var st in statuses)
        {
            Console.WriteLine($"\n📍 {st.loc.Name}");
            Console.WriteLine($"  Phase offset: {st.loc.PhaseOffset:F2}h");
            Console.WriteLine($"  Current level: {st.lvl:F2} m");
            if (st.nextType != null)
            {
                double minutesUntil = (st.nextTime - hours) * 60;
                if (minutesUntil < 0) minutesUntil += 24*60;
                int hour = (int)st.nextTime;
                int min = (int)((st.nextTime - hour) * 60);
                Console.WriteLine($"  Next event: {st.nextType} tide at {hour:D2}:{min:D2} ({st.nextLevel:F2} m) – in {(int)minutesUntil} min");
            }
            Console.WriteLine($"  Alert threshold: {st.loc.Threshold} min");
        }
    }

    static void Main(string[] args)
    {
        if (args.Length < 1)
        {
            Console.WriteLine("Usage: TideNotifier <command> [options]");
            return;
        }
        var app = new Notifier();
        string cmd = args[0];
        switch (cmd)
        {
            case "add":
                if (args.Length < 3) { Console.WriteLine("add <name> <phase_offset> [--threshold <minutes>]"); return; }
                string name = args[1];
                double phase = double.Parse(args[2]);
                int threshold = DEFAULT_THRESHOLD;
                for (int i=3; i<args.Length; i++)
                {
                    if (args[i] == "--threshold" && i+1 < args.Length)
                        threshold = int.Parse(args[++i]);
                }
                app.AddLocation(name, phase, threshold);
                break;
            case "list":
                app.ListLocations();
                break;
            case "remove":
                if (args.Length < 2) { Console.WriteLine("remove <name>"); return; }
                app.RemoveLocation(args[1]);
                break;
            case "start":
                int interval = DEFAULT_INTERVAL;
                for (int i=1; i<args.Length; i++)
                {
                    if (args[i] == "--interval" && i+1 < args.Length)
                        interval = int.Parse(args[++i]);
                }
                app.Monitor(interval);
                break;
            case "status":
                app.ShowStatus();
                break;
            default:
                Console.WriteLine("Unknown command. Available: add, list, remove, start, status");
                break;
        }
    }
}
