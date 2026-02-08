# RAID State Monitor

Automated monitoring system for JMicron RAID controller state changes.

## Features

- **Automatic monitoring** - Checks RAID state every 5 minutes
- **Email alerts** - Sends email when state changes detected
- **Detailed logging** - Records every check with timestamps and comparisons
- **State history** - Saves full protocol dumps for each state change
- **Easy control** - Simple start/stop/status commands

## Quick Start

### 0. Setup Passwordless Sudo (Required for Background Monitoring)

The monitor runs jmraidstatus with sudo, which requires a password by default. To allow background monitoring without password prompts, add a sudoers rule:

```bash
# Create sudoers rule file
cat > /tmp/jmraidstatus_sudoers << 'EOF'
# Allow running jmraidstatus without password for monitoring
jamiet ALL=(ALL) NOPASSWD: /home/jamiet/code/jm-raid-status/bin/jmraidstatus
EOF

# Validate syntax (must say "parsed OK")
sudo visudo -c -f /tmp/jmraidstatus_sudoers

# If validation passes, install it
sudo cp /tmp/jmraidstatus_sudoers /etc/sudoers.d/jmraidstatus
sudo chmod 440 /etc/sudoers.d/jmraidstatus

# Test it works
sudo /home/jamiet/code/jm-raid-status/bin/jmraidstatus /dev/sde
```

**Important:**
- Replace `jamiet` with your username if different
- Replace the full path to jmraidstatus with your actual installation path
- This only affects this specific binary, not all sudo commands

**Why this is needed:** Background daemons cannot prompt for passwords. Without this rule, the monitor will work for ~5 minutes (until sudo credentials expire) then fail silently.

### 1. (Optional) Import Initial State

If you have an existing state capture from your investigation, import it as the baseline:

```bash
./import_state.sh ../data/post_rebuild_blue_led_2.txt
```

This allows the monitor to compare against a known state rather than treating the first check as baseline.

### 2. Start Monitoring

```bash
./monitor.sh start          # Default: 1 minute interval
# or
./monitor.sh start 30       # Custom: 30 second interval
# or
./monitor.sh start 300      # Custom: 5 minute interval
```

The monitor will:
- Check RAID state immediately
- Continue checking at specified interval (default: 1 minute)
- Send email alerts on state changes
- Log all activity to `monitor.log`
- Use imported state or most recent state file as baseline (if available)

### 2. Check Status

```bash
./monitor.sh status
```

Shows:
- Running/stopped status
- Process ID
- Recent log entries

### 3. View Live Logs

```bash
./monitor.sh log
```

Tails the log file in real-time (Ctrl+C to exit).

### 4. Stop Monitoring

```bash
./monitor.sh stop
```

## Commands

### Monitor Control

