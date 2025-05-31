#!/bin/bash

echo "Cleaning build artifacts and generated files..."

# Remove build directory
if [ -d "cmake_gen" ]; then
    echo "Removing cmake_gen directory..."
    rm -rf cmake_gen
fi

# Remove generated binaries
if [ -f "cspc" ]; then
    echo "Removing cspc binary..."
    rm -f cspc
fi

if [ -f "uart_comm" ]; then
    echo "Removing uart_comm binary..."
    rm -f uart_comm
fi

# Remove Python cache files
if [ -d "code-format/__pycache__" ]; then
    echo "Removing Python cache files..."
    rm -rf code-format/__pycache__
fi

# Remove any .pyc files
find . -name "*.pyc" -type f -delete 2>/dev/null

# Remove any temporary files
find . -name "*~" -type f -delete 2>/dev/null
find . -name "*.tmp" -type f -delete 2>/dev/null

echo "Clean completed successfully."
