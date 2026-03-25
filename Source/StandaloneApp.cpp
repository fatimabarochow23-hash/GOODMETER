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
#include "PluginProcessor.h"
#include "GoodMeterLookAndFeel.h"
#include "AudioLabComponent.h"

#if JUCE_MAC
 #include <objc/message.h>
 #include <objc/runtime.h>
 #include <CoreGraphics/CoreGraphics.h>

// Pass-through replacement for NSWindow's constrainFrameRect:toScreen:
// Returns the proposed frame unmodified, allowing the window to be
// positioned below the Dock / above the menu bar — essential for a
// desktop pet that should roam freely across the entire screen.
static CGRect noConstrainFrameRect(id, SEL, CGRect frameRect, id /*screen*/)
{
    return frameRect;
}
#endif

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
class DesktopPetWindow : public juce::Component,
                         public juce::Timer
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

        // Don't intercept mouse clicks — pass through to editor/HoloNono
        setInterceptsMouseClicks(false, true);

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

        // Disable macOS Dock/menu bar position clamping so Nono can
        // roam freely across the entire screen, including below the Dock.
        disableFrameConstraint();

        setAlwaysOnTop(true);
        setVisible(true);

        // Position at bottom-right of primary display
        positionAtBottomRight();

        // Start click-through management timer (60Hz polling)
        startTimerHz(60);
    }

    ~DesktopPetWindow() override
    {
        stopTimer();
        if (editor != nullptr)
        {
            if (auto* processor = pluginHolder->processor.get())
                processor->editorBeingDeleted(editor.get());
            editor = nullptr;
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::transparentBlack);
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

    //==========================================================================
    // Hit test: THE OS-level gate for click-through.
    // If this returns false, macOS passes the click to whatever is behind.
    // Delegates to editor's pure-geometry hitTest — no child component
    // delegation anywhere in the chain.
    //==========================================================================
    bool hitTest(int x, int y) override
    {
        if (editor != nullptr)
        {
            auto localPoint = juce::Point<int>(x, y) - editor->getPosition();
            if (editor->getLocalBounds().contains(localPoint))
                return editor->hitTest(localPoint.x, localPoint.y);
        }
        return false;
    }

    //==========================================================================
    // Dynamic click-through engine (macOS native NSWindow integration)
    //
    // macOS Window Server uses pixel alpha to route clicks — any alpha > 0
    // pixel (glow, shadow, anti-alias fringe) makes the OS assign the click
    // to our window BEFORE hitTest is ever called. If hitTest then returns
    // false, the click is silently dropped ("click black hole").
    //
    // Fix: toggle [NSWindow setIgnoresMouseEvents:] based on whether the
    // mouse cursor is currently over a clickable region. When ignoring,
    // macOS skips our window entirely and delivers clicks to the desktop.
    //==========================================================================
    void timerCallback() override
    {
        updateClickThrough();
    }

private:
    std::unique_ptr<juce::AudioProcessorEditor> editor;
    bool windowIgnoringMouse = false;

    //==========================================================================
    // 60Hz global mouse poll → hitTest → toggle ignoresMouseEvents
    //==========================================================================
    void updateClickThrough()
    {
#if JUCE_MAC
        // Guard: don't switch to ignore while a drag is active — that would
        // interrupt the drag if the cursor leaves the clickable region.
        if (!windowIgnoringMouse
            && juce::ModifierKeys::currentModifiers.isAnyMouseButtonDown())
            return;

        auto screenPos = juce::Desktop::getMousePosition();
        auto localPos  = getLocalPoint(nullptr, screenPos);

        bool wantsMouse = false;

        if (getLocalBounds().contains(localPos) && editor != nullptr)
        {
            auto ep = localPos - editor->getPosition();
            if (editor->getLocalBounds().contains(ep))
                wantsMouse = editor->hitTest(ep.x, ep.y);
        }

        // Toggle only when state actually needs to change
        if (wantsMouse && windowIgnoringMouse)
        {
            setNativeIgnoreMouse(false);
            windowIgnoringMouse = false;
        }
        else if (!wantsMouse && !windowIgnoringMouse)
        {
            setNativeIgnoreMouse(true);
            windowIgnoringMouse = true;
        }
#endif
    }

    //==========================================================================
    // Native bridge: [NSWindow setIgnoresMouseEvents:] via ObjC runtime
    //
    // Uses objc_msgSend directly so we stay in a .cpp file — no need to
    // rename to .mm or change build settings.
    //==========================================================================
    void setNativeIgnoreMouse([[maybe_unused]] bool shouldIgnore)
    {
#if JUCE_MAC
        if (auto* peer = getPeer())
        {
            auto nsView = reinterpret_cast<id>(peer->getNativeHandle());
            auto nsWindow = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(
                nsView, sel_registerName("window"));

            if (nsWindow != nullptr)
                reinterpret_cast<void (*)(id, SEL, BOOL)>(objc_msgSend)(
                    nsWindow, sel_registerName("setIgnoresMouseEvents:"),
                    static_cast<BOOL>(shouldIgnore));
        }
#endif
    }

    //==========================================================================
    // Disable macOS Dock/menu bar position clamping.
    //
    // JUCE's NSWindow subclass overrides constrainFrameRect:toScreen:
    // and calls super, which clamps the window to [NSScreen visibleFrame]
    // (excluding Dock and menu bar). This prevents a desktop pet from
    // being dragged below the Dock.
    //
    // Fix: replace the method implementation on the JUCE NSWindow's class
    // with a pass-through that returns the proposed frame unmodified.
    //==========================================================================
    void disableFrameConstraint()
    {
#if JUCE_MAC
        if (auto* peer = getPeer())
        {
            auto nsView = reinterpret_cast<id>(peer->getNativeHandle());
            auto nsWindow = reinterpret_cast<id (*)(id, SEL)>(objc_msgSend)(
                nsView, sel_registerName("window"));

            if (nsWindow != nullptr)
            {
                Class windowClass = object_getClass(nsWindow);
                SEL sel = sel_registerName("constrainFrameRect:toScreen:");
                Method m = class_getInstanceMethod(windowClass, sel);
                if (m != nullptr)
                    method_setImplementation(m, reinterpret_cast<IMP>(noConstrainFrameRect));
            }
        }
#endif
    }

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
 * Implements MenuBarModel for native macOS menu bar.
 */
class GoodMeterStandaloneApp : public juce::JUCEApplication,
                                public juce::MenuBarModel
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

        // Set Neo-Brutalism LookAndFeel as the global default
        // so ALL windows, dialogs, and popups inherit it
        juce::LookAndFeel::setDefaultLookAndFeel(&appLookAndFeel);

        auto pluginHolder = std::make_unique<juce::StandalonePluginHolder>(
            appProperties.getUserSettings(),
            false,          // don't take ownership of settings
            juce::String(), // preferred device name
            nullptr,        // preferred setup options
            juce::Array<juce::StandalonePluginHolder::PluginInOuts>(),
            false           // don't auto-open MIDI devices
        );

        // CRITICAL: JUCE defaults shouldMuteInput to TRUE, which silences
        // the microphone input in audioDeviceIOCallback by replacing the
        // input buffer with zeros. We are a metering app — we NEED live input.
        pluginHolder->getMuteInputValue().setValue(false);

        mainWindow = std::make_unique<DesktopPetWindow>(
            getApplicationName(),
            std::move(pluginHolder)
        );

#if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu(this);
#endif
    }

    void shutdown() override
    {
#if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu(nullptr);
#endif
        mainWindow = nullptr;
        appProperties.saveIfNeeded();
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
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

    //==========================================================================
    // MenuBarModel implementation — "Recording" dedicated menu
    //==========================================================================
    juce::StringArray getMenuBarNames() override
    {
        return { "Recording", "Audio Source", "Audio Lab" };
    }

    juce::PopupMenu getMenuForIndex(int menuIndex, const juce::String&) override
    {
        juce::PopupMenu menu;
        if (menuIndex == 0)
        {

        // ── 1. Start / Stop Recording toggle ──
        auto* proc = getProcessor();
        bool recording = (proc != nullptr && proc->audioRecorder.getIsRecording());
        menu.addItem(1, recording ? "Stop Recording" : "Start Recording", true, recording);

        menu.addSeparator();

        // ── 2. Recent Recordings sub-menu (dynamically scanned on EVERY open) ──
        juce::PopupMenu recentMenu;
        auto recDir = getRecordingDirectory();
        auto wavFiles = recDir.findChildFiles(juce::File::findFiles, false, "GOODMETER_*.wav");

        // Sort by modification time (newest first)
        wavFiles.sort();
        std::reverse(wavFiles.begin(), wavFiles.end());

        // Show up to 10 most recent
        int recentID = 100;
        int shown = 0;
        for (auto& f : wavFiles)
        {
            if (shown >= 10) break;
            recentMenu.addItem(recentID, f.getFileName());
            recentID++;
            shown++;
        }

        if (shown == 0)
            recentMenu.addItem(-1, "(No recordings yet)", false);

        menu.addSubMenu("Recent Recordings", recentMenu);

        // ── 3. Reveal in Finder ──
        juce::File lastFile;
        if (proc != nullptr && proc->audioRecorder.getIsRecording())
            lastFile = proc->audioRecorder.getRecordingFile();
        else if (proc != nullptr && proc->audioRecorder.getLastRecordedFile().existsAsFile())
            lastFile = proc->audioRecorder.getLastRecordedFile();
        else if (!wavFiles.isEmpty())
            lastFile = wavFiles.getFirst();

        menu.addItem(50, "Reveal in Finder", lastFile.existsAsFile());

        menu.addSeparator();

        // ── 4. Set Recording Location... ──
        menu.addItem(60, "Set Recording Location...");

        // Show current location as greyed-out hint
        menu.addItem(-1, juce::String(juce::CharPointer_UTF8(u8"\u2192 ")) + recDir.getFullPathName(), false);

        } // end menuIndex == 0 (Recording)
        else if (menuIndex == 1)
        {
            // ── Audio Source menu: Microphone / System Audio toggle ──
#if JUCE_MAC
            auto* proc = getProcessor();
            bool sysActive = (proc != nullptr && proc->useSystemAudio.load(std::memory_order_relaxed));
            menu.addItem(700, "Microphone Input", true, !sysActive);
            menu.addItem(701, "System Audio (CoreAudio Tap)", true, sysActive);
#else
            menu.addItem(-1, "System audio requires macOS 14.2+", false);
#endif
        }
        else if (menuIndex == 2)
        {
            // ── Audio Lab export mode: radio group ──
            int mode = AudioLabContent::exportMode;
            menu.addItem(800, "Export Both",          true, mode == 1);
            menu.addItem(801, "Export Clean Only",    true, mode == 2);
            menu.addItem(802, "Export RoomTone Only", true, mode == 3);
        }

        return menu;
    }

    void menuItemSelected(int menuItemID, int) override
    {
        if (menuItemID == 1)
        {
            // Toggle recording
            auto* proc = getProcessor();
            if (proc == nullptr) return;

            if (proc->audioRecorder.getIsRecording())
            {
                proc->audioRecorder.stop();

                // Reveal the just-finished file in Finder immediately
                auto lastFile = proc->audioRecorder.getLastRecordedFile();
                if (lastFile.existsAsFile())
                    lastFile.revealToUser();
            }
            else
            {
                auto now = juce::Time::getCurrentTime();
                auto filename = "GOODMETER_" + now.formatted("%Y%m%d_%H%M%S") + ".wav";
                auto recDir = getRecordingDirectory();
                auto file = recDir.getChildFile(filename);

                double sr = 48000.0;
                if (mainWindow != nullptr && mainWindow->pluginHolder != nullptr)
                {
                    if (auto* device = mainWindow->pluginHolder->deviceManager.getCurrentAudioDevice())
                        sr = device->getCurrentSampleRate();
                }

                proc->audioRecorder.start(file, sr, 2);
            }

            // Force menu rebuild so label flips Start↔Stop and Recent Recordings refreshes
            menuItemsChanged();
        }
        else if (menuItemID == 50)
        {
            // Reveal in Finder
            auto* proc = getProcessor();
            juce::File target;
            if (proc != nullptr && proc->audioRecorder.getIsRecording())
                target = proc->audioRecorder.getRecordingFile();
            else if (proc != nullptr && proc->audioRecorder.getLastRecordedFile().existsAsFile())
                target = proc->audioRecorder.getLastRecordedFile();
            else
            {
                auto recDir = getRecordingDirectory();
                auto wavFiles = recDir.findChildFiles(juce::File::findFiles, false, "GOODMETER_*.wav");
                wavFiles.sort();
                if (!wavFiles.isEmpty())
                    target = wavFiles.getLast();
            }

            if (target.existsAsFile())
                target.revealToUser();
        }
        else if (menuItemID == 60)
        {
            // Set Recording Location...
            chooser = std::make_unique<juce::FileChooser>(
                "Choose Recording Location",
                getRecordingDirectory(),
                "",
                true);

            chooser->launchAsync(juce::FileBrowserComponent::openMode
                               | juce::FileBrowserComponent::canSelectDirectories,
                [this](const juce::FileChooser& fc)
                {
                    auto result = fc.getResult();
                    if (result.isDirectory())
                    {
                        setRecordingDirectory(result);
                    }
                });
        }
        else if (menuItemID >= 100 && menuItemID < 200)
        {
            // Recent Recordings — reveal the clicked file
            auto recDir = getRecordingDirectory();
            auto wavFiles = recDir.findChildFiles(juce::File::findFiles, false, "GOODMETER_*.wav");
            wavFiles.sort();
            std::reverse(wavFiles.begin(), wavFiles.end());

            int idx = menuItemID - 100;
            if (idx >= 0 && idx < wavFiles.size())
                wavFiles[idx].revealToUser();
        }
#if JUCE_MAC
        else if (menuItemID == 700)  // Microphone Input
        {
            auto* proc = getProcessor();
            if (proc && proc->systemAudioCapture)
                proc->systemAudioCapture->stop();
            if (proc)
                proc->useSystemAudio.store(false, std::memory_order_relaxed);
            menuItemsChanged();
        }
        else if (menuItemID == 701)  // System Audio
        {
            auto* proc = getProcessor();
            if (proc && proc->systemAudioCapture)
            {
                double sr = 48000.0;
                if (mainWindow != nullptr && mainWindow->pluginHolder != nullptr)
                    if (auto* device = mainWindow->pluginHolder->deviceManager.getCurrentAudioDevice())
                        sr = device->getCurrentSampleRate();

                // Async start — will not block the UI.
                // Set useSystemAudio immediately; processBlock checks isActive()
                // before actually reading from the capture stream.
                proc->systemAudioCapture->startAsync(sr);
                proc->useSystemAudio.store(true, std::memory_order_relaxed);
            }
            menuItemsChanged();
        }
#endif
        else if (menuItemID >= 800 && menuItemID <= 802)
        {
            // Audio Lab export mode: 800=Both(1), 801=Clean(2), 802=RoomTone(3)
            AudioLabContent::exportMode = menuItemID - 800 + 1;
            menuItemsChanged();
        }
    }

private:
    juce::ApplicationProperties appProperties;
    GoodMeterLookAndFeel appLookAndFeel;
    std::unique_ptr<DesktopPetWindow> mainWindow;
    std::unique_ptr<juce::FileChooser> chooser;

    /** Safely get our processor from the plugin holder */
    GOODMETERAudioProcessor* getProcessor() const
    {
        if (mainWindow != nullptr && mainWindow->pluginHolder != nullptr)
            return dynamic_cast<GOODMETERAudioProcessor*>(mainWindow->pluginHolder->processor.get());
        return nullptr;
    }

    /** Get the current recording directory (persisted in PropertiesFile).
     *  Falls back to ~/Desktop if no custom path is set or if saved path is invalid. */
    juce::File getRecordingDirectory()
    {
        if (auto* props = appProperties.getUserSettings())
        {
            auto saved = props->getValue("recordingDirectory", "");
            if (saved.isNotEmpty())
            {
                juce::File dir(saved);
                if (dir.isDirectory())
                    return dir;
            }
        }
        return juce::File::getSpecialLocation(juce::File::userDesktopDirectory);
    }

    /** Set and persist the recording directory — immediately invalidates menu cache */
    void setRecordingDirectory(const juce::File& dir)
    {
        if (auto* props = appProperties.getUserSettings())
        {
            props->setValue("recordingDirectory", dir.getFullPathName());
            props->saveIfNeeded();
        }
        // Force macOS MenuBar to rebuild on next click (kills the 2-min stale cache)
        menuItemsChanged();
    }
};

} // namespace goodmeter

// Register our custom app with JUCE's application framework
JUCE_CREATE_APPLICATION_DEFINE(goodmeter::GoodMeterStandaloneApp)

#endif // JucePlugin_Build_Standalone
