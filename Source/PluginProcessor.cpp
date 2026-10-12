#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "WaveformView.h"

WaveformScopeProcessor::WaveformScopeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "WaveformScope", createLayout())
{
    startTimerHz (5);
}

WaveformScopeProcessor::~WaveformScopeProcessor()
{
    stopTimer();
    window.reset();
}

juce::AudioProcessorValueTreeState::ParameterLayout WaveformScopeProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode",
                                                        StringArray { "RGB", "3-Band", "Mono" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "zoom", 1 }, "Zoom",
                                                       NormalisableRange<float> (0.5f, 30.0f, 0.1f, 0.5f), 6.0f,
                                                       AudioParameterFloatAttributes().withLabel ("s")));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Gain",
                                                       NormalisableRange<float> (0.25f, 8.0f, 0.01f, 0.5f), 1.5f));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "beats", 1 }, "Beat Grid", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "offset", 1 }, "Grid Offset",
                                                       NormalisableRange<float> (-500.0f, 500.0f, 0.1f), 0.0f,
                                                       AudioParameterFloatAttributes().withLabel ("ms")));
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "window", 1 }, "Floating Window", true));
    return layout;
}

bool WaveformScopeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in = layouts.getMainInputChannelSet();
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo())
        return false;
    return in == layouts.getMainOutputChannelSet();
}

void WaveformScopeProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    samplesPerFrame = juce::jmax (1, (int) std::round (sr / framesPerSecond));
    frameCounter = 0;
    acc = {};
    sumSquares = 0.0f;

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, samplesPerBlock), 1 };
    auto setup = [&] (juce::dsp::LinkwitzRileyFilter<float>& f, juce::dsp::LinkwitzRileyFilterType type, float hz)
    {
        f.prepare (spec);
        f.setType (type);
        f.setCutoffFrequency (hz);
        f.reset();
    };
    using T = juce::dsp::LinkwitzRileyFilterType;
    setup (lowLP, T::lowpass, 250.0f);
    setup (midHP, T::highpass, 250.0f);
    setup (midLP, T::lowpass, 3000.0f);
    setup (highHP, T::highpass, 3000.0f);
}

void WaveformScopeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int chans = juce::jmin (2, getTotalNumInputChannels());
    if (chans == 0)
        return;

    // Transport info for the beat grid.
    bool playing = false;
    double ppqStart = 0.0, ppqPerSample = 0.0, barStart = 0.0;
    int numerator = 4;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            auto ppq = pos->getPpqPosition();
            auto bpm = pos->getBpm();
            playing = pos->getIsPlaying() && ppq.hasValue() && bpm.hasValue();
            if (playing)
            {
                // Positive offset delays the grid, to line it up with audio that arrives late
                // (e.g. on the master, after the host's plugin delay compensation).
                const double offsetSec = apvts.getRawParameterValue ("offset")->load() / 1000.0;
                ppqStart = *ppq - offsetSec * *bpm / 60.0;
                ppqPerSample = *bpm / 60.0 / sampleRate;
                if (auto lb = pos->getPpqPositionOfLastBarStart())
                    barStart = *lb;
                if (auto ts = pos->getTimeSignature())
                    numerator = juce::jmax (1, ts->numerator);
            }
        }
    }
    if (! playing)
        lastBeat = std::numeric_limits<int64_t>::min();

    const float* l = buffer.getReadPointer (0);
    const float* r = chans > 1 ? buffer.getReadPointer (1) : l;

    for (int i = 0; i < n; ++i)
    {
        const float x = 0.5f * (l[i] + r[i]);
        const float lo = lowLP.processSample (0, x);
        const float mi = midLP.processSample (0, midHP.processSample (0, x));
        const float hi = highHP.processSample (0, x);

        acc.full = juce::jmax (acc.full, std::abs (x));
        sumSquares += x * x;
        acc.lo = juce::jmax (acc.lo, std::abs (lo));
        acc.mid = juce::jmax (acc.mid, std::abs (mi));
        acc.hi = juce::jmax (acc.hi, std::abs (hi));

        if (playing)
        {
            const auto beat = (int64_t) std::floor (ppqStart + i * ppqPerSample);
            if (lastBeat != std::numeric_limits<int64_t>::min() && beat != lastBeat)
            {
                const auto rel = beat - (int64_t) std::llround (barStart);
                pendingBeat = (rel % numerator == 0) ? 2 : 1;
            }
            lastBeat = beat;
        }

        if (++frameCounter >= samplesPerFrame)
        {
            acc.rms = std::sqrt (sumSquares / (float) samplesPerFrame);
            sumSquares = 0.0f;
            acc.beat = pendingBeat;
            pendingBeat = 0;
            const auto scope = fifo.write (1);
            if (scope.blockSize1 > 0)
                fifoData[(size_t) scope.startIndex1] = acc;
            acc = {};
            frameCounter = 0;
        }
    }
    // audio passes through untouched
}

