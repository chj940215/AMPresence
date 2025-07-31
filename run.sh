#!/bin/bash

# This script run the Swift server and C++ client together. Make sure the C++ & Swift files are compiled

# Config
# Path to the Swift server executable
SWIFT_SERVER_PATH="./NowPlayingServer"

# Path to the C++ client executable
CPP_CLIENT_PATH="./AppleMusicPresence"

# Check whether the Swift server executable exists
if [ ! -f "$SWIFT_SERVER_PATH" ]; then
    echo "❌ Error: Swift server executable not found at '$SWIFT_SERVER_PATH'"
    echo "Please compile NowPlayingServer.swift first using: swiftc NowPlayingServer.swift -o NowPlayingServer"
    exit 1
fi

# Check whether the C++ client executable exists
if [ ! -f "$CPP_CLIENT_PATH" ]; then
    echo "❌ Error: C++ client executable not found at '$CPP_CLIENT_PATH'"
    echo "Please compile the C++ project first."
    exit 1
fi

echo "🚀 Starting Music Presence for Discord..."

echo "🎧 Launching Swift data server in the background..."
$SWIFT_SERVER_PATH &
SWIFT_PID=$!

# Hold on
sleep 2

echo "💬 Launching C++ Discord client..."
$CPP_CLIENT_PATH

# Cleanup
echo "🛑 Shutting down Swift server..."
kill $SWIFT_PID
echo "✅ Done."
