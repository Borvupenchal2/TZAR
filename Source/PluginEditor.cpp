// =============================================================================
//  TZAR — Editor implementation
// =============================================================================
#include "PluginEditor.h"

using namespace juce;
using namespace tzar;
using namespace tzar::ui;

static String fr (const char* s) { return String::fromUTF8 (s); }

//==============================================================================
TzarEditor::TzarEditor (TzarProcessor& p)
    : AudioProcessorEditor (p), proc (p), timeline (p), scope (p), output (p.apvts)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    for (Component* c : std::initializer_list<Component*> { &header, &timeline, &ignition, &descent, &crush, &blast,
                                                             &debris, &tinnitus, &payload, &scope, &output })
        content.addAndMakeVisible (c);
    content.addAndMakeVisible (overlay);

    auto& s = proc.apvts;
    auto K = [&s] (ModulePanel& panel, const char* id, const char* name, const char* tip,
                   bool bipolar = false, const char* suffix = "") -> HudKnob&
    {
        return panel.addKnob (std::make_unique<HudKnob> (s, id, name, panel.getAccent(), fr (tip), bipolar, String (suffix)));
    };

    // 01 DESCENT
    fallSecKnob  = &K (descent, pid::fall, "Fall", "Durée de la chute (le sifflement) avant le silence et l'impact.");
    fallBeatKnob = &K (descent, pid::fallBeats, "Fall",
                       "Durée de la chute en temps, calée sur le tempo de FL. Automatise DETONATE N temps avant ton drop : l'impact tombe pile dessus.",
                       false, " beat");
    K (descent, pid::whStart, "Start",   "Fréquence de départ du sifflement.");
    K (descent, pid::whEnd,   "End",     "Fréquence d'arrivée du sifflement, juste avant l'impact.");
    K (descent, pid::whCurve, "Curve",   "Courbe du glissando : au-dessus de 1, la descente accélère vers la fin (effet Doppler).");
    K (descent, pid::tension, "Tension", "Pendant la chute, le mix est progressivement vidé de ses basses (un passe-haut qui monte jusqu'à 2 kHz).");
    K (descent, pid::whLevel, "Level",   "Volume du sifflement.");
    descent.setHeaderComponent (&syncToggle);
    syncToggle.setTooltip (fr ("SYNC : la durée de chute est exprimée en temps (tempo de FL) au lieu de secondes."));
    syncAttachment = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment> (s, pid::sync, syncToggle);

    // 02 VOID / CRUSH
    K (crush, pid::voidMs,   "Void",     "Silence absolu entre la fin du sifflement et l'impact (100 à 200 ms, c'est l'idéal).");
    K (crush, pid::crush,    "Depth",    "Profondeur de l'écrasement du mix au moment de l'impact.");
    K (crush, pid::hold,     "Hold",     "Durée pendant laquelle le mix reste écrasé.");
    K (crush, pid::release,  "Release",  "Temps de retour du mix après l'écrasement.");
    K (crush, pid::pressHz,  "Rate",     "Fréquence de l'onde de pression qui fait pomper le mix (10 Hz par défaut). Elle ralentit en se dissipant.");
    K (crush, pid::pressAmt, "Pressure", "Intensité du pompage : à 0 %, écrasement continu ; à 100 %, le mix pulse franchement à travers l'onde.");

    // 03 BLAST
    K (blast, pid::blDelay,  "Delay",   "Retard de l'explosion après l'impact : la lumière d'abord, le son ensuite.");
    K (blast, pid::smpLevel, "Payload", "Volume de TON sample d'impact (slot PAYLOAD en bas).");
    K (blast, pid::smpPitch, "Pitch",   "Transposition du sample d'impact. Vers le bas, c'est plus massif.", true);
    K (blast, pid::synLevel, "Synth",   "Volume du blast synthétique intégré : craquement, souffle, grondement.");
    K (blast, pid::subLevel, "Sub",     "Volume du sub drop (90 Hz qui plongent vers 28 Hz).");
    K (blast, pid::drive,    "Drive",   "Saturation du blast synthétique et du sub.");

    // 04 DEBRIS
    K (debris, pid::debTime,  "Time",    "Durée de la pluie de débris.");
    K (debris, pid::grain,    "Grain",   "Taille des fragments.");
    K (debris, pid::density,  "Density", "Fragments par seconde : très dense au début, puis ça se raréfie.");
    K (debris, pid::drop,     "Drop",    "Les fragments descendent en hauteur au fil du temps : ils retombent.");
    K (debris, pid::spread,   "Scatter", "Dispersion stéréo et aléa de hauteur des fragments.");
    K (debris, pid::reverse,  "Reverse", "Proportion de fragments joués à l'envers.");
    K (debris, pid::debLevel, "Level",   "Volume des débris. Ils sont taillés dans les 3 dernières secondes de TON mix avant l'impact.");
    K (debris, pid::residue,  "Residue", "Le grondement grave (~30 Hz) qui charge le silence après l'impact.");
    debris.setColumns (4);

    // 05 TINNITUS / DEAFNESS
    K (tinnitus, pid::tinHz,    "Freq",     "Fréquence de l'acouphène.");
    K (tinnitus, pid::tinLevel, "Level",    "Volume de l'acouphène (plafonné à -12 dB pour épargner tes oreilles).");
    K (tinnitus, pid::tinTime,  "Time",     "Durée de l'acouphène.");
    K (tinnitus, pid::ear,      "Ear",      "Oreille la plus touchée (gauche ou droite).", true);
    K (tinnitus, pid::deaf,     "Deaf",     "Surdité : le mix revient étouffé derrière ce passe-bas. Tourné à fond à droite = OFF.");
    K (tinnitus, pid::recovery, "Recovery", "Temps de récupération des oreilles : le filtre se rouvre et le niveau remonte.");

    // SYS
    output.addKnob (std::make_unique<HudKnob> (s, pid::yield, "Yield", col::sys,
                    fr ("Puissance : volume de tout ce que TZAR génère (sifflement, blast, débris, résidu, acouphène). Ton mix n'est pas affecté.")));
    output.addKnob (std::make_unique<HudKnob> (s, pid::output, "Output", col::sys, fr ("Volume de sortie global.")));

    // actions
    ignition.detonate.onFire = [this] { proc.requestDetonate(); };
    ignition.abort.onClick   = [this] { proc.requestAbort(); };
    payload.load.onClick     = [this] { launchFileChooser(); };
    payload.clear.onClick    = [this] { proc.clearPayload(); };

    const bool sync = proc.apvts.getRawParameterValue (pid::sync)->load() > 0.5f;
    fallSecKnob->setVisible (! sync);
    fallBeatKnob->setVisible (sync);

    // size: resizable with locked aspect ratio, remembered in the project
    setResizable (true, true);
    setResizeLimits (840, 546, 1920, 1248);
    getConstrainer()->setFixedAspectRatio ((double) baseW / (double) baseH);
    const int w = jlimit (840, 1920, (int) proc.apvts.state.getProperty ("uiWidth", 1080));
    setSize (w, roundToInt (w * (double) baseH / (double) baseW));

    startTimerHz (60);
}

