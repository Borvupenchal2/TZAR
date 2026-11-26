// Offline UI snapshot: renders the TZAR editor to PNG in several states.
#include "PluginProcessor.h"
#include "PluginEditor.h"

static void synthMusic (juce::AudioBuffer<float>& b, double& t, double sr)
{
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double tb = std::fmod (t, 60.0 / 128.0);
        const float kick = (float) (std::sin (6.2831853 * (45.0 + 120.0 * std::exp (-tb / 0.03)) * tb) * std::exp (-tb / 0.25));
        const float pad = 0.08f * (float) std::sin (6.2831853 * 220.0 * t) + 0.05f * (float) std::sin (6.2831853 * 329.6 * t);
        b.setSample (0, i, kick * 0.5f + pad);
        b.setSample (1, i, kick * 0.5f + pad * 0.9f);
        t += 1.0 / sr;
    }
}

static void save (juce::Component& c, const juce::String& path, float scale)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    juce::File f (path);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String outDir = argc > 1 ? argv[1] : ".";
    const juce::String payloadPath = argc > 2 ? argv[2] : "";

    const double sr = 48000.0; const int bs = 800;   // 800 samples = one UI frame at 60 fps
    TzarProcessor proc;
    proc.setPlayConfigDetails (2, 2, sr, bs);
    proc.prepareToPlay (sr, bs);
    if (payloadPath.isNotEmpty()) proc.loadPayload (juce::File (payloadPath));

    std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
    ed->setSize (1200, 780);
    auto* tz = dynamic_cast<TzarEditor*> (ed.get());

    juce::AudioBuffer<float> buf (2, bs);
    juce::MidiBuffer midi;
    double t = 0.0;
    auto frames = [&] (int n)
    {
        for (int i = 0; i < n; ++i) { synthMusic (buf, t, sr); proc.processBlock (buf, midi); tz->advanceFrame(); }
    };

    frames (30);
    save (*ed, outDir + "/ui_idle.png", 1.0f);

    proc.requestDetonate();
    frames (95);                                     // ~1.6 s into the fall
    save (*ed, outDir + "/ui_descent.png", 1.0f);
    const float impact = proc.telemetry.tImpact.load();
    frames ((int) ((impact - 1.583f) * 60.0f) + 4);   // a few frames after impact (flash + shock)
    save (*ed, outDir + "/ui_impact.png", 1.0f);
    frames (110);                                    // debris shower
    save (*ed, outDir + "/ui_debris.png", 1.0f);
    std::printf ("phase %d grains %d\n", proc.telemetry.phase.load(), proc.telemetry.grains.load());
    ed.reset();
    return 0;
}
