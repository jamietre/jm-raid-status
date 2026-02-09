# Multi-Source SMART Monitoring - Implementation Plan

## Revised Architecture: Separate Binaries

### Overview

Transform the project from a JMicron-only tool into a multi-source SMART monitoring system using **separate binaries** rather than a monolithic plugin architecture.

```
┌─────────────────┐
│  disk-health    │  Main entry point (aggregator)
│  (new binary)   │  - Reads JSON config
└────────┬────────┘  - Spawns subprocesses
         │           - Aggregates results
         │
    ┌────┴────┬──────────┬──────────┐
    │         │          │          │
┌───▼──────┐ ┌▼────────┐ ┌▼───────┐ ┌▼──────┐
│jmraidstatus│ │smartctl │ │ future │ │ ...   │
│  (JMicron) │ │(existing)│ │ tools  │ │       │
└───┬────────┘ └┬────────┘ └────────┘ └───────┘
    │           │
    ▼           ▼
   JSON        JSON
```

## Components

### 1. `jmraidstatus` - JMicron Query Tool (existing, minor changes)

**Purpose**: Standalone tool to query JMicron RAID controllers

**Changes Needed**:
- Ensure JSON output is clean and parsable (no extra output to stdout)
- Enhance JSON schema to include all necessary metadata
- Keep current functionality otherwise

**Usage**:
```bash
jmraidstatus --json /dev/sdc
```

**Output** (JSON to stdout):
```json
{
  "version": "1.0",
  "backend": "jmicron",
  "device": "/dev/sdc",
  "controller": {
    "model": "JMB567",
    "type": "raid_array"
  },
  "raid_status": {
    "status": "healthy",
    "degraded": false,
    "present_disks": 4,
    "expected_disks": 4
  },
  "disks": [
    {
      "disk_number": 0,
      "model": "WDC WD40EFRX-68N32N0",
      "serial": "WD-WCC7K0123456",
      "firmware": "80.00A80",
      "size_mb": 3815447,
      "overall_status": "passed",
      "attributes": [...]
    }
  ]
}
```

### 2. `disk-health` - Aggregator (new binary)

**Purpose**: Main entry point that aggregates SMART data from multiple sources

**Functionality**:
- Read JSON config file specifying sources
- Spawn subprocesses for each source
- Parse JSON output from each tool
- Aggregate results into unified report
- Output summary or full JSON

**Config File** (`/etc/disk-health.json` or `~/.config/disk-health.json`):
```json
{
  "sources": [
    {
      "type": "jmicron",
      "device": "/dev/sdc",
      "tool": "jmraidstatus"
    },
    {
      "type": "smartctl",
      "device": "/dev/sda"
    },
    {
      "type": "smartctl",
      "device": "/dev/sdb"
    }
  ],
  "thresholds": {
    "temperature_critical": 55,
    "attributes": [
      {"id": 5, "raw_critical": 1},
      {"id": 187, "raw_critical": 1},
      {"id": 188, "raw_critical": 1},
      {"id": 197, "raw_critical": 1},
      {"id": 198, "raw_critical": 1}
    ]
  }
}
```

**Usage**:
```bash
# Summary view
disk-health

# Full JSON output
disk-health --json

# Use custom config
disk-health --config /path/to/config.json

# Query specific source only
disk-health --source jmicron:/dev/sdc
```

**Output** (aggregated):
```json
{
  "version": "2.0",
  "timestamp": "2026-02-09T12:00:00Z",
  "sources": [
    {
      "backend": "jmicron",
      "device": "/dev/sdc",
      "status": "success",
      "controller": {...},
      "raid_status": {...},
      "disks": [...]
    },
    {
      "backend": "smartctl",
      "device": "/dev/sda",
      "status": "success",
      "disks": [...]
    }
  ],
  "summary": {
    "total_disks": 5,
    "healthy_disks": 5,
    "failed_disks": 0,
    "overall_status": "passed"
  }
}
```

### 3. `smartctl` - External Tool (no changes)

**Purpose**: Query regular SATA/NVMe drives

**Usage** (called by aggregator):
```bash
smartctl --json=c --all /dev/sda
```

## Implementation Steps

### Phase 1: Update `jmraidstatus` JSON Output