int WaveformScopeProcessor::popFrames (Frame* dest, int maxFrames)
{
    const auto scope = fifo.read (juce::jmin (maxFrames, fifo.getNumReady()));
    int k = 0;
    for (int i = 0; i < scope.blockSize1; ++i) dest[k++] = fifoData[(size_t) (scope.startIndex1 + i)];
    for (int i = 0; i < scope.blockSize2; ++i) dest[k++] = fifoData[(size_t) (scope.startIndex2 + i)];
    return k;
}

void WaveformScopeProcessor::updateTrackProperties (const TrackProperties& properties)
{
    const juce::ScopedLock sl (trackLock);
    trackName = properties.name.value_or (juce::String());
    trackColour = properties.colourARGB.has_value() ? juce::Colour (*properties.colourARGB)
                                                    : juce::Colours::transparentBlack;
}

juce::String WaveformScopeProcessor::getTrackName() const
{
    const juce::ScopedLock sl (trackLock);
    return trackName;
}

juce::Colour WaveformScopeProcessor::getTrackColour() const
{
    const juce::ScopedLock sl (trackLock);
    return trackColour;
}

void WaveformScopeProcessor::setWindowVisible (bool shouldBeVisible)
{
    if (auto* p = apvts.getParameter ("window"))
        p->setValueNotifyingHost (shouldBeVisible ? 1.0f : 0.0f);
}

// Runs on the message thread: keeps the floating window in sync with the "window" parameter.
void WaveformScopeProcessor::timerCallback()
{
    const bool wanted = apvts.getRawParameterValue ("window")->load() > 0.5f;
    if (wanted && window == nullptr)
    {
        // drain stale frames so the view starts "now"
        std::vector<Frame> tmp (4096);
        while (popFrames (tmp.data(), (int) tmp.size()) > 0) {}
        window = std::make_unique<FloatingScopeWindow> (*this);
    }
    else if (! wanted && window != nullptr)
    {
        window.reset();
    }

    if (window != nullptr)
    {
        const auto name = getTrackName();
        const auto title = name.isNotEmpty() ? "Waveform Scope - " + name : juce::String ("Waveform Scope");
        if (window->getName() != title)
            window->setName (title);
    }
}

void WaveformScopeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("winX", windowBounds.getX(), nullptr);
    state.setProperty ("winY", windowBounds.getY(), nullptr);
    state.setProperty ("winW", windowBounds.getWidth(), nullptr);
    state.setProperty ("winH", windowBounds.getHeight(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void WaveformScopeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (state.hasType (apvts.state.getType()))
        {
            windowBounds = { (int) state.getProperty ("winX", 0), (int) state.getProperty ("winY", 0),
                             (int) state.getProperty ("winW", 0), (int) state.getProperty ("winH", 0) };
            apvts.replaceState (state);
            if (window != nullptr && ! windowBounds.isEmpty())
                window->setBounds (windowBounds);
        }
    }
}

juce::AudioProcessorEditor* WaveformScopeProcessor::createEditor()
{
    return new WaveformScopeEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WaveformScopeProcessor();
}
