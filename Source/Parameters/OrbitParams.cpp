#include "OrbitParams.h"
#include <cmath>

namespace orbitp
{
static juce::NormalisableRange<float> logRange (float lo, float hi)
{
    return juce::NormalisableRange<float> (
        lo, hi,
        [] (float s, float e, float p) { return s * std::pow (e / s, p); },
        [] (float s, float e, float v) { return std::log (juce::jlimit (s, e, v) / s) / std::log (e / s); },
        [] (float s, float e, float v) { return juce::jlimit (s, e, v); });
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const StringArray modeNames { "Space", "Center + Space", "Pan" };
    const StringArray syncNames { "Free", "Tempo Sync" };
    const StringArray divNames { "1/1", "1/2", "1/4", "1/8", "1/16" };
    const StringArray waveNames { "Sine", "Triangle", "Square" };

    auto percent = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String ((int) std::round (v)) + "%"; })
        .withValueFromStringFunction ([] (const String& t) { return jlimit (0.0f, 200.0f, t.getFloatValue()); });
    auto hz = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " Hz"; })
        .withValueFromStringFunction ([] (const String& t) { return jlimit (0.05f, 10.0f, t.getFloatValue()); });
    auto deg = AudioParameterFloatAttributes()
        .withStringFromValueFunction ([] (float v, int) { return String ((int) std::round (v)) + " deg"; })
        .withValueFromStringFunction ([] (const String& t) { return jlimit (0.0f, 180.0f, t.getFloatValue()); });

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", modeNames, 0));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "bypass", 1 }, "Bypass", false));
    layout.add (std::make_unique<AudioParameterBool>   (ParameterID { "monoCheck", 1 }, "Mono Check", false));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 },  "Width",        NormalisableRange<float> (0.0f, 200.0f, 1.0f), 100.0f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "space", 1 },  "Space Amount", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 35.0f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "center", 1 }, "Center Level", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, percent));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 },    "Wet/Dry Mix",  NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, percent));

    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "rate", 1 }, "Pan Rate", logRange (0.05f, 10.0f), 1.0f, hz));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "sync", 1 }, "Pan Sync", syncNames, 0));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Pan Division", divNames, 2));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "depth", 1 }, "Pan Depth", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f, percent));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "waveform", 1 }, "Pan Waveform", waveNames, 0));
    layout.add (std::make_unique<AudioParameterFloat>  (ParameterID { "phase", 1 }, "Stereo Phase", NormalisableRange<float> (0.0f, 180.0f, 1.0f), 180.0f, deg));

    return layout;
}

ParamPtrs getParamPtrs (juce::AudioProcessorValueTreeState& apvts)
{
    ParamPtrs p;
    p.mode      = apvts.getRawParameterValue ("mode");
    p.bypass    = apvts.getRawParameterValue ("bypass");
    p.monoCheck = apvts.getRawParameterValue ("monoCheck");
    p.width     = apvts.getRawParameterValue ("width");
    p.space     = apvts.getRawParameterValue ("space");
    p.center    = apvts.getRawParameterValue ("center");
    p.mix       = apvts.getRawParameterValue ("mix");
    p.rate      = apvts.getRawParameterValue ("rate");
    p.sync      = apvts.getRawParameterValue ("sync");
    p.division  = apvts.getRawParameterValue ("division");
    p.depth     = apvts.getRawParameterValue ("depth");
    p.waveform  = apvts.getRawParameterValue ("waveform");
    p.phase     = apvts.getRawParameterValue ("phase");
    return p;
}

void fillSettings (const ParamPtrs& p, hexq::orbit::Settings& s) noexcept
{
    static const float beatsTable[5] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f };

    s.mode      = (hexq::orbit::Mode) juce::jlimit (0, 2, (int) p.mode->load());
    s.bypass    = p.bypass->load() > 0.5f;
    s.monoCheck = p.monoCheck->load() > 0.5f;
    s.width     = p.width->load() * 0.01f;
    s.space     = p.space->load() * 0.01f;
    s.center    = p.center->load() * 0.01f;
    s.mix       = p.mix->load() * 0.01f;
    s.rateHz    = p.rate->load();
    s.sync      = p.sync->load() > 0.5f;
    s.beatsPerCycle = beatsTable[juce::jlimit (0, 4, (int) p.division->load())];
    s.depth     = p.depth->load() * 0.01f;
    s.wave      = (hexq::orbit::Wave) juce::jlimit (0, 2, (int) p.waveform->load());
    s.phaseDeg  = p.phase->load();
}
} // namespace orbitp
