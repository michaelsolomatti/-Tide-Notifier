// tide_notifier.js
#!/usr/bin/env node
const fs = require('fs');
const path = require('path');
const { program } = require('commander');

const CONFIG_FILE = 'tide_notifier_config.json';
const DEFAULT_INTERVAL = 60;
const DEFAULT_THRESHOLD = 30;

class TideModel {
    static level(hours, phaseOffset = 0) {
        const M2_PERIOD = 12.42;
        const S2_PERIOD = 12.0;
        const radM2 = (hours / M2_PERIOD + phaseOffset / M2_PERIOD) * 2 * Math.PI;
        const radS2 = (hours / S2_PERIOD + phaseOffset / S2_PERIOD) * 2 * Math.PI;
        return 0.0 + 1.2 * Math.sin(radM2) + 0.4 * Math.sin(radS2);
    }

    static nextEvent(hours, phaseOffset = 0) {
        const step = 0.05;
        const times = [];
        const levels = [];
        for (let i = 0; i < Math.floor(12 * 60 / (step * 60)) + 1; i++) {
            const t = hours + i * step;
            times.push(t);
            levels.push(this.level(t, phaseOffset));
        }
        let highTimes = [], lowTimes = [];
        for (let i = 1; i < times.length - 1; i++) {
            if (levels[i] > levels[i-1] && levels[i] > levels[i+1]) {
                highTimes.push({t: times[i], lvl: levels[i]});
            } else if (levels[i] < levels[i-1] && levels[i] < levels[i+1]) {
                lowTimes.push({t: times[i], lvl: levels[i]});
            }
        }
        let nextHigh = null, nextLow = null;
        for (const h of highTimes) {
            if (h.t >= hours) {
                nextHigh = h;
                break;
            }
        }
        if (!nextHigh && highTimes.length > 0) {
            nextHigh = {t: highTimes[0].t + 24, lvl: highTimes[0].lvl};
        }
        for (const l of lowTimes) {
            if (l.t >= hours) {
                nextLow = l;
                break;
            }
        }
        if (!nextLow && lowTimes.length > 0) {
            nextLow = {t: lowTimes[0].t + 24, lvl: lowTimes[0].lvl};
        }
        return { nextHigh, nextLow };
    }
}

class Notifier {
    constructor() {
        this.locations = [];
        this.load();
    }

    load() {
        if (fs.existsSync(CONFIG_FILE)) {
            try {
                this.locations = JSON.parse(fs.readFileSync(CONFIG_FILE));
            } catch (e) {}
        }
    }

    save() {
        fs.writeFileSync(CONFIG_FILE, JSON.stringify(this.locations, null, 2));
    }

    addLocation(name, phaseOffset, threshold = DEFAULT_THRESHOLD) {
        this.locations = this.locations.filter(l => l.name !== name);
        this.locations.push({ name, phase_offset: phaseOffset, threshold });
        this.save();
        console.log(`✅ Location '${name}' added.`);
    }

    removeLocation(name) {
        this.locations = this.locations.filter(l => l.name !== name);
        this.save();
        console.log(`✅ Location '${name}' removed.`);
    }

    listLocations() {
        if (!this.locations.length) {
            console.log('No locations.');
            return;
        }
        console.log('\n📍 Monitored locations:');
        for (const loc of this.locations) {
            console.log(`  ${loc.name} (phase: ${loc.phase_offset.toFixed(2)}h, threshold: ${loc.threshold} min)`);
        }
    }

    getStatus(hours) {
        return this.locations.map(loc => {
            const lvl = TideModel.level(hours, loc.phase_offset);
            const { nextHigh, nextLow } = TideModel.nextEvent(hours, loc.phase_offset);
            let nextType = '', nextTime = null, nextLevel = null;
            if (nextHigh && nextLow) {
                if (nextHigh.t < nextLow.t) {
                    nextType = 'High';
                    nextTime = nextHigh.t;
                    nextLevel = nextHigh.lvl;
                } else {
                    nextType = 'Low';
                    nextTime = nextLow.t;
                    nextLevel = nextLow.lvl;
                }
            } else if (nextHigh) {
                nextType = 'High';
                nextTime = nextHigh.t;
                nextLevel = nextHigh.lvl;
            } else if (nextLow) {
                nextType = 'Low';
                nextTime = nextLow.t;
                nextLevel = nextLow.lvl;
            }
            return { loc, lvl, nextType, nextTime, nextLevel };
        });
    }

    monitor(interval = DEFAULT_INTERVAL) {
        console.log(`🌊 Tide Notifier (checking every ${interval}s)`);
        console.log('Press Ctrl+C to stop.\n');
        const timer = setInterval(() => {
            const now = new Date();
            const hours = now.getHours() + now.getMinutes()/60 + now.getSeconds()/3600;
            const statuses = this.getStatus(hours);
            for (const st of statuses) {
                if (st.nextType) {
                    let minutesUntil = (st.nextTime - hours) * 60;
                    if (minutesUntil < 0) minutesUntil += 24*60;
                    if (minutesUntil <= st.loc.threshold && minutesUntil > 0) {
                        console.log(`[${now.toISOString().slice(0,16).replace('T',' ')}] ${st.loc.name} – ${st.nextType} tide in ${Math.floor(minutesUntil)} min (level: ${st.nextLevel.toFixed(2)} m)`);
                    }
                }
            }
        }, interval * 1000);

        process.on('SIGINT', () => {
            clearInterval(timer);
            console.log('\nMonitoring stopped.');
            process.exit();
        });
    }

    showStatus() {
        const now = new Date();
        const hours = now.getHours() + now.getMinutes()/60 + now.getSeconds()/3600;
        const statuses = this.getStatus(hours);
        console.log(`\n🌊 Tide Status (${now.toISOString().slice(0,16).replace('T',' ')})`);
        for (const st of statuses) {
            console.log(`\n📍 ${st.loc.name}`);
            console.log(`  Phase offset: ${st.loc.phase_offset.toFixed(2)}h`);
            console.log(`  Current level: ${st.lvl.toFixed(2)} m`);
            if (st.nextType) {
                let minutesUntil = (st.nextTime - hours) * 60;
                if (minutesUntil < 0) minutesUntil += 24*60;
                const hour = Math.floor(st.nextTime);
                const min = Math.floor((st.nextTime - hour) * 60);
                console.log(`  Next event: ${st.nextType} tide at ${String(hour).padStart(2,'0')}:${String(min).padStart(2,'0')} (${st.nextLevel.toFixed(2)} m) – in ${Math.floor(minutesUntil)} min`);
            }
            console.log(`  Alert threshold: ${st.loc.threshold} min`);
        }
    }
}

program
    .command('add <name> <phase_offset>')
    .option('--threshold <minutes>', 'Alert threshold', parseInt, DEFAULT_THRESHOLD)
    .action((name, phaseOffset, options) => {
        const notifier = new Notifier();
        notifier.addLocation(name, parseFloat(phaseOffset), options.threshold);
    });

program
    .command('list')
    .action(() => {
        const notifier = new Notifier();
        notifier.listLocations();
    });

program
    .command('remove <name>')
    .action((name) => {
        const notifier = new Notifier();
        notifier.removeLocation(name);
    });

program
    .command('start')
    .option('--interval <seconds>', 'Check interval', parseInt, DEFAULT_INTERVAL)
    .action((options) => {
        const notifier = new Notifier();
        notifier.monitor(options.interval);
    });

program
    .command('status')
    .action(() => {
        const notifier = new Notifier();
        notifier.showStatus();
    });

program.parse(process.argv);
