#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "DSP/OrbitEngine.h"
#include "Parameters/OrbitParams.h"

class OrbitAudioProcessor : public juce::AudioProcessor
{
public:
    OrbitAudioProcessor();
    ~OrbitAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "HEX-Q ORBIT"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Written by the audio thread, polled by the UI timer (lock-free).
    std::atomic<float> meterCorrelation { 1.0f };
    std::atomic<float> meterMonoSumDb { 0.0f };

private:
    orbitp::ParamPtrs params;
    hexq::orbit::OrbitEngine engine;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrbitAudioProcessor)
};
