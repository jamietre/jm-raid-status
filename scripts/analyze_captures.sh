#!/bin/bash
# Analyze multiple capture files to find patterns and changes

echo "=== RAID Status Flag Analysis ==="
echo ""

for file in tests/data/*.txt; do
    echo "File: $(basename $file)"
    echo "Timestamp: $(stat -c %y "$file" | cut -d. -f1)"

    # Extract 0x1F0 line from each disk
    echo "Offset 0x1F0-0x1FF (last 16 bytes before CRC):"
    for disk in 0 1 2 3 4; do
        val=$(grep -A 32 "=== IDENTIFY DISK $disk" "$file" | grep "^01f0:" 2>/dev/null)
        if [ -n "$val" ]; then
            echo "  Disk $disk: $val"
        fi
    done
    echo ""
done

echo "=== Flag Summary ==="
echo ""
echo "Key offsets in IDENTIFY response:"
echo "  0x1F0: RAID health status"
echo "         0x07 = Degraded (missing disk)"
echo "         0x0F = Operational (all disks present)"
echo ""
echo "  0x1F5: Rebuild status (hypothesis)"
echo "         0x00 = Not rebuilding?"
echo "         0x01 = Rebuild in progress?"
echo ""

# Compare degraded vs healthy vs rebuilding states
if [ -f tests/data/degraded_state.txt ]; then
    echo "=== Degraded State (disk missing) ==="
    echo "  Flag at 0x1F0:"
    grep -A 32 "=== IDENTIFY DISK 0" tests/data/degraded_state.txt | grep "^01f0:" | awk '{print "    " $2}'
fi

if [ -f tests/data/healthy_state.txt ]; then
    echo ""
    echo "=== Healthy State (just after reinsertion) ==="
    echo "  Flag at 0x1F0:"
    grep -A 32 "=== IDENTIFY DISK 0" tests/data/healthy_state.txt | grep "^01f0:" | awk '{print "    " $2}'
fi

if [ -f tests/data/rebuilding_state_1.txt ]; then
    echo ""
    echo "=== Rebuilding State (mid-rebuild) ==="
    echo "  Flag at 0x1F0:"
    grep -A 32 "=== IDENTIFY DISK 0" tests/data/rebuilding_state_1.txt | grep "^01f0:" | awk '{print "    " $2}'
fi
