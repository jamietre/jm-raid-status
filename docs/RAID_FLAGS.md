# RAID Status Flags Reference

This document describes the JMicron controller RAID status flags used by `jmraidstatus` for monitoring array health.

## Quick Reference

| Flag | Offset | Purpose | Values |
|------|--------|---------|--------|
| **0x1F0** | Byte 0 | Disk presence bitmask | Bit N = Disk N present |
| **0x1F5** | Byte 5 | Rebuild status | `0x00` = normal, `0x01` = rebuilding |

## Disk Presence (0x1F0)

**Purpose**: Indicates which disks are present in the array.

**Format**: Bitmask where each bit represents one disk
- Bit 0 (0x01) = Disk 0
- Bit 1 (0x02) = Disk 1
- Bit 2 (0x04) = Disk 2
- Bit 3 (0x08) = Disk 3
- Bit 4 (0x10) = Disk 4 (if supported)

**Examples**:
```
0x0f (00001111b) = All 4 disks present (healthy)
0x07 (00000111b) = 3 disks present, 1 missing (degraded)
0x03 (00000011b) = 2 disks present, 2 missing (degraded)
```

**Detection**:
```python
disk_count = bin(flags_0x1F0).count('1')
if disk_count < expected_disks:
    status = "DEGRADED"
```

## Rebuild Status (0x1F5)

**Purpose**: Indicates if array is actively rebuilding.

**Values**:
- `0x00` = Normal operation (healthy or degraded, not rebuilding)
- `0x01` = **Actively rebuilding** (parity/data reconstruction in progress)

**State transitions**:
1. Healthy → Degraded: Remains `0x00`
2. Degraded → Rebuilding: Changes to `0x01` (rebuild started)
3. Rebuilding → Healthy: Returns to `0x00` (rebuild completed)

**Detection**:
```python
if flags_0x1F5 == 0x01:
    status = "REBUILDING"
```

## Monitoring Example

```python
def get_raid_status(device="/dev/sde", expected_disks=4):
    # Read flags from controller
    flags = read_jmicron_flags(device)

    disk_bitmask = int(flags['0x1F0'], 16)
    rebuild_flag = int(flags['0x1F5'], 16)

    # Count present disks
    present = bin(disk_bitmask).count('1')

    # Determine status
    if rebuild_flag == 0x01:
        return f"REBUILDING ({present}/{expected_disks} disks)"
    elif present < expected_disks:
        return f"DEGRADED ({present}/{expected_disks} disks)"
    else:
        return f"HEALTHY ({present} disks)"
```

## Configuration Requirements

To detect degraded arrays, you must configure the **expected disk count** for your array:

```json
{
  "expected_disks": 4
}
```

Without this, the tool cannot distinguish between:
- A healthy 3-disk array (0x1F0 = 0x07)
- A degraded 4-disk array (0x1F0 = 0x07)

## Limitations

1. **No RAID level detection**: Cannot determine if array is RAID0/1/5 from these flags
2. **No rebuild progress**: Only indicates active/inactive, not percentage complete
3. **No disk identification**: Bitmask shows which slot is empty, not which disk failed
4. **Controller-specific**: These flags are specific to JMicron JMS561/JMS567 controllers

## Technical Details

Flags are extracted from the IDENTIFY DEVICE (0x22 0x03) command response at offset 0x1F0.

For detailed investigation and validation methodology, see:
- `investigation/FLAG_ANALYSIS.md` - Complete flag analysis and testing
- `investigation/data/key_snapshots/` - Original test captures

## References

- JMicron JMS561/JMS567 controller (USB to SATA RAID bridge)
- Tested with 4-disk RAID5 configuration
- Validation: Disk removal, rebuild, and completion scenarios
