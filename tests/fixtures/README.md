# Test Fixtures

Real protocol responses extracted from investigation captures.

## Structure:

- **degraded/identify_disk0.bin** - IDENTIFY response when disk was removed (0x1F0=0x07)
- **healthy/identify_disk0.bin** - IDENTIFY response with all disks healthy (0x1F0=0x0F, 0x1F5=0x00)
- **rebuilding/identify_disk0.bin** - IDENTIFY response during rebuild (0x1F0=0x0F, 0x1F5=0x01)

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
