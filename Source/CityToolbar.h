#pragma once

#include "JuceIncludes.h"
#include "PowerSwitch.h"
#include "SonicEvent.h"

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
    ~CityToolbar() override;

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
                    bool polygonSelected,
                    const std::array<float, 8>& tipPitches,
                    const std::array<bool, 8>& tipRandom,
                    const std::array<float, 8>& tipRandomLow,
                    const std::array<float, 8>& tipRandomHigh,
                    const std::array<float, 8>& tipProbabilities,
                    const std::array<TipSoundLanguage, 8>& tipSoundLanguages,
                    const std::array<juce::String, 8>& tipSoundPrograms,
                    int selectedTipIndex,
                    bool tipProgramMode,
                    float tempoBpm,
                    bool playing,
                    bool switchSelected,
                    PowerSwitchActivationMode switchActivationMode,
                    float switchOffDuration,
                    PowerSwitchRetriggerPolicy switchRetriggerPolicy,
                    bool colourWireframeMode);

    std::function<void (BuildMode buildMode)> onBuildModeChanged;
    std::function<void (int sides)> onSidesChanged;
    std::function<void (float radius)> onRadiusChanged;
    std::function<void (float flapDepth)> onFlapDepthChanged;
    std::function<void (float zoom)> onZoomChanged;
    std::function<void (float rotationDegrees)> onRotationChanged;
    std::function<void (float midiNote)> onTipPitchChanged;
    std::function<void (int tipIndex, float midiNote)> onTipPitchValueChanged;
    std::function<void (int tipIndex, bool random)> onTipPitchRandomChanged;
    std::function<void (int tipIndex, float low, float high)> onTipPitchRangeChanged;
    std::function<void (int tipIndex, float probability)> onTipProbabilityChanged;
    std::function<void (int tipIndex, TipSoundLanguage language)> onTipSoundLanguageChanged;
    std::function<void (int tipIndex)> onTipCodeTipSelected;
    std::function<juce::String (int tipIndex, const juce::String& program)> onTipSoundProgramChanged;
    std::function<juce::String (int tipIndex, const juce::String& program)> onTipSoundProgramAudition;
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
    class CodeEditorWindow;

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

    struct TipPitchRow
    {
        juce::Label label;
        juce::TextEditor pitch;
        juce::TextButton random;
        juce::TextEditor low;
        juce::TextEditor high;
        juce::TextEditor probability;
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
    void commitTipPitchValue (int tipIndex);
    void commitTipPitchRange (int tipIndex);
    void commitTipProbability (int tipIndex);
    void commitTipSoundProgram();
    void auditionTipSoundProgram();
    void resetTipSoundProgram();
    void openLargeTipProgramEditor();
    void applyLargeTipProgramEditorText (const juce::String& text);
    void auditionLargeTipProgramEditorText (const juce::String& text);
    void resetLargeTipProgramEditorText();
    juce::String defaultProgramForActiveTip() const;
    void setTipProgramStatus (const juce::String& text, bool warning);
    void updateTipProgramMeta();
    void applyTheme();
    static float pitchValueFromText (const juce::String& text);
    static juce::String pitchTextForValue (float value);
    static bool textRequestsRandomPitch (const juce::String& text);
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
    LabeledCombo tipSoundLanguageControl;
    juce::Label pitchListTitle;
    std::array<TipPitchRow, 8> tipPitchRows;
    std::array<juce::TextButton, 8> tipCodeMapButtons;
    juce::Label tipProgramTitle;
    juce::TextEditor tipProgramEditor;
    juce::TextButton tipProgramApplyButton { "Apply" };
    juce::TextButton tipProgramAuditionButton { "Audition" };
    juce::TextButton tipProgramResetButton { "Reset" };
    juce::TextButton tipProgramOpenButton { "Open" };
    juce::Label tipProgramStatusLabel;
    juce::Label tipProgramMetaLabel;
    std::unique_ptr<CodeEditorWindow> codeEditorWindow;
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
    bool activePolygonSelected = false;
    bool activeTipProgramMode = false;
    bool tipProgramDirty = false;
    bool clearConfirmationArmed = false;
    bool wireframeTheme = false;
    double clearConfirmationDeadlineMs = 0.0;
    int activeSides = 6;
    int activeTipIndex = 0;
    int loadedProgramTipIndex = -1;
    TipSoundLanguage activeTipSoundLanguage = TipSoundLanguage::superCollider;
    juce::String loadedTipProgram;
    juce::String lastAppliedTipProgram;
    BuildMode activeBuildMode = BuildMode::polygon;
    BuildMode currentBuildMode = BuildMode::polygon;
};
