// Nebula Tide — offline checks for the v2 DSP and chord logic.
//
// These are numeric, not "does it compile": the shimmer is measured with a DFT
// to prove the octave really is being generated, and the chord follower is fed
// actual note sets to prove it picks the pad the theory says it should.
//
// Build:  cmake --build build --config Release --target NebulaTest
// Run:    build/NebulaTest_artefacts/Release/NebulaTest.exe

#include "../../src/PresetShare.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../../src/Shimmer.h"
#include "../../src/UserContent.h"
#include "../../src/License/LicenseClient.h"

static int failures = 0;

static void check (bool ok, const juce::String& what, const juce::String& detail = {})
{
    std::cout << (ok ? "  PASS  " : "  FAIL  ") << what;
    if (detail.isNotEmpty()) std::cout << "   [" << detail << "]";
    std::cout << std::endl;
    if (! ok) ++failures;
}

// Energy at one frequency, by direct correlation (a full FFT is overkill here).
static double energyAt (const float* x, int n, double freq, double sr)
{
    double re = 0.0, im = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double t = juce::MathConstants<double>::twoPi * freq * i / sr;
        re += x[i] * std::cos (t);
        im += x[i] * std::sin (t);
    }
    return std::sqrt (re * re + im * im) / n;
}

// Energy across a narrow band. The shimmer path is deliberately chorused, so
// the shifted tone wanders by a few cents; a single-bin reading under-reports
// it badly and makes a working shimmer look absent.
static double energyNear (const float* x, int n, double freq, double sr)
{
    double total = 0.0;
    for (int c = -6; c <= 6; ++c)
        total += energyAt (x, n, freq * (1.0 + 0.004 * c), sr);
    return total;
}

// Mirrors the processor's reverb stage so the shimmer is tested inside the
// loop it actually lives in, not in isolation. Testing the shifter on its own
// is what let the first version pass while sounding wrong.
struct ReverbRig
{
    juce::Reverb verb;
    shimmer::Engine shim;
    juce::AudioBuffer<float> dry, wet, ret, src, tail;
    float mix = 0.4f;
    bool  poisonReturn = false;

    void prepare (double sr, int block)
    {
        verb.setSampleRate (sr);
        shim.prepare (sr);
        dry.setSize (2, block); wet.setSize (2, block); ret.setSize (2, block);
        src.setSize (2, block); src.clear();
        tail.setSize (2, block); tail.clear();
        // Deliberately poisoned, not cleared. The processor resizes this buffer
        // without zeroing it, and the first version added it into the reverb
        // input before anything had written it - which is what turned the
        // shimmer into static. A rig that quietly cleared it could never catch
        // that, so it now starts as hostile as real uninitialised memory.
        if (poisonReturn)
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                    ret.setSample (ch, i, (i % 2 == 0 ? 1.0f : -1.0f) * 0.9f);
        else
            ret.clear();
        juce::Reverb::Parameters rp;
        rp.roomSize = 0.9f; rp.damping = 0.3f; rp.width = 1.0f;
        rp.wetLevel = 1.0f; rp.dryLevel = 0.0f; rp.freezeMode = 0.0f;
        verb.setParameters (rp);
    }

    void process (juce::AudioBuffer<float>& buf)
    {
        const int n = buf.getNumSamples();
        for (int ch = 0; ch < 2; ++ch) dry.copyFrom (ch, 0, buf, ch, 0, n);

        // Mirrors the processor exactly: the signal goes INTO the shifter, the
        // shifted result joins it on the way into the reverb, and the reverb
        // tail feeds back into the shifter input on the next block.
        if (shim.isActive())
        {
            const float fb = shim.getFeedback();
            for (int ch = 0; ch < 2; ++ch)
            {
                src.copyFrom (ch, 0, buf, ch, 0, n);
                src.addFrom  (ch, 0, tail, ch, 0, n, fb);
            }
            shim.shift (src, ret);
            for (int ch = 0; ch < 2; ++ch) buf.addFrom (ch, 0, ret, ch, 0, n);
        }
        else { ret.clear(); tail.clear(); }

        for (int ch = 0; ch < 2; ++ch) wet.copyFrom (ch, 0, buf, ch, 0, n);
        verb.processStereo (wet.getWritePointer (0), wet.getWritePointer (1), n);

        if (shim.isActive())
            for (int ch = 0; ch < 2; ++ch) tail.copyFrom (ch, 0, wet, ch, 0, n);

        for (int ch = 0; ch < 2; ++ch)
        {
            buf.copyFrom (ch, 0, dry, ch, 0, n);
            buf.applyGain (ch, 0, n, 2.0f * (1.0f - mix * 0.4f));
            buf.addFrom (ch, 0, wet, ch, 0, n, mix * 0.7f);
        }
    }
};