**Goal**: Ensure JSON output is clean and includes all metadata

**Tasks**:
1. Review current JSON output format
2. Add missing fields:
   - `backend`: "jmicron"
   - `device`: device path
   - `controller`: {model, type}
   - Ensure `raid_status` is complete
3. Ensure NO extra output to stdout in JSON mode (only JSON)
4. Add `--json-only` flag (implies `--json` + `--quiet`)
5. Test JSON output is valid and parsable

**Files to Modify**:
- `src/output_formatter.c`: Enhance `format_json()`
- `src/jmraidstatus.c`: Add `--json-only` flag handling
- `docs/JSON_API.md`: Document updated schema

### Phase 2: Create Aggregator Binary

**Goal**: Create `disk-health` binary that aggregates multiple sources

**New Files**:
- `src/aggregator.c`: Main entry point for aggregator
- `src/subprocess.c`: Subprocess spawning and JSON parsing
- `src/aggregator_config.c`: Config file parsing (JSON)
- `src/aggregator_output.c`: Aggregated output formatting

**Data Structures**:
```c
/* Parsed source configuration */
typedef struct {
    char type[32];           // "jmicron", "smartctl"
    char device[256];        // "/dev/sdc"
    char tool[256];          // "jmraidstatus" (optional, auto-detected)
} source_config_t;

/* Result from subprocess */
typedef struct {
    source_config_t source;
    char* json_output;       // Raw JSON from subprocess
    void* parsed_data;       // Parsed JSON structure
    int exit_code;
    char error[256];
} source_result_t;

/* Aggregated report */
typedef struct {
    source_result_t* results;
    int num_results;
    int total_disks;
    int healthy_disks;
    int failed_disks;
    disk_health_status_t overall_status;
} aggregated_report_t;
```

**Functions**:
```c
/* Config parsing */
int config_load(const char* path, source_config_t** sources, int* num_sources);

/* Subprocess execution */
int subprocess_run(const char* command, char** output, int* exit_code);
int subprocess_query_source(const source_config_t* source, source_result_t* result);

/* JSON parsing (use existing jsmn or cJSON) */
int json_parse_jmicron(const char* json, void** parsed);
int json_parse_smartctl(const char* json, void** parsed);

/* Aggregation */
int aggregator_run(source_config_t* sources, int num_sources, aggregated_report_t* report);

/* Output */
void aggregator_format_summary(const aggregated_report_t* report);
void aggregator_format_json(const aggregated_report_t* report);
```

### Phase 3: Build System Updates

**Goal**: Build both binaries

**Makefile Changes**:
```makefile
# Two targets
TARGETS = $(BINDIR)/jmraidstatus $(BINDIR)/disk-health

# JMicron tool (existing sources)
JMICRON_SOURCES = src/jmraidstatus.c src/jm_protocol.c src/jm_commands.c \
                  src/smart_parser.c src/smart_attributes.c \
                  src/output_formatter.c src/jm_crc.c src/sata_xor.c \
                  src/config.c src/hardware_detect.c

# Aggregator tool (new sources)
AGGREGATOR_SOURCES = src/aggregator.c src/subprocess.c \
                     src/aggregator_config.c src/aggregator_output.c \
                     src/config.c  # Shared config utilities

# Add jsmn for JSON parsing
AGGREGATOR_CFLAGS = -I$(SRCDIR)/jsmn
```

### Phase 4: smartctl Integration

**Goal**: Parse smartctl JSON output

**smartctl Output** (reference):
```bash
smartctl --json=c --all /dev/sda
```

**Parser**:
- Parse `json_format_version`
- Extract `model_name`, `serial_number`, `firmware_version`
- Parse `ata_smart_attributes.table[]`
- Map to our `disk_smart_data_t` structure
- Apply threshold configuration

**Files**:
- `src/smartctl_parser.c`: Parse smartctl JSON to `disk_smart_data_t`
- `src/smartctl_parser.h`: Parser interface

### Phase 5: Documentation

**Goal**: Update all documentation for new architecture

**Files to Update**:
- `README.md`: Document both binaries
- `docs/JSON_API.md`: Document both JSON schemas
- `CLAUDE.md`: Update architecture notes
- Create `docs/AGGREGATOR.md`: Usage guide for disk-health

