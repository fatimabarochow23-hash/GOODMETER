/*
  ==============================================================================
    iOSMainComponent.h
    GOODMETER iOS - Root component with two-page horizontal swipe navigation

    Page 1 (NonoPageComponent): Nono/Guoba character, file import, playback
    Page 2 (MetersPageComponent): 8 scrollable meter cards

    Navigation: horizontal swipe between pages, page indicator dots at bottom
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "iOSPluginDefines.h"
#include "../PluginProcessor.h"
#include "../GoodMeterLookAndFeel.h"
#include "iOSAudioEngine.h"
#include "NonoPageComponent.h"
#include "MetersPageComponent.h"

class iOSMainComponent : public juce::Component
{
public:
    iOSMainComponent()
    {
        setLookAndFeel(&lookAndFeel);

        // Create processor and audio engine
        processor = std::make_unique<GOODMETERAudioProcessor>();
        audioEngine = std::make_unique<iOSAudioEngine>(*processor);

        // Create pages
        nonoPage = std::make_unique<NonoPageComponent>(*processor, *audioEngine);
        metersPage = std::make_unique<MetersPageComponent>(*processor, *audioEngine);

        addAndMakeVisible(nonoPage.get());
        addChildComponent(metersPage.get());  // hidden initially

        // Page dots
        setSize(400, 800);
    }

    ~iOSMainComponent() override
    {
        setLookAndFeel(nullptr);
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(GoodMeterLookAndFeel::bgMain);

        // Draw page indicator dots
        auto bounds = getLocalBounds();
        float dotY = static_cast<float>(bounds.getHeight()) - 30.0f;
        float dotSpacing = 16.0f;
        float dotR = 4.0f;
        float totalW = dotSpacing;
        float startX = bounds.getCentreX() - totalW / 2.0f;

        for (int i = 0; i < 2; ++i)
        {
            float x = startX + static_cast<float>(i) * dotSpacing;
            bool active = (i == currentPage);

            g.setColour(active ? GoodMeterLookAndFeel::textMain
                               : GoodMeterLookAndFeel::textMuted.withAlpha(0.3f));
            g.fillEllipse(x - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Reserve space for page dots at bottom
        auto contentArea = bounds.withTrimmedBottom(40);

        nonoPage->setBounds(contentArea);
        metersPage->setBounds(contentArea);
    }

    //==========================================================================
    // Horizontal swipe navigation
    //==========================================================================
    void mouseDown(const juce::MouseEvent& e) override
    {
        swipeStartX = e.position.x;
        isSwiping = false;
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        float dx = e.position.x - swipeStartX;
        if (std::abs(dx) > 20.0f)
            isSwiping = true;
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (!isSwiping) return;

        float dx = e.position.x - swipeStartX;

        if (dx < -50.0f && currentPage == 0)
        {
            // Swipe left -> go to meters
            currentPage = 1;
            nonoPage->setVisible(false);
            metersPage->setVisible(true);
            repaint();
        }
        else if (dx > 50.0f && currentPage == 1)
        {
            // Swipe right -> go to nono
            currentPage = 0;
            metersPage->setVisible(false);
            nonoPage->setVisible(true);
            repaint();
        }

        isSwiping = false;
    }

private:
    GoodMeterLookAndFeel lookAndFeel;

    std::unique_ptr<GOODMETERAudioProcessor> processor;
    std::unique_ptr<iOSAudioEngine> audioEngine;

    std::unique_ptr<NonoPageComponent> nonoPage;
    std::unique_ptr<MetersPageComponent> metersPage;

    int currentPage = 0;  // 0 = Nono, 1 = Meters
    float swipeStartX = 0.0f;
    bool isSwiping = false;
};
