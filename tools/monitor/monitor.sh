#!/bin/bash
# RAID Monitor Control Script

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DAEMON_SCRIPT="$SCRIPT_DIR/monitor_daemon.sh"
PID_FILE="$SCRIPT_DIR/monitor.pid"
LOG_FILE="$SCRIPT_DIR/monitor.log"
INTERVAL_FILE="$SCRIPT_DIR/monitor.interval"
DEFAULT_INTERVAL=60  # 1 minute default

start() {
    # Get interval from argument or use default
    local interval=${1:-$DEFAULT_INTERVAL}

    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        if ps -p "$PID" > /dev/null 2>&1; then
            echo "Monitor already running with PID $PID"
            return 1
        else
            rm "$PID_FILE"
        fi
    fi

    echo "Starting RAID monitor (interval: ${interval}s / $((interval / 60))m)..."
    nohup "$DAEMON_SCRIPT" "$interval" >> "$LOG_FILE" 2>&1 &
    sleep 1

    if [ -f "$PID_FILE" ]; then
        # Save interval for status display
        echo "$interval" > "$INTERVAL_FILE"
        echo "Monitor started successfully"
        echo "PID: $(cat $PID_FILE)"
        echo "Interval: ${interval} seconds ($((interval / 60)) minute(s))"
        echo "Log: $LOG_FILE"
    else
        echo "Failed to start monitor"
        return 1
    fi
}

stop() {
    if [ ! -f "$PID_FILE" ]; then
        echo "Monitor is not running"
        return 1
    fi

    PID=$(cat "$PID_FILE")
    if ! ps -p "$PID" > /dev/null 2>&1; then
        echo "Monitor is not running (stale PID file)"
        rm "$PID_FILE"
        return 1
    fi

    echo "Stopping monitor (PID $PID)..."
    kill "$PID"
    sleep 1

    if ps -p "$PID" > /dev/null 2>&1; then
        echo "Process still running, forcing kill..."
        kill -9 "$PID"
    fi

    rm -f "$PID_FILE"
    echo "Monitor stopped"
}

status() {
    if [ ! -f "$PID_FILE" ]; then
        echo "Status: NOT RUNNING"
        return 1
    fi

    PID=$(cat "$PID_FILE")
    if ps -p "$PID" > /dev/null 2>&1; then
        echo "Status: RUNNING"
        echo "PID: $PID"

        # Show interval if saved
        if [ -f "$INTERVAL_FILE" ]; then
            interval=$(cat "$INTERVAL_FILE")
            echo "Interval: ${interval}s ($((interval / 60))m)"
        fi

        echo "Log: $LOG_FILE"

        # Show last few log entries
        if [ -f "$LOG_FILE" ]; then
            echo ""
            echo "Last 5 log entries:"
            tail -5 "$LOG_FILE"
        fi
    else
        echo "Status: NOT RUNNING (stale PID file)"
        rm "$PID_FILE"
        return 1
    fi
}

check_now() {
    echo "Running immediate RAID check..."
    python3 "$SCRIPT_DIR/raid_monitor.py"
}

tail_log() {
    if [ -f "$LOG_FILE" ]; then
        tail -f "$LOG_FILE"
    else
        echo "No log file found: $LOG_FILE"
        return 1
    fi
}

case "$1" in
    start)
        start "$2"
        ;;
    stop)
        stop
        ;;
    restart)
        stop
        sleep 1
        start "$2"
        ;;
    status)
        status
        ;;
    check)
        check_now
        ;;
    log)
        tail_log
        ;;
    *)
        echo "Usage: $0 {start|stop|restart|status|check|log} [interval]"
        echo ""
        echo "Commands:"
        echo "  start [interval]   - Start the monitor daemon (default: 60s / 1 minute)"
        echo "  stop               - Stop the monitor daemon"
        echo "  restart [interval] - Restart the monitor daemon"
        echo "  status             - Show monitor status and recent logs"
        echo "  check              - Run immediate check (doesn't affect daemon)"
        echo "  log                - Tail the log file (Ctrl+C to exit)"
        echo ""
        echo "Examples:"
        echo "  $0 start           # Start with 1 minute interval (default)"
        echo "  $0 start 30        # Start with 30 second interval"
        echo "  $0 start 300       # Start with 5 minute interval"
        exit 1
        ;;
esac
