# Multi-Source SMART Monitoring - Implementation Plan

## Architecture: Pipe-Based with Bash Composition

### Overview

Transform the project into a multi-source SMART monitoring system using **Unix pipes and bash command grouping** - no config files or orchestrators needed.

```
┌─────────────┐                    ┌─────────────┐
│ jmraidstatus│───────────────────▶│             │
│ --json-only │  (already correct  │             │
└─────────────┘     format)        │             │
                                   │             │
┌─────────────┐  ┌──────────────┐  │             │
│  smartctl   │─▶│ smartctl-    │─▶│ disk-health │──▶ Report
│  --json=c   │  │ parser       │  │             │
└─────────────┘  └──────────────┘  │  (stdin)    │
                                   │             │
┌─────────────┐  ┌──────────────┐  │             │
│   storcli   │─▶│ storcli-     │─▶│             │
│     /cX     │  │ parser       │  │             │
└─────────────┘  └──────────────┘  └─────────────┘

                        |
                        ▼
                  Combined via bash:
                  {
                    jmraidstatus --json-only /dev/sdc
                    smartctl --json=c /dev/sda | smartctl-parser
                  } | disk-health
```

## Standard Format (disk-health JSON)

All parsers convert to this format (which `jmraidstatus` already emits):

```json
{
  "version": "1.0",
  "backend": "jmicron|smartctl|storcli|...",
  "device": "/dev/sdc",
  "timestamp": "2026-02-09T12:00:00Z",
  "controller": {
    "model": "JMB567",
    "type": "raid_array|single_disk"
  },
  "raid_status": {
    "status": "healthy|degraded|failed",
    "degraded": false,
    "present_disks": 4,
    "expected_disks": 4,
    "rebuilding": false,
    "issues": []
  },
  "disks": [
    {
      "disk_number": 0,
      "model": "WDC WD40EFRX-68N32N0",
      "serial": "WD-WCC7K0123456",
      "firmware": "80.00A80",
      "size_mb": 3815447,
      "overall_status": "passed|failed",
      "attributes": [...]
    }
  ]
}
```

## Input Format: NDJSON (Newline-Delimited JSON)

`disk-health` reads from stdin, expecting one compact JSON object per line:

```
{"version":"1.0","backend":"jmicron",...}\n
{"version":"1.0","backend":"smartctl",...}\n
{"version":"1.0","backend":"smartctl",...}\n
```

Each source outputs one line of compact JSON.

## Components

### 1. Source Tools (emit native format)

**jmraidstatus** (already done ✅):
```bash
jmraidstatus --json-only /dev/sdc
```
Outputs: disk-health JSON format (one compact line)

**smartctl** (external tool):
```bash
smartctl --json=c --all /dev/sda
```
Outputs: smartctl's JSON format

**storcli** (future):
```bash
storcli /c0 show all J
```
Outputs: MegaRAID JSON format

### 2. Parser Binaries (convert to disk-health format)

Each parser is a simple stdin→stdout filter:

**smartctl-parser**:
```bash
smartctl --json=c /dev/sda | smartctl-parser
```
- Reads: smartctl JSON from stdin
- Outputs: disk-health JSON format (one compact line)

**storcli-parser** (future):
```bash
storcli /c0 show all J | storcli-parser
```
- Reads: storcli JSON from stdin
- Outputs: disk-health JSON format (one compact line)

**nvme-parser** (future):
```bash
nvme smart-log /dev/nvme0 -o json | nvme-parser
```
- Reads: nvme-cli JSON from stdin
- Outputs: disk-health JSON format (one compact line)

### 3. Aggregator (disk-health)

Reads NDJSON from stdin, aggregates, and outputs report:

```bash
{ cmd1; cmd2; cmd3; } | disk-health [OPTIONS]
```

**Options**:
- `--json` - Output aggregated JSON
- `--summary` - Output text summary (default)
- `--quiet` - Exit code only
- `--verbose` - Detailed output

**Output** (summary mode):
```
Disk Health Report - 2026-02-09 12:00:00

Sources: 3
  ✓ jmicron /dev/sdc (4 disks)
  ✓ smartctl /dev/sda (1 disk)
  ✓ smartctl /dev/sdb (1 disk)

Overall Status: PASSED
  Total Disks: 6
  Healthy: 6
  Failed: 0

Exit Code: 0 (all healthy)
```

