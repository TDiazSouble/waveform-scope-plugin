#include "WaveformView.h"

namespace
{
    // Soft curve so quiet parts stay visible and loud parts don't flatten out.
    float shape (float v, float gain)
    {
        v *= gain;
        return v >= 1.0f ? 1.0f : std::sqrt (v) * 0.35f + v * 0.65f;
    }
}

WaveformView::WaveformView (WaveformScopeProcessor& p) : processor (p)
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (false); // don't steal Live's keyboard (space bar) on a normal click
    setSize (900, 110);
    startTimerHz (60);
}

WaveformView::~WaveformView() { stopTimer(); }

float WaveformView::getParam (const juce::String& id) const
{
    return processor.apvts.getRawParameterValue (id)->load();
}

void WaveformView::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = processor.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
}

void WaveformView::timerCallback()
{
    for (;;)
    {
        const int got = processor.popFrames (scratch.data(), (int) scratch.size());
        if (got == 0)
            break;
        for (int i = 0; i < got; ++i)
        {
            ring[(size_t) head] = scratch[(size_t) i];
            head = (head + 1) % historyFrames;
        }
    }
    repaint();
}

void WaveformView::paint (juce::Graphics& g)
{
    const float w = (float) getWidth(), h = (float) getHeight();
    const float mid = h * 0.5f, half = h * 0.5f - 2.0f;

    g.fillAll (juce::Colour::fromFloatRGBA (0.07f, 0.07f, 0.09f, 1.0f));

    // Draw one column per *physical* pixel so scaled displays (125 %, 150 %...) get full detail.
    const float scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    const int cols = juce::jmax (1, juce::roundToInt (w * scale));
    const float cw = w / (float) cols;

    const int mode = (int) getParam ("mode");
    const float gain = getParam ("gain");
    const bool beats = getParam ("beats") > 0.5f;
    const int visible = juce::jlimit (1, historyFrames, (int) std::round (getParam ("zoom") * WaveformScopeProcessor::framesPerSecond));
    const double per = (double) visible / cols;

    const auto lowCol = juce::Colour::fromFloatRGBA (0.12f, 0.42f, 1.0f, 1.0f);
    const auto midCol = juce::Colour::fromFloatRGBA (1.0f, 0.62f, 0.15f, 1.0f);
    const auto highCol = juce::Colour::fromFloatRGBA (1.0f, 1.0f, 1.0f, 0.95f);
    const auto monoCol = juce::Colour::fromFloatRGBA (0.35f, 0.75f, 1.0f, 1.0f);

    for (int x = 0; x < cols; ++x)
    {
        // frames covered by this column (at least one; nearest frame when zoomed far in)
        const int a = (int) std::floor (x * per);
        const int b = juce::jmax (a + 1, (int) std::floor ((x + 1) * per));
        float P = 0, R = 0, L = 0, M = 0, H = 0;
        uint8_t beat = 0;
        for (int k = a; k < b; ++k)
        {
            const auto& f = ring[(size_t) ((head - visible + k + historyFrames * 2) % historyFrames)];
            P = juce::jmax (P, f.full);
            R = juce::jmax (R, f.rms);
            L = juce::jmax (L, f.lo);
            M = juce::jmax (M, f.mid);
            H = juce::jmax (H, f.hi);
            beat = juce::jmax (beat, f.beat);
        }

        const float fx = x * cw;
        if (beats && beat > 0)
        {
            g.setColour (juce::Colours::white.withAlpha (beat == 2 ? 0.35f : 0.13f));
            g.fillRect (fx, 0.0f, juce::jmax (cw, 1.0f), h);
        }

        if (mode == 1) // Rekordbox-style 3-band: blue lows, amber mids, white highs
        {
            const float hl = shape (L, gain) * half, hm = shape (M * 1.4f, gain) * half, hh = shape (H * 2.5f, gain) * half;
            g.setColour (lowCol);  g.fillRect (fx, mid - hl, cw, hl * 2.0f);
            g.setColour (midCol);  g.fillRect (fx, mid - hm, cw, hm * 2.0f);
            g.setColour (highCol); g.fillRect (fx, mid - hh, cw, hh * 2.0f);
        }
        else
        {
            juce::Colour c = monoCol;
            if (mode == 0) // Serato-style RGB: red lows, green mids, blue highs
            {
                float r = L, gg = M * 1.6f, bb = H * 3.2f;
                const float m = juce::jmax (r, gg, bb, 1.0e-6f);
                r /= m; gg /= m; bb /= m;
                c = juce::Colour::fromFloatRGBA (0.15f + 0.85f * r, 0.15f + 0.85f * gg, 0.15f + 0.85f * bb, 1.0f);
            }
            // outer layer: peak (darker), inner layer: RMS body (bright)
            const float hp = shape (P, gain) * half;
            const float hr = juce::jmin (hp, shape (R * 1.41f, gain) * half);
            g.setColour (c.withMultipliedBrightness (0.5f));
            g.fillRect (fx, mid - hp, cw, hp * 2.0f);
            g.setColour (c);
            g.fillRect (fx, mid - hr, cw, hr * 2.0f);
        }
    }

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.fillRect (0.0f, mid, w, 1.0f);
    g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.25f, 0.25f, 0.9f));
    g.fillRect (w - 2.0f, 0.0f, 2.0f, h);

    // track name tag, tinted with the track colour when the host provides it
    const auto name = processor.getTrackName();
    if (name.isNotEmpty() && h >= 30)
    {
        auto col = processor.getTrackColour();
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.setColour (col.isTransparent() ? juce::Colours::white.withAlpha (0.6f) : col.brighter (0.4f));
        g.drawText (name, 6, 3, getWidth() - 12, 14, juce::Justification::topLeft);
    }

    if (hovering)
    {
        const auto r = closeButtonArea().toFloat().reduced (4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.drawLine (r.getX(), r.getY(), r.getRight(), r.getBottom(), 1.6f);
        g.drawLine (r.getRight(), r.getY(), r.getX(), r.getBottom(), 1.6f);
    }
}

