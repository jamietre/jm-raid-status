# jmraidstatus Quick Start Guide

## ⚠️ IMPORTANT: Read Before Use

**This tool uses a reverse-engineered protocol that has caused RAID array failure and data loss in at least one documented case.**

**The tool temporarily overwrites an unused sector on your disk (default: 1024) as a communication channel.** The tool verifies the sector is empty before use and will refuse to run if it contains data. If interrupted, this sector remains corrupted.

**ENSURE YOU HAVE COMPLETE BACKUPS BEFORE USING THIS TOOL.**

See the [full warning in the README](README.md#overview) and [SECTOR_USAGE.md](SECTOR_USAGE.md) for technical details.

## Build

```bash
cd /home/jamiet/code/jmraidstatus
make
```

## Basic Usage

```bash
# Check all disks (default summary view)
sudo ./jmraidstatus /dev/sdc

# Full SMART table for disk 0
sudo ./jmraidstatus -d 0 -f /dev/sdc

# JSON output (for scripts)
sudo ./jmraidstatus -j /dev/sdc

# Quiet mode (exit code only)
sudo ./jmraidstatus -q /dev/sdc
echo $?  # 0=good, 1=warning, 2=critical, 3=error
```

## Common Commands

| Command | Description |
|---------|-------------|
| `./jmraidstatus --help` | Show help |
| `./jmraidstatus --version` | Show version |
| `./jmraidstatus /dev/sdc` | Check all disks (summary) |
| `./jmraidstatus -d 0 -f /dev/sdc` | Full SMART for disk 0 |
| `./jmraidstatus -a -j /dev/sdc` | All disks as JSON |
| `./jmraidstatus -q /dev/sdc` | Quiet (exit code only) |

## Exit Codes

- **0** = All disks healthy
- **1** = Warning (monitor closely)
- **2** = Critical (backup data now!)
- **3** = Error (check device/permissions)

## Troubleshooting

**"Cannot open device"**
→ Run with `sudo` (needs root)

**"Not an SG device"**
→ Check device path: `ls -l /dev/sdc`

**No disks found**
→ Verify RAID controller is working

## Files Changed

### New Files (10)
- `src/smart_parser.h/c` - SMART data parsing
- `src/smart_attributes.h/c` - SMART attribute definitions
- `src/jm_protocol.h/c` - JMicron protocol
- `src/jm_commands.h/c` - High-level commands
- `src/output_formatter.h/c` - Output formatting

### Modified Files (3)
- `src/jmraidstatus.c` - Rewritten main program
- `Makefile` - Updated build
- `README.md` - New documentation

### Preserved Files (5)
- `src/jmraidstatus_original.c` - Original backup
- `src/jm_crc.c/h` - CRC (unchanged)
- `src/sata_xor.c/h` - XOR (unchanged)

## What Changed

**Before:**
- Raw hex dumps only
- No health assessment
- No command-line options
- Cryptic errors

**After:**
- Human-readable output
- SMART health assessment
- Multiple output formats
- Helpful error messages
- Scriptable (JSON, exit codes)

## Testing Needed

The code compiles and basic functionality works, but **hardware testing is required**:

1. Connect to actual JMB394 controller
2. Run: `sudo ./jmraidstatus /dev/sdc` (replace with your device)
3. Verify SMART data is correct
4. Test with disks in different health states
5. Test with missing disks (gaps in array)

## Integration Examples

**Daily health check via cron:**
```bash
#!/bin/bash
# /etc/cron.daily/raid-check
/usr/local/bin/jmraidstatus /dev/sdc | mail -s "RAID Health" admin@example.com
```

**Nagios check:**
```bash
#!/bin/bash
/usr/local/bin/jmraidstatus -q /dev/sdc
exit $?
```

**JSON for monitoring:**
```bash
sudo ./jmraidstatus -j /dev/sdc > /var/lib/metrics/raid-health.json
```

## Need Help?

- Read the full documentation: `cat README.md`
- Check implementation details: `cat IMPLEMENTATION_SUMMARY.md`
- View help: `./jmraidstatus --help`