**Output** (JSON mode):
```json
{
  "version": "2.0",
  "timestamp": "2026-02-09T12:00:00Z",
  "sources": [
    {"backend": "jmicron", "device": "/dev/sdc", ...},
    {"backend": "smartctl", "device": "/dev/sda", ...},
    {"backend": "smartctl", "device": "/dev/sdb", ...}
  ],
  "summary": {
    "total_disks": 6,
    "healthy_disks": 6,
    "failed_disks": 0,
    "overall_status": "passed"
  }
}
```

## Usage Examples

### Single Source

```bash
jmraidstatus --json-only /dev/sdc | disk-health
```

### Multiple Sources (Bash Grouping)

**Option A: Curly braces**
```bash
{
  jmraidstatus --json-only /dev/sdc
  smartctl --json=c /dev/sda | smartctl-parser
  smartctl --json=c /dev/sdb | smartctl-parser
} | disk-health
```

**Option B: Subshell**
```bash
(
  jmraidstatus --json-only /dev/sdc
  smartctl --json=c /dev/sda | smartctl-parser
  smartctl --json=c /dev/sdb | smartctl-parser
) | disk-health
```

Both work identically - choose whichever you prefer.

### Wrapper Script for Convenience

Create `~/check-all-disks.sh`:
```bash
#!/bin/bash
# Check all my disks and aggregate results

{
  # RAID array via JMicron
  jmraidstatus --json-only /dev/sdc

  # Regular SATA drives
  smartctl --json=c --all /dev/sda | smartctl-parser
  smartctl --json=c --all /dev/sdb | smartctl-parser

  # NVMe drive (future)
  # nvme smart-log /dev/nvme0 -o json | nvme-parser
} | disk-health "$@"
```

**Usage**:
```bash
sudo ~/check-all-disks.sh              # Summary
sudo ~/check-all-disks.sh --json       # Full JSON
sudo ~/check-all-disks.sh --quiet      # Exit code only
```

**Add to cron**:
```cron
# Check disks daily at 2 AM
0 2 * * * /root/check-all-disks.sh --json > /var/log/disk-health.json
```

### Parallel Execution (Advanced)

Run queries in parallel for faster results:

```bash
{
  jmraidstatus --json-only /dev/sdc &
  smartctl --json=c /dev/sda | smartctl-parser &
  smartctl --json=c /dev/sdb | smartctl-parser &
  wait
} | disk-health
```

## Implementation Plan

### Phase 1: Update jmraidstatus JSON Output ✅ DONE

**Status**: COMPLETED

**Changes Made**:
- Added `"backend": "jmicron"` field
- Added `"controller": {model, type}` object
- Added `--json-only` flag (implies `--json` + `--quiet`)
- Ensured clean output with no extra messages
- Refactored hardware detection into separate module

### Phase 2: Create smartctl-parser Binary

**Goal**: Convert smartctl JSON to disk-health JSON format

**New Files**:
- `src/parsers/smartctl_parser.c` - Main parser binary
- `src/parsers/smartctl_parser.h` - Parser interface
- `src/parsers/common.c` - Shared JSON utilities (jsmn-based)
- `src/parsers/common.h` - Common parser utilities

**Implementation**:

```c
/* src/parsers/smartctl_parser.c */

int main(void) {
    // Read all stdin
    char* input = read_all_stdin();

    // Parse smartctl JSON
    smartctl_data_t data;
    if (parse_smartctl_json(input, &data) != 0) {
        fprintf(stderr, "Error: Failed to parse smartctl JSON\n");
        return 1;
    }

    // Convert to disk-health format
    disk_health_json_t output;
    convert_smartctl_to_disk_health(&data, &output);

    // Output compact JSON (one line)
    print_compact_json(&output);
    printf("\n");

    return 0;
}
```

**Key Functions**:
- `parse_smartctl_json()` - Parse smartctl's JSON schema
- `convert_smartctl_to_disk_health()` - Map fields to our format
- `print_compact_json()` - Output single-line JSON

