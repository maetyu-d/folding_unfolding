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
    return { "File", "Edit" };
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
        default: break;
    }
}

void MainComponent::prepareToPlay (int, double sampleRate)
{
    collisionSynth.prepare (sampleRate);
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
    fileChooser = std::make_unique<juce::FileChooser> ("Open Folding Polygon City",
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
            showFileError ("Could not open city", "That file is not a Folding Polygon City save.");
    });
}

void MainComponent::saveCity()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Save Folding Polygon City",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                           .getChildFile ("Folding Polygon City.foldcity"),
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
