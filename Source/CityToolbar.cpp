#include "CityToolbar.h"

#include "FoldingModule.h"

#include <cmath>

namespace
{
constexpr auto gap = 10;
constexpr auto panelRadius = 18.0f;
bool wireframePalette = false;

juce::Colour panelColour() { return wireframePalette ? juce::Colour (0xc8060d14) : juce::Colour (0xeef8fbf1); }
juce::Colour panelLineColour() { return wireframePalette ? juce::Colour (0xbb00ffc8) : juce::Colour (0xb8ffffff); }
juce::Colour textColour() { return wireframePalette ? juce::Colour (0xffeaffff) : juce::Colour (0xff263832); }
juce::Colour quietTextColour() { return wireframePalette ? juce::Colour (0xff8cefe5) : juce::Colour (0xff65766f); }
juce::Colour accentColour() { return wireframePalette ? juce::Colour (0xff23f7ff) : juce::Colour (0xff32c9b1); }
juce::Colour activeButtonColour() { return wireframePalette ? juce::Colour (0xff39ff88) : juce::Colour (0xffffd766); }
juce::Colour inactiveButtonColour() { return wireframePalette ? juce::Colour (0x3315f3ff) : juce::Colour (0xbaffffff); }
juce::Colour inkColour() { return wireframePalette ? juce::Colour (0xff02070b) : juce::Colour (0xff18241f); }

void applyPitchEditorTheme (juce::TextEditor& editor);
void applyProgramEditorTheme (juce::TextEditor& editor);
void styleProgramButton (juce::TextButton& button)
{
    button.setColour (juce::TextButton::buttonColourId, inactiveButtonColour());
    button.setColour (juce::TextButton::buttonOnColourId,
                      wireframePalette ? activeButtonColour().withAlpha (0.78f) : activeButtonColour());
    button.setColour (juce::TextButton::textColourOffId, textColour());
    button.setColour (juce::TextButton::textColourOnId,
                      wireframePalette ? textColour() : inkColour());
}

void setCodeStatus (juce::Label& label, const juce::String& text, bool warning)
{
    auto status = text.isNotEmpty() ? text : juce::String ("no status");
    status = status.replaceCharacters ("\n\r", "  ").trim();

    if (status.length() > 96)
        status = status.substring (0, 93) + "...";

    label.setText (status, juce::dontSendNotification);
    label.setColour (juce::Label::textColourId,
                     warning ? juce::Colour (0xffff8fb1) : quietTextColour());
}

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

void configurePitchEditor (juce::TextEditor& editor)
{
    editor.setJustification (juce::Justification::centred);
    editor.setSelectAllWhenFocused (true);
    editor.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    applyPitchEditorTheme (editor);
}

void applyPitchEditorTheme (juce::TextEditor& editor)
{
    editor.setColour (juce::TextEditor::backgroundColourId,
                      wireframePalette ? juce::Colour (0xbb03080e) : juce::Colours::white.withAlpha (0.70f));
    editor.setColour (juce::TextEditor::outlineColourId,
                      wireframePalette ? juce::Colour (0x8823f7ff) : juce::Colour (0x809db0a7));
    editor.setColour (juce::TextEditor::focusedOutlineColourId, accentColour());
    editor.setColour (juce::TextEditor::textColourId, textColour());
}

void configureProgramEditor (juce::TextEditor& editor)
{
    editor.setMultiLine (true);
    editor.setReturnKeyStartsNewLine (true);
    editor.setTabKeyUsedAsCharacter (true);
    editor.setScrollbarsShown (true);
    editor.setFont (juce::FontOptions (12.0f));
    editor.setIndents (8, 6);
    applyProgramEditorTheme (editor);
}

void applyProgramEditorTheme (juce::TextEditor& editor)
{
    editor.setColour (juce::TextEditor::backgroundColourId,
                      wireframePalette ? juce::Colour (0xee02070b) : juce::Colours::white.withAlpha (0.72f));
    editor.setColour (juce::TextEditor::outlineColourId,
                      wireframePalette ? juce::Colour (0xaa23f7ff) : juce::Colour (0x809db0a7));
    editor.setColour (juce::TextEditor::focusedOutlineColourId, accentColour());
    editor.setColour (juce::TextEditor::textColourId, textColour());
    editor.setColour (juce::TextEditor::highlightColourId, accentColour().withAlpha (0.32f));
    editor.setColour (juce::TextEditor::highlightedTextColourId, textColour());
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
    { 1,  "4 bars",         0.25f },
    { 2,  "3 bars",         1.0f / 3.0f },
    { 3,  "2 bars",         0.5f },
    { 4,  "1 bar",          1.0f },
    { 5,  "1/2 bar",        2.0f },
    { 6,  "1/2 bar triplet", 3.0f },
    { 7,  "1/2.",           4.0f / 3.0f },
    { 8,  "1/4.",           8.0f / 3.0f },
    { 9,  "1/4",            4.0f },
    { 10, "1/4T",           6.0f },
    { 11, "1/8.",           16.0f / 3.0f },
    { 12, "1/8",            8.0f },
    { 13, "1/8T",           12.0f },
    { 14, "1/16.",          32.0f / 3.0f },
    { 15, "1/16",           16.0f },
    { 16, "1/16T",          24.0f },
    { 17, "1/32",           32.0f },
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
    g.setColour (juce::Colours::black.withAlpha (wireframePalette ? 0.34f : 0.08f));
    g.fillRoundedRectangle (bounds.translated (0.0f, wireframePalette ? 2.0f : 5.0f), panelRadius);

    g.setColour (panelColour());
    g.fillRoundedRectangle (bounds, panelRadius);

    g.setColour (panelLineColour());
    g.drawRoundedRectangle (bounds, panelRadius, wireframePalette ? 1.4f : 1.0f);

    if (wireframePalette)
    {
        const auto inset = bounds.reduced (7.0f);
        g.setColour (accentColour().withAlpha (0.16f));
        g.drawRoundedRectangle (inset, panelRadius - 7.0f, 0.8f);
    }
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

class CityToolbar::CodeEditorWindow final : public juce::DocumentWindow
{
public:
    class Content final : public juce::Component,
                          private juce::CodeDocument::Listener
    {
    public:
        Content (CityToolbar& ownerIn, juce::String initialText)
            : owner (ownerIn),
              editor (document, &tokeniser)
        {
            document.replaceAllContent (initialText);
            document.addListener (this);
            configureCodeEditor();
            addAndMakeVisible (editor);

            for (auto* button : { &applyButton, &auditionButton, &defaultButton, &closeButton })
            {
                styleProgramButton (*button);
                addAndMakeVisible (*button);
            }

            applyButton.onClick = [this] { owner.applyLargeTipProgramEditorText (document.getAllContent()); };
            auditionButton.onClick = [this] { owner.auditionLargeTipProgramEditorText (document.getAllContent()); };
            defaultButton.onClick = [this]
            {
                setTextFromToolbar (owner.defaultProgramForActiveTip());
                owner.resetLargeTipProgramEditorText();
            };
            closeButton.onClick = [this]
            {
                if (auto* window = findParentComponentOfClass<CodeEditorWindow>())
                    window->closeButtonPressed();
            };

            statusLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            statusLabel.setJustificationType (juce::Justification::centredLeft);
            statusLabel.setColour (juce::Label::textColourId, quietTextColour());
            addAndMakeVisible (statusLabel);

            metaLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
            metaLabel.setJustificationType (juce::Justification::centredRight);
            metaLabel.setColour (juce::Label::textColourId, quietTextColour());
            addAndMakeVisible (metaLabel);

            setStatus ("ready", false);
            updateMeta();
        }

        ~Content() override
        {
            document.removeListener (this);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (wireframePalette ? juce::Colour (0xff02070b) : juce::Colour (0xfff8fbf1));
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced (18);
            auto top = bounds.removeFromTop (38);

            closeButton.setBounds (top.removeFromRight (82));
            top.removeFromRight (8);
            defaultButton.setBounds (top.removeFromRight (94));
            top.removeFromRight (8);
            auditionButton.setBounds (top.removeFromRight (104));
            top.removeFromRight (8);
            applyButton.setBounds (top.removeFromRight (88));

            bounds.removeFromTop (10);
            auto status = bounds.removeFromTop (22);
            metaLabel.setBounds (status.removeFromRight (210));
            statusLabel.setBounds (status);
            bounds.removeFromTop (10);
            editor.setBounds (bounds);
        }

        void setTextFromToolbar (const juce::String& text)
        {
            const juce::ScopedValueSetter<bool> setter (updatingDocument, true);
            document.replaceAllContent (text);
            document.clearUndoHistory();
            document.setSavePoint();
            updateMeta();
        }

        void setStatus (const juce::String& status, bool warning)
        {
            setCodeStatus (statusLabel, status, warning);
        }

    private:
        void configureCodeEditor()
        {
            juce::Font font (juce::FontOptions (15.0f));
            font.setTypefaceName (juce::Font::getDefaultMonospacedFontName());
            editor.setFont (font);
            editor.setTabSize (4, true);
            editor.setLineNumbersShown (true);
            editor.setScrollbarThickness (12);
            editor.setColour (juce::CodeEditorComponent::backgroundColourId,
                              wireframePalette ? juce::Colour (0xff02070b) : juce::Colour (0xfffbfcf7));
            editor.setColour (juce::CodeEditorComponent::highlightColourId, accentColour().withAlpha (0.30f));
            editor.setColour (juce::CodeEditorComponent::defaultTextColourId, textColour());
            editor.setColour (juce::CodeEditorComponent::lineNumberBackgroundId,
                              wireframePalette ? juce::Colour (0xff050f16) : juce::Colour (0xffedf3ec));
            editor.setColour (juce::CodeEditorComponent::lineNumberTextId, quietTextColour());

            juce::CodeEditorComponent::ColourScheme scheme;
            scheme.set ("Error", wireframePalette ? juce::Colour (0xffff5f8f) : juce::Colour (0xffc51b5a));
            scheme.set ("Comment", wireframePalette ? juce::Colour (0xff38d67a) : juce::Colour (0xff4c8a62));
            scheme.set ("Keyword", wireframePalette ? juce::Colour (0xffffd766) : juce::Colour (0xff8c55f6));
            scheme.set ("Operator", wireframePalette ? juce::Colour (0xff23f7ff) : juce::Colour (0xff149e90));
            scheme.set ("Identifier", textColour());
            scheme.set ("Integer", wireframePalette ? juce::Colour (0xffff8fb1) : juce::Colour (0xffd65c5c));
            scheme.set ("Float", wireframePalette ? juce::Colour (0xffff9f6e) : juce::Colour (0xffc8752a));
            scheme.set ("String", wireframePalette ? juce::Colour (0xff75db8a) : juce::Colour (0xff2f8f5b));
            scheme.set ("Bracket", wireframePalette ? juce::Colour (0xffc79bff) : juce::Colour (0xff5b63c7));
            scheme.set ("Punctuation", wireframePalette ? juce::Colour (0xff64dfdf) : juce::Colour (0xff298d95));
            scheme.set ("Preprocessor Text", wireframePalette ? juce::Colour (0xffffcf5f) : juce::Colour (0xffa4642e));
            editor.setColourScheme (scheme);
        }

        void codeDocumentTextInserted (const juce::String&, int) override
        {
            if (! updatingDocument)
                updateMeta();
        }

        void codeDocumentTextDeleted (int, int) override
        {
            if (! updatingDocument)
                updateMeta();
        }

        void updateMeta()
        {
            const auto lines = document.getNumLines();
            metaLabel.setText (juce::String (lines)
                                   + juce::String (lines == 1 ? " line  /  " : " lines  /  ")
                                   + juce::String (document.getNumCharacters())
                                   + " chars",
                               juce::dontSendNotification);
        }

        CityToolbar& owner;
        juce::CodeDocument document;
        juce::CPlusPlusCodeTokeniser tokeniser;
        juce::CodeEditorComponent editor;
        juce::TextButton applyButton { "Apply" };
        juce::TextButton auditionButton { "Audition" };
        juce::TextButton defaultButton { "Default" };
        juce::TextButton closeButton { "Close" };
        juce::Label statusLabel;
        juce::Label metaLabel;
        bool updatingDocument = false;
    };

    CodeEditorWindow (CityToolbar& ownerIn, juce::String title, juce::String initialText, bool wireframe)
        : DocumentWindow (std::move (title),
                          wireframe ? juce::Colour (0xff02070b) : juce::Colour (0xfff8fbf1),
                          DocumentWindow::closeButton),
          owner (ownerIn)
    {
        wireframePalette = wireframe;
        setUsingNativeTitleBar (true);
        setResizable (true, true);
        content = new Content (owner, std::move (initialText));
        setContentOwned (content, false);
        centreWithSize (760, 560);
        setVisible (true);
    }

    ~CodeEditorWindow() override
    {
        clearContentComponent();
    }

    void closeButtonPressed() override
    {
        owner.codeEditorWindow.reset();
    }

    void syncFromToolbar (const juce::String& title, const juce::String& text, bool wireframe)
    {
        wireframePalette = wireframe;
        setName (title);

        if (content != nullptr)
            content->setTextFromToolbar (text);
    }

    void setStatus (const juce::String& status, bool warning)
    {
        if (content != nullptr)
            content->setStatus (status, warning);
    }

private:
    CityToolbar& owner;
    Content* content = nullptr;
};

CityToolbar::~CityToolbar() = default;

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
        if (trimmed.equalsIgnoreCase ("r"))
            return -1.0;

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

    tipSoundLanguageControl.label.setText ("Code", juce::dontSendNotification);
    tipSoundLanguageControl.label.setJustificationType (juce::Justification::centredLeft);
    tipSoundLanguageControl.label.setColour (juce::Label::textColourId, quietTextColour());
    tipSoundLanguageControl.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    tipSoundLanguageControl.combo.addItem ("SC", 1);
   #if UNFOLDING_HAS_WELD_CHUCK
    tipSoundLanguageControl.combo.addItem ("ChucK", 2);
   #endif
    tipSoundLanguageControl.combo.setColour (juce::ComboBox::backgroundColourId, inactiveButtonColour());
    tipSoundLanguageControl.combo.setColour (juce::ComboBox::outlineColourId, panelLineColour());
    tipSoundLanguageControl.combo.setColour (juce::ComboBox::textColourId, textColour());
    tipSoundLanguageControl.combo.setColour (juce::ComboBox::arrowColourId, accentColour());
    tipSoundLanguageControl.combo.onChange = [this]
    {
        if (! suppressCallbacks && onTipSoundLanguageChanged)
            onTipSoundLanguageChanged (activeTipIndex,
                                      #if UNFOLDING_HAS_WELD_CHUCK
                                       tipSoundLanguageControl.combo.getSelectedId() == 2
                                           ? TipSoundLanguage::chuck
                                           : TipSoundLanguage::superCollider
                                      #else
                                       TipSoundLanguage::superCollider
                                      #endif
            );
    };
    addAndMakeVisible (tipSoundLanguageControl.label);
    addAndMakeVisible (tipSoundLanguageControl.combo);

    tipProgramTitle.setText ("Tip program", juce::dontSendNotification);
    tipProgramTitle.setJustificationType (juce::Justification::centredLeft);
    tipProgramTitle.setColour (juce::Label::textColourId, quietTextColour());
    tipProgramTitle.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (tipProgramTitle);

    configureProgramEditor (tipProgramEditor);
    tipProgramEditor.onTextChange = [this]
    {
        if (! suppressCallbacks)
        {
            tipProgramDirty = tipProgramEditor.getText() != lastAppliedTipProgram;
            updateTipProgramMeta();
        }
    };
    addAndMakeVisible (tipProgramEditor);

    tipProgramApplyButton.onClick = [this] { commitTipSoundProgram(); };
    tipProgramApplyButton.setColour (juce::TextButton::buttonColourId, inactiveButtonColour());
    tipProgramApplyButton.setColour (juce::TextButton::buttonOnColourId, activeButtonColour());
    tipProgramApplyButton.setColour (juce::TextButton::textColourOffId, textColour());
    tipProgramApplyButton.setColour (juce::TextButton::textColourOnId, inkColour());
    addAndMakeVisible (tipProgramApplyButton);

    tipProgramAuditionButton.onClick = [this] { auditionTipSoundProgram(); };
    tipProgramAuditionButton.setColour (juce::TextButton::buttonColourId, inactiveButtonColour());
    tipProgramAuditionButton.setColour (juce::TextButton::buttonOnColourId, activeButtonColour());
    tipProgramAuditionButton.setColour (juce::TextButton::textColourOffId, textColour());
    tipProgramAuditionButton.setColour (juce::TextButton::textColourOnId, inkColour());
    addAndMakeVisible (tipProgramAuditionButton);

    tipProgramResetButton.onClick = [this] { resetTipSoundProgram(); };
    tipProgramResetButton.setButtonText ("Default");
    tipProgramResetButton.setColour (juce::TextButton::buttonColourId, inactiveButtonColour());
    tipProgramResetButton.setColour (juce::TextButton::buttonOnColourId, activeButtonColour());
    tipProgramResetButton.setColour (juce::TextButton::textColourOffId, textColour());
    tipProgramResetButton.setColour (juce::TextButton::textColourOnId, inkColour());
    addAndMakeVisible (tipProgramResetButton);

    tipProgramOpenButton.onClick = [this] { openLargeTipProgramEditor(); };
    tipProgramOpenButton.setColour (juce::TextButton::buttonColourId, inactiveButtonColour());
    tipProgramOpenButton.setColour (juce::TextButton::buttonOnColourId, activeButtonColour());
    tipProgramOpenButton.setColour (juce::TextButton::textColourOffId, textColour());
    tipProgramOpenButton.setColour (juce::TextButton::textColourOnId, inkColour());
    addAndMakeVisible (tipProgramOpenButton);

    tipProgramStatusLabel.setText ("not applied", juce::dontSendNotification);
    tipProgramStatusLabel.setJustificationType (juce::Justification::centredLeft);
    tipProgramStatusLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    tipProgramStatusLabel.setColour (juce::Label::textColourId, quietTextColour());
    addAndMakeVisible (tipProgramStatusLabel);

    tipProgramMetaLabel.setText ("", juce::dontSendNotification);
    tipProgramMetaLabel.setJustificationType (juce::Justification::centredRight);
    tipProgramMetaLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    tipProgramMetaLabel.setColour (juce::Label::textColourId, quietTextColour());
    addAndMakeVisible (tipProgramMetaLabel);

    pitchListTitle.setText ("tip  note  R  low  high   p       tip  note  R  low  high   p", juce::dontSendNotification);
    pitchListTitle.setJustificationType (juce::Justification::centredLeft);
    pitchListTitle.setColour (juce::Label::textColourId, quietTextColour());
    pitchListTitle.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (pitchListTitle);

    for (size_t i = 0; i < tipPitchRows.size(); ++i)
    {
        auto& row = tipPitchRows[i];
        const auto tipIndex = static_cast<int> (i);
        row.label.setText (juce::String (tipIndex + 1), juce::dontSendNotification);
        row.label.setJustificationType (juce::Justification::centred);
        row.label.setColour (juce::Label::textColourId, quietTextColour());
        row.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (row.label);

        configurePitchEditor (row.pitch);
        configurePitchEditor (row.low);
        configurePitchEditor (row.high);
        configurePitchEditor (row.probability);
        addAndMakeVisible (row.pitch);
        addAndMakeVisible (row.low);
        addAndMakeVisible (row.high);
        addAndMakeVisible (row.probability);

        row.random.setButtonText ({});
        row.random.setClickingTogglesState (true);
        row.random.setColour (juce::TextButton::buttonColourId,
                              wireframePalette ? juce::Colour (0x2200fff0) : juce::Colours::white.withAlpha (0.35f));
        row.random.setColour (juce::TextButton::buttonOnColourId, activeButtonColour());
        row.random.setColour (juce::TextButton::textColourOffId, textColour());
        row.random.setColour (juce::TextButton::textColourOnId, inkColour());
        row.random.onClick = [this, tipIndex]
        {
            if (! suppressCallbacks && onTipPitchRandomChanged)
                onTipPitchRandomChanged (tipIndex, tipPitchRows[static_cast<size_t> (tipIndex)].random.getToggleState());
        };
        addAndMakeVisible (row.random);

        row.pitch.onReturnKey = [this, tipIndex] { commitTipPitchValue (tipIndex); };
        row.pitch.onFocusLost = [this, tipIndex] { commitTipPitchValue (tipIndex); };
        row.low.onReturnKey = [this, tipIndex] { commitTipPitchRange (tipIndex); };
        row.low.onFocusLost = [this, tipIndex] { commitTipPitchRange (tipIndex); };
        row.high.onReturnKey = [this, tipIndex] { commitTipPitchRange (tipIndex); };
        row.high.onFocusLost = [this, tipIndex] { commitTipPitchRange (tipIndex); };
        row.probability.onReturnKey = [this, tipIndex] { commitTipProbability (tipIndex); };
        row.probability.onFocusLost = [this, tipIndex] { commitTipProbability (tipIndex); };
    }

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

    if (activeTipSelected && ! activeTipProgramMode)
    {
        auto pitchArea = takeRow (bounds, 34);
        tipPitchControl.label.setBounds (pitchArea.removeFromLeft (50));
        tipPitchControl.slider.setBounds (pitchArea);
    }

    if (activePolygonSelected && activeTipProgramMode)
    {
        auto codeHeaderArea = takeRow (bounds, 34);
        tipSoundLanguageControl.label.setBounds (codeHeaderArea.removeFromLeft (50));
        tipSoundLanguageControl.combo.setBounds (codeHeaderArea.removeFromLeft (82));
        codeHeaderArea.removeFromLeft (gap);
        tipProgramOpenButton.setBounds (codeHeaderArea.removeFromRight (62));
        codeHeaderArea.removeFromRight (gap);
        tipProgramResetButton.setBounds (codeHeaderArea.removeFromRight (70));
        codeHeaderArea.removeFromRight (gap);
        tipProgramAuditionButton.setBounds (codeHeaderArea.removeFromRight (86));
        codeHeaderArea.removeFromRight (gap);
        tipProgramApplyButton.setBounds (codeHeaderArea.removeFromRight (72));
        codeHeaderArea.removeFromRight (gap);
        tipProgramTitle.setBounds (codeHeaderArea);

        auto statusArea = bounds.removeFromTop (20);
        tipProgramMetaLabel.setBounds (statusArea.removeFromRight (170));
        tipProgramStatusLabel.setBounds (statusArea);
        bounds.removeFromTop (6);

        const auto editorHeight = juce::jlimit (100, 230, bounds.getHeight() - 26);
        tipProgramEditor.setBounds (bounds.removeFromTop (editorHeight));
        bounds.removeFromTop (12);
    }
    else if (activePolygonSelected)
    {
        auto titleArea = bounds.removeFromTop (18);
        pitchListTitle.setBounds (titleArea);
        bounds.removeFromTop (7);

        const auto rows = (activeSides + 1) / 2;
        const auto rowHeight = 32;
        const auto rowGap = 7;
        const auto gridHeight = rows * rowHeight + juce::jmax (0, rows - 1) * rowGap;
        auto gridArea = bounds.removeFromTop (gridHeight);
        bounds.removeFromTop (14);
        const auto columnGap = 14;
        const auto columnWidth = (gridArea.getWidth() - columnGap) / 2;

        for (size_t i = 0; i < tipPitchRows.size(); ++i)
        {
            auto& row = tipPitchRows[i];

            if (static_cast<int> (i) >= activeSides)
                continue;

            const auto column = static_cast<int> (i) / rows;
            const auto rowInColumn = static_cast<int> (i) % rows;
            auto rowArea = gridArea.withX (gridArea.getX() + column * (columnWidth + columnGap))
                                   .withY (gridArea.getY() + rowInColumn * (rowHeight + rowGap))
                                   .withWidth (columnWidth)
                                   .withHeight (rowHeight);
            row.label.setBounds (rowArea.removeFromLeft (20));
            rowArea.removeFromLeft (4);
            row.pitch.setBounds (rowArea.removeFromLeft (40));
            rowArea.removeFromLeft (5);

            auto randomArea = rowArea.removeFromLeft (24);
            row.random.setBounds (randomArea.withSizeKeepingCentre (18, 18));

            rowArea.removeFromLeft (5);
            row.low.setBounds (rowArea.removeFromLeft (38));
            rowArea.removeFromLeft (4);
            row.high.setBounds (rowArea.removeFromLeft (38));
            rowArea.removeFromLeft (4);
            row.probability.setBounds (rowArea.removeFromLeft (juce::jmin (44, rowArea.getWidth())));
        }
    }

    bounds.removeFromTop (2);
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
                             bool colourWireframeMode)
{
    const juce::ScopedValueSetter<bool> scopedSetter (suppressCallbacks, true);
   #if ! UNFOLDING_HAS_WELD_CHUCK
    juce::ignoreUnused (tipSoundLanguages);
   #endif

    activeSwitchSelected = switchSelected;
    activeRotationControl = showRotationControl;
    activeTipSelected = tipSelected;
    activePolygonSelected = polygonSelected;
    activeTipProgramMode = tipProgramMode;
    activeSides = juce::jlimit (3, 8, sides);
    activeTipIndex = juce::jlimit (0, activeSides - 1, selectedTipIndex >= 0 ? selectedTipIndex : 0);

    if (wireframeTheme != colourWireframeMode)
    {
        wireframeTheme = colourWireframeMode;
        applyTheme();
    }
    else
    {
        wireframePalette = wireframeTheme;
    }

    selectBuildMode (toolMode, controlsMode);
    selectSideButton (sides);
    radiusControl.slider.setValue (radius, juce::dontSendNotification);
    flapControl.slider.setValue (flapDepth, juce::dontSendNotification);
    zoomControl.slider.setValue (zoom, juce::dontSendNotification);
    rotationControl.slider.setValue (rotationDegrees, juce::dontSendNotification);
    rotationControl.label.setVisible (activeRotationControl);
    rotationControl.slider.setVisible (activeRotationControl);
    tipPitchControl.slider.setValue (tipPitch, juce::dontSendNotification);
    tipPitchControl.label.setVisible (activeTipSelected && ! activeTipProgramMode);
    tipPitchControl.slider.setVisible (activeTipSelected && ! activeTipProgramMode);
    pitchListTitle.setVisible (activePolygonSelected && ! activeTipProgramMode);
    tipSoundLanguageControl.label.setVisible (activePolygonSelected && activeTipProgramMode);
    tipSoundLanguageControl.combo.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramTitle.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramApplyButton.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramAuditionButton.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramResetButton.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramOpenButton.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramStatusLabel.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramMetaLabel.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramEditor.setVisible (activePolygonSelected && activeTipProgramMode);
    tipSoundLanguageControl.combo.setSelectedId (
       #if UNFOLDING_HAS_WELD_CHUCK
                                                 tipSoundLanguages[static_cast<size_t> (activeTipIndex)] == TipSoundLanguage::chuck ? 2 : 1,
       #else
                                                 1,
       #endif
                                                 juce::dontSendNotification);
    activeTipSoundLanguage = tipSoundLanguages[static_cast<size_t> (activeTipIndex)];
    const auto nextProgram = tipSoundPrograms[static_cast<size_t> (activeTipIndex)];
    const auto switchedTip = loadedProgramTipIndex != activeTipIndex;

    tipProgramTitle.setText ("Tip " + juce::String (activeTipIndex + 1) + " program", juce::dontSendNotification);

    if (switchedTip || ! tipProgramEditor.hasKeyboardFocus (true) || ! tipProgramDirty)
    {
        tipProgramEditor.setText (nextProgram, false);
        loadedProgramTipIndex = activeTipIndex;
        loadedTipProgram = nextProgram;
        lastAppliedTipProgram = nextProgram;
        tipProgramDirty = false;
        setTipProgramStatus ("saved", false);

        if (codeEditorWindow != nullptr)
            codeEditorWindow->syncFromToolbar ("Tip " + juce::String (activeTipIndex + 1) + " code",
                                               nextProgram,
                                               wireframeTheme);
    }

    updateTipProgramMeta();
    for (size_t i = 0; i < tipPitchRows.size(); ++i)
    {
        auto& row = tipPitchRows[i];
        const auto visible = activePolygonSelected && ! activeTipProgramMode && static_cast<int> (i) < activeSides;
        row.label.setVisible (visible);
        row.pitch.setVisible (visible);
        row.random.setVisible (visible);
        row.low.setVisible (visible);
        row.high.setVisible (visible);
        row.probability.setVisible (visible);
        row.pitch.setText (tipRandom[i] ? juce::String ("R") : pitchTextForValue (tipPitches[i]), false);
        row.random.setToggleState (tipRandom[i], juce::dontSendNotification);
        row.low.setText (pitchTextForValue (tipRandomLow[i]), false);
        row.high.setText (pitchTextForValue (tipRandomHigh[i]), false);
        row.probability.setText (juce::String (juce::jlimit (0.0f, 1.0f, tipProbabilities[i]), 2), false);
        row.pitch.setEnabled (true);
        row.low.setEnabled (true);
        row.high.setEnabled (true);
        row.low.setAlpha (tipRandom[i] ? 1.0f : (wireframeTheme ? 0.78f : 0.72f));
        row.high.setAlpha (tipRandom[i] ? 1.0f : (wireframeTheme ? 0.78f : 0.72f));
        row.probability.setAlpha (1.0f);
    }
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

void CityToolbar::applyTheme()
{
    wireframePalette = wireframeTheme;

    auto styleButton = [] (juce::TextButton& button)
    {
        button.setColour (juce::TextButton::buttonColourId, inactiveButtonColour());
        button.setColour (juce::TextButton::buttonOnColourId,
                          wireframePalette ? activeButtonColour().withAlpha (0.78f) : activeButtonColour());
        button.setColour (juce::TextButton::textColourOffId, textColour());
        button.setColour (juce::TextButton::textColourOnId,
                          wireframePalette ? textColour() : inkColour());
    };

    for (auto* label : { &zoomControl.label, &radiusControl.label, &flapControl.label, &rotationControl.label,
                         &tipPitchControl.label, &phaseControl.label, &tempoControl.label, &standsControl.label,
                         &diameterControl.label, &blockLevelsControl.label, &blockSizeControl.label,
                         &switchOffTimeControl.label, &rateControl.label, &switchActivationControl.label,
                         &switchRetriggerControl.label, &tipSoundLanguageControl.label, &pitchListTitle,
                         &tipProgramTitle, &tipProgramStatusLabel, &tipProgramMetaLabel })
    {
        label->setColour (juce::Label::textColourId, quietTextColour());
    }

    for (auto* slider : { &zoomControl.slider, &radiusControl.slider, &flapControl.slider, &rotationControl.slider,
                          &tipPitchControl.slider, &phaseControl.slider, &tempoControl.slider, &standsControl.slider,
                          &diameterControl.slider, &blockLevelsControl.slider, &blockSizeControl.slider,
                          &switchOffTimeControl.slider })
    {
        slider->setColour (juce::Slider::backgroundColourId,
                           wireframeTheme ? juce::Colour (0x5523f7ff) : juce::Colour (0x40b6c7be));
        slider->setColour (juce::Slider::trackColourId, accentColour());
        slider->setColour (juce::Slider::thumbColourId, activeButtonColour());
        slider->setColour (juce::Slider::textBoxBackgroundColourId,
                           wireframeTheme ? juce::Colour (0xee050b12) : juce::Colours::white.withAlpha (0.68f));
        slider->setColour (juce::Slider::textBoxOutlineColourId,
                           wireframeTheme ? juce::Colour (0x9923f7ff) : juce::Colour (0x809db0a7));
        slider->setColour (juce::Slider::textBoxTextColourId, textColour());
    }

    for (auto* combo : { &rateControl.combo, &switchActivationControl.combo, &switchRetriggerControl.combo,
                         &tipSoundLanguageControl.combo })
    {
        combo->setColour (juce::ComboBox::backgroundColourId, inactiveButtonColour());
        combo->setColour (juce::ComboBox::outlineColourId, panelLineColour());
        combo->setColour (juce::ComboBox::textColourId, textColour());
        combo->setColour (juce::ComboBox::arrowColourId, accentColour());
    }

    tempoValueBox.setColour (juce::Label::backgroundColourId,
                             wireframeTheme ? juce::Colour (0xbb03080e) : juce::Colours::white.withAlpha (0.68f));
    tempoValueBox.setColour (juce::Label::outlineColourId,
                             wireframeTheme ? juce::Colour (0x9923f7ff) : juce::Colour (0x809db0a7));
    tempoValueBox.setColour (juce::Label::textColourId, textColour());
    tempoValueBox.setColour (juce::Label::textWhenEditingColourId, textColour());
    tempoValueBox.setColour (juce::Label::backgroundWhenEditingColourId,
                             wireframeTheme ? juce::Colour (0xff02070b) : juce::Colours::white.withAlpha (0.92f));

    for (auto* button : { &playButton, &pauseButton, &stopButton, &selectModeButton, &polygonModeButton,
                          &platterModeButton, &blockModeButton, &plankModeButton, &cableModeButton,
                          &tipProgramApplyButton, &tipProgramAuditionButton, &tipProgramResetButton,
                          &tipProgramOpenButton })
        styleButton (*button);

    deleteButton.setColour (juce::TextButton::buttonColourId,
                            wireframeTheme ? juce::Colour (0x4439ff88) : juce::Colour (0x55ffd766));
    deleteButton.setColour (juce::TextButton::buttonOnColourId, activeButtonColour());
    deleteButton.setColour (juce::TextButton::textColourOffId, textColour());
    deleteButton.setColour (juce::TextButton::textColourOnId, inkColour());
    clearButton.setColour (juce::TextButton::buttonColourId,
                           wireframeTheme ? juce::Colour (0x55ff4fd2) : juce::Colour (0x55ff8fb1));
    clearButton.setColour (juce::TextButton::buttonOnColourId,
                           wireframeTheme ? juce::Colour (0xccff4fd2) : juce::Colour (0xffff8fb1));
    clearButton.setColour (juce::TextButton::textColourOffId, textColour());
    clearButton.setColour (juce::TextButton::textColourOnId, inkColour());

    for (auto& row : tipPitchRows)
    {
        row.label.setColour (juce::Label::textColourId, quietTextColour());
        row.random.setColour (juce::TextButton::buttonColourId,
                              wireframeTheme ? juce::Colour (0x2200fff0) : juce::Colours::white.withAlpha (0.35f));
        row.random.setColour (juce::TextButton::buttonOnColourId,
                              wireframeTheme ? activeButtonColour().withAlpha (0.88f) : activeButtonColour());
        row.random.setColour (juce::TextButton::textColourOffId, textColour());
        row.random.setColour (juce::TextButton::textColourOnId, inkColour());
        applyPitchEditorTheme (row.pitch);
        applyPitchEditorTheme (row.low);
        applyPitchEditorTheme (row.high);
        applyPitchEditorTheme (row.probability);
    }

    applyProgramEditorTheme (tipProgramEditor);

    selectSideButton (activeSides);
    repaint();
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
                      wireframePalette ? colour.withAlpha (isSelected ? 0.62f : 0.16f)
                                       : (isSelected ? colour : colour.withAlpha (0.32f)));
    button.setColour (juce::TextButton::buttonOnColourId,
                      wireframePalette ? colour.withAlpha (0.72f) : colour);
    button.setColour (juce::TextButton::textColourOffId,
                      wireframePalette ? textColour() : (isSelected ? inkColour() : textColour()));
    button.setColour (juce::TextButton::textColourOnId,
                      wireframePalette ? textColour() : inkColour());
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
                           wireframePalette ? colour.withAlpha (selected ? 0.34f : 0.13f)
                                            : (selected ? colour : colour.withAlpha (0.32f)));
        button->setColour (juce::TextButton::buttonOnColourId,
                           wireframePalette ? colour.withAlpha (0.44f) : colour);
        button->setColour (juce::TextButton::textColourOffId,
                           wireframePalette ? textColour() : (selected ? inkColour() : textColour()));
        button->setColour (juce::TextButton::textColourOnId,
                           wireframePalette ? textColour() : inkColour());
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
    pitchListTitle.setVisible (activePolygonSelected && ! activeTipProgramMode);
    tipSoundLanguageControl.label.setVisible (activePolygonSelected && activeTipProgramMode);
    tipSoundLanguageControl.combo.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramTitle.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramApplyButton.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramAuditionButton.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramResetButton.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramOpenButton.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramStatusLabel.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramMetaLabel.setVisible (activePolygonSelected && activeTipProgramMode);
    tipProgramEditor.setVisible (activePolygonSelected && activeTipProgramMode);

