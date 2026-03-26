/*
  ==============================================================================
    iOSMainApp.cpp
    GOODMETER iOS - JUCEApplication entry point

    Creates the main window and hosts iOSMainComponent as the root view.
    This is a guiapp (not a plugin) — no juce_audio_plugin_client module.
  ==============================================================================
*/

#include "iOSPluginDefines.h"
#include <JuceHeader.h>
#include "iOSMainComponent.h"

class GOODMETERiOSApp : public juce::JUCEApplication
{
public:
    GOODMETERiOSApp() {}

    const juce::String getApplicationName() override    { return "GOODMETER"; }
    const juce::String getApplicationVersion() override { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
    }

    void systemRequestedQuit() override
    {
        quit();
    }

    void anotherInstanceStarted(const juce::String&) override {}

    //==========================================================================
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow(juce::String name)
            : DocumentWindow(name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                  .findColour(juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar(true);
            setContentOwned(new iOSMainComponent(), true);

           #if JUCE_IOS
            setFullScreen(true);
           #else
            setResizable(true, true);
            centreWithSize(getWidth(), getHeight());
           #endif

            setVisible(true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(GOODMETERiOSApp)
