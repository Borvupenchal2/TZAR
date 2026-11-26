// =============================================================================
//  TZAR — LookAndFeel implementation
// =============================================================================
#include "TzarLookAndFeel.h"
#include "BinaryData.h"

namespace tzar::ui
{
juce::Colour col::forPhase (int phase)
{
    switch (phase)
    {
        case 1:  return cyan;      // FALL
        case 2:  return white;     // VOID
        case 3:  return crush;     // IMPACT
        case 4:  return amber;     // DEBRIS
        case 5:  return ice;       // RECOVERY
        default: return green;     // STANDBY
    }
}

//==============================================================================
Fonts& Fonts::get()
{
    static Fonts f;
    return f;
}

Fonts::Fonts()
{
    orbitron  = juce::Typeface::createSystemTypefaceFor (BinaryData::OrbitronVariable_ttf,     BinaryData::OrbitronVariable_ttfSize);
    rajdhani  = juce::Typeface::createSystemTypefaceFor (BinaryData::RajdhaniSemiBold_ttf,     BinaryData::RajdhaniSemiBold_ttfSize);
    shareTech = juce::Typeface::createSystemTypefaceFor (BinaryData::ShareTechMonoRegular_ttf, BinaryData::ShareTechMonoRegular_ttfSize);
}

static juce::Font fromTf (const juce::Typeface::Ptr& tf, float h)
{
    if (tf == nullptr) return juce::Font (juce::FontOptions (h));
    return juce::Font (juce::FontOptions (tf).withHeight (h));
}

juce::Font Fonts::display (float h) const { return fromTf (orbitron, h); }
juce::Font Fonts::label   (float h) const { return fromTf (rajdhani, h); }
juce::Font Fonts::mono    (float h) const { return fromTf (shareTech, h); }

//==============================================================================
void drawTracked (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                  juce::Rectangle<float> area, juce::Justification just, float tracking)
{
    g.setFont (font.withExtraKerningFactor (tracking));
    g.drawText (text, area, just, false);
}

juce::Path hudFrame (juce::Rectangle<float> r, float cut)
{
    juce::Path p;
    p.startNewSubPath (r.getX() + cut, r.getY());
    p.lineTo (r.getRight(), r.getY());
    p.lineTo (r.getRight(), r.getBottom() - cut);
    p.lineTo (r.getRight() - cut, r.getBottom());
    p.lineTo (r.getX(), r.getBottom());
    p.lineTo (r.getX(), r.getY() + cut);
    p.closeSubPath();
    return p;
}

void neonStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width, float glow)
{
    if (glow > 0.0f)
    {
        g.setColour (c.withMultipliedAlpha (0.07f * glow));
        g.strokePath (p, juce::PathStrokeType (width * 5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.withMultipliedAlpha (0.16f * glow));
        g.strokePath (p, juce::PathStrokeType (width * 2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
TzarLookAndFeel::TzarLookAndFeel()
{
    setColour (juce::TooltipWindow::backgroundColourId, col::panel);
    setColour (juce::TooltipWindow::textColourId, col::text);
    setColour (juce::TooltipWindow::outlineColourId, col::lineHi);
    setColour (juce::PopupMenu::backgroundColourId, col::panel);
    setColour (juce::PopupMenu::textColourId, col::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, col::cyan.withAlpha (0.2f));
    setColour (juce::PopupMenu::highlightedTextColourId, col::white);
    setColour (juce::ResizableWindow::backgroundColourId, col::bg0);
}

void TzarLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float a0, float a1, juce::Slider& s)
{
    using namespace juce;
    const auto accent  = Colour ((uint32) (int64) s.getProperties().getWithDefault ("accent", (int64) col::cyan.getARGB()));
    const bool bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const bool hot     = s.isMouseOverOrDragging();
    const bool enabled = s.isEnabled();

    const auto  b  = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    const float R  = jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto  c  = b.getCentre();
    const float ang = a0 + pos * (a1 - a0);
    const Colour ac = enabled ? accent : col::textDim;

    // --- outer tick crown
    const int ticks = 25;
    for (int i = 0; i < ticks; ++i)
    {
        const float tp = (float) i / (ticks - 1);
        const float ta = a0 + tp * (a1 - a0);
        const bool lit = bipolar ? ((tp >= 0.5f && tp <= pos + 1e-4f) || (tp <= 0.5f && tp >= pos - 1e-4f))
                                 : (tp <= pos + 1e-4f);
        const bool major = (i % 6 == 0);
        const float r1 = R * (major ? 0.86f : 0.90f), r2 = R * 0.98f;
        g.setColour (lit ? ac.withAlpha (hot ? 0.95f : 0.75f) : col::lineHi.withAlpha (major ? 0.9f : 0.55f));
        g.drawLine (c.x + r1 * std::sin (ta), c.y - r1 * std::cos (ta),
                    c.x + r2 * std::sin (ta), c.y - r2 * std::cos (ta), major ? 1.4f : 1.0f);
    }

    // --- track + value arc
    const float ar = R * 0.74f;
    Path track;
    track.addCentredArc (c.x, c.y, ar, ar, 0.0f, a0, a1, true);
    g.setColour (col::line);
    g.strokePath (track, PathStrokeType (3.0f, PathStrokeType::curved, PathStrokeType::butt));

    const float from = bipolar ? (a0 + a1) * 0.5f : a0;
    if (std::abs (ang - from) > 0.001f)
    {
        Path val;
        val.addCentredArc (c.x, c.y, ar, ar, 0.0f, jmin (from, ang), jmax (from, ang), true);
        neonStroke (g, val, ac, 3.0f, hot ? 1.6f : 1.0f);
    }

    // --- body
    const float br = R * 0.56f;
    {
        ColourGradient grad (col::panelHi.brighter (hot ? 0.25f : 0.1f), c.x - br * 0.5f, c.y - br * 0.7f,
                             col::bg0, c.x + br * 0.6f, c.y + br, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - br, c.y - br, br * 2, br * 2);
        g.setColour (col::lineHi.withAlpha (0.9f));
        g.drawEllipse (c.x - br, c.y - br, br * 2, br * 2, 1.0f);
        // inner bevel
        g.setColour (Colours::white.withAlpha (0.05f));
        g.drawEllipse (c.x - br + 2.5f, c.y - br + 2.5f, br * 2 - 5, br * 2 - 5, 1.0f);
    }

    // --- pointer
    {
        const float p1 = br * 0.25f, p2 = br * 0.92f;
        Path ptr;
        ptr.startNewSubPath (c.x + p1 * std::sin (ang), c.y - p1 * std::cos (ang));
        ptr.lineTo (c.x + p2 * std::sin (ang), c.y - p2 * std::cos (ang));
        neonStroke (g, ptr, ac, 2.0f, 0.8f);
        // tip on the arc
        const float tx = c.x + ar * std::sin (ang), ty = c.y - ar * std::cos (ang);
        g.setColour (col::white.withAlpha (enabled ? 0.95f : 0.3f));
        g.fillEllipse (tx - 2.2f, ty - 2.2f, 4.4f, 4.4f);
    }
}

//==============================================================================
void TzarLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    using namespace juce;
    auto r = Rectangle<float> (0, 0, (float) width, (float) height);
    g.setColour (col::bg0.withAlpha (0.97f));
    g.fillRect (r);
    g.setColour (col::cyan.withAlpha (0.6f));
    g.drawRect (r, 1.0f);
    g.fillRect (r.removeFromLeft (3.0f));

    AttributedString s;
    s.setJustification (Justification::centredLeft);
    s.append (text, Fonts::get().label (15.0f), col::text);
    TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, (float) width - 20.0f);
    tl.draw (g, Rectangle<float> (12, 0, (float) width - 20, (float) height));
}

juce::Rectangle<int> TzarLookAndFeel::getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                                        juce::Rectangle<int> parentArea)
{
    using namespace juce;
    AttributedString s;
    s.append (tipText, Fonts::get().label (15.0f));
    TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, 320.0f);
    const int w = (int) (tl.getWidth() + 28.0f), h = (int) (tl.getHeight() + 14.0f);
    return Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                           screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
             .constrainedWithin (parentArea);
}

void TzarLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (col::panel);
    g.setColour (col::cyan.withAlpha (0.5f));
    g.drawRect (0, 0, width, height);
}

juce::Font TzarLookAndFeel::getPopupMenuFont() { return Fonts::get().label (16.0f); }

} // namespace tzar::ui
