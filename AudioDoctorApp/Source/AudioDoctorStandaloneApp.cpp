/*
  ==============================================================================
    AudioDoctorStandaloneApp.cpp
    Audio Doctor - extracted GOODMETER Audio Doctor application shell.
  ==============================================================================
*/

#include <JuceHeader.h>
#include <iostream>

#include "../../Source/AudioDoctorComponent.h"
#include "../../Source/AudioDoctorJobRunner.h"

namespace goodmeter::audio_doctor_app
{

class MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow(juce::String title, juce::File exportDirectory, juce::LookAndFeel& lookAndFeelToUse)
        : juce::DocumentWindow(std::move(title),
                               juce::Colour(0xFF07080B),
                               juce::DocumentWindow::closeButton)
    {
        setOpaque(false);
        setUsingNativeTitleBar(false);
        setTitleBarHeight(30);
        setBackgroundColour(juce::Colours::transparentBlack);
        setResizable(true, false);
        setDropShadowEnabled(true);
        setResizeLimits(980, 650, 1800, 1300);
        setLookAndFeel(&lookAndFeelToUse);
        getProperties().set("audioDoctorUsesAppMenuSave", true);

        auto* content = new AudioDoctorContent(std::move(exportDirectory));
        content->setSize(1080, 820);
        content->setOpaque(false);
        content->setLookAndFeel(&lookAndFeelToUse);
        setContentOwned(content, true);

        centreWithSize(getWidth(), getHeight());
        setVisible(true);
    }

    ~MainWindow() override
    {
        setLookAndFeel(nullptr);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }

    bool loadAudioDoctorProject(const juce::File& projectPath, juce::String& error)
    {
        if (auto* content = dynamic_cast<AudioDoctorContent*>(getContentComponent()))
            return content->loadProjectPackageFromFile(projectPath, error);

        error = "Audio Doctor content is not available.";
        return false;
    }

    void saveAudioDoctorProject(AudioDoctorContent::ProjectPathCallback onSaved = {})
    {
        if (auto* content = dynamic_cast<AudioDoctorContent*>(getContentComponent()))
            content->saveProjectFromAppMenu(std::move(onSaved));
    }

