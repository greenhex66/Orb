#pragma once
#include "../HexQ/HexQTheme.h"

namespace orbitui
{
// Large mode selector: radio dot + title + one-line description.
class ModeButton : public juce::Button
{
public:
    ModeButton (const hexq::Palette& p, const juce::String& titleText, const juce::String& subtitleText)
        : juce::Button (titleText), palette (p), title (titleText), subtitle (subtitleText)
    {
        setClickingTogglesState (true);
        setRadioGroupId (2001);
    }

    void paintButton (juce::Graphics& g, bool hover, bool) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = getToggleState();

        g.setColour (on ? palette.accent.withAlpha (0.12f) : palette.panel);
        g.fillRoundedRectangle (r, hexq::layout::corner);
        g.setColour (on ? palette.accent : (hover ? palette.textDim : palette.border));
        g.drawRoundedRectangle (r, hexq::layout::corner, on ? 1.6f : 1.0f);

        const juce::Rectangle<float> dot (r.getX() + 20.0f, r.getCentreY() - 7.0f, 14.0f, 14.0f);
        g.setColour (on ? palette.accent : palette.border);
        g.drawEllipse (dot, 1.5f);
        if (on)
            g.fillEllipse (dot.reduced (3.5f));

        auto text = r.withTrimmedLeft (50.0f).reduced (6.0f, 0.0f);
        g.setFont (hexq::makeFont (16.0f, true));
        g.setColour (on ? palette.accent : palette.text);
        g.drawText (title, text.removeFromTop (r.getHeight() * 0.5f + 4.0f), juce::Justification::bottomLeft, false);

        g.setFont (hexq::makeFont (11.0f));
        g.setColour (palette.textDim);
        g.drawText (subtitle, text, juce::Justification::topLeft, false);
    }

private:
    hexq::Palette palette;
    juce::String title, subtitle;
};
} // namespace orbitui
