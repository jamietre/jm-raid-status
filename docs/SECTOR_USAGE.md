# Technical Details: Sector Usage and Communication Protocol

## Overview

The JMicron RAID protocol uses a **direct sector read/write mechanism** to communicate with the controller. This document explains how this works, why it's necessary, the risks involved, and how to handle failure modes.

## How Communication Works

Unlike standard SMART queries (which use ATA SMART commands), the JMicron protocol communicates by **using a specific sector on the disk as a "mailbox"**:

### The Communication Sequence

**This tool uses sector `33` (0x21) by default, configurable via `--sector`.**

1. **Safety Check** (`jm_init_device`)
   - Tool reads the communication sector via SG_IO (SCSI pass-through)
   - Verifies it contains all zeros (unused)
   - **Refuses to run if sector contains unrecognized data**
   - See [Stale Mailbox State](#stale-mailbox-state) below for the JMicron artifact exception

2. **Wakeup Phase** (`jm_send_wakeup`)
   - Tool writes 4 wakeup packets to the sector
   - Each packet starts with magic `0x197b0325` followed by a fixed sequence and CRC
   - Wakes the controller from idle state

3. **Command Phase** (`jm_execute_command`)
   - Tool writes a scrambled command to the sector (XOR'd with fixed pattern + CRC)
   - Controller intercepts the write, processes it, writes scrambled response back
   - Tool reads the sector and unscrambles the response
   - Repeats for each disk query (IDENTIFY, SMART READ VALUES, SMART READ THRESHOLDS)

4. **Cleanup Phase** (`jm_cleanup_device`)
   - Tool writes 512 bytes of zeros to the sector
   - **Sector is restored to empty state**
   - Signal handlers ensure cleanup on interruption (SIGINT, SIGTERM, SIGHUP, SIGQUIT)
   - Device is closed

### Why Sector 33 (0x21)?

Sector `33` (0x21) is the original sector used by Werner Johansson's `jJMRaidCon` (2010), the first known implementation of this protocol, and also used by HD Sentinel's JMicron module. Using the same sector ensures maximum compatibility with the controller firmware, which may have specific handling for this location.

Although sector 33 is technically within the range normally reserved for system structures (sectors 0–63), it sits well past the MBR (sector 0), partition table, and GPT headers, and is reliably empty on RAID volumes where the host OS does not create partitions on the raw device. The tool enforces a mandatory emptiness check before use.

You can use a different sector with `--sector XXXX`. The tool permits:
- Sector `33` (0x21) — original default
- Sectors `64`–`2047` — partition gap (before typical first partition at sector 2048)

### Why SG_IO Rather Than Normal Block I/O?

The tool uses SG_IO (SCSI Generic I/O), a Linux kernel interface for issuing raw SCSI commands directly to a device, bypassing the block device layer and OS page cache. This is necessary because:

- The JMicron controller intercepts SCSI READ(10) and WRITE(10) commands for the communication sector and substitutes its own data
- SG_IO ensures commands reach the controller's firmware directly
- Normal block device reads (`dd`, `blockdev`) may return OS-cached data rather than what the controller actually returns via SCSI pass-through

This distinction is significant when diagnosing issues — `dd if=/dev/sdX skip=33` may show zeros while `jmraidstatus` sees non-zero data, because the controller's mailbox state is only visible via SG_IO.

### Timing

The entire process takes **1–3 seconds** typically:

- Safety check: ~1ms
- Wakeup sequence (4 writes): ~100ms
- Disk queries (5 slots × 3 commands): ~1–2 seconds
- Cleanup: ~1ms

During this time, **the communication sector contains JMicron protocol data**.

---

## Stale Mailbox State

### What It Is

The most common operational failure mode is the **stale mailbox**: the controller's communication sector contains leftover JMicron protocol data from a previous run that did not complete cleanup.

When this happens, `jmraidstatus` would previously refuse to run with a sector-not-empty error. Hourly monitoring jobs would then fail continuously until manually cleared.

### Why It Happens

The primary cause is **SIGKILL** — when a process is killed with SIGKILL (e.g., system shutdown, OOM killer, `kill -9`), no signal handlers fire and cleanup is skipped. The signal handlers installed by this tool handle SIGINT, SIGTERM, SIGHUP, and SIGQUIT, but SIGKILL cannot be caught.

A secondary cause is the **controller's response-pending state**. After the tool writes a command and the controller writes its response back to the sector, the controller may consider the response "pending" until the host reads it. If the tool crashes between writing the command and reading the response, the controller continues returning the response on subsequent reads — even across USB reconnects — until something reads it to "acknowledge" it.

This explains a subtle diagnostic symptom: the sector may appear to contain data via SG_IO for an extended period (hours or days), while `dd` shows all zeros. The OS block device layer may have the sector cached as zeros from before the write, while the controller's mailbox holds live data visible only via SG_IO direct reads.

### JMicron Artifact Auto-Clear (Current Behavior)

When the safety check finds non-zero data in the communication sector, `jmraidstatus` now checks for recognized JMicron protocol signatures:

| First 4 bytes | Source |
|---|---|
| `25 03 7b 19` | Wakeup packet (`JM_RAID_WAKEUP_CMD = 0x197b0325`, not scrambled) |
| `2a c2 1c 5d` | Command or controller response (`0x197b0322` XOR scrambling key) |

If either signature is detected, the tool:
1. Logs a warning to stderr (always, even in quiet mode)
2. Writes zeros to the sector via SG_IO to clear the controller's mailbox state
3. Proceeds normally

If the sector contains **unrecognized non-zero data**, the tool refuses to run — that data could be real partition or filesystem content, not a protocol artifact.

**Example warning output:**
```
Warning: Sector 33 contained a stale JMicron protocol artifact (leftover from interrupted run). Clearing and proceeding.
  First 4 bytes: 25 03 7b 19
```

### Diagnosing a Stale Mailbox

If the tool reports a sector-not-empty error with unrecognized content, use the `read_sector` diagnostic tool to examine what is actually there via SG_IO:

```bash
# Build (requires gcc)
gcc -o tools/read_sector tools/read_sector.c

# Read sector 33 via SG_IO and dump as hex
sudo tools/read_sector /dev/sdX 33
```

This reads the sector exactly as `jmraidstatus` does and identifies any JMicron signatures found. The first 4 bytes are the key indicator.

Compare this with the block device view:
```bash
# Read sector 33 via normal block I/O
sudo dd if=/dev/sdX skip=33 count=1 bs=512 2>/dev/null | xxd | head -4
```

If `read_sector` shows data and `dd` shows zeros, the controller is holding a stale response in its mailbox — a strong indicator of a JMicron artifact rather than real partition data.

---

## Risks and Failure Modes

### Risk 1: Tool Interruption Before Cleanup

**Scenario**: Tool killed with SIGKILL, power loss, or system crash before `jm_cleanup_device` runs.

**Result**:
- Communication sector remains filled with the last wakeup packet or command/response
- Signal handlers (SIGINT, SIGTERM, SIGHUP, SIGQUIT) are not invoked by SIGKILL
- On next run: auto-clear if recognized JMicron signature, else refuse

**Mitigation**:
- Signal handlers cover most graceful termination cases
- Auto-clear logic handles SIGKILL aftermath for recognized JMicron artifacts

### Risk 2: Communication Sector Contains Real Data

**Scenario**: Partition or filesystem data happens to occupy sector 33 (or whichever sector is configured).

**Result**: Tool refuses to run — safety check detects non-zero data with unknown signature.

**Likelihood**: Very low for RAID volumes where the host OS does not partition the raw device.

**Detection**: Automatic — tool checks sector content and shows first 16 bytes if unrecognized.

### Risk 3: Controller Malfunction

**Scenario**: Controller firmware interprets a command incorrectly or behaves unexpectedly.

**Result**: Unknown — this is likely what caused the HD Sentinel data loss incident.

**Quote from HD Sentinel developer**:
> "This tested on several systems/enclosures and on most of them, everything worked perfectly. But in one case, we found that it resulted the RAID array to break and the complete information stored on the drives lost."

**Likelihood**: Extremely rare (1 known case).

---

## Verifying Sectors Manually

### Using check_sectors

```bash
sudo tools/check_sectors /dev/sdX
```

This checks multiple candidate sectors (33, 64, 128, 256, 512, 1024, 1536, 2000, 2047) via the block device layer and shows which are empty.

**Note**: This uses `dd` (block device I/O), not SG_IO. For a complete picture, also run `read_sector` against the sector you intend to use.

### Manual Check

```bash
# Block device view
sudo dd if=/dev/sdX skip=33 count=1 bs=512 2>/dev/null | xxd | head -4

# SG_IO view (what jmraidstatus actually sees)
sudo tools/read_sector /dev/sdX 33

# Partition layout (confirm sector 33 is before first partition)
sudo fdisk -l /dev/sdX
```

---

## Emergency Recovery

### Automatic Recovery (Normal Case)

If the sector contains a JMicron protocol artifact, `jmraidstatus` clears it automatically on the next run with a warning. No manual intervention is needed.

### Manual Recovery

If `jmraidstatus` refuses to run because the sector contains **unrecognized data**, investigate before clearing anything:

1. Check what is actually there:
   ```bash
   sudo tools/read_sector /dev/sdX 33
   sudo dd if=/dev/sdX skip=33 count=1 bs=512 2>/dev/null | xxd
   sudo fdisk -l /dev/sdX
   ```

2. If you have confirmed it is safe to zero (not partition data), use `zero_sector`:
   ```bash
   # Build if not already built
   gcc -o tools/zero_sector tools/zero_sector.c

   sudo tools/zero_sector /dev/sdX 33
   ```

   `zero_sector` requires explicit `yes` confirmation and validates the sector number. It permits sector `33` (0x21) explicitly, and sectors `64`–`2047`. It refuses sector `0` and sectors `1`–`32` and `34`–`63`.

3. Use a different sector if you are unsure:
   ```bash
   sudo jmraidstatus --sector 1024 /dev/sdX
   ```

---

## Why This Approach?

This unusual communication method exists because:

1. **JMicron controllers sit between the host and disks** — normal ATA pass-through doesn't reach individual disks
2. **No official API** — JMicron doesn't provide documentation or drivers for SMART access
3. **Reverse engineering** — this protocol was discovered by analyzing traffic, not from specs
4. **Hardware RAID limitation** — the controller intercepts normal SMART commands and returns controller status instead of individual disk status

## Comparison to HD Sentinel

HD Sentinel also implements this protocol (their `detjm` module) and uses sector `33` as the communication sector, same as this tool. Key differences:

- HD Sentinel on Linux is **disabled by default** due to the data loss incident
- Their module requires explicit download and installation
- **Same underlying protocol** — compatibility with HD Sentinel behavior is intentional

## Technical References

- Original JMRaidCon: https://github.com/wjoe/jmraidcon
- HD Sentinel Discussion: https://www.hdsentinel.com/forum/viewtopic.php?p=18046#p18046
- Protocol details: `docs/PROTOCOL.md`
- Source code: `src/jm_protocol.c`, `src/jmraidstatus.c`

## Summary

The JMicron protocol **temporarily uses a communication sector** (default: `33`) as a mailbox with the controller. The controller intercepts sector writes matching the JMicron command format (magic bytes + CRC).

**Key Points:**

- ✅ Sector `33` (0x21) — original JMicron default, maximum controller compatibility
- ✅ Tool reads sector via SG_IO (bypasses OS cache, sees actual controller state)
- ✅ Tool verifies sector is empty before use — mandatory safety check
- ✅ Recognized JMicron artifacts auto-cleared with a warning
- ✅ Sector restored to zeros after operations, with signal handling for graceful exits
- ⚠️ SIGKILL cannot be caught — may leave stale mailbox state
- ⚠️ `dd` vs SG_IO discrepancy is expected and normal when controller holds a pending response
- ⚠️ Controller malfunction risk exists (extremely rare but documented)

**Always have complete backups before use.**