TzarEditor::~TzarEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void TzarEditor::paint (Graphics& g) { g.fillAll (col::bg0); }

void TzarEditor::resized()
{
    scale = (float) getWidth() / (float) baseW;
    content.setBounds (0, 0, baseW, baseH);
    content.setTransform (AffineTransform::scale (scale));
    proc.apvts.state.setProperty ("uiWidth", getWidth(), nullptr);

    header.setBounds   (0, 0, baseW, 64);
    timeline.setBounds (16, 72, 848, 236);
    ignition.setBounds (876, 72, 308, 236);

    // module row: 16 columns shared by 5 panels
    const int y = 320, h = 262, gap = 10;
    const float unit = (float) (baseW - 32 - 4 * gap) / 16.0f;
    float x = 16.0f;
    auto place = [&] (Component& c, int cols)
    {
        const float w = unit * cols;
        c.setBounds (roundToInt (x), y, roundToInt (x + w) - roundToInt (x), h);
        x += w + gap;
    };
    place (descent, 3);
    place (crush, 3);
    place (blast, 3);
    place (debris, 4);
    place (tinnitus, 3);

    payload.setBounds (16, 594, 420, 170);
    scope.setBounds   (446, 594, 418, 170);
    output.setBounds  (874, 594, 310, 170);
    overlay.setBounds (0, 0, baseW, baseH);
    overlay.centre = ignition.getBounds().toFloat().getTopLeft() + ignition.detonate.getBounds().toFloat().getCentre();
}

