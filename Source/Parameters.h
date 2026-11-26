// =============================================================================
//  TZAR — parameter definitions
// =============================================================================
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/TzarEngine.h"

namespace tzar::pid
{
    // 01 DESCENT
    inline constexpr auto fall      = "fall";
    inline constexpr auto sync      = "sync";
    inline constexpr auto fallBeats = "fallBeats";
    inline constexpr auto whStart   = "whStart";
    inline constexpr auto whEnd     = "whEnd";
    inline constexpr auto whCurve   = "whCurve";
    inline constexpr auto tension   = "tension";
    inline constexpr auto whLevel   = "whLevel";
    // 02 VOID / CRUSH
    inline constexpr auto voidMs    = "void";
    inline constexpr auto crush     = "crush";
    inline constexpr auto hold      = "hold";
    inline constexpr auto release   = "release";
    inline constexpr auto pressHz   = "pressHz";
    inline constexpr auto pressAmt  = "pressAmt";
    // 03 BLAST
    inline constexpr auto blDelay   = "blDelay";
    inline constexpr auto smpLevel  = "smpLevel";
    inline constexpr auto smpPitch  = "smpPitch";
    inline constexpr auto synLevel  = "synLevel";
    inline constexpr auto subLevel  = "subLevel";
    inline constexpr auto drive     = "drive";
    // 04 DEBRIS
    inline constexpr auto debTime   = "debTime";
    inline constexpr auto grain     = "grain";
    inline constexpr auto density   = "density";
    inline constexpr auto drop      = "drop";
    inline constexpr auto spread    = "spread";
    inline constexpr auto reverse   = "reverse";
    inline constexpr auto debLevel  = "debLevel";
    inline constexpr auto residue   = "residue";
    // 05 TINNITUS / DEAFNESS
    inline constexpr auto tinHz     = "tinHz";
    inline constexpr auto tinLevel  = "tinLevel";
    inline constexpr auto tinTime   = "tinTime";
    inline constexpr auto ear       = "ear";
    inline constexpr auto deaf      = "deaf";
    inline constexpr auto recovery  = "recovery";
    // SYS
    inline constexpr auto yield     = "yield";
    inline constexpr auto output    = "output";
    inline constexpr auto limiter   = "limiter";
    inline constexpr auto stopReset = "stopReset";
    inline constexpr auto detonate  = "detonate";
}

