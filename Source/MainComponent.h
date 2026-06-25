#pragma once

#include "CityComponent.h"
#include "CollisionSynth.h"
#include "JuceIncludes.h"


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

    void triggerCitySound (SonicEventType type, int sidesA, int sidesB, float foldA, float foldB, float pitchOverride = -1.0f);

private:
    enum MenuItemIds
    {
        newCityMenuItem = 1,
        openCityMenuItem,
        saveCityMenuItem,
        undoMenuItem,
        redoMenuItem,
        copyMenuItem,
        pasteMenuItem
    };

    void startNewCity();
    void openCity();
    void saveCity();
    void showFileError (const juce::String& title, const juce::String& message);

    CityComponent cityComponent;
    CollisionSynth collisionSynth;
    std::unique_ptr<juce::FileChooser> fileChooser;
};
