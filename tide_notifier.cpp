// tide_notifier.cpp
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cmath>
#include <thread>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <nlohmann/json.hpp>
#include <getopt.h>

using namespace std;
using json = nlohmann::json;

const int DEFAULT_INTERVAL = 60;
const int DEFAULT_THRESHOLD = 30;
const string CONFIG_FILE = "tide_notifier_config.json";

struct Location {
    string name;
    double phase_offset;
    int threshold;
};

class TideModel {
public:
    static double level(double hours, double phaseOffset) {
        const double M2_PERIOD = 12.42;
        const double S2_PERIOD = 12.0;
        double radM2 = (hours / M2_PERIOD + phaseOffset / M2_PERIOD) * 2 * M_PI;
        double radS2 = (hours / S2_PERIOD + phaseOffset / S2_PERIOD) * 2 * M_PI;
        return 0.0 + 1.2 * sin(radM2) + 0.4 * sin(radS2);
    }

    static pair<double, double> nextEvent(double hours, double phaseOffset) {
        double step = 0.05;
        vector<double> times, levels;
        for (int i = 0; i <= (int)(12*60/(step*60)); i++) {
            double t = hours + i * step;
            times.push_back(t);
            levels.push_back(level(t, phaseOffset));
        }
        vector<pair<double, double>> highTimes, lowTimes;
        for (size_t i = 1; i < times.size()-1; i++) {
            if (levels[i] > levels[i-1] && levels[i] > levels[i+1])
                highTimes.push_back({times[i], levels[i]});
            else if (levels[i] < levels[i-1] && levels[i] < levels[i+1])
                lowTimes.push_back({times[i], levels[i]});
        }
        pair<double, double> nextHigh = {0,0}, nextLow = {0,0};
        for (auto& h : highTimes) {
            if (h.first >= hours) { nextHigh = h; break; }
        }
        if (nextHigh.first == 0 && !highTimes.empty()) {
            nextHigh = {highTimes[0].first + 24, highTimes[0].second};
        }
        for (auto& l : lowTimes) {
            if (l.first >= hours) { nextLow = l; break; }
        }
        if (nextLow.first == 0 && !lowTimes.empty()) {
            nextLow = {lowTimes[0].first + 24, lowTimes[0].second};
        }
        return {nextHigh.first, nextLow.first}; // returning times only, levels are in the second component? We'll handle separately.
    }
};

class Notifier {
private:
    vector<Location> locations;
    string configFile;

    void load() {
        ifstream f(configFile);
        if (!f.is_open()) return;
        json j;
        f >> j;
        for (auto& item : j) {
            Location loc;
            loc.name = item["name"];
            loc.phase_offset = item["phase_offset"];
            loc.threshold = item["threshold"];
            locations.push_back(loc);
        }
    }

    void save() {
        json j = json::array();
        for (auto& loc : locations) {
            j.push_back({{"name", loc.name}, {"phase_offset", loc.phase_offset}, {"threshold", loc.threshold}});
        }
        ofstream f(configFile);
        f << setw(2) << j << endl;
    }

    string currentTime() {
        time_t t = time(nullptr);
        char buf[20];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", localtime(&t));
        return string(buf);
    }

    double hoursNow() {
        time_t t = time(nullptr);
        tm* tm = localtime(&t);
        return tm->tm_hour + tm->tm_min/60.0 + tm->tm_sec/3600.0;
    }

public:
    Notifier(const string& file) : configFile(file) { load(); }

    void addLocation(const string& name, double phaseOffset, int threshold) {
        locations.erase(remove_if(locations.begin(), locations.end(),
            [&](const Location& l) { return l.name == name; }), locations.end());
        Location loc{name, phaseOffset, threshold == 0 ? DEFAULT_THRESHOLD : threshold};
        locations.push_back(loc);
        save();
        cout << "✅ Location '" << name << "' added.\n";
    }

    void removeLocation(const string& name) {
        locations.erase(remove_if(locations.begin(), locations.end(),
            [&](const Location& l) { return l.name == name; }), locations.end());
        save();
        cout << "✅ Location '" << name << "' removed.\n";
    }

    void listLocations() {
        if (locations.empty()) { cout << "No locations.\n"; return; }
        cout << "\n📍 Monitored locations:\n";
        for (auto& loc : locations) {
            cout << "  " << loc.name << " (phase: " << fixed << setprecision(2) << loc.phase_offset << "h, threshold: " << loc.threshold << " min)\n";
        }
    }

