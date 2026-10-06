#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_gui_basics/juce_gui_basics.h>

// ── Drag to timeline ──────────────────────────────────────────────────────
//
// Stylus RMX style: grab the handle in the plugin window and drop it on a DAW
// track, and you get a real MIDI clip. The clip is written against Nebula
// Tide's own key zones, so playing it straight back into the plugin reproduces
// exactly what you were hearing when you dragged it:
//
//     FX       C2..B2   one note per FX sound
//     KEYS     C3..B4   pitch class = musical key
//     TEXTURES C5..B5   one note per texture
//
// Notes are sustained for the whole clip rather than pulsed, because that is
// how the instrument is actually played — hold the key, let it breathe.

namespace mididrag
{

struct Clip
{
    int    key = 0;              // pitch class 0..11, mapped into the KEYS zone
    int    fxNote = -1;          // -1 = not playing
    int    texNote = -1;
    double bpm = 120.0;
    int    bars = 4;
    int    auxChannel = 16;      // FX/textures ride their own channel
    juce::String name { "Nebula Tide" };
};

// Writes the clip to a temp .mid file and returns it, or an invalid File on
// failure. The DAW copies the file on drop, so the temp copy is disposable.
inline juce::File writeClip (const Clip& clip)
{
    const int tpqn = 960;
    const double beats = juce::jmax (1, clip.bars) * 4.0;
    const int end = juce::roundToInt (beats * tpqn);

    juce::MidiMessageSequence seq;

    // Track name shows up as the clip name in most hosts.
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, clip.name), 0.0);

    // Tempo, so the clip keeps its length if it lands in a differently-tempo'd
    // project. Hosts that impose their own tempo simply ignore this.
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (
                      juce::roundToInt (60000000.0 / juce::jmax (20.0, clip.bpm))), 0.0);

    auto addHeld = [&seq, end] (int channel, int note)
    {
        if (note < 0 || note > 127) return;
        seq.addEvent (juce::MidiMessage::noteOn  (channel, note, (juce::uint8) 100), 0.0);
        seq.addEvent (juce::MidiMessage::noteOff (channel, note), (double) end);
    };

    // The key goes on channel 1, where it reads as an ordinary played note and
    // works whether or not chord mode is on. FX and textures go on the aux
    // channel, out of reach of anything being played by hand.
    addHeld (1, 48 + juce::jlimit (0, 11, clip.key));
    addHeld (clip.auxChannel, clip.fxNote);
    addHeld (clip.auxChannel, clip.texNote);

    seq.addEvent (juce::MidiMessage::endOfTrack(), (double) end);
    seq.updateMatchedPairs();

    juce::MidiFile mf;
    mf.setTicksPerQuarterNote (tpqn);
    mf.addTrack (seq);

    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                    .getChildFile (clip.name.replaceCharacters (" /\\:", "----")
                                   + "-" + juce::String (juce::Time::currentTimeMillis())
                                   + ".mid");

    file.deleteFile();
    if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
    {
        if (mf.writeTo (*stream))
        {
            stream->flush();
            return file;
        }
    }
    return {};
}

// Starts the external drag. Must be called from a mouse drag on `source`.
inline bool startDrag (juce::Component* source, const Clip& clip)
{
    const auto file = writeClip (clip);
    if (! file.existsAsFile())
        return false;

    // canMoveFiles = false: the host copies it, our temp file stays put.
    return juce::DragAndDropContainer::performExternalDragDropOfFiles (
        juce::StringArray { file.getFullPathName() }, false, source, nullptr);
}

} // namespace mididrag
