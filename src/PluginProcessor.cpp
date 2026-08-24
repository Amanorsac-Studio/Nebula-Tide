#include "PluginProcessor.h"
#include "PluginEditor.h"

#if NEBULA_HAS_EMBEDDED_PRESETS
 #include "BinaryData.h"
#endif

//==============================================================================
void PadVoice::render (juce::AudioBuffer<float>& out, int numSamples,
                       double deviceSampleRate, float fadePerSample)
{
    if (! active || buffer.getNumSamples() == 0)
        return;

    const double ratio = sourceSampleRate / deviceSampleRate;
    const int srcLen = buffer.getNumSamples();
    const int srcChans = buffer.getNumChannels();
    const int outChans = out.getNumChannels();

    // loop crossfade region: the tail [xfStart, len) blends into the head [0, xf)
    const double xf = juce::jmin ((double) srcLen * 0.25, (double) gLoopBlendSeconds.load() * sourceSampleRate);
    const double xfStart = (double) srcLen - xf;

    for (int i = 0; i < numSamples; ++i)
    {
        if (gain < targetGain)      gain = juce::jmin (targetGain, gain + fadePerSample);
        else if (gain > targetGain) gain = juce::jmax (targetGain, gain - fadePerSample);

        const bool inXf = xf > 1.0 && position >= xfStart;
        float gOut = 1.0f, gIn = 0.0f;
        double headPos = 0.0;
        if (inXf)
        {
            const double t = juce::jlimit (0.0, 1.0, (position - xfStart) / xf);
            gOut = (float) std::cos (t * juce::MathConstants<double>::halfPi);   // equal-power
            gIn  = (float) std::sin (t * juce::MathConstants<double>::halfPi);
            headPos = position - xfStart;
        }

        for (int ch = 0; ch < outChans; ++ch)
        {
            const float* src = buffer.getReadPointer (juce::jmin (ch, srcChans - 1));
            float sample = readInterp (src, srcLen, position);
            if (inXf)
                sample = sample * gOut + readInterp (src, srcLen, headPos) * gIn;
            out.addSample (ch, i, sample * gain);
        }

        position += ratio;
        if (position >= srcLen)
            position -= xfStart;   // continue from where the head blend left off
    }

    if (gain <= 0.0001f && targetGain <= 0.0001f)
        active = false;
}

//==============================================================================
void AuxVoice::render (juce::AudioBuffer<float>& out, int numSamples,
                       double deviceSampleRate, float releasePerSample)
{
    if (! active || buffer.getNumSamples() == 0)
        return;

    const double ratio = sourceSampleRate / deviceSampleRate;
    const int srcLen = buffer.getNumSamples();
    const int srcChans = buffer.getNumChannels();
    const int outChans = out.getNumChannels();
    const float attackPerSample = 1.0f / (float) (0.02 * deviceSampleRate);   // 20 ms anti-click

    const bool loop = looping.load();
    const double xf = loop ? juce::jmin ((double) srcLen * 0.25, (double) gLoopBlendSeconds.load() * sourceSampleRate) : 0.0;
    const double xfStart = (double) srcLen - xf;

    for (int i = 0; i < numSamples; ++i)
    {
        if (gain < targetGain)      gain = juce::jmin (targetGain, gain + attackPerSample);
        else if (gain > targetGain) gain = juce::jmax (targetGain, gain - releasePerSample);

        const bool inXf = xf > 1.0 && position >= xfStart;
        float gOut = 1.0f, gIn = 0.0f;
        double headPos = 0.0;
        if (inXf)
        {
            const double t = juce::jlimit (0.0, 1.0, (position - xfStart) / xf);
            gOut = (float) std::cos (t * juce::MathConstants<double>::halfPi);
            gIn  = (float) std::sin (t * juce::MathConstants<double>::halfPi);
            headPos = position - xfStart;
        }

        const float vol = volume.load();
        for (int ch = 0; ch < outChans; ++ch)
        {
            const float* src = buffer.getReadPointer (juce::jmin (ch, srcChans - 1));
            float sample = readInterp (src, srcLen, position);
            if (inXf)
                sample = sample * gOut + readInterp (src, srcLen, headPos) * gIn;
            out.addSample (ch, i, sample * gain * vol);
        }

        position += ratio;
        if (position >= srcLen)
        {
            if (loop)
                position -= xfStart;   // continue from where the head blend left off
            else
            {
                active = false;
                gain = 0.0f;
                targetGain = 0.0f;
                return;
            }
        }
    }

    if (gain <= 0.0001f && targetGain <= 0.0001f)
        active = false;
}

//==============================================================================
// Filename convention: "<PresetName>_<Key>.wav" e.g. "Deep_Current_Eb.wav",
// "solar wind F#.mp3". A file with no key token is treated as key of C.
static int parseKeyToken (const juce::String& token)
{
    static const std::pair<const char*, int> map[] = {
        { "c", 0 }, { "c#", 1 }, { "db", 1 }, { "d", 2 }, { "d#", 3 }, { "eb", 3 },
        { "e", 4 }, { "f", 5 }, { "f#", 6 }, { "gb", 6 }, { "g", 7 }, { "g#", 8 },
        { "ab", 8 }, { "a", 9 }, { "a#", 10 }, { "bb", 10 }, { "b", 11 }
    };
    const auto t = token.toLowerCase();
    for (auto& [name, idx] : map)
        if (t == name) return idx;
    return -1;
}