| Command | Description |
|---------|-------------|
| `./monitor.sh start [interval]` | Start the monitor daemon (default: 60s) |
| `./monitor.sh stop` | Stop the monitor daemon |
| `./monitor.sh restart [interval]` | Restart the monitor daemon |
| `./monitor.sh status` | Show current status, interval, and recent logs |
| `./monitor.sh check` | Run immediate check (doesn't affect daemon) |
| `./monitor.sh log` | Tail the log file (Ctrl+C to exit) |

**Interval parameter:** Time in seconds between checks (e.g., 30, 60, 300). Default is 60 seconds (1 minute).

### State Management

| Command | Description |
|---------|-------------|
| `./import_state.sh <file>` | Import existing state file as baseline |

## How It Works

### State Detection

The monitor extracts and tracks three key protocol flags:

- **0x1F0** - RAID health status
  - `0x07` = Degraded (disk missing)
  - `0x0F` = Operational (all disks present)

- **0x1F5** - Rebuild status
  - `0x00` = Idle/healthy
  - `0x01` = Rebuilding

- **0x1FA** - Rebuild phase
  - `0x00` = Phase 1 (initial rebuild)
  - `0x01` = Phase 2 (verification)

### Initial State Loading

On first run, the monitor needs a baseline state for comparison. It checks in this order:

1. **last_state.txt** - If exists, use this cached state
2. **Most recent state file** - If no last_state.txt, find newest file in states/
3. **First check** - If no state files exist, first check becomes baseline

To provide a specific initial state:
```bash
./import_state.sh ../data/your_state_file.txt
```

The import script:
- Copies the file to `states/` with timestamp
- Extracts and displays the state for verification
- Next monitor run will use it as baseline

This is useful when:
- Continuing monitoring after investigation
- Starting from a known good state
- Initializing with current rebuilding state

### State Changes That Trigger Alerts

- Degraded → Operational (disk reinserted)
- Operational → Degraded (disk failed/removed)
- Idle → Rebuilding (rebuild started)
- Rebuilding Phase 1 → Phase 2 (verification started)
- Rebuilding → Idle (rebuild completed)

### Email Configuration

Email settings are read from `../../.env`:

```
smtp_server: smtp.gmail.com:465
ecrypttion: ssl
authuser: your-email@gmail.com
authpass: your-app-password
```

Emails are sent to the same address configured in `authuser`.

## File Structure

```
tests/monitor/
├── README.md              # This file
├── monitor.sh             # Control script (start/stop/status)
├── monitor_daemon.sh      # Background daemon runner
├── raid_monitor.py        # Main monitoring logic
├── import_state.sh        # Import existing state file as baseline
├── monitor.log            # Activity log (created on first run)
├── monitor.pid            # Process ID file (when running)
├── last_state.txt         # Last known state (for comparison)
└── states/                # State capture history
    ├── imported_post_rebuild_blue_led_2_20260207_132100.txt
    ├── operational_idle_20260207_133000.txt
    ├── operational_rebuilding_phase_1_20260207_133500.txt
    └── ...
```

## Log Format

Each check logs:
- Timestamp
- Current state interpretation
- Flag values (0x1F0, 0x1F5, 0x1FA)
- Comparison with last state
- State file saved
- Email sent (if state changed)

Example log entries:

```
[2026-02-07 13:30:00] ============================================================
[2026-02-07 13:30:00] Starting RAID state check
[2026-02-07 13:30:01] Current state: OPERATIONAL + REBUILDING_PHASE_2
[2026-02-07 13:30:01] Flags: 0x1F0=0f, 0x1F5=01, 0x1FA=01
[2026-02-07 13:30:01] Comparison: 0x1FA: 00 -> 01
[2026-02-07 13:30:01] State saved to: operational_rebuilding_phase_2_20260207_133001.txt
[2026-02-07 13:30:01] Last state: OPERATIONAL + REBUILDING_PHASE_1 (from operational_rebuilding_phase_1_20260207_132500.txt)
[2026-02-07 13:30:01] Current state: OPERATIONAL + REBUILDING_PHASE_2 (saved to operational_rebuilding_phase_2_20260207_133001.txt)
[2026-02-07 13:30:01] STATE CHANGE DETECTED: 0x1FA: 00 -> 01
[2026-02-07 13:30:02] Email sent: RAID State Changed: OPERATIONAL + REBUILDING_PHASE_1 -> OPERATIONAL + REBUILDING_PHASE_2
[2026-02-07 13:30:02] Check complete
[2026-02-07 13:30:02] ============================================================
```

## State Files

Each state change is saved to `states/` with:
- Timestamp in filename
- State interpretation
- Flag values
- Full raw protocol dump

This provides complete history for later analysis.

## Troubleshooting

### Monitor stops working after a few minutes

**Symptom:** Log shows "ERROR: Failed to extract flags from output" and "jmraidstatus returned exit code 3"

**Cause:** Sudo credentials expired (default timeout is ~5 minutes)

**Solution:** Install the sudoers rule (see Setup section above) to allow passwordless sudo for jmraidstatus

**To verify:**
```bash
# Check recent log entries
./monitor.sh status

# If you see exit code 3, you need the sudoers rule
tail -20 monitor.log | grep "exit code"
```

### Monitor won't start

Check that:
- You have sudo access (required for jmraidstatus)
- The binary exists at `../../bin/jmraidstatus`
- Python 3 is installed
- Sudoers rule is installed (for background operation)

### No email notifications

Check that:
- `.env` file exists at project root with correct settings
- Gmail app password is valid (not regular password)
- SMTP settings are correct

Monitor will continue running and logging even if email fails.

### Missing state changes

The monitor checks every 5 minutes. Very brief state transitions might be missed. Use `./monitor.sh check` for manual verification.

## Manual Testing

To test email notifications without waiting:

```bash
# Run a single check
./monitor.sh check

# If state hasn't changed, manually edit last_state.txt to trigger an alert
# Then run another check
./monitor.sh check
```

## Integration

To run monitor on system startup, add to crontab:

```bash
@reboot cd /home/jamiet/code/jm-raid-status/tests/monitor && ./monitor.sh start
```

## Notes

- **IMPORTANT:** Monitor requires passwordless sudo for jmraidstatus (see Setup section)
  - Without sudoers rule: Monitor will fail after ~5 minutes when sudo credentials expire
  - With sudoers rule: Monitor can run indefinitely in background
- Monitor must run as user with sudo access to jmraidstatus
- Each check takes ~2-3 seconds to complete
- State files are never automatically deleted (manage manually)
- Log file grows indefinitely (consider logrotate for production)
- Default check interval is 1 minute, configurable via command line argument

---

*For protocol details, see `../../PROTOCOL.md`*
*For investigation findings, see `../data/investigation.md`*
