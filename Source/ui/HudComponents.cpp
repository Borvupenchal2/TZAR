// =============================================================================
//  TZAR — HUD components implementation
// =============================================================================
#include "HudComponents.h"

namespace tzar::ui
{
using namespace juce;

//==============================================================================
//  helpers
//==============================================================================
Path trefoil (Point<float> c, float rIn, float rOut, float rDot, float rotation)
{
    Path p;
    for (int i = 0; i < 3; ++i)
    {
        const float mid = rotation + (float) i * MathConstants<float>::twoPi / 3.0f;   // blade centre
        const float a0 = mid - MathConstants<float>::pi / 6.0f;
        const float a1 = mid + MathConstants<float>::pi / 6.0f;
        Path blade;
        blade.addCentredArc (c.x, c.y, rOut, rOut, 0.0f, a0, a1, true);
        blade.addCentredArc (c.x, c.y, rIn, rIn, 0.0f, a1, a0, false);
        blade.closeSubPath();
        p.addPath (blade);
    }
    p.addEllipse (c.x - rDot, c.y - rDot, rDot * 2, rDot * 2);
    return p;
}

static void cornerBrackets (Graphics& g, Rectangle<float> r, Colour c, float len = 7.0f, float th = 1.2f)
{
    g.setColour (c);
    g.fillRect (r.getRight() - len, r.getY(), len, th);
    g.fillRect (r.getRight() - th, r.getY(), th, len);
    g.fillRect (r.getX(), r.getBottom() - th, len, th);
    g.fillRect (r.getX(), r.getBottom() - len, th, len);
}

static String phaseName (int phase)
{
    switch (phase)
    {
        case 1:  return "DESCENT";
        case 2:  return "VOID";
        case 3:  return "IMPACT";
        case 4:  return "DEBRIS";
        case 5:  return "AFTERMATH";
        default: return "STANDBY";
    }
}

//==============================================================================
//  HudKnob
//==============================================================================
HudKnob::HudKnob (APVTS& s, const String& paramID, const String& lbl, Colour ac,
                  const String& tip, bool bipolar, const String& sfx)
    : label (lbl), suffix (sfx), accent (ac)
{
    param = s.getParameter (paramID);
    slider.setSliderStyle (Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (MathConstants<float>::pi * -0.75f, MathConstants<float>::pi * 0.75f, true);
    slider.setMouseDragSensitivity (220);
    slider.setVelocityModeParameters (0.35, 1, 0.0, true, ModifierKeys::shiftModifier);
    slider.getProperties().set ("accent", (int64) ac.getARGB());
    slider.getProperties().set ("bipolar", bipolar);
    slider.setTooltip (tip);
    slider.setPopupMenuEnabled (false);
    addAndMakeVisible (slider);

    attachment = std::make_unique<APVTS::SliderAttachment> (s, paramID, slider);
    if (param != nullptr)
        slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
}

void HudKnob::resized()
{
    auto b = getLocalBounds();
    const int k = jmin (b.getWidth() - 10, 58);
    slider.setBounds (b.getCentreX() - k / 2, 2, k, k);
}

void HudKnob::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float y = (float) slider.getBottom() + 3.0f;
    const bool hot = slider.isMouseOverOrDragging();

    g.setColour (hot ? col::text : col::textDim.brighter (0.25f));
    drawTracked (g, label.toUpperCase(), Fonts::get().label (13.0f), { 0, y, b.getWidth(), 14 }, Justification::centred, 0.12f);

    String v = param != nullptr ? param->getCurrentValueAsText() : String();
    if (suffix.isNotEmpty()) v << suffix;
    g.setColour (hot ? accent.brighter (0.4f) : accent.withAlpha (0.85f));
    g.setFont (Fonts::get().mono (12.5f));
    g.drawText (v, Rectangle<float> (0, y + 15.0f, b.getWidth(), 14), Justification::centred, false);
}

//==============================================================================
//  Buttons
//==============================================================================
HudToggle::HudToggle (const String& buttonText, Colour ac) : Button (buttonText), accent (ac)
{
    setClickingTogglesState (true);
}

void HudToggle::paintButton (Graphics& g, bool over, bool)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool on = getToggleState();
    auto frame = hudFrame (r, 5.0f);
    g.setColour (on ? accent.withAlpha (0.12f) : col::bg0.withAlpha (0.6f));
    g.fillPath (frame);
    g.setColour (on ? accent.withAlpha (over ? 0.9f : 0.6f) : col::lineHi.withAlpha (over ? 1.0f : 0.7f));
    g.strokePath (frame, PathStrokeType (1.0f));

    // LED
    const float d = 6.0f;
    auto led = Rectangle<float> (r.getX() + 8.0f, r.getCentreY() - d * 0.5f, d, d);
    if (on)
    {
        g.setColour (accent.withAlpha (0.25f));
        g.fillEllipse (led.expanded (3.0f));
    }
    g.setColour (on ? accent : col::textFaint);
    g.fillEllipse (led);

    g.setColour (on ? col::text : col::textDim);
    drawTracked (g, getButtonText(), Fonts::get().label (13.0f), r.withTrimmedLeft (18.0f), Justification::centred, 0.14f);
}

HudButton::HudButton (const String& buttonText, Colour ac, bool s) : Button (buttonText), accent (ac), solid (s) {}

void HudButton::paintButton (Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    auto frame = hudFrame (r, 6.0f);
    const float a = isEnabled() ? 1.0f : 0.35f;
    g.setColour (accent.withAlpha ((down ? 0.35f : over ? 0.2f : solid ? 0.1f : 0.04f) * a));
    g.fillPath (frame);
    g.setColour (accent.withAlpha ((over ? 1.0f : 0.7f) * a));
    g.strokePath (frame, PathStrokeType (over ? 1.4f : 1.0f));
    g.setColour ((over ? col::white : accent.brighter (0.3f)).withAlpha (a));
    drawTracked (g, getButtonText(), Fonts::get().display (10.5f), r, Justification::centred, 0.25f);
}

//==============================================================================
//  ModulePanel
//==============================================================================
ModulePanel::ModulePanel (const String& c, const String& t, const String& s, Colour a)
    : code (c), title (t), subtitle (s), accent (a)
{
    setInterceptsMouseClicks (false, true);
}

