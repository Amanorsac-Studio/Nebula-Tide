#include "PluginEditor.h"
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#include <map>

namespace colours
{
    // extern, because a namespace-scope const is internal by default and
    // PresetStudio.cpp needs to link against these same objects.
    extern const juce::Colour bgDeep  { 0xff020c16 };
    extern const juce::Colour foam    { 0xffbdf3ff };
    extern const juce::Colour textDim { 0xff6fa8bd };

    // theme accents — retinted live to the current preset's colour
    juce::Colour bgMid      { 0xff04283f };
    juce::Colour sea        { 0xff16b8d8 };
    juce::Colour seaBright  { 0xff4fe3ff };

    inline void setTheme (juce::Colour c)
    {
        seaBright = c.withMultipliedSaturation (1.05f).brighter (0.05f);
        sea       = c.withMultipliedBrightness (0.75f);
        bgMid     = c.withMultipliedSaturation (0.85f).withBrightness (0.16f);
    }
}

//==============================================================================
// HD helpers
namespace ui
{
    // A true Gaussian bloom sprite per colour, rendered once and cached; drawing
    // it scaled is far cheaper than blurring live and looks like real light.
    void drawBloom (juce::Graphics& g, juce::Point<float> c, float radius,
                    juce::Colour colour, float intensity)
    {
        static std::map<juce::uint32, juce::Image> cache;
        const auto key = colour.withAlpha (1.0f).getARGB();
        auto it = cache.find (key);
        if (it == cache.end())
        {
            if (cache.size() > 48) cache.clear();
            const int n = 192;
            juce::Image img (juce::Image::ARGB, n, n, true);
            {
                juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
                const float half = n * 0.5f;
                for (int y = 0; y < n; ++y)
                    for (int x = 0; x < n; ++x)
                    {
                        const float dx = (x + 0.5f - half) / half, dy = (y + 0.5f - half) / half;
                        const float d2 = dx * dx + dy * dy;
                        const float a = d2 >= 1.0f ? 0.0f : std::exp (-d2 * 4.5f) * (1.0f - d2);   // gaussian, zero at edge
                        bd.setPixelColour (x, y, colour.withAlpha (a));
                    }
            }
            it = cache.emplace (key, img).first;
        }
        g.setOpacity (juce::jlimit (0.0f, 1.0f, intensity));
        g.drawImage (it->second, juce::Rectangle<float> (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f));
        g.setOpacity (1.0f);
    }

    static juce::String pickTypeface (std::initializer_list<const char*> prefs)
    {
        static const juce::StringArray installed = juce::Font::findAllTypefaceNames();
        for (auto* p : prefs)
            if (installed.contains (p)) return p;
        return juce::Font::getDefaultSansSerifFontName();
    }

    juce::Font titleFont (float size)
    {
        static const juce::String name = pickTypeface ({ "Bahnschrift Light", "Bahnschrift", "Avenir Next", "Helvetica Neue", "Roboto", "Segoe UI" });
        return juce::Font (juce::FontOptions (name, size, juce::Font::plain)).withExtraKerningFactor (0.32f);
    }
    juce::Font labelFont (float size)
    {
        static const juce::String name = pickTypeface ({ "Bahnschrift SemiCondensed", "Bahnschrift", "Avenir Next Condensed", "Avenir Next", "Roboto", "Segoe UI" });
        return juce::Font (juce::FontOptions (name, size, juce::Font::plain)).withExtraKerningFactor (0.24f);
    }
    juce::Font bodyFont (float size)
    {
        static const juce::String name = pickTypeface ({ "Bahnschrift", "Segoe UI", "Avenir Next", "Helvetica Neue", "Roboto" });
        return juce::Font (juce::FontOptions (name, size, juce::Font::plain));
    }
}

//==============================================================================
void NebulaBackground::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float energy = juce::jlimit (0.0f, 1.0f, smoothedLevel * 1.8f);
    const float cx = b.getCentreX();
    const float cy = b.getHeight() * 0.42f;

    juce::ColourGradient grad (colours::bgMid.brighter (energy * 0.15f), cx, cy * 0.5f,
                               colours::bgDeep, cx, b.getBottom(), true);
    g.setGradientFill (grad);
    g.fillAll();

    // nebula cloud layer: three huge soft blooms drifting slowly in the theme
    // colour — gives the sky depth instead of a flat gradient
    const float W = b.getWidth(), H = b.getHeight();
    ui::drawBloom (g, { W * (0.30f + 0.06f * std::sin (phase * 0.11f)), H * (0.35f + 0.05f * std::cos (phase * 0.09f)) },
                   W * 0.55f, colours::sea, 0.16f + energy * 0.10f);
    ui::drawBloom (g, { W * (0.72f + 0.05f * std::cos (phase * 0.07f)), H * (0.55f + 0.06f * std::sin (phase * 0.12f)) },
                   W * 0.48f, colours::seaBright, 0.09f + energy * 0.08f);
    ui::drawBloom (g, { W * (0.50f + 0.08f * std::sin (phase * 0.05f)), H * 0.95f },
                   W * 0.6f, colours::sea.withRotatedHue (0.08f), 0.10f);

    // stars streaking past — z-projected, with motion streaks toward the viewer.
    // Each star has its own colour temperature and twinkle.
    const float focal = juce::jmin (b.getWidth(), b.getHeight()) * 0.9f;
    const juce::Colour warm (0xffffe2b8), cool (0xffb9dcff);
    for (auto& s : stars)
    {
        const float px = cx + (s.x / s.z) * focal * 0.5f;
        const float py = cy + (s.y / s.z) * focal * 0.5f;
        if (px < -20 || px > b.getWidth() + 20 || py < -20 || py > b.getHeight() + 20)
            continue;

        const float closeness = juce::jlimit (0.0f, 1.0f, 1.0f - s.z);
        const float sz = s.size * (0.5f + closeness * 2.2f);
        const float twinkle = 0.82f + 0.18f * std::sin (phase * 2.3f + s.warmth * 12.0f);
        const auto starCol = colours::foam.interpolatedWith (s.warmth > 0.5f ? warm : cool, 0.55f);

        // streak: from a slightly deeper z toward current position
        const float zBehind = s.z + 0.045f + energy * 0.05f;
        const float bx = cx + (s.x / zBehind) * focal * 0.5f;
        const float by = cy + (s.y / zBehind) * focal * 0.5f;

        g.setColour (starCol.withAlpha ((0.10f + closeness * 0.30f) * twinkle));
        g.drawLine (bx, by, px, py, sz * 0.6f);
        if (closeness > 0.55f)   // near stars get a soft bloom
            ui::drawBloom (g, { px, py }, sz * 3.0f, starCol, 0.35f * closeness * twinkle);
        g.setColour (starCol.withAlpha ((0.25f + closeness * 0.55f) * twinkle));
        g.fillEllipse (px - sz * 0.5f, py - sz * 0.5f, sz, sz);
    }

    // slow rotating aurora arcs around the centre
    for (int arc = 0; arc < 3; ++arc)
    {
        const float radius = 130.0f + arc * 60.0f + std::sin (phase * 0.4f + arc) * 14.0f;
        const float rot = phase * (0.08f + arc * 0.03f) * (arc % 2 == 0 ? 1.0f : -1.0f);
        juce::Path p;
        p.addCentredArc (cx, cy, radius, radius * 0.6f, rot, 0.4f, 4.6f, true);
        g.setColour (colours::sea.withAlpha (0.08f + energy * 0.20f - arc * 0.02f));
        g.strokePath (p, juce::PathStrokeType (2.0f + energy * 3.0f));
    }

    // breathing bloom at the centre of the fall
    const float glowR = 190.0f + energy * 120.0f + std::sin (phase * 0.9f) * 18.0f;
    ui::drawBloom (g, { cx, cy }, glowR, colours::seaBright, 0.22f + energy * 0.35f);
}

//==============================================================================
NebulaLookAndFeel::NebulaLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, colours::foam);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::textDim);
    setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::textColourOffId, colours::textDim);
    setColour (juce::TextButton::textColourOnId, colours::seaBright);
}

void NebulaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                          float pos, float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (8.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    // outer bloom — the drama
    ui::drawBloom (g, centre, radius * 2.1f, colours::seaBright, 0.45f);

    // background track arc
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius + 6.0f, radius + 6.0f, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::seaBright.withAlpha (0.12f));
    g.strokePath (track, juce::PathStrokeType (3.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // body
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d3a55), centre.x - radius * 0.4f, centre.y - radius * 0.5f,
                                             juce::Colour (0xff071c2c), centre.x, centre.y + radius, true));
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2, radius * 2);
    g.setColour (colours::sea.withAlpha (0.5f));
    g.drawEllipse (centre.x - radius, centre.y - radius, radius * 2, radius * 2, 1.4f);

    // value arc — thick, glowing
    juce::Path arc;
    arc.addCentredArc (centre.x, centre.y, radius + 6.0f, radius + 6.0f, 0.0f, startAngle, angle, true);
    g.setColour (colours::sea.withAlpha (0.5f));
    g.strokePath (arc, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::seaBright);
    g.strokePath (arc, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // pointer
    juce::Path pointer;
    pointer.addRoundedRectangle (-1.8f, -radius + 4.0f, 3.6f, radius * 0.45f, 1.8f);
    g.setColour (colours::seaBright);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));

    // live value readout in the knob centre
    juce::String text;
    const double v = s.getValue();
    if (s.getName() == "pan")
        text = std::abs (v) < 0.02 ? "C" : (v < 0 ? "L" + juce::String ((int) std::round (-v * 100))
                                                  : "R" + juce::String ((int) std::round (v * 100)));
    else
        text = juce::String ((int) std::round (v * 100));
    g.setColour (colours::foam);
    g.setFont (juce::Font (juce::FontOptions (radius * 0.42f)));
    g.drawText (text, bounds, juce::Justification::centred);
}

void NebulaLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                          float pos, float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float trackY = (float) y + h * 0.5f - 3.0f;
    auto track = juce::Rectangle<float> ((float) x, trackY, (float) w, 6.0f);
    g.setColour (colours::seaBright.withAlpha (0.12f));
    g.fillRoundedRectangle (track, 3.0f);

    auto fill = track.withWidth (juce::jmax (0.0f, pos - (float) x));
    g.setColour (colours::sea);
    g.fillRoundedRectangle (fill, 3.0f);

    g.setColour (colours::foam);
    g.fillEllipse (pos - 7.0f, trackY - 4.0f, 14.0f, 14.0f);
    g.setColour (colours::seaBright.withAlpha (0.5f));
    g.drawEllipse (pos - 7.0f, trackY - 4.0f, 14.0f, 14.0f, 1.0f);
}

//==============================================================================
void PadButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto b = getLocalBounds().toFloat().reduced (3.0f);
    const float corner = 14.0f;

    juce::Colour top = isActivePad ? tint.withBrightness (0.55f).withAlpha (0.85f)
                                   : tint.withBrightness (0.24f).withSaturation (0.6f).withAlpha (0.85f);
    juce::Colour bottom = isActivePad ? tint.withBrightness (0.32f).withAlpha (0.9f)
                                      : juce::Colour (0xff03121e).withAlpha (0.9f);
    if (down) { top = top.brighter (0.2f); bottom = bottom.brighter (0.2f); }

    g.setGradientFill (juce::ColourGradient (top, b.getX(), b.getY(), bottom, b.getX(), b.getBottom(), false));
    g.fillRoundedRectangle (b, corner);

    if (isActivePad)
    {
        const float breathe = 0.5f + 0.5f * std::sin (glowPhase);
        // glow bleeds out past the edges like real light; the outline re-draws on top
        ui::drawBloom (g, b.getCentre(), b.getWidth() * (0.78f + breathe * 0.08f), tint, 0.45f + breathe * 0.25f);
        g.setGradientFill (juce::ColourGradient (top, b.getX(), b.getY(), bottom, b.getX(), b.getBottom(), false));
        g.fillRoundedRectangle (b, corner);
        g.setColour (tint.withAlpha (0.45f + breathe * 0.35f));
        g.drawRoundedRectangle (b, corner, 2.2f);
    }
    else
    {
        g.setColour (tint.withAlpha (highlighted ? 0.6f : 0.22f));
        g.drawRoundedRectangle (b, corner, 1.2f);
    }

    g.setColour (colours::foam);
    g.setFont (juce::Font (juce::FontOptions (13.0f)).withExtraKerningFactor (0.08f));
    g.drawFittedText (getButtonText(), getLocalBounds().reduced (8), juce::Justification::centred, 3);
}

//==============================================================================
juce::Point<float> KeyPlanets::planetCentre (int i) const
{
    auto b = getLocalBounds().toFloat();
    // planets spread along a shallow elliptical orbit, each wobbling on its own
    const float t = (float) i / 12.0f;
    const float baseX = b.getX() + b.getWidth() * (0.06f + 0.88f * t);
    const float arcY  = b.getCentreY() + std::sin (t * juce::MathConstants<float>::pi) * -b.getHeight() * 0.18f;
    const float wobX = std::sin (phase * 0.5f + wobblePhase[i]) * 6.0f;
    const float wobY = std::cos (phase * 0.7f + wobblePhase[i] * 1.3f) * 8.0f;
    return { baseX + wobX, arcY + wobY };
}

void KeyPlanets::paint (juce::Graphics& g)
{
    const int pad = processor.getCurrentPadIndex();
    const int currentKey = processor.getCurrentKey();
    const auto& presets = processor.getPresets();
    const bool havePad = pad >= 0 && pad < presets.size();

    // faint orbit line
    {
        juce::Path orbit;
        bool first = true;
        for (int i = 0; i < 48; ++i)
        {
            const float t = (float) i / 47.0f;
            auto b = getLocalBounds().toFloat();
            const float x = b.getX() + b.getWidth() * (0.06f + 0.88f * t);
            const float y = b.getCentreY() + std::sin (t * juce::MathConstants<float>::pi) * -b.getHeight() * 0.18f;
            if (first) { orbit.startNewSubPath (x, y); first = false; }
            else       orbit.lineTo (x, y);
        }
        g.setColour (colours::sea.withAlpha (0.10f));
        g.strokePath (orbit, juce::PathStrokeType (1.0f));
    }

    for (int i = 0; i < 12; ++i)
    {
        const auto c = planetCentre (i);
        const bool available = ! havePad || presets[pad].hasKey (i);
        const bool selected = havePad && i == currentKey;
        const bool hover = i == hovered;

        const float pulse = 0.5f + 0.5f * std::sin (phase * 1.2f + wobblePhase[i]);
        float r = selected ? 16.0f + pulse * 2.5f : (hover ? 14.0f : 11.0f);
        if (! available) r = 9.0f;

        // each planet gets a subtle personal tint around the sea palette
        auto planetColour = selected
            ? colours::seaBright
            : colours::sea.withRotatedHue ((hue[i] - 0.5f) * 0.16f).withAlpha (available ? 0.9f : 0.25f);

        if (available)   // every live planet has a faint atmosphere; selected burns
            ui::drawBloom (g, c, r * (selected ? 3.4f : hover ? 2.6f : 1.9f), planetColour,
                           selected ? 0.75f : hover ? 0.45f : 0.22f);

        g.setGradientFill (juce::ColourGradient (planetColour.brighter (0.4f), c.x - r * 0.35f, c.y - r * 0.4f,
                                                 planetColour.darker (0.8f), c.x + r * 0.6f, c.y + r * 0.7f, true));
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

        // a thin ring on every 4th planet, saturn-style
        if (i % 4 == 1 && available)
        {
            g.setColour (planetColour.withAlpha (0.5f));
            juce::Path ring;
            ring.addEllipse (c.x - r * 1.55f, c.y - r * 0.45f, r * 3.1f, r * 0.9f);
            g.strokePath (ring, juce::PathStrokeType (1.2f),
                          juce::AffineTransform::rotation (-0.45f, c.x, c.y));
        }

        g.setColour (available ? colours::foam : colours::textDim.withAlpha (0.5f));
        g.setFont (juce::Font (juce::FontOptions (selected ? 13.0f : 11.0f)));
        g.drawText (keynames::display[i],
                    juce::Rectangle<float> (c.x - 20, c.y + r + 2, 40, 14),
                    juce::Justification::centred);
    }
}