static void splitNameAndKey (const juce::String& stem, juce::String& outName, int& outKey)
{
    // try the last token separated by '_', '-' or space
    const int cut = juce::jmax (stem.lastIndexOfChar ('_'),
                                juce::jmax (stem.lastIndexOfChar ('-'), stem.lastIndexOfChar (' ')));
    if (cut > 0)
    {
        const int key = parseKeyToken (stem.substring (cut + 1).trim());
        if (key >= 0)
        {
            outName = stem.substring (0, cut).replaceCharacters ("_-", "  ").trim();
            outKey = key;
            return;
        }
    }
    outName = stem.replaceCharacters ("_-", "  ").trim();
    outKey = 0;
}

//==============================================================================
// Locates the sound library. Walks upward from the running binary (so it works
// from inside a .vst3/.component bundle), then falls back to the shared
// per-machine library the installer writes. First existing folder wins.
// A "Stems" subfolder inside the library acts as a TEST library: when it
// contains audio, the app loads from it instead (same layout: pads in the
// root, fx/ and textures/ inside). Rename or delete it to go back.
static juce::File preferTestLibrary (juce::File dir)
{
    const auto stems = dir.getChildFile ("Stems");
    if (! stems.isDirectory()) return dir;
    const char* audio = "*.flac;*.wav;*.mp3;*.ogg;*.aiff";
    const int n = stems.getNumberOfChildFiles (juce::File::findFiles, audio)
                + stems.getChildFile ("fx").getNumberOfChildFiles (juce::File::findFiles, audio)
                + stems.getChildFile ("textures").getNumberOfChildFiles (juce::File::findFiles, audio);
    return n > 0 ? stems : dir;   // only an actual test library takes over
}

static juce::File findPresetsDir()
{
    // sounds bundled inside the app itself (iOS app root / macOS .app Resources)
    const auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    for (auto c : { app.getChildFile ("presets"),
                    app.getChildFile ("Contents").getChildFile ("Resources").getChildFile ("presets") })
        if (c.isDirectory()) return preferTestLibrary (c);

    auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    for (int up = 0; up < 6 && dir.exists(); ++up)
    {
        const auto candidate = dir.getChildFile ("presets");
        if (candidate.isDirectory()) return preferTestLibrary (candidate);
        dir = dir.getParentDirectory();
    }

    for (auto candidate : {
        // macOS: the installer writes /Library/Application Support/Nebula Tide/presets
        // (JUCE's commonApplicationDataDirectory is /Library on Mac, so add both)
        juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
            .getChildFile ("Application Support").getChildFile ("Nebula Tide").getChildFile ("presets"),
        juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Application Support").getChildFile ("Nebula Tide").getChildFile ("presets"),
        juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
            .getChildFile ("Nebula Tide").getChildFile ("presets"),           // ProgramData / Library
        juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory)
            .getChildFile ("Nebula Tide").getChildFile ("presets"),           // Program Files install
        juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
            .getChildFile ("Nebula Tide").getChildFile ("presets") })        // per-user
        if (candidate.isDirectory()) return preferTestLibrary (candidate);

    return {};
}

juce::File NebulaTideProcessor::userLibraryDir()
{
   #if JUCE_MAC
    // ~/Library/Application Support/Nebula Tide/presets (Mac convention)
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Application Support").getChildFile ("Nebula Tide").getChildFile ("presets");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Nebula Tide").getChildFile ("presets");
   #endif
}

void NebulaTideProcessor::reloadLibrary()
{
    stopAll();
    scanPresets();          // clears and rescans pads, fx, textures, manifest
}

#if JUCE_ANDROID
 #include <android/asset_manager.h>
 #include <android/asset_manager_jni.h>

// Copies the encrypted container out of the APK assets into the app's private
// data folder on first launch. Runs on a background thread.
bool NebulaTideProcessor::installBundledLibrary (std::function<void (double)> progress)
{
    auto* env = juce::getEnv();
    auto context = juce::getAppContext();
    if (env == nullptr || context.get() == nullptr) return false;

    jclass ctxClass = env->GetObjectClass (context.get());
    jmethodID getAssets = env->GetMethodID (ctxClass, "getAssets", "()Landroid/content/res/AssetManager;");
    jobject assetMgrObj = env->CallObjectMethod (context.get(), getAssets);
    AAssetManager* mgr = AAssetManager_fromJava (env, assetMgrObj);
    if (mgr == nullptr) return false;

    const auto dest = userLibraryDir().getParentDirectory();   // .../Nebula Tide/
    dest.createDirectory();
    const auto target = dest.getChildFile ("NebulaTide.ntlib");

    bool ok = false;
    if (AAsset* a = AAssetManager_open (mgr, "NebulaTide.ntlib", AASSET_MODE_STREAMING))
    {
        const juce::int64 total = juce::jmax ((juce::int64) 1, (juce::int64) AAsset_getLength64 (a));
        target.deleteFile();
        juce::FileOutputStream out (target);
        if (out.openedOk())
        {
            std::vector<char> buf (1 << 16);
            juce::int64 written = 0;
            int n;
            while ((n = AAsset_read (a, buf.data(), buf.size())) > 0)
            {
                out.write (buf.data(), (size_t) n);
                written += n;
                if (progress) progress ((double) written / (double) total);
            }
            out.flush();
            ok = written > 0;
        }
        AAsset_close (a);
    }
    env->DeleteLocalRef (assetMgrObj);
    return ok;
}

