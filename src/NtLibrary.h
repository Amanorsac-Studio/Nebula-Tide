#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>

//==============================================================================
// NTLIB — Nebula Tide's encrypted sound container.
//
// One file holds the whole library (pads, fx, textures, manifest). Contents are
// encrypted with Blowfish-448 in counter mode, keyed by a secret compiled into
// the app, so the audio never exists as a playable file on the user's disk.
// Entries are decrypted straight into memory when a sound is loaded — nothing
// is ever written out.
//
// Layout
//   magic  "NTLIB\0\0\1"                (8 bytes)
//   uint32  index length (encrypted bytes that follow)
//   [index] encrypted, counter starts at 0:
//              uint32 entryCount
//              per entry: string path (UTF-8, uint32 length prefix)
//                         uint64 offset (from start of data section)
//                         uint64 size
//   [data]  each entry encrypted with its own counter stream
//
namespace ntlib
{
    inline constexpr const char* magic = "NTLIB\0\0\1";
    inline constexpr int magicLen = 8;
    inline constexpr const char* fileExtension = ".ntlib";

    // The library secret. Changing this invalidates existing containers.
    inline juce::MemoryBlock secretKey()
    {
        // assembled at runtime so the literal never sits contiguously in the binary
        const char* parts[] = { "N3bu1a", "-T1d3-", "s0und-", "v41t-", "9f2c7a", "e15b83" };
        juce::String s;
        for (auto* p : parts) s << p;
        return juce::MemoryBlock (s.toRawUTF8(), (size_t) s.getNumBytesAsUTF8());
    }

    // Blowfish-CTR keystream applied in place; `blockOffset` lets a caller
    // decrypt a slice of a stream without touching what came before.
    inline void crypt (void* data, size_t numBytes, juce::int64 blockOffset = 0)
    {
        const auto key = secretKey();
        const juce::BlowFish bf (key.getData(), (int) key.getSize());
        auto* bytes = static_cast<juce::uint8*> (data);
        juce::int64 block = blockOffset;

        for (size_t i = 0; i < numBytes; i += 8, ++block)
        {
            juce::uint32 l = (juce::uint32) (block & 0xffffffff);
            juce::uint32 r = (juce::uint32) ((block >> 32) ^ 0x9e3779b9);
            bf.encrypt (l, r);                       // keystream block

            const juce::uint8 ks[8] = {
                (juce::uint8) l,        (juce::uint8) (l >> 8),
                (juce::uint8) (l >> 16),(juce::uint8) (l >> 24),
                (juce::uint8) r,        (juce::uint8) (r >> 8),
                (juce::uint8) (r >> 16),(juce::uint8) (r >> 24) };

            const size_t n = juce::jmin ((size_t) 8, numBytes - i);
            for (size_t k = 0; k < n; ++k)
                bytes[i + k] ^= ks[k];
        }
    }

    //==========================================================================
    // Reads a container. Entry data is decrypted on demand, in memory only.
    class Reader
    {
    public:
        struct Entry { juce::String path; juce::int64 offset = 0, size = 0; };

        bool open (const juce::File& f)
        {
            entries.clear();
            stream = f.createInputStream();
            if (stream == nullptr) return false;

            char m[magicLen] = {};
            if (stream->read (m, magicLen) != magicLen
                || juce::MemoryBlock (m, magicLen) != juce::MemoryBlock (magic, magicLen))
                return false;

            const int indexLen = (int) stream->readInt();
            if (indexLen <= 0 || indexLen > (1 << 24)) return false;

            juce::MemoryBlock idx ((size_t) indexLen);
            if (stream->read (idx.getData(), indexLen) != indexLen) return false;
            crypt (idx.getData(), idx.getSize(), 0);

            dataStart = stream->getPosition();
            juce::MemoryInputStream in (idx, false);
            const int count = in.readInt();
            if (count < 0 || count > 100000) return false;

            for (int i = 0; i < count; ++i)
            {
                Entry e;
                const int len = in.readInt();
                if (len < 0 || len > 4096) return false;
                juce::MemoryBlock nameBytes ((size_t) len);
                in.read (nameBytes.getData(), len);
                e.path = juce::String::fromUTF8 ((const char*) nameBytes.getData(), len);
                e.offset = in.readInt64();
                e.size   = in.readInt64();
                if (e.size < 0) return false;
                entries.add (e);
            }
            file = f;
            return ! entries.isEmpty();
        }

        const juce::Array<Entry>& getEntries() const noexcept { return entries; }
        juce::File getFile() const noexcept { return file; }

        // Decrypts one entry into memory. Returns an empty block on failure.
        juce::MemoryBlock read (const Entry& e) const
        {
            juce::MemoryBlock out;
            if (stream == nullptr || e.size <= 0) return out;
            out.setSize ((size_t) e.size);
            const juce::ScopedLock sl (lock);
            if (! stream->setPosition (dataStart + e.offset)) return {};
            if (stream->read (out.getData(), (int) e.size) != (int) e.size) return {};
            crypt (out.getData(), out.getSize(), 0);
            return out;
        }

        juce::MemoryBlock read (const juce::String& path) const
        {
            for (const auto& e : entries)
                if (e.path.equalsIgnoreCase (path))
                    return read (e);
            return {};
        }

    private:
        juce::File file;
        std::unique_ptr<juce::InputStream> stream;
        juce::Array<Entry> entries;
        juce::int64 dataStart = 0;
        juce::CriticalSection lock;
    };
}
