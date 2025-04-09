#!/bin/bash

# Check if a directory called "build" exists
if [ -d "cmake_gen" ]; then
    # If it exists, remove it
    echo "Removing existing build directory..."
    rm -rf cmake_gen
fi

# Create a new build directory
echo "Creating new build directory..."
mkdir cmake_gen

# Change directory into the build directory
cd cmake_gen

# Run cmake with Ninja generator
echo "Running cmake..."
cmake -G Ninja ..

# Check if cmake was successful
if [ $? -eq 0 ]; then
    # Run ninja
    echo "Running ninja..."
    ninja
else
    echo "Cmake configuration failed. Exiting."
    exit 1
fi

# Copy binary
mv cspc ../

# Move back to the parent directory
cd ..

echo "Build script completed successfully."