    for (size_t i = 0; i < tipPitchRows.size(); ++i)
    {
        auto& row = tipPitchRows[i];
        const auto visible = activePolygonSelected && ! activeTipProgramMode && static_cast<int> (i) < activeSides;
        row.label.setVisible (visible);
        row.pitch.setVisible (visible);
        row.random.setVisible (visible);
        row.low.setVisible (visible);
        row.high.setVisible (visible);
        row.probability.setVisible (visible);
    }

    resized();
    repaint();
}

void CityToolbar::commitTipPitchValue (int tipIndex)
{
    if (suppressCallbacks)
        return;

    const auto index = static_cast<size_t> (juce::jlimit (0, 7, tipIndex));
    const auto text = tipPitchRows[index].pitch.getText();

    if (textRequestsRandomPitch (text))
    {
        if (onTipPitchRandomChanged)
            onTipPitchRandomChanged (tipIndex, true);

        return;
    }

    if (onTipPitchValueChanged)
        onTipPitchValueChanged (tipIndex, pitchValueFromText (text));
}

void CityToolbar::commitTipPitchRange (int tipIndex)
{
    if (suppressCallbacks || onTipPitchRangeChanged == nullptr)
        return;

    auto& row = tipPitchRows[static_cast<size_t> (juce::jlimit (0, 7, tipIndex))];
    onTipPitchRangeChanged (tipIndex, pitchValueFromText (row.low.getText()), pitchValueFromText (row.high.getText()));
}

