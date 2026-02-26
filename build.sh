#!/bin/bash
set -e

# Navigate to project directory
cd /c/Users/darek/Documents/stb

# Clean old build
rm -rf build

# Create new build directory
mkdir -p build
cd build

# Run cmake with MinGW Makefiles
cmake .. -G "MinGW Makefiles"

# Build the project
cmake --build . --config Release

echo "Build completed!"
