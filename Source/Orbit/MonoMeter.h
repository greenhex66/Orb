#pragma once
#include "../HexQ/HexQTheme.h"
#include <atomic>
#include <cmath>

namespace orbitui
{
// Mono-compatibility indicator. Polls two lock-free values written by the audio thread (15 Hz);
// repaints only when the shown numbers actually change.
class MonoMeter : public juce::Component, private juce::Timer
{
public:
    MonoMeter (const hexq::Palette& p, std::atomic<float>& correlationSource, std::atomic<float>& monoSumSource)
        : palette (p), corrSrc (correlationSource), dbSrc (monoSumSource)
    {
        startTimerHz (15);
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();

        g.setFont (hexq::makeFont (11.0f, true));
        g.setColour (palette.textDim);
        g.drawText ("MONO COMPATIBILITY", b.withHeight (16.0f), juce::Justification::centredLeft, false);

        const bool ok = corr >= 0.0f;
        g.setColour (ok ? palette.accent : palette.text);
        g.drawText (juce::String (ok ? "OK" : "CHECK PHASE") + "    CORR " + juce::String (corr, 2)
                        + "    MONO SUM " + juce::String (monoDb, 1) + " dB",
                    b.withHeight (16.0f), juce::Justification::centredRight, false);

        const juce::Rectangle<float> track (b.getX(), b.getBottom() - 12.0f, b.getWidth(), 6.0f);
        g.setColour (palette.border);
        g.fillRoundedRectangle (track, 3.0f);

        const float cx = track.getCentreX();
        const float x = cx + juce::jlimit (-1.0f, 1.0f, corr) * track.getWidth() * 0.5f;
        g.setColour (ok ? palette.accent : palette.textDim);
        g.fillRoundedRectangle (juce::Rectangle<float> (juce::jmin (cx, x), track.getY(), std::abs (x - cx), track.getHeight()), 3.0f);
        g.setColour (palette.text);
        g.fillRect (cx - 0.5f, track.getY() - 3.0f, 1.0f, track.getHeight() + 6.0f);
    }

private:
    void timerCallback() override
    {
        const float c = corrSrc.load (std::memory_order_relaxed);
        const float d = dbSrc.load (std::memory_order_relaxed);
        if (std::abs (c - corr) > 0.005f || std::abs (d - monoDb) > 0.05f)
        {
            corr = c;
            monoDb = d;
            repaint();
        }
    }

    hexq::Palette palette;
    std::atomic<float>& corrSrc;
    std::atomic<float>& dbSrc;
    float corr = 1.0f, monoDb = 0.0f;
};
} // namespace orbitui
