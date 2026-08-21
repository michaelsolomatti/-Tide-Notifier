# tide_notifier.php
#!/usr/bin/env php
<?php

define('CONFIG_FILE', 'tide_notifier_config.json');
define('DEFAULT_INTERVAL', 60);
define('DEFAULT_THRESHOLD', 30);

class TideModel {
    public static function level($hours, $phaseOffset = 0.0) {
        $M2_PERIOD = 12.42;
        $S2_PERIOD = 12.0;
        $radM2 = ($hours / $M2_PERIOD + $phaseOffset / $M2_PERIOD) * 2 * M_PI;
        $radS2 = ($hours / $S2_PERIOD + $phaseOffset / $S2_PERIOD) * 2 * M_PI;
        return 0.0 + 1.2 * sin($radM2) + 0.4 * sin($radS2);
    }

    public static function nextEvent($hours, $phaseOffset = 0.0) {
        $step = 0.05;
        $times = [];
        $levels = [];
        for ($i = 0; $i <= (int)(12*60/($step*60)); $i++) {
            $t = $hours + $i * $step;
            $times[] = $t;
            $levels[] = self::level($t, $phaseOffset);
        }
        $highTimes = [];
        $lowTimes = [];
        for ($i = 1; $i < count($times)-1; $i++) {
            if ($levels[$i] > $levels[$i-1] && $levels[$i] > $levels[$i+1]) {
                $highTimes[] = ['t' => $times[$i], 'lvl' => $levels[$i]];
            } elseif ($levels[$i] < $levels[$i-1] && $levels[$i] < $levels[$i+1]) {
                $lowTimes[] = ['t' => $times[$i], 'lvl' => $levels[$i]];
            }
        }
        $nextHigh = null;
        $nextLow = null;
        foreach ($highTimes as $h) {
            if ($h['t'] >= $hours) { $nextHigh = $h; break; }
        }
        if (!$nextHigh && !empty($highTimes)) {
            $nextHigh = ['t' => $highTimes[0]['t'] + 24, 'lvl' => $highTimes[0]['lvl']];
        }
        foreach ($lowTimes as $l) {
            if ($l['t'] >= $hours) { $nextLow = $l; break; }
        }
        if (!$nextLow && !empty($lowTimes)) {
            $nextLow = ['t' => $lowTimes[0]['t'] + 24, 'lvl' => $lowTimes[0]['lvl']];
        }
        return ['high' => $nextHigh, 'low' => $nextLow];
    }
}

class Notifier {
    private $locations = [];
    private $file;

    public function __construct($file) {
        $this->file = $file;
        $this->load();
    }

    private function load() {
        if (file_exists($this->file)) {
            $data = file_get_contents($this->file);
            $this->locations = json_decode($data, true) ?? [];
        }
    }

    private function save() {
        file_put_contents($this->file, json_encode($this->locations, JSON_PRETTY_PRINT));
    }

    public function addLocation($name, $phaseOffset, $threshold = DEFAULT_THRESHOLD) {
        $this->locations = array_filter($this->locations, function($loc) use ($name) {
            return $loc['name'] != $name;
        });
        $this->locations[] = ['name' => $name, 'phase_offset' => $phaseOffset, 'threshold' => $threshold];
        $this->save();
        echo "✅ Location '$name' added.\n";
    }

    public function removeLocation($name) {
        $this->locations = array_filter($this->locations, function($loc) use ($name) {
            return $loc['name'] != $name;
        });
        $this->save();
        echo "✅ Location '$name' removed.\n";
    }

    public function listLocations() {
        if (empty($this->locations)) {
            echo "No locations.\n";
            return;
        }
        echo "\n📍 Monitored locations:\n";
        foreach ($this->locations as $loc) {
            printf("  %s (phase: %.2fh, threshold: %d min)\n", $loc['name'], $loc['phase_offset'], $loc['threshold']);
        }
    }

