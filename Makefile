CC = gcc
CFLAGS = -g -O2 -Wall -Wextra -std=gnu99
SRCDIR = src
BINDIR = bin
OBJDIR = $(BINDIR)/obj

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

.PHONY: all clean install
