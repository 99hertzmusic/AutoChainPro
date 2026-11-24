// AutoChain (Pro build) – UI (99Hertz hardware look: brushed panel, knurled dials, lit keys, LED screens)
#include "PluginEditor.h"
#include "BinaryData.h"
#include <cmath>

using ACP = AutoChainProAudioProcessor;

namespace Pal
{
    static const juce::Colour panelTop  { 0xff202124 };
    static const juce::Colour panelBot  { 0xff141517 };
    static const juce::Colour screen    { 0xff060707 };
    static const juce::Colour silk      { 0xffe4e5e7 };
    static const juce::Colour silkMid   { 0xffb4b7bc };
    static const juce::Colour silkLo    { 0xff7d8086 };
    static const juce::Colour lampOff   { 0xff3a3b3f };
    static const juce::Colour accent    { 0xff4fb2ff };   // AutoChain's colour
    static const juce::Colour warn      { 0xffff9a2e };
    static const juce::Colour kick      { 0xfff4f1e6 };
}

// ── Fonts (embedded, OFL: Barlow Condensed + Share Tech Mono) ────────────────
namespace Fonts
{
    static juce::Typeface::Ptr load (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }
    static juce::Typeface::Ptr bold()  { static auto t = load (BinaryData::BarlowCondensedBold_ttf,     BinaryData::BarlowCondensedBold_ttfSize);     return t; }
    static juce::Typeface::Ptr semi()  { static auto t = load (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize); return t; }
    static juce::Typeface::Ptr med()   { static auto t = load (BinaryData::BarlowCondensedMedium_ttf,   BinaryData::BarlowCondensedMedium_ttfSize);   return t; }
    static juce::Typeface::Ptr mono()  { static auto t = load (BinaryData::ShareTechMonoRegular_ttf,    BinaryData::ShareTechMonoRegular_ttfSize);    return t; }

    // size ≈ CSS px, spacing = letter-spacing in em
    static juce::Font make (juce::Typeface::Ptr t, float size, float spacing = 0.0f)
    {
        return juce::Font (juce::FontOptions (t).withPointHeight (size).withKerningFactor (spacing));
    }
    static juce::Font silkBold (float s, float sp = 0.2f) { return make (bold(), s, sp); }
    static juce::Font silkSemi (float s, float sp = 0.16f) { return make (semi(), s, sp); }
    static juce::Font silkMed  (float s, float sp = 0.14f) { return make (med(), s, sp); }
    static juce::Font led      (float s, float sp = 0.06f) { return make (mono(), s, sp); }
}

static void glowText (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, juce::Justification j, juce::Colour c)
{
    g.setColour (c.withAlpha (0.18f));
    for (auto d : { juce::Point<float> (-1.0f, 0.0f), { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } })
        g.drawText (t, r.translated (d.x, d.y), j, false);
    g.setColour (c);
    g.drawText (t, r, j, false);
}

