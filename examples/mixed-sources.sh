#!/bin/bash
# Example: Monitor JMicron RAID array + regular SATA drives
#
# Usage: sudo ./examples/mixed-sources.sh
#        sudo ./examples/mixed-sources.sh --json

{
  # JMicron RAID array
  jmraidstatus --json-only /dev/sdc

  # Regular SATA drives via smartctl
  smartctl --json=c --all /dev/sda | smartctl-parser
  smartctl --json=c --all /dev/sdb | smartctl-parser

} | disk-health "$@"
