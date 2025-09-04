#!/bin/bash

# remove build directory
if [ -d "cmake_gen" ]; then
    echo "deleting cmake_gen dir.."
    rm -rf cmake_gen
fi

# remove generated binaries
echo "deleting existing binaries.."
if [ -f "cspc" ] || [ -f "uart_comm" ]; then
    rm -f cspc uart_comm
fi

# remove Python cache files
if [ -d "code-format/__pycache__" ]; then
    rm -rf code-format/__pycache__
fi

# remove any .pyc and temp files
find . -name "*.pyc" -type f -delete 2>/dev/null
find . -name "*~" -type f -delete 2>/dev/null
find . -name "*.tmp" -type f -delete 2>/dev/null

echo "clean complete"
