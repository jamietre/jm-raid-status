#!/bin/bash
# Test signal handling - verify sector is restored on interruption

DEVICE="/dev/sde"
SECTOR=1024
TOOL="../bin/jmraidstatus"

echo "=== Signal Handling Test ==="
echo ""
echo "This test verifies that the sector is properly restored"
echo "when the tool is interrupted with Ctrl+C"
echo ""

# Check sector before test
echo "1. Checking sector $SECTOR before test..."
BEFORE=$(sudo dd if="$DEVICE" skip=$SECTOR count=1 bs=512 2>/dev/null | xxd | head -1)
echo "   Before: $BEFORE"

if [ "$BEFORE" != "00000000: 0000 0000 0000 0000 0000 0000 0000 0000  ................" ]; then
    echo "   WARNING: Sector is not empty before test!"
fi

echo ""
echo "2. Starting tool in background..."
sudo $TOOL "$DEVICE" &
TOOL_PID=$!
echo "   Tool PID: $TOOL_PID"

echo ""
echo "3. Waiting 0.5 seconds (tool should be mid-operation)..."
sleep 0.5

echo ""
echo "4. Sending SIGINT (Ctrl+C) to tool..."
kill -INT $TOOL_PID

echo ""
echo "5. Waiting for cleanup..."
sleep 1

echo ""
echo "6. Checking sector $SECTOR after interruption..."
AFTER=$(sudo dd if="$DEVICE" skip=$SECTOR count=1 bs=512 2>/dev/null | xxd | head -1)
echo "   After: $AFTER"

if [ "$AFTER" = "00000000: 0000 0000 0000 0000 0000 0000 0000 0000  ................" ]; then
    echo ""
    echo "✅ SUCCESS: Sector was restored to zeros!"
    exit 0
else
    echo ""
    echo "❌ FAIL: Sector contains non-zero data after interruption!"
    echo ""
    echo "Full sector dump:"
    sudo dd if="$DEVICE" skip=$SECTOR count=1 bs=512 2>/dev/null | xxd
    exit 1
fi
