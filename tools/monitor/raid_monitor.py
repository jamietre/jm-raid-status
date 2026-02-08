#!/usr/bin/env python3
"""
JMicron RAID State Monitor
Monitors RAID status flags and sends email alerts on state changes.
"""

import os
import sys
import subprocess
import re
from datetime import datetime
from pathlib import Path

# Import email notifier module
import email_notifier

# Configuration
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent
JMRAIDSTATUS_BIN = PROJECT_ROOT / "bin" / "jmraidstatus"
DEVICE = "/dev/sde"
MONITOR_DIR = SCRIPT_DIR
STATE_DIR = MONITOR_DIR / "states"
LOG_FILE = MONITOR_DIR / "monitor.log"
LAST_STATE_FILE = MONITOR_DIR / "last_state.txt"
DISCONNECT_STATE_FILE = MONITOR_DIR / "disconnect_state.txt"
ENV_FILE = PROJECT_ROOT / ".env"

# Ensure directories exist
STATE_DIR.mkdir(exist_ok=True)


def log(message):
    """Log message to both console and log file."""
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    log_entry = f"[{timestamp}] {message}"
    print(log_entry)
    with open(LOG_FILE, "a") as f:
        f.write(log_entry + "\n")


# Email configuration and sending is now handled by email_notifier module


def is_device_connected():
    """Check if the RAID device exists and is accessible."""
    device_path = Path(DEVICE)
    return device_path.exists()


def get_disconnect_state():
    """Get the last known disconnect state."""
    if not DISCONNECT_STATE_FILE.exists():
        return None

    try:
        with open(DISCONNECT_STATE_FILE, "r") as f:
            return f.read().strip()
    except Exception:
        return None


def set_disconnect_state(state):
    """Save the disconnect state (connected/disconnected)."""
    with open(DISCONNECT_STATE_FILE, "w") as f:
        f.write(state)


def capture_raid_state():
    """Capture current RAID state using jmraidstatus."""
    try:
        # Use sudo env to pass environment variable
        result = subprocess.run(
            ["sudo", "env", "JMRAIDSTATUS_DUMP_RAW=1", str(JMRAIDSTATUS_BIN), DEVICE],
            capture_output=True,
            text=True,
            timeout=30
        )

        if result.returncode != 0:
            log(f"WARNING: jmraidstatus returned exit code {result.returncode}")

        return result.stdout + result.stderr
    except subprocess.TimeoutExpired:
        log("ERROR: jmraidstatus command timed out")
        return None
    except Exception as e:
        log(f"ERROR capturing RAID state: {e}")
        return None


def extract_flags(raw_output):
    """Extract RAID status flags from raw output."""
    # Find all 0x1F0 offset lines (one per disk)
    pattern = r"^01f0:\s+([0-9a-f ]+)"
    matches = re.findall(pattern, raw_output, re.MULTILINE)

    if not matches:
        return None

    # Parse the first disk's flags (all disks should have same RAID-wide flags)
    hex_bytes = matches[0].split()
    if len(hex_bytes) < 6:
        return None

    flags = {
        "0x1F0": hex_bytes[0],  # RAID health status
        "0x1F2": hex_bytes[2],  # Unknown
        "0x1F5": hex_bytes[5],  # Rebuild status
        "0x1FA": hex_bytes[10] if len(hex_bytes) > 10 else "??",  # Rebuild phase
        "raw": " ".join(hex_bytes[:12]) if len(hex_bytes) >= 12 else " ".join(hex_bytes),
        "num_disks": len(matches)
    }

    return flags


def interpret_state(flags):
    """Interpret flags into human-readable state."""
    if not flags:
        return "UNKNOWN"

    state_parts = []

    # RAID health
    if flags["0x1F0"] == "07":
        state_parts.append("DEGRADED")
    elif flags["0x1F0"] == "0f":
        state_parts.append("OPERATIONAL")
    else:
        state_parts.append(f"UNKNOWN_HEALTH({flags['0x1F0']})")

    # Rebuild status
    if flags["0x1F5"] == "01":
        if flags["0x1FA"] == "00":
            state_parts.append("REBUILDING_PHASE_1")
        elif flags["0x1FA"] == "01":
            state_parts.append("REBUILDING_PHASE_2")
        else:
            state_parts.append(f"REBUILDING_UNKNOWN_PHASE({flags['0x1FA']})")
    elif flags["0x1F5"] == "00":
        state_parts.append("IDLE")
    else:
        state_parts.append(f"UNKNOWN_REBUILD({flags['0x1F5']})")

    return " + ".join(state_parts)


