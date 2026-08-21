🌊 Tide Notifier — Multi‑Language Tide Alert System
8 languages, one smart tide monitor – track tide levels and receive notifications before high or low tides, right from your terminal.

✨ Features
📍 Add locations – store places with custom phase offsets to simulate different tide timings

⏰ Set alert threshold – get notified when the next high or low tide is approaching (default: 30 minutes)

🔔 Console notifications – see upcoming tide events with time remaining

📊 Current status – display real‑time tide level and next event

💾 Persistent configuration – settings saved to a local JSON file

🖥️ Cross‑platform – works on Windows, macOS, Linux

🚀 Quick Start
bash
# Add a location (name, phase offset in hours, optional alert threshold in minutes)
<command> add "Sydney" 2.5
<command> add "London" 0.0 --threshold 45

# List all monitored locations
<command> list

# Remove a location
<command> remove "Sydney"

# Start monitoring (runs until interrupted)
<command> start

# Show current tide status for all locations
<command> status
Arguments:

add <name> <phase_offset> [--threshold <minutes>] – add a location

list – show all locations

remove <name> – delete a location

start – begin monitoring (runs in a loop)

status – show current tide info for all locations

📸 Example Output
text
🌊 Tide Notifier
Monitoring: Sydney, London

[2026-08-21 14:30] Sydney – High tide in 15 min (level: 1.8 m)
[2026-08-21 14:30] London – Low tide in 42 min (level: 0.3 m)

📍 Sydney
  Phase offset: 2.5h
  Current level: 1.2 m
  Next event: High tide at 14:45 (1.8 m)
  Alert threshold: 30 min

📍 London
  Phase offset: 0.0h
  Current level: 0.8 m
  Next event: Low tide at 15:12 (0.2 m)
  Alert threshold: 45 min
📁 Repository Structure
text
.
├── README.md
├── python/
│   └── tide_notifier.py
├── go/
│   └── tide_notifier.go
├── javascript/
│   └── tide_notifier.js
├── ruby/
│   └── tide_notifier.rb
├── php/
│   └── tide_notifier.php
├── java/
│   └── TideNotifier.java
├── csharp/
│   └── TideNotifier.cs
└── cpp/
    └── tide_notifier.cpp
