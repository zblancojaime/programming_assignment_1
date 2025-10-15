# SimpleChat - Programming Assignment 2

# Jaime Blanco

## Project Overview

**SimpleChat** is a distributed peer-to-peer messaging application implemented in **C++ with Qt6**.  
Each instance acts as a peer in a **UDP-based network** with reliable messaging and anti-entropy synchronization.

This project includes:

- A **Qt6 GUI** with chat log area and peer selection
- **UDP socket communication** using `QUdpSocket`
- **Reliable messaging** with ACK/retransmission
- **Anti-entropy system** using vector clocks
- **Broadcast messaging** to all network peers
- **Automatic peer discovery** on local ports
- **Message deduplication** and ordering

---

## Requirements

- C++17 or newer
- [Qt6](https://doc.qt.io/qt-6/qtexamplesandtutorials.html)
- CMake (3.16+)
- Git (for version control)

---

## Build Instructions

Clone the repository and build with CMake:

    git clone -b programming-assignment-2 https://github.com/zblancojaime/programming_assignment_1.git
    cd programming_assignment_1

    # Create build directory
    mkdir -p build
    cd build

    # Run CMake
    cmake ..
    make

The compiled binary will be located in:

    ./build/SimpleChat

---

## Running Peers

Each peer requires command-line arguments:

- `-i` or `--id` → unique peer identifier (A, B, C, etc.)
- `-p` or `--port` → UDP port to listen on (recommended: 12340-12350)

Example with 4 peers:

    ./build/SimpleChat --id A --port 12340
    ./build/SimpleChat --id B --port 12341
    ./build/SimpleChat --id C --port 12342
    ./build/SimpleChat --id D --port 12343

Each peer opens a chat window GUI with:

- **Chat log**: Displays all messages and system events
- **Peer selector**: Choose recipient or "BROADCAST"
- **Message input**: Type and send messages

Peers automatically discover each other on the local network.

---

## Testing Script

Use the automated test script to launch all 4 peers and create log files:

```bash
chmod +x test_script.sh
./test_script.sh
```

Press Ctrl+C to stop all peers when finished testing.

### Manual Testing

1. Start multiple instances using different terminals
2. Wait for peer discovery (5-10 seconds)
3. Send direct messages between peers
4. Test broadcast functionality
5. Verify message ordering and anti-entropy

### Testing Features

- **Direct messaging**: Select a peer and send messages
- **Broadcast**: Select "BROADCAST" to send to all peers
- **Reliability**: Messages are automatically retransmitted if no ACK received
- **Anti-entropy**: Peers sync message histories every 10 seconds
- **Peer discovery**: New peers are automatically discovered and added

### Useful Commands

View logs in real-time:

    tail -f A.log B.log C.log D.log

Kill all peers:

    pkill SimpleChat