// (legacy loose-file installer, kept for reference)
bool NebulaTideProcessor::installBundledLooseFiles (std::function<void (double)> progress)
{
    auto* env = juce::getEnv();
    auto context = juce::getAppContext();
    if (env == nullptr || context.get() == nullptr) return false;

    jclass ctxClass = env->GetObjectClass (context.get());
    jmethodID getAssets = env->GetMethodID (ctxClass, "getAssets", "()Landroid/content/res/AssetManager;");
    jobject assetMgrObj = env->CallObjectMethod (context.get(), getAssets);
    AAssetManager* mgr = AAssetManager_fromJava (env, assetMgrObj);
    if (mgr == nullptr) return false;

    const juce::File dest = userLibraryDir();
    dest.createDirectory();
    const char* dirs[] = { "presets", "presets/fx", "presets/textures" };

    int total = 0;
    for (auto* d : dirs)
        if (AAssetDir* ad = AAssetManager_openDir (mgr, d))
        {
            while (AAssetDir_getNextFileName (ad) != nullptr) ++total;
            AAssetDir_close (ad);
        }

    int done = 0;
    std::vector<char> buf (1 << 16);
    for (auto* d : dirs)
    {
        AAssetDir* ad = AAssetManager_openDir (mgr, d);
        if (ad == nullptr) continue;
        const juce::String sub = juce::String (d).fromFirstOccurrenceOf ("presets", false, false).trimCharactersAtStart ("/");
        while (const char* name = AAssetDir_getNextFileName (ad))
        {
            const juce::String assetPath = juce::String (d) + "/" + name;
            if (AAsset* a = AAssetManager_open (mgr, assetPath.toRawUTF8(), AASSET_MODE_STREAMING))
            {
                auto target = (sub.isEmpty() ? dest : dest.getChildFile (sub)).getChildFile (name);
                target.getParentDirectory().createDirectory();
                target.deleteFile();
                juce::FileOutputStream out (target);
                if (out.openedOk())
                {
                    int n;
                    while ((n = AAsset_read (a, buf.data(), buf.size())) > 0)
                        out.write (buf.data(), (size_t) n);
                }
                AAsset_close (a);
            }
            ++done;
            if (progress) progress (total > 0 ? (double) done / (double) total : 1.0);
        }
        AAssetDir_close (ad);
    }
    env->DeleteLocalRef (assetMgrObj);
    return done > 0;
}
#else
bool NebulaTideProcessor::installBundledLibrary (std::function<void (double)>) { return false; }
#endif

// Where the app looked for its library — shown in the "not found" panel so a
// single screenshot from a user tells us exactly what went wrong.
juce::String NebulaTideProcessor::librarySearchReport()
{
    juce::StringArray lines;
    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    lines.add ("binary: " + exe.getFullPathName());
    for (auto base : { juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
                           .getChildFile ("Application Support").getChildFile ("Nebula Tide"),
                       juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory)
                           .getChildFile ("Nebula Tide"),
                       juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                           .getChildFile ("Application Support").getChildFile ("Nebula Tide") })
        lines.add ((base.isDirectory() ? juce::String ("found dir: ") : juce::String ("missing:   ")) + base.getFullPathName());
    return lines.joinIntoString ("\n");
}

//==============================================================================
NebulaTideProcessor::NebulaTideProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "params", createLayout())
{
    formatManager.registerBasicFormats();
    scanPresets();
    applyDefaultBindings();
    // in a DAW, piano-roll notes should gate the pad like a sampler does;
    // standalone performers usually want tap-and-sustain
    noteGate.store (wrapperType != wrapperType_Standalone);
}

//==============================================================================
// MIDI mapping

static const char* const midiParamIds[6] = { "volume", "pan", "rmix", "rsize", "rdamp", "fade" };

const char* NebulaTideProcessor::midiActionName (int action)
{
    static const char* names[numMidiActions] = {
        "Volume", "Pan", "Reverb Mix", "Reverb Size", "Reverb Damp", "Crossfade",
        "FX Star Toggle", "Texture Star Toggle", "Stop Everything",
        "Next Preset", "Previous Preset", "Next FX Sound", "Next Texture",
        "Pad Play / Stop",
        "Key C", "Key Db", "Key D", "Key Eb", "Key E", "Key F",
        "Key Gb", "Key G", "Key Ab", "Key A", "Key Bb", "Key B",
        "Select Preset 1", "Select Preset 2", "Select Preset 3", "Select Preset 4",
        "Select Preset 5", "Select Preset 6", "Select Preset 7", "Select Preset 8"
    };
    return (action >= 0 && action < numMidiActions) ? names[action] : "";
}

void NebulaTideProcessor::applyDefaultBindings()
{
    auto cc   = [] (int n) { return 0x200 | 0x100 | n; };
    auto note = [] (int n) { return 0x200 | n; };
    binding[0].store (cc (7));     // volume
    binding[1].store (cc (10));    // pan
    binding[2].store (cc (91));    // reverb mix
    binding[6].store (note (24));  // C1  fx toggle
    binding[7].store (note (25));  // C#1 texture toggle
    binding[8].store (note (28));  // E1  stop all
    binding[11].store (note (26)); // D1  next fx
    binding[12].store (note (27)); // D#1 next texture
}

juce::String NebulaTideProcessor::bindingText (int action) const
{
    const int b = (action >= 0 && action < numMidiActions) ? binding[action].load() : 0;
    if ((b & 0x200) == 0) return "-";
    const int num = b & 0xFF;
    return (b & 0x100) != 0 ? "CC " + juce::String (num)
                            : "Note " + juce::MidiMessage::getMidiNoteName (num, true, true, 3);
}

