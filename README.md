# SimpleChat - Programming Assignment 1

# Jaime Blanco

## Project Overview

**SimpleChat** is a ring-based messaging application implemented in **C++ with Qt6**.  
Each instance of the application acts as a peer in a **TCP ring network**, where messages are passed along the ring until they reach their destination.

This project includes:

- A **Qt6 GUI** with a chat log area and text input
- **TCP socket communication** between peers using `QTcpSocket`
- **Message serialization** with `QVariantMap`
- A **static peer ring topology** (A → B → C → D → A)
- A **launch script** for automated testing with multiple peers

---

## Requirements

- C++17 or newer
- [Qt6](https://doc.qt.io/qt-6/qtexamplesandtutorials.html)
- CMake (3.15+)
- Git (for version control)

---

## Build Instructions

Clone the repository and build with CMake:

    git clone <your-repo-url>
    cd programming_assignment_1

    # Create build directory
    mkdir -p build
    cd build

    # Run CMake
    cmake ..
    cmake --build .

The compiled binary will be located in:

    ./build/SimpleChat

---

## Running Peers

Each peer requires command-line arguments:

- `-i` → unique ID (A, B, C, …)
- `-p` → port to listen on
- `-n` → neighbor ID
- `-q` → neighbor’s port

Example with 4 peers (A → B → C → D → A):

    ./build/SimpleChat -i A -p 9001 -n B -q 9002
    ./build/SimpleChat -i B -p 9002 -n C -q 9003
    ./build/SimpleChat -i C -p 9003 -n D -q 9004
    ./build/SimpleChat -i D -p 9004 -n A -q 9001

Each peer opens a chat window GUI, where you can type and send messages.  
Messages are serialized, passed along the ring, and delivered to the correct destination.

---

## Script Usage

### Ring Launch Script

The script `run_ring_test.sh` automatically starts a 4-peer ring (A, B, C, D):

    #!/bin/bash

    # Clear old logs
    rm -f A.log B.log C.log D.log

    # Launch peers in background with logs
    ./build/SimpleChat -i A -p 9001 -n B -q 9002 > A.log 2>&1 &
    ./build/SimpleChat -i B -p 9002 -n C -q 9003 > B.log 2>&1 &
    ./build/SimpleChat -i C -p 9003 -n D -q 9004 > C.log 2>&1 &
    ./build/SimpleChat -i D -p 9004 -n A -q 9001 > D.log 2>&1 &

    echo "SimpleChat ring launched (A-B-C-D). Logs saved to A.log, B.log, C.log, D.log."

Run it:

    ./run_ring_test.sh

---

### Useful Commands

View logs in real-time:

    tail -f A.log B.log C.log D.log

Kill all peers:

    pkill SimpleChat

Manually clear logs:

    rm -f A.log B.log C.log D.log

---

## Testing

### Basic Propagation

- Launch the full 4-peer ring (A, B, C, D).
- Send a message from A → it should travel A → B → C → D and be delivered.
- Verify logs: each peer prints receipt and forwarding.

### Message Ordering

- Send multiple messages in quick succession.
- Ensure sequence numbers are consistent and messages arrive in correct order.

### Two-Peer Test

- Run only A and B.
- Verify messages are delivered directly without traveling around the full ring.

### Full Ring Test

- Run all 4 peers (A, B, C, D).
- Send messages between arbitrary peers (e.g., C → A).
- Confirm they circulate the expected number of hops before delivery.

### Script Validation

- Use `run_ring_test.sh` to automate setup.
- Confirm logs are cleared before each run and contain only current test output.
