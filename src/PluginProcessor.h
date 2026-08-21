#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

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
    bool isValid() const { return file.existsAsFile() || data != nullptr; }
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

// A named one-shot sound (FX / texture), loaded from disk.
struct AuxSound
{
    juce::String name;
    juce::File file;
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
    static int zoneOf (int note)   // 0 fx, 1 keys, 2 textures, -1 none
    {
        if (note >= fxZoneLo  && note <= fxZoneHi)  return 0;
        if (note >= keyZoneLo && note <= keyZoneHi) return 1;
        if (note >= texZoneLo && note <= texZoneHi) return 2;
        return -1;
    }
    int  lastNoteOn() const  { return lastNote.load(); }   // for the on-screen keyboard
    std::atomic<bool> heldKeys[128] {};                     // currently held MIDI notes

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
    // 13 = pad play/stop, 14-25 = keys C..B, 26-33 = select preset 1..8.
    enum { numMidiActions = 34 };
    static const char* midiActionName (int action);
    juce::String bindingText (int action) const;   // "CC 7", "Note C1", "—"
    void startLearn (int action)   { learnTarget.store (action); }
    void cancelLearn()             { learnTarget.store (-1); }
    int  learningAction() const    { return learnTarget.load(); }
    void clearBinding (int action) { if (action >= 0 && action < numMidiActions) binding[action].store (0); }

    juce::AudioProcessorValueTreeState apvts;

private:
    void scanPresets();
    void applyManifest();
    void startSource (const PresetSource&);
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioFormatManager formatManager;
    juce::Array<PresetGroup> presets;

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
    double deviceSampleRate = 44100.0;

    // Audio decodes happen on this worker so the UI never freezes; generation
    // counters discard stale loads when the user clicks faster than disk reads.
    juce::ThreadPool loadPool { 1 };
    std::atomic<int> padLoadGen { 0 }, auxLoadGen[2] { { 0 }, { 0 } };

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
