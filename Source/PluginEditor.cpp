#include "PluginEditor.h"
#include "Diag.h"

namespace
{
    hexq::Palette makePalette()
    {
        hexq::Palette p;
        p.accent = juce::Colour (0xff36d6e7);          // ORBIT accent: cyan (everything else is the shared HEX-Q palette)
        return p;
    }
}

OrbitAudioProcessorEditor::OrbitAudioProcessorEditor (OrbitAudioProcessor& p)
    : juce::AudioProcessorEditor (&p),
      proc (p),
      palette (makePalette()),
      laf (palette),
      header (palette, "HEX-Q", "ORBIT", "v1.0.0"),
      footer (palette, "SPACE: MID KEPT, SIDE WIDENED    CENTER + SPACE: CENTER STAYS, SPREAD ADDED    PAN: AUTO-PAN LEFT / RIGHT"),
      modeSpace (palette, "SPACE", "Spread a centered sound outward"),
      modeCenter (palette, "CENTER + SPACE", "Keep the center, add stereo spread"),
      modePan (palette, "PAN", "Auto-pan left and right"),
      meter (palette, p.meterCorrelation, p.meterMonoSumDb),
      preview (palette, p.apvts)
{
    orbitdiag::trace ("editor ctor: members built");
    setLookAndFeel (&laf);

    addAndMakeVisible (header);
    addAndMakeVisible (footer);
    addAndMakeVisible (modeSpace);
    addAndMakeVisible (modeCenter);
    addAndMakeVisible (modePan);
    addAndMakeVisible (meter);
    addChildComponent (preview);

    modeSpace.onClick  = [this] { setMode (0); };
    modeCenter.onClick = [this] { setMode (1); };
    modePan.onClick    = [this] { setMode (2); };

    auto setupField = [this] (juce::Slider& s, const juce::String& name, const char* paramId)
    {
        s.setName (name);
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 150, 26);
        s.setMouseDragSensitivity (220);
        addChildComponent (s);
        sliderAtt.push_back (std::make_unique<SA> (proc.apvts, paramId, s));
    };
    setupField (widthS,  "Width",         "width");
    setupField (spaceS,  "Space Amount",  "space");
    setupField (centerS, "Center Level",  "center");
    setupField (mixS,    "Mix",           "mix");
    setupField (rateS,   "Rate",          "rate");
    setupField (depthS,  "Depth",         "depth");
    setupField (phaseS,  "Stereo Phase",  "phase");

    const juce::StringArray syncNames { "Free", "Tempo Sync" };
    const juce::StringArray divNames { "1/1", "1/2", "1/4", "1/8", "1/16" };
    const juce::StringArray waveNames { "Sine", "Triangle", "Square" };
    syncBox.addItemList (syncNames, 1);
    divBox.addItemList (divNames, 1);
    waveBox.addItemList (waveNames, 1);
    addChildComponent (syncBox);
    addChildComponent (divBox);
    addChildComponent (waveBox);
    comboAtt.push_back (std::make_unique<CA> (proc.apvts, "sync", syncBox));
    comboAtt.push_back (std::make_unique<CA> (proc.apvts, "division", divBox));
    comboAtt.push_back (std::make_unique<CA> (proc.apvts, "waveform", waveBox));

    monoCheckButton.setClickingTogglesState (true);
    addAndMakeVisible (monoCheckButton);
    buttonAtt.push_back (std::make_unique<BA> (proc.apvts, "monoCheck", monoCheckButton));
    buttonAtt.push_back (std::make_unique<BA> (proc.apvts, "bypass", header.getBypassButton()));
    orbitdiag::trace ("editor ctor: widgets and attachments ready");

    for (auto* id : { "mode", "sync", "waveform", "depth", "phase" })
        proc.apvts.addParameterListener (id, this);

    applyModeToUi();

    setSize (860, 520);       // must stay LAST: it calls resized()
    orbitdiag::trace ("editor ctor: done");
}

OrbitAudioProcessorEditor::~OrbitAudioProcessorEditor()
{
    orbitdiag::trace ("editor dtor");
    cancelPendingUpdate();
    for (auto* id : { "mode", "sync", "waveform", "depth", "phase" })
        proc.apvts.removeParameterListener (id, this);
    setLookAndFeel (nullptr);
}

int OrbitAudioProcessorEditor::currentMode() const
{
    return juce::jlimit (0, 2, (int) proc.apvts.getRawParameterValue ("mode")->load());
}

void OrbitAudioProcessorEditor::setMode (int mode)
{
    if (auto* p = proc.apvts.getParameter ("mode"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) mode));
        p->endChangeGesture();
    }
}

