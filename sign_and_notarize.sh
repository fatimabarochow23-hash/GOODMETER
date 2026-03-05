#!/bin/bash
# GOODMETER V1.0.0 - Code Signing, Packaging & Notarization
# One-click: Build → Sign → Package PKG → Notarize → Staple
# Includes: VST3 + AU (Component) + Standalone App

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

echo -e "${BLUE}════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}   GOODMETER V1.0.0 - Sign, Package & Notarize${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════${NC}"
echo ""

# === CONFIGURATION ===
PROJECT_DIR="/Users/MediaStorm/Desktop/GOODMETER"
BUILD_DIR="$PROJECT_DIR/Builds/MacOSX/build/Release"
APP_IDENTITY="Developer ID Application: Yiyang Cai (33NJKA4738)"
PKG_IDENTITY="Developer ID Installer: Yiyang Cai (33NJKA4738)"
TEAM_ID="33NJKA4738"
APPLE_ID="bidagosila@icloud.com"
NOTARIZATION_PROFILE="GOODMETER-Notarization"
VERSION="1.0.0"
BUNDLE_ID_BASE="com.solaris.GOODMETER"

VST3_NAME="GOODMETER.vst3"
AU_NAME="GOODMETER.component"
APP_NAME="GOODMETER.app"

VST3_PATH="$BUILD_DIR/$VST3_NAME"
AU_PATH="$BUILD_DIR/$AU_NAME"
APP_PATH="$BUILD_DIR/$APP_NAME"

PKG_BUILD_DIR="/tmp/GOODMETER_PKG"
FINAL_PKG="$PROJECT_DIR/GOODMETER_Installer_v${VERSION}.pkg"

# =========================================================================
# STEP 1: BUILD RELEASE
# =========================================================================
echo -e "${YELLOW}[1/7]${NC} Building Release (all targets)..."
cd "$PROJECT_DIR/Builds/MacOSX"

if [ -d "build" ]; then
    echo "  -> Cleaning previous build..."
    rm -rf build
fi

echo "  -> Compiling..."
/Applications/Xcode.app/Contents/Developer/usr/bin/xcodebuild \
    -project GOODMETER.xcodeproj \
    -scheme "GOODMETER - All" \
    -configuration Release \
    -jobs $(sysctl -n hw.ncpu) \
    clean build \
    CODE_SIGN_IDENTITY="$APP_IDENTITY" \
    DEVELOPMENT_TEAM="$TEAM_ID" \
    2>&1 | tail -5

for f in "$VST3_PATH" "$AU_PATH" "$APP_PATH"; do
    if [ ! -e "$f" ]; then
        echo -e "${RED}x Build failed: $f not found${NC}"
        exit 1
    fi
done
echo -e "${GREEN}v Build successful (VST3 + AU + App)${NC}"
echo ""

# =========================================================================
# STEP 2: CODE SIGN ALL BUNDLES
# =========================================================================
echo -e "${YELLOW}[2/7]${NC} Code signing all plugin bundles..."

sign_bundle() {
    local path=$1
    local bid=$2
    echo "  -> Signing $(basename "$path") ..."

    xattr -cr "$path"

    # Sign nested binaries
    find "$path" -type f \( -name "*.dylib" -o -perm +111 \) | while read file; do
        codesign --force --sign "$APP_IDENTITY" \
            --timestamp --options runtime "$file" 2>/dev/null || true
    done

    # Sign main bundle
    codesign --force --sign "$APP_IDENTITY" \
        --timestamp --options runtime --deep \
        --identifier "$bid" "$path"

    # Verify
    codesign --verify --deep --strict --verbose=2 "$path"
    echo -e "     ${GREEN}v $(basename "$path") signed${NC}"
}

sign_bundle "$VST3_PATH" "${BUNDLE_ID_BASE}"
sign_bundle "$AU_PATH"   "${BUNDLE_ID_BASE}.AU"
sign_bundle "$APP_PATH"  "${BUNDLE_ID_BASE}.App"

echo -e "${GREEN}v All bundles signed${NC}"
echo ""

# =========================================================================
# STEP 3: CREATE PKG INSTALLER
# =========================================================================
echo -e "${YELLOW}[3/7]${NC} Creating installer package..."

rm -rf "$PKG_BUILD_DIR"
mkdir -p "$PKG_BUILD_DIR/payload/Library/Audio/Plug-Ins/VST3"
mkdir -p "$PKG_BUILD_DIR/payload/Library/Audio/Plug-Ins/Components"
mkdir -p "$PKG_BUILD_DIR/payload/Applications"
mkdir -p "$PKG_BUILD_DIR/scripts"
mkdir -p "$PKG_BUILD_DIR/resources"

# Copy all formats
cp -R "$VST3_PATH" "$PKG_BUILD_DIR/payload/Library/Audio/Plug-Ins/VST3/"
cp -R "$AU_PATH"   "$PKG_BUILD_DIR/payload/Library/Audio/Plug-Ins/Components/"
cp -R "$APP_PATH"  "$PKG_BUILD_DIR/payload/Applications/"