void WaveformView::mouseMove (const juce::MouseEvent&)
{
    hovering = true;
}

void WaveformView::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
}

void WaveformView::mouseDrag (const juce::MouseEvent& e)
{
    if (! draggingWindow)
        return;
    if (auto* w = findParentComponentOfClass<FloatingScopeWindow>())
        if (! w->isFullScreen())
            dragger.dragComponent (w, e.getEventRelativeTo (w), nullptr);
}

void WaveformView::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const float zoom = getParam ("zoom") * (wheel.deltaY > 0 ? 0.85f : 1.0f / 0.85f);
    setParam ("zoom", juce::jlimit (0.5f, 30.0f, zoom));
}

void WaveformView::mouseDoubleClick (const juce::MouseEvent&)
{
    if (auto* w = findParentComponentOfClass<FloatingScopeWindow>())
        w->setFullScreenMode (! w->isFullScreen());
}

bool WaveformView::keyPressed (const juce::KeyPress& key)
{
    auto* w = findParentComponentOfClass<FloatingScopeWindow>();
    if (w == nullptr)
        return false;
    if (key == juce::KeyPress::F11Key)
    {
        w->setFullScreenMode (! w->isFullScreen());
        return true;
    }
    if (key == juce::KeyPress::escapeKey && w->isFullScreen())
    {
        w->setFullScreenMode (false);
        return true;
    }
    return false;
}

void WaveformView::mouseDown (const juce::MouseEvent& e)
{
    draggingWindow = false;

    // Ctrl + click (or the hover "x") closes the window
    if (e.mods.isLeftButtonDown() && (e.mods.isCtrlDown() || closeButtonArea().contains (e.getPosition())))
    {
        processor.setWindowVisible (false);
        return;
    }

    if (! e.mods.isPopupMenu())
    {
        // plain left drag moves the borderless window
        if (auto* w = findParentComponentOfClass<FloatingScopeWindow>())
        {
            dragger.startDraggingComponent (w, e.getEventRelativeTo (w));
            draggingWindow = true;
        }
        return;
    }

    const int mode = (int) getParam ("mode");
    juce::PopupMenu modes;
    modes.addItem ("RGB (Serato)", true, mode == 0, [this] { setParam ("mode", 0); });
    modes.addItem ("3-Band (Rekordbox)", true, mode == 1, [this] { setParam ("mode", 1); });
    modes.addItem ("Mono", true, mode == 2, [this] { setParam ("mode", 2); });

    juce::PopupMenu zooms;
    for (float s : { 1.0f, 2.0f, 4.0f, 6.0f, 10.0f, 20.0f, 30.0f })
        zooms.addItem (juce::String (s, 0) + " s", true, std::abs (getParam ("zoom") - s) < 0.05f,
                       [this, s] { setParam ("zoom", s); });

    auto* window = findParentComponentOfClass<FloatingScopeWindow>();
    const bool full = window != nullptr && window->isFullScreen();

    juce::PopupMenu m;
    m.addSubMenu ("Modo", modes);
    m.addSubMenu ("Zoom", zooms);
    m.addItem ("Grilla de beats", true, getParam ("beats") > 0.5f,
               [this] { setParam ("beats", getParam ("beats") > 0.5f ? 0.0f : 1.0f); });
    m.addItem (full ? "Salir de pantalla completa (Esc)" : "Pantalla completa (F11 / doble clic)",
               [this, full]
               {
                   if (auto* w = findParentComponentOfClass<FloatingScopeWindow>())
                       w->setFullScreenMode (! full);
               });
    m.addSeparator();
    m.addItem ("Cerrar ventana (Ctrl + clic)", [this] { processor.setWindowVisible (false); });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition());
}

//==============================================================================
FloatingScopeWindow::FloatingScopeWindow (WaveformScopeProcessor& p)
    : ResizableWindow ("Waveform Scope", juce::Colour::fromFloatRGBA (0.07f, 0.07f, 0.09f, 1.0f), true),
      processor (p)
{
    setUsingNativeTitleBar (false);
    setContentOwned (new WaveformView (p), true);
    setResizable (true, false);
    setResizeLimits (200, 30, 8000, 4000);
    setAlwaysOnTop (true);

    auto bounds = p.getWindowBounds();
    if (bounds.isEmpty() || juce::Desktop::getInstance().getDisplays().getDisplayForRect (bounds) == nullptr)
    {
        const auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userBounds.toNearestInt();
        bounds = juce::Rectangle<int> (area.getX() + 40, area.getY() + 40, juce::jmin (1000, area.getWidth() - 80), 110);
    }
    setBounds (bounds);
    setVisible (true);
}

void FloatingScopeWindow::setFullScreenMode (bool shouldBeFullScreen)
{
    setFullScreen (shouldBeFullScreen);
    if (shouldBeFullScreen)
        if (auto* c = getContentComponent())
            c->grabKeyboardFocus(); // so Esc / F11 work right away
}

void FloatingScopeWindow::userTriedToCloseWindow() { processor.setWindowVisible (false); } // Alt+F4

// Remember the windowed size/position only (not the full-screen one).
void FloatingScopeWindow::moved()
{
    ResizableWindow::moved();
    if (! isFullScreen()) processor.windowBoundsChanged (getBounds());
}

void FloatingScopeWindow::resized()
{
    ResizableWindow::resized();
    if (! isFullScreen()) processor.windowBoundsChanged (getBounds());
}
