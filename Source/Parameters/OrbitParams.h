#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../DSP/OrbitEngine.h"

// HEX-Q ORBIT - parameter definitions. IDs are stable across versions (never rename them).
//   mode        Space / Center + Space / Pan
//   bypass      global bypass            monoCheck   audition the mono sum
//   width       0..200 %                 space       0..100 %
//   center      0..100 %                 mix         0..100 % (Center+Space wet/dry, Pan mix)
//   rate        0.05..10 Hz              sync        Free / Tempo Sync
//   division    1/1 .. 1/16              depth       0..100 %
//   waveform    Sine / Triangle / Square phase       0..180 deg (right-channel LFO offset)
namespace orbitp
{
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

struct ParamPtrs
{
    std::atomic<float>* mode = nullptr;
    std::atomic<float>* bypass = nullptr;
    std::atomic<float>* monoCheck = nullptr;
    std::atomic<float>* width = nullptr;
    std::atomic<float>* space = nullptr;
    std::atomic<float>* center = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* rate = nullptr;
    std::atomic<float>* sync = nullptr;
    std::atomic<float>* division = nullptr;
    std::atomic<float>* depth = nullptr;
    std::atomic<float>* waveform = nullptr;
    std::atomic<float>* phase = nullptr;
};

ParamPtrs getParamPtrs (juce::AudioProcessorValueTreeState&);

// Audio-thread safe (only atomic loads).
void fillSettings (const ParamPtrs&, hexq::orbit::Settings&) noexcept;
} // namespace orbitp
