// =============================================================================
//  TZAR — visual identity: palette, typography, LookAndFeel
// =============================================================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace tzar::ui
{
namespace col
{
    const juce::Colour bg0      { 0xff030509 };
    const juce::Colour bg1      { 0xff070b12 };
    const juce::Colour panel    { 0xff0a1019 };
    const juce::Colour panelHi  { 0xff0f1824 };
    const juce::Colour line     { 0xff1a2838 };
    const juce::Colour lineHi   { 0xff2a405a };
    const juce::Colour text     { 0xffcfdcea };
    const juce::Colour textDim  { 0xff5f7389 };
    const juce::Colour textFaint{ 0xff33455a };

    const juce::Colour cyan     { 0xff00e5ff };
    const juce::Colour red      { 0xffff2a3d };
    const juce::Colour crush    { 0xffff3d5e };
    const juce::Colour orange   { 0xffff8a1f };
    const juce::Colour amber    { 0xffffc23d };
    const juce::Colour ice      { 0xffb9a8ff };
    const juce::Colour green    { 0xff3dffa0 };
    const juce::Colour white    { 0xfff4f8ff };

    // module accents
    const juce::Colour descent  = cyan;
    const juce::Colour voidc    = crush;
    const juce::Colour blast    = orange;
    const juce::Colour debris   = amber;
    const juce::Colour tinnitus = ice;
    const juce::Colour sys      = green;

    juce::Colour forPhase (int phase);
}

//------------------------------------------------------------------------------
struct Fonts
{
    static Fonts& get();
    juce::Font display (float h) const;   // Orbitron  — titles, logo
    juce::Font label   (float h) const;   // Rajdhani  — labels
    juce::Font mono    (float h) const;   // Share Tech Mono — readouts

private:
    Fonts();
    juce::Typeface::Ptr orbitron, rajdhani, shareTech;
};

/** Draws text with explicit letter spacing (tracking), the signature HUD look. */
void drawTracked (juce::Graphics& g, const juce::String& text, const juce::Font& font,
                  juce::Rectangle<float> area, juce::Justification just, float tracking);

/** Chamfered HUD frame path (top-left and bottom-right corners cut). */
juce::Path hudFrame (juce::Rectangle<float> r, float cut);

/** Neon stroke: several passes with decreasing alpha for a soft glow. */
void neonStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width, float glow = 1.0f);

//------------------------------------------------------------------------------
class TzarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TzarLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;

    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos,
                                           juce::Rectangle<int> parentArea) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    juce::Font getPopupMenuFont() override;
};

} // namespace tzar::ui