static void testShimmer()
{
    std::cout << "\nSHIMMER" << std::endl;

    const double sr = 48000.0;
    const int block = 512;
    const double f0 = 220.0;

    // ── splitting the reverb into wet-only plus a manual mix must be exactly
    // what juce::Reverb's own dry/wet did, or v1's sound has quietly changed ──
    {
        const float mix = 0.4f;
        juce::Reverb a, b;
        a.setSampleRate (sr); b.setSampleRate (sr);

        juce::Reverb::Parameters pa;
        pa.roomSize = 0.85f; pa.damping = 0.45f; pa.width = 1.0f;
        pa.wetLevel = mix * 0.7f; pa.dryLevel = 1.0f - mix * 0.4f;
        a.setParameters (pa);

        auto pb = pa; pb.wetLevel = 1.0f; pb.dryLevel = 0.0f;
        b.setParameters (pb);

        juce::AudioBuffer<float> bufA (2, block), bufB (2, block), dryB (2, block);
        double phase = 0.0, maxDelta = 0.0;

        for (int blk = 0; blk < 60; ++blk)
        {
            for (int i = 0; i < block; ++i)
            {
                const float s = (float) std::sin (phase) * 0.4f;
                phase += juce::MathConstants<double>::twoPi * f0 / sr;
                bufA.setSample (0, i, s); bufA.setSample (1, i, s);
                bufB.setSample (0, i, s); bufB.setSample (1, i, s);
                dryB.setSample (0, i, s); dryB.setSample (1, i, s);
            }
            a.processStereo (bufA.getWritePointer (0), bufA.getWritePointer (1), block);
            b.processStereo (bufB.getWritePointer (0), bufB.getWritePointer (1), block);
            for (int ch = 0; ch < 2; ++ch)
            {
                bufB.applyGain (ch, 0, block, mix * 0.7f);
                bufB.addFrom (ch, 0, dryB, ch, 0, block, 2.0f * (1.0f - mix * 0.4f));
            }
            // The first block is juce::Reverb ramping its gains in over 10 ms.
            // The processor reproduces that with its own SmoothedValue; this
            // rig applies the gains flat, so compare once both have settled.
            if (blk < 2) continue;
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < block; ++i)
                    maxDelta = juce::jmax (maxDelta,
                        (double) std::abs (bufA.getSample (ch, i) - bufB.getSample (ch, i)));
        }
        check (maxDelta < 1.0e-6, "wet-only reverb + manual mix matches v1 exactly",
               "max delta " + juce::String (maxDelta, 9));
    }

    // ── at zero the shimmer injects nothing ──
    {
        ReverbRig rig; rig.prepare (sr, block);
        rig.shim.setParams (0.0f, 0.5f, 0.5f, 0, 0.0f, 0.0f);
        juce::AudioBuffer<float> buf (2, block);
        double phase = 0.0, maxRet = 0.0;
        for (int blk = 0; blk < 40; ++blk)
        {
            for (int i = 0; i < block; ++i)
            {
                const float s = (float) std::sin (phase) * 0.5f;
                phase += juce::MathConstants<double>::twoPi * f0 / sr;
                buf.setSample (0, i, s); buf.setSample (1, i, s);
            }
            rig.process (buf);
            for (int i = 0; i < block; ++i)
                maxRet = juce::jmax (maxRet, (double) std::abs (rig.ret.getSample (0, i)));
        }
        check (maxRet == 0.0, "amount 0 injects nothing into the reverb",
               "max return " + juce::String (maxRet, 12));
    }

    // ── the octave appears in the tail, and it is the reverb's tail ──
    {
        ReverbRig rig; rig.prepare (sr, block);
        rig.shim.setParams (0.7f, 0.0f /* short pre-delay */, 0.6f, 0, 0.0f, 0.8f);

        juce::AudioBuffer<float> buf (2, block);
        std::vector<float> tail;
        double phase = 0.0;
        const int blocks = 320;

        for (int blk = 0; blk < blocks; ++blk)
        {
            for (int i = 0; i < block; ++i)
            {
                const float s = (float) std::sin (phase) * 0.5f;
                phase += juce::MathConstants<double>::twoPi * f0 / sr;
                buf.setSample (0, i, s); buf.setSample (1, i, s);
            }
            rig.process (buf);
            if (blk >= blocks - 80)
                for (int i = 0; i < block; ++i) tail.push_back (buf.getSample (0, i));
        }

        const double eF = energyNear (tail.data(), (int) tail.size(), f0, sr);
        const double eO = energyNear (tail.data(), (int) tail.size(), f0 * 2.0, sr);
        const double e5 = energyNear (tail.data(), (int) tail.size(), f0 * 1.5, sr);
        check (eO > eF * 0.05, "octave present in the reverb tail",
               "fund " + juce::String (eF, 5) + "  oct " + juce::String (eO, 5));
        check (eO > e5 * 3.0, "it is the octave, not a stray partial",
               "oct " + juce::String (eO, 5) + "  fifth " + juce::String (e5, 5));
    }

    // ── warble: the artifact that made the first build sound wrong ──
    // A splice at a fixed interval combs against periodic input, and a drone
    // pad is highly periodic. That shows up as the shifted tone's level
    // pulsing. Measured here as envelope ripple on a steady sine: low is
    // smooth, high is the wobble. The fixed-grain build measured ~0.5+.
    {
        shimmer::AlignedShifter sh;
        sh.prepare (sr);
        sh.setRatio (2.0f);

        std::vector<float> out;
        double phase = 0.0;
        const int total = (int) (sr * 3.0);
        for (int i = 0; i < total; ++i)
        {
            const float s = (float) std::sin (phase) * 0.5f;
            phase += juce::MathConstants<double>::twoPi * 220.0 / sr;
            const float y = sh.process (s);
            if (i > total / 2) out.push_back (y);      // let it settle first
        }

        // RMS in 20 ms windows, then the spread across windows
        const int win = (int) (sr * 0.02);
        std::vector<double> rms;
        for (size_t i = 0; i + (size_t) win < out.size(); i += (size_t) win)
        {
            double acc = 0.0;
            for (int k = 0; k < win; ++k) acc += out[i + (size_t) k] * out[i + (size_t) k];
            rms.push_back (std::sqrt (acc / win));
        }
        double lo = 1e9, hi = 0.0, sum = 0.0;
        for (double v : rms) { lo = juce::jmin (lo, v); hi = juce::jmax (hi, v); sum += v; }
        const double mean = sum / juce::jmax<size_t> (1, rms.size());
        const double ripple = mean > 1e-9 ? (hi - lo) / mean : 0.0;

        check (ripple < 0.25, "shifted tone is steady (correlation-aligned splices)",
               "envelope ripple " + juce::String (ripple, 3));

        // The shifter itself must be roughly unity. A quiet shifter starves the
        // feedback loop, and the shimmer never builds.
        const double inRms = 0.5 / std::sqrt (2.0);
        check (mean > inRms * 0.5 && mean < inRms * 2.0, "shifter holds its level (near unity gain)",
               "in " + juce::String (inRms, 4) + "  out " + juce::String (mean, 4));
    }

    // ── the loop runs through a long reverb; it must still always die ──
    {
        ReverbRig rig; rig.prepare (sr, block);
        rig.shim.setParams (1.0f, 1.0f, 1.0f, 2 /* Octave + Fifth */, 1.0f, 1.0f);

        juce::AudioBuffer<float> buf (2, block);
        double phase = 0.0, peakDriven = 0.0;

        for (int blk = 0; blk < 300; ++blk)
        {
            for (int i = 0; i < block; ++i)
            {
                const float s = (float) std::sin (phase) * 0.9f;
                phase += juce::MathConstants<double>::twoPi * f0 / sr;
                buf.setSample (0, i, s); buf.setSample (1, i, s);
            }
            rig.process (buf);
            for (int i = 0; i < block; ++i)
                peakDriven = juce::jmax (peakDriven, (double) std::abs (buf.getSample (0, i)));
        }

        // Let it ring and watch the envelope. With the feedback now running
        // through a long reverb this legitimately takes many seconds, so the
        // test is that it keeps falling and reaches silence - not that it hits
        // some fraction by an arbitrary deadline.
        double peakMid = 0.0, peakAfter = 0.0;
        for (int blk = 0; blk < 2000; ++blk)
        {
            buf.clear();
            rig.process (buf);
            double p = 0.0;
            for (int i = 0; i < block; ++i)
                p = juce::jmax (p, (double) std::abs (buf.getSample (0, i)));
            if (blk == 700)  peakMid = p;
            if (blk >= 1999) peakAfter = p;
        }
        check (peakAfter < peakMid * 0.5, "the tail keeps falling, it does not sustain",
               juce::String (peakMid, 4) + " -> " + juce::String (peakAfter, 6));
        check (peakAfter < 0.01, "the tail reaches silence",
               "peak after 21s " + juce::String (peakAfter, 6));
        check (std::isfinite (peakDriven) && peakDriven < 4.0,
               "stable with every control maxed through a long reverb",
               "peak " + juce::String (peakDriven, 3));

    }
}

