#!/usr/bin/env bash
set -euo pipefail

# ─── Codemedic installer ─────────────────────────────────────────────────────
# Builds from source and installs to /usr/local/bin (or custom prefix)

PREFIX="${PREFIX:-/usr/local}"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"

RED='\033[31m'
GRN='\033[32m'
CYN='\033[36m'
RST='\033[0m'

info()  { echo -e "${CYN}[info]${RST}  $*"; }
ok()    { echo -e "${GRN}[ok]${RST}    $*"; }
fail()  { echo -e "${RED}[fail]${RST}  $*"; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."

echo ""
echo "  ╔══════════════════════════════════╗"
echo "  ║  codemedic installer             ║"
echo "  ╚══════════════════════════════════╝"
echo ""

# ── Check dependencies ────────────────────────────────────────────────────────
info "Checking dependencies..."

for cmd in cmake make; do
    if ! command -v "$cmd" &>/dev/null; then
        fail "$cmd is required but not found. Install it first."
    fi
done

# Check for C++ compiler
if ! command -v clang++ &>/dev/null && ! command -v g++ &>/dev/null; then
    fail "No C++ compiler found. Install clang++ or g++."
fi

# Check for libcurl
if ! pkg-config --exists libcurl 2>/dev/null; then
    info "libcurl development files may not be installed."
    info "  Ubuntu/Debian: sudo apt install libcurl4-openssl-dev"
    info "  Fedora/RHEL:   sudo dnf install libcurl-devel"
fi

ok "Dependencies look good."

# ── Build ─────────────────────────────────────────────────────────────────────
info "Building codemedic (${BUILD_TYPE})..."

BUILD_DIR="$ROOT/build"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake "$ROOT" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

make -j"$JOBS"

ok "Build complete."

# ── Install ───────────────────────────────────────────────────────────────────
info "Installing to ${PREFIX}/bin..."

if [ -w "${PREFIX}/bin" ]; then
    make install
else
    info "Need sudo for install to ${PREFIX}/bin"
    sudo make install
fi

ok "Installed codemedic to ${PREFIX}/bin/codemedic"

# ── Shell completions ─────────────────────────────────────────────────────────
if [ -d /etc/bash_completion.d ] && [ -f "$ROOT/completions/codemedic.bash" ]; then
    info "Installing bash completions..."
    if [ -w /etc/bash_completion.d ]; then
        cp "$ROOT/completions/codemedic.bash" /etc/bash_completion.d/codemedic
    else
        sudo cp "$ROOT/completions/codemedic.bash" /etc/bash_completion.d/codemedic
    fi
    ok "Bash completions installed."
fi

# ── Done ──────────────────────────────────────────────────────────────────────
echo ""
ok "Installation complete!"
echo ""
echo "  Quick start:"
echo "    export ANTHROPIC_API_KEY=sk-ant-..."
echo "    codemedic broken.cpp"
echo ""
echo "  Or use with Ollama (no API key needed):"
echo "    ollama pull llama3"
echo "    codemedic --provider ollama broken.cpp"
echo ""
