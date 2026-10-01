#pragma once
#include "PluginProcessor.h"

namespace colours { extern const juce::Colour bgDeep, foam, textDim; extern juce::Colour bgMid, sea, seaBright; }
namespace ui
{
    void drawBloom (juce::Graphics&, juce::Point<float>, float, juce::Colour, float intensity);
    juce::Font titleFont (float);
    juce::Font labelFont (float);
    juce::Font bodyFont  (float);
}

//==============================================================================
// ── FX and textures ───────────────────────────────────────────────────────
//
// Until now these could only be added, never seen. The two "+" buttons copied
// a file into a folder and the sound appeared on a star on the main stage,
// with no list, no way to hear one before using it, and no way to remove one
// added by mistake. This is that missing half.
//
// Two columns because the app has two stars, and the distinction between an
// effect and a texture is the distinction between them. Built-in sounds are
// listed but cannot be deleted, so the shipped library stays intact no matter
// what is done here.
class AuxLibrary : public juce::Component,
                   public juce::FileDragAndDropTarget
{
public:
    AuxLibrary (NebulaTideProcessor& p, std::function<void()> onChanged)
        : processor (p), changed (std::move (onChanged))
    {
        heading.setText ("FX & TEXTURES", juce::dontSendNotification);
        heading.setFont (ui::titleFont (15.0f));
        heading.setColour (juce::Label::textColourId, colours::foam);
        addAndMakeVisible (heading);

        hint.setText ("Drop audio files here, or use the buttons. Right-hand column is textures.",
                      juce::dontSendNotification);
        hint.setFont (ui::bodyFont (10.5f));
        hint.setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (hint);

        for (int cat = 0; cat < 2; ++cat)
        {
            auto& c = cols[cat];
            c.title.setText (cat == 0 ? "FX" : "TEXTURES", juce::dontSendNotification);
            c.title.setFont (ui::labelFont (11.0f));
            c.title.setColour (juce::Label::textColourId, colours::seaBright);
            addAndMakeVisible (c.title);

            c.add.setButtonText (cat == 0 ? "+ ADD FX" : "+ ADD TEXTURE");
            c.add.onClick = [this, cat] { browse (cat); };
            addAndMakeVisible (c.add);

            c.view.setViewedComponent (&c.holder, false);
            c.view.setScrollBarsShown (true, false);
            addAndMakeVisible (c.view);
        }

        closeBtn.onClick = [this] { processor.stopAux (0); processor.stopAux (1); setVisible (false); };
        addAndMakeVisible (closeBtn);

        message.setFont (ui::bodyFont (10.5f));
        message.setJustificationType (juce::Justification::centredRight);
        addAndMakeVisible (message);
    }

    void visibilityChanged() override { if (isVisible()) refresh(); }

    void refresh()
    {
        for (int cat = 0; cat < 2; ++cat)
        {
            auto& c = cols[cat];
            c.rows.clear();
            for (const auto& s : processor.getAuxSounds (cat))
                c.rows.add (new Row (*this, cat, s.name, processor.isUserAux (s)));
            for (auto* r : c.rows) c.holder.addAndMakeVisible (*r);
        }
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::bgDeep.withAlpha (0.97f));
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (colours::seaBright.withAlpha (dropping ? 0.7f : 0.25f));
        g.drawRoundedRectangle (b, 10.0f, dropping ? 2.0f : 1.0f);
        ui::drawBloom (g, { b.getCentreX(), b.getY() + 30.0f }, b.getWidth() * 0.4f, colours::sea, 0.08f);
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced (22, 16);
        auto top = a.removeFromTop (28);
        heading.setBounds (top.removeFromLeft (200));
        closeBtn.setBounds (top.removeFromRight (76));
        message.setBounds (top.reduced (6, 0));
        hint.setBounds (a.removeFromTop (18));
        a.removeFromTop (8);

        const int colW = (a.getWidth() - 16) / 2;
        for (int cat = 0; cat < 2; ++cat)
        {
            auto col = a.removeFromLeft (colW);
            if (cat == 0) a.removeFromLeft (16);
            auto& c = cols[cat];
            c.title.setBounds (col.removeFromTop (18));
            c.add.setBounds (col.removeFromBottom (28));
            col.removeFromBottom (6);
            c.view.setBounds (col);
            c.holder.setSize (col.getWidth() - 12, juce::jmax (1, c.rows.size() * 26));
            auto inner = c.holder.getLocalBounds();
            for (auto* r : c.rows) r->setBounds (inner.removeFromTop (26));
        }
    }

    // Which column a file lands in is decided by where it is dropped, which is
    // the only thing on screen that distinguishes the two.
    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (const auto& f : files) if (isAudio (f)) return true;
        return false;
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { dropping = true;  repaint(); }
    void fileDragExit  (const juce::StringArray&)           override { dropping = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int x, int) override
    {
        dropping = false;
        const int cat = x > getWidth() / 2 ? 1 : 0;
        int added = 0;
        juce::String last;
        for (const auto& f : files)
        {
            if (! isAudio (f)) continue;
            const auto r = processor.importAuxSound (cat, juce::File (f));
            if (r.wasOk()) ++added; else last = r.getErrorMessage();
        }
        finish (added, last);
    }