void CityToolbar::commitTipProbability (int tipIndex)
{
    if (suppressCallbacks || onTipProbabilityChanged == nullptr)
        return;

    const auto index = static_cast<size_t> (juce::jlimit (0, 7, tipIndex));
    onTipProbabilityChanged (tipIndex, juce::jlimit (0.0f, 1.0f, static_cast<float> (tipPitchRows[index].probability.getText().getFloatValue())));
}

void CityToolbar::commitTipSoundProgram()
{
    if (suppressCallbacks || ! activeTipProgramMode || onTipSoundProgramChanged == nullptr)
        return;

    const auto program = tipProgramEditor.getText();
    const auto status = onTipSoundProgramChanged (activeTipIndex, program);
    lastAppliedTipProgram = program;
    loadedTipProgram = program;
    tipProgramDirty = false;
    updateTipProgramMeta();
    setTipProgramStatus (status, status.containsIgnoreCase ("error")
                               || status.containsIgnoreCase ("unavailable")
                               || status.containsIgnoreCase ("empty")
                               || status.containsIgnoreCase ("not ready"));
}

void CityToolbar::auditionTipSoundProgram()
{
    if (suppressCallbacks || ! activeTipProgramMode || onTipSoundProgramAudition == nullptr)
        return;

    const auto status = onTipSoundProgramAudition (activeTipIndex, tipProgramEditor.getText());
    setTipProgramStatus (status, status.containsIgnoreCase ("error")
                               || status.containsIgnoreCase ("unavailable")
                               || status.containsIgnoreCase ("empty")
                               || status.containsIgnoreCase ("not ready"));
}

