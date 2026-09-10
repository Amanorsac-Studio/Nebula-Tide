#include "PresetStudio.h"

namespace colours
{
    extern const juce::Colour bgDeep, foam, textDim;
    extern juce::Colour bgMid, sea, seaBright;
}

//==============================================================================
void KeySlot::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const bool filled = file.existsAsFile();

    g.setColour (colours::bgMid.withAlpha (hovering ? 0.95f : 0.55f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour ((hovering || filled ? colours::seaBright : colours::textDim)
                     .withAlpha (hovering ? 0.95f : (filled ? 0.6f : 0.28f)));
    g.drawRoundedRectangle (r, 6.0f, filled ? 1.2f : 0.8f);

    auto top = r.removeFromTop (16.0f).reduced (6.0f, 0.0f);
    g.setColour (filled ? colours::seaBright : colours::textDim);
    g.setFont (ui::labelFont (11.0f));
    g.drawText (keynames::display[key], top, juce::Justification::centredLeft);

    g.setColour (colours::textDim.withAlpha (filled ? 0.85f : 0.45f));
    g.setFont (ui::bodyFont (9.5f));
    g.drawFittedText (filled ? file.getFileName() : "drop audio  /  click",
                      r.reduced (6.0f, 2.0f).toNearestInt(), juce::Justification::centredLeft, 2, 0.9f);

    if (filled)
    {
        g.setColour (colours::textDim.withAlpha (0.5f));
        g.setFont (ui::bodyFont (8.0f));
        g.drawText ("right-click to clear", r.reduced (6.0f, 2.0f), juce::Justification::bottomRight);
    }
}

//==============================================================================
PresetStudio::PresetStudio (NebulaTideProcessor& p, std::function<void()> onChanged)
    : processor (p), libraryChanged (std::move (onChanged))
{
    auto cap = [this] (juce::Label& l, const juce::String& t, float size = 10.0f)
    {
        l.setText (t, juce::dontSendNotification);
        l.setFont (ui::labelFont (size));
        l.setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (l);
    };

    heading.setText ("STUDIO", juce::dontSendNotification);
    heading.setFont (ui::titleFont (18.0f));
    heading.setColour (juce::Label::textColourId, colours::foam);
    addAndMakeVisible (heading);

    cap (nameLabel,   "PRESET NAME");
    cap (colourLabel, "COLOUR");
    cap (keysLabel,   "KEYS  -  drop an audio file on each key you have");
    cap (auxLabel,    "YOUR FX AND TEXTURES");

    message.setFont (ui::bodyFont (11.0f));
    message.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (message);

    nameBox.setFont (ui::bodyFont (13.0f));
    nameBox.setColour (juce::TextEditor::backgroundColourId, colours::bgDeep.withAlpha (0.7f));
    nameBox.setColour (juce::TextEditor::outlineColourId, colours::textDim.withAlpha (0.35f));
    nameBox.setColour (juce::TextEditor::textColourId, colours::foam);
    nameBox.setTextToShowWhenEmpty ("My Pad", colours::textDim.withAlpha (0.5f));
    addAndMakeVisible (nameBox);

    for (int i = 0; i < 12; ++i)
        slots.add (new KeySlot (i,
            [this] (int k) { browseForSlot (k); },
            [this] (int, juce::File) { repaint(); }));
    for (auto* s : slots) addAndMakeVisible (s);

    for (auto hex : { 0xff4fe3ff, 0xffff4dbe, 0xff9b5cff, 0xffff8c2a,
                      0xff1e4dff, 0xff3ddc97, 0xffffc96b, 0xffff5a6e })
        swatches.add ({ juce::Colour ((juce::uint32) hex), {} });

    reverbBox.addItemList ({ "Room", "Plate", "Hall" }, 1);
    reverbBox.setSelectedId (3, juce::dontSendNotification);
    addAndMakeVisible (reverbBox);

    auto slider = [this] (juce::Slider& s, juce::Label& l, const juce::String& name, double def)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setRange (0.0, 1.0, 0.01);
        s.setValue (def, juce::dontSendNotification);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setFont (ui::labelFont (9.5f));
        l.setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (l);
    };
    slider (mixSlider,  mixLabel,  "MIX",  0.4);
    slider (sizeSlider, sizeLabel, "SIZE", 0.85);
    slider (dampSlider, dampLabel, "DAMP", 0.45);

    listView.setViewedComponent (&listHolder, false);
    listView.setScrollBarsShown (true, false);
    listView.setScrollBarThickness (8);
    addAndMakeVisible (listView);

    newBtn.onClick    = [this] { startNew(); };
    saveBtn.onClick   = [this] { save(); };
    deleteBtn.onClick = [this] { removeCurrent(); };
    addFxBtn.onClick  = [this] { importAux (0); };
    addTexBtn.onClick = [this] { importAux (1); };
    closeBtn.onClick  = [this] { setVisible (false); };
    for (auto* b : { &newBtn, &saveBtn, &deleteBtn, &addFxBtn, &addTexBtn, &closeBtn })
        addAndMakeVisible (*b);

    startNew();
    startTimerHz (4);
}

