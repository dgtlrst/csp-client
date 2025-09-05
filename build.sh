#!/bin/bash

# function to check if running with sudo privileges
check_sudo() {
    if [ "$EUID" -ne 0 ]; then
        echo "sudo required to install dependencies"
        exit 1
    fi
}

# function to check if a package is installed
is_package_installed() {
    dpkg -l "$1" 2>/dev/null | grep -q "^ii"
}

# function to install a package if not already installed
install_if_needed() {
    local package="$1"
    local description="$2"

    if is_package_installed "$package"; then
        echo "$description is already installed"
    else
        echo "installing $description ($package).."
        apt install -y "$package"
    fi
}

# install deps
install_dependencies() {
    # update package list
    apt update

    # install required packages
    install_if_needed "gcc" "gcc compiler"
    install_if_needed "libsocketcan2" "socketcan library"
    install_if_needed "libsocketcan-dev" "socketcan development headers"
    install_if_needed "can-utils" "can utilities"
    install_if_needed "pkg-config" "package configuration tool"
    install_if_needed "cmake" "cmake build system"
    install_if_needed "ninja-build" "ninja build system"
    install_if_needed "clang-format" "clang code formatter"
    install_if_needed "socat" "socket relay tool"

    # Check if PEAK CAN drivers are available in the kernel
    echo "checking for peack can drivers.."
    if lsmod | grep -q "peak_usb\|pcan"; then
        echo "peack can drivers are already loaded"
    else
        echo "peak can drivers not found in loaded modules"
        echo "note: PEAK CAN drivers may need to be installed separately"
        echo "for peak usb adapters, the peak_usb driver should be available in the kernel"
        echo "for other peak adapters, you may need to install drivers from peak-system"
    fi

    echo "dependency installation complete"
    echo
}

# check for sudo privileges and install dependencies
check_sudo
install_dependencies

# get the original user who invoked sudo (if any)
ORIGINAL_USER=${SUDO_USER:-$(whoami)}
ORIGINAL_HOME=$(eval echo ~$ORIGINAL_USER)

# function to run commands as the original user
run_as_user() {
    if [ "$ORIGINAL_USER" != "root" ]; then
        sudo -u "$ORIGINAL_USER" "$@"
    else
        "$@"
    fi
}

# check if a directory called "build" exists
if [ -d "cmake_gen" ]; then
    # If it exists, remove it
    echo "removing existing build directory.."
    rm -rf cmake_gen
fi

# create a new build directory
echo "creating new build directory.."
run_as_user mkdir cmake_gen

# change directory into the build directory
cd cmake_gen

# run cmake with Ninja generator
echo "running cmake.."
run_as_user cmake -G Ninja ..

# check if cmake was successful
if [ $? -eq 0 ]; then
    # run ninja
    echo "running ninja..."
    run_as_user ninja
else
    echo "cmake configuration failed..exiting"
    exit 1
fi

# copy binaries
run_as_user mv cspc ../
run_as_user mv uart_comm ../
run_as_user mv kiss_comm ../

# move back to the parent directory
cd ..

# fix ownership of generated files
if [ "$ORIGINAL_USER" != "root" ]; then
    chown -R "$ORIGINAL_USER:$ORIGINAL_USER" .
fi

echo
echo "| build complete |"
echo
echo "run:"
echo " - set up socat: sudo socat -v PTY,link=/dev/ttyV0,raw,echo=0,mode=666 PTY,link=/dev/ttyV1,raw,echo=0,mode=666"
echo " - run first uart node: ./uart_comm -a 1 -d 2 -u /dev/ttyV0"
echo " - run second uart node: ./uart_comm -a 2 -d 1 -u /dev/ttyV1"
echo " - run first kiss node: ./kiss_comm -a 1 -d 2 -k /dev/ttyV0"
echo " - run second kiss node: ./kiss_comm -a 2 -d 1 -k /dev/ttyV1"