namespace tzar
{
inline const juce::StringArray& beatChoices()
{
    static const juce::StringArray c { "1/2", "1", "2", "3", "4", "6", "8", "12", "16" };
    return c;
}
inline float beatValue (int index)
{
    static const float v[] { 0.5f, 1, 2, 3, 4, 6, 8, 12, 16 };
    return v[juce::jlimit (0, 8, index)];
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    using F = AudioParameterFloat;
    using B = AudioParameterBool;
    using A = AudioParameterFloatAttributes;

    auto range = [] (float lo, float hi, float centre, float step = 0.0f)
    {
        NormalisableRange<float> r (lo, hi, step);
        r.setSkewForCentre (centre);
        return r;
    };
    auto lin = [] (float lo, float hi, float step = 0.0f) { return NormalisableRange<float> (lo, hi, step); };

    auto secs = A().withStringFromValueFunction ([] (float v, int) { return v < 10.0f ? String (v, 2) + " s" : String (v, 1) + " s"; })
                   .withLabel ("s");
    auto ms   = A().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " ms"; }).withLabel ("ms");
    auto hz   = A().withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " k" : String (roundToInt (v)) + " Hz"; })
                   .withLabel ("Hz");
    auto db   = A().withStringFromValueFunction ([] (float v, int) { return v <= -59.9f ? String ("OFF") : String (v, 1) + " dB"; })
                   .withLabel ("dB");
    auto pct  = A().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " %"; }).withLabel ("%");
    auto st   = A().withStringFromValueFunction ([] (float v, int) { return (v > 0 ? "+" : "") + String (v, 1) + " st"; }).withLabel ("st");

    AudioProcessorValueTreeState::ParameterLayout l;
    auto add = [&] (auto p) { l.add (std::move (p)); };
    auto P   = [] (const char* id) { return ParameterID { id, 1 }; };

    // 01 DESCENT
    add (std::make_unique<F> (P (pid::fall),    "Fall Time",     range (0.25f, 12.0f, 3.0f), 3.0f, secs));
    add (std::make_unique<B> (P (pid::sync),    "Fall Sync",     false));
    add (std::make_unique<AudioParameterChoice> (P (pid::fallBeats), "Fall Beats", beatChoices(), 4));
    add (std::make_unique<F> (P (pid::whStart), "Whistle Start", range (400.0f, 6000.0f, 1800.0f), 2400.0f, hz));
    add (std::make_unique<F> (P (pid::whEnd),   "Whistle End",   range (60.0f, 1500.0f, 350.0f), 320.0f, hz));
    add (std::make_unique<F> (P (pid::whCurve), "Whistle Curve", lin (0.5f, 3.0f), 1.6f,
                               A().withStringFromValueFunction ([] (float v, int) { return String (v, 2); })));
    add (std::make_unique<F> (P (pid::tension), "Tension",       lin (0.0f, 100.0f), 35.0f, pct));
    add (std::make_unique<F> (P (pid::whLevel), "Whistle Level", lin (-60.0f, 6.0f), -10.0f, db));

    // 02 VOID / CRUSH
    add (std::make_unique<F> (P (pid::voidMs),  "Void",          range (0.0f, 500.0f, 150.0f), 150.0f, ms));
    add (std::make_unique<F> (P (pid::crush),   "Crush Depth",   lin (-80.0f, 0.0f), -42.0f, db));
    add (std::make_unique<F> (P (pid::hold),    "Crush Hold",    range (0.1f, 6.0f, 1.5f), 1.5f, secs));
    add (std::make_unique<F> (P (pid::release), "Crush Release", range (0.02f, 4.0f, 0.7f), 0.7f, secs));
    add (std::make_unique<F> (P (pid::pressHz), "Pressure Rate", range (1.0f, 30.0f, 10.0f), 10.0f,
                               A().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " Hz"; })));
    add (std::make_unique<F> (P (pid::pressAmt),"Pressure",      lin (0.0f, 100.0f), 55.0f, pct));

    // 03 BLAST
    add (std::make_unique<F> (P (pid::blDelay), "Blast Delay",   range (0.0f, 1000.0f, 200.0f), 200.0f, ms));
    add (std::make_unique<F> (P (pid::smpLevel),"Payload Level", lin (-60.0f, 6.0f), -3.0f, db));
    add (std::make_unique<F> (P (pid::smpPitch),"Payload Pitch", lin (-24.0f, 12.0f), 0.0f, st));
    add (std::make_unique<F> (P (pid::synLevel),"Synth Blast",   lin (-60.0f, 6.0f), -5.0f, db));
    add (std::make_unique<F> (P (pid::subLevel),"Sub Drop",      lin (-60.0f, 6.0f), -6.0f, db));
    add (std::make_unique<F> (P (pid::drive),   "Drive",         lin (0.0f, 100.0f), 45.0f, pct));

    // 04 DEBRIS
    add (std::make_unique<F> (P (pid::debTime), "Debris Time",   range (0.5f, 15.0f, 5.0f), 5.0f, secs));
    add (std::make_unique<F> (P (pid::grain),   "Grain Size",    range (10.0f, 400.0f, 80.0f), 70.0f, ms));
    add (std::make_unique<F> (P (pid::density), "Density",       range (1.0f, 150.0f, 35.0f), 35.0f,
                               A().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + " /s"; })));
    add (std::make_unique<F> (P (pid::drop),    "Pitch Drop",    lin (0.0f, 36.0f), 12.0f, st));
    add (std::make_unique<F> (P (pid::spread),  "Scatter",       lin (0.0f, 100.0f), 75.0f, pct));
    add (std::make_unique<F> (P (pid::reverse), "Reverse",       lin (0.0f, 100.0f), 35.0f, pct));
    add (std::make_unique<F> (P (pid::debLevel),"Debris Level",  lin (-60.0f, 6.0f), -4.0f, db));
    add (std::make_unique<F> (P (pid::residue), "Bass Residue",  lin (-60.0f, 6.0f), -13.0f, db));

    // 05 TINNITUS / DEAFNESS
    add (std::make_unique<F> (P (pid::tinHz),   "Tinnitus Freq", range (1000.0f, 14000.0f, 5000.0f), 6200.0f, hz));
    add (std::make_unique<F> (P (pid::tinLevel),"Tinnitus Level",lin (-60.0f, -12.0f), -30.0f, db));
    add (std::make_unique<F> (P (pid::tinTime), "Tinnitus Time", range (0.5f, 30.0f, 8.0f), 9.0f, secs));
    add (std::make_unique<F> (P (pid::ear),     "Ear Balance",   lin (-100.0f, 100.0f), 25.0f,
                               A().withStringFromValueFunction ([] (float v, int)
                               {
                                   const int i = roundToInt (v);
                                   return i == 0 ? String ("C") : (i < 0 ? "L" + String (-i) : "R" + String (i));
                               })));
    add (std::make_unique<F> (P (pid::deaf),    "Deafness",      range (100.0f, 20000.0f, 1200.0f), 380.0f,
                               A().withStringFromValueFunction ([] (float v, int)
                               {
                                   if (v >= 19000.0f) return String ("OFF");
                                   return v >= 1000.0f ? String (v / 1000.0f, 2) + " k" : String (roundToInt (v)) + " Hz";
                               })));
    add (std::make_unique<F> (P (pid::recovery),"Recovery",      range (0.5f, 30.0f, 7.0f), 7.0f, secs));

    // SYS
    add (std::make_unique<F> (P (pid::yield),   "Yield",         lin (0.0f, 150.0f), 100.0f, pct));
    add (std::make_unique<F> (P (pid::output),  "Output",        lin (-24.0f, 6.0f), 0.0f, db));
    add (std::make_unique<B> (P (pid::limiter), "Limiter",       true));
    add (std::make_unique<B> (P (pid::stopReset),"Reset On Stop",true));
    add (std::make_unique<B> (P (pid::detonate),"DETONATE",      false));
    return l;
}
} // namespace tzar
