// NebulaConvert — CLI: convert any supported audio file to FLAC,
// optionally loudness-normalizing to -16 dBFS RMS (clip-protected).
// Usage: NebulaConvert <input> <output.flac> [-n16]
#include <juce_audio_formats/juce_audio_formats.h>

int main (int argc, char* argv[])
{
    if (argc < 3)
    {
        std::cerr << "usage: NebulaConvert <input> <output.flac> [-n16]\n";
        return 2;
    }

    // -n<db> e.g. -n24 → normalize RMS to -24 dBFS
    double normTargetDb = 0.0;
    bool normalize = false;
    if (argc > 3)
    {
        const juce::String flag (argv[3]);
        if (flag.startsWith ("-n"))
        {
            normTargetDb = -flag.substring (2).getDoubleValue();
            normalize = normTargetDb < 0.0;
        }
    }

    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    const juce::File in (juce::String::fromUTF8 (argv[1]));
    const juce::File out (juce::String::fromUTF8 (argv[2]));

    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (in));
    if (reader == nullptr)
    {
        std::cerr << "cannot read: " << in.getFullPathName() << "\n";
        return 1;
    }

    const auto numSamples = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> buf ((int) reader->numChannels, numSamples);
    reader->read (&buf, 0, numSamples, 0, true, true);

    if (normalize && numSamples > 0)
    {
        // RMS across all channels → gain to hit -16 dBFS, capped so peaks
        // never exceed -1 dBFS.
        double sumSq = 0.0;
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            const float* d = buf.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                sumSq += (double) d[i] * d[i];
        }
        const double rms = std::sqrt (sumSq / ((double) numSamples * buf.getNumChannels()));
        if (rms > 0.000001)
        {
            const double target = std::pow (10.0, normTargetDb / 20.0);
            double gain = target / rms;
            const float peak = buf.getMagnitude (0, numSamples);
            const double peakCeiling = std::pow (10.0, -1.0 / 20.0); // -1 dBFS
            if (peak * gain > peakCeiling)
                gain = peakCeiling / peak;
            buf.applyGain ((float) gain);
        }
    }

    const int bitDepth = juce::jlimit (16, 24, (int) reader->bitsPerSample == 0 ? 16 : (int) reader->bitsPerSample);

    out.deleteFile();
    juce::FlacAudioFormat flac;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        flac.createWriterFor (new juce::FileOutputStream (out),
                              reader->sampleRate,
                              (unsigned int) buf.getNumChannels(),
                              bitDepth,
                              {}, 5 /* compression */));
    if (writer == nullptr)
    {
        std::cerr << "cannot create FLAC writer\n";
        return 1;
    }

    if (! writer->writeFromAudioSampleBuffer (buf, 0, numSamples))
    {
        std::cerr << "write failed\n";
        return 1;
    }
    writer->flush();

    std::cout << out.getFullPathName() << " (" << out.getSize() << " bytes)\n";
    return 0;
}