// ── the macro: one knob, the whole effect ────────────────────────────────
static void testMacro()
{
    std::cout << "\nSHIMMER MACRO" << std::endl;

    const auto off = shimmer::macroAt (0.0f);
    check (off.amount == 0.0f && off.stack == 0.0f, "at zero the shimmer is fully off",
           "amount " + juce::String (off.amount, 4));

    // Every destination must move in one direction only. A macro that dips
    // mid-sweep feels broken under the hand even when each value is sensible.
    float lastAmount = -1.0f, lastTone = -1.0f, lastBloom = 2.0f, lastStack = -1.0f;
    bool amountUp = true, toneUp = true, bloomDown = true, stackUp = true;
    for (int i = 0; i <= 100; ++i)
    {
        const auto m = shimmer::macroAt ((float) i / 100.0f);
        if (i > 0)
        {
            if (m.amount < lastAmount - 1.0e-6f) amountUp = false;
            if (m.tone   < lastTone   - 1.0e-6f) toneUp = false;
            if (m.bloom  > lastBloom  + 1.0e-6f) bloomDown = false;
            if (m.stack  < lastStack  - 1.0e-6f) stackUp = false;
        }
        lastAmount = m.amount; lastTone = m.tone; lastBloom = m.bloom; lastStack = m.stack;
    }
    check (amountUp, "amount only ever rises across the sweep");
    check (toneUp,   "tone only ever opens across the sweep");
    check (bloomDown, "pre-delay only shortens - the shimmer comes closer, never further");
    check (stackUp,  "regeneration only rises across the sweep");

    // The shape that makes it "come in and out" rather than just get louder.
    const auto low = shimmer::macroAt (0.2f);
    const auto mid = shimmer::macroAt (0.5f);
    const auto top = shimmer::macroAt (1.0f);
    check (low.bloom > 0.6f && top.bloom < 0.25f,
           "quiet settings sit far away, loud ones bloom close",
           "bloom " + juce::String (low.bloom, 2) + " -> " + juce::String (top.bloom, 2));
    check (low.stack == 0.0f && mid.stack == 0.0f && top.stack > 0.9f,
           "cascading octaves only appear in the last third",
           "stack at 0.5 = " + juce::String (mid.stack, 2));
    check (low.amount < 0.12f, "the bottom of the travel stays subtle",
           "amount at 20% = " + juce::String (low.amount, 3));
}