**smartctl JSON Fields to Extract**:
```json
{
  "model_name": "...",
  "serial_number": "...",
  "firmware_version": "...",
  "user_capacity": {...},
  "ata_smart_attributes": {
    "table": [
      {"id": 5, "name": "...", "value": ..., "worst": ..., "thresh": ..., "raw": {...}},
      ...
    ]
  },
  "temperature": {...}
}
```

**Output** (disk-health format):
```json
{"version":"1.0","backend":"smartctl","device":"/dev/sda","timestamp":"...","controller":{"model":"N/A","type":"single_disk"},"raid_status":null,"disks":[{...}]}
```

### Phase 3: Create disk-health Aggregator Binary

**Goal**: Read NDJSON from stdin, aggregate, and output report

**New Files**:
- `src/aggregator/disk_health.c` - Main entry point
- `src/aggregator/ndjson_reader.c` - Read line-delimited JSON
- `src/aggregator/aggregator.c` - Aggregation logic
- `src/aggregator/output.c` - Summary and JSON output

**Data Structures**:

```c
/* Source result (parsed from one line of input) */
typedef struct {
    char backend[32];           // "jmicron", "smartctl", etc.
    char device[256];           // "/dev/sdc"
    char controller_model[64];  // "JMB567" or "N/A"
    char controller_type[32];   // "raid_array" or "single_disk"

    disk_smart_data_t disks[32];  // All disks from this source
    int num_disks;

    raid_status_t raid_status;  // If applicable
    disk_health_status_t overall_status;
} source_result_t;

/* Aggregated report */
typedef struct {
    source_result_t sources[MAX_SOURCES];
    int num_sources;

    int total_disks;
    int healthy_disks;
    int failed_disks;
    disk_health_status_t overall_status;

    char timestamp[64];
} aggregated_report_t;
```

**Implementation**:

```c
/* src/aggregator/disk_health.c */

int main(int argc, char** argv) {
    cli_options_t options;
    parse_arguments(argc, argv, &options);

    // Read NDJSON from stdin (one JSON object per line)
    source_result_t sources[MAX_SOURCES];
    int num_sources = 0;

    char line[MAX_LINE_SIZE];
    while (fgets(line, sizeof(line), stdin)) {
        if (parse_disk_health_json(line, &sources[num_sources]) == 0) {
            num_sources++;
        } else {
            fprintf(stderr, "Warning: Failed to parse JSON line\n");
        }
    }

    if (num_sources == 0) {
        fprintf(stderr, "Error: No valid sources found on stdin\n");
        return 3;
    }

    // Aggregate results
    aggregated_report_t report;
    aggregate_sources(sources, num_sources, &report);

    // Output based on mode
    if (options.output_json) {
        output_aggregated_json(&report);
    } else {
        output_summary(&report);
    }

    // Exit code based on health
    return determine_exit_code(&report);
}
```

**Key Functions**:
- `parse_disk_health_json()` - Parse one line of disk-health JSON
- `aggregate_sources()` - Combine all sources into unified report
- `output_summary()` - Text summary output
- `output_aggregated_json()` - JSON output
- `determine_exit_code()` - 0=healthy, 1=failed, 3=error

### Phase 4: Build System Updates

**Makefile Changes**:

