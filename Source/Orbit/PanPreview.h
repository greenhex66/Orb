#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "../HexQ/HexQTheme.h"
#include "../DSP/OrbitEngine.h"

namespace orbitui
{
// Static picture of one LFO cycle: left gain (accent) and right gain (grey) for the current
// waveform / depth / stereo phase. Uses the same functions as the audio engine. No animation.
class PanPreview : public juce::Component
{
public:
    PanPreview (const hexq::Palette& p, juce::AudioProcessorValueTreeState& s) : palette (p), state (s) {}

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (palette.background);
        g.fillRoundedRectangle (b, hexq::layout::controlCorner);
        g.setColour (palette.border);
        g.drawRoundedRectangle (b.reduced (0.5f), hexq::layout::controlCorner, 1.0f);

        const float depth = state.getRawParameterValue ("depth")->load() * 0.01f;
        const auto wave = (hexq::orbit::Wave) juce::jlimit (0, 2, (int) state.getRawParameterValue ("waveform")->load());
        const float phase = state.getRawParameterValue ("phase")->load() / 360.0f;

        const auto plot = b.reduced (46.0f, 12.0f);
        g.setColour (palette.border.withAlpha (0.5f));
        g.drawHorizontalLine ((int) plot.getY(), plot.getX(), plot.getRight());
        g.drawHorizontalLine ((int) plot.getBottom(), plot.getX(), plot.getRight());
        g.drawVerticalLine ((int) (plot.getX() + plot.getWidth() * 0.5f), plot.getY(), plot.getBottom());

        juce::Path left, right;
        const int steps = 240;
        for (int i = 0; i <= steps; ++i)
        {
            const float ph = (float) i / (float) steps;
            const float x = plot.getX() + plot.getWidth() * ph;
            const float yL = plot.getBottom() - plot.getHeight() * hexq::orbit::panGain (hexq::orbit::lfoShape (wave, ph), depth);
            const float yR = plot.getBottom() - plot.getHeight() * hexq::orbit::panGain (hexq::orbit::lfoShape (wave, ph + phase), depth);
            if (i == 0) { left.startNewSubPath (x, yL); right.startNewSubPath (x, yR); }
            else        { left.lineTo (x, yL);          right.lineTo (x, yR); }
        }

        g.setColour (palette.textDim);
        g.strokePath (right, juce::PathStrokeType (1.6f));
        g.setColour (palette.accent);
        g.strokePath (left, juce::PathStrokeType (2.0f));

        g.setFont (hexq::makeFont (11.0f, true));
        g.setColour (palette.accent);
        g.drawText ("L", 12, (int) b.getY() + 10, 28, 16, juce::Justification::centredRight, false);
        g.setColour (palette.textDim);
        g.drawText ("R", 12, (int) b.getY() + 28, 28, 16, juce::Justification::centredRight, false);
        g.drawText ("ONE CYCLE", getWidth() - 120, getHeight() - 20, 108, 14, juce::Justification::centredRight, false);
    }

private:
    hexq::Palette palette;
    juce::AudioProcessorValueTreeState& state;
};
} // namespace orbitui
