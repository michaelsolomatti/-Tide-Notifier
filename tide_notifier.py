# tide_notifier.py
import json
import os
import math
import time
import argparse
from datetime import datetime, timedelta

CONFIG_FILE = "tide_notifier_config.json"
DEFAULT_INTERVAL = 60  # seconds
DEFAULT_THRESHOLD = 30  # minutes

class TideModel:
    @staticmethod
    def level(hours_since_midnight, phase_offset=0.0):
        # Simple semi‑diurnal tide (M2 + S2 approx)
        # Two components: principal lunar and solar
        M2_period = 12.42
        S2_period = 12.0
        # Amplitude: 1.2 m for M2, 0.4 m for S2
        rad_m2 = (hours_since_midnight / M2_period + phase_offset / M2_period) * 2 * math.pi
        rad_s2 = (hours_since_midnight / S2_period + phase_offset / S2_period) * 2 * math.pi
        level = 0.0 + 1.2 * math.sin(rad_m2) + 0.4 * math.sin(rad_s2)
        return level

    @staticmethod
    def next_event(hours_since_midnight, phase_offset=0.0):
        # Find next high and low tide by scanning the next 12 hours
        best_high = None
        best_low = None
        best_high_level = -1000
        best_low_level = 1000
        for minutes in range(0, 12*60, 5):
            t = hours_since_midnight + minutes/60.0
            lvl = TideModel.level(t, phase_offset)
            if lvl > best_high_level:
                best_high_level = lvl
                best_high = (t, lvl)
            if lvl < best_low_level:
                best_low_level = lvl
                best_low = (t, lvl)
        # Actually, we need extrema, not just max/min over interval.
        # Simpler: find extrema by derivative (scanning).
        # For simplicity, we'll approximate by checking local maxima/minima
        step = 0.05
        times = []
        levels = []
        for i in range(int(12*60 / (step*60)) + 1):
            t = hours_since_midnight + i * step
            times.append(t)
            levels.append(TideModel.level(t, phase_offset))
        high_times = []
        low_times = []
        for i in range(1, len(times)-1):
            if levels[i] > levels[i-1] and levels[i] > levels[i+1]:
                high_times.append((times[i], levels[i]))
            elif levels[i] < levels[i-1] and levels[i] < levels[i+1]:
                low_times.append((times[i], levels[i]))
        # Find the next high and low from now
        now = hours_since_midnight
        next_high = None
        next_low = None
        for t, l in high_times:
            if t >= now:
                next_high = (t, l)
                break
        for t, l in low_times:
            if t >= now:
                next_low = (t, l)
                break
        # If none found, wrap around
        if not next_high and high_times:
            next_high = (high_times[0][0] + 24, high_times[0][1])
        if not next_low and low_times:
            next_low = (low_times[0][0] + 24, low_times[0][1])
        return next_high, next_low

class Location:
    def __init__(self, name, phase_offset, threshold=DEFAULT_THRESHOLD):
        self.name = name
        self.phase_offset = phase_offset
        self.threshold = threshold

    def to_dict(self):
        return {"name": self.name, "phase_offset": self.phase_offset, "threshold": self.threshold}

    @classmethod
    def from_dict(cls, d):
        return cls(d["name"], d["phase_offset"], d.get("threshold", DEFAULT_THRESHOLD))

