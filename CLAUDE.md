# Claude Code Project Guide - jm-raid-status

## Project Overview

SMART health monitoring tool for JMicron RAID controllers using reverse-engineered proprietary protocol. Reads individual disk SMART data from behind hardware RAID controllers where smartctl cannot reach.

## Key Documentation

1. **PROTOCOL.md** - Complete reverse-engineered protocol documentation
   - Command formats, response structures, CRC validation
   - **CRITICAL**: RAID degradation flag at offset 0x1F0
   - Safety considerations and sector selection

2. **SECTOR_USAGE.md** - Technical details about sector-as-mailbox communication
   - Why sector 1024 is used (changed from original 0x21 for safety)
   - Risks and safety mechanisms
   - HD Sentinel data loss incident

3. **README.md** - User-facing documentation
   - Installation, usage, examples
   - Warnings about data loss risks

4. **QUICK_START.md** - Quick reference for end users

## Critical Protocol Discovery

### RAID Degradation Detection (Feb 2026)

**Location**: Offset `0x1F0` in IDENTIFY DEVICE response (512-byte response)

**Values**:
- `0x07` = **Degraded** - One or more disks missing/failed, cannot rebuild, data at risk
- `0x0F` = **Operational** - All disks present (healthy OR rebuilding)

**Key Insights**:
- Flag appears in **ALL** disk responses (disks 0-4, even empty slots)
- Controller does NOT distinguish healthy from rebuilding - both report `0x0F`
- Rebuilding state (per manual): Red LED blinking on affected disk
- Healthy state: Blue LEDs on all disks
- This is RAID-wide status, not disk-specific

**Test Data**: See `tests/data/`
- `degraded_state.txt` - Raw dumps with 3 disks, slot 3 missing (flag = 0x07)
- `healthy_state.txt` - Raw dumps with 4 disks during rebuild (flag = 0x0F)

**Implementation**:
- Detection in `src/jm_commands.c`: `jm_get_disk_identify()` checks offset 0x1F0
- Sets env var `JMRAIDSTATUS_DEGRADED=1` when flag is 0x07
- Main code in `jm_get_all_disks_smart_data()` checks env var and reports

## Project Structure

```
src/
  jmraidstatus.c      - Main CLI tool, argument parsing, USB detection
  jm_protocol.c       - Low-level sector I/O, SG_IO, CRC validation
  jm_commands.c       - IDENTIFY, SMART commands, degraded detection
  smart_parser.c      - Parse SMART attribute data
  smart_attributes.c  - Attribute definitions and health assessment
  output_formatter.c  - Summary, full, JSON output formats
  jm_crc.c           - CRC-32 checksum
  sata_xor.c         - XOR parity calculations

tests/
  check_sectors      - Shell script to find safe sectors
  data/              - Raw protocol captures for analysis

docs/ (in root)
  PROTOCOL.md        - Protocol reverse engineering documentation
  SECTOR_USAGE.md    - Safety and sector selection details
```

## Hardware Tested

- **Mediasonic Proraid HFR2-SU3S2** (JMB567 via USB) ✅ Working
- **Synology NAS** with JMicron controller ✅ Working

## Safety Warnings

**CRITICAL RISKS**:
1. Tool temporarily overwrites sector 1024 (or user-specified)
2. HD Sentinel reported one case of RAID array failure and data loss
3. Triple-layer safety: range validation, empty check, backup/restore
4. **Always have complete backups before use**

## Development Notes

### Building
```bash
make              # Build
make clean        # Clean build artifacts
make install      # Install to /usr/local/bin (requires sudo)
```

### Testing Degraded Detection

**DO NOT remove disk during rebuild!** Wait for:
1. Blue LEDs on all disks (rebuild complete)
2. Then remove one disk to test degraded detection
3. Tool should show warning and exit code 1

### Debug Mode

```bash
# Dump raw protocol responses
JMRAIDSTATUS_DUMP_RAW=1 sudo ./bin/jmraidstatus /dev/sdX

# Verbose mode
sudo ./bin/jmraidstatus --verbose /dev/sdX
```

## Common Issues

1. **WSL**: Hardware detection skipped, works via direct /dev/sdX access
2. **Permissions**: Requires sudo or user in 'disk' group
3. **Sector not empty**: Tool refuses to run - safety feature working as designed
4. **CRC errors**: Communication failure, check USB connection

## Future Work

See PROTOCOL.md "Future Research" section:
- Controller configuration commands
- RAID rebuild progress monitoring
- LED control
- Disk power management

## Release Process

Releases use GitHub Actions (`.github/workflows/release.yml`):
1. Tag version: `git tag -a vX.Y.Z -m "Message"`
2. Push tag: `git push origin vX.Y.Z`
3. Workflow builds binary, creates release with:
   - Binary tarball
   - README.md, LICENSE, QUICK_START.md, SECTOR_USAGE.md
   - SHA256 checksums

## License

MIT License - See LICENSE file

## Credits

- Werner Johansson - Original jJMRaidCon (2010)
- HD Sentinel team - JMicron implementation insights
- Jamie Treworgy - This implementation (2026)
