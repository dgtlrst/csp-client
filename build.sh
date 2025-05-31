#!/bin/bash

# Function to check if running with sudo privileges
check_sudo() {
    if [ "$EUID" -ne 0 ]; then
        echo "This script requires sudo privileges to install dependencies."
        echo "Please run with: sudo ./build.sh"
        exit 1
    fi
}

# Function to install required dependencies
install_dependencies() {
    echo "Installing required dependencies..."

    # Update package list
    echo "Updating package list..."
    apt update

    # Install required packages
    echo "Installing libsocketcan2 and libsocketcan-dev..."
    apt install -y libsocketcan2 libsocketcan-dev

    echo "Installing can-utils..."
    apt install -y can-utils

    echo "Installing pkg-config..."
    apt install -y pkg-config

    echo "Installing cmake and ninja-build..."
    apt install -y cmake ninja-build

    echo "Installing clang-format..."
    apt install -y clang-format

    # Check if PEAK CAN drivers are available in the kernel
    echo "Checking for PEAK CAN drivers..."
    if lsmod | grep -q "peak_usb\|pcan"; then
        echo "PEAK CAN drivers are already loaded."
    else
        echo "PEAK CAN drivers not found in loaded modules."
        echo "Note: PEAK CAN drivers may need to be installed separately."
        echo "For PEAK USB adapters, the peak_usb driver should be available in the kernel."
        echo "For other PEAK adapters, you may need to install drivers from PEAK-System."
    fi

    echo "Dependencies installation completed."
    echo
}

# Check for sudo privileges and install dependencies
check_sudo
install_dependencies

# Get the original user who invoked sudo (if any)
ORIGINAL_USER=${SUDO_USER:-$(whoami)}
ORIGINAL_HOME=$(eval echo ~$ORIGINAL_USER)

# Function to run commands as the original user
run_as_user() {
    if [ "$ORIGINAL_USER" != "root" ]; then
        sudo -u "$ORIGINAL_USER" "$@"
    else
        "$@"
    fi
}

# Check if a directory called "build" exists
if [ -d "cmake_gen" ]; then
    # If it exists, remove it
    echo "Removing existing build directory..."
    rm -rf cmake_gen
fi

# Create a new build directory
echo "Creating new build directory..."
run_as_user mkdir cmake_gen

# Change directory into the build directory
cd cmake_gen

# Run cmake with Ninja generator
echo "Running cmake..."
run_as_user cmake -G Ninja ..

# Check if cmake was successful
if [ $? -eq 0 ]; then
    # Run ninja
    echo "Running ninja..."
    run_as_user ninja
else
    echo "Cmake configuration failed. Exiting."
    exit 1
fi

# Copy binaries
run_as_user mv cspc ../
run_as_user mv uart_comm ../

# Move back to the parent directory
cd ..

# Fix ownership of generated files
if [ "$ORIGINAL_USER" != "root" ]; then
    chown -R "$ORIGINAL_USER:$ORIGINAL_USER" .
fi

echo "Build script completed successfully."
echo
echo "To run the UART communication program:"
echo "  1. Set up socat: sudo socat -v PTY,link=/dev/ttyV0,raw,echo=0,mode=666 PTY,link=/dev/ttyV1,raw,echo=0,mode=666"
echo "  2. Run first node: ./uart_comm -a 1 -d 2 -u /tmp/ttyV0"
echo "  3. Run second node: ./uart_comm -a 2 -d 1 -u /tmp/ttyV1"
