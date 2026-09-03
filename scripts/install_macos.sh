#!/usr/bin/env bash
# Build ActivityOS from source and install it to /Applications.
# Avoids macOS Gatekeeper blocks that often affect downloaded DMG files.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "==> ActivityOS macOS installer"

if ! command -v brew >/dev/null 2>&1; then
  echo "Homebrew is required. Install it from https://brew.sh then run this script again."
  exit 1
fi

echo "==> Installing build dependencies (cmake, qt)..."
brew install cmake qt

QT_PREFIX="$(brew --prefix qt)"
CORES="$(sysctl -n hw.ncpu)"

echo "==> Building ActivityOS..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QT_PREFIX"
cmake --build build -j"$CORES" --target ActivityOS

ENT="$ROOT/resources/macos/ActivityOS.entitlements"
MACDEPLOYQT="$QT_PREFIX/bin/macdeployqt"

echo "==> Bundling Qt frameworks..."
"$MACDEPLOYQT" build/ActivityOS.app -always-overwrite 2>/dev/null || true
codesign --force --deep --sign - --entitlements "$ENT" build/ActivityOS.app

echo "==> Installing to /Applications..."
pkill -x ActivityOS 2>/dev/null || true
rm -rf /Applications/ActivityOS.app
ditto build/ActivityOS.app /Applications/ActivityOS.app
codesign --force --deep --sign - --entitlements "$ENT" /Applications/ActivityOS.app

echo "==> Done. Launching ActivityOS..."
open /Applications/ActivityOS.app

cat <<'EOF'

Installed to /Applications/ActivityOS.app

On first launch:
  1. Accept the privacy consent
  2. Allow Chrome control when prompted
  3. Allow Screen Recording if prompted

If Chrome tabs stay "General", enable ActivityOS for Google Chrome under:
  System Settings > Privacy & Security > Automation
EOF
