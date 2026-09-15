#pragma once
#include "PluginProcessor.h"
#include "UserContent.h"
#include <array>

namespace colours { extern const juce::Colour bgDeep, foam, textDim; extern juce::Colour bgMid, sea, seaBright; }
namespace ui
{
    void drawBloom (juce::Graphics&, juce::Point<float>, float, juce::Colour, float intensity);
    juce::Font titleFont (float);
    juce::Font labelFont (float);
    juce::Font bodyFont  (float);
}

//==============================================================================
// ── Import a folder ───────────────────────────────────────────────────────
//
// Filling twelve key slots one at a time is tedious, and a bounce from a DAW
// usually lands as one folder of stems whose names already carry the key. This
// takes that folder, guesses each key from the filenames, and shows the
// guesses as twelve dropdowns. The person corrects any that are wrong and
// presses Import; nothing reaches the slots until they have seen the list.
//
// An overlay inside Studio rather than a separate window, so it behaves the
// same inside a plugin host.
class FolderImport : public juce::Component
{
public:
    using ImportCallback = std::function<void (const std::array<juce::File, 12>&, const juce::String& name)>;

    explicit FolderImport (ImportCallback onImportIn) : onImport (std::move (onImportIn))
    {
        heading.setText ("IMPORT A FOLDER", juce::dontSendNotification);
        heading.setFont (ui::titleFont (15.0f));
        heading.setColour (juce::Label::textColourId, colours::foam);
        addAndMakeVisible (heading);

        for (auto* l : { &folderLabel, &summary })
        {
            l->setFont (ui::bodyFont (11.0f));
            l->setColour (juce::Label::textColourId, colours::textDim);
            addAndMakeVisible (*l);
        }

        for (int k = 0; k < 12; ++k)
        {
            auto& r = rows[(size_t) k];
            r.key.setText (keynames::display[k], juce::dontSendNotification);
            r.key.setFont (ui::labelFont (12.0f));
            r.key.setColour (juce::Label::textColourId, colours::seaBright);
            r.key.setJustificationType (juce::Justification::centredRight);
            addAndMakeVisible (r.key);
            addAndMakeVisible (r.choice);
        }

        importBtn.onClick = [this] { commit(); };
        cancelBtn.onClick = [this] { setVisible (false); };
        addAndMakeVisible (importBtn);
        addAndMakeVisible (cancelBtn);
    }

    /** Scans the folder, pre-selects the guessed file for each key, and shows. */
    void show (const juce::File& dir)
    {
        match = usercontent::matchFolder (dir);
        folderLabel.setText (dir.getFullPathName(), juce::dontSendNotification);

        for (int k = 0; k < 12; ++k)
        {
            auto& box = rows[(size_t) k].choice;
            box.clear (juce::dontSendNotification);
            box.addItem ("(none)", 1);
            for (int i = 0; i < match.files.size(); ++i)
                box.addItem (match.files.getReference (i).getFileName(), i + 2);

            const int guessed = match.files.indexOf (match.keys[k]);
            box.setSelectedId (guessed >= 0 ? guessed + 2 : 1, juce::dontSendNotification);
        }

        if (match.files.isEmpty())
            summary.setText ("No audio files in this folder.", juce::dontSendNotification);
        else
            summary.setText (juce::String (match.files.size()) + " audio files.  "
                               + juce::String (match.numMatched()) + " of 12 keys matched from the file names.  "
                               + "Check each key, then Import.",
                             juce::dontSendNotification);

        importBtn.setEnabled (! match.files.isEmpty());
        setVisible (true);
        toFront (true);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::bgDeep.withAlpha (0.97f));
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (colours::seaBright.withAlpha (0.25f));
        g.drawRoundedRectangle (b, 10.0f, 1.0f);
        ui::drawBloom (g, { b.getCentreX(), b.getY() + 30.0f }, b.getWidth() * 0.4f, colours::sea, 0.08f);
    }

    void resized() override
    {
        auto a = getLocalBounds().reduced (22, 16);
        heading.setBounds (a.removeFromTop (26));
        folderLabel.setBounds (a.removeFromTop (18));
        summary.setBounds (a.removeFromTop (20));
        a.removeFromTop (8);

        auto buttons = a.removeFromBottom (32);
        cancelBtn.setBounds (buttons.removeFromRight (100));
        buttons.removeFromRight (8);
        importBtn.setBounds (buttons.removeFromRight (120));
        a.removeFromBottom (10);

        // Two columns of six keeps every row visible without scrolling.
        const int colW = (a.getWidth() - 20) / 2;
        const int rowH = juce::jmin (34, a.getHeight() / 6);
        for (int col = 0; col < 2; ++col)
        {
            auto column = a.removeFromLeft (colW);
            if (col == 0) a.removeFromLeft (20);
            for (int k = col * 6; k < col * 6 + 6; ++k)
            {
                auto r = column.removeFromTop (rowH).reduced (0, 3);
                rows[(size_t) k].key.setBounds (r.removeFromLeft (44));
                r.removeFromLeft (8);
                rows[(size_t) k].choice.setBounds (r);
            }
        }
    }

private:
    void commit()
    {
        std::array<juce::File, 12> keys;
        for (int k = 0; k < 12; ++k)
        {
            const int id = rows[(size_t) k].choice.getSelectedId();
            if (id >= 2 && id - 2 < match.files.size())
                keys[(size_t) k] = match.files.getReference (id - 2);
        }
        setVisible (false);
        if (onImport)
            onImport (keys, match.suggestedName);
    }

    struct Row { juce::Label key; juce::ComboBox choice; };

    ImportCallback onImport;
    usercontent::FolderMatch match;
    juce::Label heading, folderLabel, summary;
    std::array<Row, 12> rows;
    juce::TextButton importBtn { "IMPORT" }, cancelBtn { "CANCEL" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FolderImport)
};
