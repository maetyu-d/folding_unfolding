#include "CityToolbar.h"

#include <cmath>

namespace
{
constexpr auto gap = 10;
constexpr auto panelRadius = 18.0f;

juce::Colour panelColour() { return juce::Colour (0xeef8fbf1); }
juce::Colour panelLineColour() { return juce::Colour (0xb8ffffff); }
juce::Colour textColour() { return juce::Colour (0xff263832); }
juce::Colour quietTextColour() { return juce::Colour (0xff65766f); }
juce::Colour accentColour() { return juce::Colour (0xff32c9b1); }
juce::Colour activeButtonColour() { return juce::Colour (0xffffd766); }
juce::Colour inactiveButtonColour() { return juce::Colour (0xbaffffff); }
juce::Colour inkColour() { return juce::Colour (0xff18241f); }

juce::Colour colourForMode (CityToolbar::BuildMode mode)
{
    switch (mode)
    {
        case CityToolbar::BuildMode::select: return juce::Colour (0xfff3f1dc);
        case CityToolbar::BuildMode::polygon: return juce::Colour (0xffffcf5f);
        case CityToolbar::BuildMode::platter: return juce::Colour (0xff75d7ff);
        case CityToolbar::BuildMode::block: return juce::Colour (0xffa8e06f);
        case CityToolbar::BuildMode::plank: return juce::Colour (0xffff9f6e);
        case CityToolbar::BuildMode::cable: return juce::Colour (0xff60f0b2);
        default: break;
    }

    return activeButtonColour();
}

juce::Colour colourForMeter (int sides)
{
    switch (juce::jlimit (3, 8, sides))
    {
        case 3: return juce::Colour (0xffff6f91);
        case 4: return juce::Colour (0xff62b6ff);
        case 5: return juce::Colour (0xffffc857);
        case 6: return juce::Colour (0xff75db8a);
        case 7: return juce::Colour (0xffc79bff);
        case 8: return juce::Colour (0xff64dfdf);
        default: break;
    }

    return activeButtonColour();
}

struct RateOption
{
    int id = 0;
    const char* label = "";
    float division = 1.0f;
};

constexpr RateOption rateOptions[] = {
    { 1,  "1/1",   1.0f },
    { 2,  "1/2.",  4.0f / 3.0f },
    { 3,  "1/2",   2.0f },
    { 4,  "1/2T",  3.0f },
    { 5,  "1/4.",  8.0f / 3.0f },
    { 6,  "1/4",   4.0f },
    { 7,  "1/4T",  6.0f },
    { 8,  "1/8.",  16.0f / 3.0f },
    { 9,  "1/8",   8.0f },
    { 10, "1/8T",  12.0f },
    { 11, "1/16.", 32.0f / 3.0f },
    { 12, "1/16",  16.0f },
    { 13, "1/16T", 24.0f },
    { 14, "1/32",  32.0f },
};

constexpr RateOption platterRateOptions[] = {
    { 1, "1/4 bar", 4.0f },
    { 2, "1/2 bar", 2.0f },
    { 3, "1 bar",   1.0f },
    { 4, "2 bars",  0.5f },
    { 5, "4 bars",  0.25f },
    { 6, "8 bars",  0.125f },
};

float beatPulseForTempo (float bpm, bool playing)
{
    if (! playing)
        return 0.12f;

    const auto beatsPerSecond = juce::jmax (1.0f, bpm) / 60.0f;
    const auto beatPhase = std::fmod (static_cast<float> (juce::Time::getMillisecondCounterHiRes() * 0.001
                                                          * static_cast<double> (beatsPerSecond)),
                                      1.0f);
    return 0.18f + 0.82f * std::exp (-beatPhase * 8.0f);
}

juce::Rectangle<int> takeRow (juce::Rectangle<int>& bounds, int height)
{
    auto slice = bounds.removeFromTop (height);
    bounds.removeFromTop (gap);
    return slice;
}

void paintPanel (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    g.setColour (juce::Colours::black.withAlpha (0.08f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 5.0f), panelRadius);

    g.setColour (panelColour());
    g.fillRoundedRectangle (bounds, panelRadius);

    g.setColour (panelLineColour());
    g.drawRoundedRectangle (bounds, panelRadius, 1.0f);
}

template <size_t size>
const RateOption& closestRateOption (const RateOption (&options)[size], float division)
{
    auto* best = &options[0];
    auto bestDistance = std::abs (division - best->division);

    for (const auto& option : options)
    {
        const auto distance = std::abs (division - option.division);

        if (distance < bestDistance)
        {
            best = &option;
            bestDistance = distance;
        }
    }

    return *best;
}
}