**New Files**:
- `docs/CONFIG_SCHEMA.md`: Document config file format
- `examples/disk-health.json`: Example config file

### Phase 6: Testing

**Goal**: Ensure both binaries work standalone and together

**Tests**:
1. **jmraidstatus**: Existing tests still pass
2. **JSON output**: Valid, parsable, complete
3. **Aggregator**: Parse config, spawn subprocesses, aggregate
4. **Integration**: End-to-end test with both tools

**Test Files**:
- `tests/test_aggregator.c`: Unit tests
- `tests/test_subprocess.c`: Subprocess execution tests
- `tests/test_smartctl_parser.c`: smartctl JSON parsing
- `tests/integration/test_end_to_end.sh`: Full integration test

## File Structure

```
src/
  # JMicron tool (jmraidstatus binary)
  jmraidstatus.c          - Main entry point
  jm_protocol.c           - JMicron protocol
  jm_commands.c           - JMicron commands
  hardware_detect.c       - Hardware detection

  # Aggregator (disk-health binary)
  aggregator.c            - Main entry point
  subprocess.c            - Subprocess execution
  aggregator_config.c     - Config parsing
  aggregator_output.c     - Aggregated output
  smartctl_parser.c       - Parse smartctl JSON

  # Shared utilities
  smart_parser.c          - SMART data parsing (shared)
  smart_attributes.c      - Attribute definitions (shared)
  output_formatter.c      - Output formatting (shared)
  config.c                - Config utilities (shared)

  # JMicron-specific (only used by jmraidstatus)
  jm_crc.c
  sata_xor.c

  # JSON parser (for aggregator)
  jsmn/jsmn.h             - Lightweight JSON parser

bin/
  jmraidstatus            - JMicron query tool
  disk-health             - Aggregator (main entry point)
```

## Dependencies

### Build Dependencies
- **jsmn**: Lightweight JSON parser (single header, MIT license)
  - Download: https://github.com/zserge/jsmn/blob/master/jsmn.h
  - Place in: `src/jsmn/jsmn.h`

### Runtime Dependencies
- **jmraidstatus**: None (self-contained)
- **disk-health**:
  - `jmraidstatus` (if querying JMicron controllers)
  - `smartctl` from smartmontools (if querying regular drives)

## Benefits of This Architecture

1. **Separation of Concerns**: Each binary does one thing well
2. **Reusability**: `jmraidstatus` can be used standalone
3. **Extensibility**: Add new tools by just calling them as subprocesses
4. **Testing**: Test each component independently
5. **Unix Philosophy**: Small tools composed together
6. **No Breaking Changes**: Existing `jmraidstatus` users unaffected
7. **Flexibility**: Can use `smartctl` directly or via aggregator

## Migration Path

**For Existing Users**:
- `jmraidstatus` continues to work exactly as before
- JSON output enhanced but backward compatible in structure
- No action required

**For New Multi-Source Monitoring**:
1. Install `disk-health` binary
2. Create config file: `/etc/disk-health.json`
3. Run: `disk-health` for summary or `disk-health --json` for full output

## Future Extensions

This architecture easily supports:
- **Additional Tools**: MegaRAID (storcli), Adaptec (arcconf), NVMe (nvme-cli)
- **Remote Monitoring**: SSH to remote hosts, call tools remotely
- **Caching**: Cache results, periodic monitoring
- **Alerting**: Email/webhook notifications
- **Web UI**: Dashboard reading JSON output

## Implementation Order

1. ✅ Phase 1: Update `jmraidstatus` JSON output (1-2 hours)
2. ✅ Phase 2: Create aggregator binary skeleton (2-3 hours)
3. ✅ Phase 3: Build system updates (1 hour)
4. ✅ Phase 4: smartctl integration (2-3 hours)
5. ✅ Phase 5: Documentation (2 hours)
6. ✅ Phase 6: Testing (2-3 hours)

**Total Estimate**: 10-14 hours

## Notes

- Remove backend abstraction files created earlier (not needed with separate binaries)
- Keep existing JMicron code unchanged (except JSON output enhancements)
- Aggregator is a new, independent binary
- Both binaries can be installed and used independently
