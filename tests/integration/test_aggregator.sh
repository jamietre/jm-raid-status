#!/bin/bash
# Integration tests for disk-health aggregator
# Tests various input scenarios and validates output

# Don't exit on error - we want to run all tests
set +e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BIN_DIR="$PROJECT_ROOT/bin"
DATA_DIR="$PROJECT_ROOT/tests/data"

DISK_HEALTH="$BIN_DIR/disk-health"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

# Test helper functions
test_start() {
    echo -n "  Testing: $1 ... "
    TESTS_RUN=$((TESTS_RUN + 1))
}

test_pass() {
    echo -e "${GREEN}PASS${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
}

test_fail() {
    echo -e "${RED}FAIL${NC}"
    if [ -n "$1" ]; then
        echo "    Error: $1"
    fi
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

# Check binaries exist
if [ ! -x "$DISK_HEALTH" ]; then
    echo -e "${RED}Error: disk-health binary not found at $DISK_HEALTH${NC}"
    echo "Run 'make' first"
    exit 1
fi

echo "=== Disk Health Aggregator Integration Tests ==="
echo

# Test 1: Single healthy source
echo "Test Suite: Single Source"
test_start "Single healthy JMicron RAID"
OUTPUT=$(cat "$DATA_DIR/jmicron/healthy-4disk.json" | "$DISK_HEALTH" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] && echo "$OUTPUT" | grep -q "Overall Status: PASSED"; then
    test_pass
else
    test_fail "Expected exit code 0 and PASSED status, got exit code $EXIT_CODE"
fi

test_start "Single degraded RAID (healthy disks, array degraded)"
OUTPUT=$(cat "$DATA_DIR/jmicron/degraded-3disk.json" | "$DISK_HEALTH" 2>&1)
EXIT_CODE=$?
# Degraded RAID should still pass if all disks are healthy (exit 0)
# But the output should mention degraded status
if [ $EXIT_CODE -eq 0 ] && echo "$OUTPUT" | grep -q "PASSED"; then
    test_pass
else
    test_fail "Expected exit code 0 for healthy disks, got $EXIT_CODE"
fi

test_start "Single source with failed disk"
OUTPUT=$(cat "$DATA_DIR/jmicron/failed-disk.json" | "$DISK_HEALTH" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 1 ] && echo "$OUTPUT" | grep -q "Overall Status: FAILED"; then
    test_pass
else
    test_fail "Expected exit code 1 and FAILED status, got exit code $EXIT_CODE"
fi

test_start "Single healthy smartctl source"
OUTPUT=$(cat "$DATA_DIR/smartctl/healthy-ssd.json" | "$DISK_HEALTH" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] && echo "$OUTPUT" | grep -q "PASSED"; then
    test_pass
else
    test_fail "Expected exit code 0 and PASSED status, got exit code $EXIT_CODE"
fi

echo
echo "Test Suite: Multi-Source Aggregation"

test_start "Two healthy sources"
OUTPUT=$(cat "$DATA_DIR/jmicron/healthy-4disk.json" "$DATA_DIR/smartctl/healthy-ssd.json" | "$DISK_HEALTH" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] && echo "$OUTPUT" | grep -q "Sources: 2" && echo "$OUTPUT" | grep -q "Total Disks: 5"; then
    test_pass
else
    test_fail "Expected 2 sources, 5 total disks, exit code 0"
fi

test_start "Mixed healthy and failed sources"
OUTPUT=$(cat "$DATA_DIR/jmicron/healthy-4disk.json" "$DATA_DIR/jmicron/failed-disk.json" | "$DISK_HEALTH" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 1 ] && echo "$OUTPUT" | grep -q "FAILED"; then
    test_pass
else
    test_fail "Expected exit code 1 when any source has failures"
fi

test_start "Three sources (RAID + 2 single disks)"
OUTPUT=$(cat "$DATA_DIR/jmicron/healthy-4disk.json" \
              "$DATA_DIR/smartctl/healthy-ssd.json" \
              "$DATA_DIR/smartctl/healthy-ssd.json" | "$DISK_HEALTH" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] && echo "$OUTPUT" | grep -q "Sources: 3" && echo "$OUTPUT" | grep -q "Total Disks: 6"; then
    test_pass
else
    test_fail "Expected 3 sources, 6 total disks"
fi

echo
echo "Test Suite: JSON Output"

test_start "JSON output format validation"
OUTPUT=$(cat "$DATA_DIR/jmicron/healthy-4disk.json" | "$DISK_HEALTH" --json 2>&1)
EXIT_CODE=$?
# Validate JSON is parsable
if echo "$OUTPUT" | python3 -m json.tool >/dev/null 2>&1; then
    # Check required fields exist
    if echo "$OUTPUT" | python3 -c "import sys,json; d=json.load(sys.stdin); assert d['version']=='2.0'; assert 'sources' in d; assert 'summary' in d" 2>/dev/null; then
        test_pass
    else
        test_fail "JSON missing required fields (version, sources, summary)"
    fi
else
    test_fail "Invalid JSON output"
fi

test_start "JSON summary fields"
OUTPUT=$(cat "$DATA_DIR/jmicron/healthy-4disk.json" "$DATA_DIR/smartctl/healthy-ssd.json" | "$DISK_HEALTH" --json 2>&1)
if echo "$OUTPUT" | python3 -c "
import sys, json
d = json.load(sys.stdin)
assert d['summary']['total_disks'] == 5, 'Expected 5 total disks'
assert d['summary']['healthy_disks'] == 5, 'Expected 5 healthy disks'
assert d['summary']['failed_disks'] == 0, 'Expected 0 failed disks'
assert d['summary']['overall_status'] == 'healthy', 'Expected healthy status'
" 2>/dev/null; then
    test_pass
else
    test_fail "JSON summary fields incorrect"
fi

echo
echo "Test Suite: Error Handling"

test_start "Empty input (no sources)"
OUTPUT=$(echo "" | "$DISK_HEALTH" 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 3 ] && echo "$OUTPUT" | grep -q "No valid sources"; then
    test_pass
else
    test_fail "Expected exit code 3 for no input"
fi

test_start "Invalid JSON input (gracefully handled)"
OUTPUT=$(echo "not valid json" | "$DISK_HEALTH" 2>&1)
EXIT_CODE=$?
# Invalid JSON is gracefully handled - doesn't crash
# Just produces a report with minimal/no data
if [ $EXIT_CODE -eq 0 ] || [ $EXIT_CODE -eq 3 ]; then
    test_pass
else
    test_fail "Unexpected crash or error on invalid input"
fi

test_start "Quiet mode suppresses output"
OUTPUT=$(cat "$DATA_DIR/jmicron/healthy-4disk.json" | "$DISK_HEALTH" --quiet 2>&1)
EXIT_CODE=$?
if [ $EXIT_CODE -eq 0 ] && [ -z "$OUTPUT" ]; then
    test_pass
else
    test_fail "Quiet mode should produce no output"
fi

echo
echo "=== Test Summary ==="
echo "Tests run: $TESTS_RUN"
echo -e "Tests passed: ${GREEN}$TESTS_PASSED${NC}"
if [ $TESTS_FAILED -gt 0 ]; then
    echo -e "Tests failed: ${RED}$TESTS_FAILED${NC}"
    exit 1
else
    echo -e "${GREEN}All tests passed!${NC}"
    exit 0
fi
