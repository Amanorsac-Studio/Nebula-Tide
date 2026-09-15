#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>

// ── Shimmer ───────────────────────────────────────────────────────────────
//
// Wired the way a shimmer plugin sits on a DAW insert: the signal goes INTO
// the pitch shifter, and the shifted result goes into the reverb. The pad
// itself passes through the effect rather than sitting beside it.
//
// An earlier version fed the shifter from the reverb's wet tail instead. That
// is a send, not an insert: the pad's own sound never entered the shifter, so
// every octave you heard came from the wash and the two never fused. It read
// as two things playing at once, because that is what it was.
//
//        pad ──┬──────────────────────────────────────► dry ──► out
//              │                                   │
//              └─►[+]─► shift ─► damp ─► comp ─────►[+]─► reverb ─┬─► wet ─► out
//                  ▲                                              │
//                  └────────── feedback × regen ◄─────────────────┘
//
// The feedback still runs through the reverb, so the rise keeps its space -
// but the first octave you hear is the pad's own, immediately.
//
// Three things decide whether this sings or grates, and all three are places
// a naive build goes wrong:
//
//   1. GRAIN ALIGNMENT. A pitch shifter that splices at fixed intervals combs
//      against any periodic input, and a drone pad is about as periodic as
//      audio gets. At a 100 ms grain the splice lands every 10 Hz — audible as
//      a wobble that the feedback loop then multiplies. The splice point here
//      is chosen by cross-correlation instead, landing where the waveform
//      actually continues, which is what the old AMS/Eventide boxes did.
//
//   2. MODULATION. juce::Reverb is Freeverb: eight combs, four allpasses, no
//      modulation at all. Its resonances sit on fixed frequencies, and feeding
//      them back through a pitch shifter stacks them into a metallic ring. A
//      slow chorus in the return path detunes each pass just enough to stop
//      those resonances piling up in the same places.
//
//   3. COMPRESSION, NOT CLIPPING. tanh() catches peaks but leaves the wash
//      lumpy. A real compressor with a fast attack and slow release evens the
//      tail out and glues each pass to the last, which is what makes the rise
//      sound continuous rather than like separate copies.

namespace shimmer
{

// Pitch shifter built on a delay line whose read head runs at the pitch ratio.
// When the read head has travelled far enough it jumps back and crossfades —
// and the jump distance is chosen by correlating the waveform either side, so
// the splice lands in phase instead of at an arbitrary point.
class AlignedShifter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        nominalJump = (float) (sr * 0.045);            // ~45 ms between splices
        xfLen       = (float) (sr * 0.012);            // 12 ms crossfade
        searchRange = (int) (sr * 0.0065);             // +/- 6.5 ms  (~77 Hz and up)
        corrLen     = (int) (sr * 0.004);              // 4 ms correlation window
        minLag      = xfLen + 4.0f;

        size = juce::nextPowerOfTwo ((int) (sr * 0.35));
        mask = size - 1;
        buf.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        write = 0;
        readPos = (float) size * 0.5f;
        fadePos = 0.0f;
        fadeLeft = 0.0f;
    }

    void setRatio (float r) noexcept { ratio = juce::jlimit (0.25f, 4.0f, r); }

    float process (float x) noexcept
    {
        buf[(size_t) write] = x;

        float out = readInterp (readPos);

        // Crossfade across a splice with an equal-power curve, so the level
        // never dips through the join.
        if (fadeLeft > 0.0f)
        {
            const float t = juce::jlimit (0.0f, 1.0f, 1.0f - fadeLeft / xfLen);
            const float gNew = std::sin (t * juce::MathConstants<float>::halfPi);
            const float gOld = std::cos (t * juce::MathConstants<float>::halfPi);
            out = out * gNew + readInterp (fadePos) * gOld;
            fadePos += ratio;
            fadeLeft -= 1.0f;
        }

        readPos += ratio;
        write = (write + 1) & mask;

        // How far the read head sits behind the write head, unwrapped.
        float lag = (float) write - readPos;
        while (lag < 0.0f)          lag += (float) size;
        while (lag >= (float) size) lag -= (float) size;

        if (fadeLeft <= 0.0f)
        {
            if (ratio > 1.0f && lag < minLag)
                splice (readPos + findAlignment (readPos, nominalJump));
            else if (ratio < 1.0f && lag > nominalJump * 2.0f + minLag)
                splice (readPos - findAlignment (readPos, -nominalJump));
        }

        return out;
    }

