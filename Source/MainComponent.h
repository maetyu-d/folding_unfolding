#pragma once

#include "CityComponent.h"
#include "CollisionSynth.h"
#include "JuceIncludes.h"
#include "Mode2EmbeddedAudioEngine.h"


class MainComponent final : public juce::AudioAppComponent,
                            public juce::MenuBarModel
{
public:
    MainComponent();
    ~MainComponent() override;

    void resized() override;
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int menuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void triggerCitySound (SonicEventType type,
                           int sidesA,
                           int sidesB,
                           float foldA,
                           float foldB,
                           float pitchOverride = -1.0f,
                           TipSoundLanguage language = TipSoundLanguage::superCollider,
                           const juce::String& program = {},
                           int tipIndex = -1);

private:
    enum class AudioMode
    {
        mode1Internal,
        mode2EmbeddedCode
    };

    enum MenuItemIds
    {
        newCityMenuItem = 1,
        openCityMenuItem,
        saveCityMenuItem,
        undoMenuItem,
        redoMenuItem,
        copyMenuItem,
        pasteMenuItem,
        isometricViewMenuItem,
        topDownViewMenuItem,
        colourWireframeMenuItem,
        uiVisibleMenuItem,
        minimapVisibleMenuItem,
        triggerTelemetryMenuItem,
        activationRingsMenuItem,
        soundingNotesMenuItem,
        audioMode1MenuItem,
        audioMode2MenuItem
    };

    void startNewCity();
    void openCity();
    void saveCity();
    void showFileError (const juce::String& title, const juce::String& message);
    void setAudioMode (AudioMode mode);
    juce::String previewTipProgram (TipSoundLanguage language,
                                    const juce::String& program,
                                    int sides,
                                    int tipIndex,
                                    float fold,
                                    float pitch,
                                    bool audition);
    static juce::String tickedText (juce::String text, bool ticked);

    CityComponent cityComponent;
    CollisionSynth collisionSynth;
    Mode2EmbeddedAudioEngine mode2EmbeddedAudioEngine;
    AudioMode audioMode = AudioMode::mode1Internal;
    std::unique_ptr<juce::FileChooser> fileChooser;
};
