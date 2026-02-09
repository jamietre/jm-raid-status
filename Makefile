CC = gcc
CFLAGS = -g -O2 -Wall -Wextra -std=gnu99 -I$(SRCDIR)/jsmn
SRCDIR = src
BINDIR = bin
OBJDIR = $(BINDIR)/obj
TESTDIR = tests
TESTBINDIR = $(BINDIR)/tests

# JMicron tool sources
JMICRON_SOURCES = $(SRCDIR)/jmraidstatus.c \
                  $(SRCDIR)/jm_protocol.c \
                  $(SRCDIR)/jm_commands.c \
                  $(SRCDIR)/smart_parser.c \
                  $(SRCDIR)/smart_attributes.c \
                  $(SRCDIR)/output_formatter.c \
                  $(SRCDIR)/jm_crc.c \
                  $(SRCDIR)/sata_xor.c \
                  $(SRCDIR)/config.c \
                  $(SRCDIR)/hardware_detect.c

JMICRON_OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(JMICRON_SOURCES))

# smartctl-parser sources
SMARTCTL_PARSER_SOURCES = $(SRCDIR)/parsers/smartctl_parser.c \
                          $(SRCDIR)/parsers/common.c \
                          $(SRCDIR)/smart_attributes.c

SMARTCTL_PARSER_OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SMARTCTL_PARSER_SOURCES))

# disk-health aggregator sources
DISK_HEALTH_SOURCES = $(SRCDIR)/aggregator/disk_health.c \
                      $(SRCDIR)/parsers/common.c \
                      $(SRCDIR)/smart_parser.c \
                      $(SRCDIR)/smart_attributes.c

DISK_HEALTH_OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(DISK_HEALTH_SOURCES))

# All targets
TARGETS = $(BINDIR)/jmraidstatus $(BINDIR)/smartctl-parser $(BINDIR)/disk-health

.DEFAULT_GOAL := all

# Help target
help:
	@echo "jm-raid-status Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  make              - Build all binaries (default)"
	@echo "  make clean        - Remove all build artifacts"
	@echo "  make test         - Run integration tests"
	@echo "  make install      - Install binaries to /usr/local/bin"
	@echo "  make tools        - Build utility tools"
	@echo ""
	@echo "Binaries built:"
	@echo "  bin/jmraidstatus   - JMicron RAID SMART query tool"
	@echo "  bin/smartctl-parser - Convert smartctl JSON to disk-health format"
	@echo "  bin/disk-health     - Multi-source SMART aggregator"
	@echo ""
	@echo "Testing:"
	@echo "  make test          - Run all integration tests"
	@echo "  make tests         - Run unit tests (if available)"
	@echo ""

all: $(TARGETS)

$(BINDIR)/jmraidstatus: $(JMICRON_OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) $(JMICRON_OBJECTS) -o $@
	@echo "Built: $@"

$(BINDIR)/smartctl-parser: $(SMARTCTL_PARSER_OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) $(SMARTCTL_PARSER_OBJECTS) -o $@
	@echo "Built: $@"

$(BINDIR)/disk-health: $(DISK_HEALTH_OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) $(DISK_HEALTH_OBJECTS) -o $@
	@echo "Built: $@"

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR):
	@mkdir -p $(BINDIR)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

clean:
	-rm -rf $(BINDIR)

install: $(TARGETS)
	install -D -m 755 $(BINDIR)/jmraidstatus $(DESTDIR)/usr/local/bin/jmraidstatus
	install -D -m 755 $(BINDIR)/smartctl-parser $(DESTDIR)/usr/local/bin/smartctl-parser
	install -D -m 755 $(BINDIR)/disk-health $(DESTDIR)/usr/local/bin/disk-health

# Unit tests
TEST_SOURCES = $(wildcard $(TESTDIR)/test_*.c)
TEST_BINS = $(patsubst $(TESTDIR)/%.c,$(TESTBINDIR)/%,$(TEST_SOURCES))

# Shared object files needed for tests (exclude main)
TEST_OBJS = $(filter-out $(OBJDIR)/jmraidstatus.o,$(JMICRON_OBJECTS))

tests: $(TEST_BINS)
	@echo ""
	@echo "Running unit tests..."
	@echo ""
	@for test in $(TEST_BINS); do \
		echo "Running $$test..."; \
		$$test || exit 1; \
	done
	@echo ""
	@echo "All test suites passed!"

$(TESTBINDIR)/%: $(TESTDIR)/%.c $(TEST_OBJS) | $(TESTBINDIR)
	$(CC) $(CFLAGS) -I$(TESTDIR) $< $(TEST_OBJS) -o $@

$(TESTBINDIR):
	@mkdir -p $(TESTBINDIR)

clean-tests:
	-rm -rf $(TESTBINDIR)

# Utility tools
TOOLSDIR = tools
TOOLSBINDIR = $(BINDIR)/tools

tools: $(TOOLSBINDIR)/zero_sector

$(TOOLSBINDIR)/zero_sector: $(TOOLSDIR)/zero_sector.c | $(TOOLSBINDIR)
	$(CC) $(CFLAGS) $< -o $@
	@echo "Built: $@"

$(TOOLSBINDIR):
	@mkdir -p $(TOOLSBINDIR)

clean-tools:
	-rm -rf $(TOOLSBINDIR)

# Integration tests
integration-tests: $(TARGETS)
	@echo "Running integration tests..."
	@bash tests/integration/test_aggregator.sh

# Alias for integration tests
test: integration-tests

.PHONY: all clean install tests clean-tests tools clean-tools integration-tests test help
