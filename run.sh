#!/bin/bash
# Drone Recorder — launch script
# Runs the binary with Linuxbrew excluded from PATH so the system
# dynamic linker resolution works correctly.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BINARY="$SCRIPT_DIR/build/drone_recorder"

if [ ! -f "$BINARY" ]; then
    echo "Binary not found. Building first..."
    mkdir -p "$SCRIPT_DIR/build"
    cd "$SCRIPT_DIR/build"
    PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin cmake .. -DCMAKE_BUILD_TYPE=Release
    PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin make -j$(nproc)
fi

echo "Starting Drone Recorder..."
exec "$BINARY" "$@"