CityToolbar::CityToolbar()
{
    selectModeButton.setButtonText ("Select");
    polygonModeButton.setButtonText ("Poly");
    platterModeButton.setButtonText ("Spin");
    blockModeButton.setButtonText ("Block");
    plankModeButton.setButtonText ("Plank");
    cableModeButton.setButtonText ("Power");

    selectModeButton.onClick = [this]
    {
        if (! suppressCallbacks && onBuildModeChanged)
            onBuildModeChanged (BuildMode::select);
    };

    polygonModeButton.onClick = [this]
    {
        if (! suppressCallbacks && onBuildModeChanged)
            onBuildModeChanged (BuildMode::polygon);
    };

    platterModeButton.onClick = [this]
    {
        if (! suppressCallbacks && onBuildModeChanged)
            onBuildModeChanged (BuildMode::platter);
    };

    blockModeButton.onClick = [this]
    {
        if (! suppressCallbacks && onBuildModeChanged)
            onBuildModeChanged (BuildMode::block);
    };

    plankModeButton.onClick = [this]
    {
        if (! suppressCallbacks && onBuildModeChanged)
            onBuildModeChanged (BuildMode::plank);
    };

    cableModeButton.onClick = [this]
    {
        if (! suppressCallbacks && onBuildModeChanged)
            onBuildModeChanged (BuildMode::cable);
    };

    addAndMakeVisible (selectModeButton);
    addAndMakeVisible (polygonModeButton);
    addAndMakeVisible (platterModeButton);
    addAndMakeVisible (blockModeButton);
    addAndMakeVisible (plankModeButton);
    addAndMakeVisible (cableModeButton);

    for (size_t i = 0; i < sideButtons.size(); ++i)
    {
        const auto sides = static_cast<int> (i) + 3;
        auto& button = sideButtons[i];
        button.setButtonText (juce::String (sides));
        button.setClickingTogglesState (false);
        button.onClick = [this, sides]
        {
            if (! suppressCallbacks && onSidesChanged)
                onSidesChanged (sides);
        };
        addAndMakeVisible (button);
    }

    configureSlider (radiusControl, "Sq", 1.0, 28.0, 1.0, "");
    configureSlider (flapControl, "F", 8.0, 110.0, 1.0, "");
    configureSlider (zoomControl, "Zoom", 0.28, 3.2, 0.01, "x");
    configureSlider (rotationControl, "Rot", 0.0, 360.0, 1.0, juce::String::fromUTF8 (u8"\u00b0"));
    configureSlider (tipPitchControl, "Pitch", 0.0, 96.0, 1.0, "");
    tipPitchControl.slider.textFromValueFunction = [] (double value)
    {
        return value <= 0.0 ? juce::String ("X") : juce::String (juce::roundToInt (value));
    };
    tipPitchControl.slider.valueFromTextFunction = [] (const juce::String& text)
    {
        const auto trimmed = text.trim();
        if (trimmed.equalsIgnoreCase ("x"))
            return 0.0;

        return juce::jlimit (0.0, 96.0, trimmed.getDoubleValue());
    };
    configureSlider (phaseControl, "Deg", 0.0, 360.0, 1.0, juce::String::fromUTF8 (u8"\u00b0"));
    configureSlider (tempoControl, "BPM", 40.0, 240.0, 1.0, "");
    tempoControl.label.setVisible (false);
    tempoControl.slider.setVisible (false);
    tempoValueBox.setText ("120", juce::dontSendNotification);
    tempoValueBox.setJustificationType (juce::Justification::centred);
    tempoValueBox.setEditable (true, true, false);
    tempoValueBox.setColour (juce::Label::backgroundColourId, juce::Colours::white.withAlpha (0.68f));
    tempoValueBox.setColour (juce::Label::outlineColourId, juce::Colour (0x809db0a7));
    tempoValueBox.setColour (juce::Label::textColourId, textColour());
    tempoValueBox.setColour (juce::Label::textWhenEditingColourId, textColour());
    tempoValueBox.setColour (juce::Label::backgroundWhenEditingColourId, juce::Colours::white.withAlpha (0.92f));
    tempoValueBox.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    tempoValueBox.onTextChange = [this]
    {
        if (suppressCallbacks)
            return;

        const auto bpm = juce::jlimit (40.0f, 240.0f, static_cast<float> (tempoValueBox.getText().getFloatValue()));
        tempoValueBox.setText (juce::String (juce::roundToInt (bpm)), juce::dontSendNotification);

        if (onTempoChanged)
            onTempoChanged (bpm);
    };
    addAndMakeVisible (tempoValueBox);
    configureSlider (standsControl, "St", 1.0, 8.0, 1.0, "");
    configureSlider (diameterControl, "Sq", 1.0, 64.0, 1.0, "");
    configureSlider (blockLevelsControl, "Lv", 1.0, 16.0, 1.0, "");
    configureSlider (blockSizeControl, "Sq", 1.0, 24.0, 1.0, "");
    configureSlider (switchOffTimeControl, "Off", 1.0, 32.0, 0.5, "s");

    rateControl.label.setText ("Rt", juce::dontSendNotification);
    rateControl.label.setJustificationType (juce::Justification::centredLeft);
    rateControl.label.setColour (juce::Label::textColourId, quietTextColour());
    rateControl.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    rateControl.combo.setColour (juce::ComboBox::backgroundColourId, inactiveButtonColour());
    rateControl.combo.setColour (juce::ComboBox::outlineColourId, panelLineColour());
    rateControl.combo.setColour (juce::ComboBox::textColourId, textColour());
    rateControl.combo.setColour (juce::ComboBox::arrowColourId, accentColour());
    updateRateOptions (BuildMode::polygon);

    rateControl.combo.onChange = [this]
    {
        if (! suppressCallbacks && onRateDivisionChanged)
            onRateDivisionChanged (selectedRateDivision());
    };
    addAndMakeVisible (rateControl.label);
    addAndMakeVisible (rateControl.combo);

    switchActivationControl.label.setText ("Act", juce::dontSendNotification);
    switchActivationControl.label.setJustificationType (juce::Justification::centredLeft);
    switchActivationControl.label.setColour (juce::Label::textColourId, quietTextColour());
    switchActivationControl.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    switchActivationControl.combo.addItem ("Tip", 1);
    switchActivationControl.combo.addItem ("Timed", 2);
    switchActivationControl.combo.setColour (juce::ComboBox::backgroundColourId, inactiveButtonColour());
    switchActivationControl.combo.setColour (juce::ComboBox::outlineColourId, panelLineColour());
    switchActivationControl.combo.setColour (juce::ComboBox::textColourId, textColour());
    switchActivationControl.combo.setColour (juce::ComboBox::arrowColourId, accentColour());
    switchActivationControl.combo.onChange = [this]
    {
        if (! suppressCallbacks && onSwitchActivationModeChanged)
            onSwitchActivationModeChanged (switchActivationControl.combo.getSelectedId() == 2
                                           ? PowerSwitchActivationMode::timedOff
                                           : PowerSwitchActivationMode::tipToggle);
    };
    addAndMakeVisible (switchActivationControl.label);
    addAndMakeVisible (switchActivationControl.combo);

    switchRetriggerControl.label.setText ("Re", juce::dontSendNotification);
    switchRetriggerControl.label.setJustificationType (juce::Justification::centredLeft);
    switchRetriggerControl.label.setColour (juce::Label::textColourId, quietTextColour());
    switchRetriggerControl.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    switchRetriggerControl.combo.addItem ("Ignore", 1);
    switchRetriggerControl.combo.addItem ("On", 2);
    switchRetriggerControl.combo.setColour (juce::ComboBox::backgroundColourId, inactiveButtonColour());
    switchRetriggerControl.combo.setColour (juce::ComboBox::outlineColourId, panelLineColour());
    switchRetriggerControl.combo.setColour (juce::ComboBox::textColourId, textColour());
    switchRetriggerControl.combo.setColour (juce::ComboBox::arrowColourId, accentColour());
    switchRetriggerControl.combo.onChange = [this]
    {
        if (! suppressCallbacks && onSwitchRetriggerPolicyChanged)
            onSwitchRetriggerPolicyChanged (switchRetriggerControl.combo.getSelectedId() == 2
                                            ? PowerSwitchRetriggerPolicy::turnOnWhileOff
                                            : PowerSwitchRetriggerPolicy::ignoreWhileOff);
    };
    addAndMakeVisible (switchRetriggerControl.label);
    addAndMakeVisible (switchRetriggerControl.combo);

    radiusControl.slider.onValueChange = [this]
    {
        if (! suppressCallbacks && onRadiusChanged)
            onRadiusChanged (static_cast<float> (radiusControl.slider.getValue()));
    };

    flapControl.slider.onValueChange = [this]
    {
        if (! suppressCallbacks && onFlapDepthChanged)
            onFlapDepthChanged (static_cast<float> (flapControl.slider.getValue()));
    };

    zoomControl.slider.onValueChange = [this]
    {
        if (! suppressCallbacks && onZoomChanged)
            onZoomChanged (static_cast<float> (zoomControl.slider.getValue()));
    };

    rotationControl.slider.onValueChange = [this]
    {
        if (! suppressCallbacks && onRotationChanged)
            onRotationChanged (static_cast<float> (rotationControl.slider.getValue()));
    };

    tipPitchControl.slider.onValueChange = [this]
    {
        if (! suppressCallbacks && onTipPitchChanged)
            onTipPitchChanged (static_cast<float> (tipPitchControl.slider.getValue()));
    };

    phaseControl.slider.onValueChange = [this]
    {
        if (! suppressCallbacks && onPhaseChanged)
            onPhaseChanged (static_cast<float> (phaseControl.slider.getValue()));
    };

    tempoControl.slider.onValueChange = [this]
    {
        if (! suppressCallbacks && onTempoChanged)
            onTempoChanged (static_cast<float> (tempoControl.slider.getValue()));
    };

    standsControl.slider.onValueChange = [this]
    {
        if (! suppressCallbacks && onPlatterStandsChanged)
            onPlatterStandsChanged (juce::roundToInt (standsControl.slider.getValue()));
    };

    diameterControl.slider.onValueChange = [this]
    {
        if (! suppressCallbacks && onPlatterDiameterChanged)
            onPlatterDiameterChanged (static_cast<float> (diameterControl.slider.getValue()));
    };

    blockLevelsControl.slider.onValueChange = [this]
    {
        if (! suppressCallbacks && onBlockLevelsChanged)
            onBlockLevelsChanged (juce::roundToInt (blockLevelsControl.slider.getValue()));
    };

    blockSizeControl.slider.onValueChange = [this]
    {
        if (! suppressCallbacks && onBlockSizeChanged)
            onBlockSizeChanged (static_cast<float> (blockSizeControl.slider.getValue()));
    };

    switchOffTimeControl.slider.onValueChange = [this]
    {
        if (! suppressCallbacks && onSwitchOffDurationChanged)
            onSwitchOffDurationChanged (static_cast<float> (switchOffTimeControl.slider.getValue()));
    };

    deleteButton.onClick = [this]
    {
        if (! suppressCallbacks && onDeleteRequested)
            onDeleteRequested();
    };
    deleteButton.setButtonText ("Delete");
    deleteButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0x55ffd766));
    deleteButton.setColour (juce::TextButton::buttonOnColourId, activeButtonColour());
    deleteButton.setColour (juce::TextButton::textColourOffId, textColour());
    deleteButton.setColour (juce::TextButton::textColourOnId, inkColour());
    addAndMakeVisible (deleteButton);

    clearButton.onClick = [this]
    {
        if (! suppressCallbacks && onClearRequested)
            onClearRequested();
    };
    clearButton.setButtonText ("Clear");
    clearButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0x55ff8fb1));
    clearButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffff8fb1));
    clearButton.setColour (juce::TextButton::textColourOffId, textColour());
    clearButton.setColour (juce::TextButton::textColourOnId, inkColour());
    addAndMakeVisible (clearButton);

    playButton.onClick = [this]
    {
        if (! suppressCallbacks && onPlayRequested)
            onPlayRequested();
    };
    pauseButton.onClick = [this]
    {
        if (! suppressCallbacks && onPauseRequested)
            onPauseRequested();
    };
    stopButton.onClick = [this]
    {
        if (! suppressCallbacks && onStopRequested)
            onStopRequested();
    };

    for (auto* button : { &playButton, &pauseButton, &stopButton })
    {
        button->setColour (juce::TextButton::buttonColourId, inactiveButtonColour());
        button->setColour (juce::TextButton::buttonOnColourId, activeButtonColour());
        button->setColour (juce::TextButton::textColourOffId, textColour());
        button->setColour (juce::TextButton::textColourOnId, inkColour());
        addAndMakeVisible (*button);
    }

    startTimerHz (30);
}