def save_state_file(raw_output, flags, state_name):
    """Save state capture to timestamped file."""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    filename = STATE_DIR / f"{state_name}_{timestamp}.txt"

    with open(filename, "w") as f:
        f.write(f"=== RAID State Capture ===\n")
        f.write(f"Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write(f"State: {interpret_state(flags)}\n")
        f.write(f"\nFlags:\n")
        f.write(f"  0x1F0 (Health):  {flags['0x1F0']}\n")
        f.write(f"  0x1F5 (Rebuild): {flags['0x1F5']}\n")
        f.write(f"  0x1FA (Phase):   {flags['0x1FA']}\n")
        f.write(f"  Raw: {flags['raw']}\n")
        f.write(f"  Disks: {flags['num_disks']}\n")
        f.write(f"\n{'='*60}\n\n")
        f.write(raw_output)

    return filename


def load_state_from_file(filepath):
    """Load state from a state file (either last_state.txt or a full state capture)."""
    try:
        with open(filepath, "r") as f:
            content = f.read()

        # Try to parse as last_state.txt format (simple format)
        lines = content.split("\n")
        if lines[0].startswith("State:") and len(lines) >= 5:
            return {
                "0x1F0": lines[1].split(":")[1].strip(),
                "0x1F5": lines[2].split(":")[1].strip(),
                "0x1FA": lines[3].split(":")[1].strip(),
                "raw": lines[4].split(":", 1)[1].strip() if len(lines) > 4 else "",
                "filename": lines[5].split(":", 1)[1].strip() if len(lines) > 5 else Path(filepath).name
            }

        # Try to parse as formatted state capture (from this monitor)
        flags_match = re.search(r"0x1F0 \(Health\):\s+([0-9a-f]+)", content)
        rebuild_match = re.search(r"0x1F5 \(Rebuild\):\s+([0-9a-f]+)", content)
        phase_match = re.search(r"0x1FA \(Phase\):\s+([0-9a-f]+)", content)
        raw_match = re.search(r"Raw:\s+([0-9a-f ]+)", content)

        if flags_match and rebuild_match:
            return {
                "0x1F0": flags_match.group(1),
                "0x1F5": rebuild_match.group(1),
                "0x1FA": phase_match.group(1) if phase_match else "??",
                "raw": raw_match.group(1) if raw_match else "",
                "filename": Path(filepath).name
            }

        # Try to parse as raw protocol dump (01f0: lines)
        flags = extract_flags(content)
        if flags:
            return {
                "0x1F0": flags["0x1F0"],
                "0x1F5": flags["0x1F5"],
                "0x1FA": flags["0x1FA"],
                "raw": flags["raw"],
                "filename": Path(filepath).name
            }

        return None
    except Exception as e:
        log(f"WARNING: Could not load state from {filepath}: {e}")
        return None


def find_most_recent_state_file():
    """Find the most recent state file in the states directory."""
    if not STATE_DIR.exists():
        return None

    state_files = sorted(STATE_DIR.glob("*.txt"), key=lambda p: p.stat().st_mtime, reverse=True)
    if state_files:
        return state_files[0]
    return None


def load_last_state():
    """Load the last known state, checking multiple sources."""
    # First, try to load from last_state.txt
    if LAST_STATE_FILE.exists():
        state = load_state_from_file(LAST_STATE_FILE)
        if state:
            log(f"Loaded last state from {LAST_STATE_FILE.name}")
            return state

    # If last_state.txt doesn't exist or is invalid, try to find most recent state file
    recent_file = find_most_recent_state_file()
    if recent_file:
        log(f"No last_state.txt found, loading most recent state file: {recent_file.name}")
        state = load_state_from_file(recent_file)
        if state:
            # Save it as last_state.txt for next time
            with open(LAST_STATE_FILE, "w") as f:
                f.write(f"State: {interpret_state(state)}\n")
                f.write(f"0x1F0: {state['0x1F0']}\n")
                f.write(f"0x1F5: {state['0x1F5']}\n")
                f.write(f"0x1FA: {state['0x1FA']}\n")
                f.write(f"Raw: {state['raw']}\n")
                f.write(f"File: {state['filename']}\n")
            log(f"Initialized last_state.txt from {recent_file.name}")
            return state

    log("No previous state found (first run)")
    return None


def save_last_state(flags, filename):
    """Save current state as last known state."""
    with open(LAST_STATE_FILE, "w") as f:
        f.write(f"State: {interpret_state(flags)}\n")
        f.write(f"0x1F0: {flags['0x1F0']}\n")
        f.write(f"0x1F5: {flags['0x1F5']}\n")
        f.write(f"0x1FA: {flags['0x1FA']}\n")
        f.write(f"Raw: {flags['raw']}\n")
        f.write(f"File: {filename}\n")


def compare_states(old_flags, new_flags):
    """Compare two flag states and return differences."""
    if not old_flags:
        return "FIRST_CHECK"

    changes = []

    for key in ["0x1F0", "0x1F5", "0x1FA"]:
        if old_flags.get(key) != new_flags.get(key):
            changes.append(f"{key}: {old_flags.get(key)} -> {new_flags.get(key)}")

    if changes:
        return ", ".join(changes)
    else:
        return "NO_CHANGE"


def check_raid_state():
    """Main check function."""
    log("=" * 60)
    log("Starting RAID state check")

    # Load email config
    email_config, email_error = email_notifier.load_email_config(ENV_FILE)
    if email_error:
        log(f"WARNING: Email notifications disabled: {email_error}")
        email_config = None

    # Check if device is connected
    last_disconnect_state = get_disconnect_state()
    device_connected = is_device_connected()

    if not device_connected:
        log(f"ERROR: Device {DEVICE} is not connected or not accessible")

        # Send disconnect notification if this is a new disconnect
        if last_disconnect_state != "disconnected":
            log("DEVICE DISCONNECT DETECTED - Sending notification")
            set_disconnect_state("disconnected")

            if email_config:
                subject = f"RAID Device Disconnected: {DEVICE}"
                body = f"""RAID Device Disconnect Detected

Device: {DEVICE}
Status: NOT CONNECTED

The RAID device is no longer accessible. This could be due to:
- USB cable disconnection
- Enclosure powered off
- WSL USB passthrough issue
- Device failure

The monitor will continue checking and notify you when the device reconnects.

Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
"""
                success, error = email_notifier.send_email(email_config, subject, body)
                if success:
                    log("Disconnect notification email sent")
                else:
                    log(f"ERROR sending disconnect email: {error}")

        log("Check complete (device disconnected)")
        log("=" * 60)
        return False

    # Device is connected - check if this is a reconnect
    if last_disconnect_state == "disconnected":
        log("DEVICE RECONNECT DETECTED - Sending notification")
        set_disconnect_state("connected")

        if email_config:
            subject = f"RAID Device Reconnected: {DEVICE}"
            body = f"""RAID Device Reconnection Detected

Device: {DEVICE}
Status: CONNECTED

The RAID device is accessible again. Normal monitoring will resume.

Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
"""
            success, error = email_notifier.send_email(email_config, subject, body)
            if success:
                log("Reconnect notification email sent")
            else:
                log(f"ERROR sending reconnect email: {error}")

    # Capture current state
    raw_output = capture_raid_state()
    if not raw_output:
        log("ERROR: Failed to capture RAID state")
        return False

    # Extract flags
    current_flags = extract_flags(raw_output)
    if not current_flags:
        log("ERROR: Failed to extract flags from output")
        return False

    current_state = interpret_state(current_flags)
    log(f"Current state: {current_state}")
    log(f"Flags: 0x1F0={current_flags['0x1F0']}, 0x1F5={current_flags['0x1F5']}, 0x1FA={current_flags['0x1FA']}")

    # Load last state
    last_flags = load_last_state()
    last_state = interpret_state(last_flags) if last_flags else "NONE"

    # Compare states
    comparison = compare_states(last_flags, current_flags)
    log(f"Comparison: {comparison}")

    # Save state file
    state_filename = save_state_file(raw_output, current_flags, current_state.lower().replace(" + ", "_"))
    log(f"State saved to: {state_filename}")

    # Log comparison details
    if last_flags:
        log(f"Last state: {last_state} (from {last_flags.get('filename', 'unknown')})")
        log(f"Current state: {current_state} (saved to {state_filename.name})")

    # Check for state change
    if comparison not in ["NO_CHANGE", "FIRST_CHECK"]:
        log(f"STATE CHANGE DETECTED: {comparison}")

        # Send email notification
        if email_config:
            subject = f"RAID State Changed: {last_state} -> {current_state}"
            body = f"""JMicron RAID State Change Detected

Previous State: {last_state}
Current State:  {current_state}

Changes: {comparison}

Previous flags:
  0x1F0: {last_flags.get('0x1F0', 'N/A')}
  0x1F5: {last_flags.get('0x1F5', 'N/A')}
  0x1FA: {last_flags.get('0x1FA', 'N/A')}

Current flags:
  0x1F0: {current_flags['0x1F0']}
  0x1F5: {current_flags['0x1F5']}
  0x1FA: {current_flags['0x1FA']}

Previous capture: {last_flags.get('filename', 'unknown')}
Current capture:  {state_filename.name}

Timestamp: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
"""
            success, error = email_notifier.send_email(email_config, subject, body)
            if success:
                log(f"Email sent: {subject}")
            else:
                log(f"ERROR sending email: {error}")
        else:
            log("WARNING: Email notification skipped (no config)")

    # Save as last state
    save_last_state(current_flags, state_filename.name)

    # Mark device as connected
    if last_disconnect_state != "connected":
        set_disconnect_state("connected")

    log("Check complete")
    log("=" * 60)

    return True


if __name__ == "__main__":
    try:
        check_raid_state()
    except Exception as e:
        log(f"FATAL ERROR: {e}")
        import traceback
        log(traceback.format_exc())
        sys.exit(1)