void CityToolbar::resetTipSoundProgram()
{
    if (suppressCallbacks || ! activeTipProgramMode)
        return;

    const auto defaultProgram = defaultProgramForActiveTip();
    tipProgramEditor.setText (defaultProgram, false);
    tipProgramDirty = defaultProgram != lastAppliedTipProgram;
    updateTipProgramMeta();
    setTipProgramStatus ("default loaded - apply to save", false);
}

void CityToolbar::openLargeTipProgramEditor()
{
    if (! activeTipProgramMode)
        return;

    const auto title = "Tip " + juce::String (activeTipIndex + 1) + " code";

    if (codeEditorWindow == nullptr)
        codeEditorWindow = std::make_unique<CodeEditorWindow> (*this,
                                                               title,
                                                               tipProgramEditor.getText(),
                                                               wireframeTheme);
    else
    {
        codeEditorWindow->syncFromToolbar (title, tipProgramEditor.getText(), wireframeTheme);
        codeEditorWindow->toFront (true);
    }
}

void CityToolbar::applyLargeTipProgramEditorText (const juce::String& text)
{
    tipProgramEditor.setText (text, false);
    commitTipSoundProgram();
}

void CityToolbar::auditionLargeTipProgramEditorText (const juce::String& text)
{
    tipProgramEditor.setText (text, false);
    auditionTipSoundProgram();
}

