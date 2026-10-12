#include "PluginEditor.h"

namespace
{
    juce::ComboBox& withModes (juce::ComboBox& c)
    {
        c.addItemList ({ "RGB (Serato)", "3-Band (Rekordbox)", "Mono" }, 1);
        return c;
    }
}

WaveformScopeEditor::WaveformScopeEditor (WaveformScopeProcessor& p)
    : AudioProcessorEditor (p),
      modeAtt (p.apvts, "mode", withModes (mode)),
      zoomAtt (p.apvts, "zoom", zoom),
      gainAtt (p.apvts, "gain", gain),
      offsetAtt (p.apvts, "offset", offset),
      beatsAtt (p.apvts, "beats", beats),
      windowAtt (p.apvts, "window", window)
{
    for (auto* s : { &zoom, &gain, &offset })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        addAndMakeVisible (*s);
    }
    zoomLabel.setText ("Zoom (s)", juce::dontSendNotification);
    gainLabel.setText ("Gain", juce::dontSendNotification);
    offsetLabel.setText ("Grilla (ms)", juce::dontSendNotification);
    offset.setTooltip ("Corre la grilla de beats. En el master suele hacer falta un valor positivo "
                       "igual a la latencia de los plugins de los canales (delay compensation).");
    offset.setDoubleClickReturnValue (true, 0.0);

    for (auto* c : std::initializer_list<juce::Component*> { &mode, &zoomLabel, &gainLabel, &offsetLabel, &beats, &window })
        addAndMakeVisible (*c);

    setSize (380, 180);
}

void WaveformScopeEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1b20));
    g.setColour (juce::Colours::white.withAlpha (0.85f));
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText ("Waveform Scope", 12, 8, 200, 20, juce::Justification::left);
}

void WaveformScopeEditor::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (22);
    top.removeFromLeft (160);
    mode.setBounds (top);
    r.removeFromTop (8);

    auto row = r.removeFromTop (24);
    zoomLabel.setBounds (row.removeFromLeft (70));
    zoom.setBounds (row);
    r.removeFromTop (4);
    row = r.removeFromTop (24);
    gainLabel.setBounds (row.removeFromLeft (70));
    gain.setBounds (row);
    r.removeFromTop (4);
    row = r.removeFromTop (24);
    offsetLabel.setBounds (row.removeFromLeft (70));
    offset.setBounds (row);
    r.removeFromTop (8);

    row = r.removeFromTop (24);
    beats.setBounds (row.removeFromLeft (row.getWidth() / 2));
    window.setBounds (row);
}