static void glowPath (juce::Graphics& g, const juce::Path& p, juce::Colour c, float w)
{
    g.setColour (c.withAlpha (0.12f)); g.strokePath (p, juce::PathStrokeType (w * 4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c.withAlpha (0.25f)); g.strokePath (p, juce::PathStrokeType (w * 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (c);                   g.strokePath (p, juce::PathStrokeType (w,        juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// Recessed LED screen: black glass, inner shadow, a faint lip of light under it.
static void drawScreen (juce::Graphics& g, juce::Rectangle<float> r, float rad = 5.0f)
{
    g.setColour (juce::Colours::white.withAlpha (0.09f));
    g.fillRoundedRectangle (r.translated (0, 1.0f), rad);
    g.setColour (Pal::screen);
    g.fillRoundedRectangle (r, rad);
    g.setGradientFill (juce::ColourGradient (juce::Colours::black, 0, r.getY(), juce::Colours::transparentBlack, 0, r.getY() + 10.0f, false));
    g.fillRoundedRectangle (r, rad);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, rad, 1.0f);
}

static void drawScrew (juce::Graphics& g, juce::Point<float> c, float angle)
{
    const float R = 7.0f;
    juce::ColourGradient gr (juce::Colour (0xff7a7c81), c.x - 1.5f, c.y - 2.5f, juce::Colour (0xff1c1d1f), c.x + R, c.y + R, true);
    gr.addColour (0.6, juce::Colour (0xff3b3c40));
    g.setColour (juce::Colours::black.withAlpha (0.8f));
    g.fillEllipse (c.x - R, c.y - R + 1.0f, 2 * R, 2 * R);
    g.setGradientFill (gr);
    g.fillEllipse (c.x - R, c.y - R, 2 * R, 2 * R);
    juce::Path slot;
    slot.addRoundedRectangle (-5.0f, -1.0f, 10.0f, 2.0f, 1.0f);
    g.setColour (juce::Colour (0xff101012));
    g.fillPath (slot, juce::AffineTransform::rotation (angle).translated (c));
}

// ── LookAndFeel: knurled dials + hardware keys ───────────────────────────────
class HardwareLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HardwareLookAndFeel() { setColourScheme (juce::LookAndFeel_V4::getDarkColourScheme()); }

    // Skirt + shading + cap don't rotate, so they're drawn once per size and cached.
    const juce::Image& knobBody (int D)
    {
        auto& img = bodies[D];
        if (img.isValid()) return img;
        const float sc = 2.0f;
        img = juce::Image (juce::Image::ARGB, (int) (D * sc), (int) (D * sc) + (int) (D * 0.2f * sc), true);
        juce::Graphics g (img);
        g.addTransform (juce::AffineTransform::scale (sc));
        const float R = D * 0.5f;
        const juce::Point<float> c (R, R);
        const float rs = R * 0.705f, rc = R * 0.564f;

        // drop shadow
        for (int i = 0; i < 6; ++i)
        {
            g.setColour (juce::Colours::black.withAlpha (0.10f));
            const float e = rs + i * 1.2f;
            g.fillEllipse (c.x - e, c.y - e + R * 0.14f, 2 * e, 2 * e);
        }
        // knurled skirt
        juce::Path skirt; skirt.addEllipse (c.x - rs, c.y - rs, 2 * rs, 2 * rs);
        g.setColour (juce::Colour (0xff0d0d0f)); g.fillPath (skirt);
        g.setColour (juce::Colour (0xff303135));
        for (int k = 0; k < 60; ++k)
        {
            juce::Path w;
            const float a0 = k * juce::MathConstants<float>::twoPi / 60.0f, a1 = a0 + juce::MathConstants<float>::twoPi / 120.0f;
            w.addPieSegment (c.x - rs, c.y - rs, 2 * rs, 2 * rs, a0, a1, 0.0f);
            g.fillPath (w);
        }
        g.setColour (juce::Colour (0xff050506)); g.drawEllipse (c.x - rs, c.y - rs, 2 * rs, 2 * rs, 1.0f);
        // shading over the skirt
        juce::ColourGradient sh (juce::Colours::white.withAlpha (0.16f), 0, c.y - rs, juce::Colours::black.withAlpha (0.42f), 0, c.y + rs, false);
        sh.addColour (0.38, juce::Colours::transparentWhite);
        g.setGradientFill (sh); g.fillPath (skirt);
        // cap
        juce::ColourGradient cg (juce::Colour (0xff404145), c.x, c.y - rc * 0.48f, juce::Colour (0xff17181a), c.x, c.y + rc * 1.1f, true);
        cg.addColour (0.52, juce::Colour (0xff26272a));
        g.setGradientFill (cg);
        g.fillEllipse (c.x - rc, c.y - rc, 2 * rc, 2 * rc);
        g.setColour (juce::Colours::white.withAlpha (0.2f));
        g.drawEllipse (c.x - rc + 0.5f, c.y - rc + 0.5f, 2 * rc - 1.0f, 2 * rc - 1.0f, 0.6f);
        g.setColour (juce::Colour (0xff08080a));
        g.drawEllipse (c.x - rc, c.y - rc, 2 * rc, 2 * rc, 1.0f);
        return img;
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider& s) override
    {
        const int D = juce::jmin (w, h);
        const float R = D * 0.5f;
        const juce::Point<float> c ((float) x + w * 0.5f, (float) y + R);
        const float angle = startAngle + juce::jlimit (0.0f, 1.0f, pos) * (endAngle - startAngle);
        const bool en = s.isEnabled();
        if (! en) g.beginTransparencyLayer (0.35f);

        // tick ring
        for (int k = 0; k <= 10; ++k)
        {
            const float a = startAngle + k * (endAngle - startAngle) / 10.0f;
            const float sx = std::sin (a), sy = -std::cos (a);
            g.setColour (juce::Colour (0xffd4d6da).withAlpha (k == 0 || k == 10 ? 0.9f : 0.55f));
            g.drawLine (c.x + sx * R * 0.86f, c.y + sy * R * 0.86f, c.x + sx * R * 0.99f, c.y + sy * R * 0.99f, 1.3f);
        }
        // value arc (LED ring)
        if (en)
        {
            juce::Path arc;
            arc.addCentredArc (c.x, c.y, R * 0.79f, R * 0.79f, 0, startAngle, angle, true);
            g.setColour (Pal::accent.withAlpha (0.25f));
            g.strokePath (arc, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour (Pal::accent);
            g.strokePath (arc, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        const auto& body = knobBody (D);
        g.drawImage (body, juce::Rectangle<float> (c.x - R, c.y - R, (float) D, (float) body.getHeight() / 2.0f));

        // indicator line on the cap
        const float rc = R * 0.564f;
        const float sx = std::sin (angle), sy = -std::cos (angle);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine (c.x + sx * rc * 0.2f, c.y + sy * rc * 0.2f + 1.0f, c.x + sx * rc * 0.88f, c.y + sy * rc * 0.88f + 1.0f, D * 0.045f);
        g.setColour (juce::Colour (0xfff4f4f1));
        g.drawLine (c.x + sx * rc * 0.2f, c.y + sy * rc * 0.2f, c.x + sx * rc * 0.88f, c.y + sy * rc * 0.88f, D * 0.045f);

        if (! en) g.endTransparencyLayer();
    }

    static void padlock (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c)
    {
        auto body = r.removeFromBottom (r.getHeight() * 0.58f);
        juce::Path shackle;
        shackle.addCentredArc (body.getCentreX(), body.getY(), body.getWidth() * 0.3f, r.getHeight() * 0.85f, 0,
                               -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.setColour (c);
        g.strokePath (shackle, juce::PathStrokeType (1.4f));
        g.fillRoundedRectangle (body, 1.5f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        if (b.getProperties().contains ("pill"))
        {
            auto r = b.getLocalBounds().toFloat().reduced (0.5f);
            const bool warnCol = (bool) b.getProperties()["warn"];
            const auto c = warnCol ? Pal::warn : Pal::accent;
            g.setColour (c.withAlpha (over ? 0.22f : 0.12f));
            g.fillRoundedRectangle (r, r.getHeight() / 2);
            g.setColour (c.withAlpha (0.7f));
            g.drawRoundedRectangle (r, r.getHeight() / 2, 1.0f);
            juce::ignoreUnused (down);
            return;
        }
        auto r = b.getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom (3.0f);
        const bool on = b.getToggleState() || down;
        if (on) r.translate (0.0f, 2.0f);
        const float rad = 6.0f;
        g.setColour (juce::Colours::black);
        g.fillRoundedRectangle (r.translated (0.0f, on ? 1.0f : 3.0f), rad);
        g.setGradientFill (juce::ColourGradient (juce::Colour (over ? 0xff34353a : 0xff2e2f33), 0, r.getY(),
                                                 juce::Colour (0xff1c1d20), 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, rad);
        g.setColour (juce::Colours::white.withAlpha (0.13f));
        g.drawLine (r.getX() + rad, r.getY() + 1.0f, r.getRight() - rad, r.getY() + 1.0f, 1.0f);
        g.setColour (juce::Colour (0xff050506));
        g.drawRoundedRectangle (r, rad, 1.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        if (b.getProperties().contains ("pill"))
        {
            const bool warnCol = (bool) b.getProperties()["warn"];
            g.setFont (Fonts::led (11.0f, 0.06f));
            g.setColour (warnCol ? Pal::warn : Pal::accent);
            g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
            return;
        }
        const bool locked = (bool) b.getProperties().getWithDefault ("locked", false);
        const bool on  = b.getToggleState() && ! locked;
        const bool savedLocked = b.getToggleState() && locked;   // an upgrade setting saved in the project: shown greyed
        const bool lit = on || ((bool) b.getProperties().getWithDefault ("lit", false) && ! locked);
        auto r = b.getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom (3.0f);
        if (on || down) r.translate (0.0f, 2.0f);
        const bool small = r.getHeight() < 34.0f;

        const float lw = small ? 20.0f : 30.0f, lh = small ? 3.0f : 4.0f;
        auto lamp = juce::Rectangle<float> (r.getCentreX() - lw / 2, r.getY() + (small ? 6.0f : 9.0f), lw, lh);
        const auto lampCol = b.getProperties().contains ("lampColour")
                                 ? juce::Colour ((juce::uint32) (juce::int64) b.getProperties()["lampColour"]) : Pal::accent;
        if (lit)
        {
            g.setColour (lampCol.withAlpha (0.25f));
            g.fillRoundedRectangle (lamp.expanded (3.0f, 2.5f), 3.0f);
            g.setColour (lampCol);
        }
        else g.setColour (savedLocked ? Pal::silkLo : Pal::lampOff);
        g.fillRoundedRectangle (lamp, lh / 2);

        if (locked)
            padlock (g, { r.getRight() - 17.0f, r.getY() + 6.0f, 9.0f, 11.0f }, Pal::silkLo);
        g.setFont (Fonts::silkSemi (small ? 12.0f : 13.5f, 0.14f));
        g.setColour (on ? juce::Colour (0xfff2f2ef) : savedLocked ? Pal::silk.withAlpha (0.55f) : Pal::silkMid.withAlpha (b.isEnabled() ? 1.0f : 0.5f));
        g.drawText (b.getButtonText(), r.withTrimmedTop (lamp.getBottom() - r.getY()), juce::Justification::centred);
    }

private:
    std::map<int, juce::Image> bodies;
};

// ── SegmentControl ────────────────────────────────────────────────────────────
SegmentControl::SegmentControl (juce::RangedAudioParameter& param, juce::StringArray labels, juce::Array<int> values)
    : vals (values),
      attachment (param, [this] (float v)
      {
          for (int i = 0; i < buttons.size(); ++i)
              buttons[i]->setToggleState (buttons.size() == 1 ? v > 0.5f : (int) std::round (v) == vals[i], juce::dontSendNotification);
      })
{
    if (vals.isEmpty()) for (int i = 0; i < labels.size(); ++i) vals.add (i);
    for (int i = 0; i < labels.size(); ++i)
    {
        auto* b = buttons.add (new juce::TextButton (labels[i]));
        b->onClick = [this, i]
        {
            if (allowValue && ! allowValue (vals[i])) { attachment.sendInitialUpdate(); return; }
            if (buttons.size() == 1) attachment.setValueAsCompleteGesture (buttons[0]->getToggleState() ? 0.0f : 1.0f);
            else                     attachment.setValueAsCompleteGesture ((float) vals[i]);
        };
        addAndMakeVisible (b);
    }
    attachment.sendInitialUpdate();
}

void SegmentControl::setLocked (int value, bool locked)
{
    for (int i = 0; i < buttons.size(); ++i)
        if (vals[i] == value && (bool) buttons[i]->getProperties().getWithDefault ("locked", false) != locked)
        {
            buttons[i]->getProperties().set ("locked", locked);
            buttons[i]->repaint();
        }
}

void SegmentControl::setDimmed (bool d)
{
    if (d == dimmed) return;
    dimmed = d;
    for (auto* b : buttons) { b->setAlpha (d ? 0.35f : 1.0f); b->setEnabled (! d); }
}

void SegmentControl::resized()
{
    auto r = getLocalBounds();
    const int cols = columns > 0 ? columns : buttons.size();
    const int rows = (buttons.size() + cols - 1) / juce::jmax (1, cols);
    const int w = (r.getWidth()  - gap * (cols - 1)) / juce::jmax (1, cols);
    const int h = (r.getHeight() - gap * (rows - 1)) / juce::jmax (1, rows);
    for (int i = 0; i < buttons.size(); ++i)
        buttons[i]->setBounds (r.getX() + (i % cols) * (w + gap), r.getY() + (i / cols) * (h + gap), w, h);
}

// ── CurveView ─────────────────────────────────────────────────────────────────
bool CurveView::isDraw() const
{
    return proc.proUnlocked.load() && (int) proc.apvts.getRawParameterValue ("shape")->load() == ACP::draw;
}

juce::Rectangle<float> CurveView::plot() const
{
    return getLocalBounds().toFloat().withTrimmedTop (30.0f).reduced (16.0f, 0.0f).withTrimmedBottom (24.0f);
}

// Gain (1 = full volume) at x in [0,1] of the whole envelope, for the active shape.
float CurveView::displayGain (float x) const
{
    if (isDraw()) return ACP::gainAt (proc.getDrawPoints(), x);

    const int shape = (int) proc.apvts.getRawParameterValue ("shape")->load();
    const float a = proc.apvts.getRawParameterValue ("attack")->load() / 1000.0f;
    const float h = proc.apvts.getRawParameterValue ("hold")->load() / 1000.0f;
    const float r = juce::jmax (0.005f, proc.stepSeconds() * proc.apvts.getRawParameterValue ("release")->load() / 100.0f);
    const float t = x * (a + h + r);
    if (t < a) return 1.0f - 0.5f * (1.0f - std::cos (t / a * juce::MathConstants<float>::pi));
    if (t < a + h) return 0.0f;
    return 1.0f - ACP::releaseShape (shape, (t - a - h) / r);
}

juce::Point<float> CurveView::toScreen (juce::Point<float> p) const
{
    const auto r = plot();
    return { r.getX() + p.x * r.getWidth(), r.getY() + (1.0f - p.y) * r.getHeight() };
}

juce::Point<float> CurveView::fromScreen (juce::Point<float> s) const
{
    const auto r = plot();
    return { juce::jlimit (0.0f, 1.0f, (s.x - r.getX()) / r.getWidth()),
             juce::jlimit (0.0f, 1.0f, 1.0f - (s.y - r.getY()) / r.getHeight()) };
}

int CurveView::hitPoint (juce::Point<float> s) const
{
    const auto pts = proc.getDrawPoints();
    for (int i = 0; i < (int) pts.size(); ++i)
        if (toScreen (pts[(size_t) i]).getDistanceFrom (s) < 10.0f) return i;
    return -1;
}

void CurveView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    drawScreen (g, area);
    const auto dim = Pal::accent.withAlpha (0.55f);

    g.setFont (Fonts::led (12.0f, 0.14f));
    glowText (g, "DUCK SHAPE", area.reduced (16.0f, 9.0f).withHeight (14.0f), juce::Justification::centredLeft, dim);
    g.setFont (Fonts::led (11.0f, 0.08f));
    g.setColour (Pal::accent.withAlpha (0.4f));
    g.drawText (isDraw() ? juce::String (juce::CharPointer_UTF8 ("CLICK ADD \xc2\xb7 DRAG \xc2\xb7 DOUBLE-CLICK REMOVE"))
                         : juce::String (proc.proUnlocked.load() ? "CLICK TO DRAW YOUR OWN" : "DRAW YOUR OWN: UPGRADE"),
                area.reduced (16.0f, 9.0f).withHeight (14.0f), juce::Justification::centredRight);

    const auto r = plot();
    g.setColour (Pal::accent.withAlpha (0.07f));
    for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (r.getY() + r.getHeight() * i / 4.0f), r.getX(), r.getRight());
    for (int i = 1; i < 8; ++i) g.drawVerticalLine ((int) (r.getX() + r.getWidth() * i / 8.0f), r.getY(), r.getBottom());
    g.setColour (Pal::accent.withAlpha (0.16f));
    g.drawHorizontalLine ((int) r.getBottom(), r.getX(), r.getRight());

    // ── The kick, as captured from the sidechain, on the same time axis as the curve ──
    const float envMs = proc.envLengthSeconds() * 1000.0f;
    const int kLen = proc.kickLenBins.load();
    const bool pro = proc.proUnlocked.load();
    const bool haveKick = pro && proc.kickCaptures.load() > 0 && kLen > 0;
    if (! pro)
    {
        g.setFont (Fonts::led (11.0f, 0.08f));
        g.setColour (Pal::kick.withAlpha (0.35f));
        auto box = r.withTrimmedLeft (r.getWidth() * 0.45f).withTrimmedTop (r.getHeight() - 44.0f).reduced (8.0f, 4.0f);
        g.drawFittedText ("UPGRADE: SEE YOUR KICK HERE AND\nDRAW THE CURVE AROUND IT", box.toNearestInt(), juce::Justification::bottomRight, 2);
    }
    else if (haveKick)
    {
        const int cols = (int) r.getWidth();
        std::vector<float> v ((size_t) cols + 1, 0.0f);
        float pk = 1.0e-6f;
        for (int b = 0; b < juce::jmin (kLen, 80); ++b) pk = juce::jmax (pk, proc.kickShape[(size_t) b].load());
        for (int i = 0; i <= cols; ++i)
        {
            const float t0 = (float) i / cols * envMs, t1 = (float) (i + 1) / cols * envMs;
            float m = 0.0f;
            for (int b = (int) t0; b <= (int) t1 && b < kLen && b < ACP::kickBins; ++b) m = juce::jmax (m, proc.kickShape[(size_t) b].load());
            v[(size_t) i] = std::sqrt (juce::jlimit (0.0f, 1.0f, m / pk));
        }
        const float mid = r.getCentreY(), half = r.getHeight() * 0.46f;
        juce::Path wave;
        wave.startNewSubPath (r.getX(), mid);
        for (int i = 0; i <= cols; ++i) wave.lineTo (r.getX() + i, mid - v[(size_t) i] * half);
        for (int i = cols; i >= 0; --i) wave.lineTo (r.getX() + i, mid + v[(size_t) i] * half);
        wave.closeSubPath();
        g.setColour (Pal::kick.withAlpha (0.13f));
        g.fillPath (wave);
        g.setColour (Pal::kick.withAlpha (0.28f));
        g.strokePath (wave, juce::PathStrokeType (1.0f));
        g.setFont (Fonts::led (12.0f, 0.14f));
        g.setColour (Pal::kick.withAlpha (0.5f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xc2\xb7  YOUR KICK")), area.reduced (16.0f, 9.0f).withHeight (14.0f).withTrimmedLeft (118.0f),
                    juce::Justification::centredLeft);
    }
    else
    {
        // bottom-right: under the curve's recovered end for every shape
        g.setFont (Fonts::led (11.0f, 0.08f));
        g.setColour (Pal::kick.withAlpha (0.35f));
        auto box = r.withTrimmedLeft (r.getWidth() * 0.45f).withTrimmedTop (r.getHeight() - 44.0f).reduced (8.0f, 4.0f);
        g.drawFittedText ("SEND YOUR KICK TO THE SIDE CHAIN\nAND PRESS PLAY TO SEE IT HERE", box.toNearestInt(), juce::Justification::bottomRight, 2);
    }

    // ── The duck curve ──
    juce::Path p;
    const int N = 300;
    for (int i = 0; i <= N; ++i)
    {
        const float x = (float) i / N;
        const auto s = toScreen ({ x, juce::jlimit (0.0f, 1.0f, displayGain (x)) });
        if (i == 0) p.startNewSubPath (s); else p.lineTo (s);
    }
    juce::Path fill (p);
    fill.lineTo (r.getRight(), r.getBottom()); fill.lineTo (r.getX(), r.getBottom()); fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (Pal::accent.withAlpha (0.16f), 0, r.getY(), Pal::accent.withAlpha (0.0f), 0, r.getBottom(), false));
    g.fillPath (fill);
    glowPath (g, p, Pal::accent, 2.0f);

    if (isDraw())
        for (auto pt : proc.getDrawPoints())
        {
            const auto s = toScreen (pt);
            g.setColour (Pal::accent.withAlpha (0.3f));
            g.fillEllipse (s.x - 7.0f, s.y - 7.0f, 14.0f, 14.0f);
            g.setColour (Pal::screen);
            g.fillEllipse (s.x - 4.5f, s.y - 4.5f, 9.0f, 9.0f);
            g.setColour (juce::Colour (0xfff2f2ef));
            g.drawEllipse (s.x - 4.5f, s.y - 4.5f, 9.0f, 9.0f, 1.6f);
        }

    // Playhead riding the curve
    const float pos = proc.envPosition.load();
    if (pos < 0.999f)
    {
        const auto s = toScreen ({ pos, juce::jlimit (0.0f, 1.0f, displayGain (pos)) });
        g.setColour (Pal::accent.withAlpha (0.25f));
        g.drawVerticalLine ((int) s.x, r.getY(), r.getBottom());
        g.setColour (Pal::accent.withAlpha (0.35f));
        g.fillEllipse (s.x - 8.0f, s.y - 8.0f, 16.0f, 16.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (s.x - 3.5f, s.y - 3.5f, 7.0f, 7.0f);
    }

    // Time axis
    g.setFont (Fonts::led (11.0f, 0.06f));
    g.setColour (Pal::accent.withAlpha (0.45f));
    auto axis = juce::Rectangle<float> (r.getX(), r.getBottom() + 5.0f, r.getWidth(), 14.0f);
    g.drawText ("0 MS", axis, juce::Justification::centredLeft);
    g.drawText (juce::String (juce::roundToInt (envMs * 0.5f)) + " MS", axis, juce::Justification::centred);
    g.drawText (juce::String (juce::roundToInt (envMs)) + " MS", axis, juce::Justification::centredRight);
}

void CurveView::mouseDown (const juce::MouseEvent& e)
{
    if (! proc.proUnlocked.load()) { if (onLockedClick) onLockedClick(); return; }
    auto pts = proc.getDrawPoints();
    if (! isDraw())
    {
        // Copy the current shape into DRAW so it can be edited from there.
        std::vector<juce::Point<float>> seeded;
        for (float x : { 0.0f, 0.03f, 0.08f, 0.16f, 0.3f, 0.5f, 0.75f, 1.0f })
            seeded.push_back ({ x, juce::jlimit (0.0f, 1.0f, displayGain (x)) });
        proc.setDrawPoints (seeded);
        if (auto* prm = proc.apvts.getParameter ("shape"))
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) ACP::draw));
        repaint();
        return;
    }

    const int hit = hitPoint (e.position);
    if (e.mods.isPopupMenu())
    {
        if (hit > 0 && hit < (int) pts.size() - 1) { pts.erase (pts.begin() + hit); proc.setDrawPoints (pts); }
        repaint();
        return;
    }
    if (hit >= 0) { dragIndex = hit; return; }

    pts.push_back (fromScreen (e.position));
    std::sort (pts.begin(), pts.end(), [] (auto a, auto b) { return a.x < b.x; });
    proc.setDrawPoints (pts);
    dragIndex = hitPoint (e.position);
    repaint();
}

void CurveView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragIndex < 0 || ! isDraw()) return;
    auto pts = proc.getDrawPoints();
    if (dragIndex >= (int) pts.size()) return;
    auto v = fromScreen (e.position);
    const int last = (int) pts.size() - 1;
    if (dragIndex == 0)          v.x = 0.0f;
    else if (dragIndex == last)  v.x = 1.0f;
    else v.x = juce::jlimit (pts[(size_t) dragIndex - 1].x + 0.001f, pts[(size_t) dragIndex + 1].x - 0.001f, v.x);
    pts[(size_t) dragIndex] = v;
    proc.setDrawPoints (pts);
    repaint();
}

void CurveView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! isDraw()) return;
    auto pts = proc.getDrawPoints();
    const int hit = hitPoint (e.position);
    if (hit > 0 && hit < (int) pts.size() - 1) { pts.erase (pts.begin() + hit); proc.setDrawPoints (pts); }
    dragIndex = -1;
    repaint();
}

// ── ScopeView ─────────────────────────────────────────────────────────────────
void ScopeView::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    drawScreen (g, area);

    g.setFont (Fonts::led (12.0f, 0.14f));
    glowText (g, "LIVE", area.reduced (16.0f, 9.0f).withHeight (14.0f), juce::Justification::centredLeft, Pal::accent.withAlpha (0.55f));
    g.setFont (Fonts::led (11.0f, 0.08f));
    auto legend = area.reduced (16.0f, 9.0f).withHeight (14.0f);
    g.setColour (Pal::accent.withAlpha (0.8f)); g.drawText ("DUCK", legend, juce::Justification::centredRight);
    g.setColour (Pal::kick.withAlpha (0.4f));   g.drawText ("IN", legend.withTrimmedRight (44.0f), juce::Justification::centredRight);

    const auto r = area.withTrimmedTop (30.0f).reduced (14.0f, 0.0f).withTrimmedBottom (14.0f);
    const int N = ACP::scopeSize;
    const int w0 = proc.scopeWrite.load();
    const float dx = r.getWidth() / (N - 1);
    const float mid = r.getCentreY();

    juce::Path wave, duck;
    duck.startNewSubPath (r.getX(), r.getY());
    for (int i = 0; i < N; ++i)
    {
        const auto& col = proc.scope[(size_t) ((w0 + i) % N)];
        const float lvl = std::sqrt (juce::jlimit (0.0f, 1.0f, col.level.load()));
        const float x = r.getX() + i * dx;
        if (i == 0) wave.startNewSubPath (x, mid - lvl * r.getHeight() * 0.5f); else wave.lineTo (x, mid - lvl * r.getHeight() * 0.5f);
        duck.lineTo (x, r.getY() + col.amount.load() * r.getHeight());
        if (col.hit.load())
        {
            g.setColour (Pal::accent.withAlpha (0.25f));
            g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        }
    }
    for (int i = N - 1; i >= 0; --i)
    {
        const auto& col = proc.scope[(size_t) ((w0 + i) % N)];
        const float lvl = std::sqrt (juce::jlimit (0.0f, 1.0f, col.level.load()));
        wave.lineTo (r.getX() + i * dx, mid + lvl * r.getHeight() * 0.5f);
    }
    wave.closeSubPath();
    g.setColour (Pal::kick.withAlpha (0.16f));
    g.fillPath (wave);

    juce::Path duckFill (duck);
    duckFill.lineTo (r.getRight(), r.getY()); duckFill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (Pal::accent.withAlpha (0.26f), 0, r.getY(), Pal::accent.withAlpha (0.03f), 0, r.getBottom(), false));
    g.fillPath (duckFill);
    glowPath (g, duck, Pal::accent, 1.6f);

    if (warning.isNotEmpty())
    {
        g.setColour (Pal::screen.withAlpha (0.9f));
        g.fillRoundedRectangle (r.expanded (8.0f, 4.0f), 4.0f);
        auto t = r.reduced (10.0f, 8.0f);
        g.setFont (Fonts::led (15.0f, 0.08f));
        glowText (g, warning, t.removeFromTop (t.getHeight() * 0.42f), juce::Justification::centredBottom, Pal::warn);
        t.removeFromTop (8.0f);
        g.setFont (Fonts::silkMed (14.0f, 0.04f));
        g.setColour (Pal::silk.withAlpha (0.85f));
        g.drawFittedText (warningSub, t.toNearestInt(), juce::Justification::centredTop, 4);
    }
}

// ── ProPanel ──────────────────────────────────────────────────────────────────
static void styleBox (juce::TextEditor& t, const juce::String& placeholder)
{
    t.setColour (juce::TextEditor::backgroundColourId, Pal::screen);
    t.setColour (juce::TextEditor::textColourId, Pal::accent);
    t.setColour (juce::TextEditor::outlineColourId, Pal::lampOff);
    t.setColour (juce::TextEditor::focusedOutlineColourId, Pal::accent);
    t.setColour (juce::CaretComponent::caretColourId, Pal::accent);
    t.setFont (Fonts::led (15.0f, 0.02f));
    t.setTextToShowWhenEmpty (placeholder, Pal::silkLo);
    t.setIndents (10, 10);
    t.setSelectAllWhenFocused (true);
}

ProPanel::ProPanel (AutoChainProAudioProcessor& p) : proc (p)
{
    styleBox (emailBox, "you@email.com");
    styleBox (keyBox, "99HZ-...");
    subscribe.setColour (juce::ToggleButton::textColourId, Pal::silkMid);
    subscribe.setColour (juce::ToggleButton::tickColourId, Pal::accent);
    subscribe.setColour (juce::ToggleButton::tickDisabledColourId, Pal::silkLo);
    for (auto* c : std::initializer_list<juce::Component*> { &emailBox, &keyBox, &subscribe, &trialButton, &unlockButton, &buyButton, &closeButton })
        addAndMakeVisible (c);
    buyButton.getProperties().set ("lit", true);
    trialButton.getProperties().set ("lit", true);

    closeButton.onClick = [this] { setVisible (false); };
    buyButton.onClick = []
    {
        juce::URL ("https://99hertz.com/autochain-pro?utm_source=plugin&utm_medium=autochainpro&utm_campaign=upgrade").launchInDefaultBrowser();
    };
    unlockButton.onClick = [this]
    {
        const auto pasted = hz::Licence::verify (keyBox.getText(), proc.licence.product);
        const auto info = proc.licence.apply (keyBox.getText());
        if (info.kind == hz::LicenceInfo::saveFailed)
        {
            message = "COULDN'T SAVE YOUR KEY. EMAIL INFO@99HERTZ.COM";
            messageIsError = true;
        }
        else if (pasted.kind == hz::LicenceInfo::trialExpired)
        {
            message = "THAT TRIAL KEY HAS ENDED";
            messageIsError = true;
        }
        else if (info.unlocked())
        {
            proc.setLicenceInfo (info);
            message = info.kind == hz::LicenceInfo::pro ? "UNLOCKED. THANK YOU!" : "TRIAL UNLOCKED";
            messageIsError = false;
            keyBox.clear();
            if (onLicenceChanged) onLicenceChanged();
        }
        else
        {
            message = "THAT KEY ISN'T VALID FOR AUTOCHAIN";
            messageIsError = true;
        }
        refresh();
    };
    trialButton.onClick = [this]
    {
        const auto email = emailBox.getText().trim();
        if (! email.containsChar ('@') || ! email.fromFirstOccurrenceOf ("@", false, false).containsChar ('.'))
        {
            message = "ENTER YOUR EMAIL TO START THE TRIAL"; messageIsError = true; refresh(); return;
        }
        busy = true; message = "STARTING YOUR TRIAL..."; messageIsError = false; refresh();
        juce::Component::SafePointer<ProPanel> safe (this);
        proc.licence.requestTrial (email, subscribe.getToggleState(), [safe] (hz::LicenceInfo info, juce::String error)
        {
            if (safe == nullptr) return;
            safe->busy = false;
            if (info.unlocked())
            {
                safe->proc.setLicenceInfo (info);
                safe->message = "UNLOCKED FOR 7 DAYS. YOUR KEY IS ON ITS WAY BY EMAIL.";
                safe->messageIsError = false;
                if (safe->onLicenceChanged) safe->onLicenceChanged();
            }
            else { safe->message = error.toUpperCase(); safe->messageIsError = true; }
            safe->refresh();
        });
    };
}

void ProPanel::licenceChanged()
{
    if (isVisible()) refresh();
}

void ProPanel::show (const juce::String& why)
{
    reason = why; message = {}; messageIsError = false;
    refresh();
    setVisible (true);
    toFront (true);
}

juce::Rectangle<int> ProPanel::card() const
{
    return getLocalBounds().withSizeKeepingCentre (600, 456);
}

void ProPanel::refresh()
{
    const auto k = proc.licenceInfo.kind;
    const bool canTrial = k == hz::LicenceInfo::none || k == hz::LicenceInfo::invalid;
    const bool isPro = k == hz::LicenceInfo::pro;
    emailBox.setVisible (canTrial); subscribe.setVisible (canTrial); trialButton.setVisible (canTrial);
    keyBox.setVisible (! isPro); unlockButton.setVisible (! isPro); buyButton.setVisible (! isPro);
    trialButton.setEnabled (! busy);
    trialButton.setButtonText (busy ? "STARTING..." : "START 7-DAY FREE TRIAL");
    resized();
    repaint();
}

void ProPanel::resized()
{
    const auto c = card();
    const int x = c.getX() + 36, w = c.getWidth() - 72;
    emailBox.setBounds (x, c.getY() + 222, w - 232, 38);
    trialButton.setBounds (x + w - 220, c.getY() + 218, 220, 46);
    subscribe.setBounds (x - 4, c.getY() + 264, 260, 24);
    keyBox.setBounds (x, c.getY() + 320, w - 232, 38);
    unlockButton.setBounds (x + w - 220, c.getY() + 316, 220, 46);
    buyButton.setBounds (x, c.getBottom() - 62, 220, 46);
    closeButton.setBounds (x + w - 120, c.getBottom() - 62, 120, 46);
}

void ProPanel::mouseDown (const juce::MouseEvent& e)
{
    if (! card().contains (e.getPosition())) setVisible (false);
}

void ProPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.62f));
    const auto c = card().toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (c.translated (0, 8.0f).expanded (4.0f), 12.0f);
    g.setGradientFill (juce::ColourGradient (Pal::panelTop.brighter (0.04f), 0, c.getY(), Pal::panelBot, 0, c.getBottom(), false));
    g.fillRoundedRectangle (c, 10.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (c.reduced (0.5f), 10.0f, 1.0f);

    const float x = c.getX() + 36.0f, w = c.getWidth() - 72.0f;
    g.setFont (Fonts::silkSemi (12.0f, 0.34f));
    g.setColour (juce::Colour (0xffa9adb3));
    g.drawText ("99HERTZ", juce::Rectangle<float> (x, c.getY() + 24.0f, w, 14.0f), juce::Justification::centredLeft);
    g.setFont (Fonts::silkBold (28.0f, 0.06f));
    g.setColour (Pal::silk);
    g.drawText ("AUTOCHAIN", juce::Rectangle<float> (x, c.getY() + 38.0f, w, 34.0f), juce::Justification::centredLeft);
    if (proc.licenceInfo.kind != hz::LicenceInfo::pro)
    {
        juce::GlyphArrangement ga; ga.addLineOfText (Fonts::silkBold (28.0f, 0.06f), "AUTOCHAIN", 0, 0);
        g.setColour (Pal::accent);
        g.drawText ("UPGRADE", juce::Rectangle<float> (x + ga.getBoundingBox (0, -1, true).getWidth() + 10.0f, c.getY() + 38.0f, 200.0f, 34.0f),
                    juce::Justification::centredLeft);
    }

    const auto k = proc.licenceInfo.kind;
    juce::String sub = reason;
    if (k == hz::LicenceInfo::trial)        sub = "TRIAL: " + proc.licenceInfo.daysLeftText();
    else if (k == hz::LicenceInfo::trialExpired) sub = "YOUR FREE TRIAL HAS ENDED";
    else if (k == hz::LicenceInfo::pro)     sub = "LICENSED TO " + proc.licenceInfo.email.toUpperCase();
    else if (sub.isEmpty())                 sub = "TRY EVERY UPGRADE FEATURE FREE FOR 7 DAYS";
    g.setFont (Fonts::led (14.0f, 0.08f));
    glowText (g, sub, { x, c.getY() + 78.0f, w, 18.0f }, juce::Justification::centredLeft, k == hz::LicenceInfo::trialExpired ? Pal::warn : Pal::accent);

    const char* feats[] = { "Duck from your real kick (sidechain)",
                            "See your kick and draw the curve around it",
                            "AUTO: fit the curve to your kick in one click",
                            "Filter ducking" };
    g.setFont (Fonts::silkMed (16.0f, 0.03f));
    for (int i = 0; i < 4; ++i)
    {
        const float y = c.getY() + 106.0f + i * 22.0f;
        g.setColour (Pal::accent);
        g.fillEllipse (x + 1.0f, y + 7.0f, 6.0f, 6.0f);
        g.setColour (Pal::silk);
        g.drawText (feats[i], juce::Rectangle<float> (x + 16.0f, y, w - 16.0f, 20.0f), juce::Justification::centredLeft);
    }

    g.setFont (Fonts::silkSemi (12.5f, 0.16f));
    g.setColour (Pal::silkMid);
    if (k == hz::LicenceInfo::none || k == hz::LicenceInfo::invalid)
        g.drawText ("NO CARD NEEDED. AFTER 7 DAYS IT GOES BACK TO FREE.", juce::Rectangle<float> (x, c.getY() + 200.0f, w, 14.0f), juce::Justification::centredLeft);
    if (k != hz::LicenceInfo::pro)
        g.drawText ("ALREADY BOUGHT IT? PASTE YOUR KEY", juce::Rectangle<float> (x, c.getY() + 302.0f, w, 14.0f), juce::Justification::centredLeft);

    if (message.isNotEmpty())
    {
        g.setFont (Fonts::led (13.0f, 0.04f));
        g.setColour (messageIsError ? Pal::warn : Pal::accent);
        g.drawFittedText (message, juce::Rectangle<int> ((int) x, (int) c.getBottom() - 86, (int) w, 18), juce::Justification::centredLeft, 1);
    }
}

