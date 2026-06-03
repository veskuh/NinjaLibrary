#!/bin/bash

# Exit immediately if any command exits with a non-zero status
set -e

# Resolve the project root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

CLEAN=false

# Check for --clean argument
for arg in "$@"; do
    if [ "$arg" == "--clean" ]; then
        CLEAN=true
    fi
done

# Perform clean if requested
if [ "$CLEAN" = true ]; then
    echo "=== Cleaning build directory ==="
    rm -rf build
fi

# Ensure build directory exists
mkdir -p build

echo "=== Configuring build system ==="
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTING=ON

echo "=== Building project ==="
cmake --build build --parallel

echo "=== Running CTest verification ==="
ctest --test-dir build --output-on-failure

echo "=== Running NinjaLibrary application ==="
./build/NinjaLibraryApp