// ── user content: the naming and grouping people's own files depend on ────
static void testUserContent()
{
    std::cout << "\nUSER CONTENT" << std::endl;

    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("NebulaUserScanTest");
    tmp.deleteRecursively();
    tmp.createDirectory();

    auto touch = [&tmp] (const juce::String& fileName)
    {
        tmp.getChildFile (fileName).replaceWithData ("RIFF", 4);
    };

    // A preset spread over four keys, named the way a person actually would.
    touch ("Test_Pad_C.wav");
    touch ("Test_Pad_D.wav");
    touch ("Test_Pad_E.wav");
    touch ("Test_Pad_G.wav");
    // A second preset, spaces instead of underscores, and a sharp spelling.
    touch ("Night Air F#.wav");
    touch ("Night Air A.wav");
    // No key token at all - should land on C rather than being dropped.
    touch ("Loner.wav");

    const auto found = usercontent::scanFolder (tmp);
    check (found.size() == 3, "three presets found from seven files",
           juce::String (found.size()) + " found");

    auto byName = [&found] (const juce::String& n) -> const usercontent::Scanned*
    {
        for (const auto& g : found) if (g.name.equalsIgnoreCase (n)) return &g;
        return nullptr;
    };

    if (const auto* g = byName ("Test Pad"))
    {
        check (g->numKeys() == 4, "underscores become spaces, four keys grouped",
               juce::String (g->numKeys()) + " keys");
        check (g->keys[0].existsAsFile() && g->keys[2].existsAsFile()
                 && g->keys[4].existsAsFile() && g->keys[7].existsAsFile(),
               "keys land in the right slots (C, D, E, G)");
        check (! g->keys[1].existsAsFile(), "keys with no file stay empty");
    }
    else check (false, "preset 'Test Pad' was not found");

    if (const auto* g = byName ("Night Air"))
    {
        check (g->numKeys() == 2, "space-separated names group too",
               juce::String (g->numKeys()) + " keys");
        check (g->keys[6].existsAsFile(), "F# is read as Gb (slot 6)");
        check (g->keys[9].existsAsFile(), "A is read as slot 9");
    }
    else check (false, "preset 'Night Air' was not found");

    if (const auto* g = byName ("Loner"))
        check (g->keys[0].existsAsFile(), "a file with no key token defaults to C");
    else check (false, "preset 'Loner' was not found");

    // Names that would collide with a key token must not lose their last word.
    {
        auto t2 = tmp.getChildFile ("edge");
        t2.createDirectory();
        t2.getChildFile ("Deep Current Eb.wav").replaceWithData ("RIFF", 4);
        const auto e = usercontent::scanFolder (t2);
        check (e.size() == 1 && e[0].name == "Deep Current" && e[0].keys[3].existsAsFile(),
               "multi-word name keeps its words, trailing token read as the key",
               e.size() == 1 ? e[0].name : "?");
    }

    // An empty or missing folder is normal on a fresh install, not an error.
    check (usercontent::scanFolder (tmp.getChildFile ("nope")).isEmpty(),
           "a missing folder scans to nothing rather than failing");

    tmp.deleteRecursively();
}


// ── licensing: the two mistakes that broke SecondOut 1.3.0 ───────────────
// Both were build-configuration errors, not logic errors, and neither showed
// up until real customers tried to activate. They are cheap to assert here,
// so they can never ship again unnoticed.
static void testLicensing()
{
    std::cout << "\nLICENSING" << std::endl;

    // Mistake 1: the base URL fell back to a dev server, so every genuine
    // activation went to a machine that did not exist.
    const auto url = amanorsacstudio::LicenseClient::defaultBaseUrl();
    check (url == "https://amanorsac.studio", "base URL is the live server",
           url);
    check (! url.containsIgnoreCase ("127.0.0.1") && ! url.containsIgnoreCase ("localhost"),
           "base URL is not a local dev server");

    // Mistake 2: the compiled-in public key was the dev keypair, so the live
    // server's proofs never verified and activation silently failed.
    check (amanorsacstudio::kLicenseSigningKeyConfigured,
           "a signing key is configured (fail closed if not)");
    check (sizeof (amanorsacstudio::kLicenseSigningKey) == 65,
           "signing key is a 65-byte uncompressed P-256 point",
           juce::String ((int) sizeof (amanorsacstudio::kLicenseSigningKey)) + " bytes");
    check (amanorsacstudio::kLicenseSigningKey[0] == 0x04,
           "signing key starts with 0x04 (SEC1 uncompressed)");

    // The exact live key from the studio's licensing notes, section 5. If this
    // ever fails the binary is carrying a different key than the server signs
    // with, and nothing will activate.
    static constexpr uint8_t expected[65] = {
        0x04, 0xcd, 0xa5, 0x7d, 0x1c, 0xc8, 0xa6, 0xe2, 0x71, 0xd5, 0x48, 0x49,
        0xce, 0x55, 0xd5, 0x03, 0x77, 0x56, 0x66, 0x90, 0xfd, 0xb6, 0x95, 0x45,
        0xa4, 0x1a, 0x92, 0xc4, 0x77, 0xda, 0xcb, 0x00, 0x0d, 0x2c, 0x06, 0x0b,
        0xa8, 0x3f, 0xbd, 0x9b, 0x70, 0x85, 0xaf, 0xff, 0xc0, 0x42, 0xd4, 0x00,
        0x7e, 0x5b, 0x96, 0xfe, 0x68, 0xff, 0xec, 0x91, 0x11, 0xf6, 0x21, 0x00,
        0x79, 0xfc, 0x43, 0x59, 0x52
    };
    bool same = true;
    for (int i = 0; i < 65; ++i)
        if (amanorsacstudio::kLicenseSigningKey[i] != expected[i]) same = false;
    check (same, "compiled-in key matches the live studio key byte for byte");

    // A malformed proof must be refused rather than accepted unverified.
    {
        const bool ok = ! amanorsacstudio::licensecrypto::parseAndVerifySignedBlob ("not-a-proof").isObject();
        check (ok, "a malformed proof is rejected");
    }
    {
        // Well-formed shape, nonsense signature.
        const bool ok = ! amanorsacstudio::licensecrypto::parseAndVerifySignedBlob ("eyJhIjoxfQ.AAAA").isObject();
        check (ok, "a proof with a bad signature is rejected");
    }
}