// ── Editor ────────────────────────────────────────────────────────────────────
static juce::RangedAudioParameter& P (AutoChainProAudioProcessor& p, const char* id) { return *p.apvts.getParameter (id); }

// Where the sidechain input is picked differs per DAW, so name the right place.
// brief = the 3-line note under SENSITIVITY; full = the scope's help panel.
static juce::String pickKickHint (bool brief = false)
{
    const juce::PluginHostType host;
    if (host.isLogic() || host.isGarageBand())
        return "Pick your kick in the Side Chain menu at the top of this window";
    if (host.isAbletonLive())
        return "Pick your kick in the Sidechain box on this device in Live";
    if (host.isFruityLoops())
        return brief ? "In FL's mixer, sidechain your kick's track to this track"
                     : "In FL's mixer, right-click the kick's send arrow on this track and choose Sidechain to this track";
    if (host.isProTools())
        return "Pick your kick in the Key Input menu at the top of this window";
    if (host.isCubase() || host.isNuendo())
        return brief ? "Turn on Side-Chain at the top of this window and send your kick"
                     : "Turn on Side-Chain at the top of this window, then send your kick to it";
    if (host.isStudioOne())
        return "Pick your kick in the Side-Chain menu at the top of this window";
    if (host.isBitwigStudio())
        return "Pick your kick in the sidechain selector on this device";
    if (host.isReaper())
        return "Send your kick to channels 3/4 of this track";
    return "Set your kick as this plugin's sidechain input in your DAW";
}

