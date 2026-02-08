CC = gcc
CFLAGS = -g -O2 -Wall -Wextra -std=gnu99
SRCDIR = src
BINDIR = bin
OBJDIR = $(BINDIR)/obj
TESTDIR = tests
TESTBINDIR = $(BINDIR)/tests

SOURCES = $(SRCDIR)/jmraidstatus.c \
          $(SRCDIR)/jm_protocol.c \
          $(SRCDIR)/jm_commands.c \
          $(SRCDIR)/smart_parser.c \
          $(SRCDIR)/smart_attributes.c \
          $(SRCDIR)/output_formatter.c \
          $(SRCDIR)/jm_crc.c \
          $(SRCDIR)/sata_xor.c

OBJECTS = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SOURCES))
TARGET = $(BINDIR)/jmraidstatus

all: $(TARGET)

$(TARGET): $(OBJECTS) | $(BINDIR)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET)
	@echo "Built: $(TARGET)"

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BINDIR):
	@mkdir -p $(BINDIR)

$(OBJDIR):
	@mkdir -p $(OBJDIR)

clean:
	-rm -rf $(BINDIR)

install: $(TARGET)
	install -D -m 755 $(TARGET) $(DESTDIR)/usr/local/bin/jmraidstatus

# Unit tests
TEST_SOURCES = $(wildcard $(TESTDIR)/test_*.c)
TEST_BINS = $(patsubst $(TESTDIR)/%.c,$(TESTBINDIR)/%,$(TEST_SOURCES))

# Shared object files needed for tests (exclude main)
TEST_OBJS = $(filter-out $(OBJDIR)/jmraidstatus.o,$(OBJECTS))

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

.PHONY: all clean install tests clean-tests tools clean-tools
