# GOODMETER Audio Doctor notarized macOS build

- Recommended file: `GOODMETER_AudioDoctor_Notarized_v1.0.2_macOS_20260506.dmg`
- App version: `1.0.2`
- Bundle identifier: `com.solaris.GOODMETER`
- Apple notarization submission: `33532d05-bad3-4ec9-b9c6-43aa8159458a`
- SHA-256: `8c155fb9d4381c5e39f2c83b4dac6aca541d3f9e1b6503c0482b2d1f879b91f6`

This disk image contains the stapled standalone `GOODMETER.app` build with the Audio Doctor workflow included. macOS Gatekeeper verification reports `source=Notarized Developer ID`. The app is signed with hardened runtime entitlements for Audio Doctor plugin hosting, including `com.apple.security.cs.disable-library-validation`.

The older zip artifact is retained for traceability only. Use the DMG for sharing and testing on other Macs.
