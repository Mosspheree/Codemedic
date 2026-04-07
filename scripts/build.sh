#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."

echo "═══════════════════════════════════════"
echo "  codemedic build"
echo "═══════════════════════════════════════"

if [ -z "$ANTHROPIC_API_KEY" ]; then
    echo "⚠  Warning: ANTHROPIC_API_KEY not set."
    echo "   Set it before running: export ANTHROPIC_API_KEY=sk-ant-..."
fi

BUILD_DIR="$ROOT/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "→ Configuring..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

echo "→ Building..."
make -j$(nproc)

echo ""
echo "✓ Build complete: $BUILD_DIR/codemedic"
echo ""
echo "Quick test:"
echo "  export ANTHROPIC_API_KEY=sk-ant-..."
echo "  ./build/codemedic tests/demo_bugs.cpp"
echo "  ./build/codemedic tests/cascade_test.cpp   # tests root cause analysis"
echo "  ./build/codemedic -e tests/demo_bugs.cpp   # explain only"
echo "  ./build/codemedic -b ./tests/              # batch mode"