void ModulePanel::paintFrame (Graphics& g, Rectangle<float> r, const String& code, const String& title,
                              const String& subtitle, Colour accent)
{
    r = r.reduced (0.5f);
    const float cut = 12.0f;
    auto frame = hudFrame (r, cut);

    ColourGradient bg (col::panelHi.withAlpha (0.92f), r.getX(), r.getY(), col::panel.withAlpha (0.88f), r.getX(), r.getBottom(), false);
    g.setGradientFill (bg);
    g.fillPath (frame);

    // header band
    auto hdr = r.withHeight (headerH);
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (frame);
        ColourGradient hg (accent.withAlpha (0.13f), hdr.getX(), hdr.getY(), accent.withAlpha (0.0f), hdr.getRight() * 0.75f, hdr.getY(), false);
        g.setGradientFill (hg);
        g.fillRect (hdr);
        // fine diagonal texture in the header
        g.setColour (accent.withAlpha (0.05f));
        for (float x = hdr.getRight() - 90.0f; x < hdr.getRight(); x += 6.0f)
            g.drawLine (x, hdr.getBottom(), x + headerH, hdr.getY(), 1.0f);
    }
    g.setColour (col::line);
    g.drawHorizontalLine ((int) hdr.getBottom(), r.getX() + 1, r.getRight() - 1);
    g.setColour (accent.withAlpha (0.8f));
    g.fillRect (r.getX() + cut, hdr.getBottom() - 1.0f, 46.0f, 2.0f);

    g.setColour (col::line.brighter (0.15f));
    g.strokePath (frame, PathStrokeType (1.0f));

    // accent on the cut corner
    Path corner;
    corner.startNewSubPath (r.getX(), r.getY() + cut + 10.0f);
    corner.lineTo (r.getX(), r.getY() + cut);
    corner.lineTo (r.getX() + cut, r.getY());
    corner.lineTo (r.getX() + cut + 18.0f, r.getY());
    neonStroke (g, corner, accent, 1.6f, 0.8f);
    cornerBrackets (g, r.reduced (3.0f), col::lineHi);

    // texts
    g.setColour (accent);
    drawTracked (g, code, Fonts::get().display (11.0f), { r.getX() + cut + 4.0f, r.getY() + 1, 30, headerH }, Justification::centredLeft, 0.1f);
    g.setColour (col::textFaint.brighter (0.3f));
    g.fillRect (r.getX() + cut + 30.0f, r.getY() + 9.0f, 1.0f, headerH - 18.0f);
    g.setColour (col::text);
    drawTracked (g, title, Fonts::get().display (11.0f), { r.getX() + cut + 38.0f, r.getY() + 1, 220, headerH }, Justification::centredLeft, 0.22f);
    if (subtitle.isNotEmpty())
    {
        g.setColour (col::textDim);
        drawTracked (g, subtitle, Fonts::get().label (12.0f), { r.getX(), r.getY() + 1, r.getWidth() - 12.0f, headerH },
                     Justification::centredRight, 0.18f);
    }
}

void ModulePanel::paint (Graphics& g)
{
    paintFrame (g, getLocalBounds().toFloat(), code, title, headerComp != nullptr ? String() : subtitle, accent);
}

HudKnob& ModulePanel::addKnob (std::unique_ptr<HudKnob> k)
{
    addAndMakeVisible (*k);
    knobs.push_back (std::move (k));
    return *knobs.back();
}

void ModulePanel::resized()
{
    auto area = getLocalBounds().toFloat().withTrimmedTop (headerH + 6.0f).reduced (4.0f, 2.0f);
    if (headerComp != nullptr)
        headerComp->setBounds (getWidth() - 86, 5, 76, 18);

    // only visible knobs take a slot
    std::vector<HudKnob*> vis;
    for (auto& k : knobs) if (k->isVisible()) vis.push_back (k.get());
    const int rows = jmax (1, ((int) vis.size() + cols - 1) / cols);
    const float cw = area.getWidth() / (float) cols;
    const float ch = area.getHeight() / (float) rows;
    for (size_t i = 0; i < vis.size(); ++i)
    {
        const int cx = (int) i % cols, cy = (int) i / cols;
        vis[i]->setBounds (Rectangle<float> (area.getX() + cx * cw, area.getY() + cy * ch, cw, ch).toNearestInt());
    }
}

//==============================================================================
//  TimelineView
//==============================================================================
TimelineView::TimelineView (TzarProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
}

void TimelineView::refresh (float a)
{
    anim = a;
    auto& T = proc.telemetry;
    phase = T.phase.load();
    live = phase != 0;
    params = proc.snapshotParams();
    payloadLen = proc.getPayloadLengthSec();
    thumb = proc.getPayloadThumbnail();
    if (live)
    {
        elapsed = T.elapsed.load();
        tl.fall = T.tFall.load();        tl.impact = T.tImpact.load();   tl.blast = T.tBlast.load();
        tl.holdEnd = T.tHoldEnd.load();  tl.releaseEnd = T.tRelEnd.load(); tl.recoveryEnd = T.tRecEnd.load();
        tl.debrisStart = T.tDebStart.load(); tl.debrisEnd = T.tDebEnd.load(); tl.residueEnd = T.tResEnd.load();
        tl.tinStart = tl.blast;          tl.tinEnd = T.tTinEnd.load();   tl.total = T.tTotal.load();
        tl.blastEnd = tl.blast + std::max (6.0, payloadLen);
    }
    else
    {
        const double len = payloadLen / std::pow (2.0, params.samplePitch / 12.0);
        tl = Timeline::compute (params, len);
    }
    repaint();
}

static float mixGainDbAt (const Params& p, const Timeline& tl, float t)
{
    if (t < tl.fall)   return 0.0f;
    if (t < tl.impact) return -90.0f;
    const float ti = t - (float) tl.impact;
    float d = 0.0f;
    if (t < tl.holdEnd) d = 1.0f;
    else if (t < tl.releaseEnd)
        d = 0.5f * (1.0f + std::cos (MathConstants<float>::pi * (t - (float) tl.holdEnd) / (float) (tl.releaseEnd - tl.holdEnd)));
    float mod = 0.0f;
    if (d > 0.0f)
    {
        const float span = (float) std::max (0.01, tl.releaseEnd - tl.impact);
        const float pr = jlimit (0.0f, 1.0f, ti / span);
        const float ph = p.pressureHz * (ti - 0.35f * ti * ti / (2.0f * span));
        mod = p.pressureAmt * (0.5f - 0.5f * std::cos (MathConstants<float>::twoPi * ph)) * std::sqrt (1.0f - pr);
    }
    const float crushLin = tzar::dbToGain (p.crushDb);
    const float g = 1.0f - d * (1.0f - mod) * (1.0f - crushLin);
    float deafDb = 0.0f;
    if (p.deafHz < 19000.0f && t < tl.recoveryEnd)
    {
        if (t < tl.holdEnd) deafDb = -9.0f;
        else deafDb = -9.0f * (1.0f - std::pow ((t - (float) tl.holdEnd) / (float) std::max (0.01, tl.recoveryEnd - tl.holdEnd), 1.7f));
    }
    return Decibels::gainToDecibels (g, -90.0f) + deafDb;
}

