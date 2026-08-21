# NEBULA TIDE — Complete Product Marketing Package

> **Purpose of this document:** everything a designer, copywriter, or another Claude Code instance needs to build an award-winning marketing website for Nebula Tide **without asking a single follow-up question**. Screenshots live in `./screenshots/`. The machine-readable handoff is in `./handoff.json` and duplicated in the appendix.

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Product Positioning](#2-product-positioning)
3. [Elevator Pitches](#3-elevator-pitches)
4. [Feature Inventory](#4-feature-inventory)
5. [User Journey](#5-user-journey)
6. [UI Analysis](#6-ui-analysis)
7. [Screenshot Plan](#7-screenshot-plan)
8. [Animation Plan](#8-animation-plan)
9. [Product Photography Direction](#9-product-photography-direction)
10. [Landing Page Content](#10-landing-page-content)
11. [Visual Style Guide](#11-visual-style-guide)
12. [Competitive Analysis](#12-competitive-analysis)
13. [Assets Needed](#13-assets-needed)
14. [Image Generation Prompts](#14-image-generation-prompts)
15. [Motion Storyboard](#15-motion-storyboard)
16. [Landing Page Structure](#16-landing-page-structure)
17. [Developer Handoff JSON](#17-developer-handoff-json)
18. [Checklists](#18-checklists)

---

## 1. Executive Summary

**What it is.** Nebula Tide is a desktop drone-pad instrument (Windows standalone + VST3, macOS and iPad planned) that plays hand-crafted ambient drone pads in any of the 12 musical keys, loops them endlessly and seamlessly, and layers them with atmospheric FX and textures — all inside a living, sea-blue deep-space interface that breathes with the sound.

**Why it exists.** Worship musicians, ambient producers, sound designers, meditation-space creators, and live performers all need the same thing: an *instant bed of sound* in the right key that never clicks, never ends, and never needs a manual. Existing tools are either full synthesizers (too complex), sample players (too generic), or looped YouTube videos (unprofessional, wrong key, ads).

**The problem it solves.** Getting a beautiful, correctly-keyed, endlessly-sustaining pad under your music takes one click — not a synth patch, not a DAW project, not a search for "pad in E ambient 1 hour."

**Who it's for.** Primary: worship keyboardists & music directors, ambient/electronic producers, live performers. Secondary: yoga/meditation instructors, film/TV composers needing instant beds, podcasters and streamers.

**Why it's different.** Every preset is a *real recording captured in all 12 keys* — never pitch-shifted, so the harmonic character stays true in every key. The key selector is a ring of floating planets. The whole UI re-colors itself to match the selected preset. It feels less like software and more like a place.

**Emotional value.** Calm, immersion, awe. The user opens Nebula Tide and enters a space that is already breathing. It removes performance anxiety ("my pad is handled") and replaces it with atmosphere.

**Business value.** A content platform: the app is the player, preset packs are the product line. New packs ship without app updates (folder-based preset system). Email capture and update channels are architected for a direct-to-musician funnel.

---

## 2. Product Positioning

| Element | Definition |
|---|---|
| **Mission** | Give every musician an endless, beautiful bed of sound in one click. |
| **Vision** | Become the default ambient instrument on every worship stage and in every ambient studio — one codebase from desktop to iPad. |
| **Target market** | Worship/CCM musicians (est. 1M+ worship teams globally), ambient & electronic producers, live performers. |
| **Primary users** | Worship keyboardists, music directors, ambient producers. |
| **Secondary users** | Meditation/yoga facilitators, composers, streamers, sound designers. |
| **USP** | Real recordings in all 12 keys — never pitch-shifted — inside a living interface that plays like an instrument, not a plugin. |
| **Brand personality** | Cosmic, serene, precise, alive. A lighthouse in deep water/deep space. |
| **Brand voice** | Calm confidence. Short sentences. Imagery of ocean and space. Never hypey, never technical jargon first. |
| **Tone** | Reverent but modern; premium but warm. |
| **Core values** | Sonic honesty (no pitch-shifting), instant playability, beauty as function, ship-simple (no manuals). |

### Personas

1. **"Sunday Keys" — Marcus, 28, worship keyboardist.** Runs Ableton with 40 tracks on Sundays. Needs a pad in the setlist's key *now*; fears silence between songs. Buys preset packs the way guitarists buy pedals.
2. **"Late Orbit" — Yuki, 34, ambient producer.** Sound quality obsessive; despises pitch-shifted samples ("you can hear the formants smear"). The 12-real-keys claim is what converts them.
3. **"Still Waters" — Dana, 41, meditation studio owner.** Not a musician. Wants one button → beautiful endless sound → volume knob. The standalone app is their whole use case.

---

## 3. Elevator Pitches

**10-second pitch.** Nebula Tide plays endless, seamless drone pads — real recordings in all 12 keys — inside an interface that feels alive. One click, infinite atmosphere.

**30-second pitch.** Nebula Tide is a drone-pad instrument for worship musicians and ambient producers. Every preset is a real recording captured in all twelve keys — never pitch-shifted — looping seamlessly forever. Switch keys by tapping floating planets; layer in wind, rain, or crowd FX with a click; shape the space with Room, Plate, or Hall reverb. It runs standalone on Windows or as a VST3 in your DAW, and the whole interface re-colors itself to match the sound you choose.

**2-minute pitch.** (Combine the above with:) Most pad solutions force a trade-off: synths demand programming, sample loops end and click, and pitch-shifting one recording across keys smears its harmonic soul. Nebula Tide refuses the trade-off. Each preset ships as twelve distinct studio recordings — one per key — so E♭ sounds as intentional as C. The engine crossfades between pads, keys, and even preset switches so sound never breaks. Two "stars" flank the stage: FX (claps, prayers, swells) and Textures (wind, rain, birds) that persist across preset changes and fade out gracefully with the drone. Under the hood: a JUCE C++ engine, sample-accurate seamless looping, loudness-normalized FLAC content pipeline, and a folder-based preset system that lets us ship new sound packs without app updates. The roadmap runs from today's Windows standalone + VST3 to macOS and iPad from the same codebase — and preset packs are the recurring product line.

**Investor pitch.** Nebula Tide is Splice-meets-instrument for the fastest-growing niche in music tech: worship production. The player is free-to-cheap; the content (12-key preset packs) is the margin. One C++ codebase covers desktop, plugin, and iPad. Content pipeline is fully automated (normalize → FLAC → manifest), so pack production cost approaches studio time only. Comparable: Worship pads sellers doing 7-figure revenue on static WAV downloads with zero product experience — we wrap that market in an instrument they'll open every day.

**Website hero statement.** *An ocean of sound. A universe of calm. Endless drone pads, in every key, alive at your fingertips.*

**App Store description (short).** Endless ambient drone pads in all 12 keys — real recordings, never pitch-shifted. Tap a planet to change key. Layer wind, rain, and FX. Shape the space with Room, Plate, and Hall reverb. The interface breathes with your sound.

**Google Play description.** Nebula Tide turns your device into an endless ambient instrument. Choose a preset, tap a floating planet to pick your key, and a seamless drone pad fills the room — a real recording in that key, never pitch-shifted, looping forever without a click. Add textures like rain and wind, trigger FX, and sculpt the space with three reverbs. Built for worship musicians, ambient producers, and anyone who needs beautiful sound *now*.

---

## 4. Feature Inventory

> Priority: **P0** = hero feature (lead marketing), **P1** = supporting, **P2** = delight/detail.

| # | Feature | Purpose | User benefit | Technical description | Why users care | When used | Vs. competitors | Priority | Screenshot | Animation idea |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | **Seamless infinite looping** | Endless pad beds | Sound never ends or clicks | Sample-accurate wrap of fully-loaded buffers; fractional-position linear interpolation resampling | Silence/clicks on stage = fear | Always | Loop libraries click or fade; videos end | **P0** | 02 | Waveform circle closing into an infinity symbol |
| 2 | **12 real keys per preset** | True harmonic character in every key | E♭ sounds recorded, not stretched | 12 discrete audio files per preset (`Name_Key.flac`); no pitch-shifting anywhere in the engine | Producers hear formant smear; worship needs the setlist key | Every session | Competitors pitch-shift one file | **P0** | 01 (planet ring) | 12 planets orbiting; clicked planet flares and the sky re-tunes |
| 3 | **Key planets UI** | Key selection as play, not menu | Changing key feels like touching a solar system | Custom JUCE component; wobble phases, per-planet hue, Saturn rings on every 4th; hover glow; click = crossfade to that key's recording | Demo-able, screenshot-able, joy | Every key change | Everyone else: dropdown | **P0** | 01 | Idle wobble; hover halo; click ripple across the orbit line |
| 4 | **Living, preset-colored UI** | The app feels alive | The whole room changes with the sound | Global theme accents re-derived each frame from preset colour; warp starfield speed follows audio RMS; aurora arcs rotate; pads breathe | Emotional connection; brand | Always | Static plugin panels | **P0** | 01 vs 02 | Full-UI color morph when switching presets |
| 5 | **Crossfaded everything** | No hard sound transitions | Switching pads/keys/presets is musical | Dual-voice engine; adjustable 0.5–12 s equal-gain crossfade; stars fade with the same curve on stop | Live use demands smoothness | Transitions | Most samplers hard-cut | **P1** | — | A/B waveforms dissolving into each other |
| 6 | **FX & Texture stars** | One-click atmosphere layers | Rain, wind, birds, claps, prayers on top of the pad | Two independent aux voices; one-shot or ∞ loop (default on); survive preset changes; fade out with drone stop; vertical-drag volume on the star | Layered atmospheres sell packs | Builds, intros, ambience | Rare in this category | **P0** | 01 (both stars) | Star pulses on trigger; rays extend while playing |
| 7 | **Room / Plate / Hall reverb** | Instant space shaping | Three characters, three clicks, still tweakable | juce::Reverb tuned per category (size/damp/width curves modeled on ValhallaRoom / EMT-140 plate / VintageVerb hall characters); MIX/SIZE/DAMP editable; per-preset defaults from manifest | "Which reverb?" answered by design | Sound design moments | Plugins give 40 knobs or none | **P1** | 01 (SPACE section) | Room→Plate→Hall morphing wireframe room |
| 8 | **Single-preset stage** | Focus | One glowing square = zero cognitive load | One pad visible at a time; `<` `>` navigate; playing state hot-swaps with crossfade | Calm software for calm music | Browsing | Grids overwhelm | **P1** | 01 | Preset square slides/dissolves on navigation |
| 9 | **Nebula Forge (authoring backend)** | Build presets without code | Creator uploads 12 keys, names, colors, reverb — done | Local Node server + browser UI; drag-drop per-key slots; waveform render; loop audition; writes manifest.json | (B2B/internal + power users) | Pack creation | Nobody ships an authoring tool | **P2** | 03 (to capture) | Waveform drawing itself after drop |
| 10 | **Auto-normalize + FLAC pipeline** | Consistent loudness, small files | Every sound lands at −24 dB RMS; library is half the size | NebulaConvert.exe (JUCE CLI): RMS analysis, −1 dBFS peak ceiling, FLAC compression level 5 | Pros notice level jumps | Invisible | Manual mastering elsewhere | **P2** | — | File shrinking animation 105 MB → 30 MB |
| 11 | **Folder-based preset packs** | Content without updates | New packs drop in, app just sees them | `presets/` scanned at launch; disk overrides embedded; manifest carries name/colour/reverb | Buy pack → drop folder → play | Pack install | Competitors: installers per pack | **P1** | — | Folder opening, pads flying onto the stage |
| 12 | **VST3 + Standalone** | Fits both workflows | Stage app and studio plugin, same sound | JUCE one-codebase build; shared preset folder | DAW users need the plugin | Studio | Many pad apps are standalone-only | **P1** | — | App window docking into a DAW mockup |
| 13 | **Constant-power pan & live readouts** | Precision without menus | Knob shows 80, C, L50 as you turn | Custom rotary LnF with halo, value arc, centered readout | Feels engineered | Mixing | Generic knobs | **P2** | 01 (knobs) | Knob halo brightening on grab |

---

## 5. User Journey

**First launch.** Double-click → 2 s → a dark ocean-space fills the window, stars already falling, one glowing preset square center stage, "drifting" status in the corner. No dialog, no license nag, no tutorial. The pad square invites the first click. Click → 4-second bloom of sound + the status flips to "transmitting" + the starfield accelerates. *The product teaches itself in one click.*

**Onboarding (implicit).** Hover states reveal everything: planets glow when hovered (keys), stars show `< ∞ >` beneath them, knobs show live values. Total learnable surface: ~90 seconds.

**Daily workflow (worship).** Open app before service → arrow to tonight's preset → tap the key planet for song 1 → volume to taste → play. Between songs: tap next key planet (seamless crossfade in the new key's real recording). Panic moment (transition runs long): tap the texture star — rain fills the gap.

**Advanced workflow (producer).** Load VST3 in the DAW → pick preset/key → record the stereo bed → resample, chop, layer. Reverb set to Plate, MIX low, pad as harmonic glue under a mix.

**Power-user workflow (pack creator).** Record 12 keys → open Forge → new preset → name, color (whole app will wear it), reverb character → drag 12 files → loop-test each → save → restart app → the library grew.

**Exit experience.** Close → instant, silent. State (last preset, key, knob positions) persists via plugin state in DAWs.

**Return experience.** Reopens to the same universe; stars still falling. Familiarity is the feature.

---

## 6. UI Analysis

### Screen: Main stage (the only screen — by design)

| Zone | Components | Details |
|---|---|---|
| **Header** | Wordmark `N E B U L A  T I D E` (letter-spaced, glowing); `<` / `>` preset navigation; preset name + current key (`DEEP CURRENT · Eb`); status word (`drifting` / `transmitting`) | Status word doubles as a live indicator — dim when idle, bright accent + literal meaning when playing |
| **FX star (left)** | Warm-gold 4-point star; name label; `<` `∞` `>` bare-glyph controls | Click = play/stop; drag vertically on the star = volume (arc appears); ∞ lit = looping (default on) |
| **Texture star (right)** | Ice-blue star, same controls | Independent of presets; both fade out when the drone stops |
| **Center stage** | One large rounded-square pad (190 px), preset name centered | Breathing glow when active; whole-UI tint source |
| **Planet ring** | 12 planets on a shallow elliptical orbit line, key labels beneath (C…B) | Wobble idle animation; hover halo; selected = bigger + pulsing; unavailable keys dim; every 4th planet has a Saturn ring |
| **Footer panel** | Translucent rounded panel: VOLUME knob (halo, arc, numeric readout) · SPACE section (ROOM/PLATE/HALL buttons + MIX/SIZE/DAMP sliders) · CROSSFADE slider · PAN knob (readout C/L/R) | Knobs 116 px, dramatic halos; sliders thin with glowing fill and foam-white thumbs |
| **Background** | Warp starfield (170 stars, z-projected, streaks), 3 rotating aurora arcs, center breathing glow | Speed and glow intensity track audio output level |

**Typography.** Monospaced (Cascadia Code family feel), generous letter-spacing (2–8 px), small caps aesthetic via uppercase. **Spacing.** 22–46 px paddings; footer panel radius 20 px; pad radius 14 px. **States.** Idle (drifting), playing (transmitting), empty-library (dashed hint box). **Dark mode.** The app *is* dark mode; there is no light variant (marketing site should also be dark-first). **Responsive.** Window resizable 900×660 → 1920×1200; layout is proportional; planets/pads recenter.

---

## 7. Screenshot Plan

> Files that already exist are marked ✅ (in `./screenshots/`). Others are specified for capture/creation.

| # | Title | File | Purpose | Visible | Framing | Overlays/Callouts | Caption / copy beside | Device frame | Background | Notes |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | Logo | `00-logo.png` ✅ | Brand mark | Star + tide arcs on navy rounded square | 1:1, 512 px | none | — | none | transparent | Also derive favicon/app icon |
| 1 | Main — idle | `01-main-idle.png` ✅ | Hero shot | Full UI: header, both stars w/ real sound names, DEEP CURRENT square, 12 planets, footer controls | Full window 1102×808 | Optional thin accent-line callouts to: planets ("12 real keys"), stars ("living textures"), square ("one-click endless pad") | "Everything you need. Nothing you don't." | Floating window, 24 px radius shadow | Radial navy gradient, subtle noise | The definitive screenshot |
| 2 | Main — playing | `02-main-playing.png` ✅ | Show "alive" | Same UI in transmitting state, lit pad, accelerated starfield | Full window | Motion-blur vignette at edges | "The interface breathes with your sound." | Same | Slightly brighter gradient | Pair with #1 as before/after |
| 3 | Forge editor | `03-forge.png` (to capture) | Creator story | Preset open: color picker, reverb sliders, 12 key slots with waveforms | Browser window, 1440×900 | Callout on a waveform ("see the loop"), on color ("theme the whole app") | "Build a preset in minutes." | Browser chrome, dark | Same navy family | Capture at localhost:8451 with Deep Current selected |
| 4 | Key planet close-up | crop of #1 | Feature detail | Planet ring only, one planet hovered | 1200×400 crop, 2× zoom | Halo highlight on Eb | "Tap a planet. Land in a new key." | none — full-bleed section image | Deep space | Recreate at higher zoom if possible |
| 5 | Reverb section close-up | crop of #1 | Feature detail | SPACE buttons + 3 sliders | 900×300 crop | HALL underlined by accent glow | "Three rooms. One click." | none | Panel texture | — |
| 6 | Theme-shift pair | 2 captures after recoloring a preset in Forge | Prove living UI | Same frame, two wildly different accent colors (cyan vs crimson) | Side-by-side halves | Diagonal split | "Your preset's color becomes the whole room." | none | Split gradient | Requires temporarily changing a preset colour in Forge |
| 7 | VST3 in DAW | to capture (any DAW hosting the plugin) | Studio credibility | Plugin window over a session | 1600×1000 | none | "Stage app. Studio plugin. Same soul." | Laptop mockup | Desk scene or gradient | Optional if no DAW available — use device-frame mockup prompt (§14) |

**Camera/finishing for all:** flat-front captures, no perspective skew for UI truth; drop-shadow (0 24px 80px rgba(0,0,0,.55)); never place UI on white.

---

## 8. Animation Plan

**Hero animation (site).** The real app's starfield recreated in WebGL/canvas: stars falling toward the viewer behind the hero copy; the hero screenshot floats in, its own pad square glowing on a 5 s breathe cycle. On scroll, the starfield parallaxes slower than content (0.3×).

**UI transitions (site).** Section reveals: opacity 0→1 + translateY 24→0, 600 ms, cubic-bezier(0.22,1,0.36,1), staggered 80 ms. Never bounce — this brand glides.

**Feature animations.**
- *12 keys:* the planet ring as an inline SVG/canvas; planets wobble idly; on section-enter, each planet lights in sequence like a scale being played (pair with actual ascending pad notes on user-initiated audio toggle).
- *Seamless loop:* a circular waveform that completes and keeps rotating — no seam flash — with an ∞ symbol materializing at the joint.
- *Crossfade:* two colored waveforms (cyan/violet) dissolving through each other over 4 s.
- *Stars:* the gold star pulses; rain streaks fall behind the ice star.
- *Theme shift:* full-viewport background hue morph as a preset-color swatch row is hovered (this is the "wow" scroll moment).

**Micro-interactions.** Buttons: glow intensifies (box-shadow accent 0→24 px) on hover, 150 ms. Cursor: default; no custom cursor (calm brand). Cards: translateY(-4px) + shadow deepen.

**Loading.** A single star that brightens and emits one tide-arc ripple, looped; site loads fast enough that it should rarely be seen.

**Scroll/parallax.** Three depth layers: starfield (0.2×), aurora arcs (0.5×), content (1×). GSAP ScrollTrigger with scrub for the theme-morph section; Framer Motion for component reveals; Lottie for the ∞-loop and star-pulse marks; Three.js/WebGL justified **only** for the hero starfield (heavier frameworks unnecessary elsewhere).

**Video loops / GIFs.** 6 s loops, ≤4 MB: (a) key-planet tap → crossfade glow, (b) theme color morph, (c) star trigger with texture name flip. Use MP4/WebM over GIF; GIF only as fallback.

---

## 9. Product Photography Direction

- **Lighting:** single soft key from upper-left, cool white (5500 K) with cyan spill; deep falloff to near-black. Practical glow from the screen itself lights any surrounding scene.
- **Composition:** generous negative space (the product is calm); UI window offset right, copy left; horizon lines low.
- **Perspective:** flat-front for truth shots; a single 8° tilt-forward laptop mockup allowed for the DAW/studio scene.
- **Device mockups:** dark titanium/space-gray hardware only; screens at 85% brightness against dark rooms.
- **Depth & blur:** background gradient orbs blurred 120 px; foreground UI always tack-sharp.
- **Glassmorphism:** footer-panel style translucency (rgba(4,24,38,.72) + 8 px blur + 1 px accent border) may be reused for site cards — this is the app's own material, so it reads as authentic, not trendy.
- **Avoid:** neumorphism, white studio sweeps, hands/lifestyle stock, lens flares. Apple-inspired restraint: one hero object, black space, light from the product.

---

## 10. Landing Page Content

**Hero headline:** `An ocean of sound. In every key.`
**Hero subheadline:** `Nebula Tide plays endless, seamless drone pads — real recordings in all 12 keys — inside an interface that's alive. One click, infinite atmosphere.`
**Primary CTA:** `Download for Windows` · **Secondary CTA:** `Hear it in 30 seconds` (plays inline demo — audio products must be heard).

**Feature section (headline + one-liner each):**
- *Twelve keys. Zero pitch-shifting.* Every preset is recorded twelve times — one real performance per key. Your E♭ was never a stretched C.
- *Loops that never end. Or click.* Sample-accurate seamless looping. Leave it running for the whole service, the whole session, the whole night.
- *Touch a planet. Change the sky.* Keys orbit as living planets. Tap one and the pad crossfades into a new tonal world.
- *Two stars. Endless weather.* Rain, wind, birds, crowds, prayers — layered atmospheres that survive preset changes and fade out with the music.
- *Three rooms in one knob row.* Room, Plate, and Hall — tuned to the classics — with Mix, Size, and Damp when you want to go deeper.
- *It wears your sound's color.* Choose a preset and the entire interface — planets, knobs, sky — re-tints to match.

**Benefits (outcome-framed):** Never fear silence on stage · Setlist-ready in seconds · Studio-honest sound in every key · An instrument you'll *want* to open.

**Workflow section:** three numbered steps with screenshots — 1. Pick your preset (arrows) → 2. Tap your key (planet) → 3. Play forever (the square). Caption: "That's the whole manual."

**Comparison section:**

| | Nebula Tide | Pad loop packs | Synth pads | YouTube pads |
|---|---|---|---|---|
| All 12 keys, real recordings | ✅ | rarely | n/a (synthesized) | ❌ |
| Seamless infinite loop | ✅ | often clicks | ✅ | ends/ads |
| One-click FX & textures | ✅ | ❌ | ❌ | ❌ |
| VST3 + standalone | ✅ | files only | plugin only | ❌ |
| Feels alive | ✅ | ❌ | ❌ | ❌ |

**Testimonials — SAMPLE PLACEHOLDERS, clearly marked, replace with real quotes before launch:**
> "It's the first pad tool my whole team actually enjoys using." — *Worship director, placeholder*
> "The 12-key thing isn't marketing. You can hear it." — *Ambient producer, placeholder*

**FAQs:**
1. *Does it work in my DAW?* Yes — VST3 on Windows now; macOS AU/VST3 planned.
2. *Are the pads pitch-shifted?* Never. Twelve real recordings per preset.
3. *Can I add my own sounds?* Preset packs install by dropping a folder — no reinstall.
4. *Will it click when looping?* No — looping is sample-accurate and crossfades protect every transition.
5. *Mac / iPad?* Same codebase, planned next.
6. *How big is it?* App ~10 MB; sound library depends on packs (FLAC, roughly half of WAV size).

**Pricing copy (structure ready; numbers TBD):** *Player* — free, includes the Deep Current preset. *Packs* — one-time purchases, own forever. *Everything bundle* — all current packs + launch-window packs.

**CTA sections:** mid-page after theme-morph moment (`Bring the ocean to your next set — Download free`), footer (`The tide is waiting.` + button).

**Footer copy:** `Nebula Tide — endless ambient instrument. Built by musicians, for the moments between the notes.` + nav (Features · Packs · FAQ · Contact) + legal.

**SEO title:** `Nebula Tide — Endless Drone Pads in All 12 Keys | Windows App & VST3`
**SEO description:** `Play seamless, infinite ambient drone pads — real recordings in every key, never pitch-shifted. FX, textures, and three reverbs in a living interface. Free download for Windows.`
**OG title:** `Nebula Tide — An ocean of sound. In every key.`
**OG description:** `Endless drone pads, living interface, twelve real keys. Hear it in 30 seconds.`
**Meta keywords:** drone pads, worship pads, ambient pads app, pad VST3, seamless pad loops, pads in every key, ambient instrument.

---

## 11. Visual Style Guide

| Token | Value | Usage |
|---|---|---|
| `--bg-deep` | `#020C16` | Page/app background |
| `--bg-mid` | `#04283F` | Gradient upper stop (retinted per theme in-app) |
| `--sea` | `#16B8D8` | Primary accent (default theme) |
| `--sea-bright` | `#4FE3FF` | Bright accent, glows, active states |
| `--foam` | `#BDF3FF` | Primary text, star bodies |
| `--text-dim` | `#6FA8BD` | Secondary text, labels |
| `--panel` | `rgba(4,24,38,0.72)` | Glass panels |
| `--fx-gold` | `#FFC96B` | FX star identity |
| `--ice` | `#BFEAFF` | Texture star identity |
| Danger | `#FF9B9B` | Destructive (Forge delete) |

**Typography:** monospace family (Cascadia Code → fallback Consolas/JetBrains Mono on web); H1 letter-spacing 0.4em uppercase; labels 10–11 px, letter-spacing 0.25em, uppercase, dim color; body 14–16 px normal case.
**Icon style:** geometric line + glow; 4-point stars; no filled blob icons.
**Illustration style:** the app's own visual system — starfields, orbit lines, aurora arcs; never flat "corporate memphis."
**Spacing system:** 4-px base; component paddings 12/18/24/46.
**Radius:** panels 18–22 px, pads 14 px, buttons 8–10 px, pills 999 px.
**Shadows/glow:** ambient `0 24px 80px rgba(0,0,0,.55)`; interactive glow = accent at 25–60% alpha, blur 12–40 px.
**Buttons:** primary = accent gradient fill (#16B8D8→#0E7EA0) with dark text; secondary = transparent + 1 px accent border; text buttons = bare glyphs (as in-app).
**Cards/forms/tables:** glass panel material, 1 px `rgba(79,227,255,.18)` borders, focus = border→bright accent.
**Dark/light:** dark only. Charts (if any): accent scale on dark, no gridlines heavier than `rgba(255,255,255,.06)`.

---

## 12. Competitive Analysis

| Competitor | What it is | Strengths | Weaknesses vs. us |
|---|---|---|---|
| **Worship pad WAV packs** (Coresound, Pad Loops, etc.) | Downloadable long WAV/MP3 loops | Cheap, known market, good recordings | Not an instrument: no seamless engine, no key UI, no FX layers, files end/click, no product experience |
| **Sunday Keys / Ableton templates** | DAW templates for worship | Deep, flexible | Requires DAW mastery; setup-heavy; not calm |
| **Ambient VST synths** (Omnisphere pads, Arturia, Vital patches) | Synthesis | Infinite variety | Complexity wall; synthesized ≠ recorded character; no 12-real-keys story |
| **YouTube "pads in E" videos** | Streamed loops | Free | Ads, wrong keys, ends, unprofessional on stage |

**Visual/marketing gap:** every competitor markets with waveform-and-Bible-verse thumbnails or dense synth panels. Nobody owns *calm premium space*. Nebula Tide's living-interface story (video-first marketing) is undefendable by file-sellers — lean into motion everywhere.

**Why we're unique (one line):** the only pad product where the *recordings* are honest (12 real keys), the *engine* is seamless, and the *experience* is alive.

---

## 13. Assets Needed

**Have (in this folder):** `00-logo.png` (512 px), `01-main-idle.png`, `02-main-playing.png`.

**To produce:**
- SVG logo (rebuild the mark as vector: rounded square, 3 tide arcs, 4-point star) + transparent-background variants (mark only, mark+wordmark horizontal, wordmark only)
- App icon `.ico` (exists inside build as `build/juce_icon.ico` — copy out) + favicon set (16/32/180/512) + maskable PWA icon
- Screenshots #3–#7 per §7
- Device mockups: dark laptop frame (hero), browser frame (Forge)
- Background: starfield still (4K), starfield WebGL/canvas script, aurora-arc SVG set
- Pattern: orbit-line SVG divider
- Icon set (line+glow): planet, star-4pt, infinity, wave-arc, knob, download, key
- Video: 30 s trailer, 6 s feature loops ×3 (§8), OG image 1200×630, social banners (X 1500×500, IG 1080×1080)
- Lottie: ∞ loop-seal animation, star pulse
- Audio: 30 s hero demo mix (pad + texture entrance) — **the most important asset on the page**

---

## 14. Image Generation Prompts

> Production-ready for Midjourney / Flux / Ideogram / Recraft. Always append: `--no text, watermark, people` (or platform equivalent). Palette anchors: deep navy #020C16, cyan #4FE3FF, foam #BDF3FF.

1. **Hero background.** "Vast deep-space starfield falling toward the viewer with subtle motion streaks, deep navy #020c16 to teal #04283f radial gradient, faint glowing aurora arcs of cyan light curving through the darkness, sparse foam-white stars, cinematic, ultra-clean, no planets, no nebula clouds, minimal, 8k, dark ambient mood"
2. **Feature image — 12 keys.** "Twelve small glowing planets arranged along a gentle elliptical orbit line in deep space, each planet a slightly different shade of teal and blue, one planet glowing brightly cyan with a soft halo, thin Saturn ring on one planet, minimal dark navy background, soft glow, product-grade 3D render, clean composition"
3. **Feature image — endless loop.** "A perfect circular waveform of cyan light closing seamlessly into itself in dark space, a subtle infinity symbol formed at the joining point, glowing line art, deep navy background, elegant, minimal, high contrast"
4. **Feature image — textures/weather.** "Two four-pointed stars facing each other in dark space, left star warm gold emitting tiny clap-like light sparks, right star ice blue with faint rain streaks and drifting mist behind it, deep navy background, minimal, cinematic glow"
5. **Device mockup — laptop.** "Dark space-gray laptop on a black desk in a dim studio, screen glowing with a dark blue audio application interface with glowing cyan circular knobs, screen is the only light source, cyan light spilling onto the desk surface, shallow depth of field, photorealistic, moody"
6. **Abstract section divider.** "Three thin concentric arcs of cyan light like ripples on dark water seen from above, fading into deep navy darkness, extreme minimalism, glow"
7. **OG/social image.** "Wide 1200x630 composition: left half deep navy space with falling star streaks, right half a floating dark app window glowing with cyan interface elements and a ring of small glowing planets, soft ambient glow, premium software marketing aesthetic"
8. **3D render — brand object.** "A four-pointed star of white light embedded in a translucent glass rounded cube filled with swirling deep blue nebula fluid and tiny tide arcs, floating in darkness, studio product render, caustics, 8k"
9. **Icon set base.** "Minimal line icon set with soft outer glow, cyan #4fe3ff strokes on transparent: planet with ring, four-pointed star, infinity symbol, sound wave arc, rotary knob, downward arrow in circle, musical key — consistent 2px stroke, rounded caps"
10. **Launch banner.** "Cinematic banner: a small glowing four-pointed star rising over a dark ocean horizon at night, its light reflected as a cyan tide path on the water, stars falling above like meteors, deep navy palette, vast negative space for headline text on the left"

---

## 15. Motion Storyboard

### Homepage intro (6 s, auto, silent)
0.0–1.0 s black → single star fades in center · 1.0–2.2 s star emits one tide-arc ripple; starfield ignites around it (stars begin falling) · 2.2–3.5 s hero screenshot rises from below 40 px with blur 8→0 · 3.5–6.0 s headline types on letter-spaced, CTA glows once. Loop only the starfield thereafter.

### 30-second product trailer
| Time | Visual | Audio | Text overlay |
|---|---|---|---|
| 0–3 s | Black; one star ignites; ripple | Sub-bass swell begins (the actual C pad) | — |
| 3–8 s | UI materializes around the star (app window forms) | Pad blooms (real audio) | "An ocean of sound." |
| 8–14 s | Cursor taps Eb planet — crossfade; planets flare in sequence | Pad crossfades key (audible!) | "In every key. Real recordings." |
| 14–20 s | Texture star tapped; rain streaks in background; FX star pulse | Rain layer + swell | "Weather included." |
| 20–25 s | Preset arrow → whole UI recolors crimson; then violet | New pad character | "It wears your sound." |
| 25–30 s | Zoom out; window floats in starfield; logo + wordmark; CTA | Music resolves, tail rings out (Hall) | "Nebula Tide — Download free" |
Camera: slow push-ins only (2–4%/s); transitions: crossfades and light-bloom wipes, never cuts on the beat-less music. Music: the product's own audio *is* the soundtrack.

### 60-second feature walkthrough
Extends the trailer: +10 s reverb section (Room→Plate→Hall on the same held pad — audible spaces), +10 s Forge montage (drop file → waveform draws → color pick → whole app recolors), +10 s VST3-in-DAW shot with voiceover-free captions.

### Social teaser (9:16, 8 s)
Planet tap → full-UI color morph → logo. Text: "Tap a planet. Change the sky." Loops perfectly.

### Launch video (90 s)
Trailer + 3 real-user vignettes (stage / studio / meditation room, each lit only by the screen) + pricing card + CTA. Music: one continuous Nebula Tide performance, C → Ab → Eb journey.

---

## 16. Landing Page Structure

1. **Hero** (starfield, headline, dual CTA, floating app window) — emotion + instant comprehension. *Trigger: aesthetic awe.*
2. **Inline audio demo bar** ("Hear it — 30 s") — audio product must be heard within one scroll. *Trigger: proof.*
3. **Three-step workflow** (Pick → Tap → Play) — kills the "is it complicated?" objection immediately. *Trigger: ease.*
4. **12-keys feature block** (planet animation) — the USP gets the biggest section. *Trigger: differentiation.*
5. **Living-UI theme-morph scroll moment** — the shareable wow. *Trigger: delight/virality.*
6. **Stars & reverb blocks** (paired half-width cards) — depth without overwhelm.
7. **Comparison table** — conversion logic for researchers. *Trigger: rational justification.*
8. **Testimonials** (placeholders until real) + download count when available. *Trigger: social proof.*
9. **Packs/pricing** — monetization surface, framed as "own it forever."
10. **FAQ** (accordion, schema.org FAQ markup for SEO).
11. **Final CTA** ("The tide is waiting.") — emotional bookend.
12. **Footer.**

**Conversion optimization:** sticky mini-CTA after 50% scroll; CTA copy states platform + free ("Download free for Windows"); demo audio player never autoplays (respect = brand). **Trust:** real screenshots only, file size + no-account-needed noted under the download button. **Accessibility:** all glow-on-dark text ≥ 4.5:1 (foam #BDF3FF on #020C16 passes), planets/star animations honor `prefers-reduced-motion` (freeze, don't remove), audio demos keyboard-operable, captions on all videos. **Mobile-first:** starfield density halved, theme-morph runs on tap instead of scroll-scrub, hero window screenshot swaps to a 4:5 crop, CTAs thumb-reachable.

---

## 17. Developer Handoff JSON

The complete machine-readable package is in [`handoff.json`](./handoff.json). It mirrors this document: product identity, full color tokens, fonts, feature list with copy, screenshot manifest, animation specs, all landing copy, SEO/OG fields, FAQs, placeholder testimonials, pricing structure, section order, navigation, footer, and every image prompt from §14.

---

## 18. Checklists

### Screenshot checklist
- [x] 00 Logo (512 px PNG)
- [x] 01 Main UI — idle/drifting
- [x] 02 Main UI — playing/transmitting
- [ ] 03 Forge — preset open with waveforms
- [ ] 04 Planet ring close-up (2× crop)
- [ ] 05 Reverb section close-up
- [ ] 06 Theme-shift pair (two accent colors)
- [ ] 07 VST3 inside a DAW

### Animation checklist
- [ ] Hero starfield (canvas/WebGL, reduced-motion fallback)
- [ ] Planet-ring section animation (sequential light-up)
- [ ] ∞ seamless-loop mark (Lottie)
- [ ] Crossfade waveform dissolve
- [ ] Theme-morph scroll moment (GSAP scrub / tap on mobile)
- [ ] Star pulse micro-loop (Lottie)
- [ ] 30 s trailer · 60 s walkthrough · 8 s social teaser
- [ ] 3 × 6 s feature MP4 loops

### Asset production checklist
- [ ] Vector logo suite (SVG ×3 variants)
- [ ] Favicon/app-icon set (extract `build/juce_icon.ico` + web sizes)
- [ ] 30 s hero demo audio mix
- [ ] OG image 1200×630 · social banners
- [ ] Icon set (7 line-glow icons)
- [ ] Starfield 4K still + aurora SVG set

---

*Package generated 2026-08-05. Screenshots: `./screenshots/`. JSON: `./handoff.json`. App source of truth: `C:\Drone Pad` (JUCE C++, one codebase → Windows standalone + VST3, macOS/iPad planned).*
