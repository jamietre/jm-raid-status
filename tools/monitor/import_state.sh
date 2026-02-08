#!/bin/bash
# Import an existing state file as the baseline for monitoring

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
STATE_DIR="$SCRIPT_DIR/states"

if [ $# -eq 0 ]; then
    echo "Usage: $0 <state_file>"
    echo ""
    echo "Import an existing RAID state capture as the baseline for monitoring."
    echo "This will be used as the 'previous state' for comparison."
    echo ""
    echo "Example:"
    echo "  $0 ../data/post_rebuild_blue_led_2.txt"
    echo ""
    echo "The file will be copied to states/ and used as the initial state."
    exit 1
fi

SOURCE_FILE="$1"

if [ ! -f "$SOURCE_FILE" ]; then
    echo "Error: File not found: $SOURCE_FILE"
    exit 1
fi

# Create states directory if it doesn't exist
mkdir -p "$STATE_DIR"

# Get timestamp and state name from filename or use generic
BASENAME=$(basename "$SOURCE_FILE" .txt)
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Copy to states directory with timestamp
DEST_FILE="$STATE_DIR/imported_${BASENAME}_${TIMESTAMP}.txt"
cp "$SOURCE_FILE" "$DEST_FILE"

echo "Imported state file:"
echo "  Source: $SOURCE_FILE"
echo "  Destination: $DEST_FILE"
echo ""

# Try to extract and display the state
if grep -q "^01f0:" "$SOURCE_FILE"; then
    echo "Detected flags in raw protocol dump:"
    FLAGS=$(grep "^01f0:" "$SOURCE_FILE" | head -1 | awk '{print $2, $3, $4, $5, $6, $7}')
    echo "  Raw: $FLAGS"

    F0=$(echo $FLAGS | awk '{print $1}')
    F5=$(echo $FLAGS | awk '{print $6}')

    echo ""
    echo "State interpretation:"
    if [ "$F0" = "07" ]; then
        echo "  Health: DEGRADED (0x1F0 = 0x07)"
    elif [ "$F0" = "0f" ]; then
        echo "  Health: OPERATIONAL (0x1F0 = 0x0F)"
    else
        echo "  Health: UNKNOWN (0x1F0 = $F0)"
    fi

    if [ "$F5" = "00" ]; then
        echo "  Rebuild: IDLE (0x1F5 = 0x00)"
    elif [ "$F5" = "01" ]; then
        echo "  Rebuild: ACTIVE (0x1F5 = 0x01)"
    else
        echo "  Rebuild: UNKNOWN (0x1F5 = $F5)"
    fi
fi

echo ""
echo "This state will be used as the baseline on next monitor run."
echo "Run './monitor.sh check' to compare current state against this baseline."
