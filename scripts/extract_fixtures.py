#!/usr/bin/env python3
"""Extract protocol responses from investigation captures to create test fixtures."""

import re
import struct
from pathlib import Path

INVESTIGATION_DIR = Path("investigation/data")
FIXTURES_DIR = Path("tests/fixtures")

def extract_identify_response(source_file, disk_num):
    """Extract 512-byte IDENTIFY response for a specific disk."""
    with open(source_file, 'r') as f:
        content = f.read()
    
    # Find the IDENTIFY section for this disk
    pattern = rf"=== IDENTIFY DISK {disk_num} RESPONSE.*?\n((?:[0-9a-f]{{4}}:.*?\n)+)"
    match = re.search(pattern, content, re.MULTILINE)
    
    if not match:
        return None
    
    # Parse hex dump lines
    hex_lines = match.group(1)
    bytes_data = bytearray()
    
    for line in hex_lines.split('\n'):
        if not line.strip():
            continue
        # Parse: "01f0: 0f 00 2f 00 01 01 00 00 00 80 00 00 dc d4 80 34"
        parts = line.split(':')
        if len(parts) != 2:
            continue
        hex_bytes = parts[1].split()[:16]  # Take first 16 hex values (ignore ASCII)
        for hex_byte in hex_bytes:
            if len(hex_byte) == 2:
                bytes_data.append(int(hex_byte, 16))
    
    return bytes(bytes_data) if len(bytes_data) == 512 else None

def extract_flags(response):
    """Extract RAID flags from IDENTIFY response."""
    if len(response) < 0x1FB:
        return None
    
    return {
        '0x1F0': response[0x1F0],  # Health status
        '0x1F5': response[0x1F5],  # Rebuild status
        '0x1FA': response[0x1FA],  # Rebuild phase
    }

def main():
    print("Extracting test fixtures from investigation data...\n")
    
    # Create fixture directories
    (FIXTURES_DIR / "degraded").mkdir(parents=True, exist_ok=True)
    (FIXTURES_DIR / "healthy").mkdir(parents=True, exist_ok=True)
    (FIXTURES_DIR / "rebuilding").mkdir(parents=True, exist_ok=True)
    
    fixtures_created = 0
    
    # Extract DEGRADED state (only disk 0 - representative sample)
    print("=== Extracting DEGRADED state ===")
    degraded_file = INVESTIGATION_DIR / "degraded_state.txt"
    if degraded_file.exists():
        response = extract_identify_response(degraded_file, 0)
        if response:
            output = FIXTURES_DIR / "degraded" / "identify_disk0.bin"
            output.write_bytes(response)
            flags = extract_flags(response)
            print(f"  ✓ Disk 0: {len(response)} bytes, flags: 0x1F0={flags['0x1F0']:02x}")
            fixtures_created += 1
    
    # Extract HEALTHY state (only disk 0 - representative sample)
    print("\n=== Extracting HEALTHY state ===")
    healthy_file = INVESTIGATION_DIR / "healthy_state.txt"
    if healthy_file.exists():
        response = extract_identify_response(healthy_file, 0)
        if response:
            output = FIXTURES_DIR / "healthy" / "identify_disk0.bin"
            output.write_bytes(response)
            flags = extract_flags(response)
            print(f"  ✓ Disk 0: {len(response)} bytes, flags: 0x1F0={flags['0x1F0']:02x}, 0x1F5={flags['0x1F5']:02x}")
            fixtures_created += 1
    
    # Extract REBUILDING state (only disk 0 - representative sample)
    print("\n=== Extracting REBUILDING state ===")
    rebuilding_file = INVESTIGATION_DIR / "rebuilding_state_1.txt"
    if rebuilding_file.exists():
        response = extract_identify_response(rebuilding_file, 0)
        if response:
            output = FIXTURES_DIR / "rebuilding" / "identify_disk0.bin"
            output.write_bytes(response)
            flags = extract_flags(response)
            print(f"  ✓ Disk 0: {len(response)} bytes, flags: 0x1F0={flags['0x1F0']:02x}, 0x1F5={flags['0x1F5']:02x}, 0x1FA={flags['0x1FA']:02x}")
            fixtures_created += 1
    
    print(f"\n✅ Created {fixtures_created} fixture files in {FIXTURES_DIR}/")
    
    # Create README
    readme = FIXTURES_DIR / "README.md"
    readme.write_text("""# Test Fixtures

Real protocol responses extracted from investigation captures.

## Structure:

- **degraded/** - IDENTIFY responses when disk 3 was removed (0x1F0=0x07)
- **healthy/** - IDENTIFY responses after disk reinsertion (0x1F0=0x0F, 0x1F5=0x00)  
- **rebuilding/** - IDENTIFY responses during rebuild (0x1F0=0x0F, 0x1F5=0x01)

## Each fixture:

- Binary file, 512 bytes
- Raw IDENTIFY DEVICE response from JMicron controller
- Contains: disk info, SMART data preview, RAID flags at 0x1F0+

## Usage in tests:

```c
FILE* f = fopen("tests/fixtures/degraded/identify_disk0.bin", "rb");
uint8_t response[512];
fread(response, 1, 512, f);
fclose(f);

// Test flag detection
assert(response[0x1F0] == 0x07);  // Degraded
```

## Regenerate fixtures:

```bash
python3 scripts/extract_fixtures.py
```
""")

if __name__ == "__main__":
    main()