//==============================================================================
void TzarEditor::timerCallback()
{
    constexpr float dt = 1.0f / 60.0f;
    anim += dt;
    auto& T = proc.telemetry;
    const int phase = T.phase.load();

    // impact choreography
    if (phase == 3 && lastPhase != 3) { overlay.flash = 1.0f; overlay.shock = 0.0001f; shake = 1.0f; }
    lastPhase = phase;

    float total, impact;
    if (phase != 0) { total = T.tTotal.load(); impact = T.tImpact.load(); }
    else
    {
        const auto params = proc.snapshotParams();
        const auto tl = Timeline::compute (params, proc.getPayloadLengthSec() / std::pow (2.0, params.samplePitch / 12.0));
        total = (float) tl.total; impact = (float) tl.impact;
    }

    header.update (T.hostBpm.load(), T.sampleRate.load(), phase, anim);
    timeline.refresh (anim);
    ignition.update (phase, T.elapsed.load(), total, impact, T.grains.load(),
                     proc.apvts.getRawParameterValue (pid::pressHz)->load(), anim);
    scope.refresh (phase, anim);
    output.meter.setLevels (T.peakL.exchange (0.0f), T.peakR.exchange (0.0f));
    payload.update (proc.getPayloadName(), proc.getPayloadThumbnail(), proc.getPayloadLengthSec());

    // SYNC swaps the FALL knob between seconds and beats
    const bool sync = proc.apvts.getRawParameterValue (pid::sync)->load() > 0.5f;
    if (fallBeatKnob->isVisible() != sync)
    {
        fallSecKnob->setVisible (! sync);
        fallBeatKnob->setVisible (sync);
        descent.resized();
    }

    // flash / shockwave / void dim
    const float voidTarget = phase == 2 ? 1.0f : 0.0f;
    const float prevVoid = overlay.voidDim;
    overlay.voidDim += (voidTarget - overlay.voidDim) * (voidTarget > overlay.voidDim ? 0.6f : 0.12f);
    if (overlay.flash > 0.0f || overlay.shock > 0.0f || std::abs (overlay.voidDim - prevVoid) > 1e-4f || overlay.voidDim > 0.001f)
    {
        overlay.flash = overlay.flash > 0.01f ? overlay.flash * 0.88f : 0.0f;
        if (overlay.shock > 0.0f) { overlay.shock += dt / 1.1f; if (overlay.shock >= 1.0f) overlay.shock = 0.0f; }
        overlay.repaint();
    }

    // screen shake on impact
    if (shake > 0.0f)
    {
        shake *= 0.93f;
        const float a = 5.0f * shake;
        if (shake < 0.02f) { shake = 0.0f; content.setTransform (AffineTransform::scale (scale)); }
        else content.setTransform (AffineTransform::translation (rnd.nextFloat() * 2.0f * a - a, rnd.nextFloat() * 2.0f * a - a).scaled (scale));
    }
}

//==============================================================================
void TzarEditor::launchFileChooser()
{
    chooser = std::make_unique<FileChooser> ("TZAR // select impact sample", File(), "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
    chooser->launchAsync (FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles,
                          [this] (const FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) proc.loadPayload (f);
                          });
}

static bool isAudioFile (const String& path)
{
    return File (path).hasFileExtension ("wav;aif;aiff;flac;mp3;ogg");
}

bool TzarEditor::isInterestedInFileDrag (const StringArray& files)
{
    for (auto& f : files) if (isAudioFile (f)) return true;
    return false;
}

void TzarEditor::fileDragEnter (const StringArray&, int, int) { payload.setDragHover (true); }
void TzarEditor::fileDragExit (const StringArray&)            { payload.setDragHover (false); }

void TzarEditor::filesDropped (const StringArray& files, int, int)
{
    payload.setDragHover (false);
    for (auto& f : files)
        if (isAudioFile (f) && proc.loadPayload (File (f))) break;
}

//==============================================================================
//  Background (rendered once per scale, at physical resolution)
//==============================================================================
void TzarEditor::Content::paint (Graphics& g)
{
    const float s = g.getInternalContext().getPhysicalPixelScaleFactor();
    if (bg.isNull() || std::abs (s - bgScale) > 0.01f)
        rebuildBackground (s);
    g.drawImageTransformed (bg, AffineTransform::scale (1.0f / bgScale));
}

