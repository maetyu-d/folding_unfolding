#pragma once

#include "CityModel.h"
#include "CityOpenGLRenderer.h"
#include "CityToolbar.h"
#include "JuceIncludes.h"
#include "SonicEvent.h"

#include <functional>
#include <atomic>
#include <map>
#include <set>
#include <vector>

class CityComponent final : public juce::Component,
                            private juce::Timer
{
public:
    CityComponent();
    ~CityComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& event) override;
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseEnter (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed (const juce::KeyPress& key) override;

    juce::var createState() const;
    bool loadState (const juce::var& state);
    void startNewCity();
    void recordUndoState();
    bool undo();
    bool redo();
    bool canUndo() const;
    bool canRedo() const;
    bool canCopySelection() const;
    bool canPasteSelection() const;
    bool copySelectionToClipboard() const;
    bool pasteSelectionFromClipboard();
    CityViewMode currentViewMode() const;
    void setViewMode (CityViewMode mode);
    bool isTriggerTelemetryVisible() const noexcept;
    void setTriggerTelemetryVisible (bool shouldBeVisible);
    bool areActivationRingsVisible() const noexcept;
    void setActivationRingsVisible (bool shouldBeVisible);
    bool areSoundingNotesVisible() const noexcept;
    void setSoundingNotesVisible (bool shouldBeVisible);
    bool isColourWireframeModeEnabled() const noexcept;
    void setColourWireframeModeEnabled (bool shouldBeEnabled);
    bool isUiVisible() const noexcept;
    void setUiVisible (bool shouldBeVisible);
    void armSoundTriggers() noexcept;

    std::function<void (SonicEventType type, int sidesA, int sidesB, float foldA, float foldB, float pitchOverride)> onCitySound;

private:
    enum class DragMode
    {
        none,
        object,
        pan,
        resizeHandle,
        phaseHandle,
        cableWire
    };

    void timerCallback() override;

    double currentTimeSeconds() const noexcept;
    void updateCollisions (double timeSeconds);
    void updatePowerSwitches (double timeSeconds);
    void updateAttachedStructures (double timeSeconds);
    bool findAvailableMount (Vec2 worldPosition, double timeSeconds, float radius, int& platterId, int& standIndex) const;
    bool findAvailablePlankSocket (Vec2 worldPosition, float radius, int& plankId) const;
    bool findTipAt (juce::Point<float> screenPoint, int& moduleId, int& tipIndex) const;
    void announcePowerChange (PowerSwitch& powerSwitch, int sourceSides, float sourceFold, double timeSeconds, float pitchOverride = -1.0f);
    void flashConnectedTargets (int switchId, float amount);
    void addTipTriggerCue (const FoldingModule& module, Vec3 tip, const PowerSwitch& powerSwitch, bool muted = false, bool contactOnly = false);
    void addTipContactCue (const FoldingModule& a, Vec3 tipA, const FoldingModule& b, Vec3 tipB, bool muted, bool contactOnly);
    void paintSoundingNotes (juce::Graphics& g, const IsoProjector& view);
    void paintActivationRings (juce::Graphics& g, const IsoProjector& view);
    void paintTriggerTelemetry (juce::Graphics& g, const IsoProjector& view);
    void triggerCitySound (SonicEventType type, int sidesA, int sidesB, float foldA, float foldB, float pitchOverride = -1.0f);
    void configureToolbar();
    void syncToolbar();
    CityRenderState createRenderState() const;

    void setSelectedSides (int sides);
    void adjustSelectedOrDefaultRadius (float delta);
    void adjustSelectedOrDefaultFlapDepth (float delta);
    void setSelectedOrDefaultRadius (float radius);
    void setSelectedOrDefaultFlapDepth (float flapDepth);
    void setSelectedOrDefaultRateDivision (float rateDivision);
    void setSelectedOrDefaultPhaseDegrees (float phaseDegrees);
    void setSelectedRotationDegrees (float rotationDegrees);
    void setSelectedTipPitch (float midiNote);
    void setSelectedTipPitch (int tipIndex, float midiNote);
    void setSelectedTipRandom (int tipIndex, bool random);
    void setSelectedTipRandomRange (int tipIndex, float low, float high);
    void setSelectedTipProbability (int tipIndex, float probability);
    void setBuildMode (CityToolbar::BuildMode mode);
    void setSelectedOrDefaultPlatterStands (int stands);
    void setSelectedOrDefaultPlatterDiameter (float diameter);
    void setSelectedOrDefaultBlockLevels (int levels);
    void setSelectedOrDefaultBlockSize (float size);
    void setSelectedOrDefaultPlankLength (float length);
    void setSelectedSwitchActivationMode (PowerSwitchActivationMode mode);
    void setSelectedSwitchOffDuration (float seconds);
    void setSelectedSwitchRetriggerPolicy (PowerSwitchRetriggerPolicy policy);
    void setGlobalTempo (float bpm);
    void playTransport();
    void pauseTransport();
    void stopTransport();
    void setZoom (float zoom);
    void toggleViewMode();
    void handleCableClick (CityHit hit, bool createPowerSource);
    void completeCableDrag (CityHit hit);
    bool deleteSelectedObject();
    void clearModules();
    void zoomAt (juce::Point<float> screenPoint, float factor);
    Vec2 worldForPointer (juce::Point<float> screenPoint) const;
    Vec2 snappedWorldForPointer (juce::Point<float> screenPoint) const;
    Vec2 snapToBuildGrid (Vec2 world) const noexcept;
    Vec2 snapToBuildGrid (Vec2 world, float footprint) const noexcept;
    float currentBuildFootprint() const noexcept;
    float snappedModuleRadiusForPlacement() const noexcept;
    float snappedPlatterDiameterForPlacement() const noexcept;
    float snappedBlockSizeForPlacement() const noexcept;
    float selectedElevation() const;
    bool hitSelectedResizeHandle (juce::Point<float> screenPoint) const;
    bool hitSelectedPhaseHandle (juce::Point<float> screenPoint) const;
    void dragSelectedResizeHandle (juce::Point<float> screenPoint);
    void dragSelectedPhaseHandle (juce::Point<float> screenPoint);

    float currentRadius() const;
    float currentFlapDepth() const;
    float currentRateDivision() const;
    float currentPhaseDegrees() const;
    int currentPlatterStands() const;
    float currentPlatterDiameter() const;
    int currentBlockLevels() const;
    float currentBlockSize() const;
    float currentPlankLength() const;
    int currentSides() const;

    static std::pair<int, int> sortedPair (int a, int b) noexcept;

    CityModel city;
    IsoProjector projector;
    juce::OpenGLContext openGLContext;
    CityOpenGLRenderer cityRenderer;
    CityToolbar toolbar;
    mutable juce::CriticalSection modelLock;

    DragMode dragMode = DragMode::none;
    juce::Point<float> lastMouse;
    Vec2 lastDragWorld;
    Vec2 hoverWorld;
    Vec2 hoverPointerWorld;
    bool mouseInCanvas = false;

    int selectedId = -1;
    CityObjectKind selectedKind = CityObjectKind::none;
    int selectedTipIndex = -1;
    int hoverTipModuleId = -1;
    int hoverTipIndex = -1;
    int cableSourceSwitchId = -1;
    int cableSourcePowerSourceId = -1;
    CityToolbar::BuildMode buildMode = CityToolbar::BuildMode::select;
    int defaultSides = 6;
    float defaultRadius = 64.0f;
    float defaultFlapDepth = 32.0f;
    float defaultRateDivision = 1.0f;
    float defaultPhase = 0.0f;
    int defaultPlatterStands = 4;
    float defaultPlatterDiameter = 288.0f;
    float defaultPlatterRateDivision = 1.0f;
    float defaultPlatterPhase = 0.0f;
    int defaultBlockLevels = 1;
    float defaultBlockSize = 96.0f;
    float defaultPlankLength = 288.0f;
    float defaultPlankAngle = 0.0f;
    float globalTempoBpm = FoldingModule::defaultGlobalTempoBpm;

    double startTimeSeconds = 0.0;
    double lastFrameTimeSeconds = 0.0;
    double transportTimeSeconds = 0.0;
    bool transportPlaying = true;
    bool triggerTelemetryVisible = false;
    bool activationRingsVisible = false;
    bool soundingNotesVisible = true;
    bool colourWireframeMode = false;
    bool uiVisible = true;
    double lastCollisionSoundTimeSeconds = -1.0;
    std::atomic<bool> soundTriggersArmed { false };
    std::atomic<bool> soundTriggerResetRequested { true };
    int renderWidth = 1;
    int renderHeight = 1;
    std::set<std::pair<int, int>> activeCollisions;
    std::set<std::pair<int, int>> activePowerSwitchTouches;
    std::set<std::pair<int, int>> activeTipContacts;
    std::map<std::pair<int, int>, double> tipContactReleaseTimes;
    std::vector<CityRenderState::TipTriggerCue> tipTriggerCues;
    std::vector<juce::String> undoStack;
    std::vector<juce::String> redoStack;
};