```makefile
# Three main targets
TARGETS = $(BINDIR)/jmraidstatus \
          $(BINDIR)/smartctl-parser \
          $(BINDIR)/disk-health

# JMicron tool sources (existing)
JMICRON_SOURCES = src/jmraidstatus.c src/jm_protocol.c src/jm_commands.c \
                  src/smart_parser.c src/smart_attributes.c \
                  src/output_formatter.c src/jm_crc.c src/sata_xor.c \
                  src/config.c src/hardware_detect.c

JMICRON_OBJECTS = $(patsubst src/%.c,$(OBJDIR)/%.o,$(JMICRON_SOURCES))

# smartctl-parser sources (new)
SMARTCTL_PARSER_SOURCES = src/parsers/smartctl_parser.c \
                          src/parsers/common.c \
                          src/smart_parser.c \
                          src/smart_attributes.c

SMARTCTL_PARSER_OBJECTS = $(patsubst src/%.c,$(OBJDIR)/%.o,$(SMARTCTL_PARSER_SOURCES))

# disk-health aggregator sources (new)
AGGREGATOR_SOURCES = src/aggregator/disk_health.c \
                     src/aggregator/ndjson_reader.c \
                     src/aggregator/aggregator.c \
                     src/aggregator/output.c \
                     src/smart_parser.c \
                     src/smart_attributes.c

AGGREGATOR_OBJECTS = $(patsubst src/%.c,$(OBJDIR)/%.o,$(AGGREGATOR_SOURCES))

# Compiler flags (add jsmn include path)
CFLAGS = -g -O2 -Wall -Wextra -std=gnu99 -Isrc/jsmn

all: $(TARGETS)

$(BINDIR)/jmraidstatus: $(JMICRON_OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) $(JMICRON_OBJECTS) -o $@
	@echo "Built: $@"

$(BINDIR)/smartctl-parser: $(SMARTCTL_PARSER_OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) $(SMARTCTL_PARSER_OBJECTS) -o $@
	@echo "Built: $@"

$(BINDIR)/disk-health: $(AGGREGATOR_OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) $(AGGREGATOR_OBJECTS) -o $@
	@echo "Built: $@"

# Object file compilation
$(OBJDIR)/%.o: src/%.c | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/parsers/%.o: src/parsers/%.c | $(OBJDIR)
	@mkdir -p $(OBJDIR)/parsers
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/aggregator/%.o: src/aggregator/%.c | $(OBJDIR)
	@mkdir -p $(OBJDIR)/aggregator
	$(CC) $(CFLAGS) -c $< -o $@
```

### Phase 5: Documentation

**Files to Create/Update**:

1. **docs/AGGREGATOR.md** - Usage guide for disk-health
   - How to use bash grouping
   - Example wrapper scripts
   - Output format documentation

2. **examples/single-source.sh** - Simple example
3. **examples/mixed-sources.sh** - RAID + regular drives
4. **examples/parallel.sh** - Parallel execution
5. **examples/cron-monitoring.sh** - Scheduled monitoring

6. **docs/JSON_API.md** - Update with:
   - disk-health standard JSON format
   - Aggregated output format
   - Parser requirements

7. **README.md** - Update with:
   - Overview of multi-source monitoring
   - Quick start examples
   - Link to AGGREGATOR.md

### Phase 6: Testing

**Test Files**:

1. **tests/test_smartctl_parser.c** - Unit tests for smartctl parsing
   - Test with real smartctl JSON samples
   - Verify output format matches disk-health schema

2. **tests/test_aggregator.c** - Unit tests for aggregation logic
   - Test with multiple sources
   - Test with failed disks
   - Test exit code logic

