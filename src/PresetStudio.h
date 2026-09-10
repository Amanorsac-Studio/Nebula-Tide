#pragma once
#include "PluginProcessor.h"

// Declared without the default argument: PluginEditor.h declares the same
// function with one, and a default may only be given once per translation unit.
namespace ui
{
    void drawBloom (juce::Graphics&, juce::Point<float>, float, juce::Colour, float intensity);
    juce::Font titleFont (float);
    juce::Font labelFont (float);
    juce::Font bodyFont  (float);
}

//==============================================================================
// ── Studio ────────────────────────────────────────────────────────────────
//
// Nebula Forge, brought inside the app as a second dashboard. Forge ran as a
// local web server, which was fine for authoring the shipped library on one
// machine but is no use to somebody who just installed the plugin — so the
// parts that matter are rebuilt here natively.
//
// What people make is theirs: plain audio copied into their own folder, never
// into the encrypted container. They can find it, back it up, and take it with
// them. The shipped library stays read-only and stays encrypted, and the two
// never mix — a built-in preset cannot be edited or deleted from here, and a
// user preset is not allowed to take a built-in name.

// One of the twelve key slots: drop a file on it, or click to browse.
class KeySlot : public juce::Component,
                public juce::FileDragAndDropTarget
{
public:
    KeySlot (int keyIndex, std::function<void (int)> onBrowse, std::function<void (int, juce::File)> onDropped)
        : key (keyIndex), browse (std::move (onBrowse)), dropped (std::move (onDropped)) {}

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && file.existsAsFile()) { clear(); return; }
        if (browse) browse (key);
    }

    bool isInterestedInFileDrag (const juce::StringArray& files) override
    {
        for (const auto& f : files)
            if (isAudio (f)) return true;
        return false;
    }
    void fileDragEnter (const juce::StringArray&, int, int) override { hovering = true;  repaint(); }
    void fileDragExit  (const juce::StringArray&)           override { hovering = false; repaint(); }
    void filesDropped (const juce::StringArray& files, int, int) override
    {
        hovering = false;
        for (const auto& f : files)
            if (isAudio (f)) { if (dropped) dropped (key, juce::File (f)); break; }
        repaint();
    }

    void setFile (juce::File f) { file = std::move (f); repaint(); }
    // juce::File() spelled out: clang reads a bare {} here as ambiguous
    // between the copy and move assignment operators, so this does not
    // compile on macOS or iOS even though MSVC accepts it.
    void clear() { file = juce::File(); if (dropped) dropped (key, {}); repaint(); }
    juce::File getFile() const { return file; }
    int getKey() const { return key; }

    static bool isAudio (const juce::String& path)
    {
        const auto e = path.fromLastOccurrenceOf (".", false, false).toLowerCase();
        return e == "wav" || e == "mp3" || e == "ogg" || e == "flac" || e == "aiff" || e == "aif";
    }

private:
    int key;
    juce::File file;
    bool hovering = false;
    std::function<void (int)> browse;
    std::function<void (int, juce::File)> dropped;
};

//==============================================================================
class PresetStudio : public juce::Component,
                     private juce::Timer
{
public:
    PresetStudio (NebulaTideProcessor&, std::function<void()> onLibraryChanged);
    ~PresetStudio() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override { if (isVisible()) refreshList(); }

private:
    void timerCallback() override;
    void refreshList();          // rebuild the user-preset list from the processor
    void loadPreset (const juce::String& name);
    void startNew();
    void save();
    void removeCurrent();
    void browseForSlot (int key);
    void importAux (int cat);
    void say (const juce::String& text, bool bad);

    NebulaTideProcessor& processor;
    std::function<void()> libraryChanged;

    juce::Label   heading, nameLabel, colourLabel, keysLabel, auxLabel, message;
    juce::TextEditor nameBox;
    juce::TextButton newBtn { "+ NEW PRESET" }, saveBtn { "SAVE" }, deleteBtn { "DELETE" }, closeBtn { "DONE" };
    juce::TextButton addFxBtn { "+ FX SOUND" }, addTexBtn { "+ TEXTURE" };
    juce::Viewport listView;
    juce::Component listHolder;
    juce::OwnedArray<juce::TextButton> listButtons;
    juce::OwnedArray<KeySlot> slots;

    // A small fixed palette rather than a full colour picker: every one of
    // these already reads correctly against the background and through the
    // whole-UI retint, which an arbitrary colour cannot be trusted to do.
    struct Swatch { juce::Colour colour; juce::Rectangle<int> bounds; };
    juce::Array<Swatch> swatches;
    int selectedSwatch = 0;
    void mouseDown (const juce::MouseEvent&) override;

    juce::ComboBox reverbBox;
    juce::Slider mixSlider, sizeSlider, dampSlider;
    juce::Label  mixLabel, sizeLabel, dampLabel;

    juce::String editingName;      // empty while creating a new preset
    std::unique_ptr<juce::FileChooser> chooser;
    juce::int64 messageUntil = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetStudio)
};
