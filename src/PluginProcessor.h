#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "NtLibrary.h"
#include "Shimmer.h"
#include "License/LicenseClient.h"

// ── Seamless looping ──────────────────────────────────────────────────────
// Instead of jumping from the last sample back to the first (audible "snap"),
// the final kLoopCrossfadeSeconds of a file are blended into its beginning
// with an equal-power crossfade, so any loop point sounds continuous.
// Adjustable in SETTINGS → LOOP BLEND; applies to pads, FX and textures alike.
inline std::atomic<float> gLoopBlendSeconds { 1.5f };

inline float readInterp (const float* src, int len, double pos) noexcept
{
    const int i0 = (int) pos;
    const int i1 = (i0 + 1 < len) ? i0 + 1 : 0;
    const float f = (float) (pos - i0);
    return src[i0] + f * (src[i1] - src[i0]);
}

// One playing drone pad: a fully-loaded audio buffer looping seamlessly,
// resampled from its native rate to the device rate with linear interpolation.
struct PadVoice
{
    juce::AudioBuffer<float> buffer;
    double sourceSampleRate = 44100.0;
    double position = 0.0;          // fractional read head
    float  gain = 0.0f;             // current crossfade gain
    float  targetGain = 0.0f;       // where the crossfade is heading
    bool   active = false;

    void render (juce::AudioBuffer<float>& out, int numSamples,
                 double deviceSampleRate, float fadePerSample);
};

// A single audio source: either a file on disk or an embedded binary resource.
struct PresetSource
{
    juce::File  file;
    const char* data = nullptr;
    int         dataSize = 0;
    juce::String libEntry;      // path inside the encrypted .ntlib container
    bool isValid() const { return file.existsAsFile() || data != nullptr || libEntry.isNotEmpty(); }
};

// A preset (drone pad) with up to 12 key variants. Key index: 0=C .. 11=B.
struct PresetGroup
{
    juce::String name;
    PresetSource keys[12];

    // authored in Nebula Forge (presets/manifest.json)
    juce::Colour colour { 0xff4fe3ff };
    bool  hasReverbDefaults = false;
    int   rType = 2;            // 0 room, 1 plate, 2 hall
    float rMix = 0.4f, rSize = 0.85f, rDamp = 0.45f;
    juce::String defaultFx, defaultTex;   // filenames of default star sounds
    // Made by the person using the app, in their own folder, as plain audio.
    // Only these can be edited or deleted from the Studio dashboard; the
    // shipped library is read-only and stays inside the encrypted container.
    bool isUser = false;
    bool hasKey (int k) const { return k >= 0 && k < 12 && keys[k].isValid(); }
    int firstAvailableKey() const
    {
        for (int k = 0; k < 12; ++k) if (hasKey (k)) return k;
        return -1;
    }
};

namespace keynames
{
    static const char* display[12] = { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };
}

// A named one-shot sound (FX / texture), from disk or the encrypted container.
struct AuxSound
{
    juce::String name;
    juce::File file;
    juce::String libEntry;
};

// One-shot player for FX/textures: plays once, or loops while looping is on.
// Independent of the preset voices — survives preset changes.
struct AuxVoice
{
    juce::AudioBuffer<float> buffer;
    double sourceSampleRate = 44100.0;
    double position = 0.0;
    float  gain = 0.0f;
    float  targetGain = 0.0f;
    bool   active = false;
    std::atomic<bool> looping { true };          // loop on by default
    std::atomic<float> volume { 0.5f };          // per-star volume, 50% default

    // attack is a fast fixed 20 ms; release follows the app's crossfade time
    void render (juce::AudioBuffer<float>& out, int numSamples,
                 double deviceSampleRate, float releasePerSample);
};

enum class ReverbType { room = 0, plate, hall };

class NebulaTideProcessor : public juce::AudioProcessor
{
public:
    NebulaTideProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Nebula Tide"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ── sound library (download on first launch for mobile / app-only installs) ──
    bool hasLibrary() const { return ! presets.isEmpty() || ! fxSounds.isEmpty() || ! texSounds.isEmpty(); }
    bool usingTestLibrary = false;              // loaded from a presets/Stems test folder
    void reloadLibrary();                       // rescan after installing (message thread)
    static juce::File userLibraryDir();         // per-user writable presets folder

