#pragma once

#include "PluginProcessor.h"

// Scrolling DJ-style waveform (newest audio at the right edge).
class WaveformView : public juce::Component,
                     private juce::Timer
{
public:
    explicit WaveformView (WaveformScopeProcessor&);
    ~WaveformView() override;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    void timerCallback() override;
    void setParam (const juce::String& id, float plainValue);
    float getParam (const juce::String& id) const;

    WaveformScopeProcessor& processor;
    static constexpr int historyFrames = WaveformScopeProcessor::framesPerSecond * 40;
    std::vector<Frame> ring = std::vector<Frame> (historyFrames);
    std::vector<Frame> scratch = std::vector<Frame> (8192);
    int head = 0;

    juce::ComponentDragger dragger;
    bool hovering = false, draggingWindow = false;
    juce::Rectangle<int> closeButtonArea() const { return { getWidth() - 20, 2, 18, 18 }; }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformView)
};

// Borderless, always-on-top window that lives independently of the host's plugin editor.
class FloatingScopeWindow : public juce::ResizableWindow
{
public:
    explicit FloatingScopeWindow (WaveformScopeProcessor&);

    void userTriedToCloseWindow() override;
    void setFullScreenMode (bool shouldBeFullScreen);
    void moved() override;
    void resized() override;

private:
    WaveformScopeProcessor& processor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FloatingScopeWindow)
};
