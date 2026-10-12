#pragma once

#include "PluginProcessor.h"

// Small control panel shown inside the host. The waveform itself lives in the floating window.
class WaveformScopeEditor : public juce::AudioProcessorEditor
{
public:
    explicit WaveformScopeEditor (WaveformScopeProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    juce::ComboBox mode;
    juce::Slider zoom, gain, offset;
    juce::Label zoomLabel, gainLabel, offsetLabel;
    juce::ToggleButton beats { "Grilla de beats" }, window { "Ventana flotante" };

    APVTS::ComboBoxAttachment modeAtt;
    APVTS::SliderAttachment zoomAtt, gainAtt, offsetAtt;
    APVTS::ButtonAttachment beatsAtt, windowAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformScopeEditor)
};
