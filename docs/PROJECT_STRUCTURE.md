# Project Structure

This document explains the organization of the jm-raid-status codebase.

## Directory Layout

```
jm-raid-status/
├── src/                    # Source code
│   ├── jmraidstatus.c      # Main program
│   ├── jm_protocol.[ch]    # Low-level JMicron protocol
│   ├── jm_commands.[ch]    # High-level commands (SMART queries)
│   ├── jm_crc.[ch]         # CRC calculation
│   ├── sata_xor.[ch]       # XOR scrambling
│   ├── smart_parser.[ch]   # SMART data parsing
│   ├── smart_attributes.[ch] # SMART attribute definitions
│   └── output_formatter.[ch] # Display formatting
│
├── tests/                  # Unit tests
│   ├── test_framework.h    # Lightweight test framework
│   ├── test_*.c            # Unit test files
│   ├── fixtures/           # Test data files
│   └── README.md           # Testing documentation
│
├── tools/                  # Utility programs
│   ├── check_sectors       # Sector verification tool
│   └── monitor/            # RAID monitoring daemon
│       ├── monitor.sh      # Control script
│       ├── raid_monitor.py # Main monitoring logic
│       ├── email_notifier.py # Email alerts
│       └── ...
│
├── scripts/                # Analysis and testing scripts
│   ├── analyze_captures.sh # Protocol capture analysis
│   ├── test_signal_handling.sh # Integration test
│   └── ...                 # Ad-hoc development scripts
│
├── investigation/          # Research data and findings
│   └── data/               # Protocol captures
│       ├── degraded_state.txt
│       ├── rebuilding_state_*.txt
│       └── investigation.md # Research findings
│
├── docs/                   # Documentation (future)
│
├── bin/                    # Build output (gitignored)
│   ├── jmraidstatus        # Main binary
│   └── tests/              # Test binaries
│
├── Makefile                # Build system
├── README.md               # Project overview
├── PROTOCOL.md             # Protocol documentation
├── SECTOR_USAGE.md         # Sector safety documentation
├── CLAUDE.md               # Development guide
└── PROJECT_STRUCTURE.md    # This file
```

## Purpose of Each Directory

### src/
Source code for the main jmraidstatus tool. All `.c` and `.h` files for production code.

**Key files:**
- `jmraidstatus.c` - Entry point, command-line parsing, main logic
- `jm_protocol.c` - Sector communication, signal handling
- `jm_commands.c` - IDENTIFY, SMART VALUE commands
- `smart_parser.c` - Parse SMART data into readable format

### tests/
**Unit tests only.** Uses test_framework.h for assertions.

- Test files: `test_<module>.c`
- Fixtures: Sample data for tests
- Built automatically by `make tests`

### tools/
**Utility programs** that use or extend jmraidstatus functionality.

- `check_sectors` - Find safe sectors for communication
- `monitor/` - Automated RAID monitoring with email alerts

### scripts/
**Development scripts** - analysis, integration tests, ad-hoc tools.

- Not compiled (shell scripts, Python, etc.)
- Integration tests that require hardware
- Development/debugging utilities

### investigation/
**Research data** from protocol discovery and testing.

- Captured protocol responses
- Analysis findings
- Not needed for normal tool operation
- Useful for understanding protocol behavior

## Build System

### Make Targets

```bash
make              # Build main program
make tests        # Build and run unit tests  
make all tests    # Build everything and test
make clean        # Remove all build artifacts
make install      # Install to /usr/local/bin
```

### Adding New Components

**New source file:**
1. Add to `src/`
2. Add to `SOURCES` in Makefile
3. Include header in relevant files

**New unit test:**
1. Create `tests/test_<module>.c`
2. Include `test_framework.h`
3. Run `make tests` (auto-discovered)

**New tool:**
1. Add to `tools/` directory
2. Create README in tool directory
3. Optionally: Add make target if needed

**New script:**
1. Add to `scripts/` directory
2. Make executable: `chmod +x script.sh`
3. Document usage in script header

## Dependencies

**Build dependencies:**
- GCC or compatible C compiler
- GNU Make
- Linux kernel headers (for SCSI ioctl)

**Runtime dependencies:**
- Linux kernel with SCSI generic (sg) support
- Sudo access for device operations
- Python 3 (for monitoring tools)

**Optional:**
- cmocka (for more advanced testing)
- Python email libraries (for monitoring)

## Conventions

### Code Style
- K&R style bracing
- 4-space indentation
- Snake_case for functions and variables
- UPPER_CASE for macros/constants

### File Naming
- Source: `module_name.c` / `module_name.h`
- Tests: `test_module.c`
- Scripts: `action_description.sh`
- Tools: lowercase, no extension

### Git Workflow
1. Make changes
2. Run `make clean && make && make tests`
3. Ensure all tests pass
4. Commit with descriptive message
5. Include Co-Authored-By for AI assistance

## Future Structure

Planned additions:
- `docs/` - Extended documentation (man pages, guides)
- `examples/` - Example programs using the library
- CI/CD workflows (GitHub Actions)
- Debian packaging (`debian/` directory)

---

*Last updated: 2026-02-07*
