# JMicron RAID Protocol Documentation

This document describes the reverse-engineered JMicron RAID controller protocol used by `jm-raid-status`.

## Overview

JMicron RAID controllers use a proprietary protocol to communicate with individual disks behind the RAID array. Unlike standard SMART commands that return aggregate RAID status, this protocol allows querying each disk independently.

**Protocol Communication Method:**
- Uses a disk sector as a "mailbox" for bidirectional communication
- Default sector: `33` (0x21) — original Werner Johansson default, maximum controller compatibility
- Controller reads commands from the sector, writes responses back
- Requires exclusive access (sector must be empty)

## Communication Flow

1. **Backup sector**: Read and save original sector data
2. **Send command**: Write command packet to sector
3. **Execute**: Controller processes command
4. **Read response**: Read controller's response from same sector
5. **Restore**: Write original data back to sector

## Command Structure

### Scrambled Command Format (512 bytes)

All commands use a "scrambled" packet format:

```
Offset  Size  Description
------  ----  -----------
0x00    4     Magic: 0x197B0322 (JM_RAID_SCRAMBLED_CMD, little-endian)
0x04    4     Command counter (increments with each command)
0x08    var   Command-specific data (probe command bytes)
...     ...   Padding with zeros to 512 bytes
```

### Command Counter
- Starts at 1, increments with each command
- Used for: sequencing, CRC calculation
- Controller echoes this in responses

## Known Commands

### IDENTIFY DEVICE (probe11-15)

Retrieves disk identification information (model, serial, firmware, capacity).

**Command bytes (starting at offset 0x08):**
```
0x00, 0x02, 0x02, 0xff,
disk_num,              // Disk slot number (0-4)
0x00, 0x00, 0x00, 0x00,
disk_num               // Disk number repeated
```

**Response (512 bytes):**
```
Offset  Size  Description
------  ----  -----------
0x00    16    JMicron header (command echo)
0x10    32    Model number (ATA byte-swapped)
0x30    16    Serial number (ATA byte-swapped)
0x50    8     Firmware revision (ATA byte-swapped)
0x4A    6     48-bit sector count (little-endian)
...     ...   Additional ATA IDENTIFY fields
0x1F0   1     RAID status flag (see below)
0x1F4   4     CRC-32 checksum
```

**RAID Status Flag (offset 0x1F0):**
- `0x0F` = RAID array is operational (all disks present)
  - Includes: healthy state AND rebuilding state
  - Safe to operate - redundancy present or being restored
- `0x07` = RAID array is degraded (one or more disks missing/failed)
  - Cannot rebuild, data at risk
  - Reduced or no redundancy
- **This flag appears in ALL disk responses**, including empty slots
- Allows reliable degradation detection without capacity calculations
- **Note:** Flag does not distinguish healthy from rebuilding - both report 0x0F

**Return values:**
- `0` = Disk present and identified successfully
- `-1` = Communication error (CRC failure, command timeout)
- `-2` = Empty slot (valid communication, no disk present)

### SMART READ ATTRIBUTE VALUES (0xD0)

Reads SMART attribute values from a specific disk.

**Command bytes (starting at offset 0x08):**
```
0x00, 0x02, 0x03, 0xff,
disk_num,              // Disk slot number (0-4)
0x02, 0x00, 0xe0, 0x00, 0x00,
0xd0,                  // ATA SMART READ ATTRIBUTE VALUE
0x00, 0x00, 0x00, 0x00, 0x00,
0x4f, 0x00, 0xc2, 0x00, 0xa0, 0x00, 0xb0, 0x00
```

**Response (512 bytes):**
```
Offset  Size  Description
------  ----  -----------
0x00    32    JMicron header/echo
0x20    480   Standard ATA SMART data page
              - 30 x 12-byte attribute entries
              - Offline data collection status
              - SMART capabilities
```

### SMART READ ATTRIBUTE THRESHOLDS (0xD1)

Reads SMART attribute threshold values.

**Command bytes (starting at offset 0x08):**
```
0x00, 0x02, 0x03, 0xff,
disk_num,              // Disk slot number (0-4)
0x02, 0x00, 0xe0, 0x00, 0x00,
0xd1,                  // ATA SMART READ ATTRIBUTE THRESHOLDS
0x00, 0x00, 0x00, 0x00, 0x00,
0x4f, 0x00, 0xc2, 0x00, 0xa0, 0x00, 0xb0, 0x00
```

**Response format:** Similar to VALUES but contains threshold data instead.

## Data Formats

### ATA String Byte-Swapping

ATA IDENTIFY strings are stored with bytes swapped within 16-bit words:

```
Stored: "D W  C"  (0x44 0x57 0x20 0x43)
Actual: "WD C "
```

Swap algorithm:
```c
for (i = 0; i < len; i += 2) {
    dest[i] = src[i + 1];
    dest[i + 1] = src[i];
}
```

### Empty Slot Detection

A disk slot is considered empty when the IDENTIFY response:
- Has valid CRC (communication succeeded)
- Model string area (0x10-0x2F) contains < 20 printable chars OR < 8 non-space chars
- OR entire response is all 0x00 or all 0xFF

## CRC Validation

**Algorithm:** CRC-32 (standard polynomial 0xEDB88320)

**Checksum location:** Last 4 bytes of response (offset 0x1FC-0x1FF)

**Checksum data:** First 508 bytes of response (0x00-0x1FB)

**Validation:**
```c
uint32_t computed = crc32(response, 508);
uint32_t received = *(uint32_t*)(response + 0x1FC);
if (computed != received) {
    // CRC error - communication failure
}
```

## Safety Considerations

See [docs/SECTOR_USAGE.md](SECTOR_USAGE.md) for full details on sector selection rationale, safe/unsafe sector ranges, validation logic, stale mailbox handling, and risk analysis.

## Protocol Quirks

### WSL Detection
In WSL environments, hardware detection is skipped as:
- `/proc/bus/pci` may not be accessible
- USB sysfs paths are unreliable
- Direct device access works via `/dev/sdX`

### Disk Slot Numbering
- Slots numbered 0-4 (5 total)
- Most controllers support 4 disks (slots 0-3)
- Slot 4 typically unused
- JMB567 supports up to 5 disks

## Known Controller Models

**USB Controllers:**
- **JMB567**: Product ID 0x0567, supports 5 disks
- **JMB575**: Product ID 0x0575, supports 5 disks
- USB Vendor ID: 0x152d (JMicron USB)

## Testing Hardware

**Successfully tested on:**
- Mediasonic Proraid HFR2-SU3S2 (JMB567 via USB)
- Synology NAS with JMicron controller

**If you test on other hardware, please report results!**

## Research Credits

This implementation is based on reverse engineering by:
- **Werner Johansson** - Original [jJMRaidCon](https://github.com/wjoe/jmraidcon) (2010)
- **H+BEDV Datentechnik GmbH** - HD Sentinel JMicron implementation
- **Jamie Treworgy** - This implementation with enhanced safety and protocol analysis (2026)

## Future Research

Potential areas for investigation:

1. **Controller configuration commands**: Can we read/modify RAID settings?
2. **Other probe commands**: Are there probe commands 0-10, 16+?
3. **RAID rebuild status**: Can we query rebuild progress?
4. **LED control**: Can we identify/blink disk LEDs?
5. **Disk power management**: Can we spin up/down individual disks?

## Reference Implementation

See `src/jm_commands.c` and `src/jm_protocol.c` for full implementation details.

## License

This protocol documentation is released to the public domain for educational and research purposes.