class TideNotifier:
    def __init__(self):
        self.locations = []
        self.load()

    def load(self):
        if os.path.exists(CONFIG_FILE):
            with open(CONFIG_FILE, "r") as f:
                data = json.load(f)
                self.locations = [Location.from_dict(d) for d in data]

    def save(self):
        with open(CONFIG_FILE, "w") as f:
            json.dump([loc.to_dict() for loc in self.locations], f, indent=2)

    def add_location(self, name, phase_offset, threshold=None):
        if threshold is None:
            threshold = DEFAULT_THRESHOLD
        # Remove if exists, then add
        self.locations = [loc for loc in self.locations if loc.name != name]
        self.locations.append(Location(name, phase_offset, threshold))
        self.save()
        print(f"✅ Location '{name}' added.")

    def remove_location(self, name):
        self.locations = [loc for loc in self.locations if loc.name != name]
        self.save()
        print(f"✅ Location '{name}' removed.")

    def list_locations(self):
        if not self.locations:
            print("No locations.")
            return
        print("\n📍 Monitored locations:")
        for loc in self.locations:
            print(f"  {loc.name} (phase: {loc.phase_offset:.2f}h, threshold: {loc.threshold} min)")

    def get_status(self, hours_since_midnight):
        status_lines = []
        for loc in self.locations:
            lvl = TideModel.level(hours_since_midnight, loc.phase_offset)
            next_high, next_low = TideModel.next_event(hours_since_midnight, loc.phase_offset)
            # Determine which event is nearer
            next_event = None
            if next_high and next_low:
                if next_high[0] < next_low[0]:
                    next_event = ("High", next_high[0], next_high[1])
                else:
                    next_event = ("Low", next_low[0], next_low[1])
            elif next_high:
                next_event = ("High", next_high[0], next_high[1])
            elif next_low:
                next_event = ("Low", next_low[0], next_low[1])
            status_lines.append((loc, lvl, next_event))
        return status_lines

    def monitor(self, interval=DEFAULT_INTERVAL):
        print(f"🌊 Tide Notifier (checking every {interval}s)")
        print("Press Ctrl+C to stop.\n")
        try:
            while True:
                now = datetime.now()
                hours = now.hour + now.minute/60.0 + now.second/3600.0
                statuses = self.get_status(hours)
                for loc, lvl, next_event in statuses:
                    if next_event:
                        event_type, event_time, event_level = next_event
                        minutes_until = (event_time - hours) * 60
                        if minutes_until < 0:
                            minutes_until += 24*60
                        if minutes_until <= loc.threshold and minutes_until > 0:
                            print(f"[{now.strftime('%Y-%m-%d %H:%M')}] {loc.name} – {event_type} tide in {int(minutes_until)} min (level: {event_level:.2f} m)")
                        # Optionally, also show current level? We'll just show notifications.
                time.sleep(interval)
        except KeyboardInterrupt:
            print("\nMonitoring stopped.")

    def show_status(self):
        now = datetime.now()
        hours = now.hour + now.minute/60.0 + now.second/3600.0
        statuses = self.get_status(hours)
        print(f"\n🌊 Tide Status ({now.strftime('%Y-%m-%d %H:%M')})")
        for loc, lvl, next_event in statuses:
            print(f"\n📍 {loc.name}")
            print(f"  Phase offset: {loc.phase_offset:.2f}h")
            print(f"  Current level: {lvl:.2f} m")
            if next_event:
                event_type, event_time, event_level = next_event
                minutes_until = (event_time - hours) * 60
                if minutes_until < 0:
                    minutes_until += 24*60
                print(f"  Next event: {event_type} tide at {int(event_time)}:{int((event_time%1)*60):02d} ({event_level:.2f} m) – in {int(minutes_until)} min")
            print(f"  Alert threshold: {loc.threshold} min")

def main():
    parser = argparse.ArgumentParser(description="Tide Notifier")
    subparsers = parser.add_subparsers(dest="cmd", required=True)

    add_parser = subparsers.add_parser("add")
    add_parser.add_argument("name")
    add_parser.add_argument("phase_offset", type=float, help="Phase offset in hours (0-24)")
    add_parser.add_argument("--threshold", type=int, default=None, help="Alert threshold in minutes")

    list_parser = subparsers.add_parser("list")
    remove_parser = subparsers.add_parser("remove")
    remove_parser.add_argument("name")

    start_parser = subparsers.add_parser("start")
    start_parser.add_argument("--interval", type=int, default=DEFAULT_INTERVAL, help="Check interval in seconds")

    status_parser = subparsers.add_parser("status")

    args = parser.parse_args()
    notifier = TideNotifier()

    if args.cmd == "add":
        notifier.add_location(args.name, args.phase_offset, args.threshold)
    elif args.cmd == "list":
        notifier.list_locations()
    elif args.cmd == "remove":
        notifier.remove_location(args.name)
    elif args.cmd == "start":
        notifier.monitor(args.interval)
    elif args.cmd == "status":
        notifier.show_status()

if __name__ == "__main__":
    main()
