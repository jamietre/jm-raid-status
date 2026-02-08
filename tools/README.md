# Tools Directory

Utility programs for jm-raid-status maintenance and troubleshooting.

## zero_sector - Emergency Sector Cleanup

**Purpose:** Overwrites a specific sector with zeros. Used for emergency recovery if jmraidstatus fails to clean up its communication sector after abnormal termination.

### When to Use

You should **only** need this tool if:
1. jmraidstatus crashed or was killed before completing cleanup
2. The communication sector (default: 1024) still contains leftover data
3. jmraidstatus refuses to run because the sector is not empty

### Building

```bash
cd tools
gcc -o zero_sector zero_sector.c
```

Or from the project root:
```bash
make tools
```

### Usage

```bash
sudo ./zero_sector <device> <sector_number>
```

**Example:**
```bash
sudo ./zero_sector /dev/sde 1024
```

### Safety Features

- Refuses to write to sector 0 (MBR/partition table)
- Refuses to write to sectors < 64 (system area)
- Requires explicit "yes" confirmation before writing
- Requires root/sudo privileges

### Important Notes

⚠️ **WARNING**: This tool can overwrite any sector on your disk. Only use it for cleaning up the jmraidstatus communication sector!

- **Default communication sector**: 1024
- If you used a different sector with `jmraidstatus --sector N`, specify that sector number
- **Always verify** you're using the correct device path before confirming
- This tool is for **emergency recovery only** - normal cleanup is automatic

### How It Works

The tool:
1. Validates the sector number (must be ≥64, not 0)
2. Asks for confirmation
3. Opens the device with SCSI generic (sg) interface
4. Writes 512 bytes of zeros to the specified sector
5. Reports success or failure

### Example Recovery Scenario

```bash
# jmraidstatus was interrupted and left data in sector 1024
$ sudo jmraidstatus /dev/sde
ERROR: Sector 1024 is not empty (contains data).
This tool requires an empty sector for communication.
Refusing to overwrite existing data for safety.

# Clean up the sector manually
$ sudo tools/zero_sector /dev/sde 1024
WARNING: This will overwrite sector 1024 on /dev/sde with zeros!
         Make sure this is the correct device and sector.

Continue? (yes/no): yes
Writing zeros to sector 1024...
SUCCESS: Sector 1024 has been overwritten with zeros.

# Now jmraidstatus works again
$ sudo jmraidstatus /dev/sde
[normal output...]
```

## check_sectors - Sector Safety Verification

**Purpose:** Scans for safe sectors to use as communication channels. Helps identify empty sectors that won't conflict with filesystem data.

See `check_sectors` script for usage.

## monitor/ - RAID Monitoring Daemon

**Purpose:** Automated monitoring daemon that runs jmraidstatus periodically and sends email alerts when RAID health changes.

See `monitor/README.md` for complete documentation.

---

For questions or issues with these tools, please open an issue at:
https://github.com/jamietre/jm-raid-status/issues
