/*
  ==============================================================================
    AudioDoctorComponent.h
    GOODMETER standalone Audio Doctor - A/B audio + AU/VST3 figure lab.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include "GoodMeterLookAndFeel.h"
#include "AudioDoctorPluginHost.h"
#include "AudioDoctorFigureRenderer.h"

class AudioDoctorContent : public juce::Component
{
public:
    enum class PluginSlot
    {
        A,
        B,
        C
    };

    enum class SourceSlot
    {
        dryA,
        dryB,
        dryC,
        wetA,
        wetB,
        wetC
    };

    explicit AudioDoctorContent(const juce::File& exportDir = {})
        : exportDirectory(exportDir.exists() ? exportDir : juce::File::getSpecialLocation(juce::File::userDocumentsDirectory))
    {
        importDryBtn.onClick = [this] { showLoadDryMenu(); };
        generateBtn.onClick = [this] { showGenerateMenu(); };
        editAudioBtn.onClick = [this] { showEditAudioMenu(); };
        busBtn.onClick = [this] { openBusRoutingWindow(); };
        pluginBtn.onClick = [this] { choosePlugin(PluginSlot::A); };
        editPluginBtn.onClick = [this] { showPluginEditor(PluginSlot::A); };
        renderBtn.onClick = [this] { renderWetWithPlugin(PluginSlot::A); };
        pluginBBtn.onClick = [this] { choosePlugin(PluginSlot::B); };
        editPluginBBtn.onClick = [this] { showPluginEditor(PluginSlot::B); };
        renderBBtn.onClick = [this] { renderWetWithPlugin(PluginSlot::B); };
        pluginCBtn.onClick = [this] { choosePlugin(PluginSlot::C); };
        editPluginCBtn.onClick = [this] { showPluginEditor(PluginSlot::C); };
        renderCBtn.onClick = [this] { renderWetWithPlugin(PluginSlot::C); };
        exportBtn.onClick = [this] { exportFigure(); };
        resetBtn.onClick = [this] { resetAll(); };

        for (auto* button : { &importDryBtn, &generateBtn, &editAudioBtn, &busBtn, &pluginBtn, &editPluginBtn,
                              &renderBtn, &pluginBBtn, &editPluginBBtn, &renderBBtn,
                              &pluginCBtn, &editPluginCBtn, &renderCBtn,
                              &exportBtn, &resetBtn })
        {
            addAndMakeVisible(button);
            GoodMeterLookAndFeel::markAsIOSEnglishMono(*button);
            button->setColour(juce::TextButton::textColourOffId, uiText());
            button->setColour(juce::TextButton::textColourOnId, juce::Colour(0xFF080A0F));
        }

        viewMode.addItem("Spectrum", 1);
        viewMode.addItem("Envelope", 2);
        viewMode.addItem("Group Delay", 3);
        viewMode.addItem("Spectrogram A/B/C", 4);
        viewMode.addItem("Reverb / Space", 5);
        viewMode.addItem("Dynamics", 6);
        viewMode.setSelectedId(1, juce::dontSendNotification);
        viewMode.onChange = [this] { repaint(); };
        GoodMeterLookAndFeel::markAsIOSEnglishMono(viewMode);
        viewMode.setLookAndFeel(&audioDoctorPopupLookAndFeel);
        addAndMakeVisible(viewMode);

        freqZoom.addItem("20-20k", 1);
        freqZoom.addItem("20-200", 2);
        freqZoom.addItem("100-1k", 3);
        freqZoom.addItem("250-4k", 4);
        freqZoom.addItem("1k-8k", 5);
        freqZoom.addItem("5k-20k", 6);
        freqZoom.setSelectedId(1, juce::dontSendNotification);
        freqZoom.onChange = [this] { applyFrequencyPreset(freqZoom.getSelectedId()); };
        GoodMeterLookAndFeel::markAsIOSEnglishMono(freqZoom);
        freqZoom.setLookAndFeel(&audioDoctorPopupLookAndFeel);
        addAndMakeVisible(freqZoom);

        themeMode.addItem("Dark", 1);
        themeMode.addItem("Light", 2);
        themeMode.setSelectedId(1, juce::dontSendNotification);
        themeMode.onChange = [this] { refreshThemeColours(); repaint(); };
        GoodMeterLookAndFeel::markAsIOSEnglishMono(themeMode);
        themeMode.setLookAndFeel(&audioDoctorPopupLookAndFeel);
        addAndMakeVisible(themeMode);

        statusLabel.setJustificationType(juce::Justification::centredLeft);
        statusLabel.setText("Load Dry audio, generate a signal, or choose an AU/VST3 plugin.", juce::dontSendNotification);
        addAndMakeVisible(statusLabel);

        pluginSlotLabel.setJustificationType(juce::Justification::centredRight);
        pluginSlotLabel.setText("Plugins: none", juce::dontSendNotification);
        addAndMakeVisible(pluginSlotLabel);

        editPluginBtn.setEnabled(false);
        renderBtn.setEnabled(false);
        editPluginBBtn.setEnabled(false);
        renderBBtn.setEnabled(false);
        editPluginCBtn.setEnabled(false);
        renderCBtn.setEnabled(false);
        editAudioBtn.setEnabled(false);
        exportBtn.setEnabled(false);
        resetBtn.setEnabled(false);
        busBtn.setEnabled(false);

        setWantsKeyboardFocus(true);
        lastAudioDirectory = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
        lastPluginDirectory = juce::File("/Library/Audio/Plug-Ins/VST3");
        if (!lastPluginDirectory.exists())
            lastPluginDirectory = juce::File::getSpecialLocation(juce::File::userHomeDirectory);

        refreshThemeColours();
        setSize(1080, 820);
    }

    ~AudioDoctorContent() override
    {
        themeMode.setLookAndFeel(nullptr);
        freqZoom.setLookAndFeel(nullptr);
        viewMode.setLookAndFeel(nullptr);
        closePluginEditorWindow();
        audioEditWindow.reset();
        busRoutingWindow.reset();
        generateSignalWindow.reset();
        aliveFlag->store(false);
        if (renderThread.joinable())
            renderThread.join();
    }

    void paint(juce::Graphics& g) override
    {
        figureLightThemeFlag() = isLightThemeSelected();
        syncTitleBarTheme();
        drawAppBackground(g);

        auto chrome = getLocalBounds().reduced(contentPadding).toFloat();
        auto toolbarPlate = chrome.removeFromTop(toolbarHeight).reduced(0.0f, 1.0f);
        drawGlassPlate(g, toolbarPlate, GoodMeterLookAndFeel::accentBlue, 0.24f);

        chrome.removeFromTop(toolbarStatusGap);
        auto statusPlate = chrome.removeFromTop(statusHeight).reduced(0.0f, 1.0f);
        drawGlassPlate(g, statusPlate, GoodMeterLookAndFeel::accentCyan, 0.18f);

        auto figureBounds = getFigureBounds().toFloat();
        if (!figureBounds.isEmpty())
        {
            drawGlassPlate(g, figureBounds, GoodMeterLookAndFeel::accentBlue.interpolatedWith(GoodMeterLookAndFeel::accentPink, 0.16f), 0.30f);
            drawFigure(g, figureBounds.reduced(14.0f), false);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(contentPadding);
        auto toolbar = bounds.removeFromTop(toolbarHeight);
        auto row1 = toolbar.removeFromTop(34);
        auto row2 = toolbar.removeFromTop(34);

        importDryBtn.setBounds(row1.removeFromLeft(82).reduced(2));
        generateBtn.setBounds(row1.removeFromLeft(88).reduced(2));
        pluginBtn.setBounds(row1.removeFromLeft(86).reduced(2));
        editPluginBtn.setBounds(row1.removeFromLeft(68).reduced(2));
        renderBtn.setBounds(row1.removeFromLeft(82).reduced(2));
        pluginBBtn.setBounds(row1.removeFromLeft(86).reduced(2));
        editPluginBBtn.setBounds(row1.removeFromLeft(68).reduced(2));
        renderBBtn.setBounds(row1.removeFromLeft(82).reduced(2));
        pluginCBtn.setBounds(row1.removeFromLeft(86).reduced(2));
        editPluginCBtn.setBounds(row1.removeFromLeft(68).reduced(2));
        renderCBtn.setBounds(row1.removeFromLeft(82).reduced(2));

        editAudioBtn.setBounds(row2.removeFromLeft(68).reduced(2));
        busBtn.setBounds(row2.removeFromLeft(64).reduced(2));
        exportBtn.setBounds(row2.removeFromLeft(82).reduced(2));
        resetBtn.setBounds(row2.removeFromLeft(74).reduced(2));
        row2.removeFromLeft(8);
        viewMode.setBounds(row2.removeFromLeft(150).reduced(2));
        freqZoom.setBounds(row2.removeFromLeft(120).reduced(2));
        themeMode.setBounds(row2.removeFromLeft(112).reduced(2));

        bounds.removeFromTop(toolbarStatusGap);
        auto statusRow = bounds.removeFromTop(statusHeight);
        statusLabel.setBounds(statusRow.removeFromLeft(juce::roundToInt(statusRow.getWidth() * 0.58f)).reduced(12, 0));
        pluginSlotLabel.setBounds(statusRow);

    }

    void mouseDown(const juce::MouseEvent&) override
    {
        grabKeyboardFocus();
    }

    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        if (!event.mods.isShiftDown())
            return;

        const float delta = std::abs(wheel.deltaY) > 0.0001f ? wheel.deltaY : -wheel.deltaX;
        if (std::abs(delta) <= 0.0001f)
            return;

        panFrequencyRange(delta > 0.0f ? -0.18f : 0.18f);
    }

    bool keyPressed(const juce::KeyPress& key) override
    {
        const auto c = static_cast<juce::juce_wchar>(key.getTextCharacter());
        if (c == 'r' || c == 'R')
        {
            zoomFrequencyRange(1.32f);
            return true;
        }

        if (c == 't' || c == 'T')
        {
            zoomFrequencyRange(1.0f / 1.32f);
            return true;
        }

        return false;
    }

private:
    using Asset = goodmeter::audio_doctor::Asset;

    class AudioDoctorPopupLookAndFeel final : public GoodMeterLookAndFeel
    {
    public:
        void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                          int, int, int, int, juce::ComboBox& box) override
        {
            auto area = juce::Rectangle<float>(0.5f, 0.5f,
                                               static_cast<float>(width) - 1.0f,
                                               static_cast<float>(height) - 1.0f);
            const float radius = 14.0f;
            const bool light = figureLightThemeFlag();
            g.setColour(light ? juce::Colour(0xFFFFFFFF).withAlpha(isButtonDown ? 0.98f : 0.92f)
                              : juce::Colour(0xFF0B1017).withAlpha(isButtonDown ? 0.32f : 0.18f));
            g.fillRoundedRectangle(area, radius);
            g.setColour(light ? juce::Colour(0xFF1E2530).withAlpha(0.20f)
                              : juce::Colour(0xFFF6EEE3).withAlpha(0.12f));
            g.drawRoundedRectangle(area, radius, 0.95f);

            const auto textColour = light ? juce::Colour(0xFF17202D)
                                          : juce::Colour(0xFFF3EEE4).withAlpha(0.96f);
            g.setColour(textColour);
            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
            g.drawText(box.getText(), juce::Rectangle<int>(12, 0, width - 34, height),
                       juce::Justification::centredLeft, true);

            juce::Path arrow;
            const float cx = static_cast<float>(width) - 17.0f;
            const float cy = static_cast<float>(height) * 0.5f + 1.0f;
            arrow.startNewSubPath(cx - 5.0f, cy - 3.0f);
            arrow.lineTo(cx, cy + 3.0f);
            arrow.lineTo(cx + 5.0f, cy - 3.0f);
            g.setColour(textColour.withAlpha(0.88f));
            g.strokePath(arrow, juce::PathStrokeType(1.7f));
        }

        void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
        {
            const bool light = figureLightThemeFlag();
            auto area = juce::Rectangle<float>(0.0f, 0.0f,
                                               static_cast<float>(width),
                                               static_cast<float>(height));
            g.setColour(light ? juce::Colour(0xFFF5F7FA) : juce::Colour(0xFF07080B));
            g.fillRect(area);
            g.setColour(light ? juce::Colours::white.withAlpha(0.98f)
                              : juce::Colour(0xFF0B1017).withAlpha(0.92f));
            g.fillRoundedRectangle(area.reduced(0.5f), 12.0f);
            g.setColour(light ? juce::Colour(0xFF1E2530).withAlpha(0.16f)
                              : juce::Colour(0xFFF6EEE3).withAlpha(0.14f));
            g.drawRoundedRectangle(area.reduced(0.5f), 12.0f, 1.0f);
        }

        void drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                               bool isSeparator, bool isActive, bool isHighlighted,
                               bool isTicked, bool, const juce::String& text,
                               const juce::String&, const juce::Drawable*,
                               const juce::Colour*) override
        {
            if (isSeparator)
            {
                const bool light = figureLightThemeFlag();
                g.setColour(light ? juce::Colour(0xFF1E2530).withAlpha(0.14f)
                                  : juce::Colour(0xFFF6EEE3).withAlpha(0.13f));
                g.fillRect(area.getX() + 10, area.getCentreY(), area.getWidth() - 20, 1);
                return;
            }

            const bool light = figureLightThemeFlag();
            auto textColour = light
                ? (isActive ? juce::Colour(0xFF17202D) : juce::Colour(0xFF17202D).withAlpha(0.36f))
                : (isActive ? juce::Colour(0xFFF3EEE4).withAlpha(0.95f)
                            : juce::Colour(0xFFF3EEE4).withAlpha(0.38f));
            if (isHighlighted && isActive)
            {
                auto highlight = area.toFloat().reduced(5.0f, 2.0f);
                g.setColour(light ? juce::Colour(0xFF17202D).withAlpha(0.07f)
                                  : juce::Colour(0xFFF6EEE3).withAlpha(0.07f));
                g.fillRoundedRectangle(highlight, 8.0f);
            }

            if (isTicked)
            {
                g.setColour(GoodMeterLookAndFeel::accentBlue);
                g.fillEllipse(static_cast<float>(area.getX()) + 8.0f,
                              static_cast<float>(area.getCentreY()) - 2.5f,
                              5.0f, 5.0f);
            }

            g.setColour(textColour);
            g.setFont(juce::Font(juce::Font::getDefaultMonospacedFontName(), 13.0f, juce::Font::bold));
            g.drawText(text, area.reduced(22, 0), juce::Justification::centredLeft, true);
        }
    };

    class PluginEditorWindow final : public juce::DocumentWindow
    {
    public:
        PluginEditorWindow(const juce::String& pluginName,
                           std::unique_ptr<juce::AudioProcessorEditor> editorToOwn,
                           std::function<void()> closeCallback)
            : juce::DocumentWindow(pluginName,
                                   juce::Colour(0xFF20242B),
                                   juce::DocumentWindow::closeButton),
              onClose(std::move(closeCallback))
        {
            setUsingNativeTitleBar(true);
            setResizable(true, true);

            if (editorToOwn->getWidth() <= 0 || editorToOwn->getHeight() <= 0)
                editorToOwn->setSize(620, 360);

            const auto editorW = editorToOwn->getWidth();
            const auto editorH = editorToOwn->getHeight();
            setContentOwned(editorToOwn.release(), true);
            centreWithSize(editorW, editorH);
            setVisible(true);
            toFront(true);
        }

        void closeButtonPressed() override
        {
            if (onClose != nullptr)
                juce::MessageManager::callAsync(onClose);
        }

    private:
        std::function<void()> onClose;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginEditorWindow)
    };

    class AudioEditComponent final : public juce::Component,
                                     private juce::Timer
    {
    public:
        using CommitCallback = std::function<void(SourceSlot, const juce::var&)>;

        AudioEditComponent(SourceSlot sourceSlot, const Asset& sourceAsset, CommitCallback commitCallback)
            : slot(sourceSlot), asset(sourceAsset), onCommit(std::move(commitCallback))
        {
            trimEndSeconds = asset.metrics.durationSeconds > 0.0
                ? asset.metrics.durationSeconds
                : static_cast<double>(asset.buffer.getNumSamples()) / juce::jmax(1.0, asset.sampleRate);

            playButton.onClick = [this]
            {
                playing = !playing;
                playButton.setButtonText(playing ? "Pause" : "Play");
                lastTickMs = juce::Time::getMillisecondCounterHiRes();
                if (playing)
                    startTimerHz(30);
                else
                    stopTimer();
            };
            startButton.onClick = [this] { playheadSeconds = 0.0; repaint(); };
            trimStartButton.onClick = [this] { setTrimStartToPlayhead(); };
            trimEndButton.onClick = [this] { setTrimEndToPlayhead(); };
            fadeInButton.onClick = [this] { fadeInMs = 8.0; repaint(); };
            fadeOutButton.onClick = [this] { fadeOutMs = 35.0; repaint(); };
            sendButton.onClick = [this] { sendEdit(); };

            for (auto* button : { &playButton, &startButton, &trimStartButton, &trimEndButton,
                                  &fadeInButton, &fadeOutButton, &sendButton })
            {
                addAndMakeVisible(button);
                GoodMeterLookAndFeel::markAsIOSEnglishMono(*button);
            }

            setWantsKeyboardFocus(true);
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced(18);
            auto controls = bounds.removeFromBottom(42);
            playButton.setBounds(controls.removeFromLeft(72).reduced(3));
            startButton.setBounds(controls.removeFromLeft(72).reduced(3));
            trimStartButton.setBounds(controls.removeFromLeft(88).reduced(3));
            trimEndButton.setBounds(controls.removeFromLeft(88).reduced(3));
            fadeInButton.setBounds(controls.removeFromLeft(82).reduced(3));
            fadeOutButton.setBounds(controls.removeFromLeft(88).reduced(3));
            sendButton.setBounds(controls.removeFromRight(92).reduced(3));
            waveformBounds = bounds.reduced(0, 12).toFloat();
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xFF080B10));
            auto title = getLocalBounds().reduced(18).removeFromTop(34);
            g.setColour(juce::Colour(0xFFF3F6FA));
            g.setFont(juce::Font(18.0f, juce::Font::bold));
            g.drawText(sourceSlotLabel(slot) + " Edit  |  " + asset.name, title, juce::Justification::centredLeft, true);

            g.setColour(juce::Colour(0xFF101720));
            g.fillRoundedRectangle(waveformBounds, 12.0f);
            g.setColour(juce::Colours::white.withAlpha(0.10f));
            g.drawRoundedRectangle(waveformBounds.reduced(0.5f), 12.0f, 1.0f);

            drawWaveform(g, waveformBounds.reduced(14.0f, 10.0f));
            drawEditOverlays(g, waveformBounds.reduced(14.0f, 10.0f));

            auto info = getLocalBounds().reduced(18).withY(waveformBounds.getBottom() + 4.0f).withHeight(24.0f);
            g.setColour(juce::Colour(0xFFC9D2DE));
            g.setFont(juce::Font(13.0f));
            g.drawText("trim " + juce::String(trimStartSeconds, 3) + "s -> "
                       + juce::String(trimEndSeconds, 3) + "s | fade "
                       + juce::String(fadeInMs, 0) + " / " + juce::String(fadeOutMs, 0)
                       + " ms | A trim head, S trim tail, D fade in, F fade out",
                       info, juce::Justification::centredLeft, true);
        }

        void mouseDown(const juce::MouseEvent& event) override
        {
            if (!waveformBounds.contains(event.position))
                return;

            grabKeyboardFocus();
            const double t = xToTime(event.position.x, waveformBounds.reduced(14.0f, 10.0f));
            playheadSeconds = t;
            selectionStartSeconds = t;
            selectionEndSeconds = t;
            dragging = true;
            repaint();
        }

        void mouseDrag(const juce::MouseEvent& event) override
        {
            if (!dragging)
                return;

            const double t = xToTime(event.position.x, waveformBounds.reduced(14.0f, 10.0f));
            playheadSeconds = t;
            selectionEndSeconds = t;
            repaint();
        }

        void mouseUp(const juce::MouseEvent&) override
        {
            dragging = false;
            if (selectionEndSeconds < selectionStartSeconds)
                std::swap(selectionStartSeconds, selectionEndSeconds);
            repaint();
        }

        bool keyPressed(const juce::KeyPress& key) override
        {
            const auto c = static_cast<juce::juce_wchar>(key.getTextCharacter());
            if (c == 'a' || c == 'A') { setTrimStartToPlayhead(); return true; }
            if (c == 's' || c == 'S') { setTrimEndToPlayhead(); return true; }
            if (c == 'd' || c == 'D') { fadeInMs = 8.0; repaint(); return true; }
            if (c == 'f' || c == 'F') { fadeOutMs = 35.0; repaint(); return true; }
            return false;
        }

    private:
        void timerCallback() override
        {
            const auto now = juce::Time::getMillisecondCounterHiRes();
            const double delta = (now - lastTickMs) * 0.001;
            lastTickMs = now;
            playheadSeconds += delta;
            if (playheadSeconds >= trimEndSeconds)
            {
                playheadSeconds = trimStartSeconds;
                playing = false;
                playButton.setButtonText("Play");
                stopTimer();
            }
            repaint();
        }

        void drawWaveform(juce::Graphics& g, juce::Rectangle<float> area)
        {
            if (asset.buffer.getNumSamples() <= 0)
                return;

            g.setColour(juce::Colour(0xFF22D3EE).withAlpha(0.92f));
            const int width = juce::jmax(1, static_cast<int>(area.getWidth()));
            const int samples = asset.buffer.getNumSamples();
            const int channels = juce::jmax(1, asset.buffer.getNumChannels());
            const float centreY = area.getCentreY();
            const float scaleY = area.getHeight() * 0.45f;

            for (int x = 0; x < width; ++x)
            {
                const int start = static_cast<int>((static_cast<double>(x) / width) * samples);
                const int end = juce::jlimit(start + 1, samples,
                    static_cast<int>((static_cast<double>(x + 1) / width) * samples));
                float minValue = 0.0f;
                float maxValue = 0.0f;
                for (int i = start; i < end; ++i)
                {
                    float v = 0.0f;
                    for (int ch = 0; ch < channels; ++ch)
                        v += asset.buffer.getSample(ch, i);
                    v /= static_cast<float>(channels);
                    minValue = juce::jmin(minValue, v);
                    maxValue = juce::jmax(maxValue, v);
                }
                const float drawX = area.getX() + static_cast<float>(x);
                g.drawVerticalLine(static_cast<int>(drawX),
                                   centreY - maxValue * scaleY,
                                   centreY - minValue * scaleY);
            }

            g.setColour(juce::Colours::white.withAlpha(0.12f));
            g.drawHorizontalLine(static_cast<int>(centreY), area.getX(), area.getRight());
        }

        void drawEditOverlays(juce::Graphics& g, juce::Rectangle<float> area)
        {
            const auto sx = timeToX(selectionStartSeconds, area);
            const auto ex = timeToX(selectionEndSeconds, area);
            if (std::abs(ex - sx) > 2.0f)
            {
                g.setColour(juce::Colour(0xFFFFD166).withAlpha(0.14f));
                g.fillRect(juce::Rectangle<float>(juce::jmin(sx, ex), area.getY(),
                                                  std::abs(ex - sx), area.getHeight()));
            }

            g.setColour(juce::Colour(0xFFFFD166).withAlpha(0.22f));
            g.fillRect(juce::Rectangle<float>(area.getX(), area.getY(),
                                              timeToX(trimStartSeconds, area) - area.getX(), area.getHeight()));
            g.fillRect(juce::Rectangle<float>(timeToX(trimEndSeconds, area), area.getY(),
                                              area.getRight() - timeToX(trimEndSeconds, area), area.getHeight()));

            auto drawMarker = [&](double time, juce::Colour colour)
            {
                const auto x = timeToX(time, area);
                g.setColour(colour);
                g.drawVerticalLine(static_cast<int>(x), area.getY(), area.getBottom());
            };

            drawMarker(trimStartSeconds, juce::Colour(0xFFFFD166));
            drawMarker(trimEndSeconds, juce::Colour(0xFFFF2D78));
            drawMarker(playheadSeconds, juce::Colour(0xFFFFFFFF));
        }

        float timeToX(double time, juce::Rectangle<float> area) const
        {
            const double duration = juce::jmax(0.001, asset.metrics.durationSeconds);
            return area.getX() + area.getWidth() * static_cast<float>(juce::jlimit(0.0, 1.0, time / duration));
        }

        double xToTime(float x, juce::Rectangle<float> area) const
        {
            const double duration = juce::jmax(0.001, asset.metrics.durationSeconds);
            return duration * juce::jlimit(0.0, 1.0, static_cast<double>((x - area.getX()) / area.getWidth()));
        }

        void setTrimStartToPlayhead()
        {
            trimStartSeconds = juce::jlimit(0.0, trimEndSeconds, playheadSeconds);
            repaint();
        }

        void setTrimEndToPlayhead()
        {
            trimEndSeconds = juce::jlimit(trimStartSeconds, juce::jmax(0.001, asset.metrics.durationSeconds), playheadSeconds);
            repaint();
        }

        void sendEdit()
        {
            auto obj = std::make_unique<juce::DynamicObject>();
            obj->setProperty("trimStartS", trimStartSeconds);
            obj->setProperty("trimEndS", trimEndSeconds);
            obj->setProperty("fadeInMs", fadeInMs);
            obj->setProperty("fadeOutMs", fadeOutMs);
            obj->setProperty("snapToZeroCrossing", true);
            if (onCommit != nullptr)
                onCommit(slot, juce::var(obj.release()));
        }

        SourceSlot slot;
        Asset asset;
        CommitCallback onCommit;
        juce::TextButton playButton { "Play" };
        juce::TextButton startButton { "Start" };
        juce::TextButton trimStartButton { "A Trim" };
        juce::TextButton trimEndButton { "S Trim" };
        juce::TextButton fadeInButton { "D Fade" };
        juce::TextButton fadeOutButton { "F Fade" };
        juce::TextButton sendButton { "Send" };
        juce::Rectangle<float> waveformBounds;
        double trimStartSeconds = 0.0;
        double trimEndSeconds = 0.0;
        double selectionStartSeconds = 0.0;
        double selectionEndSeconds = 0.0;
        double playheadSeconds = 0.0;
        double fadeInMs = 0.0;
        double fadeOutMs = 0.0;
        double lastTickMs = 0.0;
        bool dragging = false;
        bool playing = false;
    };

    class AudioEditWindow final : public juce::DocumentWindow
    {
    public:
        AudioEditWindow(const juce::String& title,
                        std::unique_ptr<AudioEditComponent> editor,
                        std::function<void()> closeCallback)
            : juce::DocumentWindow(title, juce::Colour(0xFF101720), juce::DocumentWindow::closeButton),
              onClose(std::move(closeCallback))
        {
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            setContentOwned(editor.release(), true);
            centreWithSize(900, 420);
            setVisible(true);
            toFront(true);
        }

        void closeButtonPressed() override
        {
            if (onClose != nullptr)
                juce::MessageManager::callAsync(onClose);
        }

    private:
        std::function<void()> onClose;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEditWindow)
    };

    class BusRoutingComponent final : public juce::Component
    {
    public:
        using DisplayCallback = std::function<void(int, SourceSlot)>;
        using RouteCallback = std::function<void(PluginSlot, int, bool)>;
        using RouteModeCallback = std::function<void(bool)>;

        BusRoutingComponent(std::array<SourceSlot, 3> initialDisplays,
                            std::array<std::array<bool, 3>, 3> initialRoutes,
                            bool initialAllowMixedInputs,
                            DisplayCallback displayCallback,
                            RouteCallback routeCallback,
                            RouteModeCallback routeModeCallback,
                            bool light,
                            juce::LookAndFeel* comboLookAndFeel)
            : displaySlots(initialDisplays),
              renderRoutes(initialRoutes),
              allowMixedInputs(initialAllowMixedInputs),
              onDisplayChanged(std::move(displayCallback)),
              onRouteChanged(std::move(routeCallback)),
              onRouteModeChanged(std::move(routeModeCallback)),
              lightTheme(light)
        {
            routeMode.addItem("Controlled", 1);
            routeMode.addItem("Mix", 2);
            routeMode.setSelectedId(allowMixedInputs ? 2 : 1, juce::dontSendNotification);
            routeMode.onChange = [this]
            {
                allowMixedInputs = routeMode.getSelectedId() == 2;
                if (!allowMixedInputs)
                {
                    collapseRoutesToControlled();
                    syncRouteButtons();
                }
                if (onRouteModeChanged != nullptr)
                    onRouteModeChanged(allowMixedInputs);
                repaint();
            };
            routeMode.setLookAndFeel(comboLookAndFeel);
            addAndMakeVisible(routeMode);

            for (int i = 0; i < 3; ++i)
            {
                auto& combo = displayCombos[static_cast<size_t>(i)];
                combo.addItem("DRY A", 1);
                combo.addItem("DRY B", 2);
                combo.addItem("DRY C", 3);
                combo.addItem("WET A", 4);
                combo.addItem("WET B", 5);
                combo.addItem("WET C", 6);
                combo.setSelectedId(sourceIndex(displaySlots[static_cast<size_t>(i)]) + 1, juce::dontSendNotification);
                combo.setLookAndFeel(comboLookAndFeel);
                combo.onChange = [this, i]
                {
                    const auto source = sourceFromIndex(displayCombos[static_cast<size_t>(i)].getSelectedId() - 1);
                    displaySlots[static_cast<size_t>(i)] = source;
                    if (onDisplayChanged != nullptr)
                        onDisplayChanged(i, source);
                    repaint();
                };
                addAndMakeVisible(combo);
            }

            for (int row = 0; row < 3; ++row)
            {
                for (int col = 0; col < 3; ++col)
                {
                    auto& button = routeButtons[static_cast<size_t>(row)][static_cast<size_t>(col)];
                    button.setButtonText({});
                    button.setClickingTogglesState(true);
                    button.setToggleState(renderRoutes[static_cast<size_t>(row)][static_cast<size_t>(col)],
                                          juce::dontSendNotification);
                    button.onClick = [this, row, col]
                    {
                        const bool enabled = routeButtons[static_cast<size_t>(row)][static_cast<size_t>(col)].getToggleState();
                        if (!allowMixedInputs)
                        {
                            if (!enabled)
                            {
                                routeButtons[static_cast<size_t>(row)][static_cast<size_t>(col)]
                                    .setToggleState(true, juce::dontSendNotification);
                                return;
                            }

                            for (int i = 0; i < 3; ++i)
                                renderRoutes[static_cast<size_t>(row)][static_cast<size_t>(i)] = i == col;
                            syncRouteButtons();
                        }
                        else
                        {
                            renderRoutes[static_cast<size_t>(row)][static_cast<size_t>(col)] = enabled;
                        }

                        if (onRouteChanged != nullptr)
                            onRouteChanged(pluginSlotFromRow(row), col, allowMixedInputs ? enabled : true);
                        repaint();
                    };
                    addAndMakeVisible(button);
                }
            }
        }

        ~BusRoutingComponent() override
        {
            routeMode.setLookAndFeel(nullptr);
            for (auto& combo : displayCombos)
                combo.setLookAndFeel(nullptr);
        }

        void paint(juce::Graphics& g) override
        {
            const auto bg = lightTheme ? juce::Colour(0xFFF5F7FA) : juce::Colour(0xFF07080B);
            const auto panel = lightTheme ? juce::Colours::white.withAlpha(0.88f)
                                          : juce::Colour(0xFF0B1017).withAlpha(0.82f);
            const auto stroke = lightTheme ? juce::Colour(0xFF1E2530).withAlpha(0.16f)
                                           : juce::Colours::white.withAlpha(0.12f);
            const auto text = lightTheme ? juce::Colour(0xFF17202D) : juce::Colour(0xFFF3EEE4);
            const auto muted = lightTheme ? juce::Colour(0xFF5B6470) : juce::Colour(0xFFC9D2DE).withAlpha(0.72f);

            g.fillAll(bg);
            auto bounds = getLocalBounds().reduced(18).toFloat();
            g.setColour(panel);
            g.fillRoundedRectangle(bounds, 18.0f);
            g.setColour(stroke);
            g.drawRoundedRectangle(bounds.reduced(0.5f), 18.0f, 1.0f);

            auto title = getLocalBounds().reduced(34, 24).removeFromTop(42);
            g.setColour(text);
            g.setFont(juce::Font(juce::FontOptions(22.0f)).boldened());
            g.drawText("Audio Doctor Bus Routing", title, juce::Justification::centredLeft, true);
            g.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
            g.setColour(muted);
            g.drawText(allowMixedInputs ? "Mix mode: several DRY sources are summed before each WET render."
                                        : "Controlled mode: each WET renders exactly one DRY source.",
                       title.removeFromBottom(18), juce::Justification::centredRight, true);

            g.setColour(muted);
            g.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
            g.drawText("DISPLAY", displayPanel.withHeight(22.0f), juce::Justification::centredLeft, true);
            g.drawText("ROUTING MODE", modeLabelBounds, juce::Justification::centredLeft, true);
            g.drawText("DRY INPUTS", matrixPanel.withHeight(22.0f), juce::Justification::centredLeft, true);
            g.drawText("WET BUSES", matrixPanel.withX(matrixPanel.getRight() - 160.0f).withWidth(150.0f).withHeight(22.0f),
                       juce::Justification::centredRight, true);

            for (int i = 0; i < 3; ++i)
            {
                const auto label = juce::String("Display ") + juce::String(i + 1);
                g.setColour(muted);
                g.drawText(label, displayLabelBounds[static_cast<size_t>(i)], juce::Justification::centredLeft, true);
            }

            for (int col = 0; col < 3; ++col)
            {
                g.setColour(dryBusColour(col, lightTheme));
                g.fillEllipse(dryDots[static_cast<size_t>(col)].reduced(2.0f));
                g.setColour(text);
                g.drawText(dryLabel(col), dryLabelBounds[static_cast<size_t>(col)], juce::Justification::centred, true);
            }

            for (int row = 0; row < 3; ++row)
            {
                g.setColour(wetBusColour(row, lightTheme));
                g.fillEllipse(wetDots[static_cast<size_t>(row)].reduced(2.0f));
                g.setColour(text);
                g.drawText(wetLabel(row), wetLabelBounds[static_cast<size_t>(row)], juce::Justification::centredLeft, true);
            }

            for (int row = 0; row < 3; ++row)
            {
                for (int col = 0; col < 3; ++col)
                {
                    if (!renderRoutes[static_cast<size_t>(row)][static_cast<size_t>(col)])
                        continue;

                    const auto start = dryDots[static_cast<size_t>(col)].getCentre();
                    const auto end = wetDots[static_cast<size_t>(row)].getCentre();
                    const auto colour = dryBusColour(col, lightTheme).interpolatedWith(wetBusColour(row, lightTheme), 0.45f);
                    g.setColour(colour.withAlpha(lightTheme ? 0.28f : 0.24f));
                    g.drawLine(start.x, start.y + 3.0f, end.x, end.y + 3.0f, 7.0f);
                    g.setColour(colour.withAlpha(lightTheme ? 0.88f : 0.86f));
                    g.drawLine(start.x, start.y, end.x, end.y, 2.2f);
                }
            }
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced(34, 30);
            bounds.removeFromTop(58);
            displayPanel = bounds.removeFromLeft(280).toFloat();
            bounds.removeFromLeft(28);
            matrixPanel = bounds.toFloat();

            auto displayArea = displayPanel.toNearestInt().reduced(0, 34);
            modeLabelBounds = displayArea.removeFromTop(22).toFloat();
            routeMode.setBounds(displayArea.removeFromTop(40).reduced(4, 4));
            displayArea.removeFromTop(16);
            for (int i = 0; i < 3; ++i)
            {
                auto row = displayArea.removeFromTop(54);
                displayLabelBounds[static_cast<size_t>(i)] = row.removeFromLeft(92).toFloat();
                displayCombos[static_cast<size_t>(i)].setBounds(row.reduced(4, 7));
                displayArea.removeFromTop(10);
            }

            auto matrix = matrixPanel.toNearestInt().reduced(8, 34);
            auto header = matrix.removeFromTop(62);
            const int sourceWidth = juce::jmax(76, (header.getWidth() - 170) / 3);
            for (int col = 0; col < 3; ++col)
            {
                auto area = header.removeFromLeft(sourceWidth);
                dryLabelBounds[static_cast<size_t>(col)] = area.removeFromTop(24).toFloat();
                dryDots[static_cast<size_t>(col)] = area.withSizeKeepingCentre(28, 28).toFloat();
            }

            matrix.removeFromTop(6);
            for (int row = 0; row < 3; ++row)
            {
                auto busRow = matrix.removeFromTop(78);
                for (int col = 0; col < 3; ++col)
                {
                    auto sourceCell = busRow.removeFromLeft(sourceWidth);
                    routeButtons[static_cast<size_t>(row)][static_cast<size_t>(col)]
                        .setBounds(sourceCell.withSizeKeepingCentre(30, 30));
                }

                auto wetArea = busRow.reduced(8, 0);
                wetDots[static_cast<size_t>(row)] = wetArea.removeFromLeft(30).withSizeKeepingCentre(28, 28).toFloat();
                wetLabelBounds[static_cast<size_t>(row)] = wetArea.toFloat();
                matrix.removeFromTop(12);
            }
        }

    private:
        static SourceSlot sourceFromIndex(int index)
        {
            switch (index)
            {
                case 1: return SourceSlot::dryB;
                case 2: return SourceSlot::dryC;
                case 3: return SourceSlot::wetA;
                case 4: return SourceSlot::wetB;
                case 5: return SourceSlot::wetC;
                default: return SourceSlot::dryA;
            }
        }

        static int sourceIndex(SourceSlot slot)
        {
            switch (slot)
            {
                case SourceSlot::dryB: return 1;
                case SourceSlot::dryC: return 2;
                case SourceSlot::wetA: return 3;
                case SourceSlot::wetB: return 4;
                case SourceSlot::wetC: return 5;
                default: return 0;
            }
        }

        static PluginSlot pluginSlotFromRow(int row)
        {
            switch (row)
            {
                case 1: return PluginSlot::B;
                case 2: return PluginSlot::C;
                default: return PluginSlot::A;
            }
        }

        static juce::String dryLabel(int index)
        {
            return index == 1 ? "DRY B" : (index == 2 ? "DRY C" : "DRY A");
        }

        static juce::String wetLabel(int index)
        {
            return index == 1 ? "WET B" : (index == 2 ? "WET C" : "WET A");
        }

        static juce::Colour dryBusColour(int index, bool light)
        {
            if (index == 1) return light ? juce::Colour(0xFFC26A00) : GoodMeterLookAndFeel::accentYellow;
            if (index == 2) return light ? juce::Colour(0xFFC2185B) : GoodMeterLookAndFeel::accentPink;
            return light ? juce::Colour(0xFF006D9C) : GoodMeterLookAndFeel::accentBlue;
        }

        static juce::Colour wetBusColour(int index, bool light)
        {
            if (index == 1) return light ? juce::Colour(0xFFC2185B) : GoodMeterLookAndFeel::accentPink;
            if (index == 2) return light ? juce::Colour(0xFF006D9C) : GoodMeterLookAndFeel::accentBlue;
            return light ? juce::Colour(0xFFC26A00) : GoodMeterLookAndFeel::accentYellow;
        }

        void collapseRoutesToControlled()
        {
            for (auto& row : renderRoutes)
            {
                int active = -1;
                for (int col = 0; col < 3; ++col)
                {
                    if (row[static_cast<size_t>(col)])
                    {
                        active = col;
                        break;
                    }
                }

                if (active < 0)
                    active = 0;

                for (int col = 0; col < 3; ++col)
                    row[static_cast<size_t>(col)] = col == active;
            }
        }

        void syncRouteButtons()
        {
            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 3; ++col)
                    routeButtons[static_cast<size_t>(row)][static_cast<size_t>(col)]
                        .setToggleState(renderRoutes[static_cast<size_t>(row)][static_cast<size_t>(col)],
                                        juce::dontSendNotification);
        }

        std::array<SourceSlot, 3> displaySlots;
        std::array<std::array<bool, 3>, 3> renderRoutes;
        bool allowMixedInputs = false;
        DisplayCallback onDisplayChanged;
        RouteCallback onRouteChanged;
        RouteModeCallback onRouteModeChanged;
        bool lightTheme = false;
        juce::ComboBox routeMode;
        std::array<juce::ComboBox, 3> displayCombos;
        std::array<std::array<juce::ToggleButton, 3>, 3> routeButtons;
        juce::Rectangle<float> displayPanel;
        juce::Rectangle<float> matrixPanel;
        juce::Rectangle<float> modeLabelBounds;
        std::array<juce::Rectangle<float>, 3> displayLabelBounds;
        std::array<juce::Rectangle<float>, 3> dryDots;
        std::array<juce::Rectangle<float>, 3> wetDots;
        std::array<juce::Rectangle<float>, 3> dryLabelBounds;
        std::array<juce::Rectangle<float>, 3> wetLabelBounds;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BusRoutingComponent)
    };

    class BusRoutingWindow final : public juce::DocumentWindow
    {
    public:
        BusRoutingWindow(std::unique_ptr<BusRoutingComponent> router,
                         std::function<void()> closeCallback)
            : juce::DocumentWindow("AUDIO DOCTOR BUS ROUTING",
                                   juce::Colour(0xFF101720),
                                   juce::DocumentWindow::closeButton),
              onClose(std::move(closeCallback))
        {
            setUsingNativeTitleBar(true);
            setResizable(true, true);
            setContentOwned(router.release(), true);
            centreWithSize(960, 560);
            setVisible(true);
            toFront(true);
        }

        void closeButtonPressed() override
        {
            if (onClose != nullptr)
                juce::MessageManager::callAsync(onClose);
        }

    private:
        std::function<void()> onClose;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BusRoutingWindow)
    };

    class GenerateSignalComponent final : public juce::Component
    {
    public:
        using GenerateCallback = std::function<void(SourceSlot, goodmeter::audio_doctor::GeneratedSignalSpec)>;
        using CloseCallback = std::function<void()>;

        GenerateSignalComponent(bool light, GenerateCallback generate, CloseCallback close)
            : lightTheme(light), onGenerate(std::move(generate)), onClose(std::move(close))
        {
            addAndMakeVisible(titleLabel);
            titleLabel.setText("Generate Test Signal", juce::dontSendNotification);
            titleLabel.setJustificationType(juce::Justification::centredLeft);
            titleLabel.setFont(juce::Font(juce::FontOptions(28.0f)).boldened());

            configureLabel(typeLabel, "Signal");
            configureLabel(targetLabel, "Render to");
            configureLabel(levelLabel, "Level");
            configureLabel(durationLabel, "Duration");
            configureLabel(frequencyLabel, "Frequency");
            configureLabel(startLabel, "Start Hz");
            configureLabel(endLabel, "End Hz");
            configureLabel(phaseLabel, "Phase");
            configureLabel(harmonicsLabel, "Harmonics");
            configureLabel(rolloffLabel, "Rolloff");
            configureLabel(seedLabel, "Seed");

            typeBox.addItem("Sine", 1);
            typeBox.addItem("Harmonic series", 2);
            typeBox.addItem("White noise", 3);
            typeBox.addItem("Pink noise", 4);
            typeBox.addItem("Impulse", 5);
            typeBox.addItem("Click", 6);
            typeBox.addItem("Transient burst", 7);
            typeBox.addItem("Sweep", 8);
            typeBox.addItem("Charge riser", 9);
            typeBox.addItem("Shot impact", 10);
            typeBox.addItem("Tail decay", 11);
            typeBox.addItem("CST event", 12);
            typeBox.addItem("Harmonic fusion", 13);
            typeBox.addItem("Band-limited noise", 14);
            typeBox.setSelectedId(5, juce::dontSendNotification);
            typeBox.onChange = [this] { updateParameterVisibility(); };

            targetBox.addItem("DRY A", 1);
            targetBox.addItem("DRY B", 2);
            targetBox.addItem("DRY C", 3);
            targetBox.setSelectedId(1, juce::dontSendNotification);

            configureCombo(typeBox);
            configureCombo(targetBox);
            configureSlider(levelSlider, -90.0, 0.0, 0.1, -6.0, " dB");
            configureSlider(durationSlider, 0.02, 30.0, 0.01, 2.0, " s");
            configureSlider(frequencySlider, 10.0, 20000.0, 1.0, 1000.0, " Hz");
            configureSlider(startSlider, 10.0, 20000.0, 1.0, 20.0, " Hz");
            configureSlider(endSlider, 10.0, 24000.0, 1.0, 20000.0, " Hz");
            configureSlider(phaseSlider, 0.0, 360.0, 1.0, 0.0, " deg");
            configureSlider(harmonicsSlider, 1.0, 32.0, 1.0, 6.0, "");
            configureSlider(rolloffSlider, 0.0, 24.0, 0.1, 6.0, " dB/oct");
            configureSlider(seedSlider, 1.0, 999999.0, 1.0, 4432.0, "");

            invertToggle.setButtonText("Invert polarity");
            invertToggle.setColour(juce::ToggleButton::textColourId, textColour());
            addAndMakeVisible(invertToggle);

            generateButton.setButtonText("Generate");
            closeButton.setButtonText("Close");
            for (auto* button : { &generateButton, &closeButton })
            {
                addAndMakeVisible(button);
                GoodMeterLookAndFeel::markAsIOSEnglishMono(*button);
                button->setColour(juce::TextButton::textColourOffId, textColour());
                button->setColour(juce::TextButton::buttonColourId, controlColour());
            }

            generateButton.onClick = [this]
            {
                if (onGenerate != nullptr)
                    onGenerate(targetSlot(), makeSpec());
                if (onClose != nullptr)
                    juce::MessageManager::callAsync(onClose);
            };
            closeButton.onClick = [this]
            {
                if (onClose != nullptr)
                    juce::MessageManager::callAsync(onClose);
            };

            updateParameterVisibility();
            setSize(700, 640);
        }

        ~GenerateSignalComponent() override
        {
            typeBox.setLookAndFeel(nullptr);
            targetBox.setLookAndFeel(nullptr);
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(lightTheme ? juce::Colour(0xFFF4F7FB) : juce::Colour(0xFF05070B));
            auto plate = getLocalBounds().reduced(20).toFloat();
            g.setColour(lightTheme ? juce::Colours::white : juce::Colour(0xFF0A0D13));
            g.fillRoundedRectangle(plate, 18.0f);
            g.setColour(lightTheme ? juce::Colour(0xFF1E2530).withAlpha(0.14f)
                                   : juce::Colours::white.withAlpha(0.10f));
            g.drawRoundedRectangle(plate, 18.0f, 1.2f);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced(34);
            titleLabel.setBounds(area.removeFromTop(42));
            area.removeFromTop(10);

            auto top = area.removeFromTop(40);
            layoutCombo(targetLabel, targetBox, top.removeFromLeft(top.getWidth() / 2 - 8));
            top.removeFromLeft(16);
            layoutCombo(typeLabel, typeBox, top);
            area.removeFromTop(18);

            layoutSlider(levelLabel, levelSlider, area);
            layoutSlider(durationLabel, durationSlider, area);
            layoutSlider(frequencyLabel, frequencySlider, area);
            layoutSlider(startLabel, startSlider, area);
            layoutSlider(endLabel, endSlider, area);
            layoutSlider(phaseLabel, phaseSlider, area);
            layoutSlider(harmonicsLabel, harmonicsSlider, area);
            layoutSlider(rolloffLabel, rolloffSlider, area);
            layoutSlider(seedLabel, seedSlider, area);

            auto invertRow = area.removeFromTop(invertToggle.isVisible() ? 34 : 0);
            invertToggle.setBounds(invertRow.withTrimmedLeft(138));
            area.removeFromTop(8);

            auto buttons = area.removeFromBottom(42);
            closeButton.setBounds(buttons.removeFromRight(120).reduced(2));
            buttons.removeFromRight(10);
            generateButton.setBounds(buttons.removeFromRight(140).reduced(2));
        }

    private:
        juce::Colour textColour() const { return lightTheme ? juce::Colour(0xFF17202D) : juce::Colour(0xFFF3EEE4); }
        juce::Colour detailColour() const { return lightTheme ? juce::Colour(0xFF536071) : juce::Colour(0xFFC9D2DE); }
        juce::Colour controlColour() const { return lightTheme ? juce::Colour(0xFFFFFFFF) : juce::Colour(0xFF101720); }

        void configureLabel(juce::Label& label, const juce::String& text)
        {
            label.setText(text, juce::dontSendNotification);
            label.setJustificationType(juce::Justification::centredLeft);
            label.setColour(juce::Label::textColourId, detailColour());
            label.setFont(juce::Font(juce::FontOptions(14.0f)).boldened());
            addAndMakeVisible(label);
        }

        void configureCombo(juce::ComboBox& combo)
        {
            combo.setLookAndFeel(&popupLookAndFeel);
            combo.setColour(juce::ComboBox::backgroundColourId, controlColour());
            combo.setColour(juce::ComboBox::textColourId, textColour());
            combo.setColour(juce::ComboBox::outlineColourId, detailColour().withAlpha(0.25f));
            combo.setColour(juce::ComboBox::arrowColourId, textColour());
            GoodMeterLookAndFeel::markAsIOSEnglishMono(combo);
            addAndMakeVisible(combo);
        }

        void configureSlider(juce::Slider& slider, double min, double max, double step,
                             double value, const juce::String& suffix)
        {
            slider.setRange(min, max, step);
            slider.setValue(value, juce::dontSendNotification);
            slider.setSliderStyle(juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 86, 22);
            slider.setTextValueSuffix(suffix);
            slider.setColour(juce::Slider::thumbColourId, GoodMeterLookAndFeel::accentBlue);
            slider.setColour(juce::Slider::trackColourId, GoodMeterLookAndFeel::accentBlue.withAlpha(0.30f));
            slider.setColour(juce::Slider::backgroundColourId, detailColour().withAlpha(0.16f));
            slider.setColour(juce::Slider::textBoxTextColourId, textColour());
            slider.setColour(juce::Slider::textBoxBackgroundColourId, controlColour());
            slider.setColour(juce::Slider::textBoxOutlineColourId, detailColour().withAlpha(0.22f));
            addAndMakeVisible(slider);
        }

        void layoutCombo(juce::Label& label, juce::ComboBox& combo, juce::Rectangle<int> row)
        {
            label.setBounds(row.removeFromLeft(90));
            combo.setBounds(row.reduced(2));
        }

        void layoutSlider(juce::Label& label, juce::Slider& slider, juce::Rectangle<int>& area)
        {
            const bool visible = slider.isVisible();
            label.setVisible(visible);
            if (!visible)
                return;

            auto row = area.removeFromTop(36);
            label.setBounds(row.removeFromLeft(130));
            slider.setBounds(row.reduced(2));
            area.removeFromTop(6);
        }

        void updateParameterVisibility()
        {
            const int type = typeBox.getSelectedId();
            const bool sine = type == 1;
            const bool harmonic = type == 2;
            const bool transient = type == 7;
            const bool sweep = type == 8;
            const bool charge = type == 9;
            const bool shot = type == 10;
            const bool tail = type == 11;
            const bool cst = type == 12;
            const bool fusion = type == 13;
            const bool bandNoise = type == 14;

            frequencyLabel.setText(transient || shot ? "Body Hz" : (tail ? "Fund Hz" : (fusion ? "Fund A" : "Frequency")), juce::dontSendNotification);
            startLabel.setText(charge || cst ? "Start Hz" : (bandNoise ? "Low Hz" : "Start Hz"), juce::dontSendNotification);
            endLabel.setText(transient || shot ? "Bright Hz" : (fusion ? "Fund B" : (bandNoise ? "High Hz" : "End Hz")), juce::dontSendNotification);

            frequencySlider.setVisible(sine || harmonic || transient || shot || tail || fusion);
            startSlider.setVisible(sweep || charge || cst || bandNoise);
            endSlider.setVisible(sweep || transient || charge || shot || cst || fusion || bandNoise);
            phaseSlider.setVisible(sine || harmonic || sweep);
            harmonicsSlider.setVisible(harmonic || charge || fusion);
            rolloffSlider.setVisible(harmonic || charge || fusion);
            seedSlider.setVisible(true);
            invertToggle.setVisible(true);
            resized();
            repaint();
        }

        SourceSlot targetSlot() const
        {
            switch (targetBox.getSelectedId())
            {
                case 2: return SourceSlot::dryB;
                case 3: return SourceSlot::dryC;
                default: return SourceSlot::dryA;
            }
        }

        goodmeter::audio_doctor::GeneratedSignalSpec makeSpec() const
        {
            goodmeter::audio_doctor::GeneratedSignalSpec spec;
            switch (typeBox.getSelectedId())
            {
                case 1: spec.type = "sine"; break;
                case 2: spec.type = "harmonic_series"; break;
                case 3: spec.type = "white_noise"; break;
                case 4: spec.type = "pink_noise"; break;
                case 6: spec.type = "click"; break;
                case 7: spec.type = "transient_burst"; break;
                case 8: spec.type = "sweep"; break;
                case 9: spec.type = "charge_riser"; break;
                case 10: spec.type = "shot_impact"; break;
                case 11: spec.type = "tail_decay"; break;
                case 12: spec.type = "cst_event"; break;
                case 13: spec.type = "harmonic_fusion_test"; break;
                case 14: spec.type = "band_limited_noise"; break;
                default: spec.type = "impulse"; break;
            }

            spec.levelDb = levelSlider.getValue();
            spec.seconds = durationSlider.getValue();
            spec.frequencyHz = frequencySlider.getValue();
            spec.startHz = startSlider.getValue();
            spec.endHz = endSlider.getValue();
            spec.phaseDegrees = phaseSlider.getValue();
            spec.harmonicCount = static_cast<int>(std::round(harmonicsSlider.getValue()));
            spec.harmonicRolloffDb = rolloffSlider.getValue();
            spec.seed = static_cast<int>(std::round(seedSlider.getValue()));
            spec.invert = invertToggle.getToggleState();
            spec.bodyHz = spec.frequencyHz;
            spec.fundamentalHz = spec.frequencyHz;
            spec.fundamentalAHz = spec.frequencyHz;
            spec.fundamentalBHz = spec.endHz;
            spec.crackHz = spec.endHz;
            spec.noiseBandLowHz = spec.startHz;
            spec.noiseBandHighHz = spec.endHz;
            if (spec.type == "cst_event")
            {
                spec.shotTimeSec = spec.seconds * 0.38;
                spec.tailStartSec = juce::jmin(spec.seconds - 0.05, spec.shotTimeSec + 0.12);
            }
            return spec;
        }

        bool lightTheme = false;
        GenerateCallback onGenerate;
        CloseCallback onClose;
        AudioDoctorPopupLookAndFeel popupLookAndFeel;
        juce::Label titleLabel;
        juce::Label typeLabel;
        juce::Label targetLabel;
        juce::Label levelLabel;
        juce::Label durationLabel;
        juce::Label frequencyLabel;
        juce::Label startLabel;
        juce::Label endLabel;
        juce::Label phaseLabel;
        juce::Label harmonicsLabel;
        juce::Label rolloffLabel;
        juce::Label seedLabel;
        juce::ComboBox typeBox;
        juce::ComboBox targetBox;
        juce::Slider levelSlider;
        juce::Slider durationSlider;
        juce::Slider frequencySlider;
        juce::Slider startSlider;
        juce::Slider endSlider;
        juce::Slider phaseSlider;
        juce::Slider harmonicsSlider;
        juce::Slider rolloffSlider;
        juce::Slider seedSlider;
        juce::ToggleButton invertToggle;
        juce::TextButton generateButton { "Generate" };
        juce::TextButton closeButton { "Close" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GenerateSignalComponent)
    };

    class GenerateSignalWindow final : public juce::DocumentWindow
    {
    public:
        GenerateSignalWindow(std::unique_ptr<GenerateSignalComponent> content,
                             bool light,
                             std::function<void()> closeCallback)
            : juce::DocumentWindow("AUDIO DOCTOR GENERATE",
                                   light ? juce::Colour(0xFFF4F7FB) : juce::Colour(0xFF101720),
                                   juce::DocumentWindow::closeButton),
              onClose(std::move(closeCallback))
        {
            setUsingNativeTitleBar(true);
            setResizable(false, false);
            setContentOwned(content.release(), true);
            centreWithSize(620, 520);
            setVisible(true);
            toFront(true);
        }

        void closeButtonPressed() override
        {
            if (onClose != nullptr)
                juce::MessageManager::callAsync(onClose);
        }

    private:
        std::function<void()> onClose;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GenerateSignalWindow)
    };

    static constexpr int contentPadding = 14;
    static constexpr int toolbarHeight = 68;
    static constexpr int toolbarStatusGap = 8;
    static constexpr int statusHeight = 28;
    static constexpr int statusFigureGap = 10;

    struct FrequencyRange
    {
        float minHz = 20.0f;
        float maxHz = 20000.0f;
        juce::String label = "20-20k";
    };

    FrequencyRange getFrequencyRange() const
    {
        return { frequencyMinHz, frequencyMaxHz, formatFrequencyRange(frequencyMinHz, frequencyMaxHz) };
    }

    static juce::String formatFrequency(float hz)
    {
        if (hz >= 1000.0f)
            return juce::String(hz / 1000.0f, hz >= 10000.0f ? 0 : 1) + "k";

        return juce::String(static_cast<int>(std::round(hz)));
    }

    static juce::String formatFrequencyRange(float minHz, float maxHz)
    {
        return formatFrequency(minHz) + "-" + formatFrequency(maxHz);
    }

    void setFrequencyRange(float minHz, float maxHz, bool updateComboText = true)
    {
        minHz = juce::jlimit(20.0f, 20000.0f, minHz);
        maxHz = juce::jlimit(20.0f, 20000.0f, maxHz);

        if (maxHz < minHz)
            std::swap(minHz, maxHz);

        const float minLogSpan = std::log10(1.45f);
        float lo = std::log10(minHz);
        float hi = std::log10(maxHz);
        if (hi - lo < minLogSpan)
        {
            const float centre = (lo + hi) * 0.5f;
            lo = centre - minLogSpan * 0.5f;
            hi = centre + minLogSpan * 0.5f;
        }

        const float minBound = std::log10(20.0f);
        const float maxBound = std::log10(20000.0f);
        if (lo < minBound)
        {
            hi += minBound - lo;
            lo = minBound;
        }
        if (hi > maxBound)
        {
            lo -= hi - maxBound;
            hi = maxBound;
        }

        frequencyMinHz = std::pow(10.0f, juce::jlimit(minBound, maxBound, lo));
        frequencyMaxHz = std::pow(10.0f, juce::jlimit(minBound, maxBound, hi));

        if (updateComboText)
            freqZoom.setText(formatFrequencyRange(frequencyMinHz, frequencyMaxHz), juce::dontSendNotification);

        repaint();
    }

    void applyFrequencyPreset(int presetId)
    {
        switch (presetId)
        {
            case 2:  setFrequencyRange(20.0f, 200.0f, false); break;
            case 3:  setFrequencyRange(100.0f, 1000.0f, false); break;
            case 4:  setFrequencyRange(250.0f, 4000.0f, false); break;
            case 5:  setFrequencyRange(1000.0f, 8000.0f, false); break;
            case 6:  setFrequencyRange(5000.0f, 20000.0f, false); break;
            default: setFrequencyRange(20.0f, 20000.0f, false); break;
        }
    }

    void panFrequencyRange(float spanFraction)
    {
        const float lo = std::log10(frequencyMinHz);
        const float hi = std::log10(frequencyMaxHz);
        const float minBound = std::log10(20.0f);
        const float maxBound = std::log10(20000.0f);
        if (std::abs(lo - minBound) < 0.0001f && std::abs(hi - maxBound) < 0.0001f)
            return;

        const float shift = (hi - lo) * spanFraction;
        setFrequencyRange(std::pow(10.0f, lo + shift), std::pow(10.0f, hi + shift));
    }

    void zoomFrequencyRange(float spanMultiplier)
    {
        const float lo = std::log10(frequencyMinHz);
        const float hi = std::log10(frequencyMaxHz);
        const float centre = (lo + hi) * 0.5f;
        const float halfSpan = (hi - lo) * spanMultiplier * 0.5f;
        setFrequencyRange(std::pow(10.0f, centre - halfSpan), std::pow(10.0f, centre + halfSpan));
    }

    static juce::Colour uiText()
    {
        return juce::Colour(0xFFF3EEE4);
    }

    static juce::Colour lightUiText()
    {
        return juce::Colour(0xFF17202D);
    }

    static juce::Colour mutedUiText()
    {
        return uiText().withAlpha(0.62f);
    }

    static juce::Colour detailText(bool lightTheme)
    {
        return lightTheme ? juce::Colour(0xFF20242C)
                          : juce::Colour(0xFFF7F9FC).withAlpha(0.92f);
    }

    static juce::Colour secondaryDetailText(bool lightTheme)
    {
        return lightTheme ? juce::Colour(0xFF4F5865)
                          : juce::Colour(0xFFEAF0F8).withAlpha(0.84f);
    }

    static bool& figureLightThemeFlag()
    {
        static bool light = false;
        return light;
    }

    bool isLightThemeSelected() const
    {
        return themeMode.getSelectedId() == 2;
    }

    static bool isLightFigure(bool exportMode)
    {
        juce::ignoreUnused(exportMode);
        return figureLightThemeFlag();
    }

    static juce::Colour axisText(bool exportMode)
    {
        return isLightFigure(exportMode) ? juce::Colour(0xFF20242C) : mutedUiText();
    }

    static juce::Colour dryColour(bool exportMode)
    {
        juce::ignoreUnused(exportMode);
        return isLightFigure(exportMode) ? juce::Colour(0xFF006D9C)
                                         : GoodMeterLookAndFeel::accentBlue;
    }

    static juce::Colour wetColour(bool exportMode)
    {
        juce::ignoreUnused(exportMode);
        return isLightFigure(exportMode) ? juce::Colour(0xFFC26A00)
                                         : GoodMeterLookAndFeel::accentYellow;
    }

    static juce::Colour wetBColour(bool exportMode)
    {
        juce::ignoreUnused(exportMode);
        return isLightFigure(exportMode) ? juce::Colour(0xFFC2185B)
                                         : GoodMeterLookAndFeel::accentPink;
    }

    static juce::Colour pluginSlotColour(PluginSlot slot, bool exportMode)
    {
        switch (slot)
        {
            case PluginSlot::B: return wetBColour(exportMode);
            case PluginSlot::C: return dryColour(exportMode);
            default: return wetColour(exportMode);
        }
    }

    void refreshThemeColours()
    {
        figureLightThemeFlag() = isLightThemeSelected();
        const bool light = isLightThemeSelected();
        syncTitleBarTheme();
        const auto text = light ? lightUiText() : uiText();
        const auto muted = light ? juce::Colour(0xFF4B5565) : uiText().withAlpha(0.68f);
        const auto comboBg = light ? juce::Colours::white.withAlpha(0.94f)
                                   : juce::Colour(0xFF10141D).withAlpha(0.72f);
        const auto comboOutline = light ? juce::Colour(0xFF1E2530).withAlpha(0.22f)
                                        : juce::Colours::white.withAlpha(0.10f);

        for (auto* button : { &importDryBtn, &generateBtn, &editAudioBtn, &busBtn, &pluginBtn, &editPluginBtn,
                              &renderBtn, &pluginBBtn, &editPluginBBtn, &renderBBtn,
                              &pluginCBtn, &editPluginCBtn, &renderCBtn,
                              &exportBtn, &resetBtn })
        {
            button->setColour(juce::TextButton::textColourOffId, text);
            button->setColour(juce::TextButton::textColourOnId, light ? juce::Colours::white
                                                                      : juce::Colour(0xFF080A0F));
        }

        for (auto* combo : { &viewMode, &freqZoom, &themeMode })
        {
            combo->setColour(juce::ComboBox::backgroundColourId, comboBg);
            combo->setColour(juce::ComboBox::textColourId, text);
            combo->setColour(juce::ComboBox::outlineColourId, comboOutline);
            combo->setColour(juce::ComboBox::arrowColourId, text.withAlpha(0.88f));
        }

        statusLabel.setColour(juce::Label::textColourId, text.withAlpha(light ? 0.86f : 0.88f));
        pluginSlotLabel.setColour(juce::Label::textColourId, muted);
    }

    void syncTitleBarTheme() const
    {
        if (auto* top = getTopLevelComponent())
        {
            const bool light = isLightThemeSelected();
            const auto current = static_cast<bool>(top->getProperties().getWithDefault("audioDoctorLightTheme", !light));
            if (current != light)
            {
                top->getProperties().set("audioDoctorLightTheme", light);
                top->repaint();
            }
        }
    }

    static void drawGlassPlate(juce::Graphics& g, juce::Rectangle<float> area,
                               juce::Colour tint, float fillAlpha)
    {
        juce::ignoreUnused(tint);
        const float radius = 18.0f;
        juce::Path path;
        path.addRoundedRectangle(area, radius);

        const bool light = figureLightThemeFlag();
        g.setColour(light ? juce::Colour(0xFF9AA5B4).withAlpha(0.16f)
                          : juce::Colours::black.withAlpha(0.16f));
        g.fillRoundedRectangle(area.translated(0.0f, 2.0f), radius);

        g.setColour(light ? juce::Colours::white.withAlpha(0.88f)
                          : juce::Colour(0xFF0B1017).withAlpha(fillAlpha));
        g.fillPath(path);

        g.setColour(light ? juce::Colour(0xFF1B2430).withAlpha(0.12f)
                          : juce::Colour(0xFFF6EEE3).withAlpha(0.12f));
        g.drawRoundedRectangle(area.reduced(0.5f), radius, 1.0f);
    }

    void drawAppBackground(juce::Graphics& g) const
    {
        g.fillAll(figureLightThemeFlag() ? juce::Colour(0xFFF1F3F6)
                                          : juce::Colour(0xFF07080B));
    }

    juce::Rectangle<int> getFigureBounds() const
    {
        auto bounds = getLocalBounds().reduced(contentPadding);
        bounds.removeFromTop(toolbarHeight);
        bounds.removeFromTop(toolbarStatusGap);
        bounds.removeFromTop(statusHeight);
        bounds.removeFromTop(statusFigureGap);
        return bounds;
    }

    //==========================================================================
    static juce::String sourceSlotLabel(SourceSlot slot)
    {
        switch (slot)
        {
            case SourceSlot::dryB: return "DRY B";
            case SourceSlot::dryC: return "DRY C";
            case SourceSlot::wetA: return "WET A";
            case SourceSlot::wetB: return "WET B";
            case SourceSlot::wetC: return "WET C";
            default: return "DRY A";
        }
    }

    std::unique_ptr<Asset>& assetHolderFor(SourceSlot slot)
    {
        switch (slot)
        {
            case SourceSlot::dryB: return dryBAsset;
            case SourceSlot::dryC: return dryCAsset;
            case SourceSlot::wetA: return wetAsset;
            case SourceSlot::wetB: return wetBAsset;
            case SourceSlot::wetC: return wetCAsset;
            default: return dryAsset;
        }
    }

    const std::unique_ptr<Asset>& assetHolderFor(SourceSlot slot) const
    {
        switch (slot)
        {
            case SourceSlot::dryB: return dryBAsset;
            case SourceSlot::dryC: return dryCAsset;
            case SourceSlot::wetA: return wetAsset;
            case SourceSlot::wetB: return wetBAsset;
            case SourceSlot::wetC: return wetCAsset;
            default: return dryAsset;
        }
    }

    Asset* assetFor(SourceSlot slot) const
    {
        return assetHolderFor(slot).get();
    }

    Asset* displayAsset(int index) const
    {
        return juce::isPositiveAndBelow(index, static_cast<int>(displaySlots.size()))
            ? assetFor(displaySlots[static_cast<size_t>(index)])
            : nullptr;
    }

    bool hasAnySourceAsset() const
    {
        return dryAsset != nullptr || dryBAsset != nullptr || dryCAsset != nullptr
            || wetAsset != nullptr || wetBAsset != nullptr || wetCAsset != nullptr;
    }

    juce::String displayLabel(int index) const
    {
        return juce::isPositiveAndBelow(index, static_cast<int>(displaySlots.size()))
            ? sourceSlotLabel(displaySlots[static_cast<size_t>(index)])
            : juce::String();
    }

    static bool isDrySlot(SourceSlot slot)
    {
        return slot == SourceSlot::dryA || slot == SourceSlot::dryB || slot == SourceSlot::dryC;
    }

    void showLoadDryMenu()
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel(&audioDoctorPopupLookAndFeel);
        menu.addItem(101, "Load DRY A");
        menu.addItem(102, "Load DRY B");
        menu.addItem(103, "Load DRY C");
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&importDryBtn),
            [this](int result)
            {
                if (result == 101) importDryAudio(SourceSlot::dryA);
                if (result == 102) importDryAudio(SourceSlot::dryB);
                if (result == 103) importDryAudio(SourceSlot::dryC);
            });
    }

    void importDryAudio(SourceSlot slot)
    {
        audioChooser = std::make_unique<juce::FileChooser>(
            "Import " + sourceSlotLabel(slot), lastAudioDirectory, "*.wav;*.aiff;*.flac;*.mp3;*.m4a;*.ogg;*.WAV;*.AIFF;*.FLAC;*.MP3");

        audioChooser->launchAsync(juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
            [this, slot](const juce::FileChooser& fc)
            {
                loadDryAssetFromFile(fc.getResult(), slot);
            });
    }

    void loadDryAssetFromFile(const juce::File& file, SourceSlot slot)
    {
        if (file == juce::File{})
            return;

        lastAudioDirectory = file.getParentDirectory();

        Asset loaded;
        juce::String error;
        if (!goodmeter::audio_doctor::readAudioFile(file, loaded, error))
        {
            setStatus(error);
            return;
        }

        assetHolderFor(slot) = std::make_unique<Asset>(std::move(loaded));
        wetAsset.reset();
        wetBAsset.reset();
        wetCAsset.reset();
        renderReferenceA.reset();
        renderReferenceB.reset();
        renderReferenceC.reset();
        hasPluginRenderA = false;
        hasPluginRenderB = false;
        hasPluginRenderC = false;

        if (slot == SourceSlot::dryA)
        {
            if (displaySlots[0] != SourceSlot::dryA)
                displaySlots[0] = SourceSlot::dryA;
        }
        else
        {
            if (dryAsset == nullptr)
            {
                displaySlots[0] = slot;
                setDefaultRenderRoutes(slot);
            }
            else if (displaySlots[1] == SourceSlot::wetA && wetAsset == nullptr)
                displaySlots[1] = slot;
            else if (displaySlots[2] == SourceSlot::wetB && wetBAsset == nullptr)
                displaySlots[2] = slot;
        }

        setStatus(sourceSlotLabel(slot) + " loaded: " + assetHolderFor(slot)->name);
        updateButtonStates();
        repaint();
    }

    void showGenerateMenu()
    {
        if (generateSignalWindow != nullptr)
        {
            generateSignalWindow->toFront(true);
            return;
        }

        auto content = std::make_unique<GenerateSignalComponent>(
            isLightThemeSelected(),
            [this](SourceSlot slot, goodmeter::audio_doctor::GeneratedSignalSpec spec)
            {
                applyGeneratedSignal(slot, spec);
            },
            [this]
            {
                generateSignalWindow.reset();
            });

        generateSignalWindow = std::make_unique<GenerateSignalWindow>(
            std::move(content), isLightThemeSelected(),
            [this]
            {
                generateSignalWindow.reset();
            });
    }

    void applyGeneratedSignal(SourceSlot slot, goodmeter::audio_doctor::GeneratedSignalSpec spec)
    {
        if (!isDrySlot(slot))
            slot = SourceSlot::dryA;

        Asset generated = goodmeter::audio_doctor::makeGeneratedSignalAsset(std::move(spec));
        assetHolderFor(slot) = std::make_unique<Asset>(std::move(generated));

        wetAsset.reset();
        wetBAsset.reset();
        wetCAsset.reset();
        renderReferenceA.reset();
        renderReferenceB.reset();
        renderReferenceC.reset();
        hasPluginRenderA = false;
        hasPluginRenderB = false;
        hasPluginRenderC = false;
        lastLatencySamplesA = 0;
        lastLatencySamplesB = 0;
        lastLatencySamplesC = 0;
        lastTailSecondsA = 0.0;
        lastTailSecondsB = 0.0;
        lastTailSecondsC = 0.0;
        setDefaultRenderRoutes(slot);
        displaySlots = { slot, SourceSlot::wetA, SourceSlot::wetB };
        setStatus("Generated " + sourceSlotLabel(slot) + ": " + assetHolderFor(slot)->name);
        updateButtonStates();
        repaint();
    }

    void showEditAudioMenu()
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel(&audioDoctorPopupLookAndFeel);

        auto addDry = [&](int id, SourceSlot slot)
        {
            const auto* asset = assetFor(slot);
            menu.addItem(id, sourceSlotLabel(slot) + (asset != nullptr ? ("  " + asset->name) : "  empty"),
                         asset != nullptr);
        };

        addDry(201, SourceSlot::dryA);
        addDry(202, SourceSlot::dryB);
        addDry(203, SourceSlot::dryC);
        menu.addSeparator();
        menu.addItem(211, "WET A editing disabled in MVP", false);
        menu.addItem(212, "WET B editing disabled in MVP", false);
        menu.addItem(213, "WET C editing disabled in MVP", false);

        menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&editAudioBtn),
            [this](int result)
            {
                if (result == 201) openAudioEditWindow(SourceSlot::dryA);
                if (result == 202) openAudioEditWindow(SourceSlot::dryB);
                if (result == 203) openAudioEditWindow(SourceSlot::dryC);
            });
    }

    void openAudioEditWindow(SourceSlot slot)
    {
        auto* asset = assetFor(slot);
        if (asset == nullptr)
            return;

        audioEditWindow.reset();
        auto editor = std::make_unique<AudioEditComponent>(
            slot, *asset,
            [this](SourceSlot editedSlot, const juce::var& editSpec)
            {
                commitAudioEdit(editedSlot, editSpec);
                audioEditWindow.reset();
            });

        audioEditWindow = std::make_unique<AudioEditWindow>(
            sourceSlotLabel(slot) + " Editor",
            std::move(editor),
            [this] { audioEditWindow.reset(); });
    }

    void commitAudioEdit(SourceSlot slot, const juce::var& editSpec)
    {
        auto& holder = assetHolderFor(slot);
        if (holder == nullptr)
            return;

        auto derivedDir = exportDirectory.getChildFile("AudioDoctor_Editor_Derived");
        derivedDir.createDirectory();

        juce::String error;
        if (!goodmeter::audio_doctor::applyEditToAsset(*holder, editSpec, derivedDir, sourceSlotLabel(slot), error))
        {
            setStatus(error);
            return;
        }

        if (isDrySlot(slot))
        {
            wetAsset.reset();
            wetBAsset.reset();
            wetCAsset.reset();
            renderReferenceA.reset();
            renderReferenceB.reset();
            renderReferenceC.reset();
            hasPluginRenderA = false;
            hasPluginRenderB = false;
            hasPluginRenderC = false;
        }

        refreshTransferAnalysis();
        setStatus(sourceSlotLabel(slot) + " edited and sent back. Render Plugin A/B/C again for fresh WET analysis.");
        updateButtonStates();
        repaint();
    }

    void openBusRoutingWindow()
    {
        busRoutingWindow.reset();
        auto router = std::make_unique<BusRoutingComponent>(
            displaySlots,
            renderRoutes,
            allowMixedRenderInputs,
            [this](int slotIndex, SourceSlot source)
            {
                if (!juce::isPositiveAndBelow(slotIndex, static_cast<int>(displaySlots.size())))
                    return;

                displaySlots[static_cast<size_t>(slotIndex)] = source;
                setStatus("Display " + juce::String(slotIndex + 1) + " -> " + sourceSlotLabel(source));
                updateButtonStates();
                repaint();
            },
            [this](PluginSlot slot, int dryIndex, bool enabled)
            {
                setRenderRoute(slot, dryIndex, enabled);
                setStatus("Render " + juce::String(slotName(slot)) + " route -> " + renderInputLabel(slot));
                updateButtonStates();
                repaint();
            },
            [this](bool allowMixed)
            {
                setMixedRoutingMode(allowMixed);
                setStatus(allowMixed ? "Bus routing mode: Mix. Multiple DRY sources are summed before rendering."
                                     : "Bus routing mode: Controlled. Each WET renders one DRY source.");
                updateButtonStates();
                repaint();
            },
            isLightThemeSelected(),
            &audioDoctorPopupLookAndFeel);

        busRoutingWindow = std::make_unique<BusRoutingWindow>(
            std::move(router),
            [this] { busRoutingWindow.reset(); });
    }

    void addDisplaySlotMenuItems(juce::PopupMenu& menu, int slotIndex)
    {
        for (int i = 0; i < 6; ++i)
        {
            const auto source = sourceFromMenuIndex(i);
            const auto* asset = assetFor(source);
            menu.addItem(1000 + slotIndex * 10 + i,
                         sourceSlotLabel(source) + (asset != nullptr ? ("  " + asset->name) : "  empty"),
                         asset != nullptr,
                         displaySlots[static_cast<size_t>(slotIndex)] == source);
        }
    }

    void addRenderInputMenuItems(juce::PopupMenu& menu, PluginSlot slot)
    {
        for (int i = 0; i < 3; ++i)
        {
            const auto source = sourceFromMenuIndex(i);
            const auto* asset = assetFor(source);
            menu.addItem(2000 + pluginIndex(slot) * 10 + i,
                         sourceSlotLabel(source) + (asset != nullptr ? ("  " + asset->name) : "  empty"),
                         asset != nullptr,
                         getRenderInputSlot(slot) == source);
        }
    }

    static SourceSlot sourceFromMenuIndex(int index)
    {
        switch (index)
        {
            case 1: return SourceSlot::dryB;
            case 2: return SourceSlot::dryC;
            case 3: return SourceSlot::wetA;
            case 4: return SourceSlot::wetB;
            case 5: return SourceSlot::wetC;
            default: return SourceSlot::dryA;
        }
    }

    static const char* slotName(PluginSlot slot)
    {
        switch (slot)
        {
            case PluginSlot::B: return "B";
            case PluginSlot::C: return "C";
            default: return "A";
        }
    }

    static int pluginIndex(PluginSlot slot)
    {
        switch (slot)
        {
            case PluginSlot::B: return 1;
            case PluginSlot::C: return 2;
            default: return 0;
        }
    }

    static PluginSlot pluginSlotFromIndex(int index)
    {
        switch (index)
        {
            case 1: return PluginSlot::B;
            case 2: return PluginSlot::C;
            default: return PluginSlot::A;
        }
    }

    static SourceSlot sourceForDryIndex(int index)
    {
        switch (index)
        {
            case 1: return SourceSlot::dryB;
            case 2: return SourceSlot::dryC;
            default: return SourceSlot::dryA;
        }
    }

    static int dryIndexForSource(SourceSlot source)
    {
        switch (source)
        {
            case SourceSlot::dryB: return 1;
            case SourceSlot::dryC: return 2;
            default: return 0;
        }
    }

    goodmeter::audio_doctor::PluginHost& getPluginHost(PluginSlot slot)
    {
        switch (slot)
        {
            case PluginSlot::B: return pluginHostB;
            case PluginSlot::C: return pluginHostC;
            default: return pluginHostA;
        }
    }

    std::unique_ptr<PluginEditorWindow>& getPluginEditorWindow(PluginSlot slot)
    {
        switch (slot)
        {
            case PluginSlot::B: return pluginEditorWindowB;
            case PluginSlot::C: return pluginEditorWindowC;
            default: return pluginEditorWindowA;
        }
    }

    bool& getHasPluginRender(PluginSlot slot)
    {
        switch (slot)
        {
            case PluginSlot::B: return hasPluginRenderB;
            case PluginSlot::C: return hasPluginRenderC;
            default: return hasPluginRenderA;
        }
    }

    juce::PluginDescription& getLastPluginDescription(PluginSlot slot)
    {
        switch (slot)
        {
            case PluginSlot::B: return lastPluginDescriptionB;
            case PluginSlot::C: return lastPluginDescriptionC;
            default: return lastPluginDescriptionA;
        }
    }

    int& getLastLatencySamples(PluginSlot slot)
    {
        switch (slot)
        {
            case PluginSlot::B: return lastLatencySamplesB;
            case PluginSlot::C: return lastLatencySamplesC;
            default: return lastLatencySamplesA;
        }
    }

    double& getLastTailSeconds(PluginSlot slot)
    {
        switch (slot)
        {
            case PluginSlot::B: return lastTailSecondsB;
            case PluginSlot::C: return lastTailSecondsC;
            default: return lastTailSecondsA;
        }
    }

    std::unique_ptr<Asset>& getRenderReference(PluginSlot slot)
    {
        switch (slot)
        {
            case PluginSlot::B: return renderReferenceB;
            case PluginSlot::C: return renderReferenceC;
            default: return renderReferenceA;
        }
    }

    const std::unique_ptr<Asset>& getRenderReference(PluginSlot slot) const
    {
        switch (slot)
        {
            case PluginSlot::B: return renderReferenceB;
            case PluginSlot::C: return renderReferenceC;
            default: return renderReferenceA;
        }
    }

    std::vector<SourceSlot> getRenderInputSlots(PluginSlot slot) const
    {
        std::vector<SourceSlot> sources;
        const auto row = static_cast<size_t>(pluginIndex(slot));
        for (int dry = 0; dry < 3; ++dry)
        {
            if (renderRoutes[row][static_cast<size_t>(dry)])
                sources.push_back(sourceForDryIndex(dry));
        }
        return sources;
    }

    juce::String renderInputLabel(PluginSlot slot) const
    {
        const auto sources = getRenderInputSlots(slot);
        if (sources.empty())
            return "none";

        juce::StringArray labels;
        for (auto source : sources)
            labels.add(sourceSlotLabel(source));
        return labels.joinIntoString("+");
    }

    bool hasRenderInputAsset(PluginSlot slot) const
    {
        for (auto source : getRenderInputSlots(slot))
            if (assetFor(source) != nullptr)
                return true;
        return false;
    }

    SourceSlot getRenderInputSlot(PluginSlot slot) const
    {
        const auto sources = getRenderInputSlots(slot);
        return sources.empty() ? SourceSlot::dryA : sources.front();
    }

    void setRenderInputSlot(PluginSlot slot, SourceSlot source)
    {
        const auto row = static_cast<size_t>(pluginIndex(slot));
        for (auto& enabled : renderRoutes[row])
            enabled = false;
        renderRoutes[row][static_cast<size_t>(dryIndexForSource(source))] = true;
    }

    Asset* renderInputAsset(PluginSlot slot) const
    {
        return assetFor(getRenderInputSlot(slot));
    }

    void setRenderRoute(PluginSlot slot, int dryIndex, bool enabled)
    {
        if (!juce::isPositiveAndBelow(dryIndex, 3))
            return;

        auto& row = renderRoutes[static_cast<size_t>(pluginIndex(slot))];
        if (!allowMixedRenderInputs)
        {
            if (!enabled)
                return;

            for (int col = 0; col < 3; ++col)
                row[static_cast<size_t>(col)] = col == dryIndex;
        }
        else
        {
            row[static_cast<size_t>(dryIndex)] = enabled;
        }

        invalidateWetForPlugin(slot);
        getRenderReference(slot).reset();
        refreshTransferAnalysis();
    }

    void setDefaultRenderRoutes(SourceSlot source)
    {
        const auto dryIndex = dryIndexForSource(source);
        for (auto& row : renderRoutes)
        {
            for (int col = 0; col < 3; ++col)
                row[static_cast<size_t>(col)] = col == dryIndex;
        }
    }

    void setMixedRoutingMode(bool allowMixed)
    {
        allowMixedRenderInputs = allowMixed;
        if (!allowMixedRenderInputs)
        {
            for (auto& row : renderRoutes)
            {
                int active = -1;
                for (int col = 0; col < 3; ++col)
                    if (row[static_cast<size_t>(col)])
                    {
                        active = col;
                        break;
                    }

                if (active < 0)
                    active = 0;

                for (int col = 0; col < 3; ++col)
                    row[static_cast<size_t>(col)] = col == active;
            }
        }

        wetAsset.reset();
        wetBAsset.reset();
        wetCAsset.reset();
        renderReferenceA.reset();
        renderReferenceB.reset();
        renderReferenceC.reset();
        hasPluginRenderA = false;
        hasPluginRenderB = false;
        hasPluginRenderC = false;
        refreshTransferAnalysis();
    }

    std::unique_ptr<Asset> makeRenderInputAsset(PluginSlot slot, juce::String& error) const
    {
        std::vector<const Asset*> inputs;
        juce::StringArray labels;

        for (auto source : getRenderInputSlots(slot))
        {
            if (const auto* asset = assetFor(source))
            {
                inputs.push_back(asset);
                labels.add(sourceSlotLabel(source));
            }
        }

        if (inputs.empty())
        {
            error = "Load at least one routed DRY source before Render " + juce::String(slotName(slot)) + ".";
            return {};
        }

        if (inputs.size() == 1)
            return std::make_unique<Asset>(*inputs.front());

        const double sampleRate = inputs.front()->sampleRate;
        int maxSamples = 0;
        for (const auto* asset : inputs)
        {
            if (std::abs(asset->sampleRate - sampleRate) > 1.0)
            {
                error = "Multi-input render currently requires matching sample rates.";
                return {};
            }
            maxSamples = juce::jmax(maxSamples, asset->buffer.getNumSamples());
        }

        Asset mixed;
        mixed.name = "Plugin " + juce::String(slotName(slot)) + " input " + labels.joinIntoString("+");
        mixed.sourcePath = "mixed:" + labels.joinIntoString("+");
        mixed.sampleRate = sampleRate;
        mixed.buffer.setSize(2, maxSamples);
        mixed.buffer.clear();

        const float gain = 1.0f / static_cast<float>(inputs.size());
        for (const auto* asset : inputs)
        {
            auto stereo = goodmeter::audio_doctor::toStereoBuffer(asset->buffer);
            const int samples = juce::jmin(maxSamples, stereo.getNumSamples());
            for (int ch = 0; ch < mixed.buffer.getNumChannels(); ++ch)
                mixed.buffer.addFrom(ch, 0, stereo, ch, 0, samples, gain);
        }

        goodmeter::audio_doctor::refreshAnalysis(mixed);
        return std::make_unique<Asset>(std::move(mixed));
    }

    void invalidateWetForPlugin(PluginSlot slot)
    {
        if (slot == PluginSlot::A)
        {
            wetAsset.reset();
            renderReferenceA.reset();
            hasPluginRenderA = false;
            lastLatencySamplesA = 0;
            lastTailSecondsA = 0.0;
        }
        else if (slot == PluginSlot::B)
        {
            wetBAsset.reset();
            renderReferenceB.reset();
            hasPluginRenderB = false;
            lastLatencySamplesB = 0;
            lastTailSecondsB = 0.0;
        }
        else
        {
            wetCAsset.reset();
            renderReferenceC.reset();
            hasPluginRenderC = false;
            lastLatencySamplesC = 0;
            lastTailSecondsC = 0.0;
        }
    }

    static bool isSupportedPluginBundle(const juce::File& file)
    {
        const auto ext = file.getFileExtension().toLowerCase();
        return ext == ".vst3" || ext == ".component";
    }

    void refreshPluginSlotLabel()
    {
        juce::String text = "Plugins: ";
        const bool hasA = pluginHostA.getCurrentPlugin() != nullptr;
        const bool hasB = pluginHostB.getCurrentPlugin() != nullptr;
        const bool hasC = pluginHostC.getCurrentPlugin() != nullptr;

        if (!hasA && !hasB && !hasC)
            text += "none";
        else
        {
            if (hasA)
                text += "A " + pluginHostA.getCurrentPluginName();
            if (hasA && hasB)
                text += "    ";
            if (hasB)
                text += "B " + pluginHostB.getCurrentPluginName();
            if ((hasA || hasB) && hasC)
                text += "    ";
            if (hasC)
                text += "C " + pluginHostC.getCurrentPluginName();
        }

        pluginSlotLabel.setText(text, juce::dontSendNotification);
    }

    void choosePlugin(PluginSlot slot)
    {
        pluginChooser = std::make_unique<juce::FileChooser>(
            "Choose AU/VST3 Plugin " + juce::String(slotName(slot)),
            lastPluginDirectory,
            "*.vst3;*.component");

        pluginChooser->launchAsync(juce::FileBrowserComponent::openMode
                                 | juce::FileBrowserComponent::canSelectFiles
                                 | juce::FileBrowserComponent::canSelectDirectories,
            [this, slot](const juce::FileChooser& fc)
            {
                const auto file = fc.getResult();
                if (file == juce::File{})
                    return;

                if (!isSupportedPluginBundle(file))
                {
                    lastPluginDirectory = file.isDirectory() ? file : file.getParentDirectory();
                    setStatus("Select one .vst3 or .component plugin bundle, then click Open.");
                    return;
                }

                lastPluginDirectory = file.getParentDirectory();
                const auto message = "Load Plugin " + juce::String(slotName(slot)) + ": "
                                   + file.getFileName() + "?";
                juce::AlertWindow::showOkCancelBox(
                    juce::AlertWindow::QuestionIcon,
                    "Confirm plugin load",
                    message,
                    "Load",
                    "Cancel",
                    this,
                    juce::ModalCallbackFunction::create([this, slot, file](int result)
                    {
                        if (result != 0)
                            loadPluginFromFile(slot, file);
                    }));
            });
    }

    void loadPluginFromFile(PluginSlot slot, const juce::File& file)
    {
        if (rendering.load())
            return;

        suspendOtherPluginSlots(slot);
        closePluginEditorWindow(slot);

        juce::String error;
        auto& host = getPluginHost(slot);
        if (!host.loadPluginFromFile(file, error))
        {
            setStatus(error);
            return;
        }

        setStatus("Plugin " + juce::String(slotName(slot)) + " loaded: " + host.getCurrentPluginName());
        refreshPluginSlotLabel();
        updateButtonStates();
        showPluginEditor(slot);
        repaint();
    }

    void showPluginEditor(PluginSlot slot)
    {
        auto& host = getPluginHost(slot);
        if (host.getCurrentPlugin() == nullptr)
        {
            setStatus("Choose Plugin " + juce::String(slotName(slot)) + " first.");
            return;
        }

        suspendOtherPluginSlots(slot);
        closePluginEditorWindow(slot);

        juce::String error;
        auto editor = host.createEditorForCurrentPlugin(error);
        if (editor == nullptr)
        {
            setStatus(error.isNotEmpty() ? error : "Plugin editor could not be created.");
            return;
        }

        if (editor->getWidth() <= 0 || editor->getHeight() <= 0)
            editor->setSize(620, 360);

        auto& editorWindow = getPluginEditorWindow(slot);
        editorWindow = std::make_unique<PluginEditorWindow>(
            "Plugin " + juce::String(slotName(slot)) + ": " + host.getCurrentPluginName(),
            std::move(editor),
            [this, slot] { closePluginEditorWindow(slot); });

        setStatus("Editing Plugin " + juce::String(slotName(slot)) + ": " + host.getCurrentPluginName());
    }

    void closePluginEditorWindow()
    {
        closePluginEditorWindow(PluginSlot::A);
        closePluginEditorWindow(PluginSlot::B);
        closePluginEditorWindow(PluginSlot::C);
    }

    void closePluginEditorWindow(PluginSlot slot)
    {
        getPluginEditorWindow(slot).reset();
    }

    void suspendPluginSlot(PluginSlot slot)
    {
        closePluginEditorWindow(slot);

        juce::String ignoredError;
        getPluginHost(slot).suspendEditableInstance(ignoredError);
    }

    void suspendOtherPluginSlots(PluginSlot activeSlot)
    {
        for (auto slot : { PluginSlot::A, PluginSlot::B, PluginSlot::C })
            if (slot != activeSlot)
                suspendPluginSlot(slot);
    }

    void renderWetWithPlugin(PluginSlot slot)
    {
        auto& host = getPluginHost(slot);
        juce::String inputError;
        auto inputAsset = makeRenderInputAsset(slot, inputError);
        if (rendering.load() || inputAsset == nullptr || host.getCurrentPlugin() == nullptr)
        {
            if (inputAsset == nullptr)
                setStatus(inputError.isNotEmpty() ? inputError : "Choose a routed DRY source before Render " + juce::String(slotName(slot)) + ".");
            return;
        }

        if (renderThread.joinable())
            renderThread.join();

        closePluginEditorWindow();

        juce::String stateError;
        juce::MemoryBlock pluginState;
        if (!host.captureCurrentState(pluginState, stateError))
        {
            setStatus(stateError);
            return;
        }

        suspendPluginSlot(PluginSlot::A);
        suspendPluginSlot(PluginSlot::B);
        suspendPluginSlot(PluginSlot::C);

        rendering.store(true);
        updateButtonStates();
        const auto inputLabel = renderInputLabel(slot);
        setStatus("Rendering Plugin " + juce::String(slotName(slot)) + " from " + inputLabel + "...");

        auto dryCopy = std::move(*inputAsset);
        auto reference = std::make_shared<Asset>(dryCopy);
        auto safeFlag = aliveFlag;

        renderThread = std::thread([this,
                                     slot,
                                     inputLabel,
                                     dryCopy = std::move(dryCopy),
                                     reference,
                                     pluginState = std::move(pluginState),
                                     safeFlag]() mutable
        {
            const auto* stateToApply = pluginState.getSize() > 0 ? &pluginState : nullptr;
            auto result = std::make_shared<goodmeter::audio_doctor::OfflineRenderResult>(
                getPluginHost(slot).renderOffline(dryCopy, stateToApply));

            juce::MessageManager::callAsync([this, slot, inputLabel, reference, safeFlag, result]()
            {
                if (!safeFlag->load())
                    return;

                rendering.store(false);

                if (result->error.isNotEmpty())
                {
                    setStatus(result->error);
                }
                else
                {
                    if (slot == PluginSlot::A)
                        wetAsset = std::make_unique<Asset>(std::move(result->wet));
                    else if (slot == PluginSlot::B)
                        wetBAsset = std::make_unique<Asset>(std::move(result->wet));
                    else
                        wetCAsset = std::make_unique<Asset>(std::move(result->wet));

                    getRenderReference(slot) = std::make_unique<Asset>(*reference);
                    refreshTransferAnalysis();
                    getLastLatencySamples(slot) = result->latencySamples;
                    getLastTailSeconds(slot) = result->tailSeconds;
                    getLastPluginDescription(slot) = result->pluginDescription;
                    getHasPluginRender(slot) = true;

                    setStatus("Rendered Wet " + juce::String(slotName(slot)) + " from " + inputLabel + ": "
                              + getLastPluginDescription(slot).name
                              + " | latency compensated "
                              + juce::String(getLastLatencySamples(slot)) + " samples");
                }

                updateButtonStates();
                repaint();
            });
        });
    }

    void exportFigure()
    {
        if (!hasAnySourceAsset())
        {
            setStatus("Nothing to export yet.");
            return;
        }

        auto dir = exportDirectory.getChildFile("AudioDoctor_Exports");
        if (!dir.exists() && !dir.createDirectory())
        {
            setStatus("Could not create export folder: " + dir.getFullPathName());
            return;
        }

        const auto stamp = juce::Time::getCurrentTime().formatted("%Y%m%d_%H%M%S");
        const auto baseName = makeExportBaseName(stamp);
        auto darkPngFile = dir.getChildFile(baseName + "_Dark").withFileExtension(".png");
        auto lightPngFile = dir.getChildFile(baseName + "_Light").withFileExtension(".png");
        auto jsonFile = dir.getChildFile(baseName).withFileExtension(".json");

        juce::PNGImageFormat png;
        if (!writeExportImage(darkPngFile, false, png))
        {
            setStatus("Dark PNG export failed.");
            return;
        }
        if (!writeExportImage(lightPngFile, true, png))
        {
            setStatus("Light PNG export failed.");
            return;
        }

        auto dataDir = dir.getChildFile(baseName + "_data");
        if (!dataDir.exists())
            dataDir.createDirectory();

        juce::Array<juce::var> dataFiles;
        goodmeter::audio_doctor::writeAssetCurves(dataDir, "dry",  dryAsset.get(),   dataFiles);
        goodmeter::audio_doctor::writeAssetCurves(dataDir, "dryB", dryBAsset.get(),  dataFiles);
        goodmeter::audio_doctor::writeAssetCurves(dataDir, "dryC", dryCAsset.get(),  dataFiles);
        goodmeter::audio_doctor::writeAssetCurves(dataDir, "wetA", wetAsset.get(),   dataFiles);
        goodmeter::audio_doctor::writeAssetCurves(dataDir, "wetB", wetBAsset.get(),  dataFiles);
        goodmeter::audio_doctor::writeAssetCurves(dataDir, "wetC", wetCAsset.get(),  dataFiles);
        for (int i = 0; i < 3; ++i)
            goodmeter::audio_doctor::writeAssetCurves(dataDir, "display" + juce::String(i + 1), displayAsset(i), dataFiles);

        goodmeter::audio_doctor::writeApparentAttenuationCsv(dataDir, "display2_vs_display1",
                                                             displayAsset(0), displayAsset(1), dataFiles);
        goodmeter::audio_doctor::writeApparentAttenuationCsv(dataDir, "display3_vs_display1",
                                                             displayAsset(0), displayAsset(2), dataFiles);
        auto writeRouteApparentCsv = [this, &dataDir, &dataFiles](PluginSlot slot, const juce::String& role, const Asset* target)
        {
            const auto& renderedReference = getRenderReference(slot);
            const Asset* reference = renderedReference != nullptr ? renderedReference.get() : renderInputAsset(slot);
            goodmeter::audio_doctor::writeApparentAttenuationCsv(dataDir, role, reference, target, dataFiles);
        };
        writeRouteApparentCsv(PluginSlot::A, "wetA_vs_render_reference", wetAsset.get());
        writeRouteApparentCsv(PluginSlot::B, "wetB_vs_render_reference", wetBAsset.get());
        writeRouteApparentCsv(PluginSlot::C, "wetC_vs_render_reference", wetCAsset.get());

        writeManifest(jsonFile, darkPngFile, lightPngFile, dataFiles);
        setStatus("Exported Dark/Light: " + baseName);
        darkPngFile.revealToUser();
    }

    bool writeExportImage(const juce::File& pngFile, bool lightTheme, juce::PNGImageFormat& png)
    {
        juce::ignoreUnused(png);

        constexpr int scale = 2;
        const auto sourceBounds = getFigureBounds().withTrimmedTop(0);
        const int imageW = juce::jmax(1800, sourceBounds.getWidth() * scale);
        const int imageH = juce::jmax(900, static_cast<int>(static_cast<float>(imageW) * 0.50f));
        return goodmeter::audio_doctor::AudioDoctorFigureRenderer::writePng(
            pngFile, makeFigureDataForExport(), !lightTheme, imageW, imageH);
    }

    void resetAll()
    {
        dryAsset.reset();
        dryBAsset.reset();
        dryCAsset.reset();
        wetAsset.reset();
        wetBAsset.reset();
        wetCAsset.reset();
        generateSignalWindow.reset();
        audioEditWindow.reset();
        busRoutingWindow.reset();
        closePluginEditorWindow();
        pluginHostA.clearPlugin();
        pluginHostB.clearPlugin();
        pluginHostC.clearPlugin();
        refreshPluginSlotLabel();
        hasPluginRenderA = false;
        hasPluginRenderB = false;
        hasPluginRenderC = false;
        lastLatencySamplesA = 0;
        lastLatencySamplesB = 0;
        lastLatencySamplesC = 0;
        lastTailSecondsA = 0.0;
        lastTailSecondsB = 0.0;
        lastTailSecondsC = 0.0;
        renderReferenceA.reset();
        renderReferenceB.reset();
        renderReferenceC.reset();
        allowMixedRenderInputs = false;
        setDefaultRenderRoutes(SourceSlot::dryA);
        displaySlots = { SourceSlot::dryA, SourceSlot::wetA, SourceSlot::wetB };
        setStatus("Reset. Load Dry audio, generate a signal, or choose Plugin A/B/C.");
        updateButtonStates();
        repaint();
    }

    static juce::String sanitizeFileToken(juce::String text)
    {
        text = text.trim();
        if (text.isEmpty())
            text = "Untitled";

        const juce::String illegal = "\\/:*?\"<>|";
        for (int i = 0; i < illegal.length(); ++i)
            text = text.replaceCharacter(illegal[i], '_');

        text = text.replace(" ", "_")
                   .replace("\t", "_")
                   .replace("\n", "_")
                   .replace("\r", "_");

        while (text.contains("__"))
            text = text.replace("__", "_");

        if (text.length() > 64)
            text = text.substring(0, 64);

        return text.trimCharactersAtEnd("_");
    }

    juce::String getDryExportName() const
    {
        const auto* primary = displayAsset(0) != nullptr ? displayAsset(0) : dryAsset.get();
        if (primary == nullptr)
            return "NoDry";

        if (primary->sourcePath.isNotEmpty())
            return juce::File(primary->sourcePath).getFileNameWithoutExtension();

        return primary->name;
    }

    juce::String getViewExportName() const
    {
        switch (viewMode.getSelectedId())
        {
            case 2:  return "TransientEnvelope";
            case 3:  return "GroupDelay";
            case 4:  return "SpectrogramABC";
            case 5:  return "ReverbSpace";
            case 6:  return "DynamicsResponse";
            default: return "SpectrumOverlay";
        }
    }

    juce::String makeExportBaseName(const juce::String& stamp) const
    {
        return "AUDIODOCTOR_"
            + sanitizeFileToken(getDryExportName()) + "_"
            + sanitizeFileToken(getViewExportName()) + "_"
            + stamp;
    }

    goodmeter::audio_doctor::FigureData makeFigureDataForExport()
    {
        goodmeter::audio_doctor::FigureData data;
        data.dry = displayAsset(0);
        data.wetA = displayAsset(1);
        data.wetB = displayAsset(2);
        data.label1 = displayLabel(0);
        data.label2 = displayLabel(1);
        data.label3 = displayLabel(2);
        data.pluginA = makeFigurePluginInfo(PluginSlot::A);
        data.pluginB = makeFigurePluginInfo(PluginSlot::B);
        data.pluginC = makeFigurePluginInfo(PluginSlot::C);
        data.view = getFigureExportView();
        data.viewToken = getViewExportName();
        return data;
    }

    goodmeter::audio_doctor::FigureView getFigureExportView() const
    {
        switch (viewMode.getSelectedId())
        {
            case 2:  return goodmeter::audio_doctor::FigureView::envelope;
            case 3:  return goodmeter::audio_doctor::FigureView::groupDelay;
            case 4:  return goodmeter::audio_doctor::FigureView::spectrogramABC;
            case 5:  return goodmeter::audio_doctor::FigureView::reverbSpace;
            case 6:  return goodmeter::audio_doctor::FigureView::dynamics;
            default: return goodmeter::audio_doctor::FigureView::spectrum;
        }
    }

    goodmeter::audio_doctor::FigurePluginInfo makeFigurePluginInfo(PluginSlot slot)
    {
        auto& host = getPluginHost(slot);
        host.refreshChangedParameterSnapshot();

        goodmeter::audio_doctor::FigurePluginInfo info;
        if (host.getCurrentPlugin() == nullptr)
            return info;

        const auto* desc = host.getCurrentPlugin();
        info.valid = true;
        info.name = desc->name;
        info.format = desc->pluginFormatName;
        info.latencySamples = getHasPluginRender(slot) ? getLastLatencySamples(slot) : 0;
        info.tailSeconds = getHasPluginRender(slot) ? getLastTailSeconds(slot) : 0.0;

        for (const auto& p : host.getChangedParameters())
            info.changedParameters.push_back({ p.name, p.valueText, p.normalisedValue });

        return info;
    }

    void updateButtonStates()
    {
        const bool hasDry = dryAsset != nullptr || dryBAsset != nullptr || dryCAsset != nullptr;
        const bool hasWet = wetAsset != nullptr || wetBAsset != nullptr || wetCAsset != nullptr;
        const bool hasPluginA = pluginHostA.getCurrentPlugin() != nullptr;
        const bool hasPluginB = pluginHostB.getCurrentPlugin() != nullptr;
        const bool hasPluginC = pluginHostC.getCurrentPlugin() != nullptr;
        const bool hasRenderInputA = hasRenderInputAsset(PluginSlot::A);
        const bool hasRenderInputB = hasRenderInputAsset(PluginSlot::B);
        const bool hasRenderInputC = hasRenderInputAsset(PluginSlot::C);
        const bool busy = rendering.load();

        renderBtn.setEnabled(hasRenderInputA && hasPluginA && !busy);
        editPluginBtn.setEnabled(hasPluginA && !busy);
        renderBBtn.setEnabled(hasRenderInputB && hasPluginB && !busy);
        editPluginBBtn.setEnabled(hasPluginB && !busy);
        renderCBtn.setEnabled(hasRenderInputC && hasPluginC && !busy);
        editPluginCBtn.setEnabled(hasPluginC && !busy);
        exportBtn.setEnabled(hasDry && !busy);
        editAudioBtn.setEnabled(hasDry && !busy);
        busBtn.setEnabled((hasDry || hasWet) && !busy);
        resetBtn.setEnabled((hasDry || hasWet || hasPluginA || hasPluginB || hasPluginC) && !busy);
        pluginBtn.setEnabled(!busy);
        pluginBBtn.setEnabled(!busy);
        pluginCBtn.setEnabled(!busy);
        importDryBtn.setEnabled(!busy);
        generateBtn.setEnabled(!busy);
    }

    void setStatus(const juce::String& text)
    {
        statusLabel.setText(text, juce::dontSendNotification);
    }

    void refreshTransferAnalysis()
    {
        auto refreshFor = [this](std::unique_ptr<Asset>& wet, PluginSlot slot)
        {
            if (wet == nullptr)
                return;

            const auto& renderedReference = getRenderReference(slot);
            const auto* reference = renderedReference != nullptr ? renderedReference.get() : renderInputAsset(slot);
            if (reference == nullptr)
                return;

            const double sampleRate = reference->sampleRate > 0.0 ? reference->sampleRate : wet->sampleRate;
            wet->groupDelay = goodmeter::audio_doctor::computeTransferGroupDelay(reference->buffer,
                                                                                 wet->buffer,
                                                                                 sampleRate);
        };

        refreshFor(wetAsset, PluginSlot::A);
        refreshFor(wetBAsset, PluginSlot::B);
        refreshFor(wetCAsset, PluginSlot::C);
    }

    //==========================================================================
    void drawFigure(juce::Graphics& g, juce::Rectangle<float> area, bool exportMode)
    {
        if (exportMode)
        {
            g.setColour(isLightFigure(exportMode) ? juce::Colour(0xFFF7F8FA)
                                                  : juce::Colour(0xFF07080B));
            g.fillRect(area);
        }

        auto header = area.removeFromTop(exportMode ? 82.0f : 56.0f);
        drawHeader(g, header, exportMode);

        const bool isGroupDelayView = viewMode.getSelectedId() == 3;
        const bool isReverbSpaceView = viewMode.getSelectedId() == 5;
        const bool isDynamicsView = viewMode.getSelectedId() == 6;
        const bool isSpectrogramView = viewMode.getSelectedId() == 4;
        const bool isDenseMetrics = isGroupDelayView || isReverbSpaceView || isDynamicsView;
        auto metricsArea = area.removeFromBottom(isDenseMetrics
            ? (exportMode ? 205.0f : 126.0f)
            : isSpectrogramView ? (exportMode ? 136.0f : 92.0f)
                                : (exportMode ? 155.0f : 104.0f));
        area.reduce(0.0f, exportMode ? 20.0f : 12.0f);

        if (!hasAnySourceAsset())
        {
            g.setColour(isLightFigure(exportMode) ? juce::Colour(0xFF6A7078) : mutedUiText());
            g.setFont(juce::Font(juce::FontOptions(exportMode ? 32.0f : 18.0f)));
            g.drawText("Audio Doctor", area, juce::Justification::centred);
            return;
        }

        auto plotArea = area;
        plotArea.removeFromLeft(exportMode ? 92.0f : 62.0f);
        plotArea.removeFromRight(exportMode ? 24.0f : 14.0f);
        plotArea.removeFromBottom(exportMode ? 42.0f : 30.0f);
        plotArea.removeFromTop(exportMode ? 8.0f : 4.0f);

        if (plotArea.getWidth() < 120.0f || plotArea.getHeight() < 90.0f)
            plotArea = area.reduced(8.0f);

        switch (viewMode.getSelectedId())
        {
            case 2:  drawEnvelopePlot(g, plotArea, exportMode); break;
            case 3:  drawGroupDelayPlot(g, plotArea, exportMode); break;
            case 4:  drawWaterfallAB(g, plotArea, exportMode); break;
            case 5:  drawReverbSpacePlot(g, plotArea, exportMode); break;
            case 6:  drawDynamicsPlot(g, plotArea, exportMode); break;
            default: drawSpectrumPlot(g, plotArea, exportMode); break;
        }

        drawMetrics(g, metricsArea, exportMode);
    }

    void drawHeader(juce::Graphics& g, juce::Rectangle<float> header, bool exportMode)
    {
        g.setColour(isLightFigure(exportMode) ? juce::Colour(0xFF161A21) : uiText());
        g.setFont(juce::Font(juce::FontOptions(exportMode ? 34.0f : 20.0f)).boldened());

        juce::String title;
        switch (viewMode.getSelectedId())
        {
            case 2:  title = juce::String::fromUTF8("瞬态包络对比 / Transient Envelope"); break;
            case 3:  title = juce::String::fromUTF8("群延时 / Group Delay"); break;
            case 4:  title = juce::String::fromUTF8("时频谱 A/B/C / Spectrogram A/B/C"); break;
            case 5:  title = juce::String::fromUTF8("混响空间 / Reverb Space"); break;
            case 6:  title = juce::String::fromUTF8("动态响应 / Dynamics Response"); break;
            default: title = juce::String::fromUTF8("频谱对比 / Spectrum Overlay"); break;
        }

        g.drawText(title, header.removeFromTop(exportMode ? 44.0f : 28.0f),
                   juce::Justification::centredLeft);

        g.setFont(juce::Font(juce::FontOptions(exportMode ? 20.0f : 13.0f)));
        g.setColour(secondaryDetailText(isLightFigure(exportMode)));

        juce::String subtitle;
        for (int i = 0; i < 3; ++i)
            if (auto* asset = displayAsset(i))
            {
                if (subtitle.isNotEmpty())
                    subtitle += "    ";
                subtitle += displayLabel(i) + ": " + asset->name;
            }
        if (subtitle.isEmpty())
            subtitle = "Dry: none";
        if (pluginHostA.getCurrentPlugin() != nullptr)
            subtitle += "    Plugin A: " + pluginHostA.getCurrentPluginName();
        if (pluginHostB.getCurrentPlugin() != nullptr)
            subtitle += "    Plugin B: " + pluginHostB.getCurrentPluginName();
        if (pluginHostC.getCurrentPlugin() != nullptr)
            subtitle += "    Plugin C: " + pluginHostC.getCurrentPluginName();

        g.drawText(subtitle, header, juce::Justification::centredLeft);
    }

    void drawSpectrumPlot(juce::Graphics& g, juce::Rectangle<float> plot, bool exportMode)
    {
        drawPlotFrame(g, plot, exportMode, juce::String::fromUTF8("频率 (Hz)"), juce::String::fromUTF8("幅度 (dB)"));
        drawSpectrumGrid(g, plot, exportMode);

        if (auto* a0 = displayAsset(0))
            drawSpectrumPath(g, plot, a0->spectrum, dryColour(exportMode), exportMode);
        if (auto* a1 = displayAsset(1))
            drawSpectrumPath(g, plot, a1->spectrum, wetColour(exportMode), exportMode);
        if (auto* a2 = displayAsset(2))
            drawSpectrumPath(g, plot, a2->spectrum, wetBColour(exportMode), exportMode);

        drawHarmonicPeakOverlay(g, plot, exportMode);

        if (auto* a0 = displayAsset(0); a0 != nullptr && a0->harmonicPeaks.empty() && a0->name.contains("100 Hz"))
            drawHarmonicMarkers(g, plot, 100.0f, exportMode);

        drawLegend(g, plot, exportMode);
    }

    void drawGroupDelayPlot(juce::Graphics& g, juce::Rectangle<float> plot, bool exportMode)
    {
        drawPlotFrame(g, plot, exportMode, juce::String::fromUTF8("频率 (Hz)"), "Group delay (ms)");
        drawSpectrumGrid(g, plot, exportMode);
        drawGroupDelayGrid(g, plot, exportMode);

        const bool hasWetA = displayAsset(1) != nullptr && !displayAsset(1)->groupDelay.empty();
        const bool hasWetB = displayAsset(2) != nullptr && !displayAsset(2)->groupDelay.empty();
        if (!hasWetA && !hasWetB)
        {
            drawPlotHint(g, plot, "Load matching Dry/Wet or render a plugin to show group delay.");
            return;
        }

        drawGroupDelayReference(g, plot, exportMode);
        if (hasWetA)
            drawGroupDelayPath(g, plot, displayAsset(1)->groupDelay, wetColour(exportMode), exportMode);
        if (hasWetB)
            drawGroupDelayPath(g, plot, displayAsset(2)->groupDelay, wetBColour(exportMode), exportMode);
        drawLegend(g, plot, exportMode);
    }

    void drawEnvelopePlot(juce::Graphics& g, juce::Rectangle<float> plot, bool exportMode)
    {
        drawPlotFrame(g, plot, exportMode, juce::String::fromUTF8("时间 (s)"), juce::String::fromUTF8("峰值包络 (dB)"));
        drawEnvelopeGrid(g, plot, exportMode);

        const float maxTime = getMaxEnvelopeTime();
        if (auto* a0 = displayAsset(0))
            drawEnvelopePath(g, plot, a0->envelope, maxTime, dryColour(exportMode), exportMode);
        if (auto* a1 = displayAsset(1))
            drawEnvelopePath(g, plot, a1->envelope, maxTime, wetColour(exportMode), exportMode);
        if (auto* a2 = displayAsset(2))
            drawEnvelopePath(g, plot, a2->envelope, maxTime, wetBColour(exportMode), exportMode);

        drawLegend(g, plot, exportMode);
    }

    void drawWaterfallAB(juce::Graphics& g, juce::Rectangle<float> plot, bool exportMode)
    {
        if (displayAsset(0) == nullptr || displayAsset(0)->spectrogramBlue.isNull())
        {
            drawPlotHint(g, plot, "No spectrogram data.");
            return;
        }

        struct SpectrogramTrack
        {
            const juce::Image* image = nullptr;
            double sampleRate = 0.0;
            float durationSeconds = 0.0f;
            juce::String label;
        };

        std::vector<SpectrogramTrack> tracks;
        if (auto* a0 = displayAsset(0); a0 != nullptr && !a0->spectrogramBlue.isNull())
            tracks.push_back({ &a0->spectrogramBlue, a0->sampleRate,
                               static_cast<float>(a0->metrics.durationSeconds), displayLabel(0) });
        if (auto* a1 = displayAsset(1); a1 != nullptr && !a1->spectrogramYellow.isNull())
            tracks.push_back({ &a1->spectrogramYellow, a1->sampleRate,
                               static_cast<float>(a1->metrics.durationSeconds), displayLabel(1) });
        if (auto* a2 = displayAsset(2); a2 != nullptr && !a2->spectrogramPink.isNull())
            tracks.push_back({ &a2->spectrogramPink, a2->sampleRate,
                               static_cast<float>(a2->metrics.durationSeconds), displayLabel(2) });

        float maxDurationSeconds = 0.001f;
        for (const auto& track : tracks)
            maxDurationSeconds = juce::jmax(maxDurationSeconds, track.durationSeconds);

        const float gap = tracks.size() > 1 ? (exportMode ? 18.0f : 12.0f) : 0.0f;
        const float trackHeight = (plot.getHeight() - gap * static_cast<float>(tracks.size() - 1))
                                / static_cast<float>(tracks.size());

        auto remaining = plot;
        for (const auto& track : tracks)
        {
            auto area = remaining.removeFromTop(trackHeight);
            if (track.image != nullptr)
                drawSpectrogramPanel(g, area, *track.image, track.sampleRate,
                                     track.durationSeconds, maxDurationSeconds,
                                     track.label, exportMode);

            remaining.removeFromTop(gap);
        }
    }

    void drawReverbSpacePlot(juce::Graphics& g, juce::Rectangle<float> plot, bool exportMode)
    {
        if (displayAsset(0) == nullptr || displayAsset(0)->energyDecay.empty())
        {
            drawPlotHint(g, plot, "No reverb or decay data.");
            return;
        }

        auto edcPlot = plot.removeFromTop(plot.getHeight() * 0.56f);
        plot.removeFromTop(exportMode ? 24.0f : 14.0f);
        auto spectrogramArea = plot;

        drawPlotFrame(g, edcPlot, exportMode, juce::String::fromUTF8("时间 (s)"), "EDC / decay (dB)");
        drawTimeDbGrid(g, edcPlot, getMaxDecayTime(), exportMode);

        const float maxTime = getMaxDecayTime();
        if (auto* a0 = displayAsset(0))
            drawTimeDbPath(g, edcPlot, a0->energyDecay, maxTime, dryColour(exportMode), exportMode, -80.0f, 0.0f);
        if (auto* a1 = displayAsset(1))
            drawTimeDbPath(g, edcPlot, a1->energyDecay, maxTime, wetColour(exportMode), exportMode, -80.0f, 0.0f);
        if (auto* a2 = displayAsset(2))
            drawTimeDbPath(g, edcPlot, a2->energyDecay, maxTime, wetBColour(exportMode), exportMode, -80.0f, 0.0f);
        drawLegend(g, edcPlot, exportMode);

        if (spectrogramArea.getHeight() < 40.0f)
            return;

        struct SpectrogramTrack
        {
            const juce::Image* image = nullptr;
            double sampleRate = 0.0;
            float durationSeconds = 0.0f;
            juce::String label;
        };

        std::vector<SpectrogramTrack> tracks;
        if (auto* a0 = displayAsset(0); a0 != nullptr && !a0->spectrogramBlue.isNull())
            tracks.push_back({ &a0->spectrogramBlue, a0->sampleRate,
                               static_cast<float>(a0->metrics.durationSeconds), displayLabel(0) + " tail" });
        if (auto* a1 = displayAsset(1); a1 != nullptr && !a1->spectrogramYellow.isNull())
            tracks.push_back({ &a1->spectrogramYellow, a1->sampleRate,
                               static_cast<float>(a1->metrics.durationSeconds), displayLabel(1) + " tail" });
        if (auto* a2 = displayAsset(2); a2 != nullptr && !a2->spectrogramPink.isNull())
            tracks.push_back({ &a2->spectrogramPink, a2->sampleRate,
                               static_cast<float>(a2->metrics.durationSeconds), displayLabel(2) + " tail" });

        float maxDurationSeconds = 0.001f;
        for (const auto& track : tracks)
            maxDurationSeconds = juce::jmax(maxDurationSeconds, track.durationSeconds);

        const float gap = tracks.size() > 1 ? (exportMode ? 12.0f : 8.0f) : 0.0f;
        const float trackHeight = (spectrogramArea.getHeight() - gap * static_cast<float>(tracks.size() - 1))
                                / static_cast<float>(tracks.size());
        auto remaining = spectrogramArea;
        for (const auto& track : tracks)
        {
            auto area = remaining.removeFromTop(trackHeight);
            if (track.image != nullptr && !track.image->isNull())
                drawSpectrogramPanel(g, area, *track.image, track.sampleRate,
                                     track.durationSeconds, maxDurationSeconds,
                                     track.label, exportMode);
            remaining.removeFromTop(gap);
        }
    }

    void drawDynamicsPlot(juce::Graphics& g, juce::Rectangle<float> plot, bool exportMode)
    {
        if (displayAsset(0) == nullptr || displayAsset(0)->dynamicsRms.empty())
        {
            drawPlotHint(g, plot, "No dynamics data.");
            return;
        }

        drawPlotFrame(g, plot, exportMode, juce::String::fromUTF8("时间 (s)"), "RMS response (dB)");
        drawTimeDbGrid(g, plot, getMaxEnvelopeTime(), exportMode);

        const float maxTime = getMaxEnvelopeTime();
        if (auto* a0 = displayAsset(0))
            drawTimeDbPath(g, plot, a0->dynamicsRms, maxTime, dryColour(exportMode), exportMode, -80.0f, 0.0f);
        if (auto* a1 = displayAsset(1))
            drawTimeDbPath(g, plot, a1->dynamicsRms, maxTime, wetColour(exportMode), exportMode, -80.0f, 0.0f);
        if (auto* a2 = displayAsset(2))
            drawTimeDbPath(g, plot, a2->dynamicsRms, maxTime, wetBColour(exportMode), exportMode, -80.0f, 0.0f);

        drawLegend(g, plot, exportMode);
    }

    static void drawPlotFrame(juce::Graphics& g, juce::Rectangle<float> plot, bool exportMode,
                              const juce::String& xLabel, const juce::String& yLabel)
    {
        const bool light = isLightFigure(exportMode);
        g.setColour(light ? juce::Colour(0xFFF8F9FB) : juce::Colour(0xFF0A0D13));
        g.fillRoundedRectangle(plot, exportMode ? 0.0f : 12.0f);
        if (!exportMode)
        {
            g.setColour(light ? juce::Colour(0xFF1E2530).withAlpha(0.12f)
                              : juce::Colours::white.withAlpha(0.08f));
            g.drawRoundedRectangle(plot.reduced(0.5f), 12.0f, 1.0f);
        }

        g.setFont(juce::Font(juce::FontOptions(exportMode ? 18.0f : 11.0f)));
        g.setColour(axisText(exportMode));
        g.drawText(xLabel, plot.withY(plot.getBottom() + 8.0f).withHeight(24.0f),
                   juce::Justification::centred);

        juce::Graphics::ScopedSaveState save(g);
        g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                       plot.getX() - 36.0f, plot.getCentreY()));
        g.setColour(axisText(exportMode));
        g.drawText(yLabel, plot.getX() - 160.0f, plot.getCentreY() - 12.0f, 250.0f, 24.0f,
                   juce::Justification::centred);
    }

    static float xForFrequencyInRange(float freq, juce::Rectangle<float> plot, FrequencyRange range)
    {
        const float t = (std::log10(juce::jlimit(range.minHz, range.maxHz, freq)) - std::log10(range.minHz))
                      / (std::log10(range.maxHz) - std::log10(range.minHz));
        return plot.getX() + plot.getWidth() * t;
    }

    float xForFrequency(float freq, juce::Rectangle<float> plot) const
    {
        return xForFrequencyInRange(freq, plot, getFrequencyRange());
    }

    static float yForDb(float db, juce::Rectangle<float> plot, float minDb = -90.0f, float maxDb = 0.0f)
    {
        const float t = (juce::jlimit(minDb, maxDb, db) - minDb) / (maxDb - minDb);
        return plot.getBottom() - plot.getHeight() * t;
    }

    static float yForGroupDelay(float ms, juce::Rectangle<float> plot)
    {
        const float minMs = -20.0f;
        const float maxMs = 80.0f;
        const float t = (juce::jlimit(minMs, maxMs, ms) - minMs) / (maxMs - minMs);
        return plot.getBottom() - plot.getHeight() * t;
    }

    static void drawGroupDelayReference(juce::Graphics& g, juce::Rectangle<float> plot, bool exportMode)
    {
        const float y = yForGroupDelay(0.0f, plot);
        g.setColour(dryColour(exportMode).withAlpha(exportMode ? 0.72f : 0.58f));
        g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
        g.setColour(dryColour(exportMode).withAlpha(exportMode ? 0.92f : 0.82f));
        g.drawLine(plot.getX(), y, plot.getRight(), y, exportMode ? 3.0f : 1.8f);

        g.setFont(juce::Font(GoodMeterLookAndFeel::chartFont(exportMode ? 14.0f : 9.0f)).boldened());
        g.drawText("Dry reference 0 ms", plot.getX() + 8.0f, y - 20.0f, 150.0f, 18.0f,
                   juce::Justification::centredLeft);
    }

    struct GroupDelayStats
    {
        bool valid = false;
        float meanMs = 0.0f;
        float meanAbsMs = 0.0f;
        float minMs = 0.0f;
        float maxMs = 0.0f;
        float peakAbsMs = 0.0f;
        float peakFreq = 0.0f;
        float lowMeanMs = 0.0f;
        float midMeanMs = 0.0f;
        float highMeanMs = 0.0f;
        int reliablePoints = 0;
        float reliableMinHz = 0.0f;
        float reliableMaxHz = 0.0f;
        float spectrumGateDb = -120.0f;
    };

    static GroupDelayStats computeGroupDelayStats(const std::vector<goodmeter::audio_doctor::PlotPoint>& points,
                                                  FrequencyRange range,
                                                  const std::vector<goodmeter::audio_doctor::PlotPoint>* spectrum = nullptr)
    {
        GroupDelayStats stats;
        double sum = 0.0;
        double sumAbs = 0.0;
        int count = 0;
        float minMs = std::numeric_limits<float>::max();
        float maxMs = std::numeric_limits<float>::lowest();
        float peakAbs = 0.0f;
        float peakFreq = 0.0f;

        double bandSums[3] = { 0.0, 0.0, 0.0 };
        int bandCounts[3] = { 0, 0, 0 };
        float spectrumPeakDb = std::numeric_limits<float>::lowest();
        const bool hasSpectrum = spectrum != nullptr && !spectrum->empty();
        if (hasSpectrum)
            for (const auto& s : *spectrum)
                if (s.x >= range.minHz && s.x <= range.maxHz)
                    spectrumPeakDb = juce::jmax(spectrumPeakDb, s.y);

        const bool useSpectrumGate = hasSpectrum && spectrumPeakDb > -119.0f;
        stats.spectrumGateDb = useSpectrumGate ? spectrumPeakDb - 45.0f : -120.0f;

        for (const auto& p : points)
        {
            if (p.x < range.minHz || p.x > range.maxHz)
                continue;

            if (useSpectrumGate)
            {
                float spectrumDb = -120.0f;
                if (!goodmeter::audio_doctor::interpolatePlotYAt(*spectrum, p.x, spectrumDb)
                    || spectrumDb < stats.spectrumGateDb)
                    continue;
            }

            const float value = p.y;
            sum += static_cast<double>(value);
            sumAbs += static_cast<double>(std::abs(value));
            minMs = juce::jmin(minMs, value);
            maxMs = juce::jmax(maxMs, value);
            stats.reliableMinHz = count == 0 ? p.x : juce::jmin(stats.reliableMinHz, p.x);
            stats.reliableMaxHz = count == 0 ? p.x : juce::jmax(stats.reliableMaxHz, p.x);

            const float absValue = std::abs(value);
            if (absValue > peakAbs)
            {
                peakAbs = absValue;
                peakFreq = p.x;
            }

            const int band = p.x < 200.0f ? 0 : (p.x < 2000.0f ? 1 : 2);
            bandSums[band] += static_cast<double>(value);
            ++bandCounts[band];
            ++count;
        }

        if (count <= 0)
            return stats;

        stats.valid = true;
        stats.meanMs = static_cast<float>(sum / static_cast<double>(count));
        stats.meanAbsMs = static_cast<float>(sumAbs / static_cast<double>(count));
        stats.minMs = minMs;
        stats.maxMs = maxMs;
        stats.peakAbsMs = peakAbs;
        stats.peakFreq = peakFreq;
        stats.lowMeanMs = bandCounts[0] > 0 ? static_cast<float>(bandSums[0] / static_cast<double>(bandCounts[0])) : 0.0f;
        stats.midMeanMs = bandCounts[1] > 0 ? static_cast<float>(bandSums[1] / static_cast<double>(bandCounts[1])) : 0.0f;
        stats.highMeanMs = bandCounts[2] > 0 ? static_cast<float>(bandSums[2] / static_cast<double>(bandCounts[2])) : 0.0f;
        stats.reliablePoints = count;
        return stats;
    }

    static juce::String formatDelayMs(float ms)
    {
        return juce::String(ms, std::abs(ms) >= 10.0f ? 1 : 2) + " ms";
    }

    static juce::String formatFeatureFrequency(float hz)
    {
        return hz >= 1000.0f ? juce::String(hz / 1000.0f, hz >= 10000.0f ? 1 : 2) + " kHz"
                             : juce::String(hz, 0) + " Hz";
    }

    static juce::String formatSignedDb(float db)
    {
        return (db > 0.0f ? "+" : "") + juce::String(db, 1) + " dB";
    }

    static juce::String formatApparentAttenuationLine(const Asset* reference, const Asset* target)
    {
        if (reference == nullptr || target == nullptr)
            return {};

        const auto stats = goodmeter::audio_doctor::computeApparentAttenuationStats(reference->dynamicsRms,
                                                                                    target->dynamicsRms);
        if (!stats.valid)
            return {};

        return "apparent attenuation max " + juce::String(stats.maxReductionDb, 1) + " dB"
            + " at " + juce::String(stats.peakReductionSeconds, 2) + " s"
            + " | mean delta " + formatSignedDb(stats.meanDeltaDb)
            + " | max lift " + juce::String(stats.maxExpansionDb, 1) + " dB";
    }

    static void drawFrequencyBands(juce::Graphics& g, juce::Rectangle<float> plot)
    {
        const FrequencyRange fullRange;
        auto band = [&](float f1, float f2, juce::Colour c)
        {
            const float x1 = xForFrequencyInRange(f1, plot, fullRange);
            const float x2 = xForFrequencyInRange(f2, plot, fullRange);
            g.setColour(c.withAlpha(0.08f));
            g.fillRect(juce::Rectangle<float>(x1, plot.getY(), x2 - x1, plot.getHeight()));
        };

        band(20.0f, 200.0f, juce::Colour(0xFFE74C3C));
        band(200.0f, 2000.0f, juce::Colour(0xFF2EAD62));
        band(2000.0f, 20000.0f, juce::Colour(0xFF2D77D4));
    }

    void drawSpectrumGrid(juce::Graphics& g, juce::Rectangle<float> plot, bool exportMode) const
    {
        const float freqs[] = { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f,
                                2000.0f, 5000.0f, 10000.0f, 20000.0f };
        const auto range = getFrequencyRange();
        g.setFont(juce::Font(juce::FontOptions(exportMode ? 15.0f : 9.0f)));

        for (float f : freqs)
        {
            if (f < range.minHz || f > range.maxHz)
                continue;

            const float x = xForFrequency(f, plot);
            const bool accent = std::abs(f - 100.0f) < 1.0f
                             || std::abs(f - 1000.0f) < 1.0f
                             || std::abs(f - 10000.0f) < 1.0f;
            const auto accentColour = std::abs(f - 100.0f) < 1.0f ? GoodMeterLookAndFeel::accentGreen
                                    : std::abs(f - 1000.0f) < 1.0f ? GoodMeterLookAndFeel::accentYellow
                                    : GoodMeterLookAndFeel::accentPink;
            const bool light = isLightFigure(exportMode);
            g.setColour(accent ? accentColour.withAlpha(light ? 0.50f : 0.42f)
                               : (light ? juce::Colour(0xFF1E2530).withAlpha(0.17f)
                                        : juce::Colour(0xFFF3EFE7).withAlpha(0.13f)));
            g.drawVerticalLine(static_cast<int>(x), plot.getY(), plot.getBottom());

            g.setColour(accent ? accentColour.withAlpha(0.90f)
                               : axisText(exportMode).withAlpha(exportMode ? 0.86f : 0.72f));
            const auto label = f >= 1000.0f ? juce::String(f / 1000.0f, f >= 10000.0f ? 0 : 1) + "k"
                                            : juce::String(static_cast<int>(f));
            g.drawText(label, x - 24.0f, plot.getBottom() + 2.0f, 48.0f, 18.0f,
                       juce::Justification::centred);
        }

        for (float db = -80.0f; db <= 0.0f; db += 20.0f)
        {
            const float y = yForDb(db, plot);
            g.setColour(isLightFigure(exportMode) ? juce::Colour(0xFF1E2530).withAlpha(0.15f)
                                                  : juce::Colour(0xFFF3EFE7).withAlpha(0.12f));
            g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
            g.setColour(axisText(exportMode).withAlpha(exportMode ? 0.86f : 0.72f));
            g.drawText(juce::String(static_cast<int>(db)), plot.getX() - 42.0f, y - 9.0f, 36.0f, 18.0f,
                       juce::Justification::centredRight);
        }
    }

    static void drawEnvelopeGrid(juce::Graphics& g, juce::Rectangle<float> plot, bool exportMode)
    {
        g.setFont(juce::Font(juce::FontOptions(exportMode ? 15.0f : 9.0f)));

        for (float db = -80.0f; db <= 0.0f; db += 20.0f)
        {
            const float y = yForDb(db, plot, -80.0f, 0.0f);
            g.setColour(isLightFigure(exportMode) ? juce::Colour(0xFF1E2530).withAlpha(0.15f)
                                                  : juce::Colour(0xFFF3EFE7).withAlpha(0.12f));
            g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
            g.setColour(axisText(exportMode).withAlpha(exportMode ? 0.86f : 0.72f));
            g.drawText(juce::String(static_cast<int>(db)), plot.getX() - 42.0f, y - 9.0f, 36.0f, 18.0f,
                       juce::Justification::centredRight);
        }
    }

    static void drawTimeDbGrid(juce::Graphics& g, juce::Rectangle<float> plot, float maxTime, bool exportMode)
    {
        drawEnvelopeGrid(g, plot, exportMode);

        if (maxTime <= 0.0f)
            return;

        g.setFont(juce::Font(juce::FontOptions(exportMode ? 15.0f : 9.0f)));
        const int divisions = maxTime > 4.0f ? 6 : 4;
        for (int i = 1; i < divisions; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(divisions);
            const float x = plot.getX() + plot.getWidth() * t;
            g.setColour(isLightFigure(exportMode) ? juce::Colour(0xFF1E2530).withAlpha(0.12f)
                                                  : juce::Colour(0xFFF3EFE7).withAlpha(0.10f));
            g.drawVerticalLine(static_cast<int>(x), plot.getY(), plot.getBottom());
            g.setColour(axisText(exportMode).withAlpha(exportMode ? 0.80f : 0.66f));
            g.drawText(juce::String(maxTime * t, maxTime > 2.0f ? 1 : 2) + "s",
                       x - 25.0f, plot.getBottom() + 2.0f, 50.0f, 18.0f,
                       juce::Justification::centred);
        }
    }

    static void drawGroupDelayGrid(juce::Graphics& g, juce::Rectangle<float> plot, bool exportMode)
    {
        g.setFont(juce::Font(GoodMeterLookAndFeel::chartFont(exportMode ? 15.0f : 9.0f)));
        for (float ms : { -20.0f, 0.0f, 20.0f, 40.0f, 60.0f, 80.0f })
        {
            const float y = yForGroupDelay(ms, plot);
            g.setColour(ms == 0.0f ? GoodMeterLookAndFeel::accentCyan.withAlpha(0.45f)
                                   : (isLightFigure(exportMode) ? juce::Colour(0xFF1E2530).withAlpha(0.15f)
                                                                : juce::Colour(0xFFF3EFE7).withAlpha(0.12f)));
            g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
            g.setColour(ms == 0.0f ? GoodMeterLookAndFeel::accentCyan.withAlpha(0.9f)
                                   : axisText(exportMode).withAlpha(exportMode ? 0.86f : 0.72f));
            g.drawText(juce::String(ms, 0), plot.getX() - 46.0f, y - 9.0f, 38.0f, 18.0f,
                       juce::Justification::centredRight);
        }
    }

    static void drawPlotHint(juce::Graphics& g, juce::Rectangle<float> plot, const juce::String& text)
    {
        g.setColour(juce::Colour(0xFF0A0D13));
        g.fillRect(plot);
        g.setColour(juce::Colour(0xFFF3EFE7).withAlpha(0.74f));
        g.setFont(juce::Font(juce::FontOptions(13.0f)).boldened());
        g.drawText(text, plot.reduced(18.0f), juce::Justification::centred);
    }

    void drawSpectrumPath(juce::Graphics& g, juce::Rectangle<float> plot,
                          const std::vector<goodmeter::audio_doctor::PlotPoint>& points,
                          juce::Colour colour, bool exportMode) const
    {
        if (points.empty())
            return;

        const auto range = getFrequencyRange();
        juce::Path path;
        bool started = false;
        for (const auto& p : points)
        {
            if (p.x < range.minHz || p.x > range.maxHz)
                continue;

            const float x = xForFrequency(p.x, plot);
            const float y = yForDb(p.y, plot);
            if (!started)
            {
                path.startNewSubPath(x, y);
                started = true;
            }
            else
            {
                path.lineTo(x, y);
            }
        }

        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(exportMode ? 4.0f : 2.0f));
    }

    void drawGroupDelayPath(juce::Graphics& g, juce::Rectangle<float> plot,
                            const std::vector<goodmeter::audio_doctor::PlotPoint>& points,
                            juce::Colour colour, bool exportMode) const
    {
        if (points.empty())
            return;

        const auto range = getFrequencyRange();
        juce::Path path;
        bool started = false;
        for (const auto& p : points)
        {
            if (p.x < range.minHz || p.x > range.maxHz)
                continue;

            const float x = xForFrequency(p.x, plot);
            const float y = yForGroupDelay(p.y, plot);
            if (!started)
            {
                path.startNewSubPath(x, y);
                started = true;
            }
            else
            {
                path.lineTo(x, y);
            }
        }

        g.setColour(colour.withAlpha(0.18f));
        g.strokePath(path, juce::PathStrokeType(exportMode ? 9.0f : 5.0f));
        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(exportMode ? 4.0f : 2.2f));
    }

    static void drawSpectrogramPanel(juce::Graphics& g, juce::Rectangle<float> area,
                                     const juce::Image& image, double sampleRate,
                                     float durationSeconds, float maxDurationSeconds,
                                     const juce::String& label,
                                     bool exportMode)
    {
        const bool light = isLightFigure(exportMode);
        const auto trackColour = label.contains("Wet B") ? wetBColour(exportMode)
                               : label.contains("Wet A") ? wetColour(exportMode)
                                                         : dryColour(exportMode);
        g.setColour(light ? juce::Colour(0xFFFBFCFE) : juce::Colour(0xFF0A0D13));
        g.fillRect(area);

        auto contentArea = area.reduced(0.0f, exportMode ? 12.0f : 7.0f);
        if (contentArea.getHeight() < 24.0f)
            contentArea = area;

        if (!image.isNull())
        {
            const float durationRatio = juce::jlimit(0.0f, 1.0f,
                durationSeconds / juce::jmax(0.001f, maxDurationSeconds));
            auto imageArea = contentArea.withWidth(juce::jmax(1.0f, contentArea.getWidth() * durationRatio));
            if (light)
            {
                auto lightImage = makeLightSpectrogramImage(image, trackColour);
                g.drawImage(lightImage, imageArea, juce::RectanglePlacement::stretchToFit);
            }
            else
            {
                g.drawImage(image, imageArea, juce::RectanglePlacement::stretchToFit);
            }
        }

        g.setFont(juce::Font(GoodMeterLookAndFeel::chartFont(exportMode ? 18.0f : 11.0f)).boldened());
        g.setColour(light ? trackColour.darker(0.45f) : juce::Colours::white.withAlpha(0.88f));
        g.drawText(label, area.reduced(8.0f, 5.0f), juce::Justification::topLeft);

        g.setFont(juce::Font(GoodMeterLookAndFeel::chartFont(exportMode ? 14.0f : 9.0f)));
        const float freqs[] = { 100.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
        const float nyquist = sampleRate > 0.0 ? static_cast<float>(sampleRate * 0.5) : 24000.0f;
        for (float freq : freqs)
        {
            if (freq > nyquist)
                continue;
            const float norm = freq / juce::jmax(1.0f, nyquist);
            const float y = contentArea.getBottom() - juce::jlimit(0.0f, 1.0f, norm) * contentArea.getHeight();
            g.setColour(light ? juce::Colour(0xFF334155).withAlpha(0.20f)
                              : juce::Colours::white.withAlpha(0.14f));
            g.drawHorizontalLine(static_cast<int>(y), contentArea.getX(), contentArea.getRight());
            g.setColour(light ? juce::Colour(0xFF334155).withAlpha(0.86f)
                              : juce::Colours::white.withAlpha(0.62f));
            const auto text = freq >= 1000.0f ? juce::String(static_cast<int>(freq / 1000.0f)) + "k"
                                              : juce::String(static_cast<int>(freq));
            g.drawText(text, contentArea.getRight() - 38.0f, y - 7.0f, 32.0f, 14.0f,
                       juce::Justification::centredRight);
        }
    }

    static juce::Image makeLightSpectrogramImage(const juce::Image& source, juce::Colour ink)
    {
        return makeLightSpectrogramImageOilPastel(source, ink);
    }

    static juce::Image makeLightSpectrogramImageOilPastel(const juce::Image& source, juce::Colour ink)
    {
        juce::Image out(juce::Image::RGB, source.getWidth(), source.getHeight(), true);
        const auto paper = juce::Colour(0xFFFBFCFE);
        const auto deepInk = ink.darker(0.18f);

        for (int y = 0; y < source.getHeight(); ++y)
        {
            for (int x = 0; x < source.getWidth(); ++x)
            {
                const auto c = source.getPixelAt(x, y);
                const float energy = juce::jlimit(0.0f, 1.0f,
                    (juce::jmax(c.getFloatRed(), juce::jmax(c.getFloatGreen(), c.getFloatBlue())) - 0.095f) / 0.84f);
                const float shaped = std::pow(energy, 0.78f);
                auto outColour = paper.interpolatedWith(deepInk, shaped * 0.86f);
                if (energy < 0.018f)
                    outColour = paper;
                out.setPixelAt(x, y, outColour);
            }
        }

        return out;
    }

    void drawEnvelopePath(juce::Graphics& g, juce::Rectangle<float> plot,
                          const std::vector<goodmeter::audio_doctor::PlotPoint>& points,
                          float maxTime, juce::Colour colour, bool exportMode)
    {
        if (points.empty() || maxTime <= 0.0f)
            return;

        juce::Path path;
        bool started = false;
        for (const auto& p : points)
        {
            const float x = plot.getX() + plot.getWidth() * juce::jlimit(0.0f, 1.0f, p.x / maxTime);
            const float y = yForDb(p.y, plot, -80.0f, 0.0f);
            if (!started)
            {
                path.startNewSubPath(x, y);
                started = true;
            }
            else
            {
                path.lineTo(x, y);
            }
        }

        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(exportMode ? 4.0f : 2.0f));
    }

    static void drawTimeDbPath(juce::Graphics& g, juce::Rectangle<float> plot,
                               const std::vector<goodmeter::audio_doctor::PlotPoint>& points,
                               float maxTime, juce::Colour colour, bool exportMode,
                               float minDb, float maxDb)
    {
        if (points.empty() || maxTime <= 0.0f)
            return;

        juce::Path path;
        bool started = false;
        for (const auto& p : points)
        {
            if (p.x < 0.0f || p.x > maxTime)
                continue;

            const float x = plot.getX() + plot.getWidth() * juce::jlimit(0.0f, 1.0f, p.x / maxTime);
            const float y = yForDb(p.y, plot, minDb, maxDb);
            if (!started)
            {
                path.startNewSubPath(x, y);
                started = true;
            }
            else
            {
                path.lineTo(x, y);
            }
        }

        g.setColour(colour);
        g.strokePath(path, juce::PathStrokeType(exportMode ? 4.0f : 2.0f));
    }

    void drawHarmonicMarkers(juce::Graphics& g, juce::Rectangle<float> plot,
                             float fundamental, bool exportMode) const
    {
        g.setFont(juce::Font(juce::FontOptions(exportMode ? 14.0f : 9.0f)));
        const auto markerColour = exportMode ? juce::Colour(0xFF111111) : uiText();
        const auto range = getFrequencyRange();
        for (int n = 1; n <= 12; ++n)
        {
            const float freq = fundamental * static_cast<float>(n);
            if (freq > 20000.0f)
                break;
            if (freq < range.minHz || freq > range.maxHz)
                continue;

            const float x = xForFrequency(freq, plot);
            g.setColour(markerColour.withAlpha(n == 1 ? 0.45f : 0.25f));
            g.drawVerticalLine(static_cast<int>(x), plot.getY(), plot.getBottom());
            g.setColour(markerColour.withAlpha(0.92f));
            g.drawText(n == 1 ? "f0" : juce::String(n) + "f",
                       x - 18.0f, plot.getY() + 6.0f, 36.0f, 18.0f,
                       juce::Justification::centred);
        }
    }

    const Asset* chooseHarmonicReference() const
    {
        for (int i = 1; i < 3; ++i)
            if (auto* asset = displayAsset(i); asset != nullptr && !asset->harmonicPeaks.empty())
                return asset;
        if (auto* asset = displayAsset(0); asset != nullptr && !asset->harmonicPeaks.empty())
            return asset;
        return nullptr;
    }

    static juce::Colour harmonicGlowColour(bool light, int lane)
    {
        if (lane == 1)
            return light ? juce::Colour(0xFF047C9D) : juce::Colour(0xFF6EE7FF);
        if (lane == 2)
            return light ? juce::Colour(0xFF2F7D32) : juce::Colour(0xFF9DFF6A);
        return light ? juce::Colour(0xFFB45309) : juce::Colour(0xFFFFD166);
    }

    static void drawGlowPoint(juce::Graphics& g, float x, float y,
                              juce::Colour colour, bool light, bool exportMode)
    {
        const float glow = exportMode ? 24.0f : 15.0f;
        g.setColour(colour.withAlpha(light ? 0.11f : 0.15f));
        g.fillEllipse(x - glow * 0.5f, y - glow * 0.5f, glow, glow);

        const float mid = exportMode ? 14.0f : 9.0f;
        g.setColour(colour.withAlpha(light ? 0.25f : 0.34f));
        g.fillEllipse(x - mid * 0.5f, y - mid * 0.5f, mid, mid);

        const float core = exportMode ? 8.0f : 5.0f;
        g.setColour(colour);
        g.fillEllipse(x - core * 0.5f, y - core * 0.5f, core, core);

        const float spark = exportMode ? 3.0f : 2.0f;
        g.setColour((light ? juce::Colour(0xFFFFFBF0) : juce::Colours::white).withAlpha(0.86f));
        g.fillEllipse(x - spark * 0.5f, y - spark * 0.5f, spark, spark);
    }

    void drawHarmonicPeakOverlay(juce::Graphics& g, juce::Rectangle<float> plot, bool exportMode) const
    {
        const auto range = getFrequencyRange();
        const auto markerColour = axisText(exportMode);
        const bool light = isLightFigure(exportMode);
        if (const auto* reference = chooseHarmonicReference())
        {
            int drawn = 0;
            g.setFont(juce::Font(GoodMeterLookAndFeel::chartFont(exportMode ? 14.0f : 9.0f)).boldened());
            for (const auto& peak : reference->harmonicPeaks)
            {
                if (!peak.nearHarmonic || peak.expectedHz < range.minHz || peak.expectedHz > range.maxHz)
                    continue;

                const float x = xForFrequency(peak.expectedHz, plot);
                g.setColour(markerColour.withAlpha(isLightFigure(exportMode) ? 0.20f : 0.16f));
                g.drawVerticalLine(static_cast<int>(x), plot.getY(), plot.getBottom());
                g.setColour(markerColour.withAlpha(0.68f));
                g.drawText("H" + juce::String(peak.harmonicNumber),
                           x - 18.0f, plot.getY() + 5.0f, 36.0f, 18.0f,
                           juce::Justification::centred);

                if (++drawn >= 14)
                    break;
            }
        }

        auto drawPeakSet = [&](const Asset* asset, juce::Colour colour, int lane)
        {
            if (asset == nullptr)
                return;

            int labels = 0;
            for (const auto& peak : asset->harmonicPeaks)
            {
                if (!peak.nearHarmonic || peak.frequencyHz < range.minHz || peak.frequencyHz > range.maxHz)
                    continue;

                const float x = xForFrequency(peak.frequencyHz, plot);
                const float y = yForDb(peak.magnitudeDb, plot);
                const auto glowColour = harmonicGlowColour(light, lane);
                drawGlowPoint(g, x, y, glowColour, light, exportMode);

                if (labels < (exportMode ? 6 : 4))
                {
                    const auto text = "H" + juce::String(peak.harmonicNumber) + " " + formatFrequency(peak.frequencyHz);
                    const float fontSize = exportMode ? 14.0f : 9.0f;
                    const float labelY = y - (exportMode ? 28.0f : 18.0f) - static_cast<float>(lane) * (exportMode ? 20.0f : 13.0f);
                    g.setColour((isLightFigure(exportMode) ? juce::Colours::white : juce::Colour(0xFF0A0D13))
                                    .withAlpha(isLightFigure(exportMode) ? 0.84f : 0.72f));
                    g.fillRoundedRectangle(x + 7.0f, labelY, exportMode ? 74.0f : 48.0f,
                                           exportMode ? 19.0f : 13.0f, exportMode ? 5.0f : 3.0f);
                    g.setColour(glowColour);
                    g.setFont(juce::Font(GoodMeterLookAndFeel::chartFont(fontSize)).boldened());
                    g.drawText(text, x + 10.0f, labelY, exportMode ? 66.0f : 42.0f,
                               exportMode ? 19.0f : 13.0f,
                               juce::Justification::centredLeft, true);
                    ++labels;
                }
            }
        };

        drawPeakSet(displayAsset(0), dryColour(exportMode), 0);
        drawPeakSet(displayAsset(1), wetColour(exportMode), 1);
        drawPeakSet(displayAsset(2), wetBColour(exportMode), 2);
    }

    void drawLegend(juce::Graphics& g, juce::Rectangle<float> plot, bool exportMode)
    {
        const int rowCount = (displayAsset(0) != nullptr ? 1 : 0)
                           + (displayAsset(1) != nullptr ? 1 : 0)
                           + (displayAsset(2) != nullptr ? 1 : 0);
        const float rowHeight = exportMode ? 34.0f : 22.0f;
        auto legend = juce::Rectangle<float>(plot.getRight() - (exportMode ? 260.0f : 160.0f),
                                             plot.getY() + 12.0f,
                                             exportMode ? 235.0f : 145.0f,
                                             juce::jmax(exportMode ? 58.0f : 34.0f,
                                                        rowHeight * static_cast<float>(rowCount) + (exportMode ? 10.0f : 4.0f)));
        if (exportMode)
        {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.fillRect(legend);
            g.setColour(juce::Colour(0x22000000));
            g.drawRect(legend);
        }
        else
        {
            g.setColour(juce::Colour(0xFF07080B).withAlpha(0.68f));
            g.fillRoundedRectangle(legend, 8.0f);
            g.setColour(juce::Colour(0xFFF6EEE3).withAlpha(0.10f));
            g.drawRoundedRectangle(legend.reduced(0.5f), 8.0f, 0.8f);
        }

        g.setFont(juce::Font(juce::FontOptions(exportMode ? 16.0f : 10.0f)));
        if (displayAsset(0) != nullptr)
            drawLegendItem(g, legend.removeFromTop(rowHeight), dryColour(exportMode), displayLabel(0), !exportMode);
        if (displayAsset(1) != nullptr)
            drawLegendItem(g, legend.removeFromTop(rowHeight), wetColour(exportMode), displayLabel(1), !exportMode);
        if (displayAsset(2) != nullptr)
            drawLegendItem(g, legend.removeFromTop(rowHeight), wetBColour(exportMode), displayLabel(2), !exportMode);
    }

    static void drawLegendItem(juce::Graphics& g, juce::Rectangle<float> row,
                               juce::Colour colour, const juce::String& label,
                               bool darkUi = false)
    {
        row.reduce(10.0f, 4.0f);
        g.setColour(colour);
        g.fillRect(row.removeFromLeft(36.0f).withHeight(4.0f).withY(row.getCentreY() - 2.0f));
        g.setColour(darkUi ? uiText() : juce::Colour(0xFF111111));
        g.drawText(label, row, juce::Justification::centredLeft);
    }

    float getMaxEnvelopeTime() const
    {
        auto effectiveEnd = [](const Asset* asset)
        {
            if (asset == nullptr || asset->envelope.empty())
                return 0.0f;

            float end = 0.0f;
            for (const auto& point : asset->envelope)
                if (point.y > -78.5f)
                    end = juce::jmax(end, point.x);

            if (end <= 0.0f)
                end = static_cast<float>(asset->metrics.durationSeconds);

            return end;
        };

        float t = effectiveEnd(displayAsset(0));
        t = juce::jmax(t, effectiveEnd(displayAsset(1)));
        t = juce::jmax(t, effectiveEnd(displayAsset(2)));

        const float padded = t * 1.08f;
        return juce::jlimit(0.001f, getLongestAssetDuration(), padded);
    }

    float getMaxDecayTime() const
    {
        auto effectiveEnd = [](const Asset* asset)
        {
            if (asset == nullptr)
                return 0.0f;

            if (asset->spaceMetrics.valid && asset->spaceMetrics.tailEndSeconds > 0.0f)
                return asset->spaceMetrics.tailEndSeconds;

            if (!asset->energyDecay.empty())
                return asset->energyDecay.back().x;

            return static_cast<float>(asset->metrics.durationSeconds);
        };

        float t = effectiveEnd(displayAsset(0));
        t = juce::jmax(t, effectiveEnd(displayAsset(1)));
        t = juce::jmax(t, effectiveEnd(displayAsset(2)));
        return juce::jlimit(0.001f, getLongestAssetDuration(), t * 1.08f);
    }

    float getLongestAssetDuration() const
    {
        float t = 0.0f;
        for (int i = 0; i < 3; ++i)
            if (auto* asset = displayAsset(i))
                t = juce::jmax(t, static_cast<float>(asset->metrics.durationSeconds));
        return juce::jmax(0.001f, t);
    }

    void drawMetrics(juce::Graphics& g, juce::Rectangle<float> area, bool exportMode)
    {
        pluginHostA.refreshChangedParameterSnapshot();
        pluginHostB.refreshChangedParameterSnapshot();
        pluginHostC.refreshChangedParameterSnapshot();

        area.reduce(0.0f, exportMode ? 18.0f : 8.0f);
        g.setFont(juce::Font(juce::FontOptions(exportMode ? 19.0f : 12.0f)));
        const bool light = isLightFigure(exportMode);
        g.setColour(detailText(light));

        const bool isGroupDelayView = viewMode.getSelectedId() == 3;
        const bool isReverbSpaceView = viewMode.getSelectedId() == 5;
        const bool isDynamicsView = viewMode.getSelectedId() == 6;
        const bool isDenseMetrics = isGroupDelayView || isReverbSpaceView || isDynamicsView;
        auto pluginArea = area.removeFromRight(exportMode ? 560.0f : 360.0f);
        area.removeFromRight(exportMode ? 30.0f : 16.0f);
        auto assetArea = area;

        auto drawAssetMetrics = [&](const Asset* asset, juce::String label, juce::Colour colour)
        {
            auto row = assetArea.removeFromTop(isDenseMetrics ? (exportMode ? 52.0f : 34.0f)
                                                              : (exportMode ? 34.0f : 20.0f));
            if (asset == nullptr)
                return;

            g.setColour(colour);
            g.fillRect(row.removeFromLeft(exportMode ? 12.0f : 8.0f).reduced(0.0f, 4.0f));
            g.setColour(detailText(light));
            const auto assetText = label + " | "
                + juce::String(asset->metrics.sampleRate, 0) + " Hz | "
                + juce::String(asset->metrics.channels) + " ch | "
                + juce::String(asset->metrics.durationSeconds, 2) + " s | peak "
                + juce::String(asset->metrics.peakDb, 1) + " dB | rms "
                + juce::String(asset->metrics.rmsDb, 1) + " dB | crest "
                + juce::String(asset->metrics.crestDb, 1) + " dB";

            auto textRow = row.reduced(8.0f, 0.0f);
            if (!isDenseMetrics)
            {
                g.drawText(assetText, textRow, juce::Justification::centredLeft, true);
                return;
            }

            g.drawText(assetText, textRow.removeFromTop(exportMode ? 26.0f : 17.0f),
                       juce::Justification::centredLeft, true);

            juce::String detailLine;
            if (isGroupDelayView)
            {
                if (label.startsWithIgnoreCase("dry"))
                {
                    detailLine = "GD reference 0.00 ms";
                }
                else if (!asset->groupDelay.empty())
                {
                    const auto stats = computeGroupDelayStats(asset->groupDelay, getFrequencyRange(), &asset->spectrum);
                    if (stats.valid)
                    {
                        detailLine = "GD avg " + formatDelayMs(stats.meanMs)
                            + " | abs avg " + formatDelayMs(stats.meanAbsMs)
                            + " | peak " + formatDelayMs(stats.peakAbsMs)
                            + " at " + formatFeatureFrequency(stats.peakFreq)
                            + " | reliable "
                            + formatFeatureFrequency(stats.reliableMinHz) + "-"
                            + formatFeatureFrequency(stats.reliableMaxHz)
                            + " | L/M/H "
                            + formatDelayMs(stats.lowMeanMs) + " / "
                            + formatDelayMs(stats.midMeanMs) + " / "
                            + formatDelayMs(stats.highMeanMs)
                            + " | span " + formatDelayMs(stats.maxMs - stats.minMs);
                    }
                    else
                    {
                        detailLine = "GD metrics unavailable: selected band is below the spectrum reliability gate";
                    }
                }
            }
            else if (isReverbSpaceView && asset->spaceMetrics.valid)
            {
                const auto& sm = asset->spaceMetrics;
                detailLine = "EDC tail " + juce::String(sm.tailEndSeconds, 2) + " s"
                    + " | RT20 " + (sm.rt20Seconds > 0.0f ? juce::String(sm.rt20Seconds, 2) + " s" : "n/a")
                    + " | RT30 " + (sm.rt30Seconds > 0.0f ? juce::String(sm.rt30Seconds, 2) + " s" : "n/a")
                    + " | RT60 est. " + (sm.rt60Seconds > 0.0f ? juce::String(sm.rt60Seconds, 2) + " s" : "n/a")
                    + " | DRR " + juce::String(sm.drrDb, 1) + " dB"
                    + " | early/late " + juce::String(sm.earlyLateDb, 1) + " dB"
                    + " | corr " + juce::String(sm.stereoCorrelation, 2)
                    + " | S/M " + juce::String(sm.sideToMidDb, 1) + " dB";
            }
            else if (isDynamicsView && asset->dynamicsMetrics.valid)
            {
                const auto& dm = asset->dynamicsMetrics;
                detailLine = "RMS P10/P50/P90 "
                    + juce::String(dm.rmsP10Db, 1) + " / "
                    + juce::String(dm.rmsP50Db, 1) + " / "
                    + juce::String(dm.rmsP90Db, 1) + " dB"
                    + " | range " + juce::String(dm.rmsRangeDb, 1) + " dB"
                    + " | transient/sustain " + juce::String(dm.transientToSustainDb, 1) + " dB";
                if (!label.startsWithIgnoreCase("dry"))
                {
                    const auto apparentLine = formatApparentAttenuationLine(displayAsset(0), asset);
                    if (apparentLine.isNotEmpty())
                        detailLine += " | " + apparentLine;
                }
            }

            if (detailLine.isNotEmpty())
            {
                g.setColour(secondaryDetailText(light));
                g.drawText(detailLine, textRow, juce::Justification::centredLeft, true);
                g.setColour(detailText(light));
            }
        };

        drawAssetMetrics(displayAsset(0), displayLabel(0), dryColour(exportMode));
        drawAssetMetrics(displayAsset(1), displayLabel(1), wetColour(exportMode));
        drawAssetMetrics(displayAsset(2), displayLabel(2), wetBColour(exportMode));

        auto drawPluginRenderInfo = [&](PluginSlot slot)
        {
            if (!getHasPluginRender(slot))
                return;

            g.setColour(secondaryDetailText(light));
            g.drawText("Plugin " + juce::String(slotName(slot)) + " latency: "
                       + juce::String(getLastLatencySamples(slot))
                       + " samples | rendered tail: " + juce::String(getLastTailSeconds(slot), 2) + " s",
                       assetArea.removeFromTop(exportMode ? 30.0f : 18.0f),
                       juce::Justification::centredLeft, true);
        };

        drawPluginRenderInfo(PluginSlot::A);
        drawPluginRenderInfo(PluginSlot::B);
        drawPluginRenderInfo(PluginSlot::C);
        drawPluginParameterPanel(g, pluginArea, exportMode);
    }

    void drawPluginParameterPanel(juce::Graphics& g, juce::Rectangle<float> area, bool exportMode)
    {
        const float rowHeight = exportMode ? 25.0f : 16.0f;
        const int maxParamsPerPlugin = 8;

        auto drawPlugin = [&](PluginSlot slot, juce::Rectangle<float> row)
        {
            const auto& host = getPluginHost(slot);
            if (host.getCurrentPlugin() == nullptr)
                return;

            const auto titleWidth = exportMode ? 138.0f : 82.0f;
            g.setColour(pluginSlotColour(slot, exportMode));
            g.fillRect(row.removeFromLeft(exportMode ? 12.0f : 8.0f).reduced(0.0f, 3.5f));

            g.setColour(detailText(isLightFigure(exportMode)));
            g.drawText("Plugin " + juce::String(slotName(slot)) + " params",
                       row.removeFromLeft(titleWidth),
                       juce::Justification::centredLeft, true);

            const auto& params = host.getChangedParameters();
            juce::String text;
            if (params.empty())
            {
                text = "default/no changed parameter";
            }
            else
            {
                const int count = juce::jmin(maxParamsPerPlugin, static_cast<int>(params.size()));
                for (int i = 0; i < count; ++i)
                {
                    if (i > 0)
                        text += " | ";
                    text += params[static_cast<size_t>(i)].name + " " + params[static_cast<size_t>(i)].valueText;
                }

                if (static_cast<int>(params.size()) > count)
                    text += " | +" + juce::String(static_cast<int>(params.size()) - count);
            }

            g.setColour(secondaryDetailText(isLightFigure(exportMode)));
            g.drawText(text, row, juce::Justification::centredLeft, true);
        };

        drawPlugin(PluginSlot::A, area.removeFromTop(rowHeight));
        drawPlugin(PluginSlot::B, area.removeFromTop(rowHeight));
        drawPlugin(PluginSlot::C, area.removeFromTop(rowHeight));
    }

    void writeManifest(const juce::File& jsonFile, const juce::File& darkPngFile, const juce::File& lightPngFile,
                       const juce::Array<juce::var>& dataFiles)
    {
        auto root = std::make_unique<juce::DynamicObject>();
        root->setProperty("tool", "GOODMETER Audio Doctor");
        root->setProperty("exportedAt", juce::Time::getCurrentTime().toISO8601(true));
        root->setProperty("darkFigurePath", darkPngFile.getFullPathName());
        root->setProperty("lightFigurePath", lightPngFile.getFullPathName());
        root->setProperty("dataFiles", juce::var(dataFiles));
        const char* viewName = "spectrum";
        if (viewMode.getSelectedId() == 2) viewName = "envelope";
        if (viewMode.getSelectedId() == 3) viewName = "groupDelay";
        if (viewMode.getSelectedId() == 4) viewName = "spectrogramABC";
        if (viewMode.getSelectedId() == 5) viewName = "reverbSpace";
        if (viewMode.getSelectedId() == 6) viewName = "dynamicsResponse";
        root->setProperty("view", viewName);
        const auto freqRange = getFrequencyRange();
        auto range = std::make_unique<juce::DynamicObject>();
        range->setProperty("label", freqRange.label);
        range->setProperty("minHz", freqRange.minHz);
        range->setProperty("maxHz", freqRange.maxHz);
        root->setProperty("frequencyRange", juce::var(range.release()));

        auto writePoints = [](const std::vector<goodmeter::audio_doctor::PlotPoint>& points, int maxPoints)
        {
            juce::Array<juce::var> array;
            if (points.empty())
                return juce::var(array);

            const int count = static_cast<int>(points.size());
            const int step = juce::jmax(1, count / juce::jmax(1, maxPoints));
            for (int i = 0; i < count; i += step)
            {
                auto point = std::make_unique<juce::DynamicObject>();
                point->setProperty("x", points[static_cast<size_t>(i)].x);
                point->setProperty("y", points[static_cast<size_t>(i)].y);
                array.add(juce::var(point.release()));
            }
            return juce::var(array);
        };

        auto writeAsset = [&](const Asset* asset)
        {
            auto obj = std::make_unique<juce::DynamicObject>();
            if (asset != nullptr)
            {
                obj->setProperty("name", asset->name);
                obj->setProperty("sourcePath", asset->sourcePath);
                const auto sourceFileName = asset->generatedSignal ? asset->name
                                          : juce::File(asset->sourcePath).getFileName();
                obj->setProperty("sourceFileName", sourceFileName.isNotEmpty() ? sourceFileName : asset->name);
                obj->setProperty("sourceType", asset->generatedSignal ? "generated" : "file");
                obj->setProperty("sourceHash", asset->generatedSignal ? goodmeter::audio_doctor::hashGeneratedSignalSpec(asset->generatedSignalSpec)
                                                                      : goodmeter::audio_doctor::hashSourceFnv1a64(asset->sourcePath));
                obj->setProperty("hashInputVersion", asset->generatedSignal ? goodmeter::audio_doctor::generatedSignalSpecHashInputVersion()
                                                                            : "sourceFileBytes.fnv1a64");
                obj->setProperty("sourceBytes", asset->generatedSignal ? static_cast<juce::int64>(0)
                                                                       : goodmeter::audio_doctor::sourceBytesOnDisk(asset->sourcePath));
                if (asset->generatedSignal)
                    obj->setProperty("generatedSignalSpec", goodmeter::audio_doctor::writeGeneratedSignalSpecJson(asset->generatedSignalSpec));
                obj->setProperty("sampleRate", asset->metrics.sampleRate);
                obj->setProperty("channels", asset->metrics.channels);
                obj->setProperty("durationSeconds", asset->metrics.durationSeconds);
                const double selectionStart = asset->editMetadata.valid ? asset->editMetadata.trimStartSeconds : 0.0;
                const double selectionEnd = asset->editMetadata.valid && asset->editMetadata.trimEndSeconds > selectionStart
                                          ? asset->editMetadata.trimEndSeconds
                                          : asset->metrics.durationSeconds;
                auto selection = std::make_unique<juce::DynamicObject>();
                selection->setProperty("startSeconds", selectionStart);
                selection->setProperty("endSeconds", selectionEnd);
                selection->setProperty("analysisDurationSeconds", asset->metrics.durationSeconds);
                selection->setProperty("sourceDurationKnown", asset->editMetadata.valid || asset->metrics.durationSeconds > 0.0);
                obj->setProperty("selection", juce::var(selection.release()));
                obj->setProperty("selectionStartSeconds", selectionStart);
                obj->setProperty("selectionEndSeconds", selectionEnd);
                obj->setProperty("analysisDurationSeconds", asset->metrics.durationSeconds);
                obj->setProperty("peakDb", asset->metrics.peakDb);
                obj->setProperty("rmsDb", asset->metrics.rmsDb);
                obj->setProperty("crestDb", asset->metrics.crestDb);

                auto space = std::make_unique<juce::DynamicObject>();
                space->setProperty("valid", asset->spaceMetrics.valid);
                space->setProperty("onsetSeconds", asset->spaceMetrics.onsetSeconds);
                space->setProperty("tailEndSeconds", asset->spaceMetrics.tailEndSeconds);
                space->setProperty("directEnergyDb", asset->spaceMetrics.directEnergyDb);
                space->setProperty("earlyEnergyDb", asset->spaceMetrics.earlyEnergyDb);
                space->setProperty("lateEnergyDb", asset->spaceMetrics.lateEnergyDb);
                space->setProperty("drrDb", asset->spaceMetrics.drrDb);
                space->setProperty("earlyLateDb", asset->spaceMetrics.earlyLateDb);
                space->setProperty("rt20Seconds", asset->spaceMetrics.rt20Seconds);
                space->setProperty("rt30Seconds", asset->spaceMetrics.rt30Seconds);
                space->setProperty("rt60Seconds", asset->spaceMetrics.rt60Seconds);
                space->setProperty("rt60Estimated", asset->spaceMetrics.rt60Seconds > 0.0f);
                space->setProperty("rt60DerivedFrom", asset->spaceMetrics.rt30Seconds > 0.0f ? "RT30"
                                            : (asset->spaceMetrics.rt20Seconds > 0.0f ? "RT20" : ""));
                space->setProperty("directWindowMs", 20);
                space->setProperty("earlyWindowMs", 80);
                space->setProperty("stereoCorrelation", asset->spaceMetrics.stereoCorrelation);
                space->setProperty("sideToMidDb", asset->spaceMetrics.sideToMidDb);
                obj->setProperty("reverbSpace", juce::var(space.release()));

                auto dynamics = std::make_unique<juce::DynamicObject>();
                dynamics->setProperty("valid", asset->dynamicsMetrics.valid);
                dynamics->setProperty("onsetSeconds", asset->dynamicsMetrics.onsetSeconds);
                dynamics->setProperty("rmsRangeDb", asset->dynamicsMetrics.rmsRangeDb);
                dynamics->setProperty("rmsP10Db", asset->dynamicsMetrics.rmsP10Db);
                dynamics->setProperty("rmsP50Db", asset->dynamicsMetrics.rmsP50Db);
                dynamics->setProperty("rmsP90Db", asset->dynamicsMetrics.rmsP90Db);
                dynamics->setProperty("transientToSustainDb", asset->dynamicsMetrics.transientToSustainDb);
                obj->setProperty("dynamics", juce::var(dynamics.release()));
                obj->setProperty("groupDelayMetrics",
                                 goodmeter::audio_doctor::AudioDoctorFigureRenderer::writeGroupDelayMetrics(asset->groupDelay,
                                                                                                            &asset->spectrum));

                auto curves = std::make_unique<juce::DynamicObject>();
                curves->setProperty("spectrum", writePoints(asset->spectrum, 800));
                curves->setProperty("envelope", writePoints(asset->envelope, 600));
                curves->setProperty("groupDelay", writePoints(asset->groupDelay, 600));
                curves->setProperty("energyDecay", writePoints(asset->energyDecay, 600));
                curves->setProperty("dynamicsRms", writePoints(asset->dynamicsRms, 600));
                obj->setProperty("curves", juce::var(curves.release()));
                obj->setProperty("spectrumPeaks", goodmeter::audio_doctor::writeSpectrumPeaksJson(asset->spectrumPeaks));
                obj->setProperty("harmonicPeaks", goodmeter::audio_doctor::writeSpectrumPeaksJson(asset->harmonicPeaks));
                obj->setProperty("stageMarkers", goodmeter::audio_doctor::writeStageMarkersJson(asset->stageMarkers));
                obj->setProperty("analysisSummary", goodmeter::audio_doctor::writeAnalysisSummaryJson(asset->metrics));
                auto edit = std::make_unique<juce::DynamicObject>();
                edit->setProperty("valid", asset->editMetadata.valid);
                if (asset->editMetadata.valid)
                {
                    edit->setProperty("channelName", asset->editMetadata.channelName);
                    edit->setProperty("originalSourcePath", asset->editMetadata.originalSourcePath);
                    edit->setProperty("derivedSourcePath", asset->editMetadata.derivedSourcePath);
                    edit->setProperty("trimStartSeconds", asset->editMetadata.trimStartSeconds);
                    edit->setProperty("trimEndSeconds", asset->editMetadata.trimEndSeconds);
                    edit->setProperty("fadeInMs", asset->editMetadata.fadeInMs);
                    edit->setProperty("fadeOutMs", asset->editMetadata.fadeOutMs);
                    edit->setProperty("snapToZeroCrossing", asset->editMetadata.snapToZeroCrossing);
                    edit->setProperty("createdAt", asset->editMetadata.createdAt);
                }
                obj->setProperty("editMetadata", juce::var(edit.release()));
            }
            return juce::var(obj.release());
        };

        root->setProperty("dry", writeAsset(dryAsset.get()));
        root->setProperty("dryA", writeAsset(dryAsset.get()));
        root->setProperty("dryB", writeAsset(dryBAsset.get()));
        root->setProperty("dryC", writeAsset(dryCAsset.get()));
        root->setProperty("wetA", writeAsset(wetAsset.get()));
        root->setProperty("wetB", writeAsset(wetBAsset.get()));
        root->setProperty("wetC", writeAsset(wetCAsset.get()));

        juce::Array<juce::var> slots;
        for (int i = 0; i < 3; ++i)
        {
            auto slot = std::make_unique<juce::DynamicObject>();
            slot->setProperty("slot", i + 1);
            slot->setProperty("source", sourceSlotLabel(displaySlots[static_cast<size_t>(i)]));
            slot->setProperty("label", displayLabel(i));
            slots.add(juce::var(slot.release()));
        }
        root->setProperty("displaySlots", juce::var(slots));

        auto writePlugin = [this](PluginSlot slot)
        {
            if (!getHasPluginRender(slot))
                return juce::var();

            auto plugin = std::make_unique<juce::DynamicObject>();
            const auto& description = getLastPluginDescription(slot);
            plugin->setProperty("name", description.name);
            plugin->setProperty("inputSource", renderInputLabel(slot));
            plugin->setProperty("manufacturer", description.manufacturerName);
            plugin->setProperty("format", description.pluginFormatName);
            plugin->setProperty("identifier", description.createIdentifierString());
            plugin->setProperty("latencySamples", getLastLatencySamples(slot));
            plugin->setProperty("tailSeconds", getLastTailSeconds(slot));

            const auto& host = getPluginHost(slot);
            juce::Array<juce::var> allParams;
            for (const auto& p : host.listParameters())
            {
                auto param = std::make_unique<juce::DynamicObject>();
                param->setProperty("index", p.index);
                param->setProperty("id", p.id);
                param->setProperty("name", p.name);
                param->setProperty("label", p.label);
                param->setProperty("normalizedValue", p.normalisedValue);
                param->setProperty("displayValue", p.displayValue);
                param->setProperty("nameUnavailable", p.nameUnavailable);
                param->setProperty("valueText", p.valueText);
                param->setProperty("normalisedValue", p.normalisedValue);
                param->setProperty("defaultValue", p.defaultValue);
                allParams.add(juce::var(param.release()));
            }
            plugin->setProperty("parameters", juce::var(allParams));

            juce::Array<juce::var> params;
            for (const auto& p : host.getChangedParameters())
            {
                auto param = std::make_unique<juce::DynamicObject>();
                param->setProperty("index", p.index);
                param->setProperty("id", p.id);
                param->setProperty("name", p.name);
                param->setProperty("label", p.label);
                param->setProperty("normalizedValue", p.normalisedValue);
                param->setProperty("displayValue", p.displayValue);
                param->setProperty("nameUnavailable", p.nameUnavailable);
                param->setProperty("valueText", p.valueText);
                param->setProperty("normalisedValue", p.normalisedValue);
                params.add(juce::var(param.release()));
            }
            plugin->setProperty("changedParameters", juce::var(params));
            return juce::var(plugin.release());
        };

        root->setProperty("pluginA", writePlugin(PluginSlot::A));
        root->setProperty("pluginB", writePlugin(PluginSlot::B));
        root->setProperty("pluginC", writePlugin(PluginSlot::C));

        auto writeRoute = [this](PluginSlot slot)
        {
            auto route = std::make_unique<juce::DynamicObject>();
            route->setProperty("wet", slot == PluginSlot::A ? "WET A" : (slot == PluginSlot::B ? "WET B" : "WET C"));
            route->setProperty("input", renderInputLabel(slot));
            route->setProperty("rendered", getHasPluginRender(slot));
            const Asset* wetForRoute = slot == PluginSlot::A ? wetAsset.get()
                                    : slot == PluginSlot::B ? wetBAsset.get()
                                                            : wetCAsset.get();
            const auto& renderedReference = getRenderReference(slot);
            const Asset* referenceForRoute = renderedReference != nullptr ? renderedReference.get()
                                                                           : renderInputAsset(slot);

            juce::Array<juce::var> inputs;
            juce::Array<juce::var> inputNames;
            for (auto source : getRenderInputSlots(slot))
            {
                inputs.add(sourceSlotLabel(source));
                if (auto* input = assetFor(source))
                    inputNames.add(input->name);
            }
            route->setProperty("inputs", juce::var(inputs));
            route->setProperty("inputNames", juce::var(inputNames));
            route->setProperty("mixMode", inputs.size() > 1 ? "mixed_then_rendered" : "single_input_rendered");
            if (wetForRoute != nullptr && referenceForRoute != nullptr)
                route->setProperty("apparentAttenuation",
                                   goodmeter::audio_doctor::writeApparentAttenuationStatsJson(
                                       goodmeter::audio_doctor::computeApparentAttenuationStats(referenceForRoute->dynamicsRms,
                                                                                                wetForRoute->dynamicsRms)));
            return juce::var(route.release());
        };

        auto routes = std::make_unique<juce::DynamicObject>();
        routes->setProperty("mode", allowMixedRenderInputs ? "mix" : "controlled");
        routes->setProperty("wetA", writeRoute(PluginSlot::A));
        routes->setProperty("wetB", writeRoute(PluginSlot::B));
        routes->setProperty("wetC", writeRoute(PluginSlot::C));
        root->setProperty("renderRouting", juce::var(routes.release()));

        jsonFile.replaceWithText(juce::JSON::toString(juce::var(root.release()), true));
    }

    //==========================================================================
    juce::TextButton importDryBtn { "Load Dry" };
    juce::TextButton generateBtn  { "Generate" };
    juce::TextButton editAudioBtn { "Edit" };
    juce::TextButton busBtn       { "Bus" };
    juce::TextButton pluginBtn    { "Plugin A" };
    juce::TextButton editPluginBtn { "Show A" };
    juce::TextButton renderBtn    { "Render A" };
    juce::TextButton pluginBBtn   { "Plugin B" };
    juce::TextButton editPluginBBtn { "Show B" };
    juce::TextButton renderBBtn   { "Render B" };
    juce::TextButton pluginCBtn   { "Plugin C" };
    juce::TextButton editPluginCBtn { "Show C" };
    juce::TextButton renderCBtn   { "Render C" };
    juce::TextButton exportBtn    { "Export" };
    juce::TextButton resetBtn     { "Reset" };
    juce::ComboBox viewMode;
    juce::ComboBox freqZoom;
    juce::ComboBox themeMode;
    juce::Label statusLabel;
    juce::Label pluginSlotLabel;
    AudioDoctorPopupLookAndFeel audioDoctorPopupLookAndFeel;

    std::unique_ptr<juce::FileChooser> audioChooser;
    std::unique_ptr<juce::FileChooser> pluginChooser;
    juce::File lastAudioDirectory;
    juce::File lastPluginDirectory;

    std::unique_ptr<Asset> dryAsset;
    std::unique_ptr<Asset> dryBAsset;
    std::unique_ptr<Asset> dryCAsset;
    std::unique_ptr<Asset> wetAsset;
    std::unique_ptr<Asset> wetBAsset;
    std::unique_ptr<Asset> wetCAsset;
    goodmeter::audio_doctor::PluginHost pluginHostA;
    goodmeter::audio_doctor::PluginHost pluginHostB;
    goodmeter::audio_doctor::PluginHost pluginHostC;
    std::unique_ptr<PluginEditorWindow> pluginEditorWindowA;
    std::unique_ptr<PluginEditorWindow> pluginEditorWindowB;
    std::unique_ptr<PluginEditorWindow> pluginEditorWindowC;
    std::unique_ptr<juce::DocumentWindow> audioEditWindow;
    std::unique_ptr<BusRoutingWindow> busRoutingWindow;
    std::unique_ptr<GenerateSignalWindow> generateSignalWindow;
    std::array<SourceSlot, 3> displaySlots { SourceSlot::dryA, SourceSlot::wetA, SourceSlot::wetB };
    std::array<std::array<bool, 3>, 3> renderRoutes {{
        {{ true, false, false }},
        {{ true, false, false }},
        {{ true, false, false }}
    }};
    bool allowMixedRenderInputs = false;
    std::unique_ptr<Asset> renderReferenceA;
    std::unique_ptr<Asset> renderReferenceB;
    std::unique_ptr<Asset> renderReferenceC;

    juce::File exportDirectory;
    std::thread renderThread;
    std::shared_ptr<std::atomic<bool>> aliveFlag { std::make_shared<std::atomic<bool>>(true) };
    std::atomic<bool> rendering { false };
    float frequencyMinHz = 20.0f;
    float frequencyMaxHz = 20000.0f;

    juce::PluginDescription lastPluginDescriptionA;
    juce::PluginDescription lastPluginDescriptionB;
    juce::PluginDescription lastPluginDescriptionC;
    bool hasPluginRenderA = false;
    bool hasPluginRenderB = false;
    bool hasPluginRenderC = false;
    int lastLatencySamplesA = 0;
    int lastLatencySamplesB = 0;
    int lastLatencySamplesC = 0;
    double lastTailSecondsA = 0.0;
    double lastTailSecondsB = 0.0;
    double lastTailSecondsC = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioDoctorContent)
};