void TimelineView::paint (Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    ModulePanel::paintFrame (g, full, "00", "SEQUENCE", "", col::cyan);

    const float total = (float) jmax (0.5, tl.total);
    const float impact = (float) tl.impact;

    // header right: live / preview chip + numbers
    {
        auto hr = Rectangle<float> (full.getRight() - 360.0f, 4.0f, 348.0f, 20.0f);
        g.setFont (Fonts::get().mono (12.0f));
        g.setColour (col::textDim);
        const String info = "IMPACT T+" + String (impact, 2) + "s   " + String::fromUTF8 ("Σ ") + String (total, 1) + "s";
        g.drawText (info, hr.withTrimmedRight (74.0f), Justification::centredRight, false);
        auto chip = hr.removeFromRight (64.0f);
        const Colour cc = live ? col::forPhase (phase) : col::textDim;
        g.setColour (cc.withAlpha (live ? 0.18f : 0.08f));
        g.fillRect (chip);
        g.setColour (cc.withAlpha (0.8f));
        g.drawRect (chip, 1.0f);
        if (live && std::fmod (anim, 0.8f) < 0.5f)
        {
            g.setColour (cc);
            g.fillEllipse (chip.getX() + 6.0f, chip.getCentreY() - 3.0f, 6.0f, 6.0f);
        }
        drawTracked (g, live ? "LIVE" : "PREVIEW", Fonts::get().label (12.0f), chip.withTrimmedLeft (live ? 10.0f : 0.0f), Justification::centred, 0.15f);
    }

    // geometry
    const float gutter = 76.0f;
    auto body  = full.withTrimmedTop (ModulePanel::headerH).reduced (10.0f, 6.0f);
    auto ruler = body.removeFromTop (20.0f);
    auto lanesArea = body.withTrimmedLeft (gutter);
    const float gx = lanesArea.getX(), gw = lanesArea.getWidth();
    auto X = [&] (double t) { return gx + (float) (t / total) * gw; };

    // phase strip under the ruler
    struct Seg { double a, b; Colour c; };
    const Seg segs[] = { { 0, tl.fall, col::descent }, { tl.fall, tl.impact, col::white },
                         { tl.impact, tl.releaseEnd, col::crush }, { tl.releaseEnd, std::max (tl.releaseEnd, tl.debrisEnd), col::debris },
                         { std::max (tl.releaseEnd, tl.debrisEnd), tl.total, col::ice } };
    for (auto& s : segs)
    {
        if (s.b <= s.a) continue;
        auto r = Rectangle<float> (X (s.a), ruler.getBottom() - 4.0f, X (s.b) - X (s.a) - 1.0f, 3.0f);
        g.setColour (s.c.withAlpha (0.75f));
        g.fillRect (r);
    }

    // ruler: time relative to impact (T-3 ... T0 ... T+8)
    {
        const float span = total;
        const float step = span > 40.0f ? 5.0f : span > 18.0f ? 2.0f : 1.0f;
        g.setFont (Fonts::get().mono (11.0f));
        for (float k = std::ceil (-impact / step) * step; k <= total - impact + 1e-3f; k += step)
        {
            const float x = X (impact + k);
            const bool zero = std::abs (k) < 1e-3f;
            g.setColour (zero ? col::red : col::textDim.withAlpha (0.7f));
            g.drawVerticalLine ((int) x, ruler.getBottom() - 9.0f, ruler.getBottom() - 5.0f);
            String lbl = zero ? "T0" : (k < 0 ? "T" + String ((int) std::round (k)) : "T+" + String ((int) std::round (k)));
            g.drawText (lbl, Rectangle<float> (x - 22.0f, ruler.getY() - 1.0f, 44.0f, 12.0f), Justification::centred, false);
        }
    }

    // lanes
    struct Lane { const char* name; Colour c; };
    const Lane lanes[] = { { "MIX", col::cyan }, { "WHISTLE", col::descent }, { "BLAST", col::blast },
                           { "DEBRIS", col::debris }, { "RESIDUE", col::crush }, { "TINNITUS", col::tinnitus } };
    const int nl = 6;
    const float lh = body.getHeight() / (float) nl;
    auto laneRect = [&] (int i) { return Rectangle<float> (gx, body.getY() + i * lh, gw, lh); };

    for (int i = 0; i < nl; ++i)
    {
        auto lr = laneRect (i);
        g.setColour ((i % 2 == 0 ? col::bg1 : col::panel).withAlpha (0.55f));
        g.fillRect (lr.reduced (0, 1.0f));
        g.setColour (col::line.withAlpha (0.7f));
        g.drawHorizontalLine ((int) lr.getBottom(), body.getX(), lr.getRight());
        // label
        auto lab = Rectangle<float> (body.getX(), lr.getY(), gutter - 8.0f, lh);
        g.setColour (lanes[i].c);
        g.fillRect (lab.getX(), lab.getCentreY() - 4.0f, 2.0f, 8.0f);
        g.setColour (col::textDim);
        drawTracked (g, lanes[i].name, Fonts::get().label (12.0f), lab.withTrimmedLeft (8.0f), Justification::centredLeft, 0.14f);
    }

    // vertical grid each second
    g.setColour (col::line.withAlpha (0.45f));
    for (float k = std::ceil (-impact); k <= total - impact; k += 1.0f)
        g.drawVerticalLine ((int) X (impact + k), body.getY(), body.getBottom());

    const float pxPerSec = gw / total;
    const int steps = (int) gw;

    // --- MIX lane: gain curve
    {
        auto lr = laneRect (0).reduced (0, 3.0f);
        auto Y = [&] (float db) { return lr.getBottom() - lr.getHeight() * jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f); };
        Path curve, fill;
        for (int s = 0; s <= steps; ++s)
        {
            const float t = total * (float) s / (float) steps;
            const float y = Y (mixGainDbAt (params, tl, t));
            const float x = gx + (float) s;
            if (s == 0) { curve.startNewSubPath (x, y); fill.startNewSubPath (x, lr.getBottom()); fill.lineTo (x, y); }
            else { curve.lineTo (x, y); fill.lineTo (x, y); }
        }
        fill.lineTo (gx + gw, lr.getBottom());
        fill.closeSubPath();
        g.setGradientFill (ColourGradient (col::cyan.withAlpha (0.22f), 0, lr.getY(), col::cyan.withAlpha (0.02f), 0, lr.getBottom(), false));
        g.fillPath (fill);
        g.setColour (col::cyan.withAlpha (0.9f));
        g.strokePath (curve, PathStrokeType (1.2f));

        // tension wedge during the fall (high-pass drain)
        if (params.tension > 0.01f && tl.fall > 0.0)
        {
            Path w;
            w.startNewSubPath (X (0), lr.getBottom());
            w.lineTo (X (tl.fall), lr.getBottom() - lr.getHeight() * 0.55f * params.tension);
            w.lineTo (X (tl.fall), lr.getBottom());
            w.closeSubPath();
            g.setColour (col::descent.withAlpha (0.25f));
            g.fillPath (w);
        }
        // deafness shading
        if (params.deafHz < 19000.0f)
        {
            const float x0 = X (tl.impact), x1 = X (tl.recoveryEnd);
            g.setGradientFill (ColourGradient (col::ice.withAlpha (0.22f), x0, 0, col::ice.withAlpha (0.0f), x1, 0, false));
            g.fillRect (Rectangle<float> (x0, lr.getY(), x1 - x0, lr.getHeight()));
        }
    }

    // --- WHISTLE lane: descending pitch on a log axis
    if (tl.fall > 0.0 && params.whistleDb > -99.0f)
    {
        auto lr = laneRect (1).reduced (0, 4.0f);
        auto Y = [&] (float f) { return lr.getBottom() - lr.getHeight() * (std::log (f / 60.0f) / std::log (6000.0f / 60.0f)); };
        Path p;
        const int n = jmax (2, (int) (X (tl.fall) - gx));
        for (int s = 0; s <= n; ++s)
        {
            const float pr = (float) s / (float) n;
            const float f = params.whistleStart * std::pow (params.whistleEnd / params.whistleStart, std::pow (pr, params.whistleCurve));
            const float x = gx + (float) s, y = Y (f);
            if (s == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        neonStroke (g, p, col::descent, 1.6f, 1.0f);
    }

    // --- BLAST lane
    {
        auto lr = laneRect (2).reduced (0, 3.0f);
        const float xb = X (tl.blast);
        if (! thumb.empty() && payloadLen > 0.0 && params.sampleDb > -99.0f)
        {
            const double len = payloadLen / std::pow (2.0, params.samplePitch / 12.0);
            const float w = jmax (2.0f, (float) (len * pxPerSec));
            g.setColour (col::blast.withAlpha (0.55f));
            const int nb = jmax (2, (int) w);
            for (int s = 0; s < nb; ++s)
            {
                const float v = thumb[(size_t) jmin ((int) thumb.size() - 1, s * (int) thumb.size() / nb)];
                const float h = v * lr.getHeight() * 0.5f;
                g.fillRect (xb + (float) s, lr.getCentreY() - h, 1.0f, jmax (1.0f, h * 2.0f));
            }
        }
        if (params.synthDb > -99.0f || params.subDb > -99.0f)
        {
            Path env;
            env.startNewSubPath (xb, lr.getBottom());
            for (float t = 0; t < 4.0f; t += 0.02f)
                env.lineTo (xb + t * pxPerSec, lr.getBottom() - lr.getHeight() * std::exp (-t / 0.6f));
            env.lineTo (xb + 4.0f * pxPerSec, lr.getBottom());
            env.closeSubPath();
            g.setColour (col::orange.withAlpha (0.25f));
            g.fillPath (env);
        }
        Path spike;
        spike.startNewSubPath (xb, lr.getBottom());
        spike.lineTo (xb, lr.getY());
        neonStroke (g, spike, col::white, 1.4f, 1.2f);
    }

    // --- DEBRIS lane: deterministic grain cloud
    if (params.debrisDb > -99.0f)
    {
        auto lr = laneRect (3).reduced (0, 4.0f);
        tzar::Rng r; r.s = 0x51A7u;
        const int n = jlimit (10, 260, (int) (params.density * params.debrisSec * 0.6f));
        for (int i = 0; i < n; ++i)
        {
            const float u = r.next01();
            const float pr = 1.0f - std::sqrt (1.0f - u * 0.999f);
            const float t = (float) tl.debrisStart + pr * params.debrisSec;
            const float yy = lr.getY() + r.next01() * lr.getHeight() * (0.3f + 0.7f * pr) + lr.getHeight() * 0.0f;
            const float sz = 1.2f + jlimit (0.0f, 3.0f, params.grainMs / 60.0f) * (0.5f + r.next01());
            const bool past = live && t < elapsed;
            g.setColour (col::debris.withAlpha ((past ? 0.95f : 0.55f) * (1.0f - 0.7f * pr)));
            g.fillEllipse (X (t) - sz * 0.5f, jmin (yy, lr.getBottom() - sz), sz, sz);
        }
    }

    // --- RESIDUE lane: slow bass wave
    if (params.residueDb > -99.0f)
    {
        auto lr = laneRect (4).reduced (0, 4.0f);
        const float span = (float) (tl.residueEnd - tl.impact);
        Path p;
        const int n = jmax (2, (int) (span * pxPerSec));
        for (int s = 0; s <= n; ++s)
        {
            const float ti = span * (float) s / (float) n;
            const float pr = ti / span;
            float env = jmin (1.0f, ti / 0.025f) * (1.0f + 0.9f * std::exp (-ti / 0.12f)) / 1.9f;
            if (pr > 0.45f) env *= 0.5f * (1.0f + std::cos (MathConstants<float>::pi * (pr - 0.45f) / 0.55f));
            const float y = lr.getCentreY() + std::sin (ti * 9.0f) * env * lr.getHeight() * 0.48f;
            const float x = X (tl.impact) + (float) s;
            if (s == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        neonStroke (g, p, col::crush.withAlpha (0.9f), 1.2f, 0.7f);
    }

    // --- TINNITUS lane
    if (params.tinDb > -99.0f)
    {
        auto lr = laneRect (5).reduced (0, 3.0f);
        const float y = lr.getBottom() - lr.getHeight() * jlimit (0.1f, 0.9f, std::log (params.tinHz / 1000.0f) / std::log (14.0f));
        const float span = (float) (tl.tinEnd - tl.blast);
        const int n = jmax (2, (int) (span * pxPerSec));
        for (int s = 0; s < n; ++s)
        {
            const float tt = span * (float) s / (float) n, pr = tt / span;
            float env = jlimit (0.0f, 1.0f, tt / 0.35f);
            if (pr > 0.22f) env *= std::pow ((1.0f - pr) / 0.78f, 2.2f);
            g.setColour (col::ice.withAlpha (0.15f + 0.8f * env));
            const float h = 0.8f + env * 3.5f;
            g.fillRect (X (tl.blast) + (float) s, y - h * 0.5f, 1.0f, h);
        }
    }

    // --- markers
    {
        auto lanesAll = Rectangle<float> (gx, body.getY(), gw, body.getHeight());
        Path imp;
        imp.startNewSubPath (X (impact), lanesAll.getY());
        imp.lineTo (X (impact), lanesAll.getBottom());
        neonStroke (g, imp, col::red.withAlpha (0.85f), 1.2f, 1.0f);
        g.setColour (col::red);
        g.setFont (Fonts::get().label (11.0f));
        g.drawText ("IMPACT", Rectangle<float> (X (impact) + 4.0f, lanesAll.getY() + 1.0f, 60.0f, 11.0f), Justification::centredLeft, false);

        // dashed detonate line
        g.setColour (col::cyan.withAlpha (0.5f));
        const float dash[] = { 3.0f, 3.0f };
        g.drawDashedLine (Line<float> (gx + 0.5f, lanesAll.getY(), gx + 0.5f, lanesAll.getBottom()), dash, 2, 1.0f);
    }

    // --- playhead
    if (live)
    {
        const float xp = X (elapsed);
        auto lanesAll = Rectangle<float> (gx, body.getY(), gw, body.getHeight());
        // dim the future
        g.setColour (col::bg0.withAlpha (0.35f));
        g.fillRect (Rectangle<float> (xp, lanesAll.getY(), lanesAll.getRight() - xp, lanesAll.getHeight()));
        // scan trail
        g.setGradientFill (ColourGradient (col::forPhase (phase).withAlpha (0.0f), xp - 60.0f, 0,
                                           col::forPhase (phase).withAlpha (0.18f), xp, 0, false));
        g.fillRect (Rectangle<float> (xp - 60.0f, lanesAll.getY(), 60.0f, lanesAll.getHeight()));
        Path ph;
        ph.startNewSubPath (xp, ruler.getY() + 10.0f);
        ph.lineTo (xp, lanesAll.getBottom());
        neonStroke (g, ph, col::white, 1.4f, 1.6f);
        Path tri;
        tri.addTriangle (xp - 5.0f, ruler.getY() + 6.0f, xp + 5.0f, ruler.getY() + 6.0f, xp, ruler.getY() + 12.0f);
        g.setColour (col::white);
        g.fillPath (tri);
    }
}

//==============================================================================
//  DetonateButton
//==============================================================================
DetonateButton::DetonateButton()
{
    setRepaintsOnMouseActivity (false);
    setMouseCursor (MouseCursor::PointingHandCursor);
}

void DetonateButton::setState (int ph, float el, float tot, float imp, float pHz, float a)
{
    phase = ph; elapsed = el; total = jmax (0.1f, tot); impact = imp; pressureHz = pHz; anim = a;
    repaint();
}

bool DetonateButton::overCore (Point<float> p) const
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float R = jmin (getWidth(), getHeight()) * 0.5f - 2.0f;
    return p.getDistanceFrom (c) <= R * 0.74f;
}

bool DetonateButton::hitTest (int x, int y) { return overCore ({ (float) x, (float) y }); }
void DetonateButton::mouseMove (const MouseEvent& e) { const bool h = overCore (e.position); if (h != hover) { hover = h; repaint(); } }
void DetonateButton::mouseExit (const MouseEvent&)   { hover = false; pressed = false; repaint(); }
void DetonateButton::mouseDown (const MouseEvent& e) { if (overCore (e.position)) { pressed = true; repaint(); } }
void DetonateButton::mouseUp (const MouseEvent& e)
{
    const bool fire = pressed && overCore (e.position);
    pressed = false;
    repaint();
    if (fire && onFire) onFire();
}

void DetonateButton::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float R = jmin (b.getWidth(), b.getHeight()) * 0.5f - 2.0f;
    const bool active = phase != 0;
    const Colour pc = col::forPhase (phase);
    const float twoPi = MathConstants<float>::twoPi;

    // pulse
    float pulse = 0.5f + 0.5f * std::sin (anim * (active ? 6.0f : 2.2f));
    if (phase == 3) pulse = 0.5f + 0.5f * std::sin ((elapsed - impact) * pressureHz * twoPi);

    // --- halo
    {
        const float hr = R * 1.0f;
        const float a = active ? 0.20f + 0.15f * pulse : (hover ? 0.22f : 0.10f + 0.04f * pulse);
        ColourGradient halo ((active ? pc : col::red).withAlpha (a), c.x, c.y, Colours::transparentBlack, c.x + hr, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (c.x - hr, c.y - hr, hr * 2, hr * 2);
    }

    // --- ring A: rotating dashes
    {
        const int n = 48;
        const float rot = anim * (active ? 1.2f : 0.15f);
        g.setColour ((active ? pc : col::red).withAlpha (0.55f));
        for (int i = 0; i < n; ++i)
        {
            if (i % 4 == 3) continue;
            const float a0 = rot + twoPi * i / n, a1 = a0 + twoPi / n * 0.55f;
            Path seg;
            seg.addCentredArc (c.x, c.y, R - 1.0f, R - 1.0f, 0.0f, a0, a1, true);
            g.strokePath (seg, PathStrokeType (1.6f));
        }
    }

    // --- ring B: ticks, lit with progress
    {
        const int n = 72;
        const float prog = active ? jlimit (0.0f, 1.0f, elapsed / total) : 0.0f;
        for (int i = 0; i < n; ++i)
        {
            const float tp = (float) i / n;
            const float a = twoPi * tp;
            const bool lit = tp < prog;
            const bool major = i % 6 == 0;
            const float r1 = R * (major ? 0.83f : 0.86f), r2 = R * 0.90f;
            g.setColour (lit ? pc.withAlpha (0.95f) : col::lineHi.withAlpha (major ? 0.9f : 0.5f));
            g.drawLine (c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), c.x + r2 * std::sin (a), c.y - r2 * std::cos (a), major ? 1.5f : 1.0f);
        }
        // impact mark on the dial
        if (active)
        {
            const float ai = twoPi * jlimit (0.0f, 1.0f, impact / total);
            g.setColour (col::red);
            g.drawLine (c.x + R * 0.80f * std::sin (ai), c.y - R * 0.80f * std::cos (ai),
                        c.x + R * 0.95f * std::sin (ai), c.y - R * 0.95f * std::cos (ai), 2.5f);
        }
    }

    // --- hazard annulus (amber / black), slowly turning
    {
        const float ro = R * 0.76f, ri = R * 0.64f;
        const int n = 28;
        const float rot = anim * (active ? 0.9f : (hover ? 0.4f : 0.08f));
        Path ring;
        ring.addEllipse (c.x - ro, c.y - ro, ro * 2, ro * 2);
        ring.addEllipse (c.x - ri, c.y - ri, ri * 2, ri * 2);
        ring.setUsingNonZeroWinding (false);
        g.setColour (Colour (0xff0b0b0b));
        g.fillPath (ring);
        for (int i = 0; i < n; i += 2)
        {
            const float a0 = rot + twoPi * i / n, a1 = a0 + twoPi / n;
            const float sk = 0.12f;   // skew to make diagonal stripes
            Path s;
            s.startNewSubPath (c.x + ri * std::sin (a0), c.y - ri * std::cos (a0));
            s.lineTo (c.x + ro * std::sin (a0 + sk), c.y - ro * std::cos (a0 + sk));
            s.lineTo (c.x + ro * std::sin (a1 + sk), c.y - ro * std::cos (a1 + sk));
            s.lineTo (c.x + ri * std::sin (a1), c.y - ri * std::cos (a1));
            s.closeSubPath();
            g.setColour (col::amber.withAlpha (active || hover ? 0.95f : 0.78f));
            g.fillPath (s);
        }
        g.setColour (Colours::black.withAlpha (0.8f));
        g.drawEllipse (c.x - ro, c.y - ro, ro * 2, ro * 2, 1.5f);
        g.drawEllipse (c.x - ri, c.y - ri, ri * 2, ri * 2, 1.5f);
    }

    // --- core button
    {
        const float r = R * 0.60f;
        const float press = pressed ? 0.6f : 1.0f;
        Colour hi = col::red.brighter (0.35f), lo = Colour (0xff3a0008);
        if (phase == 3)      { hi = col::white.interpolatedWith (col::red, 1.0f - pulse); lo = Colour (0xff5a0010); }
        else if (phase == 2) { hi = col::white; lo = Colour (0xff202430); }
        else if (active)     { hi = pc.interpolatedWith (col::red, 0.35f).brighter (0.2f * pulse); lo = pc.darker (3.0f); }
        else if (hover)      { hi = hi.brighter (0.2f); }

        ColourGradient grad (hi.withMultipliedBrightness (press), c.x - r * 0.35f, c.y - r * 0.45f,
                             lo, c.x + r * 0.4f, c.y + r * 0.9f, true);
        g.setGradientFill (grad);
        g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);

        // rim
        g.setColour (Colours::black.withAlpha (0.7f));
        g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 2.0f);
        g.setColour (Colours::white.withAlpha (pressed ? 0.05f : 0.18f));
        g.drawEllipse (c.x - r + 3, c.y - r + 3, r * 2 - 6, r * 2 - 6, 1.0f);

        // gloss
        ColourGradient gloss (Colours::white.withAlpha (pressed ? 0.05f : 0.28f), c.x, c.y - r,
                              Colours::white.withAlpha (0.0f), c.x, c.y, false);
        g.setGradientFill (gloss);
        g.fillEllipse (c.x - r * 0.72f, c.y - r * 0.92f, r * 1.44f, r * 0.95f);

        // trefoil
        auto tf = trefoil ({ c.x, c.y - r * 0.08f }, r * 0.17f, r * 0.62f, r * 0.11f,
                           MathConstants<float>::pi + (active ? anim * 0.6f : 0.0f));
        g.setColour (Colours::black.withAlpha (phase == 2 ? 0.85f : 0.62f));
        g.fillPath (tf);

        // label
        g.setColour (Colours::white.withAlpha (0.85f));
        drawTracked (g, active ? phaseName (phase) : "DETONATE", Fonts::get().display (8.5f),
                     Rectangle<float> (c.x - r, c.y + r * 0.56f, r * 2, 12.0f), Justification::centred, 0.25f);
    }
}