static constexpr int W = 1000, H = 600, padX = 44;

AutoChainProAudioProcessorEditor::AutoChainProAudioProcessorEditor (AutoChainProAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      trigger (P (p, "trigger"), { "TEMPO", "SIDECHAIN" }, { ACP::tempo, ACP::kick }),
      rate    (P (p, "rate"), ACP::rateNames),
      shape   (P (p, "shape"), { "CLASSIC", "HARD", "SMOOTH", "GATE", "DRAW" }),
      target  (P (p, "target"), { "VOLUME", "FILTER" }),
      lowOnly (P (p, "lowonly"), { "LOW ONLY" }),
      curve (p), scopeView (p), proPanel (p)
{
    laf.reset (new HardwareLookAndFeel());
    setLookAndFeel (laf.get());
    rate.columns = 3;  rate.gap = 6;
    shape.columns = 3;

    for (auto* c : std::initializer_list<juce::Component*> { &trigger, &rate, &shape, &target, &lowOnly, &autoButton, &curve, &scopeView })
        addAndMakeVisible (c);

    autoButton.setTooltip ("Fit the curve to your kick");
    autoButton.onClick = [this]
    {
        if (! proc.proUnlocked.load()) { openPro ("AUTO-FIT TO YOUR KICK IS AN UPGRADE FEATURE"); return; }
        if (proc.autoShapeFromKick()) flashText = "CURVE FITTED TO YOUR KICK";
        else                          flashText = "NO KICK YET: PRESS PLAY";
        flashTicks = 75;
        curve.repaint();
    };

    auto gate = [this] (const char* id, const char* why)
    {
        return [this, id, why] (int v)
        {
            if (proc.proUnlocked.load() || ! ACP::isProValue (id, v)) return true;
            openPro (why);
            return false;
        };
    };
    trigger.allowValue = gate ("trigger", "SIDECHAIN FROM YOUR KICK IS AN UPGRADE FEATURE");
    shape.allowValue   = gate ("shape",   "DRAWING YOUR OWN CURVE IS AN UPGRADE FEATURE");
    target.allowValue  = gate ("target",  "FILTER DUCKING IS AN UPGRADE FEATURE");
    curve.onLockedClick = [this] { openPro ("DRAWING YOUR OWN CURVE IS AN UPGRADE FEATURE"); };

    licenceButton.getProperties().set ("pill", true);
    licenceButton.onClick = [this] { openPro ({}); };
    addAndMakeVisible (licenceButton);
    proPanel.onLicenceChanged = [this] { updateLocks(); repaint(); };

    addKnob ("mix", "DEPTH");
    addKnob ("sens", "SENSITIVITY");
    addKnob ("attack", "ATTACK");
    addKnob ("hold", "HOLD");
    addKnob ("release", "RELEASE");
    addKnob ("xover", "LOW FREQ");
    addKnob ("cutoff", "CUTOFF");

    addChildComponent (proPanel);   // last, so it sits on top
    setSize (W, H);
    updateLocks();
    timerCallback();
    startTimerHz (30);
}

AutoChainProAudioProcessorEditor::~AutoChainProAudioProcessorEditor() { setLookAndFeel (nullptr); }

AutoChainProAudioProcessorEditor::Knob& AutoChainProAudioProcessorEditor::addKnob (const juce::String& id, const juce::String& label)
{
    auto* k = knobs.add (new Knob());
    k->label = label; k->paramId = id;
    k->slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k->slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    k->slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    k->slider.setDoubleClickReturnValue (true, (double) proc.apvts.getParameter (id)->convertFrom0to1 (proc.apvts.getParameter (id)->getDefaultValue()));
    k->slider.setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    addAndMakeVisible (k->slider);
    k->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k->slider);
    k->slider.onValueChange = [this] { repaint(); };
    return *k;
}

