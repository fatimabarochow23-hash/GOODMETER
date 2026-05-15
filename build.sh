#!/bin/bash
#==============================================================================
# GOODMETER Multi-Target Build Script
#
# Solves the JuceLibraryCode sharing problem:
# Three .jucer files share one JuceLibraryCode/ directory.
# Each Projucer --resave regenerates it for that target only.
# This script saves/restores per-target snapshots automatically.
#
# Usage:
#   ./build.sh standalone     Build macOS Standalone + AU/VST3
#   ./build.sh plugin         Build Plugin-only (AU/VST3, no Standalone)
#   ./build.sh ios            Build iOS (device arm64)
#   ./build.sh ios-sim        Build iOS Simulator (arm64)
#   ./build.sh all            Build all targets safely
#   ./build.sh resave <target> Just resave the .jucer (no build)
#==============================================================================

set -euo pipefail
export COPYFILE_DISABLE=1

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"
PROJUCER_CANDIDATES=(
    "${PROJUCER:-}"
    "/Users/caiyiyang/Downloads/JUCE/Projucer.app/Contents/MacOS/Projucer"
    "/Users/MediaStorm/Downloads/JUCE/Projucer.app/Contents/MacOS/Projucer"
)
DEVELOPER_DIR="/Applications/Xcode.app/Contents/Developer"
XCODEBUILD="$DEVELOPER_DIR/usr/bin/xcodebuild"
SIMCTL="$DEVELOPER_DIR/usr/bin/simctl"

JLCODE_DIR="$PROJECT_DIR/JuceLibraryCode"
JLCODE_CACHE="$PROJECT_DIR/.jlcode_cache"
ONNXRUNTIME_LIB_DIR="$PROJECT_DIR/ThirdParty/onnxruntime-osx-arm64-1.20.1/lib"
ONNXRUNTIME_DYLIB="$ONNXRUNTIME_LIB_DIR/libonnxruntime.1.20.1.dylib"
AUDIO_DOCTOR_PROJECT_ICON="$PROJECT_DIR/Assets/audio_doctor_project_pigeon.icns"

# .jucer → xcodeproj mapping
JUCER_STANDALONE="$PROJECT_DIR/GOODMETER.jucer"
JUCER_PLUGIN="$PROJECT_DIR/GOODMETER_Plugin.jucer"
JUCER_IOS="$PROJECT_DIR/GOODMETER_iOS.jucer"

XCPROJ_STANDALONE="$PROJECT_DIR/Builds/MacOSX/GOODMETER.xcodeproj"
XCPROJ_PLUGIN="$PROJECT_DIR/Builds/MacOSX_Plugin/GOODMETER.xcodeproj"
XCPROJ_IOS="$PROJECT_DIR/Builds/iOS/GOODMETER.xcodeproj"

# Keep signed macOS products off ExFAT/iCloud working trees. macOS code signing
# is sensitive to AppleDouble/resource-fork files (._*) that those locations can
# create, so final .app bundles are built on the local APFS volume by default.
BUILD_ROOT="${GOODMETER_BUILD_ROOT:-"$HOME/Library/Caches/GOODMETERBuild"}"
PROJECT_CACHE_NAME="$(basename "$PROJECT_DIR")"
if command -v cksum >/dev/null 2>&1; then
    PROJECT_CACHE_HASH="$(printf "%s" "$PROJECT_DIR" | cksum | awk '{ print $1 }')"
    PROJECT_CACHE_NAME="${PROJECT_CACHE_NAME}-${PROJECT_CACHE_HASH}"
fi
LOCAL_BUILD_ROOT="$BUILD_ROOT/$PROJECT_CACHE_NAME"

PROJUCER=""
for candidate in "${PROJUCER_CANDIDATES[@]}"; do
    if [ -n "$candidate" ] && [ -x "$candidate" ]; then
        PROJUCER="$candidate"
        break
    fi
done

#==============================================================================
# JuceLibraryCode snapshot management
#==============================================================================
save_jlcode() {
    local target="$1"
    local cache_dir="$JLCODE_CACHE/$target"
    mkdir -p "$cache_dir"
    rsync -a --delete "$JLCODE_DIR/" "$cache_dir/"
    echo "  [cache] Saved JuceLibraryCode → .jlcode_cache/$target"
}

