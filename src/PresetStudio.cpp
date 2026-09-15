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
    cap (keysLabel,   "KEYS  -  drop audio on a key, click one to browse, or import a whole folder");
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

    folderImport = std::make_unique<FolderImport> (
        [this] (const std::array<juce::File, 12>& keys, const juce::String& name)
        {
            int filled = 0;
            for (int k = 0; k < 12; ++k)
                if (keys[(size_t) k].existsAsFile())
                {
                    slots[k]->setFile (keys[(size_t) k]);
                    ++filled;
                }
            // Only suggest a name; never overwrite one the person already typed.
            if (nameBox.getText().trim().isEmpty() && name.isNotEmpty())
                nameBox.setText (name, juce::dontSendNotification);
            say ("Imported " + juce::String (filled) + (filled == 1 ? " key." : " keys.")
                   + " Choose a colour and save.", false);
            repaint();
        });
    addChildComponent (*folderImport);

    importFolderBtn.onClick = [this] { chooseFolderToImport(); };
    addAndMakeVisible (importFolderBtn);

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
    addFxBtn.onClick  = [this] { showAuxLibrary(); };
    addTexBtn.onClick = [this] { showAuxLibrary(); };
    makerLabel.setText ("MADE BY", juce::dontSendNotification);
    makerLabel.setFont (ui::labelFont (10.0f));
    makerLabel.setColour (juce::Label::textColourId, colours::textDim);
    addAndMakeVisible (makerLabel);
    makerBox.setTextToShowWhenEmpty ("your name, optional", colours::textDim.withAlpha (0.6f));
    if (auto saved = makerNameFile(); saved.existsAsFile())
        makerBox.setText (saved.loadFileAsString().trim(), false);
    addAndMakeVisible (makerBox);

    shareBtn.onClick  = [this] { sharePreset(); };
    importBtn.onClick = [this] { importPack(); };
    addAndMakeVisible (shareBtn);
    addAndMakeVisible (importBtn);

    closeBtn.onClick  = [this] { setVisible (false); };
    for (auto* b : { &newBtn, &saveBtn, &deleteBtn, &addFxBtn, &addTexBtn, &closeBtn, &shareBtn, &importBtn })
        addAndMakeVisible (*b);

    auxLibrary = std::make_unique<AuxLibrary> (processor, [this]
    {
        refreshAuxChoices();
        if (libraryChanged) libraryChanged();
    });
    addChildComponent (*auxLibrary);

    fxLabel.setText ("DEFAULT FX", juce::dontSendNotification);
    texLabel.setText ("DEFAULT TEXTURE", juce::dontSendNotification);
    for (auto* l : { &fxLabel, &texLabel })
    {
        l->setFont (ui::labelFont (10.0f));
        l->setColour (juce::Label::textColourId, colours::textDim);
        addAndMakeVisible (*l);
    }
    addAndMakeVisible (fxBox);
    addAndMakeVisible (texBox);
    refreshAuxChoices();

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
    refreshAuxChoices();
    fxBox.setSelectedId (1, juce::dontSendNotification);
    texBox.setSelectedId (1, juce::dontSendNotification);
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
            refreshAuxChoices();
            fxBox.setText (g.defaultFx.isNotEmpty() ? g.defaultFx : "(none)", juce::dontSendNotification);
            texBox.setText (g.defaultTex.isNotEmpty() ? g.defaultTex : "(none)", juce::dontSendNotification);
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

//==============================================================================
// Where the maker name is kept. Beside the presets rather than in the plugin
// state, because it belongs to the person, not to any one session or host.
juce::File PresetStudio::makerNameFile()
{
    return NebulaTideProcessor::userContentDir().getParentDirectory().getChildFile ("maker.txt");
}

// Imported presets must not silently overwrite something already there, and
// must not collide with a built-in name either.
juce::String PresetStudio::uniqueUserName (const juce::String& wanted) const
{
    auto taken = [this] (const juce::String& n)
    {
        for (const auto& g : processor.getPresets())
            if (g.name.equalsIgnoreCase (n)) return true;
        return false;
    };
    if (! taken (wanted)) return wanted;
    for (int n = 2; n < 100; ++n)
    {
        const auto candidate = wanted + " " + juce::String (n);
        if (! taken (candidate)) return candidate;
    }
    return wanted + " " + juce::String (juce::Random::getSystemRandom().nextInt (9999));
}

void PresetStudio::chooseFolderToImport()
{
    chooser = std::make_unique<juce::FileChooser> (
        "Choose the folder of stems for this preset",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory));

    chooser->launchAsync (juce::FileBrowserComponent::openMode
                            | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (! dir.isDirectory())
                return;
            folderImport->setBounds (getLocalBounds().reduced (40, 30));
            folderImport->show (dir);
        });
}