//==============================================================================
//  IgnitionPanel
//==============================================================================
IgnitionPanel::IgnitionPanel()
{
    addAndMakeVisible (detonate);
    addAndMakeVisible (abort);
    detonate.setTooltip (String::fromUTF8 ("DETONATE : lance la séquence complète. Aussi déclenchable par automation (paramètre DETONATE) ou par n'importe quelle note MIDI."));
    abort.setTooltip (String (CharPointer_UTF8 ("ABORT : coupe la séquence en cours (fondu de 30 ms), le mix revient immédiatement.")));
}

void IgnitionPanel::update (int ph, float el, float tot, float imp, int gr, float pHz, float a)
{
    phase = ph; elapsed = el; total = tot; impact = imp; grains = gr; anim = a;
    detonate.setState (ph, el, tot, imp, pHz, a);
    abort.setEnabled (ph != 0);
    repaint (getLocalBounds().withTrimmedLeft (190));
}

void IgnitionPanel::resized()
{
    const int s = jmin (getHeight() - 40, 192);
    detonate.setBounds (8, 32 + (getHeight() - 32 - s) / 2, s, s);
    abort.setBounds (getWidth() - 112, getHeight() - 40, 100, 26);
}

void IgnitionPanel::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    ModulePanel::paintFrame (g, b, "!!", "IGNITION", "", col::red);

    // hazard stripe under the header (right part)
    {
        auto hz = Rectangle<float> (b.getRight() - 120.0f, 9.0f, 108.0f, 10.0f);
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (hz.toNearestInt());
        g.setColour (Colour (0xff0b0b0b));
        g.fillRect (hz);
        g.setColour (col::amber.withAlpha (0.85f));
        const float off = std::fmod (anim * (phase != 0 ? 40.0f : 6.0f), 12.0f);
        for (float x = hz.getX() - 20.0f + off; x < hz.getRight() + 12.0f; x += 12.0f)
        {
            Path p;
            p.addQuadrilateral (x, hz.getBottom(), x + 6.0f, hz.getBottom(), x + 16.0f, hz.getY(), x + 10.0f, hz.getY());
            g.fillPath (p);
        }
    }

    // readouts
    const float x = 206.0f, w = b.getWidth() - x - 10.0f;
    const Colour pc = col::forPhase (phase);
    float y = 38.0f;

    auto lbl = [&] (const String& s)
    {
        g.setColour (col::textDim);
        drawTracked (g, s, Fonts::get().label (12.0f), { x, y, w, 13 }, Justification::centredLeft, 0.2f);
        y += 14.0f;
    };

    lbl ("STATUS");
    {
        const bool blink = phase != 0 && std::fmod (anim, 0.6f) < 0.35f;
        g.setColour (pc.withAlpha (blink || phase == 0 ? 1.0f : 0.35f));
        g.fillEllipse (x, y + 5.0f, 7.0f, 7.0f);
        g.setColour (pc);
        drawTracked (g, phaseName (phase), Fonts::get().display (13.0f), { x + 13.0f, y, w - 13.0f, 18 }, Justification::centredLeft, 0.12f);
        y += 28.0f;
    }

    lbl ("T-ZERO");
    {
        String t;
        if (phase == 0) t = "--.---";
        else
        {
            const float d = elapsed - impact;
            t = (d < 0 ? "T-" : "T+") + String (std::abs (d), 3);
        }
        g.setColour (phase == 0 ? col::textFaint.brighter (0.4f) : col::white);
        g.setFont (Fonts::get().mono (24.0f));
        g.drawText (t, Rectangle<float> (x, y, w, 26), Justification::centredLeft, false);
        y += 34.0f;
    }

    lbl (phase >= 3 ? "FRAGMENTS" : "SEQUENCE");
    {
        g.setFont (Fonts::get().mono (15.0f));
        g.setColour (pc.withAlpha (0.9f));
        String s = phase >= 3 ? String (grains).paddedLeft ('0', 3) + " ACTIVE"
                              : String (total, 2) + " s";
        g.drawText (s, Rectangle<float> (x, y, w, 18), Justification::centredLeft, false);
    }
}