private:
    void splice (float newPos) noexcept
    {
        fadePos = readPos;
        readPos = newPos;
        fadeLeft = xfLen;
    }

    // Searches around the nominal jump for the offset where the waveform best
    // matches what we are leaving, so the splice lands in phase. This is the
    // difference between a smooth octave and a 10 Hz wobble.
    float findAlignment (float from, float nominal) const noexcept
    {
        const int step = 3;
        float best = nominal, bestScore = -1.0e30f;

        for (int off = -searchRange; off <= searchRange; off += step)
        {
            const float cand = nominal + (float) off;
            float dot = 0.0f, energy = 1.0e-9f;
            for (int k = 0; k < corrLen; k += 4)
            {
                const float a = readInterp (from + (float) k);
                const float b = readInterp (from + cand + (float) k);
                dot += a * b;
                energy += b * b;
            }
            // Normalised, so a loud region cannot win on level alone.
            const float score = dot / std::sqrt (energy);
            if (score > bestScore) { bestScore = score; best = cand; }
        }
        return best;
    }

    float readInterp (float pos) const noexcept
    {
        while (pos < 0.0f) pos += (float) size;
        const int i0 = (int) pos & mask;
        const int i1 = (i0 + 1) & mask;
        const float f = pos - std::floor (pos);
        return buf[(size_t) i0] + f * (buf[(size_t) i1] - buf[(size_t) i0]);
    }

    std::vector<float> buf;
    double sr = 44100.0;
    int   size = 0, mask = 0, write = 0, searchRange = 64, corrLen = 128;
    float readPos = 0.0f, fadePos = 0.0f, fadeLeft = 0.0f;
    float ratio = 2.0f, nominalJump = 1024.0f, xfLen = 256.0f, minLag = 260.0f;
};

struct OnePole
{
    void setCutoff (float freq, double sr) noexcept
    {
        a = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi
                             * juce::jlimit (10.0f, (float) sr * 0.45f, freq) / (float) sr);
    }
    float lp (float x) noexcept { z += a * (x - z); return z; }
    float hp (float x) noexcept { z += a * (x - z); return x - z; }
    void reset() noexcept { z = 0.0f; }
    float a = 0.5f, z = 0.0f;
};

// Fast attack, slow release, soft knee — the shape the reference material
// calls for. It evens the tail into a consistent wash and keeps regeneration
// from stacking, which clipping alone never does.
struct Compressor
{
    void prepare (double sr) noexcept
    {
        atk = 1.0f - std::exp (-1.0f / (float) (sr * 0.005));   // 5 ms
        rel = 1.0f - std::exp (-1.0f / (float) (sr * 0.250));   // 250 ms
        env = 0.0f;
    }
    void reset() noexcept { env = 0.0f; }

    float process (float x) noexcept
    {
        const float a = std::abs (x);
        env += (a > env ? atk : rel) * (a - env);

        float gain = 1.0f;
        if (env > knee)
        {
            const float over = env / knee;
            gain = std::pow (over, 1.0f / ratio - 1.0f);
        }
        // Makeup, because this sits inside a feedback loop: compressing without
        // restoring the level starves the regeneration and the shimmer never
        // builds at all. Kept modest so loop gain stays under control.
        return x * gain * makeup;
    }

    float atk = 0.01f, rel = 0.0001f, env = 0.0f;
    float knee = 0.6f, ratio = 4.0f, makeup = 1.25f;
};

// Slow chorus. Freeverb has no modulation of its own, so without this the same
// comb resonances are excited on every pass and build into a metallic ring.
struct Chorus
{
    void prepare (double sr, float phase)
    {
        size = juce::nextPowerOfTwo ((int) (sr * 0.05));
        mask = size - 1;
        buf.assign ((size_t) size, 0.0f);
        write = 0;
        ph = phase;
        inc = juce::MathConstants<float>::twoPi * 0.27f / (float) sr;   // 0.27 Hz
        depth = (float) (sr * 0.0035);                                  // +/- 3.5 ms
        base  = (float) (sr * 0.006);
    }
    void reset() { std::fill (buf.begin(), buf.end(), 0.0f); write = 0; }

    float process (float x) noexcept
    {
        buf[(size_t) write] = x;
        ph += inc;
        if (ph > juce::MathConstants<float>::twoPi) ph -= juce::MathConstants<float>::twoPi;

        float pos = (float) write - (base + depth * std::sin (ph));
        while (pos < 0.0f) pos += (float) size;
        const int i0 = (int) pos & mask;
        const int i1 = (i0 + 1) & mask;
        const float f = pos - std::floor (pos);
        const float out = buf[(size_t) i0] + f * (buf[(size_t) i1] - buf[(size_t) i0]);

        write = (write + 1) & mask;
        return out;
    }