void KeyPlanets::mouseDown (const juce::MouseEvent& e)
{
    for (int i = 0; i < 12; ++i)
        if (planetCentre (i).getDistanceFrom (e.position) < 20.0f)
        {
            if (e.mods.isPopupMenu())      // right-click a planet → learn that key
            {
                const int action = 14 + i;
                juce::PopupMenu m;
                m.addItem ("MIDI Learn: " + juce::String (NebulaTideProcessor::midiActionName (action)),
                           [this, action] { processor.startLearn (action); });
                const auto bound = processor.bindingText (action);
                if (bound != "-")
                    m.addItem ("Clear binding (" + bound + ")",
                               [this, action] { processor.clearBinding (action); });
                m.showMenuAsync ({});
            }
            else
            {
                processor.selectKey (i);
            }
            return;
        }
}

void KeyPlanets::mouseMove (const juce::MouseEvent& e)
{
    hovered = -1;
    for (int i = 0; i < 12; ++i)
        if (planetCentre (i).getDistanceFrom (e.position) < 20.0f)
            { hovered = i; break; }
}

//==============================================================================
void StarPlayer::timerCallback()
{
    phase += 0.05f;
    repaint();
}

void StarPlayer::resized()
{
    auto b = getLocalBounds();
    auto bottom = b.removeFromBottom (24);
    prev.setBounds (bottom.removeFromLeft (26));
    next.setBounds (bottom.removeFromRight (26));
    loopBtn.setBounds (bottom.reduced (6, 0));
    b.removeFromBottom (18);   // name row (painted)
    starArea = b.toFloat();
}

// The star doubles as its own volume knob: drag vertically to set level
// (shown as the arc around the star); a plain click plays / stops.
void StarPlayer::mouseDown (const juce::MouseEvent& e)
{
    dragging = false;
    if (starArea.contains (e.position))
        dragStartVolume = processor.getAuxVolume (cat);
}

void StarPlayer::mouseDrag (const juce::MouseEvent& e)
{
    if (! starArea.contains (e.getMouseDownPosition().toFloat()))
        return;
    if (e.getDistanceFromDragStart() > 4)
        dragging = true;
    if (dragging)
        processor.setAuxVolume (cat, dragStartVolume - (float) e.getDistanceFromDragStartY() / 150.0f);
}

void StarPlayer::mouseUp (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())          // right-click = MIDI learn menu, not play
        { dragging = false; return; }
    if (dragging || ! starArea.contains (e.position))
        { dragging = false; return; }
    if (processor.isAuxPlaying (cat))
        processor.stopAux (cat);
    else
        processor.triggerAux (cat, processor.getAuxIndex (cat));
}