//==============================================================================
//  ScopeView
//==============================================================================
ScopeView::ScopeView (TzarProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }

void ScopeView::refresh (int ph, float a)
{
    phase = ph; anim = a;
    auto& T = proc.telemetry;
    const int w = T.scopeWrite.load (std::memory_order_acquire);
    const int n = TzarProcessor::Telemetry::scopeSize;
    for (int i = 0; i < n; ++i)
        data[(size_t) i] = T.scope[(size_t) ((w + i) % n)].load (std::memory_order_relaxed);
    repaint();
}

void ScopeView::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    ModulePanel::paintFrame (g, b, "06", "TELEMETRY", String::fromUTF8 ("OUTPUT · SCOPE"), col::cyan);
    auto r = b.withTrimmedTop (ModulePanel::headerH + 8.0f).reduced (12.0f, 6.0f).withTrimmedBottom (4.0f);

    g.setColour (col::bg0.withAlpha (0.65f));
    g.fillRect (r);
    g.setColour (col::line.withAlpha (0.8f));
    for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 8.0f), r.getY(), r.getBottom());
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * i / 4.0f), r.getX(), r.getRight());
    g.setColour (col::lineHi);
    g.drawRect (r, 1.0f);
    cornerBrackets (g, r.expanded (3.0f), col::cyan.withAlpha (0.6f), 6.0f, 1.0f);

    const Colour pc = phase == 0 ? col::cyan : col::forPhase (phase);
    const int n = (int) data.size();
    Path p;
    for (int i = 0; i < n; ++i)
    {
        const float x = r.getX() + r.getWidth() * (float) i / (float) (n - 1);
        const float y = r.getCentreY() - jlimit (-1.0f, 1.0f, data[(size_t) i]) * r.getHeight() * 0.46f;
        if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
    }
    {
        Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (r.toNearestInt());
        neonStroke (g, p, pc, 1.2f, 1.3f);
        // sweeping scan line
        const float sx = r.getX() + std::fmod (anim * 0.5f, 1.0f) * r.getWidth();
        g.setGradientFill (ColourGradient (pc.withAlpha (0.0f), sx - 40.0f, 0, pc.withAlpha (0.12f), sx, 0, false));
        g.fillRect (Rectangle<float> (sx - 40.0f, r.getY(), 40.0f, r.getHeight()));
    }
    g.setColour (col::textDim);
    g.setFont (Fonts::get().mono (10.5f));
    g.drawText ("43 ms", r.reduced (5.0f, 3.0f), Justification::bottomRight, false);
    g.drawText ("+1.0", r.reduced (5.0f, 3.0f), Justification::topLeft, false);
}

