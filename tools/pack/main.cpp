// NebulaPack — builds the encrypted .ntlib sound container from a presets folder.
//   NebulaPack <presetsFolder> <out.ntlib> [--ogg <quality 0-10>]
//   NebulaPack --verify <out.ntlib>
//
// --ogg transcodes the audio to Ogg Vorbis while packing. Desktop ships lossless
// FLAC; phones and tablets ship Ogg so the app stays inside the app-store size
// limits (Google Play caps delivery at 150 MB).
#include "../../src/NtLibrary.h"
#include <juce_audio_formats/juce_audio_formats.h>

// How each entry is stored in the container.
//   raw   keep the source bytes exactly
//   flac  lossless, but compressed - the right default for desktop, because
//         an AIFF bounce is uncompressed PCM and roughly doubles the download
//         for no gain in quality
//   ogg   lossy, for mobile where the store size limits bite
enum class Encode { raw, flac, ogg };

// Returns the file's bytes, transcoded as requested.
static juce::MemoryBlock loadEntry (const juce::File& f, Encode mode, int oggQuality,
                                    juce::String& nameInOut)
{
    juce::MemoryBlock out;

    if (mode == Encode::raw || f.getFileName().equalsIgnoreCase ("manifest.json"))
    {
        f.loadFileAsData (out);
        return out;
    }

    // Already in the target format: copying the bytes beats decoding and
    // re-encoding them, which would only lose quality.
    if (mode == Encode::flac && f.hasFileExtension ("flac"))
    {
        f.loadFileAsData (out);
        return out;
    }
    if (mode == Encode::ogg && f.hasFileExtension ("ogg"))
    {
        f.loadFileAsData (out);
        return out;
    }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
    if (reader == nullptr) { f.loadFileAsData (out); return out; }

    juce::AudioBuffer<float> buf ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&buf, 0, (int) reader->lengthInSamples, 0, true, true);

    auto stream = std::make_unique<juce::MemoryOutputStream> (out, false);
    std::unique_ptr<juce::AudioFormatWriter> writer;

    juce::OggVorbisAudioFormat ogg;
    juce::FlacAudioFormat flac;
    if (mode == Encode::ogg)
        writer.reset (ogg.createWriterFor (stream.get(), reader->sampleRate,
                                           (unsigned int) buf.getNumChannels(), 16, {}, oggQuality));
    else
        writer.reset (flac.createWriterFor (stream.get(), reader->sampleRate,
                                            (unsigned int) buf.getNumChannels(), 24, {}, 5));

    if (writer == nullptr) { f.loadFileAsData (out); return out; }
    stream.release();   // the writer owns it now

    writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    writer.reset();     // flush

    nameInOut = nameInOut.upToLastOccurrenceOf (".", false, false)
              + (mode == Encode::ogg ? ".ogg" : ".flac");
    return out;
}

static int pack (const juce::File& dir, const juce::File& out, Encode mode, int oggQuality)
{
    if (! dir.isDirectory())
    {
        std::cerr << "not a folder: " << dir.getFullPathName() << "\n";
        return 1;
    }

    juce::Array<juce::File> files;
    for (auto& f : dir.findChildFiles (juce::File::findFiles, true))
    {
        const auto ext = f.getFileExtension().toLowerCase();
        // ".aif" matters: Studio One bounces use it, and leaving it out here
        // dropped 36 files from the container without a word of complaint.
        if (ext == ".flac" || ext == ".wav" || ext == ".mp3" || ext == ".ogg"
            || ext == ".aiff" || ext == ".aif" || f.getFileName() == "manifest.json")
            files.add (f);
    }
    files.sort();
    if (files.isEmpty()) { std::cerr << "no sound files found\n"; return 1; }

    // load (and optionally transcode) everything first so the index carries the
    // real stored sizes
    juce::StringArray names;
    juce::Array<juce::MemoryBlock> blocks;
    for (auto& f : files)
    {
        auto name = f.getRelativePathFrom (dir).replaceCharacter ('\\', '/');
        auto data = loadEntry (f, mode, oggQuality, name);
        if (data.getSize() == 0) { std::cerr << "cannot read " << f.getFullPathName() << "\n"; return 1; }
        names.add (name);
        blocks.add (std::move (data));
    }

    // index: paths relative to the folder, always with forward slashes
    juce::MemoryOutputStream index;
    index.writeInt (names.size());
    juce::int64 offset = 0;
    for (int i = 0; i < names.size(); ++i)
    {
        const auto utf8 = names[i].toRawUTF8();
        const int len = (int) names[i].getNumBytesAsUTF8();
        index.writeInt (len);
        index.write (utf8, (size_t) len);
        index.writeInt64 (offset);
        index.writeInt64 ((juce::int64) blocks.getReference (i).getSize());
        offset += (juce::int64) blocks.getReference (i).getSize();
    }

    juce::MemoryBlock indexBytes (index.getData(), index.getDataSize());
    ntlib::crypt (indexBytes.getData(), indexBytes.getSize(), 0);

    out.deleteFile();
    juce::FileOutputStream os (out);
    if (! os.openedOk()) { std::cerr << "cannot write " << out.getFullPathName() << "\n"; return 1; }
    os.write (ntlib::magic, ntlib::magicLen);
    os.writeInt ((int) indexBytes.getSize());
    os.write (indexBytes.getData(), indexBytes.getSize());

    for (int i = 0; i < blocks.size(); ++i)
    {
        auto& data = blocks.getReference (i);
        ntlib::crypt (data.getData(), data.getSize(), 0);
        os.write (data.getData(), data.getSize());
    }
    os.flush();

    std::cout << out.getFullPathName() << "  " << files.size() << " entries, "
              << (out.getSize() / (1024 * 1024)) << " MB\n";
    return 0;
}