void PresetStudio::showAuxLibrary()
{
    auxLibrary->setBounds (getLocalBounds().reduced (40, 30));
    auxLibrary->setVisible (true);
    auxLibrary->toFront (true);
}

// The two dropdowns list whatever is in the library right now, plus a "none".
// Rebuilt rather than patched, because a sound can appear or vanish while the
// panel is open.
void PresetStudio::refreshAuxChoices()
{
    auto fill = [this] (juce::ComboBox& box, int cat)
    {
        const auto keep = box.getText();
        box.clear (juce::dontSendNotification);
        box.addItem ("(none)", 1);
        const auto& list = processor.getAuxSounds (cat);
        for (int i = 0; i < list.size(); ++i)
            box.addItem (list.getReference (i).name, i + 2);
        box.setText (keep.isNotEmpty() ? keep : "(none)", juce::dontSendNotification);
        if (box.getSelectedId() == 0) box.setSelectedId (1, juce::dontSendNotification);
    };
    fill (fxBox, 0);
    fill (texBox, 1);
}

void PresetStudio::sharePreset()
{
    presetshare::Meta meta;
    meta.name  = nameBox.getText().trim();
    meta.maker = makerBox.getText().trim();
    meta.colour = swatches.getReference (selectedSwatch).colour;
    meta.reverbType = reverbBox.getSelectedId() - 1;
    meta.mix  = (float) mixSlider.getValue();
    meta.size = (float) sizeSlider.getValue();
    meta.damp = (float) dampSlider.getValue();

    juce::File keys[12];
    for (auto* s : slots)
        if (s->getFile().existsAsFile() && s->getKey() >= 0 && s->getKey() < 12)
            keys[s->getKey()] = s->getFile();

    if (meta.name.isEmpty()) { say ("Give the preset a name first.", true); return; }

    // Look up the files behind the two dropdown choices.
    juce::File fxFile, texFile;
    auto findAux = [this] (int cat, const juce::ComboBox& box) -> juce::File
    {
        if (box.getSelectedId() <= 1) return {};
        for (const auto& s : processor.getAuxSounds (cat))
            if (s.name == box.getText()) return s.file;
        return {};
    };
    fxFile  = findAux (0, fxBox);
    texFile = findAux (1, texBox);

    // Remember the name for next time, only once there is one.
    if (meta.maker.isNotEmpty())
        makerNameFile().replaceWithText (meta.maker);

    const auto suggested = juce::File::getSpecialLocation (juce::File::userDesktopDirectory)
                               .getChildFile (meta.name.replaceCharacter (' ', '_') + presetshare::extension);

    shareChooser = std::make_unique<juce::FileChooser> (
        "Share this preset as a file", suggested, juce::String ("*") + presetshare::extension);

    shareChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                 | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, meta, fxFile, texFile, keys = std::array<juce::File, 12>{ keys[0], keys[1], keys[2], keys[3],
                                                        keys[4], keys[5], keys[6], keys[7],
                                                        keys[8], keys[9], keys[10], keys[11] }]
        (const juce::FileChooser& fc)
        {
            auto dest = fc.getResult();
            if (dest == juce::File()) return;
            if (! dest.getFileName().endsWithIgnoreCase (presetshare::extension))
                dest = dest.withFileExtension (presetshare::extension);

            const auto r = presetshare::writePack (dest, meta, keys.data(), fxFile, texFile);
            if (r.failed()) { say (r.getErrorMessage(), true); return; }

            say ("Shared. " + dest.getFileName() + " is ready to send.", false);
        });
}

void PresetStudio::importPack()
{
    shareChooser = std::make_unique<juce::FileChooser> (
        "Open a shared preset",
        juce::File::getSpecialLocation (juce::File::userDesktopDirectory),
        juce::String ("*") + presetshare::extension);

    shareChooser->launchAsync (juce::FileBrowserComponent::openMode
                                 | juce::FileBrowserComponent::canSelectFiles,
        [this] (const juce::FileChooser& fc)
        {
            const auto src = fc.getResult();
            if (src != juce::File()) acceptPack (src);
        });
}