    // ── User content (v2) ────────────────────────────────────────────
    // Presets people build themselves. Kept deliberately apart from the
    // shipped library: plain audio in a folder they own, so it is theirs to
    // move, back up and delete. Nothing here is encrypted — there is nothing
    // of ours in it to protect.
    static juce::File userContentDir();
    struct UserSlot { int key = 0; juce::File file; };
    // Copies the chosen audio into the user folder and rewrites the manifest.
    juce::Result saveUserPreset (const juce::String& name, juce::Colour colour,
                                 const juce::Array<UserSlot>& slots,
                                 int reverbType, float rMix, float rSize, float rDamp,
                                 const juce::String& defaultFx = {},
                                 const juce::String& defaultTex = {});
    juce::Result deleteUserPreset (const juce::String& name);
    juce::Result importAuxSound (int cat, const juce::File& source);   // 0 fx, 1 texture
    juce::Result deleteAuxSound (int cat, const juce::String& name);
    static bool isUserAux (const AuxSound& s);

    // Sounds always ship INSIDE the app — nothing is ever downloaded after install.
    // Android: the library is packed into the APK's assets and copied out into
    // userLibraryDir() on first launch (background thread; progress 0..1).
    bool installBundledLibrary (std::function<void (double)> progress);
    bool installBundledLooseFiles (std::function<void (double)> progress);
    static juce::String librarySearchReport();   // diagnostics for the "not found" panel

    // ── Where the sounds live ────────────────────────────────────────
    // An install can fail to write the shared folder - permissions, disk
    // space, antivirus - and the app then opens with nothing and no way
    // forward. A remembered path fixes that without a reinstall, and also
    // lets people keep 264 MB on a second drive.
    static juce::File userChosenLibraryDir();
    static void setUserChosenLibraryDir (const juce::File& dir);
    static juce::File currentLibraryFile();      // the .ntlib actually in use
    static juce::File defaultLibraryDir();       // where the installer puts it

    // Version notice only — a few hundred bytes, once per launch, never any
    // content. No connection = no check; the app is fully functional offline.
    static constexpr const char* updateUrl = "https://amanorsac.studio/nebulatide/latest.json";

    // ── preset / key control (message thread) ──
    const juce::Array<PresetGroup>& getPresets() const { return presets; }
    int  getCurrentPadIndex() const { return currentPad.load(); }
    int  getCurrentKey() const      { return currentKey.load(); }
    void selectPad (int index);         // crossfades into the chosen pad (current key)
    void selectKey (int keyIndex);      // crossfades current pad into a new key
    void stopAll();

    // performance input (MIDI notes / computer keyboard): play this pitch class —
    // switches key if the drone is running, otherwise starts the last pad in it
    void keyCommand (int pitchClass);
    void toggleAux (int cat);           // star play/stop toggle

    // Kontakt-style note gating: while on, MIDI note-on plays the key and the
    // pad fades out when the last held note is released. Off = notes only
    // switch keys and the pad sustains until stopped.
    std::atomic<bool> noteGate { false };
    std::atomic<bool> showKeyboard { false };   // zoned keyboard strip visible (UI pref, persisted)

    // ── Key zones (Kontakt-style keyboard layout) ──
    //   FX       C2..B2  (36..47)  one note per FX sound
    //   KEYS     C3..B4  (48..71)  pitch class = musical key
    //   TEXTURES C5..B5  (72..83)  one note per texture
    enum { fxZoneLo = 36, fxZoneHi = 47, keyZoneLo = 48, keyZoneHi = 71, texZoneLo = 72, texZoneHi = 83 };

    // FX and textures also answer on a channel of their own, in both modes.
    // The note zones sit inside normal playing range, so once chord mode hands
    // the keyboard over to chords there has to be a route nothing can collide
    // with — this is it, and it is what dragged MIDI clips are written to.
    static constexpr int auxMidiChannel = 16;
    static int zoneOf (int note)   // 0 fx, 1 keys, 2 textures, -1 none
    {
        if (note >= fxZoneLo  && note <= fxZoneHi)  return 0;
        if (note >= keyZoneLo && note <= keyZoneHi) return 1;
        if (note >= texZoneLo && note <= texZoneHi) return 2;
        return -1;
    }
    int  lastNoteOn() const  { return lastNote.load(); }   // for the on-screen keyboard
    std::atomic<bool> heldKeys[128] {};                     // currently held MIDI notes

    // Host tempo, captured for the drag-to-timeline MIDI clip.
    std::atomic<double> hostBpm { 120.0 };

    // sets the size/damp parameter defaults for a reverb category
    void applyReverbPreset (ReverbType type);