PresetStudio::~PresetStudio() = default;

void PresetStudio::timerCallback()
{
    if (messageUntil > 0 && juce::Time::currentTimeMillis() > messageUntil)
    {
        messageUntil = 0;
        message.setText ({}, juce::dontSendNotification);
    }
}

void PresetStudio::say (const juce::String& text, bool bad)
{
    message.setColour (juce::Label::textColourId, bad ? juce::Colour (0xffff5a6e) : colours::seaBright);
    message.setText (text, juce::dontSendNotification);
    messageUntil = juce::Time::currentTimeMillis() + 6000;
}

void PresetStudio::refreshList()
{
    listButtons.clear();
    for (const auto& g : processor.getPresets())
    {
        if (! g.isUser) continue;                    // the shipped library is not editable
        const juce::String name = g.name;
        auto* b = listButtons.add (new juce::TextButton (name));
        b->setColour (juce::TextButton::textColourOffId,
                      name.equalsIgnoreCase (editingName) ? colours::seaBright : colours::textDim);
        b->onClick = [this, name] { loadPreset (name); };
        listHolder.addAndMakeVisible (b);
    }
    resized();
}

void PresetStudio::startNew()
{
    editingName = {};
    nameBox.setText ({}, juce::dontSendNotification);
    for (auto* s : slots) s->setFile ({});
    selectedSwatch = 0;
    reverbBox.setSelectedId (3, juce::dontSendNotification);
    mixSlider.setValue (0.4, juce::dontSendNotification);
    sizeSlider.setValue (0.85, juce::dontSendNotification);
    dampSlider.setValue (0.45, juce::dontSendNotification);
    deleteBtn.setEnabled (false);
    refreshList();
    repaint();
}

void PresetStudio::loadPreset (const juce::String& name)
{
    for (const auto& g : processor.getPresets())
    {
        if (! g.isUser || ! g.name.equalsIgnoreCase (name)) continue;

        editingName = g.name;
        nameBox.setText (g.name, juce::dontSendNotification);
        for (int k = 0; k < 12; ++k)
            slots[k]->setFile (g.keys[k].file);

        selectedSwatch = 0;
        for (int i = 0; i < swatches.size(); ++i)
            if (swatches.getReference (i).colour.getARGB() == g.colour.getARGB())
                selectedSwatch = i;

        reverbBox.setSelectedId (g.rType + 1, juce::dontSendNotification);
        mixSlider.setValue (g.rMix, juce::dontSendNotification);
        sizeSlider.setValue (g.rSize, juce::dontSendNotification);
        dampSlider.setValue (g.rDamp, juce::dontSendNotification);
        deleteBtn.setEnabled (true);
        refreshList();
        repaint();
        return;
    }
}

void PresetStudio::browseForSlot (int key)
{
    chooser = std::make_unique<juce::FileChooser> (
        "Choose the audio for " + juce::String (keynames::display[key]),
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.mp3;*.ogg;*.flac;*.aiff;*.aif");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this, key] (const juce::FileChooser& fc)
        {
            const auto f = fc.getResult();
            if (f.existsAsFile() && key >= 0 && key < slots.size())
                slots[key]->setFile (f);
        });
}

void PresetStudio::save()
{
    juce::Array<NebulaTideProcessor::UserSlot> chosen;
    for (auto* s : slots)
        if (s->getFile().existsAsFile())
            chosen.add ({ s->getKey(), s->getFile() });

    const auto r = processor.saveUserPreset (nameBox.getText(),
                                             swatches.getReference (selectedSwatch).colour,
                                             chosen,
                                             reverbBox.getSelectedId() - 1,
                                             (float) mixSlider.getValue(),
                                             (float) sizeSlider.getValue(),
                                             (float) dampSlider.getValue());
    if (r.failed())
    {
        say (r.getErrorMessage(), true);
        return;
    }

    editingName = nameBox.getText().trim();
    processor.reloadLibrary();
    if (libraryChanged) libraryChanged();
    // Point the slots at the copies in the user folder, not the originals —
    // otherwise moving or deleting the source file would appear to break a
    // preset that is actually fine.
    loadPreset (editingName);
    say ("Saved. It is in your preset list now.", false);
}

void PresetStudio::removeCurrent()
{
    if (editingName.isEmpty()) return;
    const auto name = editingName;

    juce::NativeMessageBox::showAsync (
        juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::WarningIcon)
            .withTitle ("Delete preset")
            .withMessage ("Delete \"" + name + "\" and its audio files?\n\nThis cannot be undone.")
            .withButton ("Delete")
            .withButton ("Cancel"),
        [this, name] (int result)
        {
            if (result != 1) return;
            const auto r = processor.deleteUserPreset (name);
            if (r.failed()) { say (r.getErrorMessage(), true); return; }
            processor.reloadLibrary();
            if (libraryChanged) libraryChanged();
            startNew();
            say ("Deleted.", false);
        });
}