void CityToolbar::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    auto globalPanel = bounds.removeFromTop (116.0f);
    paintPanel (g, globalPanel);
    bounds.removeFromTop (14.0f);
    paintPanel (g, bounds);

    if (! beatLightBounds.isEmpty())
    {
        const auto pulse = beatPulseForTempo (currentTempoBpm, transportPlaying);
        const auto lamp = beatLightBounds.toFloat();
        const auto centre = lamp.getCentre();
        const auto radius = juce::jmin (lamp.getWidth(), lamp.getHeight()) * 0.5f;

        g.setColour (accentColour().withAlpha (0.12f + 0.20f * pulse));
        g.fillEllipse (lamp.expanded (5.0f * pulse));

        g.setColour (textColour().withAlpha (0.16f));
        g.fillEllipse (lamp);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xfffff7a0).withAlpha (0.55f + 0.45f * pulse),
                                                 centre.x - radius * 0.4f,
                                                 centre.y - radius * 0.5f,
                                                 accentColour().withAlpha (0.55f + 0.35f * pulse),
                                                 centre.x + radius * 0.5f,
                                                 centre.y + radius * 0.6f,
                                                 true));
        g.fillEllipse (lamp.reduced (2.0f));

        g.setColour (juce::Colours::white.withAlpha (0.38f + 0.42f * pulse));
        g.fillEllipse (lamp.withSizeKeepingCentre (radius * 0.8f, radius * 0.8f)
                            .translated (-radius * 0.18f, -radius * 0.18f));

        g.setColour (textColour().withAlpha (0.34f + 0.42f * pulse));
        g.drawEllipse (lamp.reduced (0.5f), 1.4f);
    }
}