void CityToolbar::resetLargeTipProgramEditorText()
{
    const auto defaultProgram = defaultProgramForActiveTip();
    tipProgramEditor.setText (defaultProgram, false);
    tipProgramDirty = defaultProgram != lastAppliedTipProgram;
    updateTipProgramMeta();
    setTipProgramStatus ("default loaded - apply to save", false);
}

juce::String CityToolbar::defaultProgramForActiveTip() const
{
    return FoldingModule::defaultTipSoundProgram (activeTipSoundLanguage,
                                                  activeTipIndex,
                                                  activeSides);
}

void CityToolbar::setTipProgramStatus (const juce::String& text, bool warning)
{
    setCodeStatus (tipProgramStatusLabel, text, warning);

    if (codeEditorWindow != nullptr)
        codeEditorWindow->setStatus (text, warning);
}

void CityToolbar::updateTipProgramMeta()
{
    const auto text = tipProgramEditor.getText();
    auto lines = 1;

    for (int i = 0; i < text.length(); ++i)
        if (text[i] == '\n')
            ++lines;

    tipProgramMetaLabel.setText ((tipProgramDirty ? "modified" : "saved")
                                     + juce::String ("  /  ")
                                     + juce::String (lines)
                                     + juce::String (lines == 1 ? " line  /  " : " lines  /  ")
                                     + juce::String (text.length())
                                     + " chars",
                                 juce::dontSendNotification);
}

float CityToolbar::pitchValueFromText (const juce::String& text)
{
    const auto trimmed = text.trim();

    if (trimmed.equalsIgnoreCase ("x"))
        return 0.0f;

    return juce::jlimit (0.0f, 96.0f, static_cast<float> (trimmed.getFloatValue()));
}

juce::String CityToolbar::pitchTextForValue (float value)
{
    return value <= 0.0f ? juce::String ("X") : juce::String (juce::roundToInt (value));
}

bool CityToolbar::textRequestsRandomPitch (const juce::String& text)
{
    return text.trim().equalsIgnoreCase ("r");
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
