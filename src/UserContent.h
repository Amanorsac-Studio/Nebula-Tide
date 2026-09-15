#pragma once
#include <juce_core/juce_core.h>

// ── Naming and scanning ───────────────────────────────────────────────────
//
// Pulled out of PluginProcessor.cpp so it can be tested on its own. The user
// content path is the one place where somebody else's files, named however
// they happen to be named, decide what the instrument loads — so it is worth
// being able to prove it groups them correctly without launching the app.
//
// Filename convention, the same for the shipped library and for user content:
//     "<PresetName>_<Key>.<ext>"     Deep_Current_Eb.flac, My Pad G.wav
// A file with no recognisable key token is treated as the key of C.

namespace usercontent
{

inline int parseKeyToken (const juce::String& token)
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

inline void splitNameAndKey (const juce::String& stem, juce::String& outName, int& outKey)
{
    // the last token separated by '_', '-' or space
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

struct Scanned
{
    juce::String name;
    juce::File   keys[12];
    int numKeys() const
    {
        int n = 0;
        for (const auto& k : keys) if (k.existsAsFile()) ++n;
        return n;
    }
};

// Groups every audio file directly inside `dir` into presets by name.
// Non-recursive on purpose: fx/ and textures/ are scanned separately, and a
// stray folder of samples underneath should not become a phantom preset.
inline juce::Array<Scanned> scanFolder (const juce::File& dir)
{
    juce::Array<Scanned> out;
    if (! dir.isDirectory())
        return out;

    auto files = dir.findChildFiles (juce::File::findFiles, false,
                                     "*.wav;*.mp3;*.ogg;*.flac;*.aiff;*.aif");
    // Sorted so the result does not depend on the order the filesystem hands
    // them back, which differs between machines and makes bugs unrepeatable.
    files.sort();

    for (const auto& f : files)
    {
        juce::String name;
        int key = 0;
        splitNameAndKey (f.getFileNameWithoutExtension(), name, key);
        if (name.isEmpty()) continue;

        Scanned* found = nullptr;
        for (auto& g : out)
            if (g.name.equalsIgnoreCase (name)) { found = &g; break; }

        if (found == nullptr)
        {
            Scanned g;
            g.name = name;
            out.add (g);
            found = &out.getReference (out.size() - 1);
        }
        // First file wins a slot, so a duplicate cannot silently replace it.
        if (! found->keys[key].existsAsFile())
            found->keys[key] = f;
    }
    return out;
}

//==============================================================================
// ── Matching a folder of stems to keys ───────────────────────────────────
// For importing a whole folder at once. The filenames already say which key
// each stem is in, so the app guesses and the person only corrects the
// guesses, rather than assigning twelve files one by one. Every match here is
// a suggestion shown in a list, never applied without being seen.

// The key a filename names, or -1. Looks at the last word first, then the
// first, so "Aurora_Veil_Db", "Preset 1 - E" and "Bb Warm Pad" all resolve,
// while a bounce stray like "Preset 2 - A Output Audio Bus L" finds nothing.
inline int detectKey (const juce::String& stem)
{
    auto tokens = juce::StringArray::fromTokens (stem.replaceCharacters ("_-", "  "), " ", "");
    tokens.removeEmptyStrings();
    if (tokens.isEmpty())
        return -1;

    if (const int k = parseKeyToken (tokens[tokens.size() - 1]); k >= 0)
        return k;

    if (tokens.size() > 1)
        if (const int k = parseKeyToken (tokens[0]); k >= 0)
            return k;

    return -1;
}

struct FolderMatch
{
    juce::Array<juce::File> files;   // every audio file in the folder, sorted
    juce::File keys[12];             // the suggested file for each key
    juce::String suggestedName;      // the preset name the files share

    int numMatched() const
    {
        int n = 0;
        for (const auto& k : keys) if (k.existsAsFile()) ++n;
        return n;
    }
};

inline FolderMatch matchFolder (const juce::File& dir)
{
    FolderMatch m;
    if (! dir.isDirectory())
        return m;

    m.files = dir.findChildFiles (juce::File::findFiles, false,
                                  "*.wav;*.mp3;*.ogg;*.flac;*.aiff;*.aif");
    m.files.sort();

    for (const auto& f : m.files)
    {
        const int k = detectKey (f.getFileNameWithoutExtension());
        // First file in name order claims a key, so a second take of the same
        // key stays available in the list instead of silently replacing it.
        if (k < 0 || m.keys[k].existsAsFile())
            continue;

        m.keys[k] = f;
        if (m.suggestedName.isEmpty())
        {
            juce::String name;
            int unused = 0;
            splitNameAndKey (f.getFileNameWithoutExtension(), name, unused);
            m.suggestedName = name;
        }
    }
    return m;
}

} // namespace usercontent
