#!/bin/bash
# RAID Monitor Daemon - Checks RAID state at configurable intervals

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_SCRIPT="$SCRIPT_DIR/raid_monitor.py"
PID_FILE="$SCRIPT_DIR/monitor.pid"
INTERVAL=${1:-60}  # Default to 1 minute (60 seconds), or use first argument

# Check if already running
if [ -f "$PID_FILE" ]; then
    PID=$(cat "$PID_FILE")
    if ps -p "$PID" > /dev/null 2>&1; then
        echo "Monitor already running with PID $PID"
        exit 1
    else
        echo "Removing stale PID file"
        rm "$PID_FILE"
    fi
fi

# Save our PID
echo $$ > "$PID_FILE"

echo "RAID Monitor started (PID $$)"
echo "Checking every $INTERVAL seconds ($((INTERVAL / 60)) minute(s))"
echo "Log file: $SCRIPT_DIR/monitor.log"
echo "Press Ctrl+C to stop"
echo ""

# Cleanup on exit
cleanup() {
    echo ""
    echo "Monitor stopped"
    rm -f "$PID_FILE"
    exit 0
}
trap cleanup INT TERM

# Run immediately on start
python3 "$PYTHON_SCRIPT"

# Then run every 5 minutes
while true; do
    sleep "$INTERVAL"
    python3 "$PYTHON_SCRIPT"
done