void StarPlayer::paint (juce::Graphics& g)
{
    const auto& sounds = processor.getAuxSounds (cat);
    const bool playing = processor.isAuxPlaying (cat);
    const bool looping = processor.getAuxLoop (cat);
    const bool empty = sounds.isEmpty();

    const auto c = starArea.getCentre();
    const float pulse = 0.5f + 0.5f * std::sin (phase);
    const float base = juce::jmin (starArea.getWidth(), starArea.getHeight()) * 0.16f;
    const float r = base * (playing ? 1.1f + pulse * 0.18f : 1.0f + pulse * 0.06f);

    // everything (halo, rays) must fade out INSIDE the component bounds,
    // otherwise the glow gets clipped and shows as a hard square edge
    const float maxReach = juce::jmin (starArea.getWidth(), starArea.getHeight()) * 0.5f - 2.0f;

    auto col = empty ? colour.withSaturation (0.1f).withAlpha (0.35f) : colour;

    // bloom halo (breathes while playing)
    const float haloR = juce::jmin (r * 3.2f, maxReach);
    ui::drawBloom (g, c, haloR, col, playing ? 0.70f + pulse * 0.25f : 0.32f);

    // 4-point star rays
    juce::Path rays;
    for (int i = 0; i < 4; ++i)
    {
        const float a = phase * 0.15f + i * juce::MathConstants<float>::halfPi;
        const float len = juce::jmin (r * (2.1f + (playing ? pulse * 0.7f : 0.0f)), maxReach);
        juce::Path ray;
        ray.addTriangle (0.0f, -len, -r * 0.16f, 0.0f, r * 0.16f, 0.0f);
        rays.addPath (ray, juce::AffineTransform::rotation (a).translated (c));
    }
    g.setColour (col.withAlpha (0.55f));
    g.fillPath (rays);

    // core
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.95f), c.x, c.y,
                                             col, c.x + r, c.y, true));
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

    // sound name
    const juce::String label = empty ? "empty"
                                     : sounds[juce::jlimit (0, sounds.size() - 1, processor.getAuxIndex (cat))].name;
    g.setColour (colours::foam.withAlpha (empty ? 0.4f : 1.0f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.15f));
    g.drawText (label.toUpperCase(),
                juce::Rectangle<float> (0.0f, starArea.getBottom(), (float) getWidth(), 16.0f),
                juce::Justification::centred);

    // volume arc: wraps the star from 7 o'clock around to 5 o'clock. Drag the
    // star vertically to change it.
    {
        const float vol = processor.getAuxVolume (cat);
        const float arcR = juce::jmin (r * 1.75f, maxReach - 1.0f);
        const float a0 = juce::MathConstants<float>::pi * 0.75f;
        const float a1 = juce::MathConstants<float>::pi * 2.25f;
        juce::Path track, fill;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
        g.setColour (col.withAlpha (0.18f));
        g.strokePath (track, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        if (vol > 0.005f)
        {
            fill.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a0 + (a1 - a0) * vol, true);
            g.setColour (col.withAlpha (0.85f));
            g.strokePath (fill, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    // loop state ring around the core
    if (looping)
    {
        g.setColour (col.withAlpha (0.9f));
        g.drawEllipse (c.x - r * 1.45f, c.y - r * 1.45f, r * 2.9f, r * 2.9f, 1.6f);
    }

    loopBtn.setColour (juce::TextButton::textColourOffId,
                       looping ? colours::seaBright : colours::textDim);
}

//==============================================================================
NebulaTideEditor::NebulaTideEditor (NebulaTideProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lnf);
#if JUCE_WINDOWS || JUCE_MAC || JUCE_LINUX
    openGL.setContinuousRepainting (false);
    openGL.attachTo (*this);
#endif
    addAndMakeVisible (background);
    addAndMakeVisible (keyPlanets);
    addChildComponent (zoneKeyboard);            // hidden by default; KEYS button toggles
    zoneKeyboard.setVisible (processor.showKeyboard.load());
    updateBtn.setColour (juce::TextButton::textColourOffId, colours::seaBright);
    updateBtn.onClick = [this]
    {
        if (updatePageUrl.isNotEmpty()) juce::URL (updatePageUrl).launchInDefaultBrowser();
    };
    addChildComponent (updateBtn);
   #if ! (JUCE_IOS || JUCE_ANDROID)
    // Desktop only. Store builds are updated by the store, and an in-app
    // button sending a phone user to a website for a new version is exactly
    // what App Review turns down.
    checkForUpdate();
   #endif

    showMainViewIfLicensed();
    keysBtn.setColour (juce::TextButton::textColourOffId, colours::textDim);
    keysBtn.onClick = [this]
    {
        const bool show = ! zoneKeyboard.isVisible();
        zoneKeyboard.setVisible (show);
        processor.showKeyboard.store (show);
        resized();
    };
    addAndMakeVisible (keysBtn);
    addAndMakeVisible (fxStar);
    addAndMakeVisible (texStar);

    auto styleLabel = [this] (juce::Label& l, const juce::String& text, float size, juce::Colour c)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (size >= 14.0f ? ui::titleFont (size) : ui::labelFont (size));
        l.setColour (juce::Label::textColourId, c);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    styleLabel (title, "N E B U L A   T I D E", 19.0f, colours::foam);
    styleLabel (presetLabel, "- select a pad -", 14.0f, colours::foam);
    styleLabel (statusLabel, "drifting", 11.0f, colours::textDim);
    styleLabel (reverbTitle, "S P A C E", 10.0f, colours::textDim);

    for (auto* btn : { &prevBtn, &nextBtn })
    {
        btn->setColour (juce::TextButton::textColourOffId, colours::sea);
        addAndMakeVisible (*btn);
    }
    prevBtn.onClick = [this] { browse (-1); };
    nextBtn.onClick = [this] { browse (1); };

    auto smallLabel = [this] (juce::Label& l, const juce::String& name)
    {
        l.setText (name, juce::dontSendNotification);
        l.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.3f));
        l.setColour (juce::Label::textColourId, colours::textDim);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };

    auto setupKnob = [&] (juce::Slider& s, juce::Label& l, const juce::String& name, const juce::String& id)
    {
        s.setName (id);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (s);
        smallLabel (l, name);
    };
    auto setupSlider = [&] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        addAndMakeVisible (s);
        smallLabel (l, name);
    };

    setupKnob (volumeKnob, volumeLabel, "VOLUME", "volume");
    setupKnob (panKnob, panLabel, "PAN", "pan");
    setupKnob (shimSlider, shimLabel, "SHIMMER", "shim");
    setupSlider (fadeSlider, fadeLabel, "CROSSFADE");
    setupSlider (rMixSlider, rMixLabel, "MIX");
    setupSlider (rSizeSlider, rSizeLabel, "SIZE");
    setupSlider (rDampSlider, rDampLabel, "DAMP");

    for (auto* btn : { &roomBtn, &plateBtn, &hallBtn })
    {
        btn->setClickingTogglesState (false);
        addAndMakeVisible (*btn);
    }
    roomBtn.onClick  = [this] { processor.applyReverbPreset (ReverbType::room); };
    plateBtn.onClick = [this] { processor.applyReverbPreset (ReverbType::plate); };
    hallBtn.onClick  = [this] { processor.applyReverbPreset (ReverbType::hall); };

    volumeAtt = std::make_unique<Attachment> (processor.apvts, "volume", volumeKnob);
    panAtt    = std::make_unique<Attachment> (processor.apvts, "pan", panKnob);
    fadeAtt   = std::make_unique<Attachment> (processor.apvts, "fade", fadeSlider);
    rMixAtt   = std::make_unique<Attachment> (processor.apvts, "rmix", rMixSlider);
    rSizeAtt  = std::make_unique<Attachment> (processor.apvts, "rsize", rSizeSlider);
    rDampAtt  = std::make_unique<Attachment> (processor.apvts, "rdamp", rDampSlider);
    shimAtt   = std::make_unique<Attachment> (processor.apvts, "shim", shimSlider);

    // Dragging a clip only makes sense where there is a timeline to drop it on.
    addChildComponent (midiDrag);
    midiDrag.setVisible (! juce::JUCEApplicationBase::isStandaloneApp());

    settingsBtn.setColour (juce::TextButton::textColourOffId, colours::textDim);
    settingsBtn.onClick = [this] { settingsPanel.setVisible (! settingsPanel.isVisible()); };
    addAndMakeVisible (settingsBtn);

    // Forge, brought inside the app. Rebuilding the pad list afterwards is
    // what makes a newly saved preset appear without a restart.
    studio = std::make_unique<PresetStudio> (processor, [this]
    {
        rebuildPads();
        resized();
    });
    addChildComponent (*studio);
    studio->setVisible (false);      // opens only from the STUDIO button
    studioBtn.setColour (juce::TextButton::textColourOffId, colours::textDim);
    studioBtn.onClick = [this] { studio->setVisible (! studio->isVisible()); };
    addAndMakeVisible (studioBtn);
    addChildComponent (settingsPanel);   // hidden until toggled

    // one settings entry point: fold the standalone's stock "Options" button
    // into our panel — device selection lives under AUDIO / MIDI DEVICES...
    if (juce::JUCEApplicationBase::isStandaloneApp())
    {
        settingsPanel.devicesBtn.onClick = []
        {
            if (auto* holder = juce::StandalonePluginHolder::getInstance())
                holder->showAudioSettingsDialog();
        };
        juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<NebulaTideEditor> (this)]
        {
            if (safeThis == nullptr) return;
            if (auto* top = safeThis->getTopLevelComponent())
                for (int i = 0; i < top->getNumChildComponents(); ++i)
                    if (auto* btn = dynamic_cast<juce::Button*> (top->getChildComponent (i)))
                        if (btn->getButtonText() == "Options")
                            btn->setVisible (false);
        });
    }
    else
    {
        settingsPanel.devicesBtn.setVisible (false);   // hosts own the devices
    }

    // right-click MIDI learn on the controls themselves
    auto attachLearn = [this] (juce::Component& c, int action)
    {
        auto* l = learnListeners.add (new MidiLearnListener (processor,
            [action] { return juce::Array<int> { action }; }));
        c.addMouseListener (l, true);
    };
    attachLearn (volumeKnob, 0);
    attachLearn (panKnob, 1);
    attachLearn (rMixSlider, 2);
    attachLearn (rSizeSlider, 3);
    attachLearn (rDampSlider, 4);
    attachLearn (shimSlider, 34);
    attachLearn (fadeSlider, 5);
    attachLearn (fxStar, 6);
    attachLearn (texStar, 7);
    attachLearn (nextBtn, 9);      // next preset
    attachLearn (prevBtn, 10);     // previous preset

    // Nothing of the instrument is reachable until a proof has verified.
    // Built at construction, so the gate is up before the first paint.
   #if NEBULA_REQUIRE_LICENSE
    if (! processor.license.isLicensed())
    {
        activation = std::make_unique<ActivationView> (processor.license,
                                                       [this] { showMainViewIfLicensed(); });
        addAndMakeVisible (*activation);
        activation->toFront (true);
    }
   #endif

    rebuildPads();

    // no sounds on this machine yet → offer the one-time library download
    if (! processor.hasLibrary())
    {
        auto* self = this;
        downloader = std::make_unique<LibraryDownloader> (processor, [self]
        {
            self->rebuildPads();
            self->resized();
            juce::Component::SafePointer<NebulaTideEditor> safe (self);
            juce::MessageManager::callAsync ([safe]
            {
                if (safe != nullptr) safe->downloader.reset();
            });
        });
        addAndMakeVisible (*downloader);
    }

    setWantsKeyboardFocus (true);   // piano-key control: A W S E D F T G Y H U J
    setResizable (true, true);
   #if JUCE_IOS || JUCE_ANDROID
    // A phone in landscape is about 430 points tall. The desktop minimum of
    // 660 below stopped the editor shrinking to fit, so the bottom third of
    // the interface - keys, reverb, volume and pan - hung off the screen.
    setResizeLimits (320, 320, 4096, 4096);
   #else
    setResizeLimits (900, 660, 1920, 1200);
   #endif
    setSize (1100, 780);
    startTimerHz (60);
}

NebulaTideEditor::~NebulaTideEditor()
{
#if JUCE_WINDOWS || JUCE_MAC || JUCE_LINUX
    openGL.detach();
#endif
    setLookAndFeel (nullptr);
}

void NebulaTideEditor::browse (int dir)
{
    const int n = processor.getPresets().size();
    if (n == 0) return;
    viewIndex = (viewIndex + dir + n) % n;
    if (processor.getCurrentPadIndex() >= 0)
        processor.selectPad (viewIndex);    // playing → crossfade into the next preset
    resized();
    repaint();
}

void NebulaTideEditor::rebuildPads()
{
    pads.clear();
    auto& presets = processor.getPresets();
    for (int i = 0; i < presets.size(); ++i)
    {
        auto* pad = pads.add (new PadButton());
        pad->setButtonText (presets[i].name.toUpperCase());
        pad->tint = presets[i].colour;
        pad->onClick = [this, i]
        {
            if (processor.getCurrentPadIndex() == i)
                processor.stopAll();
            else
                processor.selectPad (i);
        };
        // right-click: learn pad play/stop + direct-select for this preset slot
        auto* l = learnListeners.add (new MidiLearnListener (processor, [i]
        {
            juce::Array<int> actions { 13 };
            if (i < 8) actions.add (26 + i);
            return actions;
        }));
        pad->addMouseListener (l, true);
        addChildComponent (pad);    // visibility managed per-frame: one at a time
    }
}