// ── the static-noise regression ──────────────────────────────────────────
// The return buffer is added to the REVERB INPUT. juce::AudioBuffer::setSize
// does not zero what it hands back, so resizing it and adding it in the same
// breath fed uninitialised memory into the reverb on the first block, which
// then regenerated round the shimmer loop as continuous static.
//
// The processor now clears the buffer whenever it is (re)sized and whenever
// the shimmer is inactive. This proves that: the rig starts with the buffer
// full of full-scale garbage, exactly like real uninitialised memory, and the
// output must still be clean.
static void testShimmerNoInjectedGarbage()
{
    std::cout << "\nSHIMMER - INJECTED GARBAGE" << std::endl;

    const double sr = 48000.0;
    const int block = 512;

    // Poisoned and never cleared. Under the old SEND topology the return
    // buffer was added into the reverb BEFORE anything wrote it, so this
    // garbage reached the output as static. The INSERT topology writes it
    // immediately before reading it, which removes the hazard by construction
    // rather than by remembering to clear - but only while that order holds,
    // which is what this pins down.
    double cleanPeak = 0.0;
    {
        ReverbRig rig;
        rig.poisonReturn = true;
        rig.prepare (sr, block);
        rig.shim.setParams (0.6f, 0.2f, 0.6f, 0, 0.0f, 0.8f);

        juce::AudioBuffer<float> buf (2, block);
        buf.clear();
        rig.process (buf);
        for (int i = 0; i < block; ++i)
            cleanPeak = juce::jmax (cleanPeak, (double) std::abs (buf.getSample (0, i)));
    }
    check (cleanPeak == 0.0, "poisoned return buffer cannot reach the output",
           "peak " + juce::String (cleanPeak, 9));
    // 3. Silence in must stay silent for a long run, not creep up from the
    //    loop feeding on its own noise floor.
    {
        ReverbRig rig;
        rig.prepare (sr, block);
        rig.shim.setParams (1.0f, 0.0f, 1.0f, 2, 1.0f, 1.0f);   // everything wide open

        juce::AudioBuffer<float> buf (2, block);
        double peak = 0.0;
        for (int blk = 0; blk < 400; ++blk)
        {
            buf.clear();
            rig.process (buf);
            for (int i = 0; i < block; ++i)
                peak = juce::jmax (peak, (double) std::abs (buf.getSample (0, i)));
        }
        check (peak < 1.0e-6, "silence stays silent with the shimmer wide open",
               "peak over 4s " + juce::String (peak, 9));
    }
}



// ── Density: the thing that makes a shimmer sweet rather than grainy ─────
// Diffusion turns each reflection into many. The measurable consequence is
// that an impulse stops being a spike and becomes a spread-out cloud, without
// the total energy changing much. Crest factor - peak over RMS - captures
// exactly that: a sparse response is peaky, a dense one is not.
static void testDensity()
{
    std::cout << "\nSHIMMER DENSITY" << std::endl;

    const double sr = 48000.0;
    const int n = (int) (sr * 0.4);

    // Density means MANY arrivals, so count them: samples that carry real
    // level relative to the peak. Crest factor was the wrong measure - more
    // diffusion also lengthens the response, which drops peak and RMS together
    // and can move the ratio either way.
    auto arrivals = [sr, n] (float density)
    {
        shimmer::Diffuser d;
        d.prepare (sr);
        d.setDensity (density);

        std::vector<float> y ((size_t) n);
        double peak = 0.0;
        for (int i = 0; i < n; ++i)
        {
            y[(size_t) i] = d.process (i == 0 ? 1.0f : 0.0f);
            peak = juce::jmax (peak, (double) std::abs (y[(size_t) i]));
        }
        if (peak < 1.0e-12) return 0;
        int count = 0;
        for (float v : y) if (std::abs (v) > peak * 0.02) ++count;
        return count;
    };

    auto crest = [sr, n] (float density)
    {
        shimmer::Diffuser d;
        d.prepare (sr);
        d.setDensity (density);
        double peak = 0.0, sum = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const float v = d.process (i == 0 ? 1.0f : 0.0f);
            peak = juce::jmax (peak, (double) std::abs (v));
            sum += (double) v * v;
        }
        const double rms = std::sqrt (sum / n);
        return rms > 1.0e-12 ? peak / rms : 1.0e9;
    };

    const int a0 = arrivals (0.0f), a5 = arrivals (0.5f), a1 = arrivals (1.0f);
    check (a1 > a5 && a5 > a0, "raising density produces more arrivals",
           juce::String (a0) + " -> " + juce::String (a5) + " -> " + juce::String (a1));

    const double c0 = crest (0.0f), c1 = crest (1.0f);
    check (c1 < c0 * 0.5, "at full density the response is far less peaky",
           juce::String (c0 / juce::jmax (1.0, c1), 1) + "x flatter");

    // At zero it must be a true bypass, or the control has no off position.
    {
        shimmer::Diffuser d;
        d.prepare (sr);
        d.setDensity (0.0f);
        double maxDelta = 0.0;
        for (int i = 0; i < 2000; ++i)
        {
            const float x = (float) std::sin (i * 0.05) * 0.5f;
            maxDelta = juce::jmax (maxDelta, (double) std::abs (d.process (x) - x));
        }
        check (maxDelta == 0.0, "density 0 passes the signal through untouched",
               "max delta " + juce::String (maxDelta, 9));
    }

    // Diffusion must not become a resonator. An allpass chain preserves energy,
    // so a decaying input has to give a decaying output.
    {
        shimmer::Diffuser d;
        d.prepare (sr);
        d.setDensity (1.0f);
        for (int i = 0; i < (int) sr; ++i)                    // 1 s of noise in
            d.process (((float) (i * 1103515245u % 1000) / 500.0f - 1.0f) * 0.5f);

        double tail = 0.0;
        for (int i = 0; i < (int) (sr * 2.0); ++i)            // 2 s of silence
        {
            const float y = d.process (0.0f);
            if (i > (int) (sr * 1.5)) tail = juce::jmax (tail, (double) std::abs (y));
        }
        check (tail < 0.01, "diffusion decays rather than ringing",
               "tail after 1.5s " + juce::String (tail, 6));
    }

    // The macro must reach for density early - a faint shimmer still wants to
    // be smooth; sparseness is never the effect anyone is after.
    {
        const auto lo = shimmer::macroAt (0.15f);
        const auto hi = shimmer::macroAt (1.0f);
        check (lo.density > 0.5f, "even a faint shimmer is diffused",
               "density at 15% = " + juce::String (lo.density, 2));
        check (hi.density >= lo.density, "density never drops as the knob rises",
               juce::String (lo.density, 2) + " -> " + juce::String (hi.density, 2));
    }
}

