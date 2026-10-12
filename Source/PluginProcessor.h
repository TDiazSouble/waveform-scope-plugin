#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>

// One analysis frame (0.5 ms of audio): peak of full signal and of each band.
struct Frame
{
    float full = 0.0f, rms = 0.0f, lo = 0.0f, mid = 0.0f, hi = 0.0f;
    uint8_t beat = 0; // 0 none, 1 beat, 2 bar
};

class FloatingScopeWindow;

class WaveformScopeProcessor : public juce::AudioProcessor,
                               private juce::Timer
{
public:
    WaveformScopeProcessor();
    ~WaveformScopeProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Waveform Scope"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    void updateTrackProperties (const TrackProperties& properties) override;

    // Reads analysis frames produced on the audio thread (single consumer: the floating view).
    int popFrames (Frame* dest, int maxFrames);

    juce::String getTrackName() const;
    juce::Colour getTrackColour() const;
    void setWindowVisible (bool shouldBeVisible);
    void windowBoundsChanged (juce::Rectangle<int> b) { windowBounds = b; }
    juce::Rectangle<int> getWindowBounds() const { return windowBounds; }

    juce::AudioProcessorValueTreeState apvts;

    static constexpr int framesPerSecond = 2000;

private:
    void timerCallback() override;
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AbstractFifo fifo { 65536 };
    std::vector<Frame> fifoData = std::vector<Frame> (65536);

    juce::dsp::LinkwitzRileyFilter<float> lowLP, midHP, midLP, highHP;

    double sampleRate = 44100.0;
    int samplesPerFrame = 441, frameCounter = 0;
    Frame acc;
    float sumSquares = 0.0f;
    int64_t lastBeat = std::numeric_limits<int64_t>::min();
    uint8_t pendingBeat = 0;

    mutable juce::CriticalSection trackLock;
    juce::String trackName;
    juce::Colour trackColour { juce::Colours::transparentBlack };

    std::unique_ptr<FloatingScopeWindow> window;
    juce::Rectangle<int> windowBounds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformScopeProcessor)
};
