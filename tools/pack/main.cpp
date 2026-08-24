// NebulaPack — builds the encrypted .ntlib sound container from a presets folder.
//   NebulaPack <presetsFolder> <out.ntlib>
//   NebulaPack --verify <out.ntlib>
#include "../../src/NtLibrary.h"

static int pack (const juce::File& dir, const juce::File& out)
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
        if (ext == ".flac" || ext == ".wav" || ext == ".mp3" || ext == ".ogg"
            || ext == ".aiff" || f.getFileName() == "manifest.json")
            files.add (f);
    }
    files.sort();
    if (files.isEmpty()) { std::cerr << "no sound files found\n"; return 1; }

    // index: paths relative to the folder, always with forward slashes
    juce::MemoryOutputStream index;
    index.writeInt (files.size());
    juce::int64 offset = 0;
    for (auto& f : files)
    {
        const auto rel = f.getRelativePathFrom (dir).replaceCharacter ('\\', '/');
        const auto utf8 = rel.toRawUTF8();
        const int len = (int) rel.getNumBytesAsUTF8();
        index.writeInt (len);
        index.write (utf8, (size_t) len);
        index.writeInt64 (offset);
        index.writeInt64 (f.getSize());
        offset += f.getSize();
    }

    juce::MemoryBlock indexBytes (index.getData(), index.getDataSize());
    ntlib::crypt (indexBytes.getData(), indexBytes.getSize(), 0);

    out.deleteFile();
    juce::FileOutputStream os (out);
    if (! os.openedOk()) { std::cerr << "cannot write " << out.getFullPathName() << "\n"; return 1; }
    os.write (ntlib::magic, ntlib::magicLen);
    os.writeInt ((int) indexBytes.getSize());
    os.write (indexBytes.getData(), indexBytes.getSize());

    for (auto& f : files)
    {
        juce::MemoryBlock data;
        if (! f.loadFileAsData (data)) { std::cerr << "cannot read " << f.getFullPathName() << "\n"; return 1; }
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

    int audio = 0, bad = 0;
    for (const auto& e : r.getEntries())
    {
        if (e.path.endsWithIgnoreCase (".flac")) ++audio;
        const auto block = r.read (e);
        if ((juce::int64) block.getSize() != e.size) { ++bad; continue; }
        // FLAC files must start with "fLaC" once decrypted
        if (e.path.endsWithIgnoreCase (".flac")
            && juce::String::fromUTF8 ((const char*) block.getData(), 4) != "fLaC")
            ++bad;
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
        std::cerr << "usage: NebulaPack <presetsFolder> <out.ntlib>\n"
                     "       NebulaPack --verify <out.ntlib>\n";
        return 2;
    }
    return pack (juce::File (juce::String::fromUTF8 (argv[1])),
                 juce::File (juce::String::fromUTF8 (argv[2])));
}
