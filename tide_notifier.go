// tide_notifier.go
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"math"
	"os"
	"os/signal"
	"syscall"
	"time"
)

const (
	DefaultInterval  = 60
	DefaultThreshold = 30
	ConfigFile       = "tide_notifier_config.json"
)

type Location struct {
	Name         string  `json:"name"`
	PhaseOffset  float64 `json:"phase_offset"`
	Threshold    int     `json:"threshold"`
}

type TideModel struct{}

func (t TideModel) Level(hours, phaseOffset float64) float64 {
	m2Period := 12.42
	s2Period := 12.0
	radM2 := (hours/m2Period + phaseOffset/m2Period) * 2 * math.Pi
	radS2 := (hours/s2Period + phaseOffset/s2Period) * 2 * math.Pi
	return 0.0 + 1.2*math.Sin(radM2) + 0.4*math.Sin(radS2)
}

func (t TideModel) NextEvent(hours, phaseOffset float64) (nextHigh, nextLow struct{ t, lvl float64 }) {
	step := 0.05
	var times []float64
	var levels []float64
	for i := 0; i < int(12*60/(step*60))+1; i++ {
		tt := hours + float64(i)*step
		times = append(times, tt)
		levels = append(levels, t.Level(tt, phaseOffset))
	}
	var highTimes, lowTimes []struct{ t, lvl float64 }
	for i := 1; i < len(times)-1; i++ {
		if levels[i] > levels[i-1] && levels[i] > levels[i+1] {
			highTimes = append(highTimes, struct{ t, lvl float64 }{times[i], levels[i]})
		} else if levels[i] < levels[i-1] && levels[i] < levels[i+1] {
			lowTimes = append(lowTimes, struct{ t, lvl float64 }{times[i], levels[i]})
		}
	}
	// Find next from now
	for _, h := range highTimes {
		if h.t >= hours {
			nextHigh = h
			break
		}
	}
	if nextHigh.t == 0 && len(highTimes) > 0 {
		nextHigh = highTimes[0]
		nextHigh.t += 24
	}
	for _, l := range lowTimes {
		if l.t >= hours {
			nextLow = l
			break
		}
	}
	if nextLow.t == 0 && len(lowTimes) > 0 {
		nextLow = lowTimes[0]
		nextLow.t += 24
	}
	return
}

type Notifier struct {
	Locations []Location `json:"locations"`
}

func (n *Notifier) load() {
	data, err := os.ReadFile(ConfigFile)
	if err != nil {
		return
	}
	json.Unmarshal(data, n)
}

func (n *Notifier) save() {
	data, _ := json.MarshalIndent(n, "", "  ")
	os.WriteFile(ConfigFile, data, 0644)
}

func (n *Notifier) AddLocation(name string, phaseOffset float64, threshold int) {
	if threshold == 0 {
		threshold = DefaultThreshold
	}
	// Remove if exists
	var newLocs []Location
	for _, loc := range n.Locations {
		if loc.Name != name {
			newLocs = append(newLocs, loc)
		}
	}
	newLocs = append(newLocs, Location{Name: name, PhaseOffset: phaseOffset, Threshold: threshold})
	n.Locations = newLocs
	n.save()
	fmt.Printf("✅ Location '%s' added.\n", name)
}

func (n *Notifier) RemoveLocation(name string) {
	var newLocs []Location
	for _, loc := range n.Locations {
		if loc.Name != name {
			newLocs = append(newLocs, loc)
		}
	}
	n.Locations = newLocs
	n.save()
	fmt.Printf("✅ Location '%s' removed.\n", name)
}

func (n *Notifier) ListLocations() {
	if len(n.Locations) == 0 {
		fmt.Println("No locations.")
		return
	}
	fmt.Println("\n📍 Monitored locations:")
	for _, loc := range n.Locations {
		fmt.Printf("  %s (phase: %.2fh, threshold: %d min)\n", loc.Name, loc.PhaseOffset, loc.Threshold)
	}
}

func (n *Notifier) getStatus(hours float64) []struct {
	loc       Location
	lvl       float64
	nextType  string
	nextTime  float64
	nextLevel float64
} {
	var statuses []struct {
		loc       Location
		lvl       float64
		nextType  string
		nextTime  float64
		nextLevel float64
	}
	tm := TideModel{}
	for _, loc := range n.Locations {
		lvl := tm.Level(hours, loc.PhaseOffset)
		high, low := tm.NextEvent(hours, loc.PhaseOffset)
		var nextType string
		var nextTime, nextLevel float64
		if high.t > 0 && low.t > 0 {
			if high.t < low.t {
				nextType = "High"
				nextTime = high.t
				nextLevel = high.lvl
			} else {
				nextType = "Low"
				nextTime = low.t
				nextLevel = low.lvl
			}
		} else if high.t > 0 {
			nextType = "High"
			nextTime = high.t
			nextLevel = high.lvl
		} else if low.t > 0 {
			nextType = "Low"
			nextTime = low.t
			nextLevel = low.lvl
		}
		statuses = append(statuses, struct {
			loc       Location
			lvl       float64
			nextType  string
			nextTime  float64
			nextLevel float64
		}{loc, lvl, nextType, nextTime, nextLevel})
	}
	return statuses
}

