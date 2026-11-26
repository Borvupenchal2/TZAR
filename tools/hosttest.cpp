// Minimal VST3 host test: loads the built TZAR.vst3 through JUCE's VST3 hosting layer.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cmath>

using namespace juce;

static int failures = 0;
#define CHECK(cond, msg) do { if (! (cond)) { ++failures; std::printf ("  FAIL: %s\n", msg); } else std::printf ("  ok:   %s\n", msg); } while (0)

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const String path = argc > 1 ? argv[1] : "";
    AudioPluginFormatManager fm;
    fm.addFormat (std::make_unique<VST3PluginFormat>());
    OwnedArray<PluginDescription> descs;
    VST3PluginFormat vst3;
    vst3.findAllTypesForFile (descs, path);
    CHECK (descs.size() == 1, "VST3 scanned, one plugin found");
    if (descs.isEmpty()) return 1;
    std::printf ("  name=%s  vendor=%s  category=%s  inputs=%d  outputs=%d  midiIn=%d\n",
                 descs[0]->name.toRawUTF8(), descs[0]->manufacturerName.toRawUTF8(), descs[0]->category.toRawUTF8(),
                 descs[0]->numInputChannels, descs[0]->numOutputChannels, (int) descs[0]->isInstrument);

    String err;
    auto inst = fm.createPluginInstance (*descs[0], 48000.0, 512, err);
    CHECK (inst != nullptr, ("instance created " + err).toRawUTF8());
    if (inst == nullptr) return 1;

    auto& params = inst->getParameters();
    std::printf ("  %d parameters\n", params.size());
    AudioProcessorParameter* detonate = nullptr;
    for (auto* p : params) if (p->getName (64) == "DETONATE") detonate = p;
    CHECK (detonate != nullptr, "DETONATE parameter exposed to host");

    // editor
    {
        std::unique_ptr<AudioProcessorEditor> ed (inst->createEditorIfNeeded());
        CHECK (ed != nullptr, "editor opens");
        if (ed) { ed->setSize (1500, 975); CHECK (ed->getWidth() > 0, "editor resizes"); }
    }

    Random rnd (42);
    const double rates[] = { 44100, 48000, 96000 };
    const int blocks[] = { 64, 256, 512, 1024, 333 };
    bool allFinite = true, heardSomething = false;
    for (double sr : rates)
        for (int bs : blocks)
        {
            inst->releaseResources();
            inst->setPlayConfigDetails (2, 2, sr, bs);
            inst->prepareToPlay (sr, bs);
            AudioBuffer<float> buf (2, bs);
            MidiBuffer midi;
            // random params (except detonate)
            for (auto* p : params) if (p != detonate) p->setValueNotifyingHost (rnd.nextFloat());
            const int n = (int) (sr * 3.0 / bs);
            for (int b = 0; b < n; ++b)
            {
                for (int i = 0; i < bs; ++i) { buf.setSample (0, i, 0.3f * std::sin (0.05f * (float) (b * bs + i))); buf.setSample (1, i, buf.getSample (0, i)); }
                midi.clear();
                if (b == 2) detonate->setValueNotifyingHost (1.0f);          // automation trigger
                if (b == n / 2) midi.addEvent (MidiMessage::noteOn (1, 60, 1.0f), bs / 3);   // MIDI retrigger
                inst->processBlock (buf, midi);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < bs; ++i)
                    {
                        const float v = buf.getSample (c, i);
                        if (! std::isfinite (v)) allFinite = false;
                        if (std::abs (v) > 0.01f) heardSomething = true;
                    }
            }
            detonate->setValueNotifyingHost (0.0f);
        }
    CHECK (allFinite, "audio finite at 44.1/48/96 kHz, blocks 64..1024 and odd 333");
    CHECK (heardSomething, "audio flows");

    // state round-trip
    for (auto* p : params) if (p != detonate) p->setValueNotifyingHost (rnd.nextFloat());
    MemoryBlock state;
    inst->getStateInformation (state);
    // compare what the user sees (discrete params are quantised by the plugin); Bypass belongs to the host
    std::vector<String> before;
    for (auto* p : params) before.push_back (p->getText (p->getValue(), 64));
    for (auto* p : params) if (p != detonate) p->setValueNotifyingHost (rnd.nextFloat());
    inst->setStateInformation (state.getData(), (int) state.getSize());
    bool same = true;
    for (int i = 0; i < params.size(); ++i)
        if (params[i]->getName (64) != "Bypass" && params[i]->getText (params[i]->getValue(), 64) != before[(size_t) i])
        {
            same = false;
            std::printf ("    mismatch: %s  %s vs %s\n", params[i]->getName (64).toRawUTF8(),
                         params[i]->getText (params[i]->getValue(), 64).toRawUTF8(), before[(size_t) i].toRawUTF8());
        }
    CHECK (same, "state save / restore round-trip");

    inst->releaseResources();
    inst.reset();
    std::printf (failures == 0 ? "\nALL CHECKS PASSED\n" : "\n%d CHECK(S) FAILED\n", failures);
    return failures;
}