    std::vector<float> buf;
    int size = 0, mask = 0, write = 0;
    float ph = 0.0f, inc = 0.0f, depth = 0.0f, base = 0.0f;
};

class Predelay
{
public:
    void prepare (double sr)
    {
        size = juce::nextPowerOfTwo ((int) (sr * 0.6) + 4);
        mask = size - 1;
        for (auto& b : buf) b.assign ((size_t) size, 0.0f);
        write = 0;
    }
    void reset() { for (auto& b : buf) std::fill (b.begin(), b.end(), 0.0f); write = 0; }
    void setDelaySamples (int d) noexcept { delay = juce::jlimit (0, size - 2, d); }
    void push (float l, float r) noexcept
    {
        buf[0][(size_t) write] = l;
        buf[1][(size_t) write] = r;
        write = (write + 1) & mask;
    }
    float read (int ch) const noexcept { return buf[(size_t) ch][(size_t) ((write - delay - 1) & mask)]; }

private:
    std::vector<float> buf[2];
    int size = 0, mask = 0, write = 0, delay = 0;
};

// ── Density ───────────────────────────────────────────────────────────────
//
// juce::Reverb is Freeverb: eight comb filters and four allpasses, fixed, with
// no modulation. It is a sparse algorithm, and a sparse reverb is the wrong
// thing to put a pitch shifter through — you end up hearing the shifter's
// individual reflections instead of a wash, which is the grain that stops a
// shimmer sounding sweet.
//
// The fix is diffusion: a chain of allpass filters that smears each reflection
// into many, turning discrete echoes into a continuous cloud without changing
// the decay time. This is the front half of a Dattorro plate, and it is what
// dedicated shimmer reverbs put density controls on.
//
// It sits after the shifter, so the reverb receives an already-dense signal;
// and because the reverb tail feeds back through the shifter and through here
// again, density compounds with every pass.
class Diffuser
{
public:
    void prepare (double sampleRate)
    {
        // Mutually prime-ish lengths, so the allpasses never line up and
        // reinforce each other into a ringing pitch.
        static const float ms[numStages] = { 4.77f, 3.59f, 12.73f, 9.31f, 22.58f, 30.51f };
        for (int i = 0; i < numStages; ++i)
        {
            const int n = juce::jmax (8, (int) (sampleRate * ms[i] * 0.001));
            size[i] = juce::nextPowerOfTwo (n + 4);
            mask[i] = size[i] - 1;
            buf[i].assign ((size_t) size[i], 0.0f);
            delay[i] = n;
            pos[i] = 0;
        }
        modPhase = 0.0f;
        modInc = juce::MathConstants<float>::twoPi * 0.13f / (float) sampleRate;
        modDepth = (float) (sampleRate * 0.0007);      // +/- 0.7 ms
        reset();
    }

    void reset()
    {
        for (int i = 0; i < numStages; ++i)
        {
            std::fill (buf[i].begin(), buf[i].end(), 0.0f);
            pos[i] = 0;
        }
    }

    // 0 = straight through, 1 = as dense as it goes.
    void setDensity (float d) noexcept
    {
        density = juce::jlimit (0.0f, 1.0f, d);
        // Allpass coefficient. Past about 0.8 the tail starts to ring rather
        // than diffuse, so the top of the range stops short of that.
        g = 0.45f + density * 0.32f;
        // How many stages are in circuit: a low setting should be genuinely
        // lighter, not just a gentler version of the same six.
        active = juce::jlimit (0, numStages, (int) std::ceil (density * numStages));
    }

    float process (float x) noexcept
    {
        if (active <= 0) return x;

        modPhase += modInc;
        if (modPhase > juce::MathConstants<float>::twoPi)
            modPhase -= juce::MathConstants<float>::twoPi;

        for (int i = 0; i < active; ++i)
        {
            // A little movement on the longer stages keeps the diffusion from
            // settling into a fixed comb pattern.
            float d = (float) delay[i];
            if (i >= 2)
                d += modDepth * std::sin (modPhase + (float) i * 1.1f);
            d = juce::jlimit (2.0f, (float) (size[i] - 3), d);

            const float delayed = readInterp (i, (float) pos[i] - d);
            const float v = x - g * delayed;           // Schroeder allpass
            buf[i][(size_t) pos[i]] = v;
            x = delayed + g * v;
            pos[i] = (pos[i] + 1) & mask[i];
        }
        return x;
    }

private:
    float readInterp (int i, float p) const noexcept
    {
        while (p < 0.0f) p += (float) size[i];
        const int i0 = (int) p & mask[i];
        const int i1 = (i0 + 1) & mask[i];
        const float f = p - std::floor (p);
        return buf[(size_t) i][(size_t) i0] + f * (buf[(size_t) i][(size_t) i1] - buf[(size_t) i][(size_t) i0]);
    }

