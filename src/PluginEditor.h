#pragma once
#include "PluginProcessor.h"

//==============================================================================
// "Falling through space": stars fly past the viewer with z-depth projection,
// plus slow aurora arcs. Energy from the audio level speeds up the fall.
class NebulaBackground : public juce::Component, private juce::Timer
{
public:
    explicit NebulaBackground (NebulaTideProcessor& p) : processor (p)
    {
        setInterceptsMouseClicks (false, false);
        for (auto& s : stars)
            respawn (s, true);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override;

private:
    struct Star { float x, y, z, size; };   // x,y in [-1,1] around centre, z depth

    void respawn (Star& s, bool anywhere)
    {
        s.x = rnd.nextFloat() * 2.0f - 1.0f;
        s.y = rnd.nextFloat() * 2.0f - 1.0f;
        s.z = anywhere ? (0.15f + rnd.nextFloat() * 0.85f) : 1.0f;
        s.size = 0.6f + rnd.nextFloat() * 1.6f;
    }

    void timerCallback() override
    {
        const float level = processor.outputLevel.load();
        smoothedLevel += (level - smoothedLevel) * 0.08f;
        const float speed = 0.0035f + juce::jlimit (0.0f, 1.0f, smoothedLevel * 1.8f) * 0.012f;

        for (auto& s : stars)
        {
            s.z -= speed * (0.5f + s.size * 0.4f);
            if (s.z <= 0.03f)
                respawn (s, false);
        }
        phase += 0.016f;
        repaint();
    }

    NebulaTideProcessor& processor;
    std::array<Star, 170> stars;
    juce::Random rnd;
    float phase = 0.0f;
    float smoothedLevel = 0.0f;
};

//==============================================================================
class NebulaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NebulaLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float startAngle, float endAngle,
                           juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float pos, float min, float max,
                           juce::Slider::SliderStyle, juce::Slider&) override;
};

//==============================================================================
class PadButton : public juce::TextButton
{
public:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    bool isActivePad = false;
    float glowPhase = 0.0f;
    juce::Colour tint { 0xff4fe3ff };   // authored per-preset in Nebula Forge
};

//==============================================================================
// The 12 keys as floating planets on a gentle orbit. Available keys for the
// current pad glow sea-blue; the selected key burns bright; missing keys dim.
class KeyPlanets : public juce::Component, private juce::Timer
{
public:
    explicit KeyPlanets (NebulaTideProcessor& p) : processor (p)
    {
        for (int i = 0; i < 12; ++i)
        {
            wobblePhase[i] = rnd.nextFloat() * juce::MathConstants<float>::twoPi;
            hue[i] = rnd.nextFloat();
        }
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hovered = -1; }

private:
    void timerCallback() override { phase += 0.016f; repaint(); }
    juce::Point<float> planetCentre (int i) const;

    NebulaTideProcessor& processor;
    juce::Random rnd;
    float phase = 0.0f;
    float wobblePhase[12] {};
    float hue[12] {};
    int hovered = -1;
};

//==============================================================================
// Buttons with no background or outline at all — just their text/symbol.
class BareLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool, bool) override {}
};

//==============================================================================
// A glowing star that triggers FX (cat 0) or textures (cat 1). Click = play /
// stop, arrows cycle sounds, ∞ toggles loop. Independent of the preset.
class StarPlayer : public juce::Component, private juce::Timer
{
public:
    StarPlayer (NebulaTideProcessor& p, int category, juce::Colour starColour)
        : processor (p), cat (category), colour (starColour)
    {
        for (auto* b : { &prev, &next, &loopBtn })
            b->setLookAndFeel (&bareLnf);
        addAndMakeVisible (prev);
        addAndMakeVisible (next);
        addAndMakeVisible (loopBtn);
        prev.setButtonText ("<");
        next.setButtonText (">");
        loopBtn.setButtonText (juce::String::fromUTF8 ("\xe2\x88\x9e"));   // ∞ = loop
        prev.onClick = [this] { cycle (-1); };
        next.onClick = [this] { cycle (1); };
        loopBtn.onClick = [this]
        {
            processor.setAuxLoop (cat, ! processor.getAuxLoop (cat));
        };
        startTimerHz (30);
    }

