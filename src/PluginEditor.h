#pragma once
#include "PluginProcessor.h"
#include "MidiDrag.h"
#include "PresetStudio.h"
#include "ActivationView.h"
#include <juce_opengl/juce_opengl.h>

// Defined in PluginEditor.cpp; declared here so components written inline in
// this header can use the theme colours too.
namespace colours
{
    extern const juce::Colour bgDeep, foam, textDim;
    extern juce::Colour bgMid, sea, seaBright;
}

//==============================================================================
// HD rendering helpers: true Gaussian bloom (cached sprite, cheap to draw) and
// per-platform premium fonts (no bundled files needed).
namespace ui
{
    // soft light bloom centred at c with the given radius, colour and strength
    void drawBloom (juce::Graphics& g, juce::Point<float> c, float radius,
                    juce::Colour colour, float intensity = 1.0f);

    juce::Font titleFont (float size);      // wide-tracked display face
    juce::Font labelFont (float size);      // small caps labels
    juce::Font bodyFont  (float size);
}

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
        startTimerHz (60);
    }

    void paint (juce::Graphics& g) override;

private:
    struct Star { float x, y, z, size, warmth; };   // x,y in [-1,1] around centre, z depth

    void respawn (Star& s, bool anywhere)
    {
        s.x = rnd.nextFloat() * 2.0f - 1.0f;
        s.y = rnd.nextFloat() * 2.0f - 1.0f;
        s.z = anywhere ? (0.15f + rnd.nextFloat() * 0.85f) : 1.0f;
        s.size = 0.6f + rnd.nextFloat() * 1.6f;
        s.warmth = rnd.nextFloat();            // colour temperature + twinkle phase
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
        startTimerHz (60);
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
        startTimerHz (60);
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
    void timerCallback() override;
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
    explicit ZoneKeyboard (NebulaTideProcessor& p) : processor (p) { startTimerHz (60); }
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
// Shown when no sound library is found. On Android it installs the library
// that ships inside the APK (first launch, with progress). Elsewhere it simply
// explains that the install is incomplete — nothing is ever downloaded.
class LibraryDownloader : public juce::Component, private juce::Timer
{
public:
    LibraryDownloader (NebulaTideProcessor& p, std::function<void()> onReady);
    ~LibraryDownloader() override;
    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    void timerCallback() override { repaint(); }

    NebulaTideProcessor& processor;
    std::function<void()> onReady;
    std::atomic<double> fraction { 0.0 };
    std::atomic<int> state { 0 };    // 2 installing, 3 message only
    juce::String messageText, diagnosticText;
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

        blendLabel.setText ("LOOP BLEND", juce::dontSendNotification);
        blendLabel.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.3f));
        addAndMakeVisible (blendLabel);
        blendSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        blendSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 20);
        blendSlider.setRange (0.1, 6.0, 0.1);
        blendSlider.setTextValueSuffix (" s");
        blendSlider.setValue (gLoopBlendSeconds.load(), juce::dontSendNotification);
        blendSlider.onValueChange = [this] { gLoopBlendSeconds.store ((float) blendSlider.getValue()); };
        addAndMakeVisible (blendSlider);

        // ── v2 shimmer shaping ──
        auto smallCaps = [this] (juce::Label& l, const juce::String& text)
        {
            l.setText (text, juce::dontSendNotification);
            l.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.3f));
            addAndMakeVisible (l);
        };
        smallCaps (shimHeading, "SHIMMER");
        smallCaps (bloomLabel,  "BLOOM");
        smallCaps (toneLabel,   "TONE");
        smallCaps (sizeLabel,   "SIZE");
        smallCaps (pitchLabel,  "PITCH");
        smallCaps (densityLabel,"DENSITY");

        for (auto* s : { &bloomSlider, &toneSlider, &sizeSlider, &densitySlider })
        {
            s->setSliderStyle (juce::Slider::LinearHorizontal);
            s->setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            addAndMakeVisible (*s);
        }
        bloomAtt = std::make_unique<SlAtt> (processor.apvts, "shimbloom", bloomSlider);
        toneAtt  = std::make_unique<SlAtt> (processor.apvts, "shimtone",  toneSlider);
        sizeAtt  = std::make_unique<SlAtt> (processor.apvts, "shimsize",  sizeSlider);
        densityAtt = std::make_unique<SlAtt> (processor.apvts, "shimdensity", densitySlider);

        pitchBox.addItemList ({ "Octave", "Fifth", "Octave + Fifth", "High", "Sub + Octave" }, 1);
        addAndMakeVisible (pitchBox);
        pitchAtt = std::make_unique<CbAtt> (processor.apvts, "shimpitch", pitchBox);
        addAndMakeVisible (manualBtn);

        manualAtt = std::make_unique<BtAtt> (processor.apvts, "shimmanual", manualBtn);


        smallCaps (licenseHeading, "LICENSE");
        licenseStatus.setFont (ui::bodyFont (10.5f));
        licenseStatus.setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (licenseStatus);
        deactivateBtn.onClick = [this]
        {
            deactivateBtn.setEnabled (false);
            processor.license.deactivateThisDevice ([this] (amanorsacstudio::LicenseResult r)
            {
                deactivateBtn.setEnabled (true);
                licenseStatus.setText (r.ok ? "Deactivated. Reopen to enter a key."
                                            : r.message, juce::dontSendNotification);
            });
        };
        addAndMakeVisible (deactivateBtn);

        smallCaps (soundsHeading, "SOUND LIBRARY");
        soundsPath.setFont (ui::bodyFont (10.5f));
        soundsPath.setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (soundsPath);
        locateBtn.onClick = [this] { chooseLibraryFolder(); };
        resetLocationBtn.onClick = [this]
        {
            NebulaTideProcessor::setUserChosenLibraryDir ({});
            processor.reloadLibrary();
            refreshLibraryPath();
        };
        addAndMakeVisible (locateBtn);
        addAndMakeVisible (resetLocationBtn);
        refreshLibraryPath();

        startTimerHz (10);
    }

    juce::TextButton devicesBtn;   // wired by the editor (standalone only)
    // Buyers move machines; without this they have to email support to free
    // a seat (standard R10).
    juce::Label licenseHeading, licenseStatus;
    juce::TextButton deactivateBtn { "DEACTIVATE THIS DEVICE" };

    // Where the sounds are. Shown always, not just when something is broken,
    // so people can move the library to another drive on purpose rather than
    // only discovering the setting exists after an install has gone wrong.
    juce::Label soundsHeading, soundsPath;
    juce::TextButton locateBtn { "CHOOSE FOLDER..." }, resetLocationBtn { "USE DEFAULT" };
    std::unique_ptr<juce::FileChooser> folderChooser;
    void chooseLibraryFolder();
    void refreshLibraryPath();
    juce::ToggleButton gateBtn { "MIDI notes gate the pad (note off = fade out, like a sampler)" };
    juce::Label blendLabel;        // loop crossfade length (pads, FX, textures)
    juce::Slider blendSlider;

    // v2 — the shimmer's shaping controls; the amount itself lives on the main
    // panel, since that is the one you reach for while playing.
    juce::Label shimHeading, bloomLabel, toneLabel, sizeLabel, pitchLabel, densityLabel;
    juce::Slider bloomSlider, toneSlider, sizeSlider, densitySlider;
    juce::ComboBox pitchBox;
    // MANUAL frees the four destinations from the macro curve, the way turning
    // an assign off on a Montage hands the parameter back to you.
    juce::ToggleButton manualBtn { "MANUAL - set shimmer controls by hand instead of following the knob" };
    using SlAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CbAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::unique_ptr<SlAtt> bloomAtt, toneAtt, sizeAtt, densityAtt;
    std::unique_ptr<CbAtt> pitchAtt;
    using BtAtt = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<BtAtt> manualAtt;

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
// Drag this onto a DAW track and you get a MIDI clip that plays the instrument
// back exactly as it stands — current key, plus whichever FX and texture are
// running. Stylus RMX works the same way, and for the same reason: once the
// part is in the timeline you can edit it like any other MIDI.
class MidiDragHandle : public juce::Component
{
public:
    explicit MidiDragHandle (NebulaTideProcessor& p) : processor (p)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setTooltip ("Drag to a DAW track to create a MIDI clip");
    }

    void paint (juce::Graphics&) override;

    void mouseDown (const juce::MouseEvent&) override { armed = true; repaint(); }
    void mouseUp (const juce::MouseEvent&) override   { armed = false; repaint(); }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        dragging = true;

        mididrag::Clip clip;
        clip.key = processor.getCurrentKey();
        clip.bpm = processor.hostBpm.load();
        clip.auxChannel = NebulaTideProcessor::auxMidiChannel;
        if (processor.isAuxPlaying (0))
            clip.fxNote = NebulaTideProcessor::fxZoneLo + processor.getAuxIndex (0);
        if (processor.isAuxPlaying (1))
            clip.texNote = NebulaTideProcessor::texZoneLo + processor.getAuxIndex (1);

        const auto& presets = processor.getPresets();
        const int pad = processor.getCurrentPadIndex();
        if (pad >= 0 && pad < presets.size())
            clip.name = presets.getReference (pad).name;

        mididrag::startDrag (this, clip);
        dragging = false;
        armed = false;
        repaint();
    }

    void setTooltip (const juce::String& t) { tip = t; }
    juce::String getTooltip() const { return tip; }

    // Shows the preset name, so it reads as the thing itself rather than as a
    // button labelled with a file format. Nothing announces it — you find it.
    void setLabel (const juce::String& t) { if (t != label) { label = t; repaint(); } }