void OrbitAudioProcessorEditor::applyModeToUi()
{
    const int m = currentMode();
    const bool space = (m == 0), cs = (m == 1), pan = (m == 2);

    modeSpace.setToggleState (space, juce::dontSendNotification);
    modeCenter.setToggleState (cs, juce::dontSendNotification);
    modePan.setToggleState (pan, juce::dontSendNotification);

    widthS.setVisible (space || cs);
    spaceS.setVisible (space || cs);
    centerS.setVisible (cs);
    mixS.setVisible (cs || pan);
    mixS.setName (cs ? "Wet / Dry" : "Mix");

    rateS.setVisible (pan);
    depthS.setVisible (pan);
    phaseS.setVisible (pan);
    syncBox.setVisible (pan);
    divBox.setVisible (pan);
    waveBox.setVisible (pan);
    preview.setVisible (pan);

    const bool synced = proc.apvts.getRawParameterValue ("sync")->load() > 0.5f;
    rateS.setEnabled (! synced);       // Free: rate in Hz.  Tempo Sync: division
    divBox.setEnabled (synced);

    layoutPanel();
    preview.repaint();
    repaint();
}

void OrbitAudioProcessorEditor::layoutPanel()
{
    const int m = currentMode();
    auto inner = panelArea.reduced (14, 12);

    auto strip = inner.removeFromBottom (38);
    inner.removeFromBottom (10);
    meter.setBounds (strip.removeFromLeft (strip.getWidth() - 150));
    strip.removeFromLeft (20);
    monoCheckButton.setBounds (strip.withSizeKeepingCentre (130, 30));

    const int fw = 150, gap = 24, fh = 66;
    auto cell = [&] (int col, int y) { return juce::Rectangle<int> (inner.getX() + col * (fw + gap), y, fw, fh); };

    for (auto& c : comboCaptions)
        c.visible = false;

    if (m == 0)
    {
        const int y = inner.getCentreY() - fh / 2;
        widthS.setBounds (cell (0, y));
        spaceS.setBounds (cell (1, y));
    }
    else if (m == 1)
    {
        const int y = inner.getCentreY() - fh / 2;
        centerS.setBounds (cell (0, y));
        spaceS.setBounds (cell (1, y));
        widthS.setBounds (cell (2, y));
        mixS.setBounds (cell (3, y));
    }
    else
    {
        preview.setBounds (inner.removeFromTop (80));
        inner.removeFromTop (10);
        const int y1 = inner.getY(), y2 = y1 + fh + 10;

        auto place = [&] (juce::ComboBox& box, int col, int y, const char* text, Caption& cap)
        {
            const auto c = cell (col, y);
            cap.text = text;
            cap.area = juce::Rectangle<int> (c.getX(), c.getY(), c.getWidth(), 16);
            cap.visible = true;
            box.setBounds (c.getX(), c.getBottom() - 30, c.getWidth(), 30);
        };
        place (syncBox, 0, y1, "SYNC", comboCaptions[0]);
        rateS.setBounds (cell (1, y1));
        place (divBox, 2, y1, "DIVISION", comboCaptions[1]);
        place (waveBox, 3, y1, "WAVEFORM", comboCaptions[2]);

        depthS.setBounds (cell (0, y2));
        phaseS.setBounds (cell (1, y2));
        mixS.setBounds (cell (2, y2));
    }
}

void OrbitAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (palette.background);

    g.setColour (palette.panel);
    g.fillRoundedRectangle (panelArea.toFloat(), hexq::layout::corner);
    g.setColour (palette.border);
    g.drawRoundedRectangle (panelArea.toFloat().reduced (0.5f), hexq::layout::corner, 1.0f);

    g.setColour (palette.textDim);
    g.setFont (hexq::makeFont (11.0f, true));
    for (const auto& c : comboCaptions)
        if (c.visible)
            g.drawText (c.text, c.area, juce::Justification::centredLeft, false);
}

void OrbitAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    header.setBounds (r.removeFromTop (hexq::layout::headerHeight));
    footer.setBounds (r.removeFromBottom (hexq::layout::footerHeight));

    r = r.reduced (hexq::layout::margin, 0);
    r.removeFromTop (8);

    auto row = r.removeFromTop (84);
    const int gap = hexq::layout::gap;
    const int bw = (row.getWidth() - 2 * gap) / 3;
    modeSpace.setBounds (row.removeFromLeft (bw));
    row.removeFromLeft (gap);
    modeCenter.setBounds (row.removeFromLeft (bw));
    row.removeFromLeft (gap);
    modePan.setBounds (row);

    r.removeFromTop (gap);
    r.removeFromBottom (6);
    panelArea = r;

    layoutPanel();
}