void NebulaTideProcessor::runMidiCommand (int action)
{
    switch (action)
    {
        case 6:  toggleAux (0); break;
        case 7:  toggleAux (1); break;
        case 8:  stopAll(); break;
        case 9:  if (! presets.isEmpty()) selectPad ((lastPad.load() + 1) % presets.size()); break;
        case 10: if (! presets.isEmpty()) selectPad ((lastPad.load() - 1 + presets.size()) % presets.size()); break;
        case 11: { const int n = getAuxSounds (0).size();
                   if (n > 0) { const int i = (getAuxIndex (0) + 1) % n;
                                if (isAuxPlaying (0)) triggerAux (0, i); else setAuxIndex (0, i); } break; }
        case 12: { const int n = getAuxSounds (1).size();
                   if (n > 0) { const int i = (getAuxIndex (1) + 1) % n;
                                if (isAuxPlaying (1)) triggerAux (1, i); else setAuxIndex (1, i); } break; }
        case 13:                                   // pad play / stop
            if (currentPad.load() >= 0) stopAll();
            else if (! presets.isEmpty()) selectPad (juce::jlimit (0, presets.size() - 1, lastPad.load()));
            break;
        default:
            if (action >= 14 && action <= 25)      // musical keys
                keyCommand (action - 14);
            else if (action >= 26 && action <= 33) // direct preset select
                if (action - 26 < presets.size())
                    selectPad (action - 26);
            break;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout NebulaTideProcessor::createLayout()
{
    using P = juce::AudioParameterFloat;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<P> ("volume",  "Volume",       0.0f, 1.0f, 0.5f));
    layout.add (std::make_unique<P> ("pan",     "Pan",         -1.0f, 1.0f, 0.0f));
    layout.add (std::make_unique<P> ("fade",    "Crossfade",    0.5f, 12.0f, 4.0f));
    layout.add (std::make_unique<P> ("rmix",    "Reverb Mix",   0.0f, 1.0f, 0.4f));
    layout.add (std::make_unique<P> ("rsize",   "Reverb Size",  0.0f, 1.0f, 0.85f));
    layout.add (std::make_unique<P> ("rdamp",   "Reverb Damp",  0.0f, 1.0f, 0.45f));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        "rtype", "Reverb Type", juce::StringArray { "Room", "Plate", "Hall" }, 2)); // default Hall
    return layout;
}

void NebulaTideProcessor::applyReverbPreset (ReverbType type)
{
    auto set = [this] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    switch (type)
    {
        case ReverbType::room:  set ("rsize", 0.35f); set ("rdamp", 0.55f); break;
        case ReverbType::plate: set ("rsize", 0.60f); set ("rdamp", 0.15f); break;
        case ReverbType::hall:  set ("rsize", 0.85f); set ("rdamp", 0.45f); break;
    }
    if (auto* p = apvts.getParameter ("rtype"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) type));
}

void NebulaTideProcessor::scanPresets()
{
    presets.clear();

    auto addSource = [this] (const juce::String& stem, PresetSource src)
    {
        juce::String name;
        int key = 0;
        splitNameAndKey (stem, name, key);

        for (auto& g : presets)
            if (g.name.equalsIgnoreCase (name))
            {
                if (! g.keys[key].isValid())
                    g.keys[key] = src;
                return;
            }
        PresetGroup g;
        g.name = name;
        g.keys[key] = src;
        presets.add (g);
    };

    // Disk first — a "presets" folder near the executable always wins, so pads
    // can be updated without rebuilding the app.
    const auto presetsDir = findPresetsDir();
    usingTestLibrary = presetsDir.getFileName() == "Stems";
    if (presetsDir.isDirectory())
        for (auto& f : presetsDir.findChildFiles (juce::File::findFiles, false, "*.wav;*.mp3;*.ogg;*.flac;*.aiff"))
        {
            PresetSource src;
            src.file = f;
            addSource (f.getFileNameWithoutExtension(), src);
        }

#if NEBULA_HAS_EMBEDDED_PRESETS
    // Embedded audio fills any slots the disk didn't provide.
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        int size = 0;
        const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
        if (data == nullptr) continue;

        const juce::String orig (BinaryData::getNamedResourceOriginalFilename (BinaryData::namedResourceList[i]));
        const auto ext = orig.fromLastOccurrenceOf (".", false, false).toLowerCase();
        if (ext != "wav" && ext != "mp3" && ext != "ogg" && ext != "flac" && ext != "aiff")
            continue;

        PresetSource src;
        src.data = data;
        src.dataSize = size;
        addSource (orig.upToLastOccurrenceOf (".", false, false), src);
    }
#endif

    // ── encrypted containers: used when no plain folder supplied the sounds ──
    fxSounds.clear();
    texSounds.clear();
    if (presets.isEmpty())
    {
        libraries.clear();
        for (auto& lib : findLibraryFiles())
        {
            auto reader = std::make_unique<ntlib::Reader>();
            if (! reader->open (lib)) continue;

            for (const auto& e : reader->getEntries())
            {
                const auto name = e.path.fromLastOccurrenceOf ("/", false, false);
                if (name.equalsIgnoreCase ("manifest.json")) continue;

                PresetSource src;
                src.libEntry = e.path;
                const auto stem = name.upToLastOccurrenceOf (".", false, false);

                if (e.path.startsWithIgnoreCase ("fx/"))
                    fxSounds.add ({ stem.replaceCharacters ("_-", "  "), {}, e.path });
                else if (e.path.startsWithIgnoreCase ("textures/"))
                    texSounds.add ({ stem.replaceCharacters ("_-", "  "), {}, e.path });
                else if (! e.path.containsChar ('/'))
                    addSource (stem, src);
            }
            libraries.add (reader.release());
        }
    }

    applyManifest();

    // ── FX / texture sounds from a plain folder (author mode) ──
    if (presetsDir.isDirectory())
    {
        auto scanCat = [] (const juce::File& sub, juce::Array<AuxSound>& into)
        {
            if (! sub.isDirectory()) return;
            for (auto& f : sub.findChildFiles (juce::File::findFiles, false, "*.wav;*.mp3;*.ogg;*.flac;*.aiff"))
                into.add ({ f.getFileNameWithoutExtension().replaceCharacters ("_-", "  "), f, {} });
        };
        scanCat (presetsDir.getChildFile ("fx"), fxSounds);
        scanCat (presetsDir.getChildFile ("textures"), texSounds);
    }
}

