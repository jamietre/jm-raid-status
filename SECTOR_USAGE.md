# Technical Details: Sector Usage and Communication Protocol

## Overview

The JMicron RAID protocol uses a **direct sector read/write mechanism** to communicate with the controller. This document explains how this works, why it's necessary, and what the risks are.

## How Communication Works

Unlike standard SMART queries (which use ATA SMART commands), the JMicron protocol communicates by **using a specific sector on the disk as a "mailbox"**:

### The Communication Sequence

**This tool uses sector 1024 (configurable via --sector flag).**

1. **Safety Check**
   - Tool reads the communication sector (default: 1024)
   - Verifies it contains all zeros (unused)
   - **Refuses to run if sector contains any data**
   - This prevents accidental corruption

2. **Backup Phase** (`jm_init_device`)
   - Tool reads the communication sector
   - Saves the 512-byte content to memory buffer
   - Preserves original data (should be all zeros)

3. **Command Phase** (`jm_execute_command`)
   - Tool writes a command to the communication sector
   - Command includes: magic bytes, disk number, command type, CRC
   - Controller intercepts the write and processes it
   - Controller writes response back to the same sector
   - Tool reads the sector to get the response
   - Process repeats for each disk query (probe11-probe15)

4. **Cleanup Phase** (`jm_cleanup_device`)
   - Tool writes the backed-up data back to the sector
   - **Sector is restored to original state** (all zeros)
   - Device is closed

### Why Sector 1024?

**This tool uses sector 1024 (0x400) as the default communication sector.**

Sector 1024 was chosen because:

- **Safe location**: Well past partition table area (sectors 0-63)
- **Before partitions**: On modern systems, first partition starts at sector 2048 (1MB boundary)
- **Clean alignment**: 512KB boundary
- **Typically unused**: The gap between sector 63 (end of legacy MBR area) and sector 2048 (modern partition start) is normally unused
- **Verified empty**: Tool **requires** the sector to be all zeros before use

**Note about sector 0x21 (decimal 33):**

- This was used by the original JMRaidCon tool
- **This tool does NOT use sector 0x21**
- We chose 1024 for better safety margins
- Sector 33 is too close to partition tables (sectors 1-63)

**You can use a different sector** with `--sector XXXX` if needed, but it must:

1. Be all zeros (verified automatically)
2. Be between sectors 64-2047 (in the partition gap)
3. Not conflict with any partition or metadata

### Timing

The entire process takes **1-3 seconds** typically:

- Safety check: ~1ms
- Backup: ~1ms
- Disk queries (5 slots × 2 commands): ~1-2 seconds
- Cleanup: ~1ms

During this time, **the communication sector contains JMicron protocol data**, not the original content (which should be all zeros).

## Risks and Failure Modes

### Risk 1: Tool Interruption Before Cleanup

**Scenario**: Tool crashes, killed (SIGKILL), power loss, etc. before `jm_cleanup_device` runs

**Result**:

- Communication sector remains filled with the last command/response
- Original 512 bytes of data are lost
- If sector was unused (all zeros): No practical impact - sector becomes non-zero
- If sector contained data: That specific sector is corrupted

**Likelihood**: Low (tool runs for 1-3 seconds)

**Mitigation**:

- Tool verifies sector is empty before use (reduces impact)
- Signal handlers restore data when possible (SIGKILL cannot be caught)

### Risk 2: Communication Sector Contains Active Data

**Scenario**: Partition starts before expected (e.g., before sector 1024)

**Result**:

- **Tool will refuse to run** - safety check detects non-zero data
- No corruption occurs - tool exits before any writes

**Likelihood**: Extremely low - tool has mandatory safety check

**Detection**: Automatic - tool checks sector content before use

### Risk 3: Controller Malfunction

**Scenario**: Controller interprets commands incorrectly or has firmware bug

**Result**: Unknown - this is likely what happened in the HD Sentinel case

**Quote from HD Sentinel developer**:

> "This tested on several systems/enclosures and on most of them, everything worked perfectly. But in one case, we found that it resulted the RAID array to break and the complete information stored on the drives lost."

**Likelihood**: Extremely rare (1 known case across thousands of uses)

**Root cause**: Unknown - may be specific controller firmware, timing issue, or protocol interpretation

## Safety Check (v1.0.1+)

**The tool now automatically verifies the sector is empty before use.**

If the sector contains any non-zero data, the tool will:

1. **Refuse to run** - prevents accidental data corruption
2. **Show error message** - explains what was found and why it's unsafe
3. **Suggest alternatives** - directs you to check_sectors tool

```
Error: Sector 1024 contains data (not all zeros)
  The tool requires an empty sector to use as a communication channel.
  This sector may contain partition data, RAID metadata, or other critical information.

  Safety check failed to prevent potential data corruption.

  Solutions:
  1. Check your partition layout: sudo fdisk -l /dev/sdX
  2. Use a different sector: --sector XXXX (must be unused)
  3. Use tests/check_sectors to find an empty sector
```