//==============================================================================
// The About screen carries the Creative Commons attributions, and CREDITS.txt
// carries them too. Two copies drift. This fails the build when they do, which
// matters more than it looks: attribution is a condition of the CC BY licence,
// so a name quietly dropped from one of them is a licence breach, not a typo.
static void testCredits()
{
    std::cout << "\n-- credits --" << std::endl;

    const juce::File repo (NEBULA_REPO_DIR);
    const auto header  = repo.getChildFile ("src").getChildFile ("AboutView.h");
    const auto credits = repo.getChildFile ("CREDITS.txt");

    check (header.existsAsFile() && credits.existsAsFile(),
           "AboutView.h and CREDITS.txt are both present");
    if (! header.existsAsFile() || ! credits.existsAsFile()) return;

    const auto creditsText = credits.loadFileAsString();
    const auto headerText  = header.loadFileAsString();

    // Every "title  -  author" line between ccByCredits's braces.
    const int start = headerText.indexOf ("ccByCredits");
    const int open  = headerText.indexOf (start, "{");
    const int close = headerText.indexOf (open, "};");
    check (start > 0 && open > start && close > open, "ccByCredits list found in the header");
    if (close <= open) return;

    juce::StringArray entries;
    entries.addTokens (headerText.substring (open + 1, close), "\n", "");

    int checked = 0;
    for (auto line : entries)
    {
        // trailing comma first, or unquoted() sees "…klankbeeld", and leaves
        // the closing quote attached to the author's name
        line = line.trim().trimCharactersAtEnd (",").trim().unquoted();
        if (! line.contains ("  -  ")) continue;

        const auto title  = line.upToFirstOccurrenceOf ("  -  ", false, false).trim();
        const auto author = line.fromLastOccurrenceOf ("  -  ", false, false).trim();

        // Long titles are shortened with "..." on screen, so match the stem.
        const auto stem = title.upToFirstOccurrenceOf ("...", false, false).trim();

        check (creditsText.contains (stem), "CREDITS.txt lists: " + stem);
        check (creditsText.contains (author), "CREDITS.txt names the author: " + author);
        ++checked;
    }

    check (checked == 9, "all nine CC BY credits were compared",
           juce::String (checked) + " compared");

    // The two NonCommercial recordings were removed. If either name comes back
    // into the shipping library, it comes back with a licence problem attached.
    for (const auto* gone : { "Pedreira", "NonCommercial-only" })
        check (! headerText.contains (gone),
               juce::String ("the About screen does not credit ") + gone);
}

//==============================================================================
// Sharing a preset is the one feature whose output leaves this machine and is
// opened by a stranger's copy of the app. A round trip through a real file is
// the only test that means anything here.
// Deleting a preset must delete that preset, and only that preset. The
// wildcard this used to use made "Pad" swallow "Pad 2", whose files are
// named Pad_2_<key> and so matched "Pad_*" too.
static void testDeleteMatching()
{
    std::cout << "\n-- deleting presets --" << std::endl;

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("NebulaDeleteTest");
    dir.deleteRecursively();
    dir.createDirectory();

    for (const char* n : { "Pad_C.flac", "Pad_E.flac", "Pad_2_C.flac", "Pad_2_E.flac" })
        dir.getChildFile (n).replaceWithData ("x", 1);

    auto found = usercontent::scanFolder (dir);
    check (found.size() == 2, "two presets are seen", juce::String (found.size()));

    // What the delete now does: ask the scanner, take only that preset's files.
    juce::Array<juce::File> doomed;
    for (const auto& g : found)
        if (g.name == "Pad")
            for (const auto& k : g.keys)
                if (k.existsAsFile()) doomed.add (k);

    check (doomed.size() == 2, "only Pad's own two files are selected",
           juce::String (doomed.size()));
    for (const auto& f : doomed) f.deleteFile();

    check (! dir.getChildFile ("Pad_C.flac").existsAsFile(), "Pad is gone");
    check (dir.getChildFile ("Pad_2_C.flac").existsAsFile(), "Pad 2 survives");

    auto after = usercontent::scanFolder (dir);
    check (after.size() == 1 && after[0].name == "Pad 2",
           "only Pad 2 is left", after.size() == 1 ? after[0].name : juce::String (after.size()));

    // And the old wildcard, for the record, would have taken both.
    check (dir.findChildFiles (juce::File::findFiles, false, "Pad_*").size() == 2,
           "the old Pad_* wildcard still matches Pad 2's files");

    dir.deleteRecursively();
}