// Every .ntlib the app can see: beside/inside its own bundle first (so a plugin
// always finds the copy shipped with it), then the shared install location.
juce::Array<juce::File> NebulaTideProcessor::findLibraryFiles()
{
    juce::Array<juce::File> found;
    auto addFrom = [&found] (const juce::File& dir)
    {
        if (! dir.isDirectory()) return;
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.ntlib"))
            if (! found.contains (f)) found.add (f);
    };

    // inside this binary's bundle (VST3/AU/app), walking up a few levels
    auto dir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    for (int up = 0; up < 6 && dir.exists(); ++up)
    {
        addFrom (dir);
        addFrom (dir.getChildFile ("Resources"));
        addFrom (dir.getChildFile ("Contents").getChildFile ("Resources"));
        addFrom (dir.getChildFile ("sounds"));
        dir = dir.getParentDirectory();
    }

    // the app bundle itself (iOS/macOS standalone)
    const auto app = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
    addFrom (app);
    addFrom (app.getChildFile ("Contents").getChildFile ("Resources"));

    // shared install locations
    const auto common = juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory);
    const auto user   = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
    for (auto base : { common, user })
    {
        addFrom (base.getChildFile ("Nebula Tide"));
        addFrom (base.getChildFile ("Application Support").getChildFile ("Nebula Tide"));
    }
    addFrom (juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory)
                 .getChildFile ("Nebula Tide"));

    return found;
}

// Reads presets/manifest.json (authored in Nebula Forge) and applies colour,
// reverb defaults, and ordering to the scanned preset groups.
void NebulaTideProcessor::applyManifest()
{
    juce::String text;

    const auto manifest = findPresetsDir().getChildFile ("manifest.json");
    if (manifest.existsAsFile())
        text = manifest.loadFileAsString();

    if (text.isEmpty())                       // from an encrypted container
        for (auto* lib : libraries)
        {
            const auto block = lib->read ("manifest.json");
            if (block.getSize() > 0)
            {
                text = juce::String::fromUTF8 ((const char*) block.getData(), (int) block.getSize());
                break;
            }
        }

#if NEBULA_HAS_EMBEDDED_PRESETS
    if (text.isEmpty())
    {
        int size = 0;
        if (const char* data = BinaryData::getNamedResource ("manifest_json", size))
            text = juce::String::fromUTF8 (data, size);
    }
#endif

    if (text.isEmpty())
        return;

    const auto parsed = juce::JSON::parse (text);
    const auto* arr = parsed.getProperty ("presets", {}).getArray();
    if (arr == nullptr)
        return;

    juce::Array<PresetGroup> ordered;
    for (const auto& entry : *arr)
    {
        const juce::String name = entry.getProperty ("name", "").toString()
                                       .replaceCharacters ("_-", "  ").trim();
        for (int i = 0; i < presets.size(); ++i)
        {
            if (! presets.getReference (i).name.equalsIgnoreCase (name))
                continue;

            auto g = presets.getReference (i);
            const juce::String hex = entry.getProperty ("colour", "").toString();
            if (hex.startsWithChar ('#') && hex.length() == 7)
                g.colour = juce::Colour::fromString ("ff" + hex.substring (1));

            g.defaultFx  = entry.getProperty ("fx", "").toString();
            g.defaultTex = entry.getProperty ("texture", "").toString();

            const auto rv = entry.getProperty ("reverb", {});
            if (rv.isObject())
            {
                g.hasReverbDefaults = true;
                const juce::String type = rv.getProperty ("type", "hall").toString();
                g.rType = type == "room" ? 0 : (type == "plate" ? 1 : 2);
                g.rMix  = (float) (double) rv.getProperty ("mix",  0.4);
                g.rSize = (float) (double) rv.getProperty ("size", 0.85);
                g.rDamp = (float) (double) rv.getProperty ("damp", 0.45);
            }

            ordered.add (g);
            presets.remove (i);
            break;
        }
    }

    // any groups not in the manifest keep default styling, listed after
    for (auto& g : presets)
        ordered.add (g);
    presets = std::move (ordered);
}

void NebulaTideProcessor::prepareToPlay (double sampleRate, int)
{
    deviceSampleRate = sampleRate;
    reverb.setSampleRate (sampleRate);
}

// Audio comes either from a plain file (author mode) or is decrypted out of the
// container into memory — it is never written to disk in playable form.
juce::AudioFormatReader* NebulaTideProcessor::createReaderFor (const PresetSource& src)
{
    if (src.file.existsAsFile())
        return formatManager.createReaderFor (src.file);

    if (src.data != nullptr)
        return formatManager.createReaderFor (
            std::make_unique<juce::MemoryInputStream> (src.data, (size_t) src.dataSize, false));

    if (src.libEntry.isNotEmpty())
        for (auto* lib : libraries)
        {
            auto block = lib->read (src.libEntry);
            if (block.getSize() > 0)
                return formatManager.createReaderFor (
                    std::make_unique<juce::MemoryInputStream> (std::move (block)));
        }
    return nullptr;
}

