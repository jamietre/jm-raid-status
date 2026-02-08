# JSON API Documentation

This document describes the JSON output format for `jmraidstatus --json`.

## Overview

The JSON output provides machine-readable SMART health data for scripting, automation, and integration with monitoring systems. All output is valid JSON with UTF-8 encoding.

## Top-Level Schema

```json
{
  "version": "1.0",
  "device": "/dev/sdX",
  "timestamp": "2026-02-08T12:34:56Z",
  "raid_status": { ... },
  "disks": [ ... ]
}
```

### Top-Level Fields

| Field | Type | Description |
|-------|------|-------------|
| `version` | string | API version (currently "1.0") |
| `device` | string | Device path queried |
| `timestamp` | string | ISO 8601 UTC timestamp of query |
| `raid_status` | object | Overall RAID array health status |
| `disks` | array | Array of disk objects (see below) |

## RAID Status Object

The `raid_status` object provides overall array health and validation:

```json
{
  "status": "healthy",
  "expected_disks": 4,
  "present_disks": 4,
  "rebuilding": false,
  "issues": []
}
```

### RAID Status Fields

| Field | Type | Description |
|-------|------|-------------|
| `status` | string | Overall RAID health status (see values below) |
| `expected_disks` | integer | Expected disk count (only present if `--array-size` specified) |
| `present_disks` | integer | Actual disk count reported by controller bitmask |
| `rebuilding` | boolean | Whether array is rebuilding (currently always `false`, reserved for future) |
| `issues` | array[string] | Human-readable descriptions of any problems detected |

### RAID Status Values

| Value | Meaning | Exit Code |
|-------|---------|-----------|
| `"healthy"` | All disks present and passing SMART checks | 0 |
| `"degraded"` | Fewer disks present than expected (disk failure/removal) | 1 |
| `"oversized"` | More disks present than expected (configuration mismatch) | 0 or 1 |
| `"failed"` | One or more disks failing SMART health checks | 1 |
| `"unknown"` | Status could not be determined | 0 |

### Example Issues Array

```json
"issues": [
  "Degraded: Expected 5 disks but found only 4 disks",
  "Disk 2 (WDC WD80EFZZ): SMART health check failed"
]
```

When `status` is `"healthy"`, the `issues` array will be empty: `[]`

## Disk Object

Each disk in the `disks` array represents a physical drive:

```json
{
  "disk_number": 0,
  "name": "WDC WD80EFZZ-68BTXN0",
  "serial_number": "ABCD1234",
  "firmware_revision": "83.H0A83",
  "size_mb": 7630885,
  "status": "PASSED",
  "temperature_celsius": 35,
  "power_on_hours": 12543,
  "attributes": [ ... ]
}
```

### Disk Fields

| Field | Type | Description |
|-------|------|-------------|
| `disk_number` | integer | Disk slot number (0-4) |
| `name` | string | Disk model name/identifier |
| `serial_number` | string | Disk serial number (may be absent if unavailable) |
| `firmware_revision` | string | Firmware version (may be absent if unavailable) |
| `size_mb` | integer | Disk capacity in megabytes (may be absent if unavailable) |
| `status` | string | Overall disk health status (see values below) |
| `temperature_celsius` | integer | Current temperature in Celsius (may be absent) |
| `power_on_hours` | integer | Total power-on hours (may be absent) |
| `attributes` | array | SMART attributes (see below) |

### Disk Status Values

| Value | Meaning | Description |
|-------|---------|-------------|
| `"PASSED"` | Healthy | All SMART attributes within acceptable range |
| `"FAILED"` | Failed | One or more critical attributes exceed thresholds |
| `"ERROR"` | Error | Could not retrieve SMART data for this disk |

## SMART Attribute Object

Each attribute in a disk's `attributes` array:

```json
{
  "id": 5,
  "name": "Reallocated_Sector_Ct",
  "current": 200,
  "worst": 200,
  "threshold": 140,
  "raw_value": 0,
  "status": "OK",
  "critical": true
}
```

### SMART Attribute Fields

| Field | Type | Description |
|-------|------|-------------|
| `id` | integer | SMART attribute ID (hex, e.g., 5 = 0x05) |
| `name` | string | Human-readable attribute name |
| `current` | integer | Current normalized value (0-255) |
| `worst` | integer | Worst value ever recorded (0-255) |
| `threshold` | integer | Manufacturer failure threshold (0-255) |
| `raw_value` | integer | Raw value (meaning varies by attribute) |
| `status` | string | Health status of this attribute (see values below) |
| `critical` | boolean | Whether this attribute is considered critical for disk health |

### Attribute Status Values

| Value | Meaning |
|-------|---------|
| `"OK"` | Attribute is within acceptable range |
| `"FAILED"` | Attribute has failed (current ≤ threshold, or critical raw value detected) |
| `"UNKNOWN"` | Status could not be determined |

### Common Critical Attributes

| ID | Name | Critical? | Failure Condition |
|----|------|-----------|-------------------|
| 5 | `Reallocated_Sector_Ct` | Yes | raw_value > 0 |
| 196 | `Reallocated_Event_Count` | Yes | raw_value > 0 |
| 197 | `Current_Pending_Sector` | Yes | raw_value > 0 |
| 198 | `Offline_Uncorrectable` | Yes | raw_value > 0 |
| 194 | `Temperature_Celsius` | No | raw_value > 60°C (warning) |
| 9 | `Power_On_Hours` | No | Informational |

**Note**: Some drives don't provide manufacturer thresholds (threshold = 0). In these cases, the tool uses default health checks based on raw values for critical attributes.

