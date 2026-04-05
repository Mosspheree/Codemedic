#!/usr/bin/env bash
set -euo pipefail
echo "Building fixcc..."
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release 2>&1 | grep -v "^--"
make -j$(nproc)
echo ""
echo "✓ Built: build/fixcc"
echo ""
echo "Usage:"
echo "  export ANTHROPIC_API_KEY=sk-ant-..."
echo "  ./build/fixcc tests/demo_bugs.cpp"
echo "  ./build/fixcc -y tests/demo_bugs.cpp   # auto-apply"