func (n *Notifier) Monitor(interval int) {
	fmt.Printf("🌊 Tide Notifier (checking every %ds)\n", interval)
	fmt.Println("Press Ctrl+C to stop.\n")
	sigChan := make(chan os.Signal, 1)
	signal.Notify(sigChan, syscall.SIGINT, syscall.SIGTERM)

	ticker := time.NewTicker(time.Duration(interval) * time.Second)
	go func() {
		for range ticker.C {
			now := time.Now()
			hours := float64(now.Hour()) + float64(now.Minute())/60.0 + float64(now.Second())/3600.0
			statuses := n.getStatus(hours)
			for _, st := range statuses {
				if st.nextType != "" {
					minutesUntil := (st.nextTime - hours) * 60
					if minutesUntil < 0 {
						minutesUntil += 24 * 60
					}
					if minutesUntil <= float64(st.loc.Threshold) && minutesUntil > 0 {
						fmt.Printf("[%s] %s – %s tide in %.0f min (level: %.2f m)\n",
							now.Format("2006-01-02 15:04"), st.loc.Name, st.nextType, minutesUntil, st.nextLevel)
					}
				}
			}
		}
	}()
	<-sigChan
	fmt.Println("\nMonitoring stopped.")
	ticker.Stop()
}

func (n *Notifier) ShowStatus() {
	now := time.Now()
	hours := float64(now.Hour()) + float64(now.Minute())/60.0 + float64(now.Second())/3600.0
	statuses := n.getStatus(hours)
	fmt.Printf("\n🌊 Tide Status (%s)\n", now.Format("2006-01-02 15:04"))
	for _, st := range statuses {
		fmt.Printf("\n📍 %s\n", st.loc.Name)
		fmt.Printf("  Phase offset: %.2fh\n", st.loc.PhaseOffset)
		fmt.Printf("  Current level: %.2f m\n", st.lvl)
		if st.nextType != "" {
			minutesUntil := (st.nextTime - hours) * 60
			if minutesUntil < 0 {
				minutesUntil += 24 * 60
			}
			hour := int(st.nextTime)
			min := int((st.nextTime - float64(hour)) * 60)
			fmt.Printf("  Next event: %s tide at %02d:%02d (%.2f m) – in %.0f min\n",
				st.nextType, hour, min, st.nextLevel, minutesUntil)
		}
		fmt.Printf("  Alert threshold: %d min\n", st.loc.Threshold)
	}
}

func main() {
	if len(os.Args) < 2 {
		fmt.Println("Usage: tide_notifier <command> [options]")
		return
	}
	notifier := &Notifier{}
	notifier.load()

	cmd := os.Args[1]
	switch cmd {
	case "add":
		addCmd := flag.NewFlagSet("add", flag.ExitOnError)
		name := addCmd.String("name", "", "")
		phase := addCmd.Float64("phase", 0, "")
		threshold := addCmd.Int("threshold", 0, "")
		addCmd.Parse(os.Args[2:])
		// parse positional args
		args := addCmd.Args()
		if len(args) < 2 {
			fmt.Println("Usage: add <name> <phase_offset> [--threshold <minutes>]")
			return
		}
		nameStr := args[0]
		phaseStr := args[1]
		phaseVal, _ := strconv.ParseFloat(phaseStr, 64)
		thr := *threshold
		if thr == 0 {
			thr = DefaultThreshold
		}
		notifier.AddLocation(nameStr, phaseVal, thr)
	case "list":
		notifier.ListLocations()
	case "remove":
		if len(os.Args) < 3 {
			fmt.Println("remove <name>")
			return
		}
		notifier.RemoveLocation(os.Args[2])
	case "start":
		startCmd := flag.NewFlagSet("start", flag.ExitOnError)
		interval := startCmd.Int("interval", DefaultInterval, "Check interval in seconds")
		startCmd.Parse(os.Args[2:])
		notifier.Monitor(*interval)
	case "status":
		notifier.ShowStatus()
	default:
		fmt.Println("Unknown command. Available: add, list, remove, start, status")
	}
}
