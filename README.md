# Nebula Tide

Ambient drone pad player — deep sea, deep space. One JUCE/C++ codebase producing:

- **Windows standalone app** (building now)
- **VST3 plugin** (same build)
- **macOS app / AU** (same code, build on a Mac)
- **iPad (AUv3)** (same code, build via Xcode)

## How it works

- Drop your drone pad audio files (`.wav`, `.mp3`, `.ogg`, `.flac`, `.aiff`) into `presets/`
  and rebuild — they get **embedded into the binary** and appear as glowing pads.
- Alternatively, a `presets` folder placed next to the built executable is scanned at
  launch (no rebuild needed — handy for testing).
- Pads play at original tempo/key, **loop seamlessly** (sample-accurate wrap), and
  **crossfade** smoothly when you switch pads (crossfade time adjustable, 0.5–12 s).
- Controls: Volume, Pan (constant-power), Reverb wet mix, Crossfade time.
- The UI is a living sea-blue nebula: drifting stars, rotating aurora arcs, and a glow
  that breathes with the audio output level.

## Building

Requires Visual Studio 2026 (C++ workload) — CMake is bundled with VS.

```bash
cmake -S . -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Release
```

Outputs:
- `build/NebulaTide_artefacts/Release/Standalone/Nebula Tide.exe`
- `build/NebulaTide_artefacts/Release/VST3/Nebula Tide.vst3`

## Structure

- `CMakeLists.txt` — build config; auto-embeds everything in `presets/`
- `src/PluginProcessor.*` — audio engine: loop playback, crossfade voices, reverb, pan
- `src/PluginEditor.*` — UI: nebula background, pad grid, knobs/sliders
- `presets/` — your drone pad audio (currently contains a generated test pad, `Deep_Current.wav` — replace with your real pads)