void NebulaTideEditor::updateReverbButtons()
{
    const int type = (int) processor.apvts.getRawParameterValue ("rtype")->load();
    auto colour = [&] (juce::TextButton& b, bool on)
    {
        b.setColour (juce::TextButton::textColourOffId, on ? colours::seaBright : colours::textDim);
    };
    colour (roomBtn, type == 0);
    colour (plateBtn, type == 1);
    colour (hallBtn, type == 2);
}

void NebulaTideEditor::updatePadStates()
{
    const int current = processor.getCurrentPadIndex();
    auto& presets = processor.getPresets();

    // playing pad drives the view; the whole UI retints to the viewed preset
    if (current >= 0)
        viewIndex = current;
    if (! presets.isEmpty())
    {
        viewIndex = juce::jlimit (0, presets.size() - 1, viewIndex);
        colours::setTheme (presets.getReference (viewIndex).colour);
    }

    for (int i = 0; i < pads.size(); ++i)
    {
        pads[i]->setVisible (i == viewIndex);
        const bool active = (i == current);
        if (pads[i]->isActivePad != active)
            pads[i]->isActivePad = active;
        if (active)
            pads[i]->glowPhase += 0.09f;
        pads[i]->repaint();
    }

    juce::String name = "- no pads -";
    if (! presets.isEmpty())
    {
        name = presets.getReference (viewIndex).name.toUpperCase();
        if (current == viewIndex && current >= 0)
            name += juce::String::fromUTF8 ("   \xc2\xb7   ") + keynames::display[processor.getCurrentKey()];
    }
    presetLabel.setText (name, juce::dontSendNotification);
    statusLabel.setText (juce::String (current >= 0 ? "transmitting" : "drifting")
                             + (processor.usingTestLibrary ? "  [TEST LIB]" : ""),
                         juce::dontSendNotification);
    statusLabel.setColour (juce::Label::textColourId,
                           current >= 0 ? colours::seaBright : colours::textDim);
}

//==============================================================================
int ZoneKeyboard::whiteIndex (int n)
{
    int count = 0;
    for (int i = firstNote; i < n; ++i)
        if (! isBlack (i)) ++count;
    return count;
}

juce::Rectangle<float> ZoneKeyboard::whiteKeyRect (int note) const
{
    const int totalWhite = whiteIndex (lastNote + 1);
    const float kw = (float) getWidth() / (float) totalWhite;
    const float top = 18.0f;
    return { whiteIndex (note) * kw, top, kw, (float) getHeight() - top };
}

juce::Rectangle<float> ZoneKeyboard::blackKeyRect (int note) const
{
    const auto prevWhite = whiteKeyRect (note - 1);
    const float bw = prevWhite.getWidth() * 0.62f;
    return { prevWhite.getRight() - bw * 0.5f, prevWhite.getY(), bw, prevWhite.getHeight() * 0.6f };
}

int ZoneKeyboard::noteAt (juce::Point<float> p) const
{
    for (int n = firstNote; n <= lastNote; ++n)          // black keys sit on top
        if (isBlack (n) && blackKeyRect (n).contains (p)) return n;
    for (int n = firstNote; n <= lastNote; ++n)
        if (! isBlack (n) && whiteKeyRect (n).contains (p)) return n;
    return -1;
}

void ZoneKeyboard::paint (juce::Graphics& g)
{
    const juce::Colour fxCol (0xffffc96b), texCol (0xffff5a6e);
    auto zoneColour = [&] (int note) -> juce::Colour
    {
        switch (NebulaTideProcessor::zoneOf (note))
        {
            case 0: return fxCol;
            case 1: return colours::seaBright;
            case 2: return texCol;
            default: return juce::Colours::transparentBlack;
        }
    };

    // zone label band
    struct Z { int lo, hi; const char* name; juce::Colour c; };
    const Z zones[3] = {
        { NebulaTideProcessor::fxZoneLo,  NebulaTideProcessor::fxZoneHi,  "FX",       fxCol },
        { NebulaTideProcessor::keyZoneLo, NebulaTideProcessor::keyZoneHi, "KEYS",     colours::seaBright },
        { NebulaTideProcessor::texZoneLo, NebulaTideProcessor::texZoneHi, "TEXTURES", texCol } };
    for (auto& z : zones)
    {
        const float x0 = whiteKeyRect (z.lo).getX();
        const float x1 = whiteKeyRect (isBlack (z.hi) ? z.hi - 1 : z.hi).getRight();
        g.setColour (z.c.withAlpha (0.22f));
        g.fillRoundedRectangle (x0, 1.0f, x1 - x0, 14.0f, 4.0f);
        g.setColour (z.c);
        g.setFont (juce::Font (juce::FontOptions (9.5f)).withExtraKerningFactor (0.25f));
        g.drawText (z.name, juce::Rectangle<float> (x0, 0.0f, x1 - x0, 16.0f), juce::Justification::centred);
    }

    // white keys
    for (int n = firstNote; n <= lastNote; ++n)
    {
        if (isBlack (n)) continue;
        auto r = whiteKeyRect (n).reduced (0.6f, 0.0f);
        const auto zc = zoneColour (n);
        const bool held = processor.heldKeys[n].load() || n == mouseNote;
        juce::Colour fill = zc.isTransparent() ? juce::Colour (0xffdbe4ea)
                                               : zc.interpolatedWith (juce::Colours::white, 0.55f);
        if (held) fill = zc.isTransparent() ? colours::seaBright : zc;
        g.setColour (fill);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (juce::Colour (0xff08141c).withAlpha (0.8f));
        g.drawRoundedRectangle (r, 2.0f, 0.8f);
        if (n % 12 == 0)   // octave label on each C
        {
            g.setColour (juce::Colour (0xff08141c).withAlpha (0.7f));
            g.setFont (juce::Font (juce::FontOptions (8.5f)));
            g.drawText ("C" + juce::String (n / 12 - 1), r.removeFromBottom (12.0f), juce::Justification::centred);
        }
    }
    // black keys
    for (int n = firstNote; n <= lastNote; ++n)
    {
        if (! isBlack (n)) continue;
        auto r = blackKeyRect (n);
        const auto zc = zoneColour (n);
        const bool held = processor.heldKeys[n].load() || n == mouseNote;
        juce::Colour fill = zc.isTransparent() ? juce::Colour (0xff141c24)
                                               : zc.interpolatedWith (juce::Colour (0xff141c24), 0.55f);
        if (held) fill = zc.isTransparent() ? colours::seaBright : zc.brighter (0.2f);
        g.setColour (fill);
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (juce::Colour (0xff02080e));
        g.drawRoundedRectangle (r, 2.0f, 0.8f);
    }
}

void ZoneKeyboard::mouseDown (const juce::MouseEvent& e)
{
    mouseNote = noteAt (e.position);
    if (mouseNote < 0) return;
    const int n = mouseNote;
    switch (NebulaTideProcessor::zoneOf (n))
    {
        case 1: processor.keyCommand (n % 12); break;
        case 0: { const int i = n - NebulaTideProcessor::fxZoneLo;
                  if (i < processor.getAuxSounds (0).size())
                  { if (processor.isAuxPlaying (0) && processor.getAuxIndex (0) == i) processor.stopAux (0);
                    else processor.triggerAux (0, i); } break; }
        case 2: { const int i = n - NebulaTideProcessor::texZoneLo;
                  if (i < processor.getAuxSounds (1).size())
                  { if (processor.isAuxPlaying (1) && processor.getAuxIndex (1) == i) processor.stopAux (1);
                    else processor.triggerAux (1, i); } break; }
        default: break;
    }
}

void ZoneKeyboard::mouseUp (const juce::MouseEvent&)
{
    mouseNote = -1;
}