private:
    NebulaTideProcessor& processor;
    juce::String tip, label;
    bool armed = false, dragging = false;
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
    juce::TextButton updateBtn { "UPDATE AVAILABLE" };   // hidden unless one exists
    juce::String updatePageUrl;
    juce::ThreadPool updatePool { 1 };
    void checkForUpdate();

    juce::TextButton settingsBtn { "SETTINGS" };
    juce::TextButton studioBtn { "STUDIO" };   // the preset dashboard
    juce::TextButton keysBtn { "KEYS" };       // show/hide the zoned keyboard strip
    SettingsPanel settingsPanel { processor };
    std::unique_ptr<PresetStudio> studio;
    // Covers the whole window until a proof has verified. Removed, not hidden,
    // the instant isLicensed() turns true.
    std::unique_ptr<ActivationView> activation;
    void showMainViewIfLicensed();
    std::unique_ptr<LibraryDownloader> downloader;   // only while the library is missing
    juce::OwnedArray<PadButton> pads;

    juce::Slider volumeKnob, panKnob, fadeSlider;
    juce::Slider rMixSlider, rSizeSlider, rDampSlider, shimSlider;
    juce::Label volumeLabel, panLabel, fadeLabel;
    juce::Label rMixLabel, rSizeLabel, rDampLabel, shimLabel, reverbTitle;
    juce::TextButton roomBtn { "ROOM" }, plateBtn { "PLATE" }, hallBtn { "HALL" };

    // v2
    MidiDragHandle midiDrag { processor };     // drag a MIDI clip to the timeline

    using Attachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attachment> volumeAtt, panAtt, fadeAtt, rMixAtt, rSizeAtt, rDampAtt, shimAtt;

    juce::OwnedArray<MidiLearnListener> learnListeners;

#if JUCE_WINDOWS || JUCE_MAC || JUCE_LINUX
    juce::OpenGLContext openGL;   // GPU-accelerated rendering on desktop
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NebulaTideEditor)
};