    // ── FX / texture stars: cat 0 = fx, cat 1 = texture ──
    const juce::Array<AuxSound>& getAuxSounds (int cat) const { return cat == 0 ? fxSounds : texSounds; }
    void triggerAux (int cat, int index);     // starts the sound (stops previous in that slot)
    void stopAux (int cat);                   // fades the slot out
    bool isAuxPlaying (int cat) const         { return auxVoices[cat].active && auxVoices[cat].targetGain > 0.0f; }
    void setAuxLoop (int cat, bool shouldLoop){ auxVoices[cat].looping = shouldLoop; }
    bool getAuxLoop (int cat) const           { return auxVoices[cat].looping; }
    int  getAuxIndex (int cat) const          { return auxIndex[cat].load(); }
    void setAuxIndex (int cat, int i)         { auxIndex[cat].store (i); }
    float getAuxVolume (int cat) const        { return auxVoices[cat].volume.load(); }
    void  setAuxVolume (int cat, float v)     { auxVoices[cat].volume.store (juce::jlimit (0.0f, 1.0f, v)); }

    std::atomic<float> outputLevel { 0.0f };   // for the reactive UI

    // ── MIDI mapping + learn ─────────────────────────────────────────
    // Actions 0-5 are continuous (parameters), 6-12 are core commands,
    // 13 = pad play/stop, 14-25 = keys C..B, 26-33 = select preset 1..8,
    // 34-35 = shimmer (v2). New actions are appended so saved v1 maps still load.
    enum { numMidiActions = 36 };
    static const char* midiActionName (int action);
    juce::String bindingText (int action) const;   // "CC 7", "Note C1", "—"
    void startLearn (int action)   { learnTarget.store (action); }
    void cancelLearn()             { learnTarget.store (-1); }
    int  learningAction() const    { return learnTarget.load(); }
    void clearBinding (int action) { if (action >= 0 && action < numMidiActions) binding[action].store (0); }

    // One client per instance, but one device id, key and proof on disk, so a
    // machine uses one seat however many plugin windows are open (standard R11).
    amanorsacstudio::LicenseClient license;

    juce::AudioProcessorValueTreeState apvts;

private:
    void scanPresets();
    void applyManifest();
    void scanUserContent();
    void writeUserManifest();
    void startSource (const PresetSource&);
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioFormatManager formatManager;
    juce::Array<PresetGroup> presets;
    juce::OwnedArray<ntlib::Reader> libraries;   // encrypted containers, in memory only

    // Opens an audio reader for a source, decrypting from the container when needed.
    juce::AudioFormatReader* createReaderFor (const PresetSource&);
    juce::AudioFormatReader* createReaderFor (const AuxSound&);
    static juce::Array<juce::File> findLibraryFiles();

    static constexpr int maxVoices = 2;
    PadVoice voices[maxVoices];
    int nextVoice = 0;
    std::atomic<int> currentPad { -1 };
    std::atomic<int> currentKey { 0 };
    std::atomic<int> lastPad { 0 };     // last selected pad, for MIDI-started playback

    juce::Array<AuxSound> fxSounds, texSounds;
    AuxVoice auxVoices[2];
    std::atomic<int> auxIndex[2] { { 0 }, { 0 } };

    juce::SpinLock voiceLock;
    juce::Reverb reverb;
    shimmer::Engine shimmerFx;          // v2 — bypassed entirely when amount is 0
    juce::AudioBuffer<float> dryBuf, wetBuf, shimReturn, shimSource, prevTail;
    juce::SmoothedValue<float> dryGainSm, wetGainSm;   // match juce::Reverb's 10 ms ramp
    double deviceSampleRate = 44100.0;

    // Audio decodes happen on this worker so the UI never freezes; generation
    // counters discard stale loads when the user clicks faster than disk reads.
    juce::ThreadPool loadPool { 1 };
    std::atomic<int> padLoadGen { 0 }, auxLoadGen[2] { { 0 }, { 0 } };
    std::atomic<bool> offlineMode { false };     // host is bouncing: work synchronously
    std::atomic<int> pendingRestorePad { -1 };   // state restore for offline instances

    // binding encoding: 0 = unbound, else 0x200 | (isCC ? 0x100 : 0) | number
    std::atomic<int> binding[numMidiActions] {};
    std::atomic<int> lastCmdVal[numMidiActions] {};   // CC edge detection for commands
    std::atomic<int> learnTarget { -1 };
    void applyDefaultBindings();
    void runMidiCommand (int action);

    std::atomic<int> heldNotes { 0 };   // held KEY-zone notes, for gate release
    std::atomic<int> lastNote { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NebulaTideProcessor)
};
