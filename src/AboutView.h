#pragma once
#include "PluginProcessor.h"

namespace colours
{
    extern const juce::Colour bgDeep, foam, textDim;
    extern juce::Colour bgMid, sea, seaBright;
}
namespace ui
{
    void drawBloom (juce::Graphics&, juce::Point<float>, float, juce::Colour, float intensity);
    juce::Font titleFont (float);
    juce::Font labelFont (float);
    juce::Font bodyFont  (float);
}

//==============================================================================
// ── About ─────────────────────────────────────────────────────────────────
//
// Company P0 #6: "About screen + shared settings surface — zero products have
// one. Build once, template into every product." This is Nebula Tide's, and it
// is written to be lifted wholesale into the others: version, credits,
// third-party licences and legal links, in that order, with only the constants
// at the top of this file needing to change per product.
//
// The credits are compiled in rather than read from CREDITS.txt beside the
// binary. Creative Commons attribution has to travel with the work, and a text
// file next to an executable is the first thing a copy loses.
namespace about
{
    static constexpr const char* productName = "NEBULA TIDE";
    static constexpr const char* company     = "Amanorsac Studio";
    static constexpr const char* website     = "https://amanorsac.studio";

    // Creative Commons Attribution 4.0 — crediting these is a licence
    // condition, not a courtesy. Kept in step with CREDITS.txt by a test.
    static const juce::StringArray ccByCredits
    {
        "applause in medium hall 1118 AM 250223_1003  -  klankbeeld",
        "Clapping  -  freekit",
        "LAYERS 004 - 2 Phase Space Explorer G6  -  Jovica",
        "stream flowing through a park  -  motivecap",
        "Water stream 5  -  Kolezan",
        "Stream6  -  sonicport",
        "summer forest NL EU 12.06 PM 220617_0405  -  klankbeeld",
        "birds small stream Bollertsbaach forest ... 260516_1195  -  klankbeeld",
        "ForestFamrfield 816 AM NL EU 220515_0345  -  klankbeeld"
    };

    static const juce::StringArray thirdParty
    {
        "JUCE  -  audio, GUI and plugin framework",
        "FLAC  -  lossless audio codec (Xiph.Org Foundation)",
        "Ogg Vorbis  -  lossy audio codec (Xiph.Org Foundation)"
    };
}

class AboutView : public juce::Component
{
public:
    AboutView()
    {
        title.setText (about::productName, juce::dontSendNotification);
        title.setFont (ui::titleFont (24.0f));
        title.setColour (juce::Label::textColourId, colours::foam);
        addAndMakeVisible (title);

        version.setText (juce::String ("Version ") + JucePlugin_VersionString
                           + "   \xc2\xb7   " + about::company, juce::dontSendNotification);
        version.setFont (ui::bodyFont (12.5f));
        version.setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (version);

        body.setMultiLine (true);
        body.setReadOnly (true);
        body.setScrollbarsShown (true);
        body.setCaretVisible (false);
        body.setFont (ui::bodyFont (11.5f));
        body.setColour (juce::TextEditor::backgroundColourId, colours::bgDeep.withAlpha (0.55f));
        body.setColour (juce::TextEditor::outlineColourId, colours::textDim.withAlpha (0.25f));
        body.setColour (juce::TextEditor::textColourId, colours::textDim);
        body.setText (buildText(), false);
        addAndMakeVisible (body);

        siteBtn.onClick  = [] { juce::URL (about::website).launchInDefaultBrowser(); };
        legalBtn.onClick = [] { juce::URL (juce::String (about::website) + "/legal").launchInDefaultBrowser(); };
        closeBtn.onClick = [this] { setVisible (false); };
        for (auto* b : { &siteBtn, &legalBtn, &closeBtn }) addAndMakeVisible (*b);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::bgDeep.withAlpha (0.97f));
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (colours::seaBright.withAlpha (0.25f));
        g.drawRoundedRectangle (b, 10.0f, 1.0f);
        ui::drawBloom (g, { b.getCentreX(), b.getY() + 40.0f }, b.getWidth() * 0.45f,
                       colours::sea, 0.10f);
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced (26, 20);
        auto top = a.removeFromTop (34);
        title.setBounds (top.removeFromLeft (260));
        closeBtn.setBounds (top.removeFromRight (80));
        version.setBounds (a.removeFromTop (20));
        a.removeFromTop (10);

        auto row = a.removeFromBottom (30);
        siteBtn.setBounds (row.removeFromLeft (150));
        row.removeFromLeft (8);
        legalBtn.setBounds (row.removeFromLeft (170));
        a.removeFromBottom (10);

        body.setBounds (a);
    }

private:
    static juce::String buildText()
    {
        juce::String t;
        t << "SOUND CREDITS\n"
          << "Some recordings come from Freesound under the Creative Commons\n"
          << "Attribution 4.0 licence (creativecommons.org/licenses/by/4.0/).\n"
          << "All have been edited for use here - trimmed, looped and processed -\n"
          << "so none is the original file as published.\n\n";
        for (const auto& c : about::ccByCredits)
            t << "    " << c << "\n";

        t << "\nOther recordings in the library are public domain (CC0) or original\n"
          << "material created for Nebula Tide.\n\n"
          << "THIRD-PARTY SOFTWARE\n";
        for (const auto& c : about::thirdParty)
            t << "    " << c << "\n";

        t << "\nLICENCE\n"
          << "Nebula Tide and its sound library are \xc2\xa9 Amanorsac Studio.\n"
          << "Your licence covers use on two machines; move one from My Apps at\n"
          << "amanorsac.studio, or use DEACTIVATE THIS DEVICE in Settings.\n";
        return t;
    }

    juce::Label title, version;
    juce::TextEditor body;
    juce::TextButton siteBtn { "AMANORSAC.STUDIO" }, legalBtn { "PRIVACY & TERMS" },
                     closeBtn { "DONE" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AboutView)
};
