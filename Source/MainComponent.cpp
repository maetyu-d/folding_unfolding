#include "MainComponent.h"

MainComponent::MainComponent()
{
    addAndMakeVisible (cityComponent);
    cityComponent.onCitySound = [this] (SonicEventType type, int sidesA, int sidesB, float foldA, float foldB, float pitchOverride)
    {
        triggerCitySound (type, sidesA, sidesB, foldA, foldB, pitchOverride);
    };

    setSize (1280, 800);
    setAudioChannels (0, 2);

    juce::MenuBarModel::setMacMainMenu (this);
}

MainComponent::~MainComponent()
{
    juce::MenuBarModel::setMacMainMenu (nullptr);
    shutdownAudio();
}

void MainComponent::resized()
{
    cityComponent.setBounds (getLocalBounds());
}

juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "View" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int menuIndex, const juce::String& menuName)
{
    juce::ignoreUnused (menuIndex);

    juce::PopupMenu menu;

    if (menuName == "File")
    {
        menu.addItem (newCityMenuItem, "New");
        menu.addSeparator();
        menu.addItem (openCityMenuItem, "Open...");
        menu.addItem (saveCityMenuItem, "Save...");
    }
    else if (menuName == "Edit")
    {
        menu.addItem (undoMenuItem, "Undo", cityComponent.canUndo());
        menu.addItem (redoMenuItem, "Redo", cityComponent.canRedo());
        menu.addSeparator();
        menu.addItem (copyMenuItem, "Copy", cityComponent.canCopySelection());
        menu.addItem (pasteMenuItem, "Paste", cityComponent.canPasteSelection());
    }
    else if (menuName == "View")
    {
        auto tickedText = [] (juce::String text, bool ticked)
        {
            return juce::String (ticked ? juce::String::fromUTF8 (u8"\u2713 ") : juce::String ("  ")) + text;
        };

        const auto viewMode = cityComponent.currentViewMode();
        menu.addItem (isometricViewMenuItem,
                      tickedText ("Isometric View", viewMode == CityViewMode::isometric),
                      true,
                      viewMode == CityViewMode::isometric);
        menu.addItem (topDownViewMenuItem,
                      tickedText ("Top-down View", viewMode == CityViewMode::topDown),
                      true,
                      viewMode == CityViewMode::topDown);
        menu.addSeparator();
        const auto colourWireframeEnabled = cityComponent.isColourWireframeModeEnabled();
        menu.addItem (colourWireframeMenuItem,
                      tickedText ("Colour Wireframe Mode", colourWireframeEnabled),
                      true,
                      colourWireframeEnabled);
        const auto uiVisible = cityComponent.isUiVisible();
        menu.addItem (uiVisibleMenuItem,
                      tickedText ("Show UI", uiVisible),
                      true,
                      uiVisible);
        const auto minimapVisible = cityComponent.isMinimapVisible();
        menu.addItem (minimapVisibleMenuItem,
                      tickedText ("Show Minimap", minimapVisible),
                      true,
                      minimapVisible);
        menu.addSeparator();
        const auto activationRingsVisible = cityComponent.areActivationRingsVisible();
        menu.addItem (activationRingsMenuItem,
                      tickedText ("Polygon Activation Rings", activationRingsVisible),
                      true,
                      activationRingsVisible);
        const auto soundingNotesVisible = cityComponent.areSoundingNotesVisible();
        menu.addItem (soundingNotesMenuItem,
                      tickedText ("Polygon Sounding Notes", soundingNotesVisible),
                      true,
                      soundingNotesVisible);
        const auto triggerTelemetryVisible = cityComponent.isTriggerTelemetryVisible();
        menu.addItem (triggerTelemetryMenuItem,
                      tickedText ("Trigger Telemetry Overlay", triggerTelemetryVisible),
                      true,
                      triggerTelemetryVisible);
    }

    return menu;
}

void MainComponent::menuItemSelected (int menuItemID, int topLevelMenuIndex)
{
    juce::ignoreUnused (topLevelMenuIndex);

    switch (menuItemID)
    {
        case newCityMenuItem:  startNewCity(); break;
        case openCityMenuItem: openCity(); break;
        case saveCityMenuItem: saveCity(); break;
        case undoMenuItem:     cityComponent.undo(); break;
        case redoMenuItem:     cityComponent.redo(); break;
        case copyMenuItem:     cityComponent.copySelectionToClipboard(); break;
        case pasteMenuItem:    cityComponent.pasteSelectionFromClipboard(); break;
        case isometricViewMenuItem: cityComponent.setViewMode (CityViewMode::isometric); break;
        case topDownViewMenuItem:   cityComponent.setViewMode (CityViewMode::topDown); break;
        case colourWireframeMenuItem:
            cityComponent.setColourWireframeModeEnabled (! cityComponent.isColourWireframeModeEnabled());
            break;
        case uiVisibleMenuItem:
            cityComponent.setUiVisible (! cityComponent.isUiVisible());
            break;
        case minimapVisibleMenuItem:
            cityComponent.setMinimapVisible (! cityComponent.isMinimapVisible());
            break;
        case triggerTelemetryMenuItem:
            cityComponent.setTriggerTelemetryVisible (! cityComponent.isTriggerTelemetryVisible());
            break;
        case activationRingsMenuItem:
            cityComponent.setActivationRingsVisible (! cityComponent.areActivationRingsVisible());
            break;
        case soundingNotesMenuItem:
            cityComponent.setSoundingNotesVisible (! cityComponent.areSoundingNotesVisible());
            break;
        default: break;
    }

    menuItemsChanged();
}

void MainComponent::prepareToPlay (int, double sampleRate)
{
    collisionSynth.prepare (sampleRate);
    cityComponent.armSoundTriggers();
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();
    collisionSynth.render (*bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples);
}

void MainComponent::releaseResources()
{
    collisionSynth.reset();
}

void MainComponent::triggerCitySound (SonicEventType type, int sidesA, int sidesB, float foldA, float foldB, float pitchOverride)
{
    collisionSynth.triggerSound (type, sidesA, sidesB, foldA, foldB, pitchOverride);
}

void MainComponent::startNewCity()
{
    cityComponent.recordUndoState();
    cityComponent.startNewCity();
}

void MainComponent::openCity()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Open unfolding",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                       "*.foldcity;*.json",
                                                       true,
                                                       false,
                                                       this);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (file == juce::File())
            return;

        auto parsed = juce::JSON::parse (file);

        if (parsed.isVoid())
        {
            showFileError ("Could not open city", "That file was not readable city data.");
            return;
        }

        cityComponent.recordUndoState();

        if (! cityComponent.loadState (parsed))
            showFileError ("Could not open city", "That file is not an unfolding save.");
    });
}

void MainComponent::saveCity()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Save unfolding",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                           .getChildFile ("unfolding.foldcity"),
                                                       "*.foldcity",
                                                       true,
                                                       false,
                                                       this);

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();
        if (file == juce::File())
            return;

        if (! file.hasFileExtension ("foldcity"))
            file = file.withFileExtension ("foldcity");

        if (! file.replaceWithText (juce::JSON::toString (cityComponent.createState(), true)))
            showFileError ("Could not save city", "The save file could not be written.");
    });
}

void MainComponent::showFileError (const juce::String& title, const juce::String& message)
{
    juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon, title, message);
}