restore_jlcode() {
    local target="$1"
    local cache_dir="$JLCODE_CACHE/$target"
    if [ -d "$cache_dir" ]; then
        rsync -a --delete "$cache_dir/" "$JLCODE_DIR/"
        echo "  [cache] Restored JuceLibraryCode ← .jlcode_cache/$target"
        return 0
    fi
    return 1
}

bundle_onnxruntime_for_app() {
    local app_bundle="$1"
    local frameworks_dir="$app_bundle/Contents/Frameworks"
    local resources_dir="$app_bundle/Contents/Resources"

    if [ ! -f "$ONNXRUNTIME_DYLIB" ]; then
        echo "ONNX Runtime dylib not found: $ONNXRUNTIME_DYLIB"
        exit 1
    fi

    mkdir -p "$frameworks_dir" "$resources_dir"
    COPYFILE_DISABLE=1 cp "$ONNXRUNTIME_DYLIB" "$frameworks_dir/"
    ln -sf "libonnxruntime.1.20.1.dylib" "$frameworks_dir/libonnxruntime.dylib"

    if [ -f "$AUDIO_DOCTOR_PROJECT_ICON" ]; then
        COPYFILE_DISABLE=1 cp "$AUDIO_DOCTOR_PROJECT_ICON" "$resources_dir/"
    else
        echo "Audio Doctor project icon not found: $AUDIO_DOCTOR_PROJECT_ICON"
        exit 1
    fi

    /usr/bin/codesign --force --sign - "$frameworks_dir/libonnxruntime.1.20.1.dylib" >/dev/null
    /usr/bin/codesign --force --sign - --preserve-metadata=entitlements,requirements,flags "$app_bundle" >/dev/null
    echo "  [bundle] Copied ONNX Runtime and Audio Doctor project icon"
}

verify_app_signature() {
    local app_bundle="$1"
    if find "$app_bundle" -name '._*' -type f | grep -q .; then
        echo "AppleDouble metadata files found inside app bundle:"
        find "$app_bundle" -name '._*' -type f | sed -n '1,20p'
        exit 1
    fi
    /usr/bin/codesign --verify --deep --strict --verbose=2 "$app_bundle" >/dev/null
    echo "  [sign] Verified GOODMETER.app"
}

#==============================================================================
# Projucer resave + cache
#==============================================================================
resave_target() {
    local target="$1"
    local jucer_file

    if [ -z "$PROJUCER" ]; then
        echo "Projucer not found. Set PROJUCER or install JUCE Projucer first."
        exit 1
    fi

    case "$target" in
        standalone) jucer_file="$JUCER_STANDALONE" ;;
        plugin)     jucer_file="$JUCER_PLUGIN" ;;
        ios)        jucer_file="$JUCER_IOS" ;;
        *)          echo "Unknown target: $target"; exit 1 ;;
    esac

    echo "  [resave] $jucer_file"
    "$PROJUCER" --resave "$jucer_file" 2>&1 | tail -1
    save_jlcode "$target"
}

#==============================================================================
# Prepare JuceLibraryCode for a target (restore cache or resave)
#==============================================================================
prepare_jlcode() {
    local target="$1"
    if restore_jlcode "$target"; then
        return 0
    fi
    echo "  [cache] No cache for '$target' — running Projucer --resave"
    resave_target "$target"
}

#==============================================================================
# Build functions
#==============================================================================
build_standalone() {
    echo ""
    echo "=========================================="
    echo " Building: macOS Standalone + AU/VST3"
    echo "=========================================="
    prepare_jlcode "standalone"
    local build_dir="$LOCAL_BUILD_ROOT/standalone"
    local objroot="$build_dir/OBJROOT"
    local symroot="$build_dir/SYMROOT"
    local products_dir="$build_dir/Products/Release"
    local app_bundle="$products_dir/GOODMETER.app"
    mkdir -p "$objroot" "$symroot" "$products_dir"
    rm -rf "$app_bundle"
    echo "  [build] Products → $products_dir"
    DEVELOPER_DIR="$DEVELOPER_DIR" "$XCODEBUILD" \
        -project "$XCPROJ_STANDALONE" \
        -scheme "GOODMETER - Standalone Plugin" \
        -configuration Release \
        OBJROOT="$objroot" \
        SYMROOT="$symroot" \
        CONFIGURATION_BUILD_DIR="$products_dir" \
        build 2>&1 | tail -3
    bundle_onnxruntime_for_app "$app_bundle"
    verify_app_signature "$app_bundle"
    echo "  ✓ Standalone build complete"
    echo "  → $app_bundle"
}