static int verify (const juce::File& lib)
{
    ntlib::Reader r;
    if (! r.open (lib)) { std::cerr << "cannot open container\n"; return 1; }

    int audio = 0, bad = 0, decoded = 0;
    for (const auto& e : r.getEntries())
    {
        // Every audio format has to be counted, not just the two we happen to
        // write. Checking only .flac and .ogg meant 36 AIFF entries were packed
        // and reported as fine without ever being opened.
        const bool isFlac = e.path.endsWithIgnoreCase (".flac");
        const bool isOgg  = e.path.endsWithIgnoreCase (".ogg");
        const bool isAiff = e.path.endsWithIgnoreCase (".aif") || e.path.endsWithIgnoreCase (".aiff");
        const bool isWav  = e.path.endsWithIgnoreCase (".wav");
        if (isFlac || isOgg || isAiff || isWav) ++audio;
        const auto block = r.read (e);
        if ((juce::int64) block.getSize() != e.size) { ++bad; continue; }
        // decrypted audio must start with its format's signature
        const auto sig = juce::String::fromUTF8 ((const char*) block.getData(), 4);
        if ((isFlac && sig != "fLaC") || (isOgg && sig != "OggS")
            || (isAiff && sig != "FORM") || (isWav && sig != "RIFF"))
            { ++bad; continue; }

        // decode a few entries exactly the way the app does, to prove the audio
        // really survives the round trip
        if ((isFlac || isOgg) && decoded < 3)
        {
            juce::AudioFormatManager fm;
            fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (
                std::make_unique<juce::MemoryInputStream> (block, false)));
            if (rd == nullptr || rd->lengthInSamples < 1000) { ++bad; continue; }

            juce::AudioBuffer<float> buf ((int) rd->numChannels, 48000);
            rd->read (&buf, 0, 48000, 0, true, true);
            const float peak = buf.getMagnitude (0, 48000);
            std::cout << "  decoded " << e.path << ": " << rd->lengthInSamples << " samples, "
                      << (int) rd->sampleRate << " Hz, peak " << peak << "\n";
            if (peak <= 0.0001f) ++bad;     // silence means something is wrong
            ++decoded;
        }
    }
    std::cout << "entries=" << r.getEntries().size() << " audio=" << audio << " bad=" << bad << "\n";
    return bad == 0 && audio > 0 ? 0 : 1;
}

int main (int argc, char* argv[])
{
    if (argc == 3 && juce::String (argv[1]) == "--verify")
        return verify (juce::File (juce::String::fromUTF8 (argv[2])));

    if (argc < 3)
    {
        std::cerr << "usage: NebulaPack <presetsFolder> <out.ntlib> [--ogg <quality 0-10>] [--raw]\n"
                     "       NebulaPack --verify <out.ntlib>\n";
        return 2;
    }

    // Desktop defaults to FLAC rather than raw: lossless either way, but an
    // uncompressed AIFF bounce is about twice the size for nothing.
    Encode mode = Encode::flac;
    int oggQuality = -1;
    for (int i = 3; i < argc - 1; ++i)
        if (juce::String (argv[i]) == "--ogg")
        {
            mode = Encode::ogg;
            oggQuality = juce::String (argv[i + 1]).getIntValue();
        }
        else if (juce::String (argv[i]) == "--raw")
            mode = Encode::raw;

    return pack (juce::File (juce::String::fromUTF8 (argv[1])),
                 juce::File (juce::String::fromUTF8 (argv[2])), mode, oggQuality);
}
