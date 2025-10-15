#!/bin/bash

# SimpleChat - Automated Test Script
# Programming Assignment 2

echo "========================================"
echo "SimpleChat - Automated Test Launcher"
echo "========================================"

# Check if build exists
if [ ! -f "build/SimpleChat" ]; then
    echo "ERROR: SimpleChat executable not found!"
    echo "Please build the project first:"
    echo "  mkdir -p build && cd build && cmake .. && make"
    exit 1
fi

# Clean up any previous instances
echo "Cleaning up previous instances..."
pkill -f SimpleChat 2>/dev/null || true
sleep 1

# Clear old logs
echo "Clearing old log files..."
rm -f *.log

echo ""
echo "Starting SimpleChat peers..."
echo "Press Ctrl+C to stop all peers"
echo ""

# Launch peers with different ports
echo "Launching Peer A on port 12340..."
./build/SimpleChat --id A --port 12340 > A.log 2>&1 &
PEER_A_PID=$!

echo "Launching Peer B on port 12341..."
./build/SimpleChat --id B --port 12341 > B.log 2>&1 &
PEER_B_PID=$!

echo "Launching Peer C on port 12342..."
./build/SimpleChat --id C --port 12342 > C.log 2>&1 &
PEER_C_PID=$!

echo "Launching Peer D on port 12343..."
./build/SimpleChat --id D --port 12343 > D.log 2>&1 &
PEER_D_PID=$!

echo ""
echo "All peers started!"
echo "Peer A: PID $PEER_A_PID (port 12340)"
echo "Peer B: PID $PEER_B_PID (port 12341)" 
echo "Peer C: PID $PEER_C_PID (port 12342)"
echo "Peer D: PID $PEER_D_PID (port 12343)"
echo ""
echo "Logs are being written to A.log, B.log, C.log, D.log"
echo "Use 'tail -f *.log' to monitor in real-time"
echo ""
echo "Testing Instructions:"
echo "1. Wait 5-10 seconds for peer discovery"
echo "2. Send direct messages between peers"
echo "3. Test broadcast functionality"
echo "4. Verify anti-entropy synchronization"
echo ""
echo "Press Ctrl+C to stop all peers..."

# Trap to cleanup on exit
cleanup() {
    echo ""
    echo "Stopping all peers..."
    kill $PEER_A_PID $PEER_B_PID $PEER_C_PID $PEER_D_PID 2>/dev/null || true
    wait
    echo "All peers stopped."
    echo "Log files preserved: A.log B.log C.log D.log"
}

trap cleanup EXIT INT TERM

# Wait for user to stop
wait