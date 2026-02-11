# RAID Flag Investigation

This directory contains the investigation and validation of JMicron RAID controller status flags.

## Why & How

My use case for creating this is to expand storage on my Synology NAS with the cheap mediasonic enclosure I already own. This works fine, but my NAS is in a dark corner and human eyes only see it when there's a problem. I wouldn't be comfortable with a hardware RAID that only lets me know there's a problem with blinking lights. This tool allows active monitoring of the status of the hard drives in the enclosure, so I can create a scheduled task to poll it and report any issues.

This was written almost entirely by Claude Code, through a process of analyzing the controller query responses, and comparing them to known data about the hard disk SMART status obtained using [Hard Disk Sentinel](https://www.hdsentinel.com/) on Windows, which does have the ability to read the drive details.

Additionally, I forced the array into a degraded state by removing a disk and taking snapshots while degraded and during the rebuild process and compared these to good state snapshots.

It's only been tested against the two Mediasonic HFR2-SU3S2 boxes I own, which have about 6 different kinds of drives. A couple limitations and edge cases were observed:

- I have not determined a way to identify an array in a degraded state (e.g. one drive offline) other than comparing the number of drives reported to the number expected using this process
- One drive I own did not report manufacturer SMART thresholds; I don't know if this is a problem with the drive or the software/controller, but in this case we use default thresholds
- There appears to be a flag reported when the array is in the process of rebuilding. This has only been tested once but it was definitely set only during the rebuild process, so I have reasonable confidence.

If you are intrested in experimenting more, the repo includes a number of test script for collecting and comparing data.

## Summary of Findings

**Confirmed useful flags for monitoring**:

1. **0x1F0** (byte 0) - Disk presence bitmask
2. **0x1F5** (byte 5) - Rebuild status flag

See `../docs/RAID_FLAGS.md` for implementation reference.

## Directory Structure

```
investigation/
├── README.md              ← This file
├── FLAG_ANALYSIS.md       ← Detailed technical analysis
└── data/
    ├── key_snapshots/     ← The 4 critical test captures
    │   ├── healthy_state.txt
    │   ├── degraded_state.txt
    │   ├── rebuilding_state_1.txt
    │   └── healthy_state_after_rebuild.txt
    └── archive/           ← Additional captures during testing
```

## Key Snapshots

The 4 critical snapshots that define our understanding:

1. **healthy_state.txt** (Feb 7, 10:54)
   - Original baseline: RAID5 with all 4 disks
   - Flags: `0x1F0=0x0f, 0x1F5=0x00`

2. **degraded_state.txt** (Feb 7, 10:52)
   - After removing disk 3
   - Flags: `0x1F0=0x07, 0x1F5=0x00`

3. **rebuilding_state_1.txt** (Feb 7, 11:12)
   - During active rebuild after disk re-insertion
   - Flags: `0x1F0=0x0f, 0x1F5=0x01`

4. **healthy_state_after_rebuild.txt** (Feb 8, 09:35)
   - After rebuild completed
   - Flags: `0x1F0=0x0f, 0x1F5=0x00`

## Flag Behavior Verified

| State              | 0x1F0 | 0x1F5 | Interpretation                    |
| ------------------ | ----- | ----- | --------------------------------- |
| Healthy (4 disks)  | 0x0f  | 0x00  | All disks present, not rebuilding |
| Degraded (3 disks) | 0x07  | 0x00  | One disk missing, not rebuilding  |
| Rebuilding         | 0x0f  | 0x01  | All disks present, rebuild active |

## Test Configuration

- **Hardware**: Mediasonic ProBox HF2-SU3S2 (JMicron JMS561 controller)
- **RAID Level**: RAID5
- **Disk Count**: 4 disks
- **Connection**: USB 3.0 (WSL2 environment)

## Testing Methodology

1. Captured baseline healthy state
2. Physically removed disk 3 from enclosure
3. Captured degraded state (verified disk count changed)
4. Re-inserted disk 3
5. Captured rebuilding state (verified rebuild flag set)
6. Waited for rebuild completion (~18 hours)
7. Captured post-rebuild state (verified rebuild flag cleared)
8. Verified current live state matches post-rebuild

## Questions Investigated

### ✅ Answered

- **Q: How to detect degraded array?**
  A: Check bit count in `0x1F0` against expected disk count

- **Q: How to detect active rebuild?**
  A: Check if `0x1F5 == 0x01`

- **Q: Does rebuild flag clear when complete?**
  A: Yes, confirmed `0x1F5` returns to `0x00` when rebuild finishes

### ❓ Still Unknown

- **Q: What does byte `0x1F9` indicate?**
  A: Unknown - observed as `0x80` in most states, transiently `0x00`, not reliable for monitoring

- **Q: Is there a rebuild progress indicator?**
  A: None found in these flags - only active/inactive status available

- **Q: Can we detect RAID level (0/1/5)?**
  A: Not from these flags

## Related Documentation

- `../docs/RAID_FLAGS.md` - User-facing reference guide
- `FLAG_ANALYSIS.md` - Detailed technical analysis
- `../tests/fixtures/` - Test data used in unit tests

## Validation Status

- ✅ Disk presence detection (0x1F0)
- ✅ Rebuild status detection (0x1F5)
- ✅ State transitions during disk removal/insertion
- ✅ Rebuild completion detection
- ❓ Purpose of other flag bytes
- ❓ Rebuild progress percentage
