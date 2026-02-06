# Test Programs

This directory contains development and debugging utilities used during jmraidstatus development.

## Test Programs

### dump_smart
Dumps raw SMART data responses from the JMicron controller for analysis.

### test_crc
Tests CRC-32 calculation and validation for JMicron protocol communication.

### test_disk4
Tests communication with disk 4 (5th disk slot) on the JMicron controller.

### test_disk4_crc
Tests CRC calculation specifically for disk 4 commands.

### test_identify
Tests the IDENTIFY DEVICE command through the JMicron controller.

### test_multi_crc
Tests CRC calculation across multiple command types.

### test_which_crc
Determines which CRC polynomial and seed are used by the JMicron protocol.

### trace_crc
Traces CRC calculation steps for debugging.

## Building Test Programs

These programs are standalone utilities compiled separately from the main jmraidstatus binary. To compile:

```bash
cd tests
gcc -o test_name test_name.c ../src/jm_*.o ../src/sata_xor.o
```

## Note

These utilities were used during development to reverse-engineer the JMicron protocol and are kept for reference and debugging purposes. They are not required for normal operation of jmraidstatus.