//==============================================================================
LibraryDownloader::LibraryDownloader (NebulaTideProcessor& p, std::function<void()> ready)
    : processor (p), onReady (std::move (ready))
{
   #if JUCE_ANDROID
    // first launch: unpack the library that ships inside the APK
    state.store (2);
    pool.addJob ([this]
    {
        const bool ok = processor.installBundledLibrary ([this] (double f) { fraction.store (f); });
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<LibraryDownloader> (this), ok]
        {
            if (safe == nullptr) return;
            if (ok)
            {
                safe->processor.reloadLibrary();
                if (safe->onReady) safe->onReady();
            }
            else
            {
                safe->messageText = "The sound library could not be installed.\nPlease reinstall Nebula Tide.";
                safe->state.store (3);
            }
        });
    });
   #else
    messageText = "Sound library not found.\nPlease reinstall Nebula Tide so the sounds are installed with the app.";
    diagnosticText = NebulaTideProcessor::librarySearchReport();
    state.store (3);
   #endif
    startTimerHz (10);
}

LibraryDownloader::~LibraryDownloader()
{
    pool.removeAllJobs (true, 4000);
}

void LibraryDownloader::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgDeep.withAlpha (0.94f));
    auto box = getLocalBounds().withSizeKeepingCentre (juce::jmin (620, getWidth() - 40), 300).toFloat();
    g.setColour (juce::Colour (0xff031420));
    g.fillRoundedRectangle (box, 18.0f);
    g.setColour (colours::seaBright.withAlpha (0.3f));
    g.drawRoundedRectangle (box, 18.0f, 1.2f);

    g.setColour (colours::foam);
    g.setFont (ui::titleFont (15.0f));
    g.drawText ("S O U N D   L I B R A R Y", box.removeFromTop (56), juce::Justification::centred);

    g.setFont (ui::bodyFont (12.5f));
    g.setColour (colours::textDim);
    const juce::String msg = state.load() == 2
        ? "Installing the sound library...  " + juce::String ((int) (fraction.load() * 100)) + "%"
        : messageText;
    g.drawFittedText (msg, box.reduced (30, 0).removeFromTop (70).toNearestInt(), juce::Justification::centred, 3);

    if (state.load() == 2)
    {
        auto bar = box.reduced (40, 0).withHeight (8.0f).withY (box.getY() + 120);
        g.setColour (colours::seaBright.withAlpha (0.15f));
        g.fillRoundedRectangle (bar, 4.0f);
        g.setColour (colours::seaBright);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) fraction.load()), 4.0f);
    }
    else if (diagnosticText.isNotEmpty())
    {
        // support info: exactly where the app looked
        g.setFont (ui::bodyFont (9.5f));
        g.setColour (colours::textDim.withAlpha (0.65f));
        g.drawFittedText (diagnosticText, box.reduced (24, 0).withTrimmedTop (60).toNearestInt(),
                          juce::Justification::centredTop, 6);
    }
}

//==============================================================================
void SettingsPanel::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff031420).withAlpha (0.96f));
    g.fillRoundedRectangle (b, 18.0f);
    g.setColour (colours::seaBright.withAlpha (0.25f));
    g.drawRoundedRectangle (b.reduced (0.5f), 18.0f, 1.2f);

    g.setColour (colours::foam);
    g.setFont (ui::titleFont (15.0f));
    g.drawText ("S E T T I N G S", getLocalBounds().removeFromTop (44), juce::Justification::centred);

    auto info = getLocalBounds().reduced (26, 0).removeFromTop (118).withTrimmedTop (46);
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.setColour (colours::textDim);
    g.drawFittedText (
        "MIDI ZONES  -  FX C2-B2 (one note per sound)  |  KEYS C3-B4 (pitch = key)  |  TEXTURES C5-B5 (one note per sound)\n"
        "KEYBOARD  -  A W S E D F T G Y H U J = C..B   |   SPACE play/stop   |   \x3c \x3e presets   |   1 FX   |   2 texture\n"
        "MIDI LEARN  -  click LEARN, then move a knob or press a pad on your controller.",
        info, juce::Justification::topLeft, 4);
}

void SettingsPanel::refreshLibraryPath()
{
    const auto lib = NebulaTideProcessor::currentLibraryFile();
    const auto pick = NebulaTideProcessor::userChosenLibraryDir();

    if (lib != juce::File())
        soundsPath.setText (lib.getFullPathName()
                              + (pick != juce::File() ? "   (chosen)" : "   (default)"),
                            juce::dontSendNotification);
    else
        soundsPath.setText ("No sound library found - choose the folder containing NebulaTide.ntlib",
                            juce::dontSendNotification);

    soundsPath.setColour (juce::Label::textColourId,
                          lib != juce::File() ? colours::textDim : juce::Colour (0xffff5a6e));
    resetLocationBtn.setEnabled (pick != juce::File());
}

void SettingsPanel::chooseLibraryFolder()
{
    folderChooser = std::make_unique<juce::FileChooser> (
        "Where is your Nebula Tide sound library?",
        NebulaTideProcessor::defaultLibraryDir(), juce::String());

    folderChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectDirectories,
        [this] (const juce::FileChooser& fc)
        {
            const auto dir = fc.getResult();
            if (! dir.isDirectory()) return;

            // Say so plainly rather than accepting a folder and failing later
            // with the same blank screen this setting exists to cure.
            if (dir.findChildFiles (juce::File::findFiles, false, "*.ntlib").isEmpty())
            {
                soundsPath.setText ("No .ntlib file in " + dir.getFullPathName(),
                                    juce::dontSendNotification);
                soundsPath.setColour (juce::Label::textColourId, juce::Colour (0xffff5a6e));
                return;
            }

            NebulaTideProcessor::setUserChosenLibraryDir (dir);
            processor.reloadLibrary();
            refreshLibraryPath();
        });
}

void SettingsPanel::resized()
{
    auto area = getLocalBounds().reduced (26, 12);
    area.removeFromTop (124);
    if (devicesBtn.isVisible())
        devicesBtn.setBounds (area.removeFromBottom (36).withSizeKeepingCentre (240, 30));
    gateBtn.setBounds (area.removeFromTop (26));
    auto blendRow = area.removeFromTop (26);
    blendLabel.setBounds (blendRow.removeFromLeft (96));
    blendSlider.setBounds (blendRow.reduced (4, 2));

    // v2 block: shimmer shaping, then chord-follow settle time
    auto labelledRow = [&area] (juce::Label& l, juce::Component& c)
    {
        auto r = area.removeFromTop (24);
        l.setBounds (r.removeFromLeft (96));
        c.setBounds (r.reduced (4, 2));
    };
    area.removeFromTop (6);
    shimHeading.setBounds (area.removeFromTop (16));
    labelledRow (bloomLabel, bloomSlider);
    labelledRow (toneLabel,  toneSlider);
    labelledRow (sizeLabel,  sizeSlider);
    labelledRow (densityLabel, densitySlider);
    labelledRow (pitchLabel, pitchBox);
    area.removeFromTop (10);
   #if NEBULA_REQUIRE_LICENSE
    licenseHeading.setBounds (area.removeFromTop (16));
    licenseStatus.setBounds (area.removeFromTop (18));
    deactivateBtn.setBounds (area.removeFromTop (26).removeFromLeft (220).reduced (0, 2));
   #endif
    area.removeFromTop (10);
    soundsHeading.setBounds (area.removeFromTop (16));
    soundsPath.setBounds (area.removeFromTop (18));
    {
        auto r = area.removeFromTop (26);
        locateBtn.setBounds (r.removeFromLeft (150).reduced (0, 2));
        r.removeFromLeft (8);
        resetLocationBtn.setBounds (r.removeFromLeft (120).reduced (0, 2));
        r.removeFromLeft (8);
        aboutBtn.setBounds (r.removeFromLeft (90).reduced (0, 2));
    }
    manualBtn.setBounds (area.removeFromTop (24));
    area.removeFromTop (4);

    aboutView.setBounds (getLocalBounds().reduced (18, 14));

    viewport.setBounds (area);
    const int rowH = 28;
    rowsHolder.setSize (area.getWidth() - 12, rows.size() * rowH);
    auto inner = rowsHolder.getLocalBounds();
    for (auto* row : rows)
    {
        auto r = inner.removeFromTop (rowH);
        row->clear.setBounds (r.removeFromRight (30).reduced (2));
        row->learn.setBounds (r.removeFromRight (74).reduced (2));
        row->bind.setBounds (r.removeFromRight (110));
        row->name.setBounds (r);
    }
}