void PresetStudio::acceptPack (const juce::File& src)
{
    presetshare::Contents peeked;
    auto r = presetshare::peekPack (src, peeked);
    if (r.failed()) { say (r.getErrorMessage(), true); return; }

    const auto landing = uniqueUserName (peeked.meta.name);

    presetshare::Contents got;
    const auto userDir = NebulaTideProcessor::userContentDir();
    r = presetshare::readPack (src, userDir, got, landing,
                               userDir.getChildFile ("fx"),
                               userDir.getChildFile ("textures"));
    if (r.failed()) { say (r.getErrorMessage(), true); return; }

    // Carry the colour and the reverb across too, so an imported preset looks
    // and sounds the way its maker left it rather than reverting to defaults.
    juce::Array<NebulaTideProcessor::UserSlot> chosen;
    const auto stem = landing.replaceCharacter (' ', '_');
    for (int i = 0; i < 12; ++i)
    {
        if (! got.keyPresent[i]) continue;
        const auto f = NebulaTideProcessor::userContentDir()
                           .getChildFile (stem + "_" + presetshare::keyNames[i] + ".flac");
        if (f.existsAsFile()) chosen.add ({ i, f });
    }
    // The manifest stores the display name, which is the filename with
    // underscores and hyphens turned into spaces - the same transform the
    // scanner applies, so the two agree.
    auto auxDisplayName = [] (const juce::String& fileName)
    {
        return fileName.upToLastOccurrenceOf (".", false, false)
                       .replaceCharacters ("_-", "  ");
    };
    const auto saved = processor.saveUserPreset (landing, got.meta.colour, chosen,
                                                 got.meta.reverbType, got.meta.mix,
                                                 got.meta.size, got.meta.damp,
                                                 got.hasFx  ? auxDisplayName (got.meta.fxName)  : juce::String(),
                                                 got.hasTex ? auxDisplayName (got.meta.texName) : juce::String());
    if (saved.failed()) { say (saved.getErrorMessage(), true); return; }

    processor.reloadLibrary();
    if (libraryChanged) libraryChanged();
    refreshAuxChoices();
    refreshList();
    loadPreset (landing);

    juce::String note = "Imported " + landing + ".";
    if (got.meta.maker.isNotEmpty()) note += " Made by " + got.meta.maker + ".";
    if (got.hasFx || got.hasTex) note += " Its sounds came with it.";
    if (! landing.equalsIgnoreCase (peeked.meta.name))
        note += " Renamed, that name was taken.";
    say (note, false);
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
                                             (float) dampSlider.getValue(),
                                             (fxBox.getSelectedId() <= 1 ? juce::String() : fxBox.getText()),
                                             (texBox.getSelectedId() <= 1 ? juce::String() : texBox.getText()));
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
            // Cancel first, and deliberately so. The callback is given the
            // index of the button pressed, counting from zero, and this used
            // to test for 1 while Delete sat at 0 — so Delete did nothing and
            // Cancel deleted. Putting Cancel at index 0 also means a dismissed
            // dialog cancels rather than destroys.
            .withButton ("Cancel")
            .withButton ("Delete"),
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
    if (auxLibrary != nullptr && auxLibrary->isVisible())
        auxLibrary->setBounds (getLocalBounds().reduced (40, 30));

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
    nameBox.setBounds (row.removeFromLeft (200));
    row.removeFromLeft (10);
    makerLabel.setBounds (row.removeFromLeft (56));
    makerBox.setBounds (row.removeFromLeft (150));
    row.removeFromLeft (16);
    {
        auto sw = row.removeFromLeft (swatches.size() * 30).withSizeKeepingCentre (swatches.size() * 30, 22);
        for (auto& s : swatches)
            s.bounds = sw.removeFromLeft (30);
    }
    area.removeFromTop (14);

    {
        auto keysRow = area.removeFromTop (22);
        importFolderBtn.setBounds (keysRow.removeFromRight (150).reduced (0, 1));
        keysLabel.setBounds (keysRow);
    }
    if (folderImport != nullptr && folderImport->isVisible())
        folderImport->setBounds (getLocalBounds().reduced (40, 30));
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

    area.removeFromTop (8);
    row = area.removeFromTop (24);
    fxLabel.setBounds (row.removeFromLeft (74));
    fxBox.setBounds (row.removeFromLeft (150));
    row.removeFromLeft (16);
    texLabel.setBounds (row.removeFromLeft (104));
    texBox.setBounds (row.removeFromLeft (150));

    area.removeFromTop (12);
    row = area.removeFromTop (30);
    saveBtn.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (8);
    deleteBtn.setBounds (row.removeFromLeft (100));
    row.removeFromLeft (8);
    shareBtn.setBounds (row.removeFromLeft (100));
    row.removeFromLeft (8);
    importBtn.setBounds (row.removeFromLeft (100));
    row.removeFromLeft (24);
    auxLabel.setBounds (row.removeFromLeft (170));
    addFxBtn.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (8);
    addTexBtn.setBounds (row.removeFromLeft (110));
}