    int getDesktopWindowStyleFlags() const override
    {
        return juce::DocumentWindow::getDesktopWindowStyleFlags()
             | juce::ComponentPeer::windowIsSemiTransparent;
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
};

namespace CommandIDs
{
    static constexpr juce::CommandID saveProject = 0x41534450; // ASDP
}

namespace MenuIDs
{
    static constexpr int recentProjectBase = 0x5200;
}

class AudioDoctorApplication final : public juce::JUCEApplication,
                                     public juce::MenuBarModel
{
public:
    AudioDoctorApplication()
    {
        juce::PropertiesFile::Options options;
        options.applicationName = "Audio Doctor";
        options.filenameSuffix = ".settings";
        options.osxLibrarySubFolder = "Application Support";
        appProperties.setStorageParameters(options);
        setApplicationCommandManagerToWatch(&commandManager);
        loadRecentProjects();
    }

    const juce::String getApplicationName() override    { return "Audio Doctor"; }
    const juce::String getApplicationVersion() override { return "1.0.2"; }

    bool moreThanOneInstanceAllowed() override
    {
        const auto args = juce::JUCEApplicationBase::getCommandLineParameterArray();
        for (const auto& arg : args)
        {
            if (arg == "--audio-doctor-job" || arg == "--doctor-job"
                || arg.startsWith("--audio-doctor-job=") || arg.startsWith("--doctor-job="))
                return true;
        }

        return false;
    }

    void initialise(const juce::String& commandLine) override
    {
        if (runAudioDoctorJobIfRequested(commandLine, true))
            return;

        juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

        mainWindow = std::make_unique<MainWindow>(getApplicationName(),
                                                  getDefaultExportDirectory(),
                                                  lookAndFeel);

        commandManager.registerAllCommandsForTarget(this);
        commandManager.setFirstCommandTarget(this);

#if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu(this);
#endif

        openAudioDoctorProjectIfRequested(commandLine);
    }

    void shutdown() override
    {
#if JUCE_MAC
        juce::MenuBarModel::setMacMainMenu(nullptr);
#endif
        mainWindow = nullptr;
        juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
    }

    void anotherInstanceStarted(const juce::String& commandLine) override
    {
        if (runAudioDoctorJobIfRequested(commandLine, false))
            return;

        openAudioDoctorProjectIfRequested(commandLine);
    }

    void systemRequestedQuit() override
    {
        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay(100, []()
            {
                if (auto* app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
            return;
        }

        quit();
    }

    juce::StringArray getMenuBarNames() override
    {
        return { "File" };
    }

    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override
    {
        juce::PopupMenu menu;
        if (topLevelMenuIndex == 0 && menuName == "File")
        {
            menu.addCommandItem(&commandManager, CommandIDs::saveProject);
            menu.addSeparator();

            juce::PopupMenu recentMenu;
            if (recentProjectPaths.isEmpty())
            {
                recentMenu.addItem(1, "No Recent Projects", false);
            }
            else
            {
                for (int i = 0; i < recentProjectPaths.size(); ++i)
                    recentMenu.addItem(MenuIDs::recentProjectBase + i,
                                       makeRecentProjectMenuLabel(juce::File(recentProjectPaths[i])));
            }

            menu.addSubMenu("Open Recent", recentMenu);
        }

        return menu;
    }

    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override
    {
        juce::ignoreUnused(topLevelMenuIndex);

        if (menuItemID >= MenuIDs::recentProjectBase)
            openRecentProject(menuItemID - MenuIDs::recentProjectBase);
    }

    juce::ApplicationCommandTarget* getNextCommandTarget() override
    {
        return nullptr;
    }

    void getAllCommands(juce::Array<juce::CommandID>& commands) override
    {
        commands.add(CommandIDs::saveProject);
    }

    void getCommandInfo(juce::CommandID commandID, juce::ApplicationCommandInfo& result) override
    {
        if (commandID == CommandIDs::saveProject)
        {
            result.setInfo("Save Project",
                           "Save the current Audio Doctor project package.",
                           "File",
                           0);
            result.addDefaultKeypress('s', juce::ModifierKeys::commandModifier);
        }
    }

    bool perform(const InvocationInfo& info) override
    {
        if (info.commandID == CommandIDs::saveProject)
        {
            if (mainWindow != nullptr)
            {
                mainWindow->saveAudioDoctorProject([this](const juce::File& projectFile)
                {
                    addRecentProject(projectFile);
                });
            }

            return true;
        }

        return false;
    }

private:
    GoodMeterLookAndFeel lookAndFeel;
    juce::ApplicationCommandManager commandManager;
    juce::ApplicationProperties appProperties;
    std::unique_ptr<MainWindow> mainWindow;
    juce::StringArray recentProjectPaths;

    static constexpr int maxRecentProjects = 10;
    static constexpr const char* recentProjectsKey = "recentProjects";

    static juce::File getDefaultExportDirectory()
    {
        auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                       .getChildFile("Audio Doctor");
        dir.createDirectory();
        return dir;
    }

    static juce::String stripPathQuotes(juce::String text)
    {
        text = text.trim();
        if (text.startsWithChar('"') || text.startsWithChar('\''))
            text = text.substring(1);
        if (text.endsWithChar('"') || text.endsWithChar('\''))
            text = text.dropLastCharacters(1);
        return text.trim();
    }

    static bool isAudioDoctorProjectPath(const juce::File& file)
    {
        if (file.existsAsFile() && file.getFileName().equalsIgnoreCase("project.json"))
            return true;

        const auto ext = file.getFileExtension().toLowerCase();
        if (ext != ".clz" && ext != ".goodmeterdoctor")
            return false;

        return file.exists() || file.getChildFile("project.json").existsAsFile();
    }

    static juce::String makeRecentProjectMenuLabel(const juce::File& file)
    {
        const auto projectName = file.getFileName().isNotEmpty() ? file.getFileName()
                                                                 : file.getFullPathName();
        const auto parentName = file.getParentDirectory().getFileName();
        return parentName.isNotEmpty() ? projectName + " - " + parentName
                                       : projectName;
    }

    void loadRecentProjects()
    {
        recentProjectPaths.clear();
        if (auto* settings = appProperties.getUserSettings())
        {
            recentProjectPaths.addLines(settings->getValue(recentProjectsKey));
            recentProjectPaths.trim();
            recentProjectPaths.removeEmptyStrings();
        }
    }

    void saveRecentProjects()
    {
        if (auto* settings = appProperties.getUserSettings())
        {
            settings->setValue(recentProjectsKey, recentProjectPaths.joinIntoString("\n"));
            settings->saveIfNeeded();
        }
        menuItemsChanged();
    }

    void addRecentProject(const juce::File& projectFile)
    {
        if (projectFile == juce::File{})
            return;

        const auto path = projectFile.getFullPathName();
        recentProjectPaths.removeString(path);
        recentProjectPaths.insert(0, path);

        while (recentProjectPaths.size() > maxRecentProjects)
            recentProjectPaths.remove(recentProjectPaths.size() - 1);

        saveRecentProjects();
    }

    void removeRecentProjectAt(int index)
    {
        if (juce::isPositiveAndBelow(index, recentProjectPaths.size()))
        {
            recentProjectPaths.remove(index);
            saveRecentProjects();
        }
    }

    void showProjectOpenError(const juce::String& error)
    {
        if (error.isEmpty())
            return;

        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
                                               "Audio Doctor Project",
                                               error);
    }

    void openRecentProject(int index)
    {
        if (!juce::isPositiveAndBelow(index, recentProjectPaths.size()))
            return;

        const auto projectFile = juce::File(recentProjectPaths[index]);
        if (!isAudioDoctorProjectPath(projectFile))
        {
            removeRecentProjectAt(index);
            showProjectOpenError("This recent Audio Doctor project is no longer available:\n"
                                 + projectFile.getFullPathName());
            return;
        }

        if (mainWindow != nullptr)
            mainWindow->toFront(true);

        juce::String error;
        if (mainWindow != nullptr && mainWindow->loadAudioDoctorProject(projectFile, error))
        {
            addRecentProject(projectFile);
            return;
        }

        showProjectOpenError(error);
    }

    static juce::File findAudioDoctorProjectArgument(const juce::String& commandLine)
    {
        juce::StringArray args;
        args.addTokens(commandLine, true);
        if (args.isEmpty())
            args = juce::JUCEApplicationBase::getCommandLineParameterArray();
        args.trim();
        args.removeEmptyStrings();

        for (int start = 0; start < args.size(); ++start)
        {
            auto joined = stripPathQuotes(args[start]);
            if (joined.startsWithChar('-'))
                continue;

            for (int end = start; end < args.size(); ++end)
            {
                if (end > start)
                    joined << " " << stripPathQuotes(args[end]);

                const juce::File candidate(joined);
                if (isAudioDoctorProjectPath(candidate))
                    return candidate;
            }
        }

        return {};
    }

    bool runAudioDoctorJobIfRequested(const juce::String& commandLine, bool shouldQuitAfter)
    {
        auto args = juce::JUCEApplicationBase::getCommandLineParameterArray();
        if (args.isEmpty())
            args.addTokens(commandLine, true);
        args.trim();
        args.removeEmptyStrings();

        int index = args.indexOf("--audio-doctor-job");
        if (index < 0)
            index = args.indexOf("--doctor-job");
        int pathContinuationStart = -1;

        juce::String jobPath;
        if (index >= 0)
        {
            if (index + 1 >= args.size())
                return false;

            jobPath = stripPathQuotes(args[index + 1]);
            pathContinuationStart = index + 2;
        }
        else
        {
            for (int i = 0; i < args.size(); ++i)
            {
                const auto& arg = args[i];
                if (arg.startsWith("--audio-doctor-job="))
                {
                    jobPath = stripPathQuotes(arg.fromFirstOccurrenceOf("=", false, false));
                    pathContinuationStart = i + 1;
                    break;
                }

                if (arg.startsWith("--doctor-job="))
                {
                    jobPath = stripPathQuotes(arg.fromFirstOccurrenceOf("=", false, false));
                    pathContinuationStart = i + 1;
                    break;
                }
            }

            if (jobPath.isEmpty())
                return false;
        }

        if (!juce::File(jobPath).existsAsFile())
        {
            const int startIndex = juce::jmax(0, pathContinuationStart);
            for (int i = startIndex; i < args.size(); ++i)
            {
                if (args[i].startsWith("--"))
                    break;

                jobPath << " " << stripPathQuotes(args[i]);
                jobPath = stripPathQuotes(jobPath);
                if (juce::File(jobPath).existsAsFile())
                    break;
            }
        }

        juce::String response;
        goodmeter::audio_doctor::runAudioDoctorJobFile(juce::File(jobPath), response);
        std::cout << response << std::endl;

        if (shouldQuitAfter)
            quit();

        return true;
    }

    void openAudioDoctorProjectIfRequested(const juce::String& commandLine)
    {
        const auto projectFile = findAudioDoctorProjectArgument(commandLine);
        if (projectFile == juce::File{})
            return;

        if (mainWindow != nullptr)
            mainWindow->toFront(true);

        juce::String error;
        if (mainWindow == nullptr || !mainWindow->loadAudioDoctorProject(projectFile, error))
        {
            showProjectOpenError(error.isNotEmpty() ? error
                                                    : "Audio Doctor project could not be opened.");
            return;
        }

        addRecentProject(projectFile);
    }
};

} // namespace goodmeter::audio_doctor_app

START_JUCE_APPLICATION(goodmeter::audio_doctor_app::AudioDoctorApplication)
