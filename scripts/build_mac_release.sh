#!/bin/bash

# Standalone macOS Release Packager for NinjaLibrary
# Copyright (c) 2026 NinjaLibrary. All rights reserved.
# Licensed under the BSD-3-Clause License.

set -e

# Resolve the project root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

echo "=============================================="
echo " Starting NinjaLibrary Standalone Mac Release"
echo "=============================================="

# 1. Ensure we are on macOS
if [ "$(uname)" != "Darwin" ]; then
    echo "Error: This script must be run on macOS."
    exit 1
fi

# 2. Check for required build tools
echo "Checking dependencies..."
for tool in cmake codesign hdiutil; do
    if ! command -v $tool &> /dev/null; then
        echo "Error: Required tool '$tool' not found."
        exit 1
    fi
done

# 2a. Locate official Qt installation from ~/Qt
QT_VERSION="6.8.3"
QT_DIR="$HOME/Qt/${QT_VERSION}/macos"
if [ ! -d "$QT_DIR" ]; then
    echo "Error: Official Qt installation not found at $QT_DIR"
    echo "Please install Qt ${QT_VERSION} via the Qt Online Installer to ~/Qt"
    exit 1
fi

QT_CMAKE_PREFIX="${QT_DIR}/lib/cmake"
MACDEPLOYQT_BIN="${QT_DIR}/bin/macdeployqt"

if [ ! -f "$MACDEPLOYQT_BIN" ]; then
    echo "Error: macdeployqt not found at $MACDEPLOYQT_BIN"
    exit 1
fi

echo "Using Qt from: $QT_DIR"
echo "Using macdeployqt: $MACDEPLOYQT_BIN"

# 2b. Generate Apple Icon Image (.icns) file from base PNG
echo "=== Generating macOS Application Icon ==="
PNG_ICON="assets/ninjalibrary.png"
ICNS_OUT="packaging/macos/NinjaLibrary.icns"
if [ -f "$PNG_ICON" ]; then
    if command -v sips &> /dev/null && command -v iconutil &> /dev/null; then
        ICONSET_DIR="packaging/macos/NinjaLibrary.iconset"
        rm -rf "$ICONSET_DIR"
        mkdir -p "$ICONSET_DIR"
        
        echo "Creating icon sizes..."
        sips -z 16 16     "$PNG_ICON" --out "$ICONSET_DIR/icon_16x16.png" &>/dev/null
        sips -z 32 32     "$PNG_ICON" --out "$ICONSET_DIR/icon_16x16@2x.png" &>/dev/null
        sips -z 32 32     "$PNG_ICON" --out "$ICONSET_DIR/icon_32x32.png" &>/dev/null
        sips -z 64 64     "$PNG_ICON" --out "$ICONSET_DIR/icon_32x32@2x.png" &>/dev/null
        sips -z 128 128   "$PNG_ICON" --out "$ICONSET_DIR/icon_128x128.png" &>/dev/null
        sips -z 256 256   "$PNG_ICON" --out "$ICONSET_DIR/icon_128x128@2x.png" &>/dev/null
        sips -z 256 256   "$PNG_ICON" --out "$ICONSET_DIR/icon_256x256.png" &>/dev/null
        sips -z 512 512   "$PNG_ICON" --out "$ICONSET_DIR/icon_256x256@2x.png" &>/dev/null
        sips -z 512 512   "$PNG_ICON" --out "$ICONSET_DIR/icon_512x512.png" &>/dev/null
        sips -z 1024 1024 "$PNG_ICON" --out "$ICONSET_DIR/icon_512x512@2x.png" &>/dev/null
        
        echo "Compiling into $ICNS_OUT..."
        iconutil -c icns "$ICONSET_DIR" -o "$ICNS_OUT"
        rm -rf "$ICONSET_DIR"
        echo "Application icon compiled successfully."
    else
        echo "Warning: 'sips' or 'iconutil' not found. Skipping icon generation."
    fi
else
    echo "Warning: Base icon '$PNG_ICON' not found. Skipping icon generation."
fi

# 3. Clean and create build directory
BUILD_DIR="build-release"
echo "=== Cleaning build directory ($BUILD_DIR) ==="
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# 4. Configure project in Release mode with Universal Binary support
echo "=== Configuring project with CMake in Release mode (Universal Binary: x86_64 + arm64) ==="
# Workaround: AGL framework is missing in modern macOS SDKs but referenced by Qt 6.8's FindWrapOpenGL.
# We redirect it to OpenGL framework which is still present.
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_CMAKE_PREFIX" \
    -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="12.0" \
    -DWrapOpenGL_AGL="-framework OpenGL" \
    -DBUILD_UNIVERSAL=ON \
    -DENABLE_TESTING=OFF

# 5. Build application target
echo "=== Building Universal Binary application target ==="
cmake --build "$BUILD_DIR" --target NinjaLibraryApp --parallel

# 6. Verify App Bundle creation
APP_BUNDLE="$BUILD_DIR/NinjaLibrary.app"
if [ ! -d "$APP_BUNDLE" ]; then
    echo "Error: App bundle was not created at $APP_BUNDLE"
    exit 1
fi

# 7. Tesseract OCR data files are no longer needed on macOS.
#    OCR is handled natively by the macOS Vision framework.
echo "=== Skipping Tesseract tessdata (macOS uses native Vision framework for OCR) ==="

# 7b. Copy macOS application icon
if [ -f "$ICNS_OUT" ]; then
    echo "=== Copying application icon to bundle ==="
    mkdir -p "$APP_BUNDLE/Contents/Resources"
    cp "$ICNS_OUT" "$APP_BUNDLE/Contents/Resources/"
    echo "  Copied NinjaLibrary.icns to bundle Resources/"
fi

# 8. Deploy Qt frameworks, plugins, and third-party dynamic libraries
echo "=== Running macdeployqt for dependency resolution ==="
"$MACDEPLOYQT_BIN" "$APP_BUNDLE" -qmldir=. -verbose=1

# 9. Perform ad-hoc code signing with App Sandbox entitlements
echo "=== Code signing application bundle ==="
ENTITLEMENTS="packaging/macos/entitlements.plist"
if [ -f "$ENTITLEMENTS" ]; then
    echo "Signing with entitlements: $ENTITLEMENTS"
    # Sign all dylibs and frameworks inside the bundle first to prevent code signing verification failure
    find "$APP_BUNDLE/Contents/Frameworks" -type f \( -name "*.dylib" -o -name "*.framework" -o -perm +111 \) -exec codesign --force --sign - {} \; 2>/dev/null || true
    find "$APP_BUNDLE/Contents/PlugIns" -type f \( -name "*.dylib" -o -perm +111 \) -exec codesign --force --sign - {} \; 2>/dev/null || true
    # Sign main bundle with entitlements
    codesign --force --deep --sign - --entitlements "$ENTITLEMENTS" "$APP_BUNDLE"
else
    echo "Warning: entitlements.plist not found. Performing standard ad-hoc signing..."
    codesign --force --deep --sign - "$APP_BUNDLE"
fi

# 10. Generate compressed DMG installer disk image
DMG_FILE="$BUILD_DIR/NinjaLibrary.dmg"
echo "=== Generating standalone DMG disk image ==="
if [ -f "$DMG_FILE" ]; then
    rm "$DMG_FILE"
fi

hdiutil create -volname "NinjaLibrary" -srcfolder "$APP_BUNDLE" -ov -format UDZO "$DMG_FILE"

echo "=============================================="
echo " Mac Release Standalone Build Complete!"
echo " App Bundle: $APP_BUNDLE"
echo " DMG Installer: $DMG_FILE"
echo "=============================================="