juce::AudioFormatReader* NebulaTideProcessor::createReaderFor (const AuxSound& s)
{
    if (s.file.existsAsFile())
        return formatManager.createReaderFor (s.file);

    if (s.libEntry.isNotEmpty())
        for (auto* lib : libraries)
        {
            auto block = lib->read (s.libEntry);
            if (block.getSize() > 0)
                return formatManager.createReaderFor (
                    std::make_unique<juce::MemoryInputStream> (std::move (block)));
        }
    return nullptr;
}

void NebulaTideProcessor::startSource (const PresetSource& src)
{
    // Decode on the worker thread: the current sound keeps playing untouched,
    // and the crossfade begins the moment the new buffer is ready.
    const int gen = padLoadGen.fetch_add (1) + 1;
    const PresetSource source = src;

    loadPool.addJob ([this, source, gen]
    {
        std::unique_ptr<juce::AudioFormatReader> reader (createReaderFor (source));
        if (reader == nullptr)
            return;

        juce::AudioBuffer<float> loaded ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&loaded, 0, (int) reader->lengthInSamples, 0, true, true);

        if (gen != padLoadGen.load())   // user already clicked something newer
            return;

        const juce::SpinLock::ScopedLockType sl (voiceLock);
        for (auto& v : voices)
            v.targetGain = 0.0f;

        auto& v = voices[nextVoice];
        nextVoice = (nextVoice + 1) % maxVoices;
        v.buffer = std::move (loaded);
        v.sourceSampleRate = reader->sampleRate;
        v.position = 0.0;
        v.gain = 0.0f;
        v.targetGain = 1.0f;
        v.active = true;
    });
}

void NebulaTideProcessor::selectPad (int index)
{
    if (index < 0 || index >= presets.size())
        return;
    auto& g = presets.getReference (index);

    int key = currentKey.load();
    if (! g.hasKey (key))
        key = g.firstAvailableKey();
    if (key < 0)
        return;

    startSource (g.keys[key]);
    currentPad.store (index);
    currentKey.store (key);
    lastPad.store (index);

    // apply the preset's authored reverb defaults (from Nebula Forge)
    if (g.hasReverbDefaults)
    {
        auto set = [this] (const char* id, float v)
        {
            if (auto* prm = apvts.getParameter (id))
                prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        };
        set ("rmix",  g.rMix);
        set ("rsize", g.rSize);
        set ("rdamp", g.rDamp);
        if (auto* prm = apvts.getParameter ("rtype"))
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) g.rType));
    }

    // arm (or hot-swap) the stars to this preset's authored default sounds
    auto applyAuxDefault = [this] (int cat, const juce::String& fileName)
    {
        if (fileName.isEmpty()) return;
        const auto stem = fileName.upToLastOccurrenceOf (".", false, false)
                              .replaceCharacters ("_-", "  ").trim();
        const auto& list = getAuxSounds (cat);
        for (int i = 0; i < list.size(); ++i)
        {
            if (! list.getReference (i).file.getFileName().equalsIgnoreCase (fileName)
                && ! list.getReference (i).name.equalsIgnoreCase (stem))
                continue;
            if (i != getAuxIndex (cat))
            {
                if (isAuxPlaying (cat)) triggerAux (cat, i);   // playing → crossfade to it
                else                    setAuxIndex (cat, i);  // idle → just arm it
            }
            return;
        }
    };
    applyAuxDefault (0, g.defaultFx);
    applyAuxDefault (1, g.defaultTex);
}

void NebulaTideProcessor::selectKey (int keyIndex)
{
    if (keyIndex < 0 || keyIndex >= 12)
        return;

    const int pad = currentPad.load();
    if (pad < 0)                          // nothing playing: just arm the key
    {
        currentKey.store (keyIndex);
        return;
    }

    auto& g = presets.getReference (pad);
    if (! g.hasKey (keyIndex))
        return;                           // this pad has no variant in that key

    startSource (g.keys[keyIndex]);
    currentKey.store (keyIndex);
}

void NebulaTideProcessor::triggerAux (int cat, int index)
{
    const auto& list = getAuxSounds (cat);
    if (cat < 0 || cat > 1 || index < 0 || index >= list.size())
        return;

    // update selection immediately (UI), decode in the background (no freeze)
    auxIndex[cat].store (index);
    const int gen = auxLoadGen[cat].fetch_add (1) + 1;
    const AuxSound sound = list[index];

    loadPool.addJob ([this, cat, sound, gen]
    {
        std::unique_ptr<juce::AudioFormatReader> reader (createReaderFor (sound));
        if (reader == nullptr)
            return;

        juce::AudioBuffer<float> loaded ((int) reader->numChannels, (int) reader->lengthInSamples);
        reader->read (&loaded, 0, (int) reader->lengthInSamples, 0, true, true);

        if (gen != auxLoadGen[cat].load())   // superseded by a newer click
            return;

        const juce::SpinLock::ScopedLockType sl (voiceLock);
        auto& v = auxVoices[cat];
        v.buffer = std::move (loaded);
        v.sourceSampleRate = reader->sampleRate;
        v.position = 0.0;
        v.gain = 0.0f;
        v.targetGain = 1.0f;
        v.active = true;
    });
}

void NebulaTideProcessor::stopAux (int cat)
{
    if (cat < 0 || cat > 1) return;
    const juce::SpinLock::ScopedLockType sl (voiceLock);
    auxVoices[cat].targetGain = 0.0f;
}