    void monitor(int interval = DEFAULT_INTERVAL) {
        cout << "🌊 Tide Notifier (checking every " << interval << "s)\n";
        cout << "Press Ctrl+C to stop.\n\n";
        while (true) {
            double hours = hoursNow();
            for (auto& loc : locations) {
                double lvl = TideModel::level(hours, loc.phase_offset);
                auto highLow = TideModel::nextEvent(hours, loc.phase_offset);
                // We need to determine which event is next by scanning again.
                // For simplicity, reuse the previous logic but with high/low times.
                // We'll implement inline: find next high/low within next 12h
                double nextHighTime = 0, nextLowTime = 0;
                double nextHighLevel = 0, nextLowLevel = 0;
                double step = 0.05;
                for (int i=0; i <= (int)(12*60/(step*60)); i++) {
                    double t = hours + i * step;
                    double l = TideModel::level(t, loc.phase_offset);
                    // Approx extrema detection
                }
                // We'll use a simpler method: just scan and find nearest extrema.
                // For brevity, we'll assume we have a function that gives times.
                // Since C++ version is complex, we'll keep it simple: we use the same logic as other languages.
                // For now, we'll just print a placeholder.
                // Actually, we'll implement a mini scanning inside.
                double bestHighTime = 0, bestLowTime = 0;
                double bestHighLvl = -1000, bestLowLvl = 1000;
                for (int i=0; i <= (int)(12*60/(step*60)); i++) {
                    double t = hours + i * step;
                    double l = TideModel::level(t, loc.phase_offset);
                    if (l > bestHighLvl) { bestHighLvl = l; bestHighTime = t; }
                    if (l < bestLowLvl) { bestLowLvl = l; bestLowTime = t; }
                }
                // This is not accurate, but for demo we'll use it.
                // Better: use extrema detection as in Python.
                // We'll copy the Python logic (vector of times and levels).
                vector<double> times, levels;
                for (int i=0; i <= (int)(12*60/(step*60)); i++) {
                    double t = hours + i * step;
                    times.push_back(t);
                    levels.push_back(TideModel::level(t, loc.phase_offset));
                }
                vector<pair<double, double>> highTimes, lowTimes;
                for (size_t i = 1; i < times.size()-1; i++) {
                    if (levels[i] > levels[i-1] && levels[i] > levels[i+1])
                        highTimes.push_back({times[i], levels[i]});
                    else if (levels[i] < levels[i-1] && levels[i] < levels[i+1])
                        lowTimes.push_back({times[i], levels[i]});
                }
                double nextHighT = 0, nextLowT = 0;
                double nextHighLvl = 0, nextLowLvl = 0;
                for (auto& h : highTimes) {
                    if (h.first >= hours) { nextHighT = h.first; nextHighLvl = h.second; break; }
                }
                if (nextHighT == 0 && !highTimes.empty()) {
                    nextHighT = highTimes[0].first + 24;
                    nextHighLvl = highTimes[0].second;
                }
                for (auto& l : lowTimes) {
                    if (l.first >= hours) { nextLowT = l.first; nextLowLvl = l.second; break; }
                }
                if (nextLowT == 0 && !lowTimes.empty()) {
                    nextLowT = lowTimes[0].first + 24;
                    nextLowLvl = lowTimes[0].second;
                }
                string nextType;
                double nextTime, nextLevel;
                if (nextHighT > 0 && nextLowT > 0) {
                    if (nextHighT < nextLowT) {
                        nextType = "High"; nextTime = nextHighT; nextLevel = nextHighLvl;
                    } else {
                        nextType = "Low"; nextTime = nextLowT; nextLevel = nextLowLvl;
                    }
                } else if (nextHighT > 0) {
                    nextType = "High"; nextTime = nextHighT; nextLevel = nextHighLvl;
                } else if (nextLowT > 0) {
                    nextType = "Low"; nextTime = nextLowT; nextLevel = nextLowLvl;
                } else {
                    continue;
                }
                double minutesUntil = (nextTime - hours) * 60;
                if (minutesUntil < 0) minutesUntil += 24*60;
                if (minutesUntil <= loc.threshold && minutesUntil > 0) {
                    cout << "[" << currentTime() << "] " << loc.name << " – " << nextType
                         << " tide in " << (int)minutesUntil << " min (level: " << fixed << setprecision(2) << nextLevel << " m)\n";
                }
            }
            this_thread::sleep_for(chrono::seconds(interval));
        }
    }

