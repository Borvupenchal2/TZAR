// =============================================================================
//  TZAR — Editor
// =============================================================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "ui/TzarLookAndFeel.h"
#include "ui/HudComponents.h"

class TzarEditor : public juce::AudioProcessorEditor,
                   public juce::FileDragAndDropTarget,
                   private juce::Timer
{
public:
    explicit TzarEditor (TzarProcessor&);
    ~TzarEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

    static constexpr int baseW = 1200, baseH = 780;

    /** Test hook (offline screenshot tool): advance the UI by one frame. */
    void advanceFrame() { timerCallback(); }

private:
    void timerCallback() override;
    void launchFileChooser();

    //--------------------------------------------------------------------------
    /** Everything is laid out at base size inside this component, which is then scaled. */
    struct Content : public juce::Component
    {
        void paint (juce::Graphics&) override;
        void rebuildBackground (float scale);
        juce::Image bg;
        float bgScale = 0.0f;
        float glowPhase = 0.0f;
    };

    struct FlashOverlay : public juce::Component
    {
        FlashOverlay() { setInterceptsMouseClicks (false, false); }
        void paint (juce::Graphics&) override;
        float flash = 0.0f, shock = 0.0f, voidDim = 0.0f;
        juce::Point<float> centre;
    };

    TzarProcessor& proc;
    tzar::ui::TzarLookAndFeel lnf;
    Content content;

    tzar::ui::HeaderBar     header;
    tzar::ui::TimelineView  timeline;
    tzar::ui::IgnitionPanel ignition;
    tzar::ui::ModulePanel   descent  { "01", "DESCENT",  "GLISSANDO", tzar::ui::col::descent };
    tzar::ui::ModulePanel   crush    { "02", "VOID",     "CRUSH",     tzar::ui::col::voidc };
    tzar::ui::ModulePanel   blast    { "03", "BLAST",    "SHOCKWAVE", tzar::ui::col::blast };
    tzar::ui::ModulePanel   debris   { "04", "DEBRIS",   "GRANULAR FALLOUT", tzar::ui::col::debris };
    tzar::ui::ModulePanel   tinnitus { "05", "TINNITUS", "AFTERMATH", tzar::ui::col::tinnitus };
    tzar::ui::PayloadSlot   payload;
    tzar::ui::ScopeView     scope;
    tzar::ui::OutputPanel   output;
    FlashOverlay            overlay;

    tzar::ui::HudToggle syncToggle { "SYNC", tzar::ui::col::descent };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> syncAttachment;
    tzar::ui::HudKnob* fallSecKnob = nullptr;
    tzar::ui::HudKnob* fallBeatKnob = nullptr;

    juce::TooltipWindow tooltips { this, 600 };
    std::unique_ptr<juce::FileChooser> chooser;

    float anim = 0.0f;
    int lastPhase = 0;
    float shake = 0.0f;
    juce::Random rnd;
    float scale = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TzarEditor)
};