AutoChainProAudioProcessorEditor::Knob* AutoChainProAudioProcessorEditor::knob (const juce::String& id)
{
    for (auto* k : knobs) if (k->paramId == id) return k;
    return nullptr;
}

void AutoChainProAudioProcessorEditor::resized()
{
    // Header: logo | LED display | DEPTH dial
    ledArea = { (W - 430) / 2, 30, 430, 66 };
    knob ("mix")->slider.setBounds (W - padX - 62, 26, 62, 62);

    // Screens
    const int sy = 126, sh = 216;
    curve.setBounds (padX, sy, 580, sh);
    scopeView.setBounds (padX + 596, sy, 270, sh);
    meterArea = { W - padX - 30, sy, 30, sh };

    // Sections
    const int top = 378;
    secTrigger = { padX, top, 210, 196 };
    secShape   = { secTrigger.getRight(), top, 280, 196 };
    secEnv     = { secShape.getRight(), top, 230, 196 };
    secOutput  = { secEnv.getRight(), top, W - padX - secEnv.getRight(), 196 };
    const int keysY = top + 34, D = 62;

    {   // TRIGGER
        auto c = secTrigger.withTrimmedRight (18);
        trigger.setBounds (c.getX(), keysY, c.getWidth(), 44);
        rate.setBounds (c.getX(), keysY + 58, c.getWidth(), 66);
        knob ("sens")->slider.setBounds (c.getX() + 6, keysY + 58, D, D);
        hintArea = { c.getX() + D + 18, keysY + 56, c.getWidth() - D - 18, 70 };
    }
    {   // SHAPE: 5 shapes + AUTO in a 3x2 grid
        auto c = secShape.reduced (18, 0);
        const int kw = (c.getWidth() - 16) / 3;
        shape.setBounds (c.getX(), keysY, c.getWidth(), 44 + 8 + 44);
        autoButton.setBounds (c.getX() + 2 * (kw + 8), keysY + 52, kw, 44);
    }
    {   // ENVELOPE
        auto c = secEnv.reduced (14, 0);
        const int w = c.getWidth() / 3;
        int i = 0;
        for (auto id : { "attack", "hold", "release" })
            knob (id)->slider.setBounds (c.getX() + w * i++ + (w - D) / 2, keysY + 6, D, D);
    }
    {   // OUTPUT
        auto c = secOutput.withTrimmedLeft (18);
        target.setBounds (c.getX(), keysY, c.getWidth(), 38);
        const int half = c.getWidth() / 2;
        for (auto id : { "xover", "cutoff" })
            knob (id)->slider.setBounds (c.getX() + (half - D) / 2, keysY + 56, D, D);
        lowOnly.setBounds (c.getX() + half + 4, keysY + 66, half - 4, 36);
    }

    proPanel.setBounds (getLocalBounds());
    renderBackground();
    updateLocks();
}

