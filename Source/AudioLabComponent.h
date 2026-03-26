/*
  ==============================================================================
    AudioLabComponent.h
    GOODMETER - Audio Lab (洗音暗房)

    DialogWindow content for offline audio processing:
      - Import audio files (WAV, AIFF, FLAC, MP3, M4A, OGG)
      - Three display modes: Waveform (AudioThumbnail) / Holo-PSR (holographic grid)
                             / Spectrogram (STFT energy heatmap)
      - Room Tone extraction: VAD → spectral envelope → white noise synthesis
      - Three-track export:
          1. Clean audio (_clean.wav)     — DeepFilterNet3 (Phase 2)
          2. Real noise floor (_noise.wav) — original minus clean (Phase 2)
          3. Synthesized Room Tone (_roomtone.wav) — VAD + spectral synthesis

    Performance: Holo-PSR peaks are pre-computed on import (max 512 floats).
                 Spectrogram image is pre-computed on import.
                 paint() never touches the raw audioData buffer.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"
#include "RoomToneExtractor.h"
#include "DeepFilterProcessor.h"

//==============================================================================
class AudioLabContent : public juce::Component, private juce::Timer
{
public:
    // Export mode: 1=Both, 2=Clean only, 3=RoomTone only (set from macOS menu bar)
    static inline int exportMode = 1;

    AudioLabContent(const juce::File& exportDir = {},
                    juce::AudioDeviceManager* sharedDevMgr = nullptr)
        : thumbnail(512, formatManager, thumbCache),
          exportDirectory(exportDir)
    {
        formatManager.registerBasicFormats();

        // ── Initialize DeepFilterNet3 ──
        initDeepFilter();

        // ── Toolbar buttons ──
        importBtn.onClick = [this]() { importAudioFile(); };
        processBtn.onClick = [this]() { processAudio(); };
        exportBtn.onClick = [this]() { exportThreeTracks(); };
        resetBtn.onClick = [this]() { resetAll(); };

        processBtn.setEnabled(false);
        exportBtn.setEnabled(false);
        resetBtn.setEnabled(false);

        addAndMakeVisible(importBtn);
        addAndMakeVisible(processBtn);
        addAndMakeVisible(exportBtn);
        addAndMakeVisible(resetBtn);

        // ── Display mode toggles (radio group) ──
        waveformToggle.setRadioGroupId(1001);
        holoPsrToggle.setRadioGroupId(1001);
        spectrogramToggle.setRadioGroupId(1001);
        holoPsrToggle.setToggleState(true, juce::dontSendNotification);

        waveformToggle.onClick = [this]()
        {
            if (waveformToggle.getToggleState())
            {
                displayMode = DisplayMode::Waveform;
                updateToolbarColours();
                repaint();
            }
        };
        holoPsrToggle.onClick = [this]()
        {
            if (holoPsrToggle.getToggleState())
            {
                displayMode = DisplayMode::HoloPsr;
                updateToolbarColours();
                repaint();
            }
        };
        spectrogramToggle.onClick = [this]()
        {
            if (spectrogramToggle.getToggleState())
            {
                displayMode = DisplayMode::Spectrogram;
                updateToolbarColours();
                repaint();
            }
        };

        addAndMakeVisible(waveformToggle);
        addAndMakeVisible(holoPsrToggle);
        addAndMakeVisible(spectrogramToggle);

        // ── Room Tone duration selector ──
        roomToneDurationBox.addItem("10 sec", 1);
        roomToneDurationBox.addItem("30 sec", 2);
        roomToneDurationBox.addItem("60 sec", 3);
        roomToneDurationBox.addItem("120 sec", 4);
        roomToneDurationBox.setSelectedId(2);  // default 30s
        addAndMakeVisible(roomToneDurationBox);

        // ── Denoise strength slider (wet/dry blend) ──
        denoiseStrengthSlider.setRange(0.0, 1.0, 0.01);
        denoiseStrengthSlider.setValue(0.85);
        denoiseStrengthSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        denoiseStrengthSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 36, 20);
        denoiseStrengthSlider.setNumDecimalPlacesToDisplay(2);
        addAndMakeVisible(denoiseStrengthSlider);
        denoiseStrengthLabel.attachToComponent(&denoiseStrengthSlider, true);
        addAndMakeVisible(denoiseStrengthLabel);

        // ── Preview transport controls ──
        playBtn.onClick = [this]() { togglePlayback(); };
        playBtn.setEnabled(false);
        addAndMakeVisible(playBtn);

        previewToggle.onClick = [this]()
        {
            previewProcessed = previewToggle.getToggleState();
            previewSource.setPreviewProcessed(previewProcessed);
        };
        previewToggle.setEnabled(false);
        addAndMakeVisible(previewToggle);

        // Update preview blend when slider changes during playback
        denoiseStrengthSlider.onValueChange = [this]()
        {
            previewSource.setWetDry(
                static_cast<float>(denoiseStrengthSlider.getValue()));
        };

        // ── Audio device for preview playback ──
        // Use shared device manager if provided (avoids CoreAudio conflict with main app).
        // Fall back to creating own device manager if none provided (e.g. plugin mode).
        if (sharedDevMgr != nullptr)
        {
            deviceMgr = sharedDevMgr;
        }
        else
        {
            ownDeviceManager = std::make_unique<juce::AudioDeviceManager>();
            ownDeviceManager->initialiseWithDefaultDevices(0, 2);
            deviceMgr = ownDeviceManager.get();
        }
        audioSourcePlayer.setSource(&previewSource);

        // Allow spacebar shortcut
        setWantsKeyboardFocus(true);

        // Set initial toolbar colours for default mode (HoloPsr = dark)
        updateToolbarColours();
    }

    ~AudioLabContent() override
    {
        // Invalidate alive flag FIRST — any pending callAsync will bail out
        aliveFlag->store(false);

        // Reset title bar flags when dialog closes
        GoodMeterLookAndFeel::holoTitleBar = false;
        GoodMeterLookAndFeel::spectroTitleBar = false;
        GoodMeterLookAndFeel::spectroProcessed = false;

        stopTimer();
        // Stop preview playback
        stopPlayback();
        audioSourcePlayer.setSource(nullptr);
        deviceMgr->removeAudioCallback(&audioSourcePlayer);
        // Wait for processing thread to finish
        if (processingThread.joinable())
            processingThread.join();
        if (exportThread.joinable())
            exportThread.join();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        // ── Mode-specific full-window background ──
        if (displayMode == DisplayMode::Waveform)
        {
            // Blueprint paper across entire window
            static const juce::Colour paperColour(0xFFE8E4DD);
            g.setColour(paperColour);
            g.fillRect(bounds);
            drawGridLines(g, bounds.toFloat());
        }
        else if (displayMode == DisplayMode::HoloPsr)
        {
            // Dark holographic background across entire window
            g.setColour(juce::Colour(0xFF1A1A24));
            g.fillRect(bounds);
            drawHoloGridLines(g, bounds.toFloat());
        }
        else // Spectrogram
        {
            // Dark background across entire window
            g.setColour(juce::Colour(0xFF0A0A18));
            g.fillRect(bounds);
        }

        // ── Waveform display area ──
        auto waveArea = getLocalBounds();
        waveArea.removeFromTop(toolbarH);
        waveArea.removeFromBottom(statusBarH);
        auto waveRect = waveArea.toFloat().reduced(4.0f);

        if (displayMode == DisplayMode::Waveform)
            drawWaveformMode(g, waveRect);
        else if (displayMode == DisplayMode::Spectrogram)
            drawSpectrogramMode(g, waveRect);
        else
            drawHoloPsrMode(g, waveRect);

        // ── Blue fluorescent scan overlay during processing ──
        drawScanOverlay(g, waveRect);

        // ── Playhead line during preview ──
        drawPlayhead(g, waveRect);

        // ── Status bar text (no background, just text on paper) ──
        auto statusArea = getLocalBounds().removeFromBottom(statusBarH).toFloat();
        drawStatusBar(g, statusArea);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        auto waveArea = getWaveformArea();
        if (waveArea.contains(e.getPosition()) && audioData.getNumSamples() > 0)
        {
            // Cmd+Click or Right-click: select/deselect a specific channel (multi-channel only)
            if ((e.mods.isCommandDown() || e.mods.isPopupMenu()) && fileNumChannels > 1)
            {
                float relY = (static_cast<float>(e.y) - waveArea.toFloat().getY())
                             / waveArea.toFloat().getHeight();
                int ch = static_cast<int>(relY * static_cast<float>(fileNumChannels));
                ch = juce::jlimit(0, fileNumChannels - 1, ch);

                // Toggle: click same channel again to deselect
                selectedChannel = (selectedChannel == ch) ? -1 : ch;
                repaint();
                return;
            }

            // Normal click: seek playback position
            float relX = (static_cast<float>(e.x) - waveArea.toFloat().getX())
                         / waveArea.toFloat().getWidth();
            relX = juce::jlimit(0.0f, 1.0f, relX);

            int newPos = static_cast<int>(relX * static_cast<float>(audioData.getNumSamples()));
            previewSource.setPosition(newPos);
            playheadVisible = true;
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        // Drag on waveform area to scrub playback position
        auto waveArea = getWaveformArea();
        if (audioData.getNumSamples() > 0)
        {
            float relX = (static_cast<float>(e.x) - waveArea.toFloat().getX())
                         / waveArea.toFloat().getWidth();
            relX = juce::jlimit(0.0f, 1.0f, relX);

            int newPos = static_cast<int>(relX * static_cast<float>(audioData.getNumSamples()));
            previewSource.setPosition(newPos);
            repaint();
        }
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        // Spacebar toggles play/pause
        if (key == juce::KeyPress::spaceKey && audioData.getNumSamples() > 0)
        {
            togglePlayback();
            return true;
        }
        return false;
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // ── Row 1: Toolbar buttons (top 40px) ──
        auto toolbar = bounds.removeFromTop(40).reduced(8, 4);
        int btnW = 72;

        importBtn.setBounds(toolbar.removeFromLeft(btnW).reduced(2, 0));
        toolbar.removeFromLeft(4);
        processBtn.setBounds(toolbar.removeFromLeft(btnW).reduced(2, 0));
        toolbar.removeFromLeft(4);
        exportBtn.setBounds(toolbar.removeFromLeft(btnW).reduced(2, 0));
        toolbar.removeFromLeft(4);
        resetBtn.setBounds(toolbar.removeFromLeft(btnW).reduced(2, 0));

        // Toggles on the right side of row 1
        auto fullBounds = getLocalBounds();
        int toggleW = 80;
        int toggleY = 8;
        spectrogramToggle.setBounds(fullBounds.getRight() - toggleW - 8, toggleY, toggleW, 24);
        holoPsrToggle.setBounds(fullBounds.getRight() - toggleW * 2 - 12, toggleY, toggleW, 24);
        waveformToggle.setBounds(fullBounds.getRight() - toggleW * 3 - 16, toggleY, toggleW, 24);

        // Room tone duration combo
        roomToneDurationBox.setBounds(fullBounds.getRight() - toggleW * 3 - 16 - 90, toggleY, 80, 24);

        // ── Row 2: Denoise slider + Preview controls (next 32px) ──
        auto row2 = bounds.removeFromTop(32).reduced(8, 2);

        // Preview controls on the left
        playBtn.setBounds(row2.removeFromLeft(60).reduced(2, 0));
        row2.removeFromLeft(4);
        previewToggle.setBounds(row2.removeFromLeft(100).reduced(2, 0));
        row2.removeFromLeft(12);

        // Denoise strength slider (remaining space, with label offset)
        int labelW = 60;
        row2.removeFromLeft(labelW);  // space for attached label
        denoiseStrengthSlider.setBounds(row2);
    }

    //==========================================================================
    // Update toolbar component colours based on current display mode
    // Dark modes (HoloPsr, Spectrogram) need light text/tick colours
    //==========================================================================
    void updateToolbarColours()
    {
        bool isDark = (displayMode != DisplayMode::Waveform);
        auto textCol  = isDark ? juce::Colour(0xFFD5D3DE) : GoodMeterLookAndFeel::ink;
        auto textOnCol = isDark ? juce::Colour(0xFF1A1A24) : GoodMeterLookAndFeel::bgPanel;
        auto bgCol    = isDark ? juce::Colour(0xFF1A1A24) : GoodMeterLookAndFeel::bgPanel;

        // Set holo title bar flag for LookAndFeel (Holo-PSR mode only)
        GoodMeterLookAndFeel::holoTitleBar = (displayMode == DisplayMode::HoloPsr);
        GoodMeterLookAndFeel::spectroTitleBar = (displayMode == DisplayMode::Spectrogram);
        GoodMeterLookAndFeel::spectroProcessed = isProcessed;

        // Trigger title bar repaint on the parent DialogWindow
        if (auto* dlg = findParentComponentOfClass<juce::DialogWindow>())
            dlg->repaint();

        // TextButtons
        for (auto* btn : { &importBtn, &processBtn, &exportBtn, &resetBtn, &playBtn })
        {
            btn->setColour(juce::TextButton::textColourOffId, textCol);
            btn->setColour(juce::TextButton::textColourOnId, textOnCol);
        }

        // ToggleButtons
        for (auto* tog : { &waveformToggle, &holoPsrToggle, &spectrogramToggle, &previewToggle })
        {
            tog->setColour(juce::ToggleButton::textColourId, textCol);
            tog->setColour(juce::ToggleButton::tickColourId, textCol);
        }

        // ComboBox
        roomToneDurationBox.setColour(juce::ComboBox::textColourId, textCol);
        roomToneDurationBox.setColour(juce::ComboBox::arrowColourId, textCol);
        roomToneDurationBox.setColour(juce::ComboBox::backgroundColourId, bgCol);

        // Label
        denoiseStrengthLabel.setColour(juce::Label::textColourId, textCol);

        // Slider
        denoiseStrengthSlider.setColour(juce::Slider::thumbColourId, textCol);
        denoiseStrengthSlider.setColour(juce::Slider::trackColourId, textCol.withAlpha(0.2f));
        denoiseStrengthSlider.setColour(juce::Slider::textBoxTextColourId, textCol);
        denoiseStrengthSlider.setColour(juce::Slider::textBoxOutlineColourId, textCol.withAlpha(0.3f));
        denoiseStrengthSlider.setColour(juce::Slider::textBoxBackgroundColourId,
                                         juce::Colours::transparentBlack);
    }

private:
    //==========================================================================
    // Layout constants
    //==========================================================================
    static constexpr int toolbarH = 72;   // two rows: buttons(40) + slider/preview(32)
    static constexpr int statusBarH = 30;

    //==========================================================================
    // UI components
    //==========================================================================
    juce::TextButton importBtn  { "Import" };
    juce::TextButton processBtn { "Process" };
    juce::TextButton exportBtn  { "Export" };
    juce::TextButton resetBtn   { "Reset" };
    juce::ToggleButton waveformToggle    { "Waveform" };
    juce::ToggleButton holoPsrToggle     { "Holo-PSR" };
    juce::ToggleButton spectrogramToggle { "Spectro" };
    juce::ComboBox roomToneDurationBox;

    // Denoise strength slider
    juce::Slider denoiseStrengthSlider;
    juce::Label  denoiseStrengthLabel { {}, "Denoise" };

    // Preview transport
    juce::TextButton playBtn { "Play" };
    juce::ToggleButton previewToggle { "Processed" };
    bool previewProcessed = false;

    std::unique_ptr<juce::FileChooser> fileChooser;

    //==========================================================================
    // Display mode
    //==========================================================================
    enum class DisplayMode { Waveform, HoloPsr, Spectrogram };
    DisplayMode displayMode = DisplayMode::HoloPsr;

    //==========================================================================
    // Audio data
    //==========================================================================
    juce::AudioFormatManager formatManager;
    juce::AudioBuffer<float> audioData;
    juce::AudioThumbnailCache thumbCache { 5 };
    juce::AudioThumbnail thumbnail;

    // Pre-computed peaks for Holo-PSR (paint() reads only this small array)
    std::vector<float> holoPsrPeaks;
    static constexpr int maxPeakColumns = 512;

    // Processing results
    juce::AudioBuffer<float> denoisedData;   // Phase 2
    juce::AudioBuffer<float> roomToneData;   // synthesized room tone

    // File info
    juce::File sourceFile;
    double fileSampleRate = 0;
    int fileNumChannels = 0;
    int64_t fileLengthSamples = 0;

    // Processing state
    std::atomic<float> processingProgress { 0.0f };
    std::thread processingThread;
    std::thread exportThread;
    bool isProcessed = false;
    bool hasProcessedOnce = false;   // true after first successful full process
    int  selectedChannel = -1;       // -1 = all channels, 0..N-1 = single channel

    // Pre-computed spectrogram image (STFT energy heatmap, computed on import)
    juce::Image spectrogramImage;
    static constexpr int spectroFFTOrder = 10;      // 2^10 = 1024
    static constexpr int spectroFFTSize  = 1024;
    static constexpr int spectroHalfFFT  = 512;
    static constexpr int spectroHopSize  = 256;     // 75% overlap

    // Cached magnitude data for fast re-render (avoid redoing FFT)
    std::vector<std::array<float, 512>> spectroMagnitudes;
    float spectroLogMax = 0.0f;
    int spectroImgW = 0;

    // Export directory (passed from StandaloneNonoEditor via getRecordingDirectory)
    juce::File exportDirectory;

    // DeepFilterNet3 noise reduction engine
    DeepFilterProcessor deepFilter;
    bool deepFilterReady = false;

    /** Try to find and load DeepFilterNet3 ONNX models */
    void initDeepFilter()
    {
        // Search for models relative to the executable or project directory
        auto exe = juce::File::getSpecialLocation(
            juce::File::currentExecutableFile);

        // Try several paths: app bundle Resources, ThirdParty dev path
        std::vector<juce::File> searchPaths = {
            exe.getParentDirectory().getParentDirectory()
                .getChildFile("Resources/DeepFilterNet3_onnx"),
            exe.getParentDirectory().getParentDirectory()
                .getChildFile("Frameworks/DeepFilterNet3_onnx"),
            juce::File("/Users/MediaStorm/Desktop/GOODMETER/ThirdParty/DeepFilterNet3_onnx")
        };

        for (const auto& dir : searchPaths)
        {
            if (dir.getChildFile("enc.onnx").existsAsFile())
            {
                deepFilterReady = deepFilter.initialize(dir);
                if (deepFilterReady)
                {
                    DBG("DeepFilterNet3 loaded from: " << dir.getFullPathName());
                    return;
                }
            }
        }
        DBG("DeepFilterNet3 models not found — denoising disabled");
    }

    //==========================================================================
    // File import
    //==========================================================================
    void importAudioFile()
    {
        fileChooser = std::make_unique<juce::FileChooser>(
            "Import Audio", juce::File{}, "*.wav,*.aiff,*.flac,*.mp3,*.m4a,*.ogg,*.WAV,*.AIFF,*.FLAC,*.MP3");

        fileChooser->launchAsync(
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this](const juce::FileChooser& fc)
            {
                auto file = fc.getResult();
                if (file == juce::File{}) return;

                std::unique_ptr<juce::AudioFormatReader> reader(
                    formatManager.createReaderFor(file));
                if (!reader) return;

                sourceFile = file;
                fileSampleRate = reader->sampleRate;
                fileNumChannels = static_cast<int>(reader->numChannels);
                fileLengthSamples = reader->lengthInSamples;

                // Load entire file into memory (offline processing)
                audioData.setSize(fileNumChannels,
                    static_cast<int>(fileLengthSamples));
                reader->read(&audioData, 0,
                    static_cast<int>(fileLengthSamples), 0, true, true);

                // Update thumbnail for Waterfall mode
                thumbnail.clear();
                thumbnail.reset(fileNumChannels, fileSampleRate, fileLengthSamples);
                thumbnail.addBlock(0, audioData, 0,
                    static_cast<int>(fileLengthSamples));

                // Pre-compute peaks for Holo-PSR mode (avoids paint() scanning raw data)
                precomputeHoloPeaks();

                // Pre-compute spectrogram image (avoids FFT in paint())
                precomputeSpectrogram();

                // Enable processing
                processBtn.setEnabled(true);
                exportBtn.setEnabled(false);
                resetBtn.setEnabled(true);
                playBtn.setEnabled(true);       // can preview original
                previewToggle.setEnabled(false); // no processed data yet
                previewToggle.setToggleState(false, juce::dontSendNotification);
                isProcessed = false;
                hasProcessedOnce = false;
                selectedChannel = -1;
                denoisedData.setSize(0, 0);
                roomToneData.setSize(0, 0);

                repaint();
            });
    }

    /** Pre-compute downsampled peak values on import — paint() only reads this.
        Takes max absolute value across ALL channels so multi-channel files display correctly. */
    void precomputeHoloPeaks()
    {
        int totalSamples = audioData.getNumSamples();
        int numCh = audioData.getNumChannels();
        if (totalSamples == 0 || numCh == 0) { holoPsrPeaks.clear(); return; }

        int numCols = juce::jmin(maxPeakColumns, totalSamples);
        holoPsrPeaks.resize(static_cast<size_t>(numCols));

        int samplesPerCol = totalSamples / numCols;

        for (int c = 0; c < numCols; ++c)
        {
            int startSample = c * samplesPerCol;
            int endSample = juce::jmin(startSample + samplesPerCol, totalSamples);

            float peak = 0.0f;
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float* data = audioData.getReadPointer(ch);
                for (int s = startSample; s < endSample; ++s)
                    peak = juce::jmax(peak, std::abs(data[s]));
            }

            holoPsrPeaks[static_cast<size_t>(c)] = peak;
        }
    }

    //==========================================================================
    // Waveform mode — AudioThumbnail waveform on blueprint paper
    //==========================================================================
    void drawWaveformMode(juce::Graphics& g, juce::Rectangle<float> area)
    {
        // Paper background already drawn by paint() globally — just draw a subtle border
        g.setColour(juce::Colour(0x30000000));
        g.drawRect(area, 1.0f);

        int numCh = thumbnail.getNumChannels();
        if (numCh == 0)
        {
            drawPlaceholderText(g, area);
            return;
        }

        double totalLen = thumbnail.getTotalLength();
        float chHeight = area.getHeight() / static_cast<float>(numCh);

        if (isProcessed && selectedChannel < 0)
        {
            // After full process (no solo): all channels in blue
            g.setColour(juce::Colour(scanBlue).withAlpha(0.6f));
            thumbnail.drawChannels(g, area.toNearestInt(), 0.0, totalLen, 1.0f);
        }
        else
        {
            // Draw each channel in its own colour
            // If processed + solo: only selected channel turns blue, others stay their colour
            for (int ch = 0; ch < numCh; ++ch)
            {
                auto chArea = area.withY(area.getY() + chHeight * static_cast<float>(ch))
                                  .withHeight(chHeight);

                bool thisChProcessed = isProcessed && (selectedChannel < 0 || selectedChannel == ch);

                if (thisChProcessed)
                    g.setColour(juce::Colour(scanBlue).withAlpha(0.6f));
                else
                    g.setColour(getChannelColour(ch).withAlpha(0.75f));

                thumbnail.drawChannel(g, chArea.toNearestInt(), 0.0, totalLen, ch, 1.0f);
            }
        }

        // ── Selected channel highlight (uses channel's own colour) ──
        if (selectedChannel >= 0 && selectedChannel < numCh)
        {
            auto selColour = getChannelColour(selectedChannel);
            auto chArea = area.withY(area.getY() + chHeight * static_cast<float>(selectedChannel))
                              .withHeight(chHeight);

            // Semi-transparent fill + border in channel's colour
            g.setColour(selColour.withAlpha(0.12f));
            g.fillRect(chArea);
            g.setColour(selColour.withAlpha(0.6f));
            g.drawRect(chArea, 1.5f);

            // Channel label
            g.setColour(selColour);
            g.setFont(juce::Font(juce::FontOptions(11.0f)).boldened());
            g.drawText("Ch " + juce::String(selectedChannel + 1) + " Solo",
                       chArea.reduced(6.0f, 2.0f),
                       juce::Justification::topLeft);
        }
    }

    /** Get the display colour for a given channel index */
    static juce::Colour getChannelColour(int ch)
    {
        static const juce::Colour colours[] = {
            juce::Colour(0xFF1A1A1A),  // near-black
            juce::Colour(0xFF8B2252),  // dark rose
            juce::Colour(0xFF2244AA),  // deep blue
            juce::Colour(0xFF228B44),  // forest green
            juce::Colour(0xFFCC6600),  // burnt orange
            juce::Colour(0xFF6633AA),  // purple
        };
        return colours[ch % 6];
    }

    /** Blueprint grid (fine + major lines) */
    void drawGridLines(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float gridSmall = 16.0f;
        float gridLarge = 80.0f;

        g.setColour(juce::Colour(0x0C000000));
        for (float x = area.getX(); x < area.getRight(); x += gridSmall)
            g.drawVerticalLine(static_cast<int>(x), area.getY(), area.getBottom());
        for (float y = area.getY(); y < area.getBottom(); y += gridSmall)
            g.drawHorizontalLine(static_cast<int>(y), area.getX(), area.getRight());

        g.setColour(juce::Colour(0x18000000));
        for (float x = area.getX(); x < area.getRight(); x += gridLarge)
            g.drawVerticalLine(static_cast<int>(x), area.getY(), area.getBottom());
        for (float y = area.getY(); y < area.getBottom(); y += gridLarge)
            g.drawHorizontalLine(static_cast<int>(y), area.getX(), area.getRight());

        // Border frame
        g.setColour(juce::Colour(0x30000000));
        g.drawRect(area, 1.0f);
    }

    //==========================================================================
    // Holographic grid lines — drawn across any area (for full-window bg)
    //==========================================================================
    void drawHoloGridLines(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float cellW = 3.0f;
        float cellH = 2.0f;

        g.setColour(juce::Colour(0xFFD5D3DE).withAlpha(0.06f));
        for (float y = area.getY(); y < area.getBottom(); y += cellH)
            g.drawHorizontalLine(static_cast<int>(y), area.getX(), area.getRight());
        for (float x = area.getX(); x < area.getRight(); x += cellW)
            g.drawVerticalLine(static_cast<int>(x), area.getY(), area.getBottom());
    }

    //==========================================================================
    // Holo-PSR mode — holographic grid squares (reads pre-computed peaks only)
    // Background + grid already drawn by paint() globally
    //==========================================================================
    void drawHoloPsrMode(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float cellW = 3.0f;
        float cellH = 2.0f;
        int numCols = static_cast<int>(area.getWidth() / cellW);
        float centerY = area.getCentreY();

        if (holoPsrPeaks.empty())
        {
            drawPlaceholderText(g, area);
            return;
        }

        // Read from pre-computed peaks — zero overhead
        int peakCount = static_cast<int>(holoPsrPeaks.size());
        float halfH = area.getHeight() / 2.0f;

        for (int c = 0; c < numCols; ++c)
        {
            int peakIdx = c * peakCount / numCols;
            if (peakIdx >= peakCount) peakIdx = peakCount - 1;

            float peak = holoPsrPeaks[static_cast<size_t>(peakIdx)];
            float barH = peak * halfH;
            float x = area.getX() + static_cast<float>(c) * cellW;

            int cellsUp = static_cast<int>(barH / cellH) + 1;

            for (int r = 0; r < cellsUp; ++r)
            {
                float alpha = 1.0f - (static_cast<float>(r)
                    / static_cast<float>(cellsUp + 1)) * 0.5f;

                // Upper half
                g.setColour(juce::Colour(0xFFD5D3DE).withAlpha(0.25f * alpha));
                g.fillRect(x, centerY - (static_cast<float>(r) + 1.0f) * cellH,
                           cellW - 0.5f, cellH - 0.5f);

                // Lower half (mirror)
                g.setColour(juce::Colour(0xFFD5D3DE).withAlpha(0.25f * alpha));
                g.fillRect(x, centerY + static_cast<float>(r) * cellH,
                           cellW - 0.5f, cellH - 0.5f);
            }

            // Top highlight (holographic scan line)
            g.setColour(juce::Colour(0xFFD5D3DE).withAlpha(0.7f));
            g.fillRect(x, centerY - static_cast<float>(cellsUp) * cellH,
                       cellW - 0.5f, 1.0f);
            g.fillRect(x, centerY + (static_cast<float>(cellsUp) - 1.0f) * cellH + cellH,
                       cellW - 0.5f, 1.0f);
        }

        // Center line
        g.setColour(juce::Colour(0xFFD5D3DE).withAlpha(0.3f));
        g.drawHorizontalLine(static_cast<int>(centerY), area.getX(), area.getRight());
    }

    //==========================================================================
    // Placeholder text when no audio loaded
    //==========================================================================
    void drawPlaceholderText(juce::Graphics& g, juce::Rectangle<float> area)
    {
        bool isDark = (displayMode != DisplayMode::Waveform);
        g.setColour(isDark
            ? juce::Colour(0xFFD5D3DE).withAlpha(0.3f)
            : GoodMeterLookAndFeel::textMuted);
        g.setFont(juce::Font(16.0f));
        g.drawText("Import an audio file to begin",
                   area, juce::Justification::centred);
    }

    //==========================================================================
    // Pre-compute spectrogram: FFT pass (on import) + render pass (recolour)
    //==========================================================================
    void precomputeSpectrogram()
    {
        int totalSamples = audioData.getNumSamples();
        if (totalSamples < spectroFFTSize)
        {
            spectrogramImage = {};
            spectroMagnitudes.clear();
            return;
        }

        // Mix to mono
        auto mono = RoomToneExtractor::mixToMono(audioData);
        const float* data = mono.getReadPointer(0);

        int numFrames = (totalSamples - spectroFFTSize) / spectroHopSize + 1;
        spectroImgW = juce::jmin(numFrames, 2048);  // cap width

        juce::dsp::FFT fft(spectroFFTOrder);
        juce::dsp::WindowingFunction<float> window(
            spectroFFTSize, juce::dsp::WindowingFunction<float>::hann);

        // FFT pass — compute magnitudes and store
        float globalMax = 1e-10f;
        spectroMagnitudes.resize(static_cast<size_t>(spectroImgW));

        for (int col = 0; col < spectroImgW; ++col)
        {
            int frameIdx = col * numFrames / spectroImgW;
            int pos = frameIdx * spectroHopSize;

            std::array<float, 2048> fftBuf = {};
            for (int i = 0; i < spectroFFTSize && (pos + i) < totalSamples; ++i)
                fftBuf[static_cast<size_t>(i)] = data[pos + i];

            window.multiplyWithWindowingTable(fftBuf.data(), spectroFFTSize);
            fft.performFrequencyOnlyForwardTransform(fftBuf.data());

            for (int bin = 0; bin < spectroHalfFFT; ++bin)
            {
                float mag = fftBuf[static_cast<size_t>(bin)];
                spectroMagnitudes[static_cast<size_t>(col)][static_cast<size_t>(bin)] = mag;
                if (mag > globalMax) globalMax = mag;
            }
        }

        spectroLogMax = std::log10(globalMax + 1e-10f);

        // Render with default blue colour scheme
        renderSpectrogramImage(false);
    }

    /** Render spectrogram image from cached magnitudes.
        useYellow = true: bright yellow scheme (after Process) */
    void renderSpectrogramImage(bool useYellow)
    {
        if (spectroMagnitudes.empty()) return;

        int imgW = spectroImgW;
        int imgH = spectroHalfFFT;

        spectrogramImage = juce::Image(juce::Image::ARGB, imgW, imgH, true);

        for (int col = 0; col < imgW; ++col)
        {
            for (int bin = 0; bin < imgH; ++bin)
            {
                float mag = spectroMagnitudes[static_cast<size_t>(col)][static_cast<size_t>(bin)];
                float logMag = std::log10(mag + 1e-10f);
                float norm = juce::jlimit(0.0f, 1.0f, (logMag - (spectroLogMax - 4.0f)) / 4.0f);

                juce::Colour c;
                if (useYellow)
                {
                    // Yellow scheme: dark → dark amber → bright yellow → white
                    if (norm < 0.3f)
                    {
                        float t = norm / 0.3f;
                        c = juce::Colour(0xFF0A0A04).interpolatedWith(juce::Colour(0xFF8B6600), t);
                    }
                    else if (norm < 0.6f)
                    {
                        float t = (norm - 0.3f) / 0.3f;
                        c = juce::Colour(0xFF8B6600).interpolatedWith(juce::Colour(0xFFFFD700), t);
                    }
                    else
                    {
                        float t = (norm - 0.6f) / 0.4f;
                        c = juce::Colour(0xFFFFD700).interpolatedWith(juce::Colour(0xFFFFF8E8), t);
                    }
                }
                else
                {
                    // Blue scheme: dark → blue → cyan → white
                    if (norm < 0.3f)
                    {
                        float t = norm / 0.3f;
                        c = juce::Colour(0xFF0A0A18).interpolatedWith(juce::Colour(0xFF2244AA), t);
                    }
                    else if (norm < 0.6f)
                    {
                        float t = (norm - 0.3f) / 0.3f;
                        c = juce::Colour(0xFF2244AA).interpolatedWith(juce::Colour(0xFF40C8E0), t);
                    }
                    else
                    {
                        float t = (norm - 0.6f) / 0.4f;
                        c = juce::Colour(0xFF40C8E0).interpolatedWith(juce::Colour(0xFFE8E4F0), t);
                    }
                }

                int y = imgH - 1 - bin;
                spectrogramImage.setPixelAt(col, y, c);
            }
        }
    }

    //==========================================================================
    // Spectrogram mode — pre-computed STFT energy heatmap
    //==========================================================================
    void drawSpectrogramMode(juce::Graphics& g, juce::Rectangle<float> area)
    {
        // Background already drawn by paint() globally

        if (spectrogramImage.isNull())
        {
            drawPlaceholderText(g, area);
            return;
        }

        // Draw the pre-computed spectrogram image, stretched to fill area
        g.drawImage(spectrogramImage, area,
                    juce::RectanglePlacement::stretchToFit);

        // Frequency scale labels (right side)
        if (fileSampleRate > 0)
        {
            g.setColour(juce::Colour(0xFFD5D3DE).withAlpha(0.5f));
            g.setFont(juce::Font(10.0f));

            float nyquist = static_cast<float>(fileSampleRate) / 2.0f;
            float freqs[] = { 100.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
            for (float freq : freqs)
            {
                if (freq > nyquist) continue;
                float binFrac = freq / nyquist;
                float y = area.getBottom() - binFrac * area.getHeight();

                // Horizontal guide line
                g.setColour(juce::Colour(0xFFD5D3DE).withAlpha(0.12f));
                g.drawHorizontalLine(static_cast<int>(y), area.getX(), area.getRight());

                // Label
                g.setColour(juce::Colour(0xFFD5D3DE).withAlpha(0.5f));
                juce::String label = (freq >= 1000.0f)
                    ? juce::String(static_cast<int>(freq / 1000.0f)) + "k"
                    : juce::String(static_cast<int>(freq));
                g.drawText(label, area.getRight() - 32.0f, y - 6.0f, 28.0f, 12.0f,
                           juce::Justification::centredRight);
            }
        }

        // Border
        g.setColour(juce::Colour(0xFFD5D3DE).withAlpha(0.15f));
        g.drawRect(area, 1.0f);
    }

    //==========================================================================
    // Waveform area helper (used by paint, mouseDown, playhead)
    //==========================================================================
    juce::Rectangle<int> getWaveformArea() const
    {
        auto area = getLocalBounds();
        area.removeFromTop(toolbarH);
        area.removeFromBottom(statusBarH);
        return area.reduced(4);
    }

    //==========================================================================
    // Playhead — vertical line showing current playback position
    //==========================================================================
    void drawPlayhead(juce::Graphics& g, juce::Rectangle<float> area)
    {
        if (audioData.getNumSamples() == 0) return;

        // Use audioData length directly (previewSource may not have buffers set yet)
        int pos = previewSource.getPosition();
        int total = audioData.getNumSamples();

        float frac = static_cast<float>(pos) / static_cast<float>(total);
        // Always show playhead if user has clicked (playheadVisible), or if playing
        if (!playheadVisible && !isPlaying) return;

        float x = area.getX() + area.getWidth() * frac;

        static constexpr juce::uint32 playheadOrange = 0xFFFF8C00;  // bright orange

        // Inverted triangle marker at the top
        float triW = 8.0f;
        float triH = 7.0f;
        juce::Path triangle;
        triangle.addTriangle(x - triW * 0.5f, area.getY(),
                             x + triW * 0.5f, area.getY(),
                             x,               area.getY() + triH);
        g.setColour(juce::Colour(playheadOrange));
        g.fillPath(triangle);

        // Playhead line (orange)
        g.setColour(juce::Colour(playheadOrange).withAlpha(0.9f));
        g.fillRect(x - 0.5f, area.getY() + triH, 1.5f, area.getHeight() - triH);

        // Timestamp label
        if (fileSampleRate > 0)
        {
            double timeSec = static_cast<double>(pos) / fileSampleRate;
            int mins = static_cast<int>(timeSec) / 60;
            int secs = static_cast<int>(timeSec) % 60;
            int ms   = static_cast<int>((timeSec - std::floor(timeSec)) * 1000.0);

            juce::String timeStr = juce::String(mins) + ":"
                + juce::String(secs).paddedLeft('0', 2) + "."
                + juce::String(ms).paddedLeft('0', 3);

            g.setColour(juce::Colour(playheadOrange));
            g.setFont(juce::Font(juce::FontOptions(10.0f)));
            float labelX = juce::jmin(x + 3.0f, area.getRight() - 48.0f);
            g.drawText(timeStr, labelX, area.getY() + triH + 2.0f, 46.0f, 12.0f,
                       juce::Justification::centredLeft);
        }
    }

    //==========================================================================
    // Blue fluorescent scan overlay — sweeps left→right during processing
    //==========================================================================
    static constexpr juce::uint32 scanBlue  = 0xFF4A9EFF;  // fluorescent blue
    static constexpr juce::uint32 scanAmber = 0xFFFFB840;  // warm amber for spectrogram

    void drawScanOverlay(juce::Graphics& g, juce::Rectangle<float> area)
    {
        float progress = processingProgress.load(std::memory_order_relaxed);
        if (progress <= 0.0f && !isProcessed) return;
        if (audioData.getNumSamples() == 0) return;

        // Use full progress when processing, or 1.0 when done
        float scanT = isProcessed ? 1.0f : progress;
        if (scanT <= 0.0f) return;

        // Pick scan colour based on display mode
        bool isSpectro = (displayMode == DisplayMode::Spectrogram);
        juce::uint32 scanColour = isSpectro ? scanAmber : scanBlue;

        float scanX = area.getX() + area.getWidth() * scanT;

        // Determine the scan region (full area or single channel strip)
        int numCh = fileNumChannels > 0 ? fileNumChannels : 1;
        float chHeight = area.getHeight() / static_cast<float>(numCh);
        bool soloScan = (selectedChannel >= 0 && selectedChannel < numCh
                         && displayMode == DisplayMode::Waveform);

        // Scan area: either just the selected channel strip, or full area
        auto scanArea = soloScan
            ? area.withY(area.getY() + chHeight * static_cast<float>(selectedChannel))
                  .withHeight(chHeight)
            : area;

        // ── Scanned region: tint overlay ──
        auto scannedRect = scanArea.withRight(scanX);
        g.setColour(juce::Colour(scanColour).withAlpha(0.06f));
        g.fillRect(scannedRect);

        // ── Re-draw scanned region in blue ──
        if (displayMode == DisplayMode::HoloPsr && !holoPsrPeaks.empty())
        {
            // Holo-PSR: draw ONLY the outline (top/bottom edge of each column)
            float cellW = 3.0f;
            float cellH = 2.0f;
            int numCols = static_cast<int>(area.getWidth() / cellW);
            float centerY = area.getCentreY();
            float halfH = area.getHeight() / 2.0f;
            int peakCount = static_cast<int>(holoPsrPeaks.size());

            int scanCol = static_cast<int>((scanX - area.getX()) / cellW);

            for (int c = 0; c < juce::jmin(scanCol, numCols); ++c)
            {
                int peakIdx = c * peakCount / numCols;
                if (peakIdx >= peakCount) peakIdx = peakCount - 1;

                float peak = holoPsrPeaks[static_cast<size_t>(peakIdx)];
                float barH = peak * halfH;
                float x = area.getX() + static_cast<float>(c) * cellW;
                int cellsUp = static_cast<int>(barH / cellH) + 1;

                // Only draw the top and bottom edge cells (outline)
                // Top edge (upper half)
                g.setColour(juce::Colour(scanBlue).withAlpha(0.9f));
                g.fillRect(x, centerY - static_cast<float>(cellsUp) * cellH,
                           cellW - 0.5f, cellH);
                // Bottom edge (lower half mirror)
                g.fillRect(x, centerY + (static_cast<float>(cellsUp) - 1.0f) * cellH,
                           cellW - 0.5f, cellH);

                // Thin glow on the 2nd cell from edge
                if (cellsUp > 1)
                {
                    g.setColour(juce::Colour(scanBlue).withAlpha(0.3f));
                    g.fillRect(x, centerY - (static_cast<float>(cellsUp) - 1.0f) * cellH,
                               cellW - 0.5f, cellH - 0.5f);
                    g.fillRect(x, centerY + (static_cast<float>(cellsUp) - 2.0f) * cellH,
                               cellW - 0.5f, cellH - 0.5f);
                }
            }
        }
        else if (displayMode == DisplayMode::Waveform && thumbnail.getNumChannels() > 0)
        {
            // Blue overlay on scanned portion of waveform — clipped to scanArea
            g.saveState();
            g.reduceClipRegion(scannedRect.toNearestInt());

            if (soloScan)
            {
                // Only draw blue on the selected channel's strip
                g.setColour(juce::Colour(scanBlue).withAlpha(0.5f));
                thumbnail.drawChannel(g, scanArea.toNearestInt(),
                    0.0, thumbnail.getTotalLength(), selectedChannel, 1.0f);
            }
            else
            {
                g.setColour(juce::Colour(scanBlue).withAlpha(0.5f));
                thumbnail.drawChannels(g, area.toNearestInt(),
                    0.0, thumbnail.getTotalLength(), 1.0f);
            }
            g.restoreState();
        }
        else if (displayMode == DisplayMode::Spectrogram)
        {
            // Spectrogram is re-rendered in yellow after process — no overlay needed
        }

        // ── Scan line: bright vertical fluorescent edge ──
        if (scanT < 1.0f)
        {
            // Glow (wide, diffuse) — restricted to scanArea height
            float glowW = 12.0f;
            juce::ColourGradient glow(
                juce::Colour(scanColour).withAlpha(0.25f), scanX, scanArea.getCentreY(),
                juce::Colours::transparentBlack, scanX + glowW, scanArea.getCentreY(), false);
            glow.addColour(0.0, juce::Colour(scanColour).withAlpha(0.25f));
            g.setGradientFill(glow);
            g.fillRect(scanX - glowW * 0.5f, scanArea.getY(), glowW, scanArea.getHeight());

            // Core line (2px bright)
            g.setColour(juce::Colour(scanColour).withAlpha(0.9f));
            g.fillRect(scanX - 1.0f, scanArea.getY(), 2.0f, scanArea.getHeight());
        }
    }

    //==========================================================================
    // Status bar
    //==========================================================================
    void drawStatusBar(juce::Graphics& g, juce::Rectangle<float> area)
    {
        bool isDark = (displayMode != DisplayMode::Waveform);
        auto textCol = isDark ? juce::Colour(0xFFD5D3DE) : GoodMeterLookAndFeel::ink;

        g.setColour(textCol);
        g.setFont(juce::Font(12.0f));

        auto textArea = area.reduced(8.0f, 2.0f);

        if (sourceFile == juce::File{})
        {
            g.drawText("No file loaded", textArea, juce::Justification::centredLeft);
            return;
        }

        // File info
        juce::String info = sourceFile.getFileName();
        info += " | " + juce::String(static_cast<int>(fileSampleRate)) + " Hz";
        info += " | " + juce::String(fileNumChannels) + " ch";

        double durationSec = static_cast<double>(fileLengthSamples) / fileSampleRate;
        int mins = static_cast<int>(durationSec) / 60;
        int secs = static_cast<int>(durationSec) % 60;
        info += " | " + juce::String(mins) + ":" + juce::String(secs).paddedLeft('0', 2);

        g.drawText(info, textArea.removeFromLeft(textArea.getWidth() * 0.6f),
                   juce::Justification::centredLeft);

        // Progress bar
        float progress = processingProgress.load(std::memory_order_relaxed);
        if (progress > 0.0f && progress < 1.0f)
        {
            auto progressArea = textArea.reduced(2.0f, 6.0f);

            // Background
            g.setColour(textCol.withAlpha(0.1f));
            g.fillRect(progressArea);

            // Fill
            g.setColour(juce::Colour(scanBlue));
            g.fillRect(progressArea.withWidth(progressArea.getWidth() * progress));

            // Border
            g.setColour(textCol.withAlpha(0.3f));
            g.drawRect(progressArea, 1.0f);
        }
        else if (isProcessed)
        {
            g.setColour(juce::Colour(scanBlue));
            g.drawText("Ready to export", textArea,
                       juce::Justification::centredRight);
        }
    }

    //==========================================================================
    // Processing (background thread)
    //==========================================================================
    void processAudio()
    {
        if (audioData.getNumSamples() == 0) return;

        // Wait for previous thread
        if (processingThread.joinable())
            processingThread.join();

        bool singleChMode = (selectedChannel >= 0 && hasProcessedOnce && deepFilterReady);
        int targetCh = selectedChannel;  // capture for thread

        processBtn.setEnabled(false);
        exportBtn.setEnabled(false);
        processingProgress.store(0.0f);
        startTimerHz(20);

        if (singleChMode)
        {
            // ══════════════════════════════════════════════════
            // Single-channel fast reprocess (no scan animation)
            // ══════════════════════════════════════════════════
            // Keep isProcessed = true so scan overlay stays static (no animation)

            processingThread = std::thread([this, targetCh]()
            {
                juce::AudioBuffer<float> singleCh(1, audioData.getNumSamples());
                singleCh.copyFrom(0, 0, audioData, targetCh, 0, audioData.getNumSamples());

                std::atomic<float> dfProgress { 0.0f };
                std::atomic<bool> dfDone { false };
                juce::AudioBuffer<float> result;

                std::thread dfThread([&]()
                {
                    result = deepFilter.process(singleCh, fileSampleRate, dfProgress, 1.0f);
                    dfDone.store(true);
                });

                while (!dfDone.load())
                {
                    processingProgress.store(dfProgress.load());
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                dfThread.join();

                // Write back single processed channel to denoisedData
                if (result.getNumSamples() > 0 && targetCh < denoisedData.getNumChannels())
                {
                    int copyLen = juce::jmin(result.getNumSamples(), denoisedData.getNumSamples());
                    denoisedData.copyFrom(targetCh, 0, result, 0, 0, copyLen);
                }

                processingProgress.store(1.0f);

                auto flag = aliveFlag;
                juce::MessageManager::callAsync([this, flag]()
                {
                    if (!flag->load()) return;
                    processBtn.setEnabled(true);
                    exportBtn.setEnabled(true);
                    previewToggle.setEnabled(true);
                    isProcessed = true;
                    GoodMeterLookAndFeel::spectroProcessed = true;
                    stopTimer();
                    if (auto* dlg = findParentComponentOfClass<juce::DialogWindow>())
                        dlg->repaint();
                    repaint();
                });
            });
        }
        else
        {
            // ══════════════════════════════════════════════════
            // Full processing pipeline with scan animation
            // ══════════════════════════════════════════════════
            isProcessed = false;
            GoodMeterLookAndFeel::spectroProcessed = false;
            renderSpectrogramImage(false);  // restore blue spectrogram
            if (auto* dlg = findParentComponentOfClass<juce::DialogWindow>())
                dlg->repaint();
            repaint();

            float rtDuration = 30.0f;
            switch (roomToneDurationBox.getSelectedId())
            {
                case 1: rtDuration = 10.0f;  break;
                case 2: rtDuration = 30.0f;  break;
                case 3: rtDuration = 60.0f;  break;
                case 4: rtDuration = 120.0f; break;
            }

            int rtChannels = fileNumChannels;

            processingThread = std::thread([this, rtDuration, rtChannels]()
            {
                // 1. VAD: detect silent segments (10%)
                auto silentSegs = RoomToneExtractor::detectSilentSegments(
                    audioData, fileSampleRate);
                processingProgress.store(0.10f);

                // 2. Spectral envelope from silent segments (15%)
                auto envelope = RoomToneExtractor::extractSpectralEnvelope(
                    audioData, silentSegs, fileSampleRate);
                processingProgress.store(0.15f);

                // 2b. Measure actual noise floor RMS for calibration
                float noiseFloorRms = RoomToneExtractor::measureNoiseFloorRms(
                    audioData, silentSegs);

                // 3. Synthesize Room Tone (20%) — calibrated to real noise floor
                roomToneData = RoomToneExtractor::synthesizeRoomTone(
                    envelope, fileSampleRate, rtDuration, rtChannels, noiseFloorRms);
                processingProgress.store(0.20f);

                // 4. DeepFilterNet3 denoise (20% → 95%)
                if (deepFilterReady)
                {
                    std::atomic<float> dfProgress { 0.0f };
                    std::atomic<bool> dfDone { false };
                    std::thread dfThread([&]()
                    {
                        denoisedData = deepFilter.process(
                            audioData, fileSampleRate, dfProgress, 1.0f);
                        dfDone.store(true);
                    });

                    while (!dfDone.load())
                    {
                        float p = dfProgress.load();
                        processingProgress.store(0.20f + 0.75f * p);
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                    dfThread.join();
                    processingProgress.store(0.95f);
                }

                processingProgress.store(1.0f);

                auto flag = aliveFlag;
                juce::MessageManager::callAsync([this, flag]()
                {
                    if (!flag->load()) return;
                    processBtn.setEnabled(true);
                    exportBtn.setEnabled(true);
                    previewToggle.setEnabled(true);
                    isProcessed = true;
                    hasProcessedOnce = true;
                    GoodMeterLookAndFeel::spectroProcessed = true;
                    renderSpectrogramImage(true);  // re-render in yellow
                    stopTimer();
                    if (auto* dlg = findParentComponentOfClass<juce::DialogWindow>())
                        dlg->repaint();
                    repaint();
                });
            });
        }
    }

    //==========================================================================
    // Two-track export (Clean + Room Tone, controlled by export mode)
    //==========================================================================
    void exportThreeTracks()
    {
        if (!isProcessed) return;

        // Wait for previous export
        if (exportThread.joinable())
            exportThread.join();

        exportBtn.setEnabled(false);

        // Use configured export directory if available, otherwise source file's directory
        auto dir = exportDirectory.exists() ? exportDirectory : sourceFile.getParentDirectory();
        if (!dir.exists()) dir.createDirectory();

        auto baseName = sourceFile.getFileNameWithoutExtension();

        // Capture denoise strength for export blend
        float exportWetDry = static_cast<float>(denoiseStrengthSlider.getValue());
        int exportSoloCh = selectedChannel;  // capture for thread
        int expMode = AudioLabContent::exportMode;  // 1=Both, 2=Clean, 3=RoomTone

        // If solo channel selected, add channel suffix to filenames
        juce::String suffix = (exportSoloCh >= 0)
            ? "_ch" + juce::String(exportSoloCh + 1) : "";

        auto cleanFile    = dir.getChildFile(baseName + suffix + "_clean.wav");
        auto roomToneFile = dir.getChildFile(baseName + suffix + "_roomtone.wav");

        exportThread = std::thread([this, cleanFile, roomToneFile, dir,
                                    exportWetDry, exportSoloCh, expMode]()
        {
            // Helper: extract a single channel as mono buffer
            auto extractMono = [](const juce::AudioBuffer<float>& buf, int ch)
            {
                juce::AudioBuffer<float> mono(1, buf.getNumSamples());
                mono.copyFrom(0, 0, buf, ch, 0, buf.getNumSamples());
                return mono;
            };

            bool wantClean = (expMode == 1 || expMode == 2);
            bool wantRoomTone = (expMode == 1 || expMode == 3);

            if (exportSoloCh >= 0)
            {
                // ── Solo channel export: mono WAV files ──
                if (wantClean)
                {
                    if (denoisedData.getNumSamples() > 0
                        && exportSoloCh < denoisedData.getNumChannels())
                    {
                        auto origMono = extractMono(audioData, exportSoloCh);
                        auto procMono = extractMono(denoisedData, exportSoloCh);
                        auto blended = blendWetDry(origMono, procMono, exportWetDry);
                        writeWavFile(cleanFile, blended, fileSampleRate);
                    }
                    else
                    {
                        auto origMono = extractMono(audioData, exportSoloCh);
                        writeWavFile(cleanFile, origMono, fileSampleRate);
                    }
                }

                if (wantRoomTone && roomToneData.getNumSamples() > 0)
                {
                    int rtCh = juce::jmin(exportSoloCh, roomToneData.getNumChannels() - 1);
                    auto rtMono = extractMono(roomToneData, rtCh);
                    writeWavFile(roomToneFile, rtMono, fileSampleRate);
                }
            }
            else
            {
                // ── Normal multi-channel export ──
                if (wantClean)
                {
                    if (denoisedData.getNumSamples() > 0)
                    {
                        auto blended = blendWetDry(audioData, denoisedData, exportWetDry);
                        writeWavFile(cleanFile, blended, fileSampleRate);
                    }
                    else
                    {
                        writeWavFile(cleanFile, audioData, fileSampleRate);
                    }
                }

                if (wantRoomTone)
                {
                    writeWavFile(roomToneFile, roomToneData, fileSampleRate);
                }
            }

            auto flag = aliveFlag;
            juce::MessageManager::callAsync([this, flag, dir]()
            {
                if (!flag->load()) return;
                exportBtn.setEnabled(true);
                dir.revealToUser();
            });
        });
    }

    /** Blend original (dry) with denoised (wet) at given ratio */
    static juce::AudioBuffer<float> blendWetDry(
        const juce::AudioBuffer<float>& dry,
        const juce::AudioBuffer<float>& wet,
        float wetAmount)
    {
        int numCh = dry.getNumChannels();
        int numSamples = dry.getNumSamples();
        juce::AudioBuffer<float> result(numCh, numSamples);

        float w = juce::jlimit(0.0f, 1.0f, wetAmount);
        float d = 1.0f - w;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* dryPtr = dry.getReadPointer(ch);
            const float* wetPtr = wet.getReadPointer(ch);
            float* dst = result.getWritePointer(ch);

            int wetSamples = wet.getNumSamples();
            for (int s = 0; s < numSamples; ++s)
            {
                float wSample = (s < wetSamples) ? wetPtr[s] : 0.0f;
                dst[s] = wSample * w + dryPtr[s] * d;
            }
        }

        return result;
    }

    /** Write AudioBuffer to WAV file (24-bit) */
    static void writeWavFile(const juce::File& file,
                             const juce::AudioBuffer<float>& data, double sr)
    {
        juce::WavAudioFormat wav;
        auto fos = file.createOutputStream();
        if (!fos) return;

        std::unique_ptr<juce::AudioFormatWriter> writer(
            wav.createWriterFor(fos.release(), sr,
                static_cast<unsigned int>(data.getNumChannels()),
                24, {}, 0));

        if (writer)
            writer->writeFromAudioSampleBuffer(data, 0, data.getNumSamples());
    }

    //==========================================================================
    // Reset — clear all loaded data and return to initial state
    //==========================================================================
    void resetAll()
    {
        stopTimer();
        stopPlayback();

        // Wait for background threads
        if (processingThread.joinable())
            processingThread.join();
        if (exportThread.joinable())
            exportThread.join();

        // Clear audio data
        audioData.setSize(0, 0);
        denoisedData.setSize(0, 0);
        roomToneData.setSize(0, 0);
        holoPsrPeaks.clear();
        spectrogramImage = {};
        spectroMagnitudes.clear();
        thumbnail.clear();

        // Reset file info
        sourceFile = juce::File();
        fileSampleRate = 0;
        fileNumChannels = 0;
        fileLengthSamples = 0;

        // Reset state
        processingProgress.store(0.0f);
        isProcessed = false;
        hasProcessedOnce = false;
        selectedChannel = -1;
        GoodMeterLookAndFeel::spectroProcessed = false;

        // Reset buttons
        processBtn.setEnabled(false);
        exportBtn.setEnabled(false);
        resetBtn.setEnabled(false);
        playBtn.setEnabled(false);
        previewToggle.setEnabled(false);
        previewToggle.setToggleState(false, juce::dontSendNotification);
        previewProcessed = false;
        playheadVisible = false;

        repaint();
    }

    //==========================================================================
    // Timer callback — refresh progress bar during processing
    //==========================================================================
    void timerCallback() override
    {
        // Check if preview playback finished
        if (isPlaying && previewSource.isFinished())
        {
            stopPlayback();
        }

        repaint();
    }

    //==========================================================================
    // Preview audio source — reads from audioData or denoisedData
    //==========================================================================
    class PreviewAudioSource : public juce::AudioSource
    {
    public:
        void setBuffers(const juce::AudioBuffer<float>* original,
                        const juce::AudioBuffer<float>* processed,
                        double sr, float blend = 1.0f)
        {
            const juce::SpinLock::ScopedLockType lock(bufferLock);
            origBuf = original;
            procBuf = processed;
            sampleRate = sr;
            wetDry = blend;
            // NOTE: do NOT reset position here — preserve user's seek position
        }

        void setPreviewProcessed(bool useProcessed) { showProcessed = useProcessed; }
        void setWetDry(float wd) { wetDry = wd; }
        void setSoloChannel(int ch) { soloChannel = ch; }  // -1 = all, 0..N-1 = mono solo
        void setPosition(int pos) { position.store(pos); }
        int getPosition() const { return position.load(); }
        int getTotalSamples() const
        {
            if (origBuf) return origBuf->getNumSamples();
            return 0;
        }
        bool isFinished() const { return finished.load(); }

        void prepareToPlay(int /*samplesPerBlockExpected*/, double /*sr*/) override
        {
            finished.store(false);
        }

        void releaseResources() override {}

        void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override
        {
            const juce::SpinLock::ScopedLockType lock(bufferLock);
            bufferToFill.clearActiveBufferRegion();

            if (!origBuf || origBuf->getNumSamples() == 0)
            {
                finished.store(true);
                return;
            }

            int srcSamples = origBuf->getNumSamples();
            int srcChannels = origBuf->getNumChannels();
            int outChannels = bufferToFill.buffer->getNumChannels();
            int numToCopy = bufferToFill.numSamples;

            int pos = position.load();
            if (pos >= srcSamples)
            {
                finished.store(true);
                return;
            }

            int available = juce::jmin(numToCopy, srcSamples - pos);
            bool useProc = showProcessed && procBuf && procBuf->getNumSamples() > 0;

            // ── Solo channel mode: play only one channel as mono (same to L+R) ──
            if (soloChannel >= 0 && soloChannel < srcChannels)
            {
                // Read the solo channel (with wet/dry blend if processed)
                for (int ch = 0; ch < outChannels; ++ch)
                {
                    float* dst = bufferToFill.buffer->getWritePointer(ch)
                                 + bufferToFill.startSample;

                    if (useProc && soloChannel < procBuf->getNumChannels())
                    {
                        const float* orig = origBuf->getReadPointer(soloChannel);
                        const float* proc = procBuf->getReadPointer(soloChannel);
                        float w = wetDry, d = 1.0f - w;
                        for (int s = 0; s < available; ++s)
                            dst[s] = proc[pos + s] * w + orig[pos + s] * d;
                    }
                    else
                    {
                        const float* src = origBuf->getReadPointer(soloChannel);
                        for (int s = 0; s < available; ++s)
                            dst[s] = src[pos + s];
                    }
                }
            }
            else
            {
                // ── Normal multi-channel playback ──
                for (int ch = 0; ch < outChannels; ++ch)
                {
                    float* dst = bufferToFill.buffer->getWritePointer(ch)
                                 + bufferToFill.startSample;

                    if (srcChannels <= outChannels)
                    {
                        int srcCh = juce::jmin(ch, srcChannels - 1);
                        if (useProc)
                        {
                            const float* orig = origBuf->getReadPointer(srcCh);
                            const float* proc = procBuf->getReadPointer(
                                juce::jmin(srcCh, procBuf->getNumChannels() - 1));
                            float w = wetDry, d = 1.0f - w;
                            for (int s = 0; s < available; ++s)
                                dst[s] = proc[pos + s] * w + orig[pos + s] * d;
                        }
                        else
                        {
                            bufferToFill.buffer->copyFrom(
                                ch, bufferToFill.startSample,
                                *origBuf, srcCh, pos, available);
                        }
                    }
                    else
                    {
                        std::memset(dst, 0, static_cast<size_t>(available) * sizeof(float));
                        float gain = 1.0f / static_cast<float>(
                            (srcChannels + outChannels - 1) / outChannels);

                        for (int sc = ch; sc < srcChannels; sc += outChannels)
                        {
                            if (useProc)
                            {
                                const float* orig = origBuf->getReadPointer(sc);
                                const float* proc = procBuf->getReadPointer(
                                    juce::jmin(sc, procBuf->getNumChannels() - 1));
                                float w = wetDry, d = 1.0f - w;
                                for (int s = 0; s < available; ++s)
                                    dst[s] += (proc[pos + s] * w + orig[pos + s] * d) * gain;
                            }
                            else
                            {
                                const float* src = origBuf->getReadPointer(sc);
                                for (int s = 0; s < available; ++s)
                                    dst[s] += src[pos + s] * gain;
                            }
                        }
                    }
                }
            }

            position.store(pos + available);

            if (pos + available >= srcSamples)
                finished.store(true);
        }

    private:
        const juce::AudioBuffer<float>* origBuf = nullptr;
        const juce::AudioBuffer<float>* procBuf = nullptr;
        double sampleRate = 48000.0;
        std::atomic<int> position { 0 };
        float wetDry = 1.0f;
        bool showProcessed = false;
        int soloChannel = -1;   // -1 = all channels, >= 0 = mono solo
        std::atomic<bool> finished { false };
        juce::SpinLock bufferLock;  // protects origBuf/procBuf against concurrent access
    };

    PreviewAudioSource previewSource;
    juce::AudioDeviceManager* deviceMgr = nullptr;                       // shared or owned
    std::unique_ptr<juce::AudioDeviceManager> ownDeviceManager;          // fallback if no shared
    juce::AudioSourcePlayer audioSourcePlayer;
    std::shared_ptr<std::atomic<bool>> aliveFlag = std::make_shared<std::atomic<bool>>(true);
    bool isPlaying = false;
    bool playheadVisible = false;  // becomes true after first click on waveform

    //==========================================================================
    // Playback control
    //==========================================================================
    void togglePlayback()
    {
        if (isPlaying)
            stopPlayback();
        else
            startPlayback();
    }

    void startPlayback()
    {
        if (audioData.getNumSamples() == 0) return;

        float blend = static_cast<float>(denoiseStrengthSlider.getValue());
        previewSource.setBuffers(&audioData,
            isProcessed ? &denoisedData : nullptr,
            fileSampleRate, blend);
        previewSource.setPreviewProcessed(previewProcessed);
        previewSource.setSoloChannel(selectedChannel);

        deviceMgr->addAudioCallback(&audioSourcePlayer);
        isPlaying = true;
        playheadVisible = true;
        playBtn.setButtonText("Stop");

        // Start polling for playback-finished + playhead redraw
        if (!isTimerRunning())
            startTimerHz(20);
    }

    void stopPlayback()
    {
        deviceMgr->removeAudioCallback(&audioSourcePlayer);
        isPlaying = false;
        playBtn.setButtonText("Play");
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioLabContent)
};