//==============================================================================
//  Meter
//==============================================================================
void MeterView::setLevels (float l, float r)
{
    const float in[2] { l, r };
    for (int c = 0; c < 2; ++c)
    {
        const float db = jlimit (-60.0f, 6.0f, Decibels::gainToDecibels (in[c], -60.0f));
        const float v = (db + 60.0f) / 66.0f;
        lvl[c] = v > lvl[c] ? v : lvl[c] - 0.018f;
        if (v >= hold[c]) { hold[c] = v; holdCount[c] = 45; }
        else if (--holdCount[c] < 0) hold[c] = jmax (0.0f, hold[c] - 0.01f);
    }
    repaint();
}

void MeterView::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float bw = (b.getWidth() - 6.0f) / 2.0f;
    const int segs = 30;
    for (int c = 0; c < 2; ++c)
    {
        auto r = Rectangle<float> (b.getX() + c * (bw + 6.0f), b.getY(), bw, b.getHeight() - 14.0f);
        const float sh = r.getHeight() / segs;
        for (int s = 0; s < segs; ++s)
        {
            const float v = (float) (s + 1) / segs;
            const Colour sc = v > 0.94f ? col::red : v > 0.82f ? col::amber : col::sys;
            const bool on = v <= lvl[c] + 1e-3f;
            const bool pk = std::abs (v - hold[c]) < 0.5f / segs;
            g.setColour (on || pk ? sc : sc.withAlpha (0.08f));
            g.fillRect (r.getX(), r.getBottom() - (s + 1) * sh + 1.0f, r.getWidth(), sh - 1.5f);
        }
        g.setColour (col::textDim);
        g.setFont (Fonts::get().label (11.0f));
        g.drawText (c == 0 ? "L" : "R", Rectangle<float> (r.getX(), r.getBottom() + 1.0f, r.getWidth(), 12.0f), Justification::centred, false);
    }
}

