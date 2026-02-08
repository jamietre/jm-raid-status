# jmraidstatus - SMART Health Monitor for JMicron RAID Controllers

A console tool to monitor disk health in JMicron hardware RAID arrays, including USB-connected external enclosures.

## Overview

`jmraidstatus` communicates with JMicron SATA RAID controllers to read SMART (Self-Monitoring, Analysis and Reporting Technology) data from disks behind the controller. Unlike `smartctl` which cannot access disks behind hardware RAID controllers, `jmraidstatus` uses the controller's proprietary protocol to retrieve health information.

This tool uses the reverse-engineered JMicron proprietary protocol to communicate with RAID controllers. It has been tested successfully on:

- Mediasonic Proraid HFR2-SU3S2 External hardware RAID box (JMB567 controller)

If you use this on other devices, please report your results! Make a pull request or open an issue.

## Acknowledgments

This project was inspired by the original [jJMRaidCon by Werner Johansson](https://github.com/wjoe/jmraidcon) (2010), which laid groundwork for reverse engineering of the JMicron protocol.

This implementation is a rewrite using the protocol knowledge from that research, with features for SMART parsing, user-friendly output, and modern tooling.

## Features

- **Read SMART data** from disks behind JMicron RAID controllers
- **Automatic hardware detection** for USB enclosures
- **Display human-readable health status** with easy-to-understand summaries
- **Monitor critical attributes** including:
  - Reallocated sectors
  - Pending sectors
  - Uncorrectable sectors
  - Drive temperature
  - Power-on hours
  - And all other standard SMART attributes
- **Multiple output formats**:
  - Summary view (default) - Quick health overview
  - Full table view - Complete SMART attribute listing
  - JSON format - For scripting and automation
- **Exit codes for monitoring systems** - Integrate with monitoring tools
- **Configurable sector number** - Avoid conflicts with data

## Possible Risk

**The JMicron protocol uses a disk sector as a communication channel** (default: sector 1024). The tool temporarily writes commands to this sector and reads responses back. To protect your data:

- **The tool refuses to run if the sector contains any data** - this safety check cannot be bypassed
- Sector contents are verified as empty before any operations
- The sector is restored to zeros after communication completes
- Signal handlers ensure cleanup even if interrupted (Ctrl+C, crashes, etc.)

See [SECTOR_USAGE.md](SECTOR_USAGE.md) for detailed technical explanation of the protocol and safety mechanisms.

**Historical context:** Early implementations of this protocol (including [Hard Disk Sentinel](https://www.hdsentinel.com/)) experienced RAID array failures in at least one case, likely due to overwriting in-use sectors. This implementation includes safety checks specifically designed to prevent this issue. However, as with any tool that interacts destructively with storage hardware, **ensure you have current backups before first use**.

## Installation

[![CI Status](https://github.com/jamietre/jm-raid-status/workflows/CI/badge.svg)](https://github.com/jamietre/jm-raid-status/actions)

### From GitHub Releases (Recommended)

Download the latest pre-built binary from the [Releases page](https://github.com/jamietre/jm-raid-status/releases):

```bash
wget https://github.com/jamietre/jm-raid-status/releases/download/vX.X.X/jmraidstatus-X.X.X-linux-x86_64.tar.gz
tar xzf jmraidstatus-X.X.X-linux-x86_64.tar.gz
sudo install -m 755 jmraidstatus /usr/local/bin/
```

### Building from Source

```bash
git clone https://github.com/jamietre/jm-raid-status.git
cd jm-raid-status
make
```

The binary will be built to `bin/jmraidstatus`.

### Installation (optional)

```bash
sudo make install
```

This installs `jmraidstatus` to `/usr/local/bin/`.

## Usage

```bash
jmraidstatus [OPTIONS] /dev/sdX
```

### Options

- `-h, --help` - Show help message
- `-v, --version` - Show version information
- `-d, --disk DISK` - Query specific disk (0-4)
- `-a, --all` - Query all disks (default)
- `-s, --summary` - Show summary only (default)
- `-f, --full` - Show full SMART attribute table
- `-j, --json` - Output in JSON format
- `-q, --quiet` - Minimal output (exit code only)
- `--verbose` - Verbose output with debug info
- `--force` - Skip hardware detection (use if auto-detection fails)
- `--sector SECTOR` - Use specific sector number (default: 1024, must be empty)

**Note**: For USB-connected RAID enclosures, the tool automatically detects the USB connection and proceeds without additional flags.

### Finding Your RAID Device

To find your RAID enclosure's device path:

```bash
# For USB-connected enclosures, connect/disconnect to see which device appears
lsblk
# or
dmesg | tail
```

Your device path will typically be `/dev/sdX` where X is a letter (e.g., `/dev/sdc`, `/dev/sdq`).

### Examples

**Check all disks (summary view):**

```bash
sudo jmraidstatus /dev/sdc
```

**Full SMART table for disk 0:**

```bash
sudo jmraidstatus -d 0 -f /dev/sdc
```

**JSON output for all disks (for scripts):**

```bash
sudo jmraidstatus -j /dev/sdc > health.json
```

**Quiet mode (exit code only, for monitoring):**

```bash
sudo jmraidstatus -q /dev/sdc
echo $?  # 0=healthy, 1=warning, 2=critical, 3=error
```

### Exit Codes

- `0` - All disks healthy
- `1` - Warning condition detected (e.g., reallocated sectors, elevated temperature)
- `2` - Critical condition detected (e.g., failing disk, uncorrectable sectors)
- `3` - Error (device not found, permission denied, communication error)

## Sample Output

### Summary View (Default)

**USB Enclosure Example:**

```
jmraidstatus v1.0 - SMART Health Monitor
Device: /dev/sdq (Controller: JMB567)

Disk 0: WDC WD100EMAZ-00WJTA0
  Serial: JEK615JN
  Firmware: 83.H0A83
  Size: 9.1 TB
  Status: GOOD
  Temperature: 33°C
  Power On Hours: 30666 hours (1277 days)
  Power Cycles: 84
  No errors detected

Disk 1: WDC WD100EMAZ-00WJTA0
  Serial: JEK4ZU8N
  Firmware: 83.H0A83
  Size: 9.1 TB
  Status: WARNING
  Temperature: 42°C
  Power On Hours: 23,891 hours (995 days)
  Reallocated Sectors: 8
  Current Pending Sectors: 2
  Warning: Disk showing signs of wear

Overall RAID Health: WARNING - Monitor disk(s) closely
```

### Full SMART Table

```
Disk 0: WDC WD30EFRX-68EUZN0
Status: GOOD

SMART Attributes:
ID  Name                        Value Worst Thresh Raw Value      Status
--------------------------------------------------------------------------------
01  Read_Error_Rate             200   200   51     0              OK
05  Reallocated_Sector_Ct       200   200   140    0              OK [CRITICAL]
09  Power_On_Hours              87    87    0      12,456 hours   OK
C5  Current_Pending_Sector      200   200   0      0              OK [CRITICAL]
C6  Offline_Uncorrectable       100   253   0      0              OK [CRITICAL]

Health Assessment:
  OK: No reallocated sectors
  OK: No pending sectors
  OK: No uncorrectable sectors
  OK: Temperature within normal range
```

## Technical Details

### How It Works

jmraidstatus uses Linux SCSI Generic (SG_IO) ioctls to communicate with the RAID controller. It:

1. **Verifies sector is empty** - Checks that the sector contains all zeros (safety requirement)
2. **Sends commands** - Writes protocol commands to the sector (default: sector `1024`)
3. **Exchanges commands/responses** - Uses the sector as a "mailbox" to query each disk
4. **Parses SMART data** - Extracts attribute values and thresholds from responses
5. **Assesses disk health** - Evaluates critical attributes for warnings/failures
6. **Restores sector** - Writes zeros back to the sector (cleanup phase)

**⚠️ IMPORTANT**: The tool **temporarily overwrites an unused sector** (default: 1024) during the 1-3 seconds it runs. The tool **will refuse to run** if the sector contains any data. If somehow interrupted before completion, that sector remains corrupted. See **[SECTOR_USAGE.md](SECTOR_USAGE.md)** for detailed technical explanation and risks.

### SMART Health Assessment

The tool evaluates disk health using these criteria:

**CRITICAL** status if:

- Reallocated Sector Count (0x05) > 0
- Uncorrectable Sector Count (0x06, 0xC6) > 0
- Spin Retry Count (0x0A) > 0
- Any attribute current value ≤ threshold
- Temperature ≥ 60°C

**WARNING** status if:

- Current Pending Sector Count (0xC5) > 0
- Reallocation Event Count (0xC4) > 0
- Current value within 10 of threshold
- Temperature ≥ 50°C

**GOOD** status if:

- All attributes above thresholds
- No critical errors
- Temperature normal

### Supported Disks

The JMB567 controller supports up to 5 disks (numbered 0-4). The tool automatically detects which disks are present and queries only those.

## Requirements

- Linux operating system with SCSI Generic support
- JMicron RAID controller (tested: JMB567)
  - USB-connected external enclosures
- Root privileges (required for raw device access)

## Limitations

- **Requires empty sector**: Uses sector 1024 by default (can be changed with `--sector`, must be all zeros)
- **Controller-specific**: Works only with JMicron JMB3xx series controllers
- **Root required**: Needs root access for SCSI device operations
- **Proprietary protocol**: Based on reverse-engineering of JMicron protocol
- **USB enclosures**: For USB-connected devices, ensure the enclosure uses a JMicron controller internally
- **Sector safety check**: Will refuse to run if the chosen sector contains any data

## Safety

**⚠️ CRITICAL: See the [Important Warning](#overview) section at the top of this document before using this tool.**

**Read [SECTOR_USAGE.md](SECTOR_USAGE.md) to understand how the tool temporarily overwrites an unused sector on your disk.**

The tool takes precautions to avoid data corruption:

- **Verifies sector is empty** before use (must be all zeros - hard requirement)
- **Restores sector to zeros** after operations (cleanup on normal exit and signal handling)
- Validates CRC checksums on all responses
- Only modifies one sector temporarily (default: `1024`, configurable with `--sector`)

**However, there are inherent risks:**

- If the tool crashes or is interrupted before cleanup, **the sector remains corrupted**
- There is documented evidence of this protocol causing **RAID array failure and complete data loss** in at least one case
- The safety check prevents using sectors with data, but **cannot prevent all failure modes**

**Always ensure you have complete, verified backups before using this tool.** See [SECTOR_USAGE.md](SECTOR_USAGE.md) for technical details and how to verify your disk layout is safe.

## Troubleshooting

**"Error: Compatible RAID controller not detected"**

- For USB enclosures, this should auto-detect and work
- If auto-detection fails, use `--force` to bypass hardware detection
- Verify your enclosure uses a JMicron controller internally

**"Error: Cannot open /dev/sdc"**

- Run with `sudo` (root privileges required)
- Verify device path exists: `ls -l /dev/sdc`
- Check device permissions
- For USB devices, ensure the enclosure is powered on and connected

**"Error: Not an SG device"**

- Ensure you're using the correct device path
- The device must be a SCSI generic device (should work with `/dev/sd*`)
- Load sg kernel module if needed: `sudo modprobe sg`

**No disks detected**

- Verify RAID controller is functioning
- Ensure the enclosure is powered on and connected
- Ensure disks are properly inserted
- Try `--verbose` flag to see detailed detection information

## Integration with Monitoring Systems

### Nagios/Icinga

```bash
#!/bin/bash
/usr/local/bin/jmraidstatus -q /dev/sdc
exit $?
```

### Cron Job for Daily Checks

```bash
# Add to /etc/cron.daily/jmraidstatus-check
#!/bin/bash
/usr/local/bin/jmraidstatus /dev/sdc | mail -s "RAID Health Report" admin@example.com
```

### Prometheus Node Exporter (Textfile Collector)

```bash
#!/bin/bash
# Export metrics for Prometheus
/usr/local/bin/jmraidstatus -j /dev/sdc > /var/lib/node_exporter/jmraidcon.prom
```

## Credits

- **Original Protocol Reverse-Engineering**: Werner Johansson (2010)
- **SMART Parsing & UX Improvements**: 2026 enhancements

## License

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

## Contributing

Contributions are welcome! Areas for improvement:

- Dynamic sector selection (auto-detect unused sector)
- Support for more disk vendors' SMART attributes
- Continuous monitoring mode
- Color output support
- Additional output formats

## See Also

- `smartctl` - Standard SMART monitoring tool (doesn't work behind RAID controllers)
- `hdparm` - Get/set hard disk parameters
- Linux SCSI Generic documentation: `/usr/src/linux/Documentation/scsi/scsi_generic.txt`
