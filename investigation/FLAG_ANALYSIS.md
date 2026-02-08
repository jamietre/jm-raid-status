# JMicron RAID Flag Analysis

## Current Understanding (Verified)

Analysis of IDENTIFY DEVICE response flags (bytes 0x1F0-0x1FB) for JMicron JMS561/JMS567 RAID controllers.

**Test Configuration**: 4-disk RAID5 array, tested with disk removal and rebuild scenarios.

## Reliable Flags (CONFIRMED)

### Byte 0 (0x1F0): Disk Presence Bitmask ✓ VERIFIED
- **Bits 0-3**: Represent disks 0-3 (bit set = disk present, bit clear = disk missing)
- `0x0f` (1111b) = All 4 disks present → **OPERATIONAL**
- `0x07` (0111b) = Disks 0,1,2 present, disk 3 missing → **DEGRADED**
- `0x03` (0011b) = Disks 0,1 present → **DEGRADED** (2 disks missing)

**Usage for monitoring**: Count set bits in 0x1F0 and compare to expected disk count. If fewer disks than expected, array is DEGRADED.

### Byte 5 (0x1F5): Rebuild Status ✓ VERIFIED
- `0x00` = Normal operation (healthy or degraded, not actively rebuilding)
- `0x01` = **ACTIVELY REBUILDING**

**State transitions verified**:
- Healthy → Degraded: Remains `0x00` (not rebuilding)
- Degraded → Rebuilding: Changes to `0x01` (rebuild started)
- Rebuilding → Healthy: Returns to `0x00` (rebuild completed)

**Usage for monitoring**: Check if `0x1F5 == 0x01` to detect active rebuild operations.

## Unknown/Unreliable Flags

### Byte 9 (0x1F9): Purpose Unknown ❓
- Observed as `0x80` in healthy, degraded, and rebuilding states
- Transiently observed as `0x00` in one capture (may be dynamic or time-dependent)
- **Not reliable for state detection**
- Purpose unknown - may be internal controller state

### Other Bytes (1-4, 6-8, 10-11)
- Remained constant across all tested states
- Purpose unknown
- Not useful for health/rebuild monitoring

## Test Data

Located in `investigation/data/key_snapshots/`:

1. **healthy_state.txt** - Original healthy state (RAID5, all 4 disks, before testing)
2. **degraded_state.txt** - One disk removed (RAID5 degraded)
3. **rebuilding_state_1.txt** - During active rebuild (disk re-inserted)
4. **healthy_state_after_rebuild.txt** - After rebuild completed

## Flag Bytes Comparison

```
Offset:  0    1    2    3    4    5    6    7    8    9   10   11
State
-------  ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ----
Healthy  0f   00   2f   00   01   00   00   00   00   80   00   00
Degraded 07   00   2f   00   01   00   00   00   00   80   00   00
Rebuild  0f   00   2f   00   01   01   00   00   00   80   00   00
         ^^                     ^^
         |                      |
      Disk count            Rebuilding
```

**Key differences**:
- **0x1F0**: `0x0f` (4 disks) vs `0x07` (3 disks) - degraded detection
- **0x1F5**: `0x00` (normal) vs `0x01` (rebuilding) - rebuild detection

## State Transitions (Verified)

```
HEALTHY (0x1F0=0x0f, 0x1F5=0x00)
    ↓ (disk removed)
DEGRADED (0x1F0=0x07, 0x1F5=0x00)  ← Disk bitmask changes
    ↓ (disk re-inserted, rebuild starts)
REBUILDING (0x1F0=0x0f, 0x1F5=0x01)  ← Rebuild flag set
    ↓ (rebuild complete)
HEALTHY (0x1F0=0x0f, 0x1F5=0x00)  ← Rebuild flag clears
```

## Monitoring Implementation

### Recommended Approach

```python
def check_raid_status(flags):
    """
    flags: dict with '0x1F0' and '0x1F5' as hex strings
    expected_disks: number of disks in array (e.g., 4)
    """
    disk_bitmask = int(flags['0x1F0'], 16)
    rebuild_flag = int(flags['0x1F5'], 16)

    # Count present disks
    present_disks = bin(disk_bitmask).count('1')

    # Determine state
    is_degraded = (present_disks < expected_disks)
    is_rebuilding = (rebuild_flag == 0x01)

    if is_rebuilding:
        return "REBUILDING"
    elif is_degraded:
        return "DEGRADED"
    else:
        return "HEALTHY"
```

### Alert Conditions

1. **DEGRADED**: When `popcount(0x1F0) < expected_disk_count`
   - Action: Alert immediately - array has reduced/no redundancy

2. **REBUILDING**: When `0x1F5 == 0x01`
   - Action: Monitor progress, avoid additional stress on array

3. **REBUILD COMPLETE**: When `0x1F5` transitions from `0x01` → `0x00`
   - Action: Send completion notification

## Known Limitations

1. **No degraded flag**: Must configure expected disk count and compare to actual
2. **No rebuild progress**: Flag only indicates active/inactive, not percentage complete
3. **No RAID level detection**: Cannot determine if array is RAID0/1/5 from these flags
4. **0x1F9 purpose unknown**: Observed to vary, not useful for monitoring

## Test Methodology

All captures taken from JMicron JMS561/JMS567 controller via:
```bash
sudo env JMRAIDSTATUS_DUMP_RAW=1 bin/jmraidstatus /dev/sde
```

Flags extracted from IDENTIFY DEVICE (0x22 0x03) response at offset 0x1F0.

**Test sequence**:
1. Captured baseline healthy state (4 disks, RAID5)
2. Removed disk 3, captured degraded state
3. Re-inserted disk 3, captured rebuilding state
4. Waited for rebuild completion, verified flag changes