# Welcome screen
cat > "$PKG_BUILD_DIR/resources/welcome.html" << 'HTMLEOF'
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; font-size: 13px; line-height: 1.6; color: #333; margin: 20px; }
        h1 { font-size: 24px; font-weight: 600; }
        .version { color: #666; font-size: 12px; margin-bottom: 20px; }
        .feature { margin: 8px 0; padding-left: 20px; }
    </style>
</head>
<body>
    <h1>GOODMETER v1.0.0</h1>
    <div class="version">Professional Audio Metering Suite</div>
    <p><strong>GOODMETER</strong> is a comprehensive audio metering plugin featuring:</p>
    <div class="feature">LEVELS - EBU R128 / Streaming loudness metering</div>
    <div class="feature">VU METER - Classic analog VU needle</div>
    <div class="feature">SPECTRUM - FFT spectrum analyzer</div>
    <div class="feature">3-BAND - Chemical flask tri-band visualization</div>
    <div class="feature">PHASE - Phase correlation oscilloscope</div>
    <div class="feature">STEREO - LRMS cylinders + Goniometer</div>
    <div class="feature">SPECTROGRAM - Waterfall spectrogram</div>
    <div class="feature">PSR - Peak-to-Short-Term ratio meter</div>
    <div class="feature">NONO - AI companion with offline loudness analysis</div>
    <p style="margin-top: 15px;">This installer will place plugins in system audio folders.</p>
    <p><strong>Formats:</strong> VST3 + Audio Unit (AU) + Standalone App</p>
</body>
</html>
HTMLEOF

# License
cat > "$PKG_BUILD_DIR/resources/license.rtf" << 'RTFEOF'
{\rtf1\ansi\ansicpg936\cocoartf2761
{\fonttbl\f0\fswiss\fcharset0 Helvetica;}
{\colortbl;\red255\green255\blue255;}
\paperw11900\paperh16840\margl1440\margr1440
\f0\fs28 \cf0 GOODMETER SOFTWARE LICENSE AGREEMENT\
\
Version 1.0.0\
Copyright \u169 2025 Solaris. All rights reserved.\
\
This software is provided "as is" without warranty of any kind.\
\
By installing this software, you agree to:\
\
1. You may use this software for personal and commercial audio production.\
2. You may not reverse engineer, decompile, or disassemble this software.\
3. Redistribution is prohibited without written permission.\
\
Support: bidagosila@icloud.com}
RTFEOF

# Conclusion screen
cat > "$PKG_BUILD_DIR/resources/conclusion.html" << 'HTMLEOF'
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, sans-serif; font-size: 13px; line-height: 1.6; color: #333; margin: 20px; }
        h1 { font-size: 24px; color: #4CAF50; }
        .path { background: #f5f5f5; padding: 8px 12px; border-radius: 4px; font-family: monospace; font-size: 11px; margin: 5px 0; }
    </style>
</head>
<body>
    <h1>Installation Complete!</h1>
    <p><strong>GOODMETER</strong> has been installed successfully.</p>
    <h3>Installed locations:</h3>
    <div class="path">/Library/Audio/Plug-Ins/VST3/GOODMETER.vst3</div>
    <div class="path">/Library/Audio/Plug-Ins/Components/GOODMETER.component</div>
    <div class="path">/Applications/GOODMETER.app</div>
    <h3>Next steps:</h3>
    <p>1. Restart your DAW (REAPER, Logic Pro, Ableton Live, etc.)</p>
    <p>2. Load GOODMETER from the plugin list</p>
    <p style="margin-top: 20px; color: #666;">Thank you for using GOODMETER!</p>
</body>
</html>
HTMLEOF

# Postinstall script
cat > "$PKG_BUILD_DIR/scripts/postinstall" << 'SHEOF'
#!/bin/bash
chmod -R 755 "/Library/Audio/Plug-Ins/VST3/GOODMETER.vst3" 2>/dev/null || true
chmod -R 755 "/Library/Audio/Plug-Ins/Components/GOODMETER.component" 2>/dev/null || true
chmod -R 755 "/Applications/GOODMETER.app" 2>/dev/null || true
xattr -cr "/Library/Audio/Plug-Ins/VST3/GOODMETER.vst3" 2>/dev/null || true
xattr -cr "/Library/Audio/Plug-Ins/Components/GOODMETER.component" 2>/dev/null || true
xattr -cr "/Applications/GOODMETER.app" 2>/dev/null || true
exit 0
SHEOF
chmod +x "$PKG_BUILD_DIR/scripts/postinstall"

echo -e "${GREEN}v Package structure created${NC}"
echo ""

# =========================================================================
# STEP 4: BUILD PKG
# =========================================================================
echo -e "${YELLOW}[4/7]${NC} Building installer package..."

# Component package
pkgbuild --root "$PKG_BUILD_DIR/payload" \
    --identifier "$BUNDLE_ID_BASE" \
    --version "$VERSION" \
    --scripts "$PKG_BUILD_DIR/scripts" \
    --install-location "/" \
    "$PKG_BUILD_DIR/GOODMETER-component.pkg"

# Distribution XML
cat > "$PKG_BUILD_DIR/distribution.xml" << DISTEOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>GOODMETER v${VERSION}</title>
    <organization>com.solaris</organization>
    <domains enable_localSystem="true"/>
    <options customize="never" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <welcome file="welcome.html" mime-type="text/html"/>
    <license file="license.rtf"/>
    <conclusion file="conclusion.html" mime-type="text/html"/>
    <choices-outline>
        <line choice="default">
            <line choice="${BUNDLE_ID_BASE}"/>
        </line>
    </choices-outline>
    <choice id="default"/>
    <choice id="${BUNDLE_ID_BASE}" visible="false">
        <pkg-ref id="${BUNDLE_ID_BASE}"/>
    </choice>
    <pkg-ref id="${BUNDLE_ID_BASE}" version="${VERSION}" onConclusion="none">GOODMETER-component.pkg</pkg-ref>
</installer-gui-script>
DISTEOF

# Product package
productbuild --distribution "$PKG_BUILD_DIR/distribution.xml" \
    --resources "$PKG_BUILD_DIR/resources" \
    --package-path "$PKG_BUILD_DIR" \
    "$PKG_BUILD_DIR/GOODMETER_unsigned.pkg"

echo -e "${GREEN}v Package built${NC}"
echo ""

# =========================================================================
# STEP 5: SIGN PKG
# =========================================================================
echo -e "${YELLOW}[5/7]${NC} Signing installer package..."

productsign --sign "$PKG_IDENTITY" \
    "$PKG_BUILD_DIR/GOODMETER_unsigned.pkg" \
    "$FINAL_PKG"

pkgutil --check-signature "$FINAL_PKG"
echo -e "${GREEN}v Package signed${NC}"
echo ""

# =========================================================================
# STEP 6: NOTARIZE
# =========================================================================
echo -e "${YELLOW}[6/7]${NC} Submitting to Apple for notarization..."

# Check if credentials are stored
if ! xcrun notarytool history --keychain-profile "$NOTARIZATION_PROFILE" > /dev/null 2>&1; then
    echo -e "${YELLOW}Notarization credentials not found. Setting up...${NC}"
    echo "Please enter your App-Specific Password for $APPLE_ID:"
    read -s app_password
    echo ""
    xcrun notarytool store-credentials "$NOTARIZATION_PROFILE" \
        --apple-id "$APPLE_ID" \
        --team-id "$TEAM_ID" \
        --password "$app_password"
    echo -e "${GREEN}v Credentials stored${NC}"
fi

echo "  -> Uploading to Apple..."
xcrun notarytool submit "$FINAL_PKG" \
    --keychain-profile "$NOTARIZATION_PROFILE" \
    --wait

# Check result
LAST_ID=$(xcrun notarytool history --keychain-profile "$NOTARIZATION_PROFILE" 2>/dev/null | grep -m1 'id:' | awk '{print $2}')
STATUS=$(xcrun notarytool info "$LAST_ID" --keychain-profile "$NOTARIZATION_PROFILE" 2>/dev/null | grep 'status:' | awk '{print $2}')

if [ "$STATUS" = "Accepted" ]; then
    echo -e "${GREEN}v Notarization accepted!${NC}"
    echo ""

    # =========================================================================
    # STEP 7: STAPLE
    # =========================================================================
    echo -e "${YELLOW}[7/7]${NC} Stapling notarization ticket..."
    xcrun stapler staple "$FINAL_PKG"
    xcrun stapler validate "$FINAL_PKG"
    echo -e "${GREEN}v Ticket stapled${NC}"
else
    echo -e "${RED}x Notarization status: $STATUS${NC}"
    echo "  View log: xcrun notarytool log $LAST_ID --keychain-profile $NOTARIZATION_PROFILE"
fi

# =========================================================================
# CLEANUP & SUMMARY
# =========================================================================
rm -rf "$PKG_BUILD_DIR"

echo ""
echo -e "${BLUE}════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}v GOODMETER v${VERSION} - Process Complete${NC}"
echo -e "${BLUE}════════════════════════════════════════════════════════${NC}"
echo ""
echo "Installer: $FINAL_PKG"
ls -lh "$FINAL_PKG"
echo ""
echo "Contents:"
echo "  VST3:       /Library/Audio/Plug-Ins/VST3/GOODMETER.vst3"
echo "  AU:         /Library/Audio/Plug-Ins/Components/GOODMETER.component"
echo "  Standalone: /Applications/GOODMETER.app"
echo ""
echo -e "${GREEN}v Code Signed (Developer ID Application)${NC}"
echo -e "${GREEN}v Package Signed (Developer ID Installer)${NC}"
if [ "$STATUS" = "Accepted" ]; then
    echo -e "${GREEN}v Notarized by Apple${NC}"
    echo -e "${GREEN}v Stapled - Ready for distribution${NC}"
fi
echo ""
echo -e "${BLUE}════════════════════════════════════════════════════════${NC}"