    static constexpr int numStages = 6;
    std::vector<float> buf[numStages];
    int   size[numStages] {}, mask[numStages] {}, pos[numStages] {}, delay[numStages] {};
    float g = 0.6f, density = 0.0f, modPhase = 0.0f, modInc = 0.0f, modDepth = 0.0f;
    int   active = 0;
};

enum class Pitch { octaveUp = 0, fifthUp, octaveAndFifth, high, subAndOctave };

// ── The macro ─────────────────────────────────────────────────────────────
//
// Montage's Super Knob idea: one control, several destinations, each with its
// own curve, so a single sweep travels through a designed arc instead of
// raising one number. A raw "amount" knob is the wrong control for a shimmer —
// at the bottom it is inaudible and at the top it is harsh, and the interesting
// part is a narrow band in the middle that also wants a different tone and a
// different pre-delay at each end.
//
// So one turn moves everything together:
//
//   0%    silent, fully bypassed
//   10-35 far away — long pre-delay, dark, barely regenerating. It arrives
//         late and low, like the room answering rather than a new sound.
//   35-70 the shimmer proper — pre-delay shortening, tone opening, the octave
//         becoming a voice in its own right.
//   70-100 cascading — regeneration climbing so each pass stacks another
//         octave, pre-delay short so it blooms almost with the pad.
//
// Every destination is still reachable by hand: MANUAL mode in Settings frees
// the four controls, exactly as switching an assign off does on the Montage.
struct Macro
{
    float amount = 0.0f;   // how much shifted tail returns to the reverb
    float bloom  = 0.0f;   // pre-delay: far away at the bottom, close at the top
    float tone   = 0.0f;   // dark to open
    float stack  = 0.0f;   // extra regeneration for cascading octaves
    float density = 0.0f;  // how smeared the tail is, sparse to a wash
};

inline Macro macroAt (float knob) noexcept
{
    const float k = juce::jlimit (0.0f, 1.0f, knob);
    Macro m;
    if (k <= 0.0005f)
    {
        // Off, but still at the far end of the pre-delay travel so the curve is
        // continuous. Leaving it at zero made the shimmer leap from close to
        // distant on the first hair of movement.
        m.bloom = 1.0f;
        return m;
    }

    // Amount eases in slowly so the bottom of the travel is usable rather than
    // jumping straight to an obvious octave, then opens up through the middle.
    m.amount = std::pow (k, 1.45f);

    // Pre-delay runs backwards against the knob: distant when the shimmer is
    // faint, close when it is the point. This is what makes it "come in".
    m.bloom = 1.0f - std::pow (k, 0.75f) * 0.82f;

    // Tone opens through the middle, then eases so the top never turns brittle.
    m.tone = juce::jlimit (0.0f, 1.0f, 0.18f + std::sin (k * juce::MathConstants<float>::halfPi) * 0.62f);

    // Stacking only begins in the last third — that is the cascade, and it
    // would muddy everything below it.
    m.stack = k <= 0.62f ? 0.0f : std::pow ((k - 0.62f) / 0.38f, 1.3f);

    // Density climbs early and stays high. A faint shimmer still wants to be
    // smooth - sparseness is never the effect anyone is after, it is just what
    // you get from a plain reverb.
    m.density = juce::jlimit (0.0f, 1.0f, 0.35f + std::pow (k, 0.6f) * 0.65f);

    return m;
}