void CityToolbar::resized()
{
    auto bounds = getLocalBounds().reduced (16);
    auto globalBounds = bounds.removeFromTop (84);
    bounds.removeFromTop (48);

    globalBounds.removeFromTop (14);

    auto transportArea = takeRow (globalBounds, 32);
    const auto transportButtonWidth = 64;
    playButton.setBounds (transportArea.removeFromLeft (transportButtonWidth));
    transportArea.removeFromLeft (gap);
    pauseButton.setBounds (transportArea.removeFromLeft (transportButtonWidth));
    transportArea.removeFromLeft (gap);
    stopButton.setBounds (transportArea.removeFromLeft (transportButtonWidth));
    transportArea.removeFromLeft (gap);
    beatLightBounds = transportArea.removeFromLeft (32).reduced (1);
    transportArea.removeFromLeft (gap);
    tempoValueBox.setBounds (transportArea.removeFromLeft (50));

    auto globalUtilityArea = takeRow (globalBounds, 32);
    zoomControl.label.setBounds (globalUtilityArea.removeFromLeft (52));
    zoomControl.slider.setBounds (globalUtilityArea.removeFromLeft (juce::jmax (116, globalUtilityArea.getWidth() - 92)));
    globalUtilityArea.removeFromLeft (gap);
    clearButton.setBounds (globalUtilityArea);

    bounds.removeFromTop (14);
    auto buildArea = takeRow (bounds, 78);
    auto modeTop = buildArea.removeFromTop (34);
    buildArea.removeFromTop (gap);
    auto modeBottom = buildArea.removeFromTop (34);
    const auto modeWidth = (modeTop.getWidth() - gap * 2) / 3;
    selectModeButton.setBounds (modeTop.removeFromLeft (modeWidth));
    modeTop.removeFromLeft (gap);
    polygonModeButton.setBounds (modeTop.removeFromLeft (modeWidth));
    modeTop.removeFromLeft (gap);
    platterModeButton.setBounds (modeTop);
    blockModeButton.setBounds (modeBottom.removeFromLeft (modeWidth));
    modeBottom.removeFromLeft (gap);
    plankModeButton.setBounds (modeBottom.removeFromLeft (modeWidth));
    modeBottom.removeFromLeft (gap);
    cableModeButton.setBounds (modeBottom);

    if (activeBuildMode == BuildMode::polygon)
    {
        auto sidesArea = takeRow (bounds, 72);
        const auto buttonWidth = (sidesArea.getWidth() - gap * 2) / 3;
        const auto buttonHeight = (sidesArea.getHeight() - gap) / 2;

        for (size_t i = 0; i < sideButtons.size(); ++i)
        {
            auto& button = sideButtons[i];
            auto row = i < 3 ? sidesArea.withHeight (buttonHeight)
                             : sidesArea.withTrimmedTop (buttonHeight + gap);
            auto x = static_cast<int> (i % 3) * (buttonWidth + gap);
            button.setBounds (row.withX (sidesArea.getX() + x).withWidth (buttonWidth));
        }
    }

    if (activeBuildMode == BuildMode::polygon || activeBuildMode == BuildMode::platter)
    {
        auto rateArea = takeRow (bounds, 34);
        rateControl.label.setBounds (rateArea.removeFromLeft (44));
        rateControl.combo.setBounds (rateArea);

        auto phaseArea = takeRow (bounds, 34);
        phaseControl.label.setBounds (phaseArea.removeFromLeft (44));
        phaseControl.slider.setBounds (phaseArea);
    }

    if (activeBuildMode == BuildMode::platter)
    {
        for (auto* control : { &standsControl, &diameterControl })
        {
            auto area = takeRow (bounds, 34);
            control->label.setBounds (area.removeFromLeft (44));
            control->slider.setBounds (area);
        }
    }
    else if (activeBuildMode == BuildMode::plank)
    {
        auto lengthArea = takeRow (bounds, 34);
        diameterControl.label.setBounds (lengthArea.removeFromLeft (44));
        diameterControl.slider.setBounds (lengthArea);
    }
    else if (activeBuildMode == BuildMode::block)
    {
        for (auto* control : { &blockLevelsControl, &blockSizeControl })
        {
            auto area = takeRow (bounds, 34);
            control->label.setBounds (area.removeFromLeft (44));
            control->slider.setBounds (area);
        }
    }
    else if (activeBuildMode == BuildMode::cable && activeSwitchSelected)
    {
        auto activationArea = takeRow (bounds, 34);
        switchActivationControl.label.setBounds (activationArea.removeFromLeft (44));
        switchActivationControl.combo.setBounds (activationArea);

        auto offArea = takeRow (bounds, 34);
        switchOffTimeControl.label.setBounds (offArea.removeFromLeft (44));
        switchOffTimeControl.slider.setBounds (offArea);

        auto retriggerArea = takeRow (bounds, 34);
        switchRetriggerControl.label.setBounds (retriggerArea.removeFromLeft (44));
        switchRetriggerControl.combo.setBounds (retriggerArea);
    }

    if (activeBuildMode == BuildMode::polygon)
    {
        for (auto* control : { &radiusControl, &flapControl })
        {
            auto area = takeRow (bounds, 34);
            control->label.setBounds (area.removeFromLeft (44));
            control->slider.setBounds (area);
        }
    }

    if (activeRotationControl)
    {
        auto rotationArea = takeRow (bounds, 34);
        rotationControl.label.setBounds (rotationArea.removeFromLeft (44));
        rotationControl.slider.setBounds (rotationArea);
    }

    if (activeTipSelected)
    {
        auto pitchArea = takeRow (bounds, 34);
        tipPitchControl.label.setBounds (pitchArea.removeFromLeft (50));
        tipPitchControl.slider.setBounds (pitchArea);
    }

    bounds.removeFromTop (4);
    auto utilityArea = takeRow (bounds, 34);
    deleteButton.setBounds (utilityArea);
}

