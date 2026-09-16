#pragma once
#include "PluginProcessor.h"
#include "License/LicenseClient.h"

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
// ── Activation ────────────────────────────────────────────────────────────
//
// Built to the studio's License Integration Standard v1.0. The rules that
// shaped this screen are R3, R4 and §11: SecondOut 1.3.0 told customers
// "Activated." on the server's HTTP 200 and then left them staring at the
// activation screen, because the proof had failed to verify and nothing
// noticed. So:
//
//   * success is reported only when a proof verified — LicenseClient sets
//     isLicensed() inside adoptProof(), before the callback runs, so the flag
//     is the thing to trust, never the HTTP status;
//   * the view swaps the moment that flag turns true, from the same code path
//     that runs when a cached proof loads at startup (A3);
//   * a signature that fails says so plainly rather than claiming success.
class ActivationView : public juce::Component,
                       private juce::Timer
{
public:
    ActivationView (amanorsacstudio::LicenseClient& lc, std::function<void()> onLicensed)
        : license (lc), licensed (std::move (onLicensed))
    {
        title.setText ("NEBULA TIDE", juce::dontSendNotification);
        title.setFont (ui::titleFont (26.0f));
        title.setColour (juce::Label::textColourId, colours::foam);
        title.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (title);

        blurb.setText ("Enter the license key from your purchase.\n"
                       "You can find it at amanorsac.studio under My Apps.",
                       juce::dontSendNotification);
        blurb.setFont (ui::bodyFont (13.0f));
        blurb.setColour (juce::Label::textColourId, colours::textDim);
        blurb.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (blurb);

        keyBox.setFont (ui::bodyFont (18.0f));
        keyBox.setJustification (juce::Justification::centred);
        keyBox.setTextToShowWhenEmpty ("NEBU-0000-0000-0000", colours::textDim.withAlpha (0.45f));
        keyBox.setColour (juce::TextEditor::backgroundColourId, colours::bgDeep.withAlpha (0.75f));
        keyBox.setColour (juce::TextEditor::outlineColourId, colours::textDim.withAlpha (0.4f));
        keyBox.setColour (juce::TextEditor::textColourId, colours::foam);
        keyBox.setInputRestrictions (24, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-");
        keyBox.onReturnKey = [this] { activate(); };
        addAndMakeVisible (keyBox);

        activateBtn.onClick = [this] { activate(); };
        addAndMakeVisible (activateBtn);

        buyBtn.onClick = [] { juce::URL ("https://amanorsac.studio").launchInDefaultBrowser(); };
        addAndMakeVisible (buyBtn);

        message.setFont (ui::bodyFont (12.5f));
        message.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (message);

        // Belt and braces alongside the callback: if the flag turns true for
        // any reason - a cached proof loading late, a heartbeat succeeding -
        // the interface follows it (standard §7).
        startTimerHz (4);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::bgDeep);
        auto b = getLocalBounds().toFloat();
        ui::drawBloom (g, { b.getCentreX(), b.getHeight() * 0.34f },
                       b.getWidth() * 0.45f, colours::sea, 0.30f);
    }

    void resized() override
    {
        // A phone in landscape is about 400 points tall, and the on-screen
        // keyboard covers the lower half while the key is typed, so short
        // screens drop the generous desktop margins and keep the box high.
        const bool compact = getHeight() < 600;
        auto a = getLocalBounds().reduced (compact ? 16 : 40);
        a.removeFromTop (compact ? 4 : juce::jmax (20, a.getHeight() / 6));
        title.setBounds (a.removeFromTop (40));
        a.removeFromTop (12);
        blurb.setBounds (a.removeFromTop (44));
        a.removeFromTop (18);
        keyBox.setBounds (a.removeFromTop (44).withSizeKeepingCentre (juce::jmin (360, a.getWidth()), 44));
        a.removeFromTop (14);
        {
            auto r = a.removeFromTop (34).withSizeKeepingCentre (juce::jmin (360, a.getWidth()), 34);
            activateBtn.setBounds (r.removeFromLeft (r.getWidth() / 2 - 5));
            r.removeFromLeft (10);
            buyBtn.setBounds (r);
        }
        a.removeFromTop (14);
        message.setBounds (a.removeFromTop (46));
    }

private:
    void timerCallback() override
    {
        if (license.isLicensed() && licensed)
        {
            stopTimer();
            licensed();
        }
    }

    void say (const juce::String& text, bool bad)
    {
        message.setColour (juce::Label::textColourId,
                           bad ? juce::Colour (0xffff5a6e) : colours::seaBright);
        message.setText (text, juce::dontSendNotification);
    }

    void activate()
    {
        const auto key = keyBox.getText().trim().toUpperCase();
        if (key.isEmpty()) { say ("Enter your license key.", true); return; }

        activateBtn.setEnabled (false);
        say ("Checking...", false);

        license.activate (key, [this] (amanorsacstudio::LicenseResult r)
        {
            activateBtn.setEnabled (true);

            // The flag, not the reply, decides. A server 200 whose proof did
            // not verify is a failure, and saying otherwise is the exact bug
            // the standard was written around.
            if (license.isLicensed())
            {
                say ("Activated.", false);
                if (licensed) { stopTimer(); licensed(); }
                return;
            }
            say (r.message.isNotEmpty() ? r.message
                                        : "That did not activate. Check the key and try again.", true);
        });
    }

    amanorsacstudio::LicenseClient& license;
    std::function<void()> licensed;

    juce::Label title, blurb, message;
    juce::TextEditor keyBox;
    juce::TextButton activateBtn { "ACTIVATE" }, buyBtn { "GET A LICENSE" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ActivationView)
};