void AutoChainProAudioProcessorEditor::renderBackground()
{
    const float sc = 2.0f;
    background = juce::Image (juce::Image::ARGB, (int) (W * sc), (int) (H * sc), true);
    juce::Graphics g (background);
    g.addTransform (juce::AffineTransform::scale (sc));

    // Panel
    auto panel = juce::Rectangle<float> (0, 0, (float) W, (float) H);
    g.setColour (juce::Colour (0xff0a0a0b)); g.fillAll();
    g.setGradientFill (juce::ColourGradient (Pal::panelTop, 0, 0, Pal::panelBot, 0, (float) H, false));
    g.fillRoundedRectangle (panel, 10.0f);
    g.setColour (juce::Colours::white.withAlpha (0.022f));          // brushed grain
    for (int x = 0; x < W; x += 3) g.fillRect ((float) x, 0.0f, 1.0f, (float) H);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawLine (10.0f, 0.5f, W - 10.0f, 0.5f, 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawLine (10.0f, H - 1.0f, W - 10.0f, H - 1.0f, 2.0f);

    drawScrew (g, { 19.0f, 19.0f }, 0.49f);
    drawScrew (g, { W - 19.0f, 19.0f }, -0.91f);
    drawScrew (g, { 19.0f, H - 19.0f }, 1.29f);
    drawScrew (g, { W - 19.0f, H - 19.0f }, -0.21f);

    // Logo
    g.setFont (Fonts::silkSemi (12.0f, 0.34f));
    g.setColour (juce::Colour (0xffa9adb3));
    g.drawText ("99HERTZ", padX, 34, 240, 16, juce::Justification::centredLeft);
    g.setFont (Fonts::silkBold (31.0f, 0.06f));
    g.setColour (Pal::silk);
    g.drawText ("AUTOCHAIN", padX, 50, 240, 36, juce::Justification::centredLeft);
    {
        juce::GlyphArrangement ga; ga.addLineOfText (Fonts::silkBold (31.0f, 0.06f), "AUTOCHAIN", 0, 0);
        const float w = ga.getBoundingBox (0, -1, true).getWidth();
        juce::ignoreUnused (w);
    }

    // DEPTH label (header right)
    g.setFont (Fonts::silkSemi (13.0f, 0.2f));
    g.setColour (Pal::silk);
    g.drawText ("DEPTH", W - padX - 62 - 140, 38, 126, 16, juce::Justification::centredRight);

    // Dividers
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.fillRect ((float) padX, 110.0f, (float) (W - 2 * padX), 1.0f);
    g.fillRect ((float) padX, 360.0f, (float) (W - 2 * padX), 1.0f);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    for (auto s : { secShape, secEnv, secOutput })
        g.fillRect ((float) s.getX() - 0.5f, (float) s.getY(), 1.0f, (float) s.getHeight() - 6.0f);
}

void AutoChainProAudioProcessorEditor::openPro (const juce::String& reason)
{
    proPanel.show (reason);
}

void AutoChainProAudioProcessorEditor::updateLocks()
{
    const bool pro = proc.proUnlocked.load();
    trigger.setLocked (ACP::kick, ! pro);
    shape.setLocked (ACP::draw, ! pro);
    target.setLocked (ACP::filter, ! pro);
    autoButton.getProperties().set ("locked", ! pro);

    const auto& li = proc.licenceInfo;
    const auto t = li.buttonText();
    const bool warnCol = li.kind == hz::LicenceInfo::trialExpired;
    if (licenceButton.getButtonText() != t || (bool) licenceButton.getProperties()["warn"] != warnCol)
    {
        licenceButton.setButtonText (t);
        licenceButton.getProperties().set ("warn", warnCol);
        juce::GlyphArrangement ga; ga.addLineOfText (Fonts::led (11.0f, 0.06f), t, 0, 0);
        const int w = (int) std::ceil (ga.getBoundingBox (0, -1, true).getWidth()) + 24;
        licenceButton.setBounds (padX, 88, w, 20);
        licenceButton.repaint();
    }
}

void AutoChainProAudioProcessorEditor::timerCallback()
{
    if (const int v = proc.licenceVersion.load(); v != seenLicenceVersion)
    {
        seenLicenceVersion = v;   // another instance unlocked, a trial ended, or the panel saved a key
        proPanel.licenceChanged();
        repaint();
    }
    const bool pro = proc.proUnlocked.load();
    const int trig = pro ? (int) proc.apvts.getRawParameterValue ("trigger")->load() : (int) ACP::tempo;
    const int shpP = (int) proc.apvts.getRawParameterValue ("shape")->load();
    const int shp  = (pro || shpP != ACP::draw) ? shpP : (int) ACP::classic;
    const int tgt  = pro ? (int) proc.apvts.getRawParameterValue ("target")->load() : (int) ACP::volume;
    const bool low = proc.apvts.getRawParameterValue ("lowonly")->load() > 0.5f;
    const bool sc  = trig == ACP::kick;

    rate.setVisible (! sc);
    knob ("sens")->slider.setVisible (sc);
    lowOnly.setVisible (tgt == ACP::volume);
    knob ("xover")->slider.setVisible (tgt == ACP::volume);
    knob ("cutoff")->slider.setVisible (tgt == ACP::filter);
    knob ("xover")->slider.setEnabled (low);
    for (auto id : { "attack", "hold" }) knob (id)->slider.setEnabled (shp != ACP::draw);

    const bool haveKick = pro && proc.kickCaptures.load() > 0;
    autoButton.getProperties().set ("lit", haveKick);
    updateLocks();
    autoButton.repaint();

    ledWarn = false;
    scopeView.warning = {};
    scopeView.warningSub = {};
    const auto rateName = ACP::rateNames[(int) proc.apvts.getRawParameterValue ("rate")->load()];
    if (sc && proc.sidechainIsSelf.load())
    {
        ledWarn = true;
        ledLabel = "SIDECHAIN";
        ledText  = "NO KICK PICKED";
        scopeView.warning = "PICK YOUR KICK";
        scopeView.warningSub = "AutoChain can't tell which track your kick is on. " + pickKickHint() + ".";
    }
    else if (sc && proc.sidechainSilent.load())
    {
        ledWarn = true;
        ledLabel = "SIDECHAIN";
        ledText  = "WAITING FOR KICK";
        scopeView.warning = "WAITING FOR YOUR KICK";
        scopeView.warningSub = pickKickHint() + ", then press play. Nothing ducks while the kick is silent.";
    }
    else if (sc)
    {
        ledLabel = "SIDECHAIN";
        ledText  = "DUCKING ON KICK";
    }
    else
    {
        ledLabel = "TEMPO";
        ledText  = "DUCKING EVERY " + rateName;
    }
    // A project saved with upgrade settings, opened without a trial or key: say so (it plays as free).
    upgradeNudge = ! pro && proc.usesUpgradeSettings();
    if (upgradeNudge)
    {
        ledWarn  = true;
        ledLabel = "THIS PROJECT USES UPGRADE SETTINGS";
        ledText  = "UPGRADE TO USE THEM AGAIN";
    }
    if (flashTicks > 0 && --flashTicks > 0) ledText = flashText;

    const float target = proc.currentAmount.load();
    meterValue = target > meterValue ? target : meterValue * 0.85f + target * 0.15f;
    const float sci = proc.scInLevel.load();
    scMeter = sci > scMeter ? sci : scMeter * 0.88f + sci * 0.12f;

    curve.repaint();
    scopeView.repaint();
    repaint (ledArea.expanded (12));
    repaint (meterArea.expanded (4));
    repaint (hintArea);
}

void AutoChainProAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.drawImage (background, getLocalBounds().toFloat());

    // ── LED display ──
    {
        auto r = ledArea.toFloat();
        drawScreen (g, r);
        const auto col = ledWarn ? Pal::warn : Pal::accent;
        auto in = r.reduced (20.0f, 11.0f);
        auto left = in.removeFromLeft (upgradeNudge ? in.getWidth() - 80.0f : 232.0f);
        g.setFont (Fonts::led (12.0f, 0.14f));
        glowText (g, ledLabel, left.removeFromTop (16.0f), juce::Justification::centredLeft, col.withAlpha (0.75f));
        g.setFont (Fonts::led (upgradeNudge ? 18.0f : 21.0f, 0.04f));
        glowText (g, ledText, left, juce::Justification::centredLeft, col);

        auto right = in.removeFromRight (64.0f);
        g.setFont (Fonts::led (12.0f, 0.14f));
        glowText (g, "BPM", right.removeFromTop (16.0f), juce::Justification::centredRight, Pal::accent.withAlpha (0.75f));
        g.setFont (Fonts::led (21.0f, 0.04f));
        glowText (g, juce::String (proc.lastBpm.load(), 1), right, juce::Justification::centredRight, Pal::accent);

        // KICK IN meter: shows the sidechain is actually arriving
        auto mid = in.reduced (10.0f, 0.0f);
        if (! upgradeNudge)
        {
            g.setFont (Fonts::led (12.0f, 0.14f));
            glowText (g, "KICK IN", mid.removeFromTop (16.0f), juce::Justification::centredLeft, Pal::accent.withAlpha (0.75f));
            const float lvlDb = juce::Decibels::gainToDecibels (scMeter, -60.0f);
            const int lit = juce::jlimit (0, 8, (int) std::ceil ((lvlDb + 48.0f) / 6.0f));
            auto seg = mid.withTrimmedTop (7.0f).withHeight (12.0f);
            const float sw = (seg.getWidth() - 7 * 3.0f) / 8.0f;
            for (int i = 0; i < 8; ++i)
            {
                auto s = juce::Rectangle<float> (seg.getX() + i * (sw + 3.0f), seg.getY(), sw, seg.getHeight());
                if (i < lit) { g.setColour (Pal::accent.withAlpha (0.25f)); g.fillRect (s.expanded (1.5f)); g.setColour (Pal::accent); }
                else g.setColour (Pal::accent.withAlpha (0.08f));
                g.fillRect (s);
            }
        }
    }

    // Licensed: the licence email in small print beside the LICENSED pill.
    if (proc.licenceInfo.kind == hz::LicenceInfo::pro && proc.licenceInfo.email.isNotEmpty())
    {
        const auto lb = licenceButton.getBounds();
        g.setFont (Fonts::silkMed (12.0f, 0.04f));
        g.setColour (Pal::silkMid);
        g.drawFittedText (proc.licenceInfo.email, { lb.getRight() + 8, lb.getY(), ledArea.getX() - lb.getRight() - 16, lb.getHeight() },
                          juce::Justification::centredLeft, 1, 0.8f);
    }

    // DEPTH value under its label
    {
        g.setFont (Fonts::led (14.0f, 0.06f));
        auto* prm = proc.apvts.getParameter ("mix");
        glowText (g, prm->getCurrentValueAsText() + " %", { (float) (W - padX - 62 - 140), 58.0f, 126.0f, 18.0f },
                  juce::Justification::centredRight, Pal::accent);
    }

    // ── GR meter ──
    {
        auto m = meterArea.toFloat();
        drawScreen (g, m, 4.0f);
        g.setFont (Fonts::led (10.5f, 0.1f));
        g.setColour (Pal::accent.withAlpha (0.6f));
        g.drawText ("GR", m.withHeight (24.0f), juce::Justification::centred);
        auto bar = m.reduced (8.0f, 0.0f).withTrimmedTop (26.0f).withTrimmedBottom (26.0f);
        const int segs = 22;
        const float gap = 2.5f, sh = (bar.getHeight() - gap * (segs - 1)) / segs;
        const int lit = juce::roundToInt (juce::jlimit (0.0f, 1.0f, meterValue) * segs);
        for (int i = 0; i < segs; ++i)
        {
            auto s = juce::Rectangle<float> (bar.getX(), bar.getY() + i * (sh + gap), bar.getWidth(), sh);
            g.setColour (i < lit ? Pal::accent : Pal::accent.withAlpha (0.08f));
            g.fillRect (s);
        }
        const float db = juce::Decibels::gainToDecibels (1.0f - juce::jlimit (0.0f, 1.0f, meterValue) * 0.999f, -60.0f);
        g.setColour (Pal::accent.withAlpha (0.8f));
        g.drawText (meterValue < 0.005f ? "0" : (db <= -59.0f ? "-INF" : juce::String (juce::roundToInt (db))), m.withTrimmedTop (m.getHeight() - 24.0f), juce::Justification::centred);
    }

    // ── Section titles ──
    const bool pro = proc.proUnlocked.load();
    const int trig = pro ? (int) proc.apvts.getRawParameterValue ("trigger")->load() : (int) ACP::tempo;
    const int shp  = (pro || (int) proc.apvts.getRawParameterValue ("shape")->load() != ACP::draw)
                         ? (int) proc.apvts.getRawParameterValue ("shape")->load() : (int) ACP::classic;
    const bool sc  = trig == ACP::kick;
    auto title = [&] (juce::Rectangle<int> s, const char* t, bool on, int inset)
    {
        auto r = s.toFloat().withTrimmedLeft ((float) inset).withHeight (20.0f);
        if (on) { g.setColour (Pal::accent.withAlpha (0.3f)); g.fillEllipse (r.getX() - 2.5f, r.getCentreY() - 6.0f, 12.0f, 12.0f); }
        g.setColour (on ? Pal::accent : Pal::lampOff);
        g.fillEllipse (r.getX() + 1.0f, r.getCentreY() - 3.5f, 7.0f, 7.0f);
        g.setFont (Fonts::silkBold (16.0f, 0.22f));
        g.setColour (Pal::silk);
        g.drawText (t, r.withTrimmedLeft (17.0f), juce::Justification::centredLeft);
    };
    title (secTrigger, "TRIGGER", true, 0);
    title (secShape, "SHAPE", true, 18);
    title (secEnv, "ENVELOPE", shp != ACP::draw, 14);
    title (secOutput, "OUTPUT", true, 18);

    // Small silkscreen notes
    g.setFont (Fonts::silkMed (13.0f, 0.06f));
    g.setColour (Pal::silkMid);
    if (sc)
        g.drawFittedText (pickKickHint (true), hintArea, juce::Justification::centredLeft, 3);
    g.drawFittedText ("AUTO fits the curve to your kick", { secShape.getX() + 18, shape.getBottom() + 8, secShape.getWidth() - 36, 18 },
                      juce::Justification::centredRight, 1);
    if (shp == ACP::draw)
        g.drawFittedText ("Drawn curve: RELEASE sets its length", { secEnv.getX() + 14, secEnv.getBottom() - 24, secEnv.getWidth() - 28, 18 },
                          juce::Justification::centred, 1);

    // ── Dial labels + values ──
    for (auto* k : knobs)
    {
        if (! k->slider.isVisible() || k->paramId == "mix") continue;
        const auto sb = k->slider.getBounds();
        const float alpha = k->slider.isEnabled() ? 1.0f : 0.35f;
        auto cell = juce::Rectangle<float> ((float) sb.getCentreX() - 50.0f, (float) sb.getBottom() + 2.0f, 100.0f, 16.0f);
        g.setFont (Fonts::silkSemi (13.0f, 0.16f));
        g.setColour (Pal::silk.withAlpha (alpha));
        g.drawText (k->label, cell, juce::Justification::centred);
        auto* prm = proc.apvts.getParameter (k->paramId);
        g.setFont (Fonts::led (13.0f, 0.04f));
        g.setColour (Pal::accent.withAlpha (alpha));
        g.drawText (prm->getCurrentValueAsText() + " " + prm->getLabel(), cell.translated (0, 16.0f), juce::Justification::centred);
    }
}
