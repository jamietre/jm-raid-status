# Claude Code Project Guide - jm-raid-status

## Project Overview

Multi-source SMART health monitoring platform with three complementary tools:

1. **jmraidstatus** - JMicron RAID controller monitor using reverse-engineered proprietary protocol. Reads individual disk SMART data from behind hardware RAID controllers where smartctl cannot reach.

2. **smartctl-parser** - Converts smartctl JSON output to a common format for aggregation.

3. **disk-health** - Multi-source aggregator that combines SMART data from JMicron arrays and individual drives into unified reports.

**Architecture**: Unix pipe-based composition using NDJSON (Newline-Delimited JSON) format:
```bash
{
  sudo jmraidstatus --json-only /dev/sdc
  smartctl --json=c --all /dev/sda | smartctl-parser
} | disk-health
```

## Key Documentation

1. **docs/PROTOCOL.md** - Complete reverse-engineered protocol documentation
   - Command formats, response structures, CRC validation
   - **CRITICAL**: RAID degradation flag at offset 0x1F0
   - Safety considerations and sector selection

2. **docs/SECTOR_USAGE.md** - Technical details about sector-as-mailbox communication
   - Why sector 1024 is used (changed from original 0x21 for safety)
   - Risks and safety mechanisms
   - HD Sentinel data loss incident

3. **README.md** - User-facing documentation
   - Installation, usage, examples
   - Warnings about data loss risks
   - Multi-source monitoring examples

4. **docs/JSON_API.md** - JSON output schema and API documentation
   - Complete field descriptions and possible values
   - Exit code mappings
   - Scripting examples
   - **MUST be updated when JSON output changes**

5. **docs/MULTI_SOURCE_PLAN.md** - Multi-source architecture implementation plan
   - Unix pipe-based composition design
   - NDJSON format specification
   - Three-binary architecture rationale

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

### Overview

Three separate binaries with Unix pipe-based composition:
1. **jmraidstatus** - JMicron RAID controller monitor
2. **smartctl-parser** - Converts smartctl JSON to common format
3. **disk-health** - Multi-source aggregator

```
src/
  # JMicron RAID monitor (jmraidstatus binary)
  jmraidstatus.c      - Main CLI tool, argument parsing
  jm_protocol.c       - Low-level sector I/O, SG_IO, CRC validation
  jm_commands.c       - IDENTIFY, SMART commands, degraded detection
  jm_crc.c           - CRC-32 checksum
  sata_xor.c         - XOR parity calculations
  hardware_detect.c   - JMicron USB hardware detection

  # Shared SMART parsing (used by all binaries)
  smart_parser.c      - Parse SMART attribute data
  smart_attributes.c  - Attribute definitions and health assessment
  output_formatter.c  - Summary, full, JSON output formats
  config.c           - Configuration file parsing

  # smartctl-parser binary
  parsers/
    smartctl_parser.c - Convert smartctl JSON to disk-health format
    common.c          - Shared JSON utilities

  # disk-health aggregator binary
  aggregator/
    disk_health.c     - Multi-source SMART aggregator

  # Dependencies
  jsmn/
    jsmn.h           - Lightweight JSON parser (single header)

tests/
  check_sectors            - Shell script to find safe sectors
  integration/
    test_aggregator.sh     - Integration test suite (12 tests)
  data/
    jmicron/              - Sample JMicron RAID JSON outputs
      healthy-4disk.json
      degraded-3disk.json
      failed-disk.json
    smartctl/             - Sample smartctl JSON outputs
      healthy-ssd.json

examples/
  mixed-sources.sh        - Example multi-source monitoring script

docs/
  PROTOCOL.md             - Protocol reverse engineering documentation
  SECTOR_USAGE.md         - Safety and sector selection details
  JSON_API.md             - JSON output schema documentation
  MULTI_SOURCE_PLAN.md    - Multi-source implementation plan
  CONTRIBUTING.md         - Contribution guidelines
  PROJECT_STRUCTURE.md    - Historical project structure notes
```

## Hardware Tested

- **Mediasonic Proraid HFR2-SU3S2** (JMB567 via USB) ✅ Working
- **Synology NAS** with JMicron controller ✅ Working

## Safety Warnings

**CRITICAL RISKS**:
1. Tool temporarily overwrites sector `1024` (or user-specified)
2. HD Sentinel reported one case of RAID array failure and data loss
3. Triple-layer safety: range validation, empty sector verification, cleanup with signal handling
4. **Always have complete backups before use**

## Development Notes

### Building
```bash
make              # Build all three binaries (debug mode)
make release      # Build optimized release binaries (-O3, stripped)
make clean        # Clean build artifacts
make test         # Run integration tests (12 tests)
make install      # Install all three binaries to /usr/local/bin (requires sudo)
make help         # Show all available targets
```

Binaries are built to:
- `bin/jmraidstatus` - JMicron RAID monitor
- `bin/smartctl-parser` - smartctl converter
- `bin/disk-health` - Multi-source aggregator

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

## Documentation Style Guide

When writing or updating documentation:

1. **Command names**: Always use backticks around command/program names
   - ✅ `jmraidstatus`, `smartctl`, `dd`, `fdisk`
   - ❌ jmraidstatus, smartctl (no backticks)

2. **File paths**: Use backticks
   - ✅ `/dev/sdc`, `src/jm_protocol.c`, `bin/jmraidstatus`
   - ❌ /dev/sdc (in running text, not code blocks)

3. **Options/flags**: Use backticks
   - ✅ `--verbose`, `--sector`, `-d 0`
   - ❌ --verbose (no backticks)

4. **Technical terms**: Use backticks for specific values
   - ✅ Sector `1024`, offset `0x1F0`, flag `0x07`
   - ❌ sector 1024 (when referring to the specific value)

5. **Code blocks**: Use triple backticks with language
   ```bash
   sudo jmraidstatus /dev/sdc
   ```

6. **Emphasis**: Use **bold** for warnings/important points, _italics_ for quotes

## JSON Output Documentation

**CRITICAL**: When making ANY changes to JSON output format:

1. **Update `docs/JSON_API.md`** - This is the authoritative JSON schema documentation
   - Add/modify field descriptions
   - Update possible values for enums (e.g., `status` values)
   - Add examples showing the new/changed fields
   - Update version history table

2. **Write/Update Tests** in `tests/test_output_formatter.c`
   - Add test cases for new fields/values
   - Verify JSON is valid (balanced braces, proper structure)
   - Test that no extraneous text appears outside JSON output
   - Validate enum values are documented

3. **Why This Matters**:
   - JSON output is consumed by scripts and monitoring systems
   - Undocumented changes break integrations
   - Schema documentation is a contract with users
   - Tests catch regressions (e.g., plain text leaking into JSON mode)

4. **Example Changes Requiring Documentation**:
   - Adding new fields to top-level object or disk objects
   - Changing status values or meanings
   - Adding/removing fields from `raid_status`
   - Modifying attribute structure
   - Changing exit code mappings

5. **Verification Checklist**:
   - [ ] `docs/JSON_API.md` updated with new fields/values
   - [ ] Tests added/updated in `tests/test_output_formatter.c`
   - [ ] Example JSON in docs matches actual output
   - [ ] `make tests` passes
   - [ ] Version history table updated if breaking change

## License

MIT License - See LICENSE file

## Credits

- Werner Johansson - Original jJMRaidCon (2010)
- HD Sentinel team - JMicron implementation insights
- Jamie Treworgy - This implementation (2026)