This check **cannot be bypassed** - it's a hard safety requirement.

## Verifying Sectors Manually

You can verify sector safety using the included `tests/check_sectors` tool:

### Using the check_sectors Tool

```bash
# Run the included sector verification tool
cd tests
sudo ./check_sectors /dev/sdX
```

This will:

- Show your partition layout
- Check multiple candidate sectors (33, 64, 128, 256, 512, 1024, 1536, 2000, 2047)
- Display which sectors are empty (safe)
- Show hex dump of key sectors
- Provide recommendations

**Example output:**

```
Checking candidate sectors:
  Sector 33 (Original default): ✗ Contains data (24 non-zero bytes)
  Sector 1024 (New default): ✓ EMPTY (safe to use)
  Sector 1536: ✓ EMPTY (safe to use)
```

### Manual Check: Partition Layout

```bash
# Check where partitions start
sudo fdisk -l /dev/sdX

# Look for "Start" column - should be 2048 or higher for first partition
```

**Example Safe Layout:**

```
Device     Start       End   Sectors   Size Type
/dev/sdc1   2048  19531775  19529728   9.3G Linux filesystem
```

→ Sector 1024 is before 2048, **safe to use**

**Example UNSAFE Layout:**

```
Device     Start       End   Sectors   Size Type
/dev/sdc1      1  19531775  19531775   9.3G Linux filesystem
```

→ Partition starts at sector 1, **NO safe sectors available**, **DO NOT USE THIS TOOL**

### Manual Check: Sector Content

```bash
# Read sector 1024 and check if it's all zeros (unused)
sudo dd if=/dev/sdX skip=1024 count=1 bs=512 2>/dev/null | xxd | head -20

# If output is all zeros (00 00 00 ...), sector is unused
```

### Using Alternative Sector

If sector 1024 contains data, you can specify a different sector:

```bash
# Use sector 1536 instead (still before typical partition start at 2048)
sudo jmraidstatus --sector 1536 /dev/sdX
```

**Important**: The alternative sector must:

1. Be **all zeros** (verified by tool before use)
2. Be before the first partition
3. Not be in the MBR/partition table area (avoid 0-63)
4. Be between sectors 64-2047 for typical modern layouts

## Why This Approach?

This unusual communication method exists because:

1. **JMicron controllers sit between the host and disks** - normal ATA pass-through doesn't work
2. **No official API** - JMicron doesn't provide documentation or drivers for SMART access
3. **Reverse engineering** - This protocol was discovered by analyzing traffic, not from specs
4. **Hardware RAID limitation** - The controller intercepts normal SMART commands

Standard tools like `smartctl` cannot access disks behind hardware RAID because:

- They send ATA SMART commands to the device
- The RAID controller intercepts these and returns controller status, not individual disk status
- The JMicron protocol bypasses this by using the sector mailbox

## Comparison to HD Sentinel Approach

HD Sentinel also uses this protocol but:

- **Disabled by default** on Linux due to the data loss incident
- **External module** (`detjm`) that must be explicitly downloaded
- **Used successfully** on most systems but failed catastrophically in one case
- **Different implementation** but same underlying protocol

## Recommendations

1. ✅ **Always have backups** before using this tool
2. ✅ **Verify partition layout** - ensure first partition starts at 2048+
3. ✅ **Test on non-critical system first** if possible
4. ✅ **Don't interrupt the tool** while running (no Ctrl+C, let it complete)
5. ✅ **Check RAID status** after first use
6. ⚠️ **Avoid using on production systems** during critical operations
7. ⚠️ **Don't use if partitions start before sector 64**

## Technical References

- Original JMRaidCon: https://github.com/wjoe/jmraidcon
- HD Sentinel Discussion: https://www.hdsentinel.com/forum/viewtopic.php?p=18046#p18046
- Source Code: `src/jm_protocol.c` (this repository)

## Summary

The JMicron protocol **temporarily uses a communication sector** (default: 1024) as a mailbox with the controller. The controller intercepts sector writes that match the JMicron command format (magic bytes + CRC) regardless of which sector is used.

**Key Points:**

- ✅ No special "wakeup" needed - controller watches all sector writes
- ✅ Sector 1024 chosen for safety (in the partition gap)
- ✅ Tool verifies sector is empty before use (mandatory safety check)
- ✅ Sector content is backed up and restored
- ⚠️ Brief window (1-3 seconds) where sector contains protocol data
- ⚠️ Tool interruption leaves sector non-zero (minor impact if sector was unused)
- ⚠️ Controller malfunction risk exists (extremely rare but documented)

**Always have complete backups before use.**
