#pragma once

#include "JuceIncludes.h"
#include "PowerSwitch.h"

#include <array>
#include <functional>

class CityToolbar final : public juce::Component,
                          private juce::Timer
{
public:
    enum class BuildMode
    {
        select,
        polygon,
        platter,
        block,
        plank,
        cable
    };

    CityToolbar();

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setValues (int sides,
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
                    PowerSwitchRetriggerPolicy switchRetriggerPolicy);

    std::function<void (BuildMode buildMode)> onBuildModeChanged;
    std::function<void (int sides)> onSidesChanged;
    std::function<void (float radius)> onRadiusChanged;
    std::function<void (float flapDepth)> onFlapDepthChanged;
    std::function<void (float zoom)> onZoomChanged;
    std::function<void (float rotationDegrees)> onRotationChanged;
    std::function<void (float midiNote)> onTipPitchChanged;
    std::function<void (float rateDivision)> onRateDivisionChanged;
    std::function<void (float phaseDegrees)> onPhaseChanged;
    std::function<void (float tempoBpm)> onTempoChanged;
    std::function<void()> onPlayRequested;
    std::function<void()> onPauseRequested;
    std::function<void()> onStopRequested;
    std::function<void (int stands)> onPlatterStandsChanged;
    std::function<void (float diameter)> onPlatterDiameterChanged;
    std::function<void (int levels)> onBlockLevelsChanged;
    std::function<void (float size)> onBlockSizeChanged;
    std::function<void (PowerSwitchActivationMode mode)> onSwitchActivationModeChanged;
    std::function<void (float seconds)> onSwitchOffDurationChanged;
    std::function<void (PowerSwitchRetriggerPolicy policy)> onSwitchRetriggerPolicyChanged;
    std::function<void()> onDeleteRequested;
    std::function<void()> onClearRequested;

private:
    struct LabeledSlider
    {
        juce::Label label;
        juce::Slider slider;
    };

    struct LabeledCombo
    {
        juce::Label label;
        juce::ComboBox combo;
    };

    void configureSlider (LabeledSlider& control,
                          const juce::String& name,
                          double min,
                          double max,
                          double interval,
                          const juce::String& suffix);

    void selectSideButton (int sides);
    void selectRateDivision (float rateDivision);
    void selectBuildMode (BuildMode toolMode, BuildMode controlsMode);
    void updateRateOptions (BuildMode buildMode);
    float selectedRateDivision() const;
    void timerCallback() override;

    juce::TextButton selectModeButton { "Select" };
    juce::TextButton polygonModeButton { "Polygon" };
    juce::TextButton platterModeButton { "Platter" };
    juce::TextButton blockModeButton { "Block" };
    juce::TextButton plankModeButton { "Plank" };
    juce::TextButton cableModeButton { "Cable" };
    std::array<juce::TextButton, 6> sideButtons;
    LabeledSlider standsControl;
    LabeledSlider diameterControl;
    LabeledSlider blockLevelsControl;
    LabeledSlider blockSizeControl;
    LabeledSlider radiusControl;
    LabeledSlider flapControl;
    LabeledSlider zoomControl;
    LabeledSlider rotationControl;
    LabeledSlider tipPitchControl;
    LabeledSlider phaseControl;
    LabeledSlider tempoControl;
    LabeledSlider switchOffTimeControl;
    LabeledCombo rateControl;
    LabeledCombo switchActivationControl;
    LabeledCombo switchRetriggerControl;
    juce::TextButton deleteButton { "Delete" };
    juce::TextButton clearButton { "Clear" };
    juce::TextButton playButton { "Play" };
    juce::TextButton pauseButton { "Pause" };
    juce::TextButton stopButton { "Stop" };
    juce::Label tempoValueBox;
    juce::Rectangle<int> beatLightBounds;
    float currentTempoBpm = 120.0f;
    bool transportPlaying = false;

    bool suppressCallbacks = false;
    bool activeSwitchSelected = false;
    bool activeRotationControl = false;
    bool activeTipSelected = false;
    BuildMode activeBuildMode = BuildMode::polygon;
    BuildMode currentBuildMode = BuildMode::polygon;
};
