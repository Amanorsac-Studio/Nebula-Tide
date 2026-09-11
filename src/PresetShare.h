#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_graphics/juce_graphics.h>   // for juce::Colour, which is part of a preset

// ── Sharing a preset as one file ─────────────────────────────────────────
//
// A user preset on disk is twelve audio files whose names carry the key.
// That is fine for the app and hopeless for sending to somebody: rename one
// file and a key goes silent, and there is nothing in the folder that says
// who made it or what colour it should be.
//
// A .ntpreset is that folder packed into a single zip, with a small JSON
// alongside the audio carrying the name, the maker, the colour and the
// reverb defaults. Deliberately not encrypted — the whole point is that
// another copy of Nebula Tide, on somebody else's machine, can open it. The
// encrypted container is for our library; this is for theirs.
//
// Audio is transcoded to FLAC on the way out. Lossless, and it roughly halves
// an AIFF preset, which is the difference between a file people actually send
// and one they give up on.
//
// No GUI in here, so the round trip can be tested without launching anything.

namespace presetshare
{

static constexpr const char* extension = ".ntpreset";
static constexpr int formatVersion = 1;

static const char* const keyNames[12] =
    { "C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B" };

struct Meta
{
    juce::String name;
    juce::String maker;                 // free text, may be empty
    juce::Colour colour { 0xff4aa3c7 };
    int   reverbType = 0;
    float mix = 0.3f, size = 0.5f, damp = 0.5f;
};

// Everything the app needs to know about a pack without unpacking it.
struct Contents
{
    Meta meta;
    int  numKeys = 0;
    bool keyPresent[12] {};
};

//==============================================================================
namespace detail
{
    inline juce::String sanitise (const juce::String& s)
    {
        return s.retainCharacters ("abcdefghijklmnopqrstuvwxyz"
                                   "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 _-").trim();
    }

    // Decode whatever the source is and re-encode it as FLAC.
    inline bool toFlac (const juce::File& src, const juce::File& dest, juce::String& error)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (src));
        if (reader == nullptr)
        {
            error = "Could not read " + src.getFileName();
            return false;
        }

        dest.deleteFile();
        std::unique_ptr<juce::FileOutputStream> out (dest.createOutputStream());
        if (out == nullptr)
        {
            error = "Could not write " + dest.getFileName();
            return false;
        }

        juce::FlacAudioFormat flac;
        // FLAC stores 24-bit at most; anything higher is float source material
        // and 24 bits is past the point of audibility either way.
        const int bits = (int) juce::jlimit ((unsigned int) 16, (unsigned int) 24,
                                             reader->bitsPerSample);
        std::unique_ptr<juce::AudioFormatWriter> writer (
            flac.createWriterFor (out.get(), reader->sampleRate,
                                  reader->numChannels, bits, {}, 5));
        if (writer == nullptr)
        {
            error = "Could not encode " + src.getFileName() + " as FLAC";
            return false;
        }
        out.release();      // the writer owns the stream from here

        if (! writer->writeFromAudioReader (*reader, 0, reader->lengthInSamples))
        {
            error = "Ran out of room writing " + dest.getFileName();
            return false;
        }
        return true;
    }

    inline juce::String metaToJson (const Meta& m, const bool present[12])
    {
        juce::DynamicObject::Ptr root (new juce::DynamicObject());
        root->setProperty ("format", formatVersion);
        root->setProperty ("name", m.name);
        root->setProperty ("maker", m.maker);
        root->setProperty ("colour", m.colour.toDisplayString (false));
        root->setProperty ("reverbType", m.reverbType);
        root->setProperty ("mix", m.mix);
        root->setProperty ("size", m.size);
        root->setProperty ("damp", m.damp);

        juce::Array<juce::var> keys;
        for (int i = 0; i < 12; ++i)
            if (present[i]) keys.add (juce::String (keyNames[i]));
        root->setProperty ("keys", keys);

        return juce::JSON::toString (juce::var (root.get()), false);
    }
}