//==============================================================================
//  OutputPanel
//==============================================================================
OutputPanel::OutputPanel (APVTS& s) : ModulePanel ("SYS", "OUTPUT", "MASTER", col::sys)
{
    setColumns (2);
    addAndMakeVisible (limiter);
    addAndMakeVisible (autoReset);
    addAndMakeVisible (meter);
    limiter.setTooltip (String (CharPointer_UTF8 ("LIMIT : soft-clipper de sécurité en sortie (transparent sous -1.4 dBFS). Le blast peut être très fort.")));
    autoReset.setTooltip (String (CharPointer_UTF8 ("AUTO-RESET : si tu stoppes la lecture dans FL, la séquence est annulée et le mix revient.")));
    aLim   = std::make_unique<APVTS::ButtonAttachment> (s, pid::limiter, limiter);
    aReset = std::make_unique<APVTS::ButtonAttachment> (s, pid::stopReset, autoReset);
}

void OutputPanel::resized()
{
    auto area = getLocalBounds().withTrimmedTop ((int) headerH + 6).reduced (6, 4);
    auto meterR = area.removeFromRight (40);
    meter.setBounds (meterR.withTrimmedTop (4).withTrimmedBottom (2));
    area.removeFromRight (8);
    auto tog = area.removeFromRight (96);
    limiter.setBounds (tog.getX(), tog.getY() + 14, tog.getWidth(), 24);
    autoReset.setBounds (tog.getX(), tog.getY() + 46, tog.getWidth(), 24);
    const int kw = area.getWidth() / 2;
    for (size_t i = 0; i < knobs.size(); ++i)
        knobs[i]->setBounds (area.getX() + (int) i * kw, area.getY(), kw, area.getHeight());
}

void OutputPanel::paint (Graphics& g)
{
    ModulePanel::paint (g);
    auto tog = limiter.getBounds().toFloat();
    g.setColour (col::textFaint.brighter (0.3f));
    g.setFont (Fonts::get().mono (10.5f));
    g.drawText ("TZAR v1.0  //  AN602", Rectangle<float> (tog.getX() - 4.0f, (float) autoReset.getBottom() + 14.0f, tog.getWidth() + 10.0f, 12.0f),
                Justification::centred, false);
}

//==============================================================================
//  PayloadSlot
//==============================================================================
PayloadSlot::PayloadSlot()
{
    addAndMakeVisible (load);
    addAndMakeVisible (clear);
    load.setTooltip (String (CharPointer_UTF8 ("Charge ton sample d'impact (WAV/AIFF/FLAC/MP3). Tu peux aussi le glisser-déposer n'importe où sur TZAR.")));
    clear.setTooltip (String::fromUTF8 ("Retire le sample (le blast synthétique reste actif)."));
}

void PayloadSlot::update (const String& n, std::vector<float> th, double len)
{
    if (n != name || th.size() != thumb.size() || std::abs (len - lenSec) > 1e-6)
    {
        name = n; thumb = std::move (th); lenSec = len;
        clear.setEnabled (name.isNotEmpty());
        repaint();
    }
}

void PayloadSlot::resized()
{
    load.setBounds (getWidth() - 160, getHeight() - 38, 70, 26);
    clear.setBounds (getWidth() - 84, getHeight() - 38, 72, 26);
}