//==============================================================================
// Importing a folder guesses each stem's key from its name. A wrong guess puts
// a pad in the wrong key without anyone noticing, so the guessing is pinned.
static void testFolderMatch()
{
    std::cout << "\n-- matching a folder of stems to keys --" << std::endl;

    check (usercontent::detectKey ("Aurora_Veil_Db") == 1, "the key at the end of a name is found");
    check (usercontent::detectKey ("Preset 1 - E") == 4, "a key after a dash is found");
    check (usercontent::detectKey ("Pad F#") == 6, "a sharp is read as its key");
    check (usercontent::detectKey ("Bb Warm Pad") == 10, "a key at the start is found when the end has none");
    check (usercontent::detectKey ("Preset 2 - A Output Audio Bus L") == -1,
           "a bounce stray is not mistaken for a key");
    check (usercontent::detectKey ("Rain Loop") == -1, "a name with no key gives no key");

    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("NebulaFolderMatchTest");
    dir.deleteRecursively();
    dir.createDirectory();
    for (const char* n : { "Aurora_Veil_C.aif", "Aurora_Veil_Db.aif", "Aurora_Veil_E.aif",
                           "Other_E.wav", "Stray Output L.aif", "notes.txt" })
        dir.getChildFile (n).replaceWithData ("x", 1);

    const auto m = usercontent::matchFolder (dir);
    check (m.files.size() == 5, "every audio file is listed, the text file is not",
           juce::String (m.files.size()));
    check (m.keys[0].getFileName() == "Aurora_Veil_C.aif", "C gets the C file", m.keys[0].getFileName());
    check (m.keys[1].getFileName() == "Aurora_Veil_Db.aif", "Db gets the Db file", m.keys[1].getFileName());
    check (m.keys[4].getFileName() == "Aurora_Veil_E.aif",
           "when two files name the same key, the first in name order is suggested",
           m.keys[4].getFileName());
    check (m.numMatched() == 3, "three keys matched, the stray left for the person",
           juce::String (m.numMatched()));
    check (m.suggestedName == "Aurora Veil", "the preset name is taken from the files", m.suggestedName);

    check (usercontent::matchFolder (dir.getChildFile ("missing")).files.isEmpty(),
           "a folder that does not exist gives an empty match");

    dir.deleteRecursively();
}

