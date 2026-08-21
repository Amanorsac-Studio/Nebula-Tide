# Nebula Tide — macOS & iOS/iPadOS Build Guide

The codebase already supports Apple platforms; the only requirement is that the
build itself runs on a Mac (Apple's rule — Xcode does not exist for Windows).

## What you need (one-time)

1. **A Mac** — any Apple-Silicon Mac works, including a base Mac mini.
   No Mac? Rent one: MacStadium or MacinCloud (~$30–50/month), or use a free
   GitHub Actions macOS runner for the builds (ask Claude to set up the workflow).
2. **Xcode** — free, from the Mac App Store. Open it once to accept the license.
3. **Apple Developer Program** — $99/year at https://developer.apple.com/programs/
   Required for: signing the Mac app so it opens without warnings (notarization),
   and for putting the iPad app on the App Store (incl. TestFlight betas).
4. **CMake** on the Mac: `brew install cmake` (install Homebrew from https://brew.sh first).

## Get the project onto the Mac

Copy the whole `C:\Drone Pad` folder (minus `build/`, which is Windows-specific)
— zip it, or push it to a private GitHub repo and clone it on the Mac.

## macOS build (app + VST3 + AU)

```bash
cd "Drone Pad"
cmake -S . -B build-mac -G Xcode
cmake --build build-mac --config Release
```

Outputs (in `build-mac/NebulaTide_artefacts/Release/`):
- `Standalone/Nebula Tide.app`
- `VST3/Nebula Tide.vst3`   → copy to `/Library/Audio/Plug-Ins/VST3/`
- `AU/Nebula Tide.component` → copy to `/Library/Audio/Plug-Ins/Components/`
  (this is what Logic Pro and GarageBand use)

Put the sound library at **`/Library/Application Support/Nebula Tide/presets`**
(same folder contents as `C:\ProgramData\Nebula Tide\presets` on Windows) —
the app/plugins find it there automatically.

To distribute the Mac app outside the App Store, sign and notarize it:
```bash
codesign --deep --force --options runtime \
  --sign "Developer ID Application: YOUR NAME (TEAMID)" "Nebula Tide.app"
ditto -c -k --keepParent "Nebula Tide.app" NebulaTide-mac.zip
xcrun notarytool submit NebulaTide-mac.zip --keychain-profile "notary" --wait
xcrun stapler staple "Nebula Tide.app"
```

## iOS / iPadOS build (standalone app + AUv3 plugin)

```bash
cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0
open build-ios/NebulaTide.xcodeproj
```

In Xcode:
1. Select the **NebulaTide_Standalone** target → *Signing & Capabilities* →
   set your Team (your $99 account). Do the same for the **AUv3** target.
2. iOS has no shared presets folder — the sound library must ship inside the
   app. Drag the `presets` folder into the Xcode project and tick
   "Copy items if needed" + add to both targets as a folder reference
   (blue folder icon), OR ask Claude to add the iOS resource-bundling step to
   CMake before you copy the project over.
3. Pick a connected iPad (or an iPad simulator) and press Run to test.
   Touch works out of the box — planets, stars, and knobs are all drag/tap.

To ship it:
1. Product → Archive → Distribute App → App Store Connect.
2. Create the app entry at https://appstoreconnect.apple.com (name, description,
   screenshots — the `marketing/` package has copy and a screenshot plan).
3. Use **TestFlight** first: invite testers by email, instant distribution,
   no review wait for internal testers.
4. Submit for review (typically 1–3 days) → App Store.

## Gotchas to expect

- **AUv3 is the iPad plugin format** — it runs inside GarageBand, Logic for
  iPad, AUM, Cubasis. The standalone iPad app and the AUv3 build together.
- The beta-expiry flag works on all platforms:
  add `-DNEBULA_BETA_EXPIRY=YYYY-MM-DD` to any cmake configure for test builds.
- The **Forge editor and NebulaConvert stay on your PC** — they are authoring
  tools; you author once and copy the presets folder to each platform.
- If Logic doesn't see the AU immediately: `killall -9 AudioComponentRegistrar`
  or reboot; first AU validation can be slow.