    private function getStatus($hours) {
        $statuses = [];
        foreach ($this->locations as $loc) {
            $lvl = TideModel::level($hours, $loc['phase_offset']);
            $events = TideModel::nextEvent($hours, $loc['phase_offset']);
            $nextType = null;
            $nextTime = null;
            $nextLevel = null;
            if ($events['high'] && $events['low']) {
                if ($events['high']['t'] < $events['low']['t']) {
                    $nextType = 'High';
                    $nextTime = $events['high']['t'];
                    $nextLevel = $events['high']['lvl'];
                } else {
                    $nextType = 'Low';
                    $nextTime = $events['low']['t'];
                    $nextLevel = $events['low']['lvl'];
                }
            } elseif ($events['high']) {
                $nextType = 'High';
                $nextTime = $events['high']['t'];
                $nextLevel = $events['high']['lvl'];
            } elseif ($events['low']) {
                $nextType = 'Low';
                $nextTime = $events['low']['t'];
                $nextLevel = $events['low']['lvl'];
            }
            $statuses[] = ['loc' => $loc, 'lvl' => $lvl, 'nextType' => $nextType, 'nextTime' => $nextTime, 'nextLevel' => $nextLevel];
        }
        return $statuses;
    }

    public function monitor($interval = DEFAULT_INTERVAL) {
        echo "🌊 Tide Notifier (checking every {$interval}s)\n";
        echo "Press Ctrl+C to stop.\n\n";
        while (true) {
            $now = new DateTime();
            $hours = (int)$now->format('H') + (int)$now->format('i')/60 + (int)$now->format('s')/3600;
            $statuses = $this->getStatus($hours);
            foreach ($statuses as $st) {
                if ($st['nextType']) {
                    $minutesUntil = ($st['nextTime'] - $hours) * 60;
                    if ($minutesUntil < 0) $minutesUntil += 24*60;
                    if ($minutesUntil <= $st['loc']['threshold'] && $minutesUntil > 0) {
                        printf("[%s] %s – %s tide in %d min (level: %.2f m)\n",
                            $now->format('Y-m-d H:i'), $st['loc']['name'], $st['nextType'], (int)$minutesUntil, $st['nextLevel']);
                    }
                }
            }
            sleep($interval);
        }
    }

    public function showStatus() {
        $now = new DateTime();
        $hours = (int)$now->format('H') + (int)$now->format('i')/60 + (int)$now->format('s')/3600;
        $statuses = $this->getStatus($hours);
        echo "\n🌊 Tide Status (" . $now->format('Y-m-d H:i') . ")\n";
        foreach ($statuses as $st) {
            echo "\n📍 " . $st['loc']['name'] . "\n";
            printf("  Phase offset: %.2fh\n", $st['loc']['phase_offset']);
            printf("  Current level: %.2f m\n", $st['lvl']);
            if ($st['nextType']) {
                $minutesUntil = ($st['nextTime'] - $hours) * 60;
                if ($minutesUntil < 0) $minutesUntil += 24*60;
                $hour = (int)$st['nextTime'];
                $min = (int)(($st['nextTime'] - $hour) * 60);
                printf("  Next event: %s tide at %02d:%02d (%.2f m) – in %d min\n",
                    $st['nextType'], $hour, $min, $st['nextLevel'], (int)$minutesUntil);
            }
            printf("  Alert threshold: %d min\n", $st['loc']['threshold']);
        }
    }
}

if ($argc < 2) {
    die("Usage: php tide_notifier.php <command> [options]\n");
}

$notifier = new Notifier(CONFIG_FILE);
$cmd = $argv[1];

switch ($cmd) {
    case 'add':
        if ($argc < 4) die("add <name> <phase_offset> [--threshold <minutes>]\n");
        $name = $argv[2];
        $phase = (float)$argv[3];
        $threshold = DEFAULT_THRESHOLD;
        for ($i=4; $i<$argc; $i++) {
            if ($argv[$i] == '--threshold' && isset($argv[$i+1])) {
                $threshold = (int)$argv[++$i];
            }
        }
        $notifier->addLocation($name, $phase, $threshold);
        break;
    case 'list':
        $notifier->listLocations();
        break;
    case 'remove':
        if ($argc < 3) die("remove <name>\n");
        $notifier->removeLocation($argv[2]);
        break;
    case 'start':
        $interval = DEFAULT_INTERVAL;
        for ($i=2; $i<$argc; $i++) {
            if ($argv[$i] == '--interval' && isset($argv[$i+1])) {
                $interval = (int)$argv[++$i];
            }
        }
        $notifier->monitor($interval);
        break;
    case 'status':
        $notifier->showStatus();
        break;
    default:
        echo "Unknown command. Available: add, list, remove, start, status\n";
}
?>