void SettingsPanel::timerCallback()
{
    licenseStatus.setText (processor.license.isLicensed()
                             ? "Licensed on this device."
                             : "Not activated.", juce::dontSendNotification);
    deactivateBtn.setEnabled (processor.license.isLicensed());

    const int learning = processor.learningAction();
    for (int i = 0; i < rows.size(); ++i)
    {
        auto* row = rows[i];
        row->bind.setText (processor.bindingText (i), juce::dontSendNotification);
        row->bind.setColour (juce::Label::textColourId,
                             processor.bindingText (i) == "-" ? colours::textDim : colours::seaBright);
        const bool isLearning = (learning == i);
        row->learn.setButtonText (isLearning ? "WAITING..." : "LEARN");
        row->learn.setColour (juce::TextButton::textColourOffId,
                              isLearning ? colours::seaBright : colours::textDim);
    }
}

//==============================================================================
// Beta builds stop working after their expiry date (compile-time constant).
static bool betaExpired()
{
    const juce::String expiry (NEBULA_BETA_EXPIRY);
    if (expiry.length() != 10) return false;
    const juce::Time exp (expiry.substring (0, 4).getIntValue(),
                          expiry.substring (5, 7).getIntValue() - 1,
                          expiry.substring (8, 10).getIntValue(), 0, 0);
    return juce::Time::getCurrentTime() > exp;
}

// Fetches the small version file from the website in the background. Any
// failure (offline, DNS, 404) is silent — the app never depends on it.
void NebulaTideEditor::checkForUpdate()
{
    juce::Component::SafePointer<NebulaTideEditor> safe (this);
    updatePool.addJob ([safe]
    {
        const juce::URL url (NebulaTideProcessor::updateUrl);
        const auto text = url.readEntireTextStream (false);
        if (text.isEmpty()) return;

        const auto json = juce::JSON::parse (text);
        const auto latest = json.getProperty ("version", "").toString().trim();
        const auto page   = json.getProperty ("page", "").toString().trim();
        if (latest.isEmpty()) return;

        // numeric compare: 1.10.0 is newer than 1.9.0
        auto parts = [] (const juce::String& v)
        {
            juce::Array<int> a;
            for (auto& s : juce::StringArray::fromTokens (v, ".", "")) a.add (s.getIntValue());
            while (a.size() < 3) a.add (0);
            return a;
        };
        const auto mine = parts (NEBULA_VERSION), theirs = parts (latest);
        bool newer = false;
        for (int i = 0; i < 3; ++i)
        {
            if (theirs[i] > mine[i]) { newer = true; break; }
            if (theirs[i] < mine[i]) break;
        }
        if (! newer) return;

        juce::MessageManager::callAsync ([safe, latest, page]
        {
            if (safe == nullptr) return;
            safe->updatePageUrl = page;
            safe->updateBtn.setButtonText ("UPDATE " + latest);
            safe->updateBtn.setVisible (true);
            safe->resized();
        });
    });
}

bool NebulaTideEditor::keyPressed (const juce::KeyPress& k)
{
    if (betaExpired()) return false;
    // piano row → the 12 keys
    static const juce::String pianoRow ("awsedftgyhuj");
    const auto ch = (juce::juce_wchar) juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) k.getTextCharacter());
    const int pc = pianoRow.indexOfChar (ch);
    if (pc >= 0)
    {
        processor.keyCommand (pc);
        return true;
    }

    if (k == juce::KeyPress::spaceKey)
    {
        if (processor.getCurrentPadIndex() >= 0) processor.stopAll();
        else                                     processor.selectPad (viewIndex);
        return true;
    }
    if (k == juce::KeyPress::leftKey)  { browse (-1); return true; }
    if (k == juce::KeyPress::rightKey) { browse (1);  return true; }
    if (ch == '1') { processor.toggleAux (0); return true; }
    if (ch == '2') { processor.toggleAux (1); return true; }

    return false;
}

void NebulaTideEditor::timerCallback()
{
   #if JUCE_IOS || JUCE_ANDROID
    // Turning the phone round moves the Dynamic Island to the other edge
    // without changing the window's size, so resized() is never called for it.
    if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
        if (display->safeAreaInsets != appliedSafeArea)
        {
            appliedSafeArea = display->safeAreaInsets;
            resized();
        }
   #endif

    if (betaExpired())
    {
        // curtain down: silence and disable everything, repaint the notice
        static bool silenced = false;
        if (! silenced) { processor.stopAll(); silenced = true; }
        for (auto* c : getChildren())
            c->setEnabled (false);
        repaint();
        return;
    }
    updatePadStates();
    updateReverbButtons();
    keysBtn.setColour (juce::TextButton::textColourOffId,
                       zoneKeyboard.isVisible() ? colours::seaBright : colours::textDim);

    if (midiDrag.isVisible())
    {
        const auto& presets = processor.getPresets();
        const int pad = juce::jlimit (0, juce::jmax (0, presets.size() - 1), viewIndex);
        if (! presets.isEmpty())
            midiDrag.setLabel (presets.getReference (pad).name);
    }
}

void NebulaTideEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bgDeep);

    auto footer = getLocalBounds().removeFromBottom (170).reduced (26, 10).toFloat();
    g.setColour (juce::Colour (0xff041826).withAlpha (0.72f));
    g.fillRoundedRectangle (footer, 20.0f);
    g.setColour (colours::seaBright.withAlpha (0.14f));
    g.drawRoundedRectangle (footer, 20.0f, 1.0f);

    if (pads.isEmpty())
    {
        g.setColour (colours::textDim);
        g.setFont (juce::Font (juce::FontOptions (14.0f)));
        g.drawFittedText ("No pads found.\nAdd audio files to the 'presets' folder and rebuild,\nor place a 'presets' folder next to the app.",
                          getLocalBounds().reduced (60), juce::Justification::centred, 4);
    }

    // beta badge / expiry curtain
    const juce::String expiry (NEBULA_BETA_EXPIRY);
    if (expiry.isNotEmpty())
    {
        if (betaExpired())
        {
            g.fillAll (colours::bgDeep.withAlpha (0.94f));
            g.setColour (colours::foam);
            g.setFont (juce::Font (juce::FontOptions (16.0f)).withExtraKerningFactor (0.2f));
            g.drawFittedText ("THIS BETA BUILD HAS EXPIRED\n\nThank you for testing Nebula Tide.\nPlease ask for the latest build.",
                              getLocalBounds().reduced (60), juce::Justification::centred, 5);
        }
        else
        {
            g.setColour (colours::textDim.withAlpha (0.7f));
            g.setFont (juce::Font (juce::FontOptions (9.5f)).withExtraKerningFactor (0.2f));
            g.drawText ("BETA " + juce::String (NEBULA_VERSION) + "  -  expires " + expiry,
                        getLocalBounds().removeFromBottom (14).reduced (10, 0), juce::Justification::centredRight);
        }
    }
}

// Called both when a cached proof loads at startup and when someone types a
// key - the same path, so activation can never leave them at the gate (A3).
void NebulaTideEditor::showMainViewIfLicensed()
{
    if (! processor.license.isLicensed() || activation == nullptr)
        return;
    activation.reset();
    resized();
    repaint();
}

juce::Rectangle<int> NebulaTideEditor::contentBounds() const
{
    auto r = getLocalBounds();
   #if JUCE_IOS || JUCE_ANDROID
    if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
        r = display->safeAreaInsets.subtractedFrom (r);
   #endif
    return r;
}