## Complete Example

### Healthy Array

```json
{
  "version": "1.0",
  "device": "/dev/sdc",
  "timestamp": "2026-02-08T14:23:45Z",
  "raid_status": {
    "status": "healthy",
    "expected_disks": 4,
    "present_disks": 4,
    "rebuilding": false,
    "issues": []
  },
  "disks": [
    {
      "disk_number": 0,
      "name": "WDC WD80EFZZ-68BTXN0",
      "serial_number": "VGH123AB",
      "firmware_revision": "83.H0A83",
      "size_mb": 7630885,
      "status": "PASSED",
      "temperature_celsius": 35,
      "power_on_hours": 12543,
      "attributes": [
        {
          "id": 5,
          "name": "Reallocated_Sector_Ct",
          "current": 200,
          "worst": 200,
          "threshold": 140,
          "raw_value": 0,
          "status": "OK",
          "critical": true
        },
        {
          "id": 9,
          "name": "Power_On_Hours",
          "current": 100,
          "worst": 100,
          "threshold": 0,
          "raw_value": 12543,
          "status": "OK",
          "critical": false
        }
      ]
    }
  ]
}
```

### Degraded Array with Failed Disk

```json
{
  "version": "1.0",
  "device": "/dev/sdc",
  "timestamp": "2026-02-08T14:23:45Z",
  "raid_status": {
    "status": "degraded",
    "expected_disks": 5,
    "present_disks": 4,
    "rebuilding": false,
    "issues": [
      "Degraded: Expected 5 disks but found only 4 disks"
    ]
  },
  "disks": [
    {
      "disk_number": 0,
      "name": "ST8000DM004-2CX188",
      "status": "FAILED",
      "attributes": [
        {
          "id": 5,
          "name": "Reallocated_Sector_Ct",
          "current": 140,
          "worst": 140,
          "threshold": 140,
          "raw_value": 24,
          "status": "FAILED",
          "critical": true
        }
      ]
    }
  ]
}
```

## Exit Codes

The `jmraidstatus` command exits with codes that correspond to the JSON status:

| Exit Code | Meaning | Corresponds To |
|-----------|---------|----------------|
| 0 | Success - All healthy | `raid_status.status == "healthy"` and no failed disks |
| 1 | Warning/Failure | `raid_status.status == "degraded"` or `"failed"`, or any disk with `status == "FAILED"` |
| 3 | Error | Device not found, permission denied, communication error, etc. |

## Usage Examples

### Parse Status in Shell Script

```bash
#!/bin/bash
output=$(sudo jmraidstatus -j /dev/sdc)
exit_code=$?

if [ $exit_code -eq 0 ]; then
    echo "✓ RAID array healthy"
elif [ $exit_code -eq 1 ]; then
    # Parse issues from JSON
    echo "✗ RAID array has issues:"
    echo "$output" | jq -r '.raid_status.issues[]'
else
    echo "✗ Error querying RAID array"
fi
```

### Check Specific Disk Temperature

```bash
# Get temperature of disk 2
sudo jmraidstatus -j /dev/sdc | jq '.disks[] | select(.disk_number == 2) | .temperature_celsius'
```

### Monitor for Reallocated Sectors

```bash
# Alert if any disk has reallocated sectors
sudo jmraidstatus -j /dev/sdc | jq -r '
  .disks[] |
  select(.attributes[]? | select(.id == 5 and .raw_value > 0)) |
  "ALERT: Disk \(.disk_number) (\(.name)) has \(.attributes[] | select(.id == 5) | .raw_value) reallocated sectors"
'
```

### Integration with Monitoring Systems

```python
#!/usr/bin/env python3
import json
import subprocess
import sys

result = subprocess.run(
    ['sudo', 'jmraidstatus', '-j', '/dev/sdc'],
    capture_output=True,
    text=True
)

data = json.loads(result.stdout)

# Check RAID status
raid_status = data['raid_status']['status']
if raid_status != 'healthy':
    print(f"CRITICAL: RAID status is {raid_status}")
    for issue in data['raid_status']['issues']:
        print(f"  - {issue}")
    sys.exit(2)

# Check individual disks
for disk in data['disks']:
    if disk['status'] == 'FAILED':
        print(f"CRITICAL: Disk {disk['disk_number']} ({disk['name']}) has failed")
        sys.exit(2)

    # Check temperature
    if 'temperature_celsius' in disk:
        temp = disk['temperature_celsius']
        if temp > 55:
            print(f"WARNING: Disk {disk['disk_number']} temperature is {temp}°C")

print("OK: All disks healthy")
sys.exit(0)
```

## Version History

| API Version | Tool Version | Changes |
|-------------|--------------|---------|
| 1.0 | 1.0+ | Initial release with `raid_status` object |

## Notes

- **Field Presence**: Some fields (like `serial_number`, `firmware_revision`, `temperature_celsius`) may be absent if the hardware doesn't provide them or communication fails. Always check for field existence before accessing.
- **Array Size Validation**: The `expected_disks` and `present_disks` fields only appear when `--array-size` is specified on the command line.
- **Raw Values**: SMART attribute `raw_value` meanings vary by manufacturer. Common ones:
  - Temperature attributes: Lower byte = temperature in Celsius
  - Power On Hours: Raw value = hours (may use only lower 32 bits)
  - Sector counts: Raw value = number of sectors
- **Rebuilding Detection**: The `rebuilding` field is currently always `false`. Future versions may detect rebuild state from controller responses.