static void testPresetSharing()
{
    std::cout << "\n-- preset sharing --" << std::endl;

    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("NebulaShareTest");
    tmp.deleteRecursively();
    tmp.createDirectory();

    // Three keys of real audio, each a different tone so a swapped file shows.
    const int keys[] = { 0, 4, 7 };                 // C, E, G
    const double freq[] = { 261.63, 329.63, 392.0 };
    juce::File source[12];

    juce::WavAudioFormat wav;
    for (int n = 0; n < 3; ++n)
    {
        auto f = tmp.getChildFile (juce::String ("src_") + presetshare::keyNames[keys[n]] + ".wav");
        std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), 44100.0, 1, 16, {}, 0));
        os.release();
        juce::AudioBuffer<float> buf (1, 4410);
        for (int i = 0; i < buf.getNumSamples(); ++i)
            buf.setSample (0, i, 0.5f * std::sin (juce::MathConstants<double>::twoPi * freq[n] * i / 44100.0));
        w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
        w.reset();
        source[keys[n]] = f;
    }

    presetshare::Meta meta;
    meta.name = "Shared Test Pad";
    meta.maker = "Amanorsac Studio";
    meta.colour = juce::Colour (0xff3fa9c9);
    meta.reverbType = 2;
    meta.mix = 0.42f; meta.size = 0.66f; meta.damp = 0.31f;

    const auto pack = tmp.getChildFile (juce::String ("shared") + presetshare::extension);
    auto wrote = presetshare::writePack (pack, meta, source);
    check (wrote.wasOk(), "a pack is written", wrote.getErrorMessage());
    check (pack.existsAsFile() && pack.getSize() > 1000, "the pack is a real file",
           juce::String (pack.getSize()) + " bytes");

    // FLAC must actually be smaller than the WAV it came from, or the reason
    // for transcoding at all has quietly stopped being true.
    juce::int64 sourceBytes = 0;
    for (int n = 0; n < 3; ++n) sourceBytes += source[keys[n]].getSize();
    check (pack.getSize() < sourceBytes, "the pack is smaller than the audio going in",
           juce::String (sourceBytes) + " -> " + juce::String (pack.getSize()));

    presetshare::Contents peeked;
    auto pk = presetshare::peekPack (pack, peeked);
    check (pk.wasOk(), "the pack can be read without unpacking", pk.getErrorMessage());
    check (peeked.meta.name == "Shared Test Pad", "the name survives", peeked.meta.name);
    check (peeked.meta.maker == "Amanorsac Studio", "the maker survives", peeked.meta.maker);
    check (peeked.meta.colour == juce::Colour (0xff3fa9c9), "the colour survives",
           peeked.meta.colour.toDisplayString (true));
    check (peeked.meta.reverbType == 2, "the reverb type survives");
    check (std::abs (peeked.meta.mix - 0.42f) < 0.001f
             && std::abs (peeked.meta.size - 0.66f) < 0.001f
             && std::abs (peeked.meta.damp - 0.31f) < 0.001f, "the reverb settings survive");
    check (peeked.numKeys == 3, "three keys in, three keys out",
           juce::String (peeked.numKeys));
    check (peeked.keyPresent[0] && peeked.keyPresent[4] && peeked.keyPresent[7]
             && ! peeked.keyPresent[1], "the right three keys");

    // Unpack where a second machine would, and check the scanner - the code
    // that actually decides what the app loads - groups it as one preset.
    auto userDir = tmp.getChildFile ("User");
    presetshare::Contents got;
    auto readBack = presetshare::readPack (pack, userDir, got);
    check (readBack.wasOk(), "the pack unpacks", readBack.getErrorMessage());

    auto scanned = usercontent::scanFolder (userDir);
    check (scanned.size() == 1, "the scanner sees exactly one preset",
           juce::String (scanned.size()) + " found");
    if (scanned.size() == 1)
    {
        check (scanned[0].name == "Shared Test Pad", "under the name it was given",
               scanned[0].name);
        check (scanned[0].numKeys() == 3, "with its three keys",
               juce::String (scanned[0].numKeys()));
        check (scanned[0].keys[0].existsAsFile() && scanned[0].keys[4].existsAsFile()
                 && scanned[0].keys[7].existsAsFile(), "in the right slots");
    }

    // The audio has to still be the audio. Decode C back and confirm it is
    // the tone that went in rather than, say, E.
    if (scanned.size() == 1 && scanned[0].keys[0].existsAsFile())
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (scanned[0].keys[0]));
        check (r != nullptr, "the unpacked audio is readable");
        if (r != nullptr)
        {
            juce::AudioBuffer<float> buf ((int) r->numChannels, (int) r->lengthInSamples);
            r->read (&buf, 0, buf.getNumSamples(), 0, true, true);
            const double atC = energyNear (buf.getReadPointer (0), buf.getNumSamples(), 261.63, 44100.0);
            const double atE = energyNear (buf.getReadPointer (0), buf.getNumSamples(), 329.63, 44100.0);
            check (atC > atE * 4.0, "slot C holds the tone that was put in C",
                   juce::String (atC, 4) + " vs " + juce::String (atE, 4));
        }
    }

    // The effect and texture a preset names must travel with it, or a
    // shared preset arrives pointing at sounds the recipient does not have.
    {
        auto fxSrc  = tmp.getChildFile ("Thunder Hit.wav");
        auto texSrc = tmp.getChildFile ("Rain Loop.wav");
        for (auto* f : { &fxSrc, &texSrc })
        {
            std::unique_ptr<juce::FileOutputStream> os (f->createOutputStream());
            std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), 44100.0, 1, 16, {}, 0));
            os.release();
            juce::AudioBuffer<float> b (1, 2205);
            b.clear();
            b.setSample (0, 0, 0.9f);
            w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
        }

        auto withAux = tmp.getChildFile (juce::String ("withaux") + presetshare::extension);
        auto wr2 = presetshare::writePack (withAux, meta, source, fxSrc, texSrc);
        check (wr2.wasOk(), "a pack can carry an effect and a texture", wr2.getErrorMessage());

        presetshare::Contents auxPeek;
        check (presetshare::peekPack (withAux, auxPeek).wasOk(), "it still reads back");
        check (auxPeek.hasFx && auxPeek.hasTex, "both aux sounds are in there");
        check (auxPeek.meta.fxName == "Thunder Hit.wav", "the effect keeps its filename",
               auxPeek.meta.fxName);

        auto fxDir  = tmp.getChildFile ("User2/fx");
        auto texDir = tmp.getChildFile ("User2/textures");
        presetshare::Contents landed;
        auto rd2 = presetshare::readPack (withAux, tmp.getChildFile ("User2"), landed, {}, fxDir, texDir);
        check (rd2.wasOk(), "a pack with aux sounds unpacks", rd2.getErrorMessage());
        check (fxDir.getChildFile ("Thunder Hit.wav").existsAsFile(),
               "the effect lands in the fx folder");
        check (texDir.getChildFile ("Rain Loop.wav").existsAsFile(),
               "the texture lands in the textures folder");

        // A sound already there must not be replaced: other presets point at it.
        auto existing = fxDir.getChildFile ("Thunder Hit.wav");
        existing.replaceWithText ("not audio, but mine");
        presetshare::Contents again;
        presetshare::readPack (withAux, tmp.getChildFile ("User2"), again, {}, fxDir, texDir);
        check (existing.loadFileAsString() == "not audio, but mine",
               "an aux sound the person already has is left alone");
    }

    // Renaming on import, for when a name is already taken.
    presetshare::Contents renamed;
    auto r2 = presetshare::readPack (pack, userDir, renamed, "Borrowed Pad");
    check (r2.wasOk(), "a pack can be imported under a different name", r2.getErrorMessage());
    check (userDir.getChildFile ("Borrowed_Pad_C.flac").existsAsFile(),
           "the renamed copy lands beside the original");

    // Rubbish in must not be mistaken for a preset.
    auto junk = tmp.getChildFile (juce::String ("junk") + presetshare::extension);
    junk.replaceWithText ("this is not a zip");
    presetshare::Contents nope;
    check (presetshare::peekPack (junk, nope).failed(), "a file that is not a pack is refused");

    auto empty = tmp.getChildFile ("empty.ntpreset");
    juce::File none[12];
    presetshare::Meta m2; m2.name = "Nothing";
    check (presetshare::writePack (empty, m2, none).failed(),
           "a preset with no sounds is refused");

    presetshare::Meta m3; m3.name = "   ";
    check (presetshare::writePack (empty, m3, source).failed(),
           "a preset with no name is refused");

    tmp.deleteRecursively();
}

int main()
{
    std::cout << "Nebula Tide v2 checks" << std::endl;
    testShimmer();
    testShimmerNoInjectedGarbage();
    testMacro();
    testDensity();
    testUserContent();
    testLicensing();
    testCredits();
    testPresetSharing();
    testDeleteMatching();
    testFolderMatch();
    std::cout << "\n" << (failures == 0 ? "ALL PASSED" : juce::String (failures) + " FAILED").toStdString()
              << std::endl;
    return failures == 0 ? 0 : 1;
}