//==============================================================================
/** Packs up to twelve key files into one shareable .ntpreset.
    `keys` is indexed 0..11 = C..B; entries that do not exist are skipped.
*/
inline juce::Result writePack (const juce::File& dest, const Meta& metaIn,
                               const juce::File keys[12])
{
    Meta meta = metaIn;
    meta.name = detail::sanitise (meta.name);
    meta.maker = detail::sanitise (meta.maker);
    if (meta.name.isEmpty())
        return juce::Result::fail ("Give the preset a name before sharing it.");

    bool present[12] {};
    int count = 0;
    for (int i = 0; i < 12; ++i)
        if (keys[i].existsAsFile()) { present[i] = true; ++count; }
    if (count == 0)
        return juce::Result::fail ("This preset has no sounds in it yet.");

    // Transcoding goes to a scratch folder that is removed whatever happens,
    // including on the failure paths below.
    auto scratch = juce::File::getSpecialLocation (juce::File::tempDirectory)
                       .getChildFile ("ntpreset-" + juce::String (juce::Random::getSystemRandom().nextInt (1 << 30)));
    if (! scratch.createDirectory())
        return juce::Result::fail ("Could not create a temporary folder.");
    const struct Cleanup { juce::File dir; ~Cleanup() { dir.deleteRecursively(); } } cleanup { scratch };

    juce::ZipFile::Builder builder;
    juce::OwnedArray<juce::File> encoded;       // must outlive writeToStream

    for (int i = 0; i < 12; ++i)
    {
        if (! present[i]) continue;
        auto* flacFile = encoded.add (new juce::File (scratch.getChildFile (juce::String (keyNames[i]) + ".flac")));
        juce::String error;
        if (! detail::toFlac (keys[i], *flacFile, error))
            return juce::Result::fail (error);
        // Already FLAC-compressed, so asking the zip to squeeze it again only
        // costs time.
        builder.addFile (*flacFile, 0, juce::String (keyNames[i]) + ".flac");
    }

    auto jsonFile = scratch.getChildFile ("preset.json");
    if (! jsonFile.replaceWithText (detail::metaToJson (meta, present)))
        return juce::Result::fail ("Could not write the preset details.");
    builder.addFile (jsonFile, 9, "preset.json");

    dest.deleteFile();
    std::unique_ptr<juce::FileOutputStream> out (dest.createOutputStream());
    if (out == nullptr)
        return juce::Result::fail ("Could not write " + dest.getFullPathName());

    double progress = 0.0;
    if (! builder.writeToStream (*out, &progress))
        return juce::Result::fail ("Could not finish writing " + dest.getFileName());

    return juce::Result::ok();
}

//==============================================================================
/** Reads the details out of a pack without unpacking the audio. */
inline juce::Result peekPack (const juce::File& src, Contents& out)
{
    juce::ZipFile zip (src);
    const int jsonIndex = zip.getIndexOfFileName ("preset.json");
    if (jsonIndex < 0)
        return juce::Result::fail (src.getFileName() + " is not a Nebula Tide preset.");

    std::unique_ptr<juce::InputStream> stream (zip.createStreamForEntry (jsonIndex));
    if (stream == nullptr)
        return juce::Result::fail ("Could not read the preset details.");

    const auto parsed = juce::JSON::parse (stream->readEntireStreamAsString());
    auto* obj = parsed.getDynamicObject();
    if (obj == nullptr)
        return juce::Result::fail ("The preset details are damaged.");

    const int version = (int) obj->getProperty ("format");
    if (version > formatVersion)
        return juce::Result::fail ("This preset was made by a newer version of Nebula Tide.");

    out = {};
    out.meta.name  = obj->getProperty ("name").toString();
    out.meta.maker = obj->getProperty ("maker").toString();
    out.meta.colour = juce::Colour::fromString ("ff" + obj->getProperty ("colour").toString());
    out.meta.reverbType = (int) obj->getProperty ("reverbType");
    out.meta.mix  = (float) (double) obj->getProperty ("mix");
    out.meta.size = (float) (double) obj->getProperty ("size");
    out.meta.damp = (float) (double) obj->getProperty ("damp");

    if (out.meta.name.isEmpty())
        return juce::Result::fail ("The preset has no name.");

    // Trust the audio that is actually in the zip over the list in the JSON:
    // the list is a convenience, the entries are the truth.
    for (int i = 0; i < 12; ++i)
        if (zip.getIndexOfFileName (juce::String (keyNames[i]) + ".flac") >= 0)
        {
            out.keyPresent[i] = true;
            ++out.numKeys;
        }

    if (out.numKeys == 0)
        return juce::Result::fail ("The preset has no sounds in it.");

    return juce::Result::ok();
}

/** Unpacks into `destDir` using the app's "<Name>_<Key>.flac" convention.
    Returns the name it landed under, which may differ from the one in the
    pack if `nameOverride` is given to avoid clobbering something.
*/
inline juce::Result readPack (const juce::File& src, const juce::File& destDir,
                              Contents& out, const juce::String& nameOverride = {})
{
    auto r = peekPack (src, out);
    if (r.failed()) return r;

    const auto name = detail::sanitise (nameOverride.isNotEmpty() ? nameOverride : out.meta.name);
    if (name.isEmpty())
        return juce::Result::fail ("The preset has no usable name.");
    out.meta.name = name;

    if (! destDir.createDirectory())
        return juce::Result::fail ("Could not create " + destDir.getFullPathName());

    juce::ZipFile zip (src);
    const auto stem = name.replaceCharacter (' ', '_');

    for (int i = 0; i < 12; ++i)
    {
        if (! out.keyPresent[i]) continue;
        const auto entry = juce::String (keyNames[i]) + ".flac";
        const int index = zip.getIndexOfFileName (entry);
        std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (index));
        if (in == nullptr)
            return juce::Result::fail ("Could not read " + entry + " out of the preset.");

        const auto dest = destDir.getChildFile (stem + "_" + keyNames[i] + ".flac");
        dest.deleteFile();
        std::unique_ptr<juce::FileOutputStream> o (dest.createOutputStream());
        if (o == nullptr || ! o->writeFromInputStream (*in, -1))
            return juce::Result::fail ("Could not write " + dest.getFileName());
    }
    return juce::Result::ok();
}

} // namespace presetshare