void CityToolbar::setValues (int sides,
                    float radius,
                    float flapDepth,
                    float zoom,
                    float rotationDegrees,
                    bool showRotationControl,
                             float rateDivision,
                             float phaseDegrees,
                             BuildMode toolMode,
                             BuildMode controlsMode,
                             int platterStands,
                             float platterDiameter,
                             int blockLevels,
                             float blockSize,
                             bool tipSelected,
                             float tipPitch,
                             float tempoBpm,
                             bool playing,
                             bool switchSelected,
                             PowerSwitchActivationMode switchActivationMode,
                             float switchOffDuration,
                             PowerSwitchRetriggerPolicy switchRetriggerPolicy)
{
    const juce::ScopedValueSetter<bool> scopedSetter (suppressCallbacks, true);

    activeSwitchSelected = switchSelected;
    activeRotationControl = showRotationControl;
    activeTipSelected = tipSelected;
    selectBuildMode (toolMode, controlsMode);
    selectSideButton (sides);
    radiusControl.slider.setValue (radius, juce::dontSendNotification);
    flapControl.slider.setValue (flapDepth, juce::dontSendNotification);
    zoomControl.slider.setValue (zoom, juce::dontSendNotification);
    rotationControl.slider.setValue (rotationDegrees, juce::dontSendNotification);
    rotationControl.label.setVisible (activeRotationControl);
    rotationControl.slider.setVisible (activeRotationControl);
    tipPitchControl.slider.setValue (tipPitch, juce::dontSendNotification);
    tipPitchControl.label.setVisible (activeTipSelected);
    tipPitchControl.slider.setVisible (activeTipSelected);
    phaseControl.slider.setValue (phaseDegrees, juce::dontSendNotification);
    standsControl.slider.setValue (juce::jlimit (1, 8, platterStands), juce::dontSendNotification);
    diameterControl.slider.setValue (platterDiameter, juce::dontSendNotification);
    blockLevelsControl.slider.setValue (juce::jlimit (1, 16, blockLevels), juce::dontSendNotification);
    blockSizeControl.slider.setValue (blockSize, juce::dontSendNotification);
    tempoControl.slider.setValue (tempoBpm, juce::dontSendNotification);
    currentTempoBpm = tempoBpm;
    transportPlaying = playing;
    tempoValueBox.setText (juce::String (juce::roundToInt (tempoBpm)), juce::dontSendNotification);
    playButton.setToggleState (playing, juce::dontSendNotification);
    pauseButton.setToggleState (! playing, juce::dontSendNotification);
    switchOffTimeControl.slider.setValue (switchOffDuration, juce::dontSendNotification);
    switchActivationControl.combo.setSelectedId (switchActivationMode == PowerSwitchActivationMode::timedOff ? 2 : 1,
                                                 juce::dontSendNotification);
    switchRetriggerControl.combo.setSelectedId (switchRetriggerPolicy == PowerSwitchRetriggerPolicy::turnOnWhileOff ? 2 : 1,
                                                juce::dontSendNotification);
    selectRateDivision (rateDivision);
}