3. **tests/integration/** - End-to-end tests
   - `test_single_source.sh` - One source through pipe
   - `test_multi_source.sh` - Multiple sources with grouping
   - `test_invalid_input.sh` - Error handling

4. **tests/data/** - Test JSON samples
   - `smartctl-healthy.json` - Sample smartctl output
   - `smartctl-failed.json` - Failed disk sample
   - `jmicron-healthy.json` - JMicron output sample

## File Structure

```
bin/
  jmraidstatus          # JMicron SMART query tool
  smartctl-parser       # smartctl JSON → disk-health JSON
  disk-health           # Aggregator (reads stdin)

src/
  # JMicron tool (Phase 1 - DONE)
  jmraidstatus.c
  jm_protocol.c
  jm_commands.c
  hardware_detect.c
  smart_parser.c        # Shared by all components
  smart_attributes.c    # Shared by all components
  output_formatter.c
  config.c
  jm_crc.c
  sata_xor.c

  # Parsers (Phase 2)
  parsers/
    smartctl_parser.c   # smartctl converter
    common.c            # Shared parser utilities
    common.h
    # Future: storcli_parser.c, nvme_parser.c

  # Aggregator (Phase 3)
  aggregator/
    disk_health.c       # Main entry point
    ndjson_reader.c     # Read line-delimited JSON
    aggregator.c        # Aggregation logic
    output.c            # Summary/JSON output

  # JSON parser library
  jsmn/
    jsmn.h              # Lightweight JSON parser (single header)

examples/
  single-source.sh      # Example: one RAID array
  mixed-sources.sh      # Example: RAID + regular drives
  parallel.sh           # Example: parallel queries
  cron-monitoring.sh    # Example: scheduled monitoring

docs/
  AGGREGATOR.md         # disk-health usage guide
  JSON_API.md           # JSON format documentation
  PARSERS.md            # Guide for writing new parsers

tests/
  test_smartctl_parser.c
  test_aggregator.c
  integration/
    test_single_source.sh
    test_multi_source.sh
  data/
    smartctl-healthy.json
    jmicron-healthy.json
```

## Dependencies

### Build Dependencies

- **jsmn**: Lightweight JSON parser (MIT license)
  - Single header file: `jsmn.h`
  - Download: https://github.com/zserge/jsmn
  - ~500 lines, zero dependencies
  - Place in: `src/jsmn/jsmn.h`

### Runtime Dependencies

**For jmraidstatus**:
- None (self-contained)

**For smartctl-parser**:
- None (self-contained)
- Input comes from smartctl (user must install smartmontools)

**For disk-health**:
- None (self-contained)
- Requires compatible JSON input from parsers

## Benefits of This Architecture

1. ✅ **No Config Files** - Just bash scripts (which users already know)
2. ✅ **Total Decoupling** - Each component is independent
3. ✅ **Composability** - Use any bash syntax to combine sources
4. ✅ **Extensibility** - Add new parsers without touching disk-health
5. ✅ **Testability** - Test each component with static files
6. ✅ **Debuggability** - Run each command separately to troubleshoot
7. ✅ **Unix Philosophy** - Small tools doing one thing well
8. ✅ **Flexibility** - Users can customize exactly how they want

## Example Workflows

### Daily Monitoring with Cron

`/root/check-disks.sh`:
```bash
#!/bin/bash
LOG=/var/log/disk-health.json
ERROR_LOG=/var/log/disk-health-errors.log

{
  jmraidstatus --json-only /dev/sdc 2>>"$ERROR_LOG"
  smartctl --json=c /dev/sda 2>>"$ERROR_LOG" | smartctl-parser
  smartctl --json=c /dev/sdb 2>>"$ERROR_LOG" | smartctl-parser
} | disk-health --json > "$LOG"

# Check exit code and alert if failed
if [ $? -ne 0 ]; then
  echo "DISK HEALTH ALERT: Issues detected" | mail -s "Disk Health Alert" admin@example.com
fi
```

Add to crontab:
```
0 2 * * * /root/check-disks.sh
```

### Manual Health Check

```bash
# Quick check (summary)
{
  jmraidstatus --json-only /dev/sdc
  smartctl --json=c /dev/sda | smartctl-parser
} | disk-health

# Full details (JSON)
{
  jmraidstatus --json-only /dev/sdc
  smartctl --json=c /dev/sda | smartctl-parser
} | disk-health --json | jq .
```

### Remote Monitoring

```bash
# Monitor remote server via SSH
ssh root@server '{
  jmraidstatus --json-only /dev/sdc
  smartctl --json=c /dev/sda | smartctl-parser
}' | disk-health
```

## Implementation Timeline

- ✅ **Phase 1**: Update jmraidstatus JSON output (COMPLETED)
- ⏳ **Phase 2**: Create smartctl-parser (2-3 hours)
- ⏳ **Phase 3**: Create disk-health aggregator (3-4 hours)
- ⏳ **Phase 4**: Build system updates (1 hour)
- ⏳ **Phase 5**: Documentation (2 hours)
- ⏳ **Phase 6**: Testing (2-3 hours)

**Total Estimate**: 10-13 hours remaining

## Future Extensions

This architecture makes it trivial to add:

- **Additional parsers**: storcli, nvme-cli, arcconf, etc.
- **Custom formatters**: HTML, Prometheus metrics, InfluxDB line protocol
- **Alerting**: Wrapper scripts with email/webhook notifications
- **Web dashboard**: Read JSON output and display in browser
- **Historical tracking**: Store JSON outputs, trend analysis
