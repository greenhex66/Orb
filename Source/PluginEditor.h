#pragma once
#include "PluginProcessor.h"
#include "HexQ/HexQLookAndFeel.h"
#include "HexQ/HexQHeader.h"
#include "HexQ/HexQFooter.h"
#include "Orbit/ModeButton.h"
#include "Orbit/MonoMeter.h"
#include "Orbit/PanPreview.h"
#include <array>
#include <memory>

class OrbitAudioProcessorEditor : public juce::AudioProcessorEditor,
                                  private juce::AsyncUpdater,
                                  private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit OrbitAudioProcessorEditor (OrbitAudioProcessor&);
    ~OrbitAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void parameterChanged (const juce::String&, float) override { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override { applyModeToUi(); }

    int currentMode() const;
    void setMode (int mode);
    void applyModeToUi();
    void layoutPanel();

    OrbitAudioProcessor& proc;
    hexq::Palette palette;
    hexq::HexQLookAndFeel laf;

    hexq::HexQHeader header;
    hexq::HexQFooter footer;

    orbitui::ModeButton modeSpace, modeCenter, modePan;

    juce::Slider widthS, spaceS, centerS, mixS, rateS, depthS, phaseS;
    juce::ComboBox syncBox, divBox, waveBox;
    juce::TextButton monoCheckButton { "MONO CHECK" };

    orbitui::MonoMeter meter;
    orbitui::PanPreview preview;

    struct Caption { juce::String text; juce::Rectangle<int> area; bool visible = false; };
    std::array<Caption, 3> comboCaptions;
    juce::Rectangle<int> panelArea;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::vector<std::unique_ptr<SA>> sliderAtt;
    std::vector<std::unique_ptr<CA>> comboAtt;
    std::vector<std::unique_ptr<BA>> buttonAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OrbitAudioProcessorEditor)
};