void CityToolbar::configureSlider (LabeledSlider& control,
                                   const juce::String& name,
                                   double min,
                                   double max,
                                   double interval,
                                   const juce::String& suffix)
{
    control.label.setText (name, juce::dontSendNotification);
    control.label.setJustificationType (juce::Justification::centredLeft);
    control.label.setColour (juce::Label::textColourId, quietTextColour());
    control.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));

    control.slider.setRange (min, max, interval);
    control.slider.setSliderStyle (juce::Slider::LinearHorizontal);
    control.slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 54, 20);
    control.slider.setTextValueSuffix (suffix);
    control.slider.setColour (juce::Slider::backgroundColourId, juce::Colour (0x40b6c7be));
    control.slider.setColour (juce::Slider::trackColourId, accentColour());
    control.slider.setColour (juce::Slider::thumbColourId, activeButtonColour());
    control.slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::white.withAlpha (0.68f));
    control.slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0x809db0a7));
    control.slider.setColour (juce::Slider::textBoxTextColourId, textColour());

    addAndMakeVisible (control.label);
    addAndMakeVisible (control.slider);
}

void CityToolbar::selectSideButton (int sides)
{
    for (size_t i = 0; i < sideButtons.size(); ++i)
    {
        auto& button = sideButtons[i];
        const auto isSelected = static_cast<int> (i) + 3 == sides;
        const auto colour = colourForMeter (static_cast<int> (i) + 3);
        button.setToggleState (isSelected, juce::dontSendNotification);
        button.setColour (juce::TextButton::buttonColourId,
                          isSelected ? colour : colour.withAlpha (0.32f));
        button.setColour (juce::TextButton::buttonOnColourId, colour);
        button.setColour (juce::TextButton::textColourOffId,
                          isSelected ? inkColour() : textColour());
        button.setColour (juce::TextButton::textColourOnId, inkColour());
    }
}

