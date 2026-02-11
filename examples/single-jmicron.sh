#!/bin/bash
# Example: Monitor single JMicron RAID array
#
# Usage: sudo ./examples/single-jmicron.sh
#        sudo ./examples/single-jmicron.sh --json

jmraidstatus --json-only /dev/sdc | disk-health "$@"