    void showStatus() {
        double hours = hoursNow();
        cout << "\n🌊 Tide Status (" << currentTime() << ")\n";
        for (auto& loc : locations) {
            double lvl = TideModel::level(hours, loc.phase_offset);
            // Find next events
            vector<double> times, levels;
            double step = 0.05;
            for (int i=0; i <= (int)(12*60/(step*60)); i++) {
                double t = hours + i * step;
                times.push_back(t);
                levels.push_back(TideModel::level(t, loc.phase_offset));
            }
            vector<pair<double, double>> highTimes, lowTimes;
            for (size_t i = 1; i < times.size()-1; i++) {
                if (levels[i] > levels[i-1] && levels[i] > levels[i+1])
                    highTimes.push_back({times[i], levels[i]});
                else if (levels[i] < levels[i-1] && levels[i] < levels[i+1])
                    lowTimes.push_back({times[i], levels[i]});
            }
            double nextHighT = 0, nextLowT = 0;
            double nextHighLvl = 0, nextLowLvl = 0;
            for (auto& h : highTimes) {
                if (h.first >= hours) { nextHighT = h.first; nextHighLvl = h.second; break; }
            }
            if (nextHighT == 0 && !highTimes.empty()) {
                nextHighT = highTimes[0].first + 24;
                nextHighLvl = highTimes[0].second;
            }
            for (auto& l : lowTimes) {
                if (l.first >= hours) { nextLowT = l.first; nextLowLvl = l.second; break; }
            }
            if (nextLowT == 0 && !lowTimes.empty()) {
                nextLowT = lowTimes[0].first + 24;
                nextLowLvl = lowTimes[0].second;
            }
            string nextType;
            double nextTime, nextLevel;
            if (nextHighT > 0 && nextLowT > 0) {
                if (nextHighT < nextLowT) {
                    nextType = "High"; nextTime = nextHighT; nextLevel = nextHighLvl;
                } else {
                    nextType = "Low"; nextTime = nextLowT; nextLevel = nextLowLvl;
                }
            } else if (nextHighT > 0) {
                nextType = "High"; nextTime = nextHighT; nextLevel = nextHighLvl;
            } else if (nextLowT > 0) {
                nextType = "Low"; nextTime = nextLowT; nextLevel = nextLowLvl;
            } else {
                nextType = ""; nextTime = 0; nextLevel = 0;
            }
            cout << "\n📍 " << loc.name << "\n";
            cout << "  Phase offset: " << fixed << setprecision(2) << loc.phase_offset << "h\n";
            cout << "  Current level: " << fixed << setprecision(2) << lvl << " m\n";
            if (!nextType.empty()) {
                double minutesUntil = (nextTime - hours) * 60;
                if (minutesUntil < 0) minutesUntil += 24*60;
                int hour = (int)nextTime;
                int min = (int)((nextTime - hour) * 60);
                cout << "  Next event: " << nextType << " tide at " << setw(2) << setfill('0') << hour << ":" << setw(2) << setfill('0') << min
                     << " (" << fixed << setprecision(2) << nextLevel << " m) – in " << (int)minutesUntil << " min\n";
            }
            cout << "  Alert threshold: " << loc.threshold << " min\n";
        }
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: tide_notifier <command> [options]\n";
        return 1;
    }
    Notifier notifier(CONFIG_FILE);
    string cmd = argv[1];

    if (cmd == "add") {
        if (argc < 4) { cerr << "add <name> <phase_offset> [--threshold <minutes>]\n"; return 1; }
        string name = argv[2];
        double phase = stod(argv[3]);
        int threshold = DEFAULT_THRESHOLD;
        for (int i=4; i<argc; i++) {
            if (string(argv[i]) == "--threshold" && i+1 < argc) {
                threshold = stoi(argv[++i]);
            }
        }
        notifier.addLocation(name, phase, threshold);
    } else if (cmd == "list") {
        notifier.listLocations();
    } else if (cmd == "remove") {
        if (argc < 3) { cerr << "remove <name>\n"; return 1; }
        notifier.removeLocation(argv[2]);
    } else if (cmd == "start") {
        int interval = DEFAULT_INTERVAL;
        for (int i=2; i<argc; i++) {
            if (string(argv[i]) == "--interval" && i+1 < argc) {
                interval = stoi(argv[++i]);
            }
        }
        notifier.monitor(interval);
    } else if (cmd == "status") {
        notifier.showStatus();
    } else {
        cerr << "Unknown command. Available: add, list, remove, start, status\n";
        return 1;
    }
    return 0;
}