void CityToolbar::selectRateDivision (float rateDivision)
{
    if (currentBuildMode == BuildMode::platter)
    {
        rateControl.combo.setSelectedId (closestRateOption (platterRateOptions, rateDivision).id,
                                         juce::dontSendNotification);
        return;
    }

    rateControl.combo.setSelectedId (closestRateOption (rateOptions, rateDivision).id,
                                     juce::dontSendNotification);
}

void CityToolbar::selectBuildMode (BuildMode toolMode, BuildMode controlsMode)
{
    activeBuildMode = controlsMode;
    updateRateOptions (controlsMode);
    radiusControl.label.setText (controlsMode == BuildMode::platter ? "Poly" : "Sq",
                                 juce::dontSendNotification);
    diameterControl.label.setText (controlsMode == BuildMode::plank ? "Len" : "Sq", juce::dontSendNotification);

    for (auto* button : { &selectModeButton, &polygonModeButton, &platterModeButton, &blockModeButton, &plankModeButton, &cableModeButton })
    {
        const auto buttonMode = button == &selectModeButton ? BuildMode::select
                              : button == &polygonModeButton ? BuildMode::polygon
                              : button == &platterModeButton ? BuildMode::platter
                              : button == &blockModeButton ? BuildMode::block
                              : button == &plankModeButton ? BuildMode::plank
                              : BuildMode::cable;
        const auto selected = toolMode == buttonMode;
        const auto colour = colourForMode (buttonMode);
        button->setToggleState (selected, juce::dontSendNotification);
        button->setColour (juce::TextButton::buttonColourId,
                           selected ? colour : colour.withAlpha (0.32f));
        button->setColour (juce::TextButton::buttonOnColourId, colour);
        button->setColour (juce::TextButton::textColourOffId,
                           selected ? inkColour() : textColour());
        button->setColour (juce::TextButton::textColourOnId, inkColour());
    }

    const auto showsFoldControls = controlsMode == BuildMode::polygon;
    const auto showsMotionControls = controlsMode == BuildMode::polygon || controlsMode == BuildMode::platter;
    const auto showsPlatterControls = controlsMode == BuildMode::platter;
    const auto showsBlockControls = controlsMode == BuildMode::block;
    const auto showsPlankControls = controlsMode == BuildMode::plank;
    const auto showsSwitchControls = controlsMode == BuildMode::cable && activeSwitchSelected;

    for (auto& button : sideButtons)
        button.setVisible (showsFoldControls);

    rateControl.label.setVisible (showsMotionControls);
    rateControl.combo.setVisible (showsMotionControls);
    phaseControl.label.setVisible (showsMotionControls);
    phaseControl.slider.setVisible (showsMotionControls);
    radiusControl.label.setVisible (showsFoldControls);
    radiusControl.slider.setVisible (showsFoldControls);
    flapControl.label.setVisible (showsFoldControls);
    flapControl.slider.setVisible (showsFoldControls);

    standsControl.label.setVisible (showsPlatterControls);
    standsControl.slider.setVisible (showsPlatterControls);
    diameterControl.label.setVisible (showsPlatterControls);
    diameterControl.slider.setVisible (showsPlatterControls);

    if (showsPlankControls)
    {
        diameterControl.label.setVisible (true);
        diameterControl.slider.setVisible (true);
    }

    blockLevelsControl.label.setVisible (showsBlockControls);
    blockLevelsControl.slider.setVisible (showsBlockControls);
    blockSizeControl.label.setVisible (showsBlockControls);
    blockSizeControl.slider.setVisible (showsBlockControls);

    switchActivationControl.label.setVisible (showsSwitchControls);
    switchActivationControl.combo.setVisible (showsSwitchControls);
    switchOffTimeControl.label.setVisible (showsSwitchControls);
    switchOffTimeControl.slider.setVisible (showsSwitchControls);
    switchRetriggerControl.label.setVisible (showsSwitchControls);
    switchRetriggerControl.combo.setVisible (showsSwitchControls);

    resized();
    repaint();
}

void CityToolbar::updateRateOptions (BuildMode buildMode)
{
    const auto rateMode = buildMode == BuildMode::platter ? BuildMode::platter : BuildMode::polygon;

    if (currentBuildMode == rateMode && rateControl.combo.getNumItems() > 0)
        return;

    currentBuildMode = rateMode;
    rateControl.combo.clear (juce::dontSendNotification);

    if (rateMode == BuildMode::platter)
    {
        for (const auto& option : platterRateOptions)
            rateControl.combo.addItem (option.label, option.id);
    }
    else
    {
        for (const auto& option : rateOptions)
            rateControl.combo.addItem (option.label, option.id);
    }
}

float CityToolbar::selectedRateDivision() const
{
    const auto selectedId = rateControl.combo.getSelectedId();

    if (currentBuildMode == BuildMode::platter)
    {
        for (const auto& option : platterRateOptions)
            if (option.id == selectedId)
                return option.division;
    }
    else
    {
        for (const auto& option : rateOptions)
            if (option.id == selectedId)
                return option.division;
    }

    return 1.0f;
}

void CityToolbar::timerCallback()
{
    if (! beatLightBounds.isEmpty())
        repaint (beatLightBounds.expanded (8));
}
