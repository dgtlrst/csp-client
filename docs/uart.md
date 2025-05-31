# UART Communication with libcsp

This document explains how to use the UART interface with libcsp, particularly using `socat` for testing.

## Overview

The `comm_uart.c` file provides a minimal implementation for CSP (Cubesat Space Protocol) communication over UART. It allows nodes to communicate with each other using raw CSP packets over UART without the KISS protocol overhead.

## Building the Code

To build the code, use the provided build script:

```bash
./build.sh
```

This will create the `uart_comm` executable in the root directory.

## Setting Up Virtual TTY Ports with socat

For testing without physical UART hardware, you can use `socat` to create virtual TTY ports:

```bash
socat -dd pty,raw,echo=0,link=/tmp/pty1 pty,raw,echo=0,link=/tmp/pty2
```

This command creates two virtual TTY ports (`/tmp/pty1` and `/tmp/pty2`) that are connected to each other. Any data written to one port will be available for reading on the other port.

You can verify that the virtual ports have been created successfully with:

```bash
ls -la /tmp/pty*
```

This should show both `/tmp/pty1` and `/tmp/pty2` as symbolic links to their respective `/dev/pts/XX` devices.

## Running the UART Communication Program

After setting up the virtual TTY ports, you can run two instances of the program to simulate communication between two nodes:

### First Node (Address 1)

```bash
./uart_comm -a 1 -d 2 -u /tmp/pty1
```

### Second Node (Address 2)

```bash
./uart_comm -a 2 -d 1 -u /tmp/pty2
```

### Command Line Options

- `-a <node_addr>`: Set the node's address (default: 2)
- `-d <dest_addr>`: Set the destination address (default: 1)
- `-u <uart_device>`: Set the UART device path (default: /tmp/pty1)
- `-h`: Display help message

## How It Works

1. The program initializes CSP and sets up a UART interface.
2. It creates three threads:
   - Router thread: Handles CSP packet routing
   - Server thread: Listens for incoming connections
   - Client thread: Periodically sends messages to the other node
3. When data is received on the UART port, it's converted to CSP packets and processed.
4. The program uses a custom UART interface that directly processes raw data without the KISS protocol overhead.

## Example Communication Flow

1. Node 1 sends a ping to Node 2
2. Node 2 responds to the ping
3. Node 1 sends a message to Node 2
4. Node 2 receives and processes the message

## Notes

- The implementation uses static memory allocation, making it suitable for embedded systems.
- The code is designed to be minimal and clean, focusing on essential functionality.
- Error handling is included throughout the code to ensure robustness.
