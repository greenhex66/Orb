#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Diag.h"

OrbitAudioProcessor::OrbitAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", orbitp::createLayout())
{
    orbitdiag::trace ("processor ctor: parameters = " + juce::String (getParameters().size()));
    params = orbitp::getParamPtrs (apvts);
}

void OrbitAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    orbitdiag::trace ("prepareToPlay: sr=" + juce::String (sampleRate) + " block=" + juce::String (samplesPerBlock));
    engine.prepare (sampleRate);
}

// Stereo output always. Input may be stereo or mono (a mono input is routed to both channels).
bool OrbitAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    const auto& in = layouts.getMainInputChannelSet();
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void OrbitAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || numSamples <= 0)
        return;                                              // unsupported layout: audio passes through untouched

    if (getTotalNumInputChannels() == 1)                     // mono in -> identical signal on both channels
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    hexq::orbit::Settings s;
    orbitp::fillSettings (params, s);

    hexq::orbit::HostInfo host;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())
                host.bpm = *bpm;
            host.playing = pos->getIsPlaying();
            if (auto ppq = pos->getPpqPosition())
            {
                host.ppq = *ppq;
                host.havePpq = true;
            }
        }
    }

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples, s, host);

    const auto meter = engine.getMeter();
    meterCorrelation.store (meter.correlation, std::memory_order_relaxed);
    meterMonoSumDb.store (meter.monoSumDb, std::memory_order_relaxed);

    for (int c = 2; c < buffer.getNumChannels(); ++c)
        buffer.clear (c, 0, numSamples);
}

juce::AudioProcessorEditor* OrbitAudioProcessor::createEditor()
{
    orbitdiag::trace ("createEditor");
    return new OrbitAudioProcessorEditor (*this);
}

void OrbitAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
    orbitdiag::trace ("getStateInformation: bytes=" + juce::String ((int) destData.getSize()));
}

void OrbitAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    orbitdiag::trace ("setStateInformation: begin, size=" + juce::String (sizeInBytes));
    try
    {
        std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
        if (xml != nullptr && xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
    catch (...)
    {
        orbitdiag::trace ("setStateInformation: exception");
    }
    orbitdiag::trace ("setStateInformation: end");
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OrbitAudioProcessor();
}