class Engine
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int v = 0; v < 2; ++v) shifters[ch][v].prepare (sr);
            preHigh[ch].setCutoff (250.0f, sr);
            damping[ch].setCutoff (4500.0f, sr);
            comp[ch].prepare (sr);
            diffuser[ch].prepare (sr);
            // Quarter-cycle apart, so the two sides never detune together and
            // the shimmer opens up across the stereo field.
            chorus[ch].prepare (sr, ch == 0 ? 0.0f : juce::MathConstants<float>::halfPi);
        }
        predelay.prepare (sr);
        reset();
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int v = 0; v < 2; ++v) shifters[ch][v].reset();
            preHigh[ch].reset();
            damping[ch].reset();
            comp[ch].reset();
            chorus[ch].reset();
            diffuser[ch].reset();
        }
        predelay.reset();
        quiet = true;
    }

    void setParams (float amountIn, float bloomIn, float toneIn, int pitchMode, float stackIn,
                    float densityIn)
    {
        amount = juce::jlimit (0.0f, 1.0f, amountIn) * 0.85f;
        // Capped hard: this multiplies the amount around a loop that also
        // contains a reverb which can be set very long. Loop gain has to stay
        // comfortably under one at every setting, not just the ones I tried.
        feedback = juce::jlimit (0.0f, 1.0f, stackIn) * 0.55f;

        for (auto& d : diffuser) d.setDensity (densityIn);

        const float cutoff = 1400.0f * std::pow (5.0f, juce::jlimit (0.0f, 1.0f, toneIn));
        for (auto& f : damping) f.setCutoff (cutoff, sampleRate);

        predelay.setDelaySamples ((int) (sampleRate * 0.001
                                         * (20.0f + juce::jlimit (0.0f, 1.0f, bloomIn) * 480.0f)));

        float rA = 2.0f, rB = 2.0f, mixB = 0.0f;
        switch ((Pitch) juce::jlimit (0, 4, pitchMode))
        {
            case Pitch::octaveUp:       rA = 2.0f;    rB = 2.0f;    mixB = 0.0f;  break;
            case Pitch::fifthUp:        rA = 1.4983f; rB = 1.4983f; mixB = 0.0f;  break;
            case Pitch::octaveAndFifth: rA = 2.0f;    rB = 1.4983f; mixB = 0.42f; break;
            case Pitch::high:           rA = 2.9966f; rB = 2.0f;    mixB = 0.35f; break;
            case Pitch::subAndOctave:   rA = 2.0f;    rB = 0.5f;    mixB = 0.45f; break;
        }
        blendB = mixB;
        for (int ch = 0; ch < 2; ++ch)
        {
            shifters[ch][0].setRatio (rA);
            shifters[ch][1].setRatio (rB);
        }
    }

    bool isActive() const noexcept { return amount > 0.0005f || ! quiet; }

    // How much of the reverb tail the processor should fold back into the
    // shimmer input. The regeneration lives out there, in the shared reverb,
    // rather than in a second private loop in here.
    float getFeedback() const noexcept { return feedback; }

    void shift (const juce::AudioBuffer<float>& in, juce::AudioBuffer<float>& out)
    {
        const int n = in.getNumSamples();
        out.clear();

        if (amount <= 0.0005f && quiet)
            return;

        const int inCh = in.getNumChannels();
        auto* inL = in.getReadPointer (0);
        auto* inR = inCh > 1 ? in.getReadPointer (1) : inL;
        auto* outL = out.getWritePointer (0);
        auto* outR = out.getNumChannels() > 1 ? out.getWritePointer (1) : outL;

        float peak = 0.0f;

        for (int i = 0; i < n; ++i)
        {
            predelay.push (inL[i], inR[i]);

            for (int ch = 0; ch < 2; ++ch)
            {
                float x = predelay.read (ch);
                x = preHigh[ch].hp (x);              // keep lows out of the shifter
                x = chorus[ch].process (x);          // detune each pass

                float s = shifters[ch][0].process (x);
                if (blendB > 0.0f)
                    s = s * (1.0f - blendB) + shifters[ch][1].process (x) * blendB;

                s = diffuser[ch].process (s);       // smear it into a wash
                s = damping[ch].lp (s);
                s = comp[ch].process (s);            // even the wash, glue the passes
                s = std::tanh (s * 1.05f) * 0.92f;   // final safety only

                const float y = s * amount;
                (ch == 0 ? outL : outR)[i] = y;
                peak = juce::jmax (peak, std::abs (y));
            }
        }

        quiet = (amount <= 0.0005f) && (peak < 1.0e-6f);
    }

private:
    double sampleRate = 44100.0;
    AlignedShifter shifters[2][2];
    OnePole preHigh[2], damping[2];
    Compressor comp[2];
    Chorus chorus[2];
    Diffuser diffuser[2];
    Predelay predelay;
    float amount = 0.0f, blendB = 0.0f, feedback = 0.0f;
    bool  quiet = true;
};

} // namespace shimmer
