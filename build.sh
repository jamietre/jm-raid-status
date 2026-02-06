#!/bin/bash
# Simple build script for jmraidstatus

set -e

echo "Building jmraidstatus..."
make clean
make

echo ""
echo "Build complete!"
echo "Binary location: bin/jmraidstatus"
echo ""
echo "To install:"
echo "  sudo make install"
echo ""
echo "To test:"
echo "  sudo bin/jmraidstatus /dev/sdX"