    ~StarPlayer() override
    {
        for (auto* b : { &prev, &next, &loopBtn })
            b->setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    void timerCallback();
    float dragStartVolume = 0.5f;
    bool  dragging = false;
    void cycle (int dir)
    {
        const int n = processor.getAuxSounds (cat).size();
        if (n == 0) return;
        const int next = (processor.getAuxIndex (cat) + dir + n) % n;
        if (processor.isAuxPlaying (cat))
            processor.triggerAux (cat, next);      // hot-swap to the new sound
        else
            processor.setAuxIndex (cat, next);
        repaint();
    }

    NebulaTideProcessor& processor;
    int cat;
    juce::Colour colour;
    BareLookAndFeel bareLnf;
    juce::TextButton prev, next, loopBtn;
    juce::Rectangle<float> starArea;
    float phase = 0.0f;
};

//==============================================================================
// Kontakt-style keyboard strip: C1..C7, key zones tinted (FX gold, KEYS preset
// colour, TEXTURES ice), held notes lit, zone labels above. Clickable.
class ZoneKeyboard : public juce::Component, private juce::Timer
{
public:
    explicit ZoneKeyboard (NebulaTideProcessor& p) : processor (p) { startTimerHz (30); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    static constexpr int firstNote = 24, lastNote = 96;   // C1..C7

private:
    void timerCallback() override { repaint(); }
    int noteAt (juce::Point<float>) const;
    juce::Rectangle<float> whiteKeyRect (int note) const;
    juce::Rectangle<float> blackKeyRect (int note) const;
    static bool isBlack (int n) { const int pc = n % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
    static int whiteIndex (int n);     // count of white keys from firstNote

    NebulaTideProcessor& processor;
    int mouseNote = -1;
};

//==============================================================================
// Right-click any mapped control → "MIDI Learn" context menu.
class MidiLearnListener : public juce::MouseListener
{
public:
    MidiLearnListener (NebulaTideProcessor& p, std::function<juce::Array<int>()> actionsFn)
        : processor (p), getActions (std::move (actionsFn)) {}

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! e.mods.isPopupMenu()) return;
        juce::PopupMenu m;
        for (const int action : getActions())
        {
            if (action < 0 || action >= NebulaTideProcessor::numMidiActions) continue;
            m.addItem ("MIDI Learn: " + juce::String (NebulaTideProcessor::midiActionName (action)),
                       [this, action] { processor.startLearn (action); });
            const auto bound = processor.bindingText (action);
            if (bound != "-")
                m.addItem ("Clear " + juce::String (NebulaTideProcessor::midiActionName (action))
                             + " (" + bound + ")",
                           [this, action] { processor.clearBinding (action); });
        }
        m.showMenuAsync ({});
    }

private:
    NebulaTideProcessor& processor;
    std::function<juce::Array<int>()> getActions;
};

//==============================================================================
// First-launch sound library downloader. Shown when no presets are found
// (mobile builds, app-only installs). Downloads the library zip, unpacks it
// into the per-user presets folder, then rescans.
class LibraryDownloader : public juce::Component,
                          private juce::URL::DownloadTaskListener,
                          private juce::Timer
{
public:
    LibraryDownloader (NebulaTideProcessor& p, std::function<void()> onReady);
    ~LibraryDownloader() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void start();
    void progress (juce::URL::DownloadTask*, juce::int64 downloaded, juce::int64 total) override;
    void finished (juce::URL::DownloadTask*, bool success) override;
    void timerCallback() override { repaint(); }
    void unpackAndFinish();

    NebulaTideProcessor& processor;
    std::function<void()> onReady;
    juce::TextButton downloadBtn { "DOWNLOAD SOUND LIBRARY" };
    std::unique_ptr<juce::URL::DownloadTask> task;
    juce::File zipFile;
    std::atomic<double> fraction { 0.0 };
    std::atomic<int> state { 0 };    // 0 idle, 1 downloading, 2 unpacking, 3 error
    juce::String errorText;
    juce::ThreadPool pool { 1 };
};

//==============================================================================
// Settings overlay: control map reference + MIDI learn per action.
class SettingsPanel : public juce::Component, private juce::Timer
{
public:
    explicit SettingsPanel (NebulaTideProcessor& p) : processor (p)
    {
        for (int i = 0; i < NebulaTideProcessor::numMidiActions; ++i)
        {
            auto* row = rows.add (new Row());
            row->name.setText (NebulaTideProcessor::midiActionName (i), juce::dontSendNotification);
            row->learn.setButtonText ("LEARN");
            row->clear.setButtonText ("X");
            row->learn.onClick = [this, i]
            {
                if (processor.learningAction() == i) processor.cancelLearn();
                else processor.startLearn (i);
            };
            row->clear.onClick = [this, i] { processor.clearBinding (i); };
            rowsHolder.addAndMakeVisible (row->name);
            rowsHolder.addAndMakeVisible (row->bind);
            rowsHolder.addAndMakeVisible (row->learn);
            rowsHolder.addAndMakeVisible (row->clear);
        }
        viewport.setViewedComponent (&rowsHolder, false);
        viewport.setScrollBarsShown (true, false);
        viewport.setScrollBarThickness (8);
        addAndMakeVisible (viewport);
        devicesBtn.setButtonText ("AUDIO / MIDI DEVICES...");
        addAndMakeVisible (devicesBtn);
        gateBtn.setClickingTogglesState (true);
        gateBtn.setToggleState (processor.noteGate.load(), juce::dontSendNotification);
        gateBtn.onClick = [this] { processor.noteGate.store (gateBtn.getToggleState()); };
        addAndMakeVisible (gateBtn);
        startTimerHz (10);
    }

