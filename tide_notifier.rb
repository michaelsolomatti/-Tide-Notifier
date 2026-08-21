# tide_notifier.rb
#!/usr/bin/env ruby
require 'json'
require 'optparse'
require 'time'

CONFIG_FILE = 'tide_notifier_config.json'
DEFAULT_INTERVAL = 60
DEFAULT_THRESHOLD = 30

class TideModel
  def self.level(hours, phase_offset = 0.0)
    m2_period = 12.42
    s2_period = 12.0
    rad_m2 = (hours / m2_period + phase_offset / m2_period) * 2 * Math::PI
    rad_s2 = (hours / s2_period + phase_offset / s2_period) * 2 * Math::PI
    0.0 + 1.2 * Math.sin(rad_m2) + 0.4 * Math.sin(rad_s2)
  end

  def self.next_event(hours, phase_offset = 0.0)
    step = 0.05
    times = []
    levels = []
    (0..(12*60 / (step*60)).to_i).each do |i|
      t = hours + i * step
      times << t
      levels << level(t, phase_offset)
    end
    high_times = []
    low_times = []
    (1...times.length-1).each do |i|
      if levels[i] > levels[i-1] && levels[i] > levels[i+1]
        high_times << {t: times[i], lvl: levels[i]}
      elsif levels[i] < levels[i-1] && levels[i] < levels[i+1]
        low_times << {t: times[i], lvl: levels[i]}
      end
    end
    next_high = high_times.find { |h| h[:t] >= hours }
    next_low = low_times.find { |l| l[:t] >= hours }
    if next_high.nil? && !high_times.empty?
      next_high = {t: high_times.first[:t] + 24, lvl: high_times.first[:lvl]}
    end
    if next_low.nil? && !low_times.empty?
      next_low = {t: low_times.first[:t] + 24, lvl: low_times.first[:lvl]}
    end
    {high: next_high, low: next_low}
  end
end

class Notifier
  attr_reader :locations

  def initialize
    @locations = []
    load
  end

  def load
    if File.exist?(CONFIG_FILE)
      begin
        @locations = JSON.parse(File.read(CONFIG_FILE))
      rescue
        @locations = []
      end
    end
  end

  def save
    File.write(CONFIG_FILE, JSON.pretty_generate(@locations))
  end

  def add_location(name, phase_offset, threshold = DEFAULT_THRESHOLD)
    @locations.reject! { |loc| loc['name'] == name }
    @locations << {'name' => name, 'phase_offset' => phase_offset, 'threshold' => threshold}
    save
    puts "✅ Location '#{name}' added."
  end

  def remove_location(name)
    @locations.reject! { |loc| loc['name'] == name }
    save
    puts "✅ Location '#{name}' removed."
  end

  def list_locations
    if @locations.empty?
      puts "No locations."
      return
    end
    puts "\n📍 Monitored locations:"
    @locations.each do |loc|
      puts "  #{loc['name']} (phase: #{loc['phase_offset'].round(2)}h, threshold: #{loc['threshold']} min)"
    end
  end

  def get_status(hours)
    @locations.map do |loc|
      lvl = TideModel.level(hours, loc['phase_offset'])
      events = TideModel.next_event(hours, loc['phase_offset'])
      next_type = nil
      next_time = nil
      next_level = nil
      if events[:high] && events[:low]
        if events[:high][:t] < events[:low][:t]
          next_type = 'High'
          next_time = events[:high][:t]
          next_level = events[:high][:lvl]
        else
          next_type = 'Low'
          next_time = events[:low][:t]
          next_level = events[:low][:lvl]
        end
      elsif events[:high]
        next_type = 'High'
        next_time = events[:high][:t]
        next_level = events[:high][:lvl]
      elsif events[:low]
        next_type = 'Low'
        next_time = events[:low][:t]
        next_level = events[:low][:lvl]
      end
      {loc: loc, lvl: lvl, next_type: next_type, next_time: next_time, next_level: next_level}
    end
  end

  def monitor(interval = DEFAULT_INTERVAL)
    puts "🌊 Tide Notifier (checking every #{interval}s)"
    puts "Press Ctrl+C to stop.\n"
    trap('INT') do
      puts "\nMonitoring stopped."
      exit
    end
    loop do
      now = Time.now
      hours = now.hour + now.min/60.0 + now.sec/3600.0
      statuses = get_status(hours)
      statuses.each do |st|
        if st[:next_type]
          minutes_until = (st[:next_time] - hours) * 60
          minutes_until += 24*60 if minutes_until < 0
          if minutes_until <= st[:loc]['threshold'] && minutes_until > 0
            puts "[#{now.strftime('%Y-%m-%d %H:%M')}] #{st[:loc]['name']} – #{st[:next_type]} tide in #{minutes_until.to_i} min (level: #{st[:next_level].round(2)} m)"
          end
        end
      end
      sleep(interval)
    end
  end

  def show_status
    now = Time.now
    hours = now.hour + now.min/60.0 + now.sec/3600.0
    statuses = get_status(hours)
    puts "\n🌊 Tide Status (#{now.strftime('%Y-%m-%d %H:%M')})"
    statuses.each do |st|
      puts "\n📍 #{st[:loc]['name']}"
      puts "  Phase offset: #{st[:loc]['phase_offset'].round(2)}h"
      puts "  Current level: #{st[:lvl].round(2)} m"
      if st[:next_type]
        minutes_until = (st[:next_time] - hours) * 60
        minutes_until += 24*60 if minutes_until < 0
        hour = st[:next_time].to_i
        min = ((st[:next_time] - hour) * 60).to_i
        puts "  Next event: #{st[:next_type]} tide at #{'%02d' % hour}:#{'%02d' % min} (#{st[:next_level].round(2)} m) – in #{minutes_until.to_i} min"
      end
      puts "  Alert threshold: #{st[:loc]['threshold']} min"
    end
  end
end

options = {}
$command = ARGV.shift
if $command.nil?
  puts "Usage: tide_notifier.rb <command> [options]"
  exit 1
end

notifier = Notifier.new

case $command
when 'add'
  name = ARGV.shift
  phase_offset = ARGV.shift.to_f
  threshold = DEFAULT_THRESHOLD
  if ARGV.include?('--threshold')
    idx = ARGV.index('--threshold')
    threshold = ARGV[idx+1].to_i if idx
  end
  notifier.add_location(name, phase_offset, threshold)
when 'list'
  notifier.list_locations
when 'remove'
  name = ARGV.shift
  notifier.remove_location(name)
when 'start'
  interval = DEFAULT_INTERVAL
  if ARGV.include?('--interval')
    idx = ARGV.index('--interval')
    interval = ARGV[idx+1].to_i if idx
  end
  notifier.monitor(interval)
when 'status'
  notifier.show_status
else
  puts "Unknown command. Available: add, list, remove, start, status"
end