void NebulaTideEditor::resized()
{
    background.setBounds (getLocalBounds());
    if (activation != nullptr)
    {
        activation->setBounds (getLocalBounds());
        activation->toFront (false);
    }
    // Controls stay clear of the Dynamic Island and home indicator; the
    // background above is still laid edge to edge.
    const auto safe = contentBounds();
    auto area = safe;

    // Short screens - a phone in landscape - get tighter rows. Only the space
    // between things shrinks; the text keeps its size, so nothing becomes
    // unreadable the way it would if the whole desktop layout were scaled.
    const bool compact = safe.getHeight() < 600;

    // header
    auto header = area.removeFromTop (compact ? 46 : 64).reduced (26, compact ? 6 : 10);
    // On a phone the header is the tightest row: the wide-tracked title and
    // the status word between them left the preset selector no room at all,
    // squeezing its arrow to a sliver and pushing the preset name out. The
    // title steps down a size, the status word (decoration) goes, and the
    // buttons narrow, so the width lands on the thing people actually use.
    title.setFont (ui::titleFont (compact ? 15.0f : 19.0f));
    title.setBounds (header.removeFromLeft (compact ? 200 : 280));
    settingsBtn.setBounds (header.removeFromRight (86));
    header.removeFromRight (6);
    keysBtn.setBounds (header.removeFromRight (compact ? 54 : 62));
    header.removeFromRight (6);
    studioBtn.setBounds (header.removeFromRight (compact ? 64 : 74));
    if (midiDrag.isVisible())
    {
        header.removeFromRight (6);
        midiDrag.setBounds (header.removeFromRight (96).withSizeKeepingCentre (96, 26));
    }
    if (updateBtn.isVisible())
    {
        header.removeFromRight (6);
        updateBtn.setBounds (header.removeFromRight (130));
    }
    statusLabel.setVisible (! compact);
    if (! compact)
        statusLabel.setBounds (header.removeFromRight (130));

    settingsPanel.setBounds (safe.withSizeKeepingCentre (
        juce::jmin (620, safe.getWidth() - 80), juce::jmin (620, safe.getHeight() - 100)));
    settingsPanel.toFront (false);
    if (studio != nullptr)
    {
        studio->setBounds (safe.withSizeKeepingCentre (
            juce::jmin (940, safe.getWidth() - 40), juce::jmin (660, safe.getHeight() - 40)));
        studio->toFront (false);
    }
    if (downloader != nullptr)
    {
        downloader->setBounds (getLocalBounds());
        downloader->toFront (false);
    }
    const auto navRoom = compact ? header.reduced (10, 0) : header;
    auto nav = navRoom.withSizeKeepingCentre (juce::jmin (440, navRoom.getWidth()), 36);
    prevBtn.setBounds (nav.removeFromLeft (40));
    nextBtn.setBounds (nav.removeFromRight (40));
    presetLabel.setBounds (nav);

    // footer
    auto footer = compact ? area.removeFromBottom (112).reduced (28, 8)
                          : area.removeFromBottom (170).reduced (40, 20);
    const int knob = compact ? 76 : 116;

    auto volArea = footer.removeFromLeft (compact ? 110 : 150);
    volumeLabel.setBounds (volArea.removeFromBottom (16));
    volumeKnob.setBounds (volArea.withSizeKeepingCentre (knob, knob));

    auto panArea = footer.removeFromRight (compact ? 110 : 150);
    panLabel.setBounds (panArea.removeFromBottom (16));
    panKnob.setBounds (panArea.withSizeKeepingCentre (knob, knob));

    // middle: reverb block (left) + crossfade (right)
    auto middle = footer.reduced (24, 0);
    auto reverbArea = middle.removeFromLeft (middle.getWidth() * 55 / 100);
    reverbTitle.setBounds (reverbArea.removeFromTop (14));

    auto typeRow = reverbArea.removeFromTop (26);
    const int bw = typeRow.getWidth() / 3;
    roomBtn.setBounds (typeRow.removeFromLeft (bw).reduced (4, 0));
    plateBtn.setBounds (typeRow.removeFromLeft (bw).reduced (4, 0));
    hallBtn.setBounds (typeRow.reduced (4, 0));

    int rowsLeft = 3;      // MIX · SIZE · DAMP
    auto sliderRow = [&] (juce::Slider& s, juce::Label& l)
    {
        auto r = reverbArea.removeFromTop (juce::jmax (18, reverbArea.getHeight() / rowsLeft--));
        l.setBounds (r.removeFromLeft (44));
        s.setBounds (r);
    };
    sliderRow (rMixSlider, rMixLabel);
    sliderRow (rSizeSlider, rSizeLabel);
    sliderRow (rDampSlider, rDampLabel);

    middle.removeFromLeft (30);
    auto fadeArea = middle.withSizeKeepingCentre (middle.getWidth(), 56);
    fadeLabel.setBounds (fadeArea.removeFromBottom (16));
    fadeSlider.setBounds (fadeArea);

    // Kontakt-style zoned keyboard strip (when shown), then the key planets ribbon
    if (zoneKeyboard.isVisible())
        zoneKeyboard.setBounds (area.removeFromBottom (74).reduced (30, 0).withTrimmedBottom (6));
    // SHIMMER sits beside the keys as a single macro knob — one sweep takes
    // the whole effect from silent to cascading, so it wants presence rather
    // than a slider buried among the reverb trims.
    {
        const int keyRowH = compact ? 74 : (zoneKeyboard.isVisible() ? 100 : 110);
        auto keyRow = area.removeFromBottom (keyRowH).reduced (30, 0);
        auto shimArea = keyRow.removeFromRight (compact ? 80 : 112);
        shimLabel.setBounds (shimArea.removeFromBottom (16));
        const int shimKnob = compact ? 56 : 86;
        shimSlider.setBounds (shimArea.withSizeKeepingCentre (shimKnob, shimKnob));
        keyPlanets.setBounds (keyRow);
    }

    // FX star (left) and texture star (right) flank the pad grid, sitting
    // slightly above centre
    const int starW = juce::jmin (120, area.getWidth() / 7);
    const int starH = compact ? 118 : 150;
    // On a short screen the quarter-height rule lifted the stars into the
    // header, so they sit centred in whatever room is left instead.
    const int starY = compact ? area.getY() + (area.getHeight() - starH) / 2
                              : area.getY() + area.getHeight() / 4 - starH / 2;
    fxStar.setBounds (area.getX() + 22, starY, starW, starH);
    texStar.setBounds (area.getRight() - starW - 22, starY, starW, starH);

    // single-preset view: one large pad, centre stage. All pads share the same
    // bounds; visibility (one at a time) is handled in updatePadStates().
    if (! pads.isEmpty())
    {
        const int padSize = juce::jmin (190, area.getHeight() - 20);
        const auto centre = juce::Rectangle<int> (area.getCentreX() - padSize / 2,
                                                  area.getY() + (area.getHeight() - padSize) / 2,
                                                  padSize, padSize);
        for (auto* pad : pads)
            pad->setBounds (centre);
    }
}

//==============================================================================
// The drag handle: a quiet glass chip carrying the preset's own name. Nothing
// labels it as MIDI and nothing points at it — it is meant to be found rather
// than advertised, so it never competes with the controls you use constantly.
void MidiDragHandle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const auto tint = armed ? colours::seaBright : colours::textDim;

    if (armed)
        ui::drawBloom (g, r.getCentre(), r.getWidth() * 0.6f, colours::seaBright, 0.45f);

    g.setColour (colours::bgMid.withAlpha (armed ? 0.8f : 0.42f));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (tint.withAlpha (armed ? 0.85f : 0.30f));
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    // a small grab texture on each side, so it reads as draggable without a word
    auto grip = [&g, &r, tint] (float x)
    {
        for (int i = 0; i < 3; ++i)
            g.fillRect (x, r.getCentreY() - 4.0f + (float) i * 4.0f, 7.0f, 1.0f);
        juce::ignoreUnused (tint);
    };
    g.setColour (tint.withAlpha (armed ? 0.7f : 0.35f));
    grip (r.getX() + 8.0f);
    grip (r.getRight() - 15.0f);

    g.setColour (tint.withAlpha (armed ? 1.0f : 0.8f));
    g.setFont (ui::labelFont (10.0f));
    g.drawText (label.toUpperCase(), r.reduced (20.0f, 0.0f), juce::Justification::centred, true);
}