void PresetStudio::importAux (int cat)
{
    chooser = std::make_unique<juce::FileChooser> (
        cat == 0 ? "Choose an FX sound" : "Choose a texture",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.mp3;*.ogg;*.flac;*.aiff;*.aif");

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::canSelectMultipleItems,
        [this, cat] (const juce::FileChooser& fc)
        {
            int added = 0;
            juce::String lastError;
            for (const auto& f : fc.getResults())
            {
                const auto r = processor.importAuxSound (cat, f);
                if (r.wasOk()) ++added; else lastError = r.getErrorMessage();
            }
            if (added > 0)
            {
                processor.reloadLibrary();
                if (libraryChanged) libraryChanged();
                say (juce::String (added) + (added == 1 ? " sound added." : " sounds added."), false);
            }
            else if (lastError.isNotEmpty())
                say (lastError, true);
        });
}

void PresetStudio::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < swatches.size(); ++i)
        if (swatches.getReference (i).bounds.contains (e.getPosition()))
        {
            selectedSwatch = i;
            repaint();
            return;
        }
}

//==============================================================================
void PresetStudio::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgDeep.withAlpha (0.97f));
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (colours::seaBright.withAlpha (0.25f));
    g.drawRoundedRectangle (b, 10.0f, 1.0f);

    ui::drawBloom (g, { b.getCentreX(), b.getY() + 40.0f }, b.getWidth() * 0.45f,
                   colours::sea, 0.10f);

    // colour swatches
    for (int i = 0; i < swatches.size(); ++i)
    {
        const auto& s = swatches.getReference (i);
        auto r = s.bounds.toFloat().reduced (2.0f);
        g.setColour (s.colour);
        g.fillRoundedRectangle (r, 4.0f);
        if (i == selectedSwatch)
        {
            g.setColour (colours::foam);
            g.drawRoundedRectangle (r.expanded (2.0f), 5.0f, 1.6f);
        }
    }

    // a quiet note about where their files actually live
    g.setColour (colours::textDim.withAlpha (0.55f));
    g.setFont (ui::bodyFont (10.0f));
    g.drawText ("Your sounds are copied to  " + NebulaTideProcessor::userContentDir().getFullPathName()
                    + "  as ordinary audio files.",
                getLocalBounds().reduced (24, 12), juce::Justification::bottomLeft, true);
}

void PresetStudio::resized()
{
    auto area = getLocalBounds().reduced (24, 18);

    auto top = area.removeFromTop (30);
    heading.setBounds (top.removeFromLeft (140));
    closeBtn.setBounds (top.removeFromRight (72));
    message.setBounds (top.reduced (8, 0));
    area.removeFromTop (10);

    area.removeFromBottom (22);                        // room for the folder note

    // left: the person's presets
    auto left = area.removeFromLeft (190);
    newBtn.setBounds (left.removeFromTop (30));
    left.removeFromTop (8);
    listView.setBounds (left);
    listHolder.setSize (left.getWidth() - 10, juce::jmax (1, listButtons.size() * 28));
    {
        auto inner = listHolder.getLocalBounds();
        for (auto* b : listButtons)
            b->setBounds (inner.removeFromTop (28).reduced (0, 2));
    }

    area.removeFromLeft (18);

    // right: the editor
    auto row = area.removeFromTop (22);
    nameLabel.setBounds (row.removeFromLeft (110));
    colourLabel.setBounds (row.removeFromRight (240).removeFromLeft (70));
    area.removeFromTop (2);

    row = area.removeFromTop (28);
    nameBox.setBounds (row.removeFromLeft (260));
    row.removeFromLeft (16);
    {
        auto sw = row.removeFromLeft (swatches.size() * 30).withSizeKeepingCentre (swatches.size() * 30, 22);
        for (auto& s : swatches)
            s.bounds = sw.removeFromLeft (30);
    }
    area.removeFromTop (14);

    keysLabel.setBounds (area.removeFromTop (18));
    area.removeFromTop (4);

    auto grid = area.removeFromTop (juce::jmax (120, area.getHeight() - 128));
    const int cols = 4, rows = 3;
    const int cw = grid.getWidth() / cols;
    const int chh = grid.getHeight() / rows;
    for (int r = 0; r < rows; ++r)
    {
        auto line = grid.removeFromTop (chh);
        for (int c = 0; c < cols; ++c)
        {
            const int idx = r * cols + c;
            if (idx < slots.size())
                slots[idx]->setBounds (line.removeFromLeft (cw).reduced (3));
        }
    }

    area.removeFromTop (10);
    row = area.removeFromTop (26);
    reverbBox.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (12);
    auto trim = [&row] (juce::Label& l, juce::Slider& s)
    {
        l.setBounds (row.removeFromLeft (38));
        s.setBounds (row.removeFromLeft (96));
        row.removeFromLeft (8);
    };
    trim (mixLabel, mixSlider);
    trim (sizeLabel, sizeSlider);
    trim (dampLabel, dampSlider);

    area.removeFromTop (12);
    row = area.removeFromTop (30);
    saveBtn.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (8);
    deleteBtn.setBounds (row.removeFromLeft (100));
    row.removeFromLeft (24);
    auxLabel.setBounds (row.removeFromLeft (170));
    addFxBtn.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (8);
    addTexBtn.setBounds (row.removeFromLeft (110));
}