void PayloadSlot::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    ModulePanel::paintFrame (g, b, "PL", "PAYLOAD", "IMPACT SAMPLE", col::blast);
    auto wave = Rectangle<float> (12.0f, ModulePanel::headerH + 10.0f, b.getWidth() - 24.0f, b.getHeight() - ModulePanel::headerH - 58.0f);

    g.setColour (col::bg0.withAlpha (0.65f));
    g.fillRect (wave);

    if (thumb.empty())
    {
        g.setColour ((dragHover ? col::blast : col::lineHi).withAlpha (dragHover ? 0.9f : 0.8f));
        const float dash[] = { 4.0f, 4.0f };
        Path pth; pth.addRectangle (wave.reduced (1.0f));
        PathStrokeType (1.0f).createDashedStroke (pth, pth, dash, 2);
        g.fillPath (pth);
        g.setColour (dragHover ? col::blast : col::textDim);
        drawTracked (g, dragHover ? String ("RELEASE TO ARM PAYLOAD") : String::fromUTF8 ("DROP IMPACT SAMPLE  ·  WAV / AIFF / FLAC"),
                     Fonts::get().label (14.0f), wave, Justification::centred, 0.14f);
    }
    else
    {
        const int n = (int) wave.getWidth() / 3;
        for (int i = 0; i < n; ++i)
        {
            const float v = thumb[(size_t) jmin ((int) thumb.size() - 1, i * (int) thumb.size() / n)];
            const float h = jmax (1.0f, v * wave.getHeight() * 0.46f);
            g.setColour (col::blast.withAlpha (0.35f + 0.6f * v));
            g.fillRect (wave.getX() + 2.0f + i * 3.0f, wave.getCentreY() - h, 2.0f, h * 2.0f);
        }
        g.setColour (col::blast.withAlpha (dragHover ? 1.0f : 0.5f));
        g.drawRect (wave, 1.0f);
    }
    cornerBrackets (g, wave.expanded (3.0f), col::blast.withAlpha (0.6f), 6.0f, 1.0f);

    // name + length
    auto info = Rectangle<float> (14.0f, b.getHeight() - 40.0f, b.getWidth() - 190.0f, 28.0f);
    g.setColour (name.isEmpty() ? col::textDim : col::text);
    g.setFont (Fonts::get().mono (13.0f));
    g.drawText (name.isEmpty() ? String ("NO PAYLOAD  //  SYNTH BLAST ONLY") : name.toUpperCase(),
                info.removeFromTop (15.0f), Justification::centredLeft, true);
    g.setColour (col::textDim);
    g.setFont (Fonts::get().mono (11.0f));
    if (name.isNotEmpty())
        g.drawText (String (lenSec, 2) + " s  //  ARMED", info, Justification::centredLeft, false);
}

//==============================================================================
//  HeaderBar
//==============================================================================
void HeaderBar::update (float b, double s, int ph, float a)
{
    bpm = b; sr = s; phase = ph; anim = a;
    repaint (getLocalBounds().withTrimmedLeft (getWidth() - 420));
    repaint (getLocalBounds().withWidth (70));
}

void HeaderBar::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    // bottom rule
    g.setGradientFill (ColourGradient (col::cyan.withAlpha (0.7f), 0, 0, col::cyan.withAlpha (0.0f), b.getWidth() * 0.6f, 0, false));
    g.fillRect (16.0f, b.getBottom() - 1.0f, b.getWidth() - 32.0f, 1.0f);
    g.setColour (col::cyan);
    g.fillRect (16.0f, b.getBottom() - 2.5f, 64.0f, 2.5f);

    // emblem
    {
        const Point<float> c (40.0f, b.getCentreY());
        const float r = 19.0f;
        const float pulse = 0.5f + 0.5f * std::sin (anim * (phase != 0 ? 8.0f : 1.6f));
        ColourGradient halo (col::red.withAlpha (0.25f + 0.2f * pulse), c.x, c.y, Colours::transparentBlack, c.x + r * 1.8f, c.y, true);
        g.setGradientFill (halo);
        g.fillEllipse (c.x - r * 1.8f, c.y - r * 1.8f, r * 3.6f, r * 3.6f);
        Path ring; ring.addEllipse (c.x - r, c.y - r, r * 2, r * 2);
        neonStroke (g, ring, col::red, 1.5f, 1.0f);
        g.setColour (col::red);
        g.fillPath (trefoil (c, r * 0.22f, r * 0.8f, r * 0.14f, MathConstants<float>::pi + anim * (phase != 0 ? 1.5f : 0.15f)));
    }

    // logo
    {
        const auto f = Fonts::get().display (38.0f).withExtraKerningFactor (0.32f);
        GlyphArrangement ga;
        ga.addLineOfText (f, "TZAR", 76.0f, b.getCentreY() + 14.0f);
        Path p;
        ga.createPath (p);
        g.setColour (col::cyan.withAlpha (0.10f));
        g.strokePath (p, PathStrokeType (7.0f));
        g.setColour (col::cyan.withAlpha (0.18f));
        g.strokePath (p, PathStrokeType (3.0f));
        const auto pb = p.getBounds();
        g.setGradientFill (ColourGradient (col::white, pb.getX(), pb.getY(), col::cyan, pb.getX(), pb.getBottom(), false));
        g.fillPath (p);
        g.setColour (col::white.withAlpha (0.9f));
        g.strokePath (p, PathStrokeType (0.8f));

        const float sx = pb.getRight() + 20.0f;
        g.setColour (col::textFaint.brighter (0.5f));
        g.fillRect (sx - 10.0f, b.getCentreY() - 14.0f, 1.0f, 28.0f);
        g.setColour (col::text.withAlpha (0.85f));
        drawTracked (g, "THERMONUCLEAR", Fonts::get().label (14.0f), { sx, b.getCentreY() - 16.0f, 200, 16 }, Justification::centredLeft, 0.38f);
        g.setColour (col::textDim);
        drawTracked (g, "SEQUENCE ENGINE", Fonts::get().label (14.0f), { sx, b.getCentreY(), 200, 16 }, Justification::centredLeft, 0.38f);
    }

    // centre decoration
    {
        const float cx = b.getWidth() * 0.5f + 40.0f;
        g.setColour (col::textFaint);
        for (int i = 0; i < 14; ++i)
            g.fillRect (cx - 70.0f + i * 10.0f, b.getCentreY() - 1.0f, 6.0f, 2.0f);
        g.setColour (col::textDim.withAlpha (0.8f));
        g.setFont (Fonts::get().mono (10.5f));
        g.drawText ("AN602 // 50 MT CLASS", Rectangle<float> (cx - 80.0f, b.getCentreY() + 4.0f, 160.0f, 12.0f), Justification::centred, false);
    }

    // status chips (right)
    {
        float x = b.getWidth() - 16.0f;
        auto chip = [&] (const String& k, const String& v, Colour c, float w, bool led)
        {
            x -= w;
            auto r = Rectangle<float> (x, b.getCentreY() - 15.0f, w, 30.0f);
            auto fr = hudFrame (r, 6.0f);
            g.setColour (c.withAlpha (0.07f));
            g.fillPath (fr);
            g.setColour (c.withAlpha (0.5f));
            g.strokePath (fr, PathStrokeType (1.0f));
            float tx = r.getX() + 10.0f;
            if (led)
            {
                const bool blink = phase != 0 && std::fmod (anim, 0.5f) < 0.25f;
                g.setColour (c.withAlpha (blink ? 0.3f : 1.0f));
                g.fillEllipse (tx, r.getCentreY() - 3.5f, 7.0f, 7.0f);
                tx += 13.0f;
            }
            g.setColour (col::textDim);
            drawTracked (g, k, Fonts::get().label (12.0f), { tx, r.getY(), 40.0f, r.getHeight() }, Justification::centredLeft, 0.15f);
            g.setColour (c);
            g.setFont (Fonts::get().mono (14.0f));
            g.drawText (v, r.withTrimmedRight (10.0f), Justification::centredRight, false);
            x -= 8.0f;
        };
        chip ("SYS", phaseName (phase), col::forPhase (phase), 148.0f, true);
        chip ("SR", String (sr / 1000.0, 1) + "k", col::text, 96.0f, false);
        chip ("BPM", String (bpm, 1), col::text, 104.0f, false);
    }
}

} // namespace tzar::ui