void TzarEditor::Content::rebuildBackground (float s)
{
    bgScale = jlimit (0.5f, 4.0f, s);
    const int W = TzarEditor::baseW, H = TzarEditor::baseH;
    bg = Image (Image::ARGB, roundToInt (W * bgScale), roundToInt (H * bgScale), true);
    Graphics g (bg);
    g.addTransform (AffineTransform::scale (bgScale));

    g.setGradientFill (ColourGradient (col::bg1, 0, 0, col::bg0, 0, (float) H, false));
    g.fillAll();

    auto glow = [&] (float x, float y, float r, Colour c)
    {
        g.setGradientFill (ColourGradient (c, x, y, c.withAlpha (0.0f), x + r, y, true));
        g.fillEllipse (x - r, y - r, r * 2, r * 2);
    };
    glow (1030.0f, 190.0f, 420.0f, col::red.withAlpha (0.10f));
    glow (430.0f, 190.0f, 640.0f, col::cyan.withAlpha (0.05f));
    glow (600.0f, 700.0f, 520.0f, col::amber.withAlpha (0.03f));

    // grid
    for (int x = 0; x <= W; x += 20)
    {
        g.setColour ((x % 100 == 0 ? col::cyan.withAlpha (0.045f) : Colours::white.withAlpha (0.018f)));
        g.drawVerticalLine (x, 0.0f, (float) H);
    }
    for (int y = 0; y <= H; y += 20)
    {
        g.setColour ((y % 100 == 0 ? col::cyan.withAlpha (0.045f) : Colours::white.withAlpha (0.018f)));
        g.drawHorizontalLine (y, 0.0f, (float) W);
    }
    g.setColour (col::cyan.withAlpha (0.16f));
    for (int x = 100; x < W; x += 100)
        for (int y = 100; y < H; y += 100)
        {
            g.fillRect ((float) x - 3.0f, (float) y - 0.5f, 7.0f, 1.0f);
            g.fillRect ((float) x - 0.5f, (float) y - 3.0f, 1.0f, 7.0f);
        }

    // scanlines
    g.setColour (Colours::black.withAlpha (0.16f));
    for (int y = 0; y < H; y += 3)
        g.drawHorizontalLine (y, 0.0f, (float) W);

    // vignette
    g.setGradientFill (ColourGradient (Colours::transparentBlack, W * 0.5f, H * 0.45f,
                                       Colours::black.withAlpha (0.6f), 0.0f, 0.0f, true));
    g.fillAll();

    // outer frame + footer
    g.setColour (col::line);
    g.drawRect (Rectangle<float> (4.0f, 4.0f, W - 8.0f, H - 8.0f), 1.0f);
    g.setColour (col::textFaint.brighter (0.2f));
    g.setFont (Fonts::get().mono (10.0f));
    g.drawText ("TZAR LABS  //  EXPERIMENTAL AUDIO ORDNANCE", Rectangle<float> (18.0f, H - 15.0f, 500.0f, 11.0f), Justification::centredLeft, false);
    g.drawText (fr ("DBL-CLICK = RESET  ·  SHIFT-DRAG = FINE  ·  MIDI NOTE / AUTOMATION = DETONATE"),
                Rectangle<float> (W - 618.0f, H - 15.0f, 600.0f, 11.0f), Justification::centredRight, false);
}

//==============================================================================
void TzarEditor::FlashOverlay::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    if (voidDim > 0.001f)
    {
        g.setColour (Colours::black.withAlpha (0.55f * voidDim));
        g.fillRect (b);
    }
    if (flash > 0.0f)
    {
        g.setGradientFill (ColourGradient (col::white.withAlpha (0.85f * flash), centre.x, centre.y,
                                           col::red.withAlpha (0.25f * flash), centre.x + 1200.0f, centre.y, true));
        g.fillRect (b);
    }
    if (shock > 0.0f)
    {
        const float r = 20.0f + shock * 1500.0f;
        const float a = (1.0f - shock);
        Path ring;
        ring.addEllipse (centre.x - r, centre.y - r, r * 2, r * 2);
        neonStroke (g, ring, col::white.withAlpha (0.8f * a), 2.0f + 10.0f * (1.0f - shock), 1.5f);
        const float r2 = r * 0.72f;
        Path ring2;
        ring2.addEllipse (centre.x - r2, centre.y - r2, r2 * 2, r2 * 2);
        neonStroke (g, ring2, col::red.withAlpha (0.6f * a), 1.5f, 1.2f);
    }
}