build_plugin() {
    echo ""
    echo "=========================================="
    echo " Building: Plugin-only (AU/VST3)"
    echo "=========================================="
    prepare_jlcode "plugin"
    DEVELOPER_DIR="$DEVELOPER_DIR" "$XCODEBUILD" \
        -project "$XCPROJ_PLUGIN" \
        -scheme "GOODMETER - All" \
        -configuration Release \
        build 2>&1 | tail -3
    echo "  ✓ Plugin build complete"
}

build_ios() {
    echo ""
    echo "=========================================="
    echo " Building: iOS (arm64 device)"
    echo "=========================================="
    prepare_jlcode "ios"
    DEVELOPER_DIR="$DEVELOPER_DIR" "$XCODEBUILD" \
        -project "$XCPROJ_IOS" \
        -target "GOODMETER - App" \
        -configuration Release \
        -sdk iphoneos \
        -arch arm64 \
        CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO \
        ONLY_ACTIVE_ARCH=NO \
        2>&1 | tail -3
    echo "  ✓ iOS device build complete"
    echo "  → $PROJECT_DIR/Builds/iOS/build/Release-iphoneos/GOODMETER.app"
}

build_ios_sim() {
    echo ""
    echo "=========================================="
    echo " Building: iOS Simulator (arm64)"
    echo "=========================================="
    prepare_jlcode "ios"
    DEVELOPER_DIR="$DEVELOPER_DIR" "$XCODEBUILD" \
        -project "$XCPROJ_IOS" \
        -target "GOODMETER - App" \
        -configuration Release \
        -sdk iphonesimulator \
        -arch arm64 \
        CODE_SIGN_IDENTITY="" CODE_SIGNING_REQUIRED=NO CODE_SIGNING_ALLOWED=NO \
        CONFIGURATION_BUILD_DIR="$PROJECT_DIR/Builds/iOS/build/Release-iphonesimulator" \
        2>&1 | tail -3
    echo "  ✓ iOS simulator build complete"
    echo "  → $PROJECT_DIR/Builds/iOS/build/Release-iphonesimulator/GOODMETER.app"
}

#==============================================================================
# Main
#==============================================================================
case "${1:-help}" in
    standalone)
        build_standalone
        ;;
    plugin)
        build_plugin
        ;;
    ios)
        build_ios
        ;;
    ios-sim)
        build_ios_sim
        ;;
    all)
        build_standalone
        build_plugin
        build_ios
        build_ios_sim
        echo ""
        echo "=========================================="
        echo " All targets built successfully!"
        echo "=========================================="
        ;;
    resave)
        target="${2:-}"
        if [ -z "$target" ]; then
            echo "Usage: ./build.sh resave <standalone|plugin|ios>"
            exit 1
        fi
        echo "Resaving $target..."
        resave_target "$target"
        echo "Done."
        ;;
    help|*)
        echo "GOODMETER Build Script"
        echo ""
        echo "Usage: ./build.sh <target>"
        echo ""
        echo "Targets:"
        echo "  standalone   Build macOS Standalone + AU/VST3"
        echo "  plugin       Build Plugin-only (AU/VST3)"
        echo "  ios          Build iOS device (arm64)"
        echo "  ios-sim      Build iOS Simulator (arm64)"
        echo "  all          Build all targets safely"
        echo "  resave <t>   Just resave a .jucer (standalone|plugin|ios)"
        echo ""
        echo "The script automatically manages JuceLibraryCode snapshots"
        echo "so targets never interfere with each other."
        ;;
esac
