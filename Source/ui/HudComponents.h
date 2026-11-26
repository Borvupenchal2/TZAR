// =============================================================================
//  TZAR — HUD components
// =============================================================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "TzarLookAndFeel.h"
#include "../PluginProcessor.h"

namespace tzar::ui
{
using APVTS = juce::AudioProcessorValueTreeState;

//==============================================================================
class HudKnob : public juce::Component
{
public:
    HudKnob (APVTS&, const juce::String& paramID, const juce::String& label, juce::Colour accent,
             const juce::String& tooltip, bool bipolar = false, const juce::String& suffix = {});
    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider;

private:
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<APVTS::SliderAttachment> attachment;
    juce::String label, suffix;
    juce::Colour accent;
};

//==============================================================================
class HudToggle : public juce::Button
{
public:
    HudToggle (const juce::String& buttonText, juce::Colour accent);
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    juce::Colour accent;
};

class HudButton : public juce::Button
{
public:
    HudButton (const juce::String& buttonText, juce::Colour accent, bool solid = false);
    void paintButton (juce::Graphics&, bool over, bool down) override;
private:
    juce::Colour accent;
    bool solid;
};

//==============================================================================
/** Chamfered HUD panel with coded header ("01 // DESCENT"). */
class ModulePanel : public juce::Component
{
public:
    ModulePanel (const juce::String& code, const juce::String& title, const juce::String& subtitle, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void resized() override;

    HudKnob& addKnob (std::unique_ptr<HudKnob> k);
    void setColumns (int c) { cols = c; }
    void setHeaderComponent (juce::Component* c) { headerComp = c; if (c) addAndMakeVisible (c); }
    juce::Colour getAccent() const { return accent; }
    std::vector<std::unique_ptr<HudKnob>>& getKnobs() { return knobs; }

    static constexpr float headerH = 28.0f;
    static void paintFrame (juce::Graphics&, juce::Rectangle<float> r, const juce::String& code,
                            const juce::String& title, const juce::String& subtitle, juce::Colour accent);

protected:
    juce::String code, title, subtitle;
    juce::Colour accent;
    std::vector<std::unique_ptr<HudKnob>> knobs;
    juce::Component* headerComp = nullptr;
    int cols = 3;
};

//==============================================================================
class TimelineView : public juce::Component
{
public:
    explicit TimelineView (TzarProcessor&);
    void paint (juce::Graphics&) override;
    void refresh (float animTime);

private:
    TzarProcessor& proc;
    tzar::Params params;
    tzar::Timeline tl;
    bool live = false;
    float elapsed = 0.0f, anim = 0.0f;
    int phase = 0;
    std::vector<float> thumb;
    double payloadLen = 0.0;
};

//==============================================================================
class DetonateButton : public juce::Component,
                       public juce::SettableTooltipClient
{
public:
    DetonateButton();
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool hitTest (int x, int y) override;

    void setState (int phase, float elapsed, float total, float impact, float pressureHz, float anim);
    std::function<void()> onFire;

private:
    bool overCore (juce::Point<float> p) const;
    int phase = 0;
    float elapsed = 0, total = 1, impact = 1, pressureHz = 10, anim = 0;
    bool hover = false, pressed = false;
};

class IgnitionPanel : public juce::Component
{
public:
    IgnitionPanel();
    void paint (juce::Graphics&) override;
    void resized() override;
    void update (int phase, float elapsed, float total, float impact, int grains, float pressureHz, float anim);

    DetonateButton detonate;
    HudButton abort { "ABORT", col::red };

private:
    int phase = 0, grains = 0;
    float elapsed = 0, total = 1, impact = 1, anim = 0;
};

//==============================================================================
class ScopeView : public juce::Component
{
public:
    explicit ScopeView (TzarProcessor&);
    void paint (juce::Graphics&) override;
    void refresh (int phase, float anim);

private:
    TzarProcessor& proc;
    std::array<float, TzarProcessor::Telemetry::scopeSize> data {};
    int phase = 0;
    float anim = 0;
};

//==============================================================================
class MeterView : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void setLevels (float l, float r);
private:
    float lvl[2] { 0, 0 }, hold[2] { 0, 0 };
    int holdCount[2] { 0, 0 };
};

class OutputPanel : public ModulePanel
{
public:
    OutputPanel (APVTS&);
    void resized() override;
    void paint (juce::Graphics&) override;

    HudToggle limiter { "LIMIT", col::sys };
    HudToggle autoReset { "AUTO-RESET", col::sys };
    MeterView meter;

private:
    std::unique_ptr<APVTS::ButtonAttachment> aLim, aReset;
};

//==============================================================================
class PayloadSlot : public juce::Component
{
public:
    PayloadSlot();
    void paint (juce::Graphics&) override;
    void resized() override;
    void update (const juce::String& name, std::vector<float> thumb, double lenSec);
    void setDragHover (bool h) { if (h != dragHover) { dragHover = h; repaint(); } }

    HudButton load  { "LOAD", col::blast, true };
    HudButton clear { "CLEAR", col::textDim };

private:
    juce::String name;
    std::vector<float> thumb;
    double lenSec = 0;
    bool dragHover = false;
};

//==============================================================================
class HeaderBar : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void update (float bpm, double sr, int phase, float anim);
private:
    float bpm = 120.0f, anim = 0.0f;
    double sr = 48000.0;
    int phase = 0;
};

/** Radiation trefoil, drawn as a path (generic hazard symbol). */
juce::Path trefoil (juce::Point<float> c, float rIn, float rOut, float rDot, float rotation = 0.0f);

} // namespace tzar::ui
