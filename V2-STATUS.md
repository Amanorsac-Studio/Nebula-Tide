# Nebula Tide v2 — status and open decisions

Branch `v2`. `main` is untouched at v1.3.0.
Written 9 September 2026, so a new session can pick this up cold.

---

## Decisions waiting on you

**1. Is v2 free or paid?**
The app currently opens on an activation screen and cannot be used without a
licence key. That is the CMake option `NEBULA_REQUIRE_LICENSE`, default `ON`.
Set it `OFF` for a free build — the licence client stays compiled in either
way, only the gate changes. Nothing else needs touching.

**2. The app id is registered as `nebulatide`, keys prefixed `NEBU-`.**
The studio still needs: that app id, the display name, the price, and the
platforms shipped, plus the §9 acceptance report. Nothing has been sent.

**3. `stream.flac` and `stream-tameiga.flac` are in `presets-archive-by-nc/`.**
Both were removed because one of them is *Pedreira Stream* (CC BY-NC, not
licensable for a commercial product) and the two could not be told apart. If
you identify which is which, the CC0 one can come back.

**4. Two files were left out of the new presets:**
`Preset 2 - A Output Audio Bus L.aif` and the Preset 3 equivalent. They are
different audio from the matching `- A.aif` files, so they look like bounce
strays — but worth confirming the A stems that shipped are the intended ones.

---

## Where things live now

The repo moved to `C:\Amanorsac Studio\products\nebula-tide\`, beside the other
products. CMake bakes absolute paths into its cache, so `build/` was wiped and
reconfigured from scratch; anything holding an old path needs the same.

Runtime data is `Documents\Amanorsac Studio\Nebula Tide\` — `User\` for saved
presets, `Library\` for user presets, `library-path.txt` for the pointer. The
old `%APPDATA%\Nebula Tide` contents are copied across once on first run.
Licence proofs stay in `%LOCALAPPDATA%`: they are DPAPI-encrypted and bound to
one machine, and Documents is commonly synced to OneDrive.

`NEBULA_REQUIRE_LICENSE` is currently **OFF** in the CMake cache, left as it was
for DAW testing. It is decision 1 below and has not been made.

---

## Cannot be built on this Windows machine

| Target | Blocker |
|---|---|
| macOS, iOS, iPadOS | needs a Mac with Xcode |
| Android | needs Projucer + Android SDK/NDK, or CI |
| Windows installer | Inno Setup is not installed; `installer/NebulaTide.iss` is written and ready |
| Licence tests A1–A10 | need a real key against the live server |

A11 and A12 pass: exactly one occurrence of the studio signing key, live base
URL, no dev fallback in the licence path.

---

## Measured facts, so nobody re-derives them

- desktop library, FLAC: **264 MB** (was 396 MB when stored as raw PCM)
- mobile library, Ogg q7: **101 MB**
- 101 MB is inside Google Play's 150 MB cap, so **Android needs no asset
  packs** — sounds ship inside the app. iOS has no equivalent cap.
- 43 tests, `build/NebulaTest_artefacts/Release/NebulaTest.exe`

Repack with:

```
build/NebulaPack_artefacts/Release/NebulaPack.exe presets NebulaTide.ntlib
build/NebulaPack_artefacts/Release/NebulaPack.exe presets mobile-q7.ntlib --ogg 7
build/NebulaPack_artefacts/Release/NebulaPack.exe --verify NebulaTide.ntlib
```

`verify` must report `audio=105 bad=0`. A lower audio count means files are
being skipped — that has happened twice, silently, and both times the entry
count was the only sign.

---

## Testing this build

The app reads the library from the folder named in
`Documents\Amanorsac Studio\Nebula Tide\library-path.txt`, currently pointing at
`Desktop\NebulaTide-v2-TEST\Sounds` because replacing the copy in
`C:\ProgramData\Nebula Tide` needs admin rights. USE DEFAULT in Settings goes
back to the ProgramData one, which is still the v1 library.

Running the standalone from `build\NebulaTide_artefacts\Release\Standalone`
reads `presets/` straight off disk instead, which avoids repacking while
iterating on sounds.

---

## Not yet done by anyone

- the rebuilt shimmer has never been listened to on the current build
- Studio's SAVE button has never been clicked
- drag-to-timeline has never been tried in a DAW
- the "blank screen after install" report was never diagnosed; ask an affected
  user for the diagnostics text the not-found panel shows