void NebulaTideProcessor::keyCommand (int pitchClass)
{
    if (pitchClass < 0 || pitchClass > 11 || presets.isEmpty())
        return;

    if (currentPad.load() >= 0)
    {
        selectKey (pitchClass);                    // running → crossfade to the key
    }
    else
    {
        currentKey.store (pitchClass);             // idle → start last pad in that key
        selectPad (juce::jlimit (0, presets.size() - 1, lastPad.load()));
    }
}

void NebulaTideProcessor::toggleAux (int cat)
{
    if (cat < 0 || cat > 1) return;
    if (isAuxPlaying (cat)) stopAux (cat);
    else                    triggerAux (cat, getAuxIndex (cat));
}

void NebulaTideProcessor::stopAll()
{
    const juce::SpinLock::ScopedLockType sl (voiceLock);
    for (auto& v : voices)
        v.targetGain = 0.0f;
    // stopping the drone takes the fx/texture stars down with it
    // (they stay independent across preset/key changes, which never call this)
    for (auto& v : auxVoices)
        v.targetGain = 0.0f;
    currentPad.store (-1);
}

void NebulaTideProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // ── MIDI control ──
    // Notes C2+ (36+): pitch class = musical key. Octave 1 = control notes:
    //   24 C1 = FX star toggle · 25 C#1 = texture toggle · 26 D1 = next FX ·
    //   27 D#1 = next texture · 28 E1 = stop everything
    // Program change = preset. CC7 volume · CC10 pan · CC91 reverb mix.
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        const bool isCC = m.isController();
        const bool isNote = m.isNoteOn();

        // note gating (Kontakt-style): release of the last held KEY-zone note fades the pad
        if (m.isNoteOff() || m.isAllNotesOff())
        {
            const int n = m.getNoteNumber();
            if (m.isAllNotesOff())
            {
                heldNotes.store (0);
                for (auto& h : heldKeys) h.store (false);
            }
            else if (n >= 0 && n < 128)
            {
                heldKeys[n].store (false);
                if (zoneOf (n) == 1 && heldNotes.load() > 0) heldNotes.fetch_sub (1);
            }
            if (noteGate.load() && heldNotes.load() == 0 && zoneOf (n) == 1)
                juce::MessageManager::callAsync ([this] { if (heldNotes.load() == 0) stopAll(); });
            continue;
        }
        if (isNote)
        {
            const int n = m.getNoteNumber();
            if (n >= 0 && n < 128) heldKeys[n].store (true);
            lastNote.store (n);
            if (zoneOf (n) == 1) heldNotes.fetch_add (1);
        }

        if (! isCC && ! isNote && ! m.isProgramChange())
            continue;

        // MIDI learn: the next CC or note lands on the armed action
        const int target = learnTarget.load();
        if (target >= 0 && (isCC || isNote))
        {
            const int num = isCC ? m.getControllerNumber() : m.getNoteNumber();
            binding[target].store (0x200 | (isCC ? 0x100 : 0) | num);
            learnTarget.store (-1);
            continue;               // consumed by learn
        }

        if (m.isProgramChange())
        {
            const int prog = m.getProgramChangeNumber();
            juce::MessageManager::callAsync ([this, prog] { selectPad (prog); });
            continue;
        }

        // dispatch against the binding table
        const int num = isCC ? m.getControllerNumber() : m.getNoteNumber();
        const int match = 0x200 | (isCC ? 0x100 : 0) | num;
        bool consumed = false;

        for (int a = 0; a < numMidiActions; ++a)
        {
            if (binding[a].load() != match) continue;
            consumed = true;

            if (a < 6)              // continuous parameter
            {
                const float v = isCC ? (float) m.getControllerValue() / 127.0f
                                     : m.getFloatVelocity();
                if (auto* prm = apvts.getParameter (midiParamIds[a]))
                    prm->setValueNotifyingHost (v);
            }
            else                    // command: notes fire directly; CCs on rising edge
            {
                const int v = isCC ? m.getControllerValue() : 127;
                const int prev = lastCmdVal[a].exchange (v);
                if (! isCC || (v >= 64 && prev < 64))
                    juce::MessageManager::callAsync ([this, a] { runMidiCommand (a); });
            }
        }

        // unbound notes route by key zone
        if (! consumed && isNote)
        {
            const int n = m.getNoteNumber();
            switch (zoneOf (n))
            {
                case 1:     // KEYS C3..B4 → musical key by pitch class
                {
                    const int pc = n % 12;
                    juce::MessageManager::callAsync ([this, pc] { keyCommand (pc); });
                    break;
                }
                case 0:     // FX C2..B2 → one note per FX sound (C2 = first)
                {
                    const int idx = n - fxZoneLo;
                    juce::MessageManager::callAsync ([this, idx]
                    {
                        if (idx < getAuxSounds (0).size())
                        {
                            if (isAuxPlaying (0) && getAuxIndex (0) == idx) stopAux (0);
                            else triggerAux (0, idx);
                        }
                    });
                    break;
                }
                case 2:     // TEXTURES C5..B5 → one note per texture (C5 = first)
                {
                    const int idx = n - texZoneLo;
                    juce::MessageManager::callAsync ([this, idx]
                    {
                        if (idx < getAuxSounds (1).size())
                        {
                            if (isAuxPlaying (1) && getAuxIndex (1) == idx) stopAux (1);
                            else triggerAux (1, idx);
                        }
                    });
                    break;
                }
                default: break;
            }
        }
    }
    midi.clear();

    buffer.clear();

    const float fadeSeconds = apvts.getRawParameterValue ("fade")->load();
    const float fadePerSample = 1.0f / (float) juce::jmax (1.0, fadeSeconds * deviceSampleRate);

    {
        const juce::SpinLock::ScopedTryLockType sl (voiceLock);
        if (sl.isLocked())
            for (auto& v : voices)
                v.render (buffer, buffer.getNumSamples(), deviceSampleRate, fadePerSample);
    }

    // ── reverb: category shapes the space, size/damp fine-tune it ──
    const float mix  = apvts.getRawParameterValue ("rmix")->load();
    const float size = apvts.getRawParameterValue ("rsize")->load();
    const float damp = apvts.getRawParameterValue ("rdamp")->load();
    const int   type = (int) apvts.getRawParameterValue ("rtype")->load();

    juce::Reverb::Parameters rp;
    switch ((ReverbType) type)
    {
        case ReverbType::room:                      // tight, natural, mid-dark
            rp.roomSize = 0.15f + size * 0.45f;
            rp.damping  = 0.35f + damp * 0.55f;
            rp.width    = 0.72f;
            break;
        case ReverbType::plate:                     // dense, bright, ultra-wide
            rp.roomSize = 0.35f + size * 0.45f;
            rp.damping  = damp * 0.40f;
            rp.width    = 1.0f;
            break;
        case ReverbType::hall: default:             // vast, slow, smooth
            rp.roomSize = 0.55f + size * 0.45f;
            rp.damping  = 0.20f + damp * 0.55f;
            rp.width    = 1.0f;
            break;
    }
    rp.wetLevel = mix * 0.7f;
    rp.dryLevel = 1.0f - mix * 0.4f;
    reverb.setParameters (rp);

    if (buffer.getNumChannels() >= 2)
        reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), buffer.getNumSamples());
    else if (buffer.getNumChannels() == 1)
        reverb.processMono (buffer.getWritePointer (0), buffer.getNumSamples());

    // ── FX / texture stars: post-reverb, so they ride the volume/pan knobs ──
    {
        const juce::SpinLock::ScopedTryLockType sl (voiceLock);
        if (sl.isLocked())
            for (auto& v : auxVoices)
                v.render (buffer, buffer.getNumSamples(), deviceSampleRate, fadePerSample);
    }

    // ── pan + volume (constant-power pan) ──
    const float vol = apvts.getRawParameterValue ("volume")->load();
    const float pan = apvts.getRawParameterValue ("pan")->load();
    const float panL = std::cos ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);
    const float panR = std::sin ((pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f);

    if (buffer.getNumChannels() >= 2)
    {
        buffer.applyGain (0, 0, buffer.getNumSamples(), vol * panL * juce::MathConstants<float>::sqrt2);
        buffer.applyGain (1, 0, buffer.getNumSamples(), vol * panR * juce::MathConstants<float>::sqrt2);
    }
    else
    {
        buffer.applyGain (vol);
    }

    outputLevel.store (buffer.getMagnitude (0, buffer.getNumSamples()));
}

void NebulaTideProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    state.setProperty ("currentPad", currentPad.load(), nullptr);
    state.setProperty ("currentKey", currentKey.load(), nullptr);

    juce::StringArray binds;
    for (int i = 0; i < numMidiActions; ++i)
        binds.add (juce::String (binding[i].load()));
    state.setProperty ("midimap", binds.joinIntoString (","), nullptr);
    state.setProperty ("noteGate", noteGate.load(), nullptr);
    state.setProperty ("showKeyboard", showKeyboard.load(), nullptr);
    state.setProperty ("loopBlend", (double) gLoopBlendSeconds.load(), nullptr);
    for (int c = 0; c < 2; ++c)
    {
        state.setProperty ("auxVol" + juce::String (c), auxVoices[c].volume.load(), nullptr);
        state.setProperty ("auxLoop" + juce::String (c), auxVoices[c].looping.load(), nullptr);
    }
    juce::MemoryOutputStream mos (dest, false);
    state.writeToStream (mos);
}

void NebulaTideProcessor::setStateInformation (const void* data, int size)
{
    auto state = juce::ValueTree::readFromData (data, (size_t) size);
    if (! state.isValid()) return;
    apvts.replaceState (state);

    const juce::String map = state.getProperty ("midimap", "").toString();
    if (map.isNotEmpty())
    {
        juce::StringArray binds = juce::StringArray::fromTokens (map, ",", "");
        for (int i = 0; i < juce::jmin ((int) numMidiActions, binds.size()); ++i)
            binding[i].store (binds[i].getIntValue());
    }

    if (state.hasProperty ("noteGate"))
        noteGate.store ((bool) state.getProperty ("noteGate"));
    if (state.hasProperty ("showKeyboard"))
        showKeyboard.store ((bool) state.getProperty ("showKeyboard"));
    if (state.hasProperty ("loopBlend"))
        gLoopBlendSeconds.store (juce::jlimit (0.1f, 6.0f, (float) (double) state.getProperty ("loopBlend")));

    const int pad = state.getProperty ("currentPad", -1);
    const int key = state.getProperty ("currentKey", 0);
    currentKey.store (juce::jlimit (0, 11, key));
    for (int c = 0; c < 2; ++c)
    {
        auxVoices[c].volume.store (juce::jlimit (0.0f, 1.0f,
            (float) (double) state.getProperty ("auxVol" + juce::String (c), 0.5)));
        auxVoices[c].looping.store ((bool) state.getProperty ("auxLoop" + juce::String (c), true));
    }
    if (pad >= 0)
        juce::MessageManager::callAsync ([this, pad] { selectPad (pad); });
}

juce::AudioProcessorEditor* NebulaTideProcessor::createEditor()
{
    return new NebulaTideEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NebulaTideProcessor();
}
