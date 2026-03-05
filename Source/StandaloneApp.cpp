/*
  ==============================================================================
    StandaloneApp.cpp
    GOODMETER - Custom Standalone Application (Desktop Pet Mode)

    Replaces JUCE's default StandaloneFilterApp with a custom version that:
    - Creates a borderless, transparent, always-on-top window
    - Positions Nono at the bottom-right corner of the screen
    - Preserves full audio I/O via StandalonePluginHolder

    Architecture: Uses a raw Component window (NOT DocumentWindow/ResizableWindow)
    to avoid their multi-layer opaque paint routines. The StandalonePluginHolder
    handles audio device management separately from the window.

    Requires: JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
  ==============================================================================
*/

#include <JuceHeader.h>

#if JucePlugin_Build_Standalone

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace goodmeter
{

//==============================================================================
/**
 * Pure transparent component window for desktop pet mode.
 *
 * Does NOT inherit from DocumentWindow/ResizableWindow/StandaloneFilterWindow.
 * Instead, it's a plain Component that directly hosts the AudioProcessorEditor.
 * This avoids all the opaque background painting from the DocumentWindow chain.
 *
 * Audio I/O is managed by StandalonePluginHolder, which is independent of the window.
 */
class DesktopPetWindow : public juce::Component
{
public:
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;

    DesktopPetWindow(const juce::String& title,
                     std::unique_ptr<juce::StandalonePluginHolder> holder)
        : pluginHolder(std::move(holder))
    {
        juce::ignoreUnused(title);

        setOpaque(false);
        setSize(280, 360);

        // Create and add the editor
        if (auto* processor = pluginHolder->processor.get())
        {
            editor.reset(processor->createEditorIfNeeded());
            if (editor != nullptr)
            {
                editor->setOpaque(false);
                addAndMakeVisible(editor.get());

                // Size the window to match editor
                setSize(editor->getWidth(), editor->getHeight());
            }
        }

        // Add to desktop as a semi-transparent, borderless window
        addToDesktop(juce::ComponentPeer::windowIsSemiTransparent
                   | juce::ComponentPeer::windowAppearsOnTaskbar);

        setAlwaysOnTop(true);
        setVisible(true);

        // Position at bottom-right of primary display
        positionAtBottomRight();
    }

    ~DesktopPetWindow() override
    {
        if (editor != nullptr)
        {
            if (auto* processor = pluginHolder->processor.get())
                processor->editorBeingDeleted(editor.get());
            editor = nullptr;
        }
    }

    void paint(juce::Graphics&) override
    {
        // Fully transparent — only the editor draws
    }

    void resized() override
    {
        if (editor != nullptr)
            editor->setBounds(getLocalBounds());
    }

    void userTriedToCloseWindow() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

private:
    std::unique_ptr<juce::AudioProcessorEditor> editor;

    void positionAtBottomRight()
    {
        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            auto area = display->userArea;
            int w = getWidth();
            int h = getHeight();
            setTopLeftPosition(area.getRight() - w - 30,
                               area.getBottom() - h - 30);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DesktopPetWindow)
};

//==============================================================================
/**
 * Custom standalone application for GOODMETER desktop pet mode.
 */
class GoodMeterStandaloneApp : public juce::JUCEApplication
{
public:
    GoodMeterStandaloneApp()
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = juce::CharPointer_UTF8(JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        appProperties.setStorageParameters(options);
    }

    const juce::String getApplicationName() override    { return juce::CharPointer_UTF8(JucePlugin_Name); }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    //==========================================================================
    void initialise(const juce::String&) override
    {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
            return;

        auto pluginHolder = std::make_unique<juce::StandalonePluginHolder>(
            appProperties.getUserSettings(),
            false,          // don't take ownership of settings
            juce::String(), // preferred device name
            nullptr,        // preferred setup options
            juce::Array<juce::StandalonePluginHolder::PluginInOuts>(),
            false           // don't auto-open MIDI devices
        );

        mainWindow = std::make_unique<DesktopPetWindow>(
            getApplicationName(),
            std::move(pluginHolder)
        );
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            mainWindow->pluginHolder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay(100, []()
            {
                if (auto app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    juce::ApplicationProperties appProperties;
    std::unique_ptr<DesktopPetWindow> mainWindow;
};

} // namespace goodmeter

// Register our custom app with JUCE's application framework
JUCE_CREATE_APPLICATION_DEFINE(goodmeter::GoodMeterStandaloneApp)

#endif // JucePlugin_Build_Standalone