    juce::TextButton devicesBtn;   // wired by the editor (standalone only)
    juce::ToggleButton gateBtn { "MIDI notes gate the pad (note off = fade out, like a sampler)" };

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override { if (! isVisible()) processor.cancelLearn(); }

private:
    void timerCallback() override;

    struct Row { juce::Label name, bind; juce::TextButton learn, clear; };
    NebulaTideProcessor& processor;
    juce::OwnedArray<Row> rows;
    juce::Viewport viewport;
    juce::Component rowsHolder;
};

//==============================================================================
class NebulaTideEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit NebulaTideEditor (NebulaTideProcessor&);
    ~NebulaTideEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void rebuildPads();
    void updatePadStates();
    void updateReverbButtons();
    void browse (int dir);          // move the single-preset view left/right

    int viewIndex = 0;              // which preset the UI is showing

    NebulaTideProcessor& processor;
    NebulaLookAndFeel lnf;
    NebulaBackground background { processor };
    KeyPlanets keyPlanets { processor };
    ZoneKeyboard zoneKeyboard { processor };
    StarPlayer fxStar  { processor, 0, juce::Colour (0xffffd27b) };   // warm gold star
    StarPlayer texStar { processor, 1, juce::Colour (0xffff5a6e) };   // coral-red star

    juce::Label title, presetLabel, statusLabel;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" };
    juce::TextButton settingsBtn { "SETTINGS" };
    juce::TextButton keysBtn { "KEYS" };       // show/hide the zoned keyboard strip
    SettingsPanel settingsPanel { processor };
    std::unique_ptr<LibraryDownloader> downloader;   // only while the library is missing
    juce::OwnedArray<PadButton> pads;

    juce::Slider volumeKnob, panKnob, fadeSlider;
    juce::Slider rMixSlider, rSizeSlider, rDampSlider;
    juce::Label volumeLabel, panLabel, fadeLabel;
    juce::Label rMixLabel, rSizeLabel, rDampLabel, reverbTitle;
    juce::TextButton roomBtn { "ROOM" }, plateBtn { "PLATE" }, hallBtn { "HALL" };

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> volumeAtt, panAtt, fadeAtt, rMixAtt, rSizeAtt, rDampAtt;

    juce::OwnedArray<MidiLearnListener> learnListeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NebulaTideEditor)
};