private:
    static bool isAudio (const juce::String& path)
    {
        const auto e = path.fromLastOccurrenceOf (".", false, false).toLowerCase();
        return e == "wav" || e == "mp3" || e == "ogg" || e == "flac" || e == "aiff" || e == "aif";
    }

    void finish (int added, const juce::String& error)
    {
        if (added > 0)
        {
            processor.reloadLibrary();
            if (changed) changed();
            refresh();
            say (juce::String (added) + (added == 1 ? " sound added." : " sounds added."), false);
        }
        else say (error.isNotEmpty() ? error : "Nothing was added.", true);
    }

    void say (const juce::String& text, bool bad)
    {
        message.setText (text, juce::dontSendNotification);
        message.setColour (juce::Label::textColourId,
                           bad ? juce::Colour (0xffff7a7a) : colours::textDim);
    }

    void browse (int cat)
    {
        chooser = std::make_unique<juce::FileChooser> (
            cat == 0 ? "Choose FX sounds" : "Choose textures",
            juce::File::getSpecialLocation (juce::File::userMusicDirectory),
            "*.wav;*.mp3;*.ogg;*.flac;*.aiff;*.aif");

        chooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::canSelectMultipleItems,
            [this, cat] (const juce::FileChooser& fc)
            {
                int added = 0;
                juce::String last;
                for (const auto& f : fc.getResults())
                {
                    const auto r = processor.importAuxSound (cat, f);
                    if (r.wasOk()) ++added; else last = r.getErrorMessage();
                }
                finish (added, last);
            });
    }

    void audition (int cat, const juce::String& name)
    {
        const auto& list = processor.getAuxSounds (cat);
        for (int i = 0; i < list.size(); ++i)
            if (list.getReference (i).name == name)
            {
                processor.setAuxIndex (cat, i);
                processor.triggerAux (cat, i);
                say ("Playing " + name, false);
                return;
            }
    }

    void remove (int cat, const juce::String& name)
    {
        processor.stopAux (cat);
        const auto r = processor.deleteAuxSound (cat, name);
        if (r.failed()) { say (r.getErrorMessage(), true); return; }
        processor.reloadLibrary();
        if (changed) changed();
        refresh();
        say ("Removed " + name + ".", false);
    }

    struct Row : juce::Component
    {
        Row (AuxLibrary& ownerIn, int catIn, juce::String nameIn, bool deletable)
            : owner (ownerIn), cat (catIn), name (std::move (nameIn))
        {
            play.setButtonText (">");
            play.onClick = [this] { owner.audition (cat, name); };
            addAndMakeVisible (play);

            if (deletable)
            {
                del.setButtonText ("X");
                del.onClick = [this] { owner.remove (cat, name); };
                addAndMakeVisible (del);
            }
        }
        void paint (juce::Graphics& g) override
        {
            g.setColour (colours::textDim);
            g.setFont (ui::bodyFont (11.0f));
            g.drawText (name, getLocalBounds().withTrimmedRight (58).withTrimmedLeft (4),
                        juce::Justification::centredLeft, true);
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (0, 2);
            if (del.isVisible()) del.setBounds (r.removeFromRight (26));
            play.setBounds (r.removeFromRight (26));
        }
        AuxLibrary& owner;
        int cat;
        juce::String name;
        juce::TextButton play, del;
    };

    struct Column
    {
        juce::Label title;
        juce::TextButton add;
        juce::Viewport view;
        juce::Component holder;
        juce::OwnedArray<Row> rows;
    };

    NebulaTideProcessor& processor;
    std::function<void()> changed;
    juce::Label heading, hint, message;
    juce::TextButton closeBtn { "DONE" };
    Column cols[2];
    bool dropping = false;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuxLibrary)
};
