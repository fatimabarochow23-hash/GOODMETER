/*
  ==============================================================================
    MeterCardComponent.h
    GOODMETER - Collapsible meter card container

    REFACTORED: Fixed Web → JUCE migration traps:
    1. Removed juce::TextButton (Z-index occlusion)
    2. Hand-rolled 60Hz animation with parent layout triggering
    3. Defensive height calculation with fallback
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"

//==============================================================================
/**
 * Collapsible card component with smooth push-down animation
 * Implements hand-rolled animation to trigger parent relayout every frame
 */
class MeterCardComponent : public juce::Component,
                          public juce::Timer
{
public:
    //==========================================================================
    MeterCardComponent(const juce::String& title,
                      const juce::Colour& indicatorColour = GoodMeterLookAndFeel::accentPink,
                      bool defaultExpanded = false)
        : cardTitle(title),
          statusColour(indicatorColour),
          isExpanded(defaultExpanded)
    {
        // Initialize animation state
        currentHeight = static_cast<float>(getDesiredHeight());
        targetHeight = currentHeight;

        // Set initial size
        setSize(500, static_cast<int>(currentHeight));
    }

    ~MeterCardComponent() override
    {
        stopTimer();
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        // Draw card background with thick border
        GoodMeterLookAndFeel::drawCard(g, bounds);

        // Draw header background
        auto headerBounds = bounds.removeFromTop(headerHeight);

        // Header hover effect (manual hover tracking)
        if (isHeaderHovered)
        {
            g.setColour(GoodMeterLookAndFeel::border.withAlpha(0.1f));
            g.fillRect(headerBounds);
        }

        if (isExpanded)
        {
            // Draw bottom border of header
            g.setColour(GoodMeterLookAndFeel::border);
            g.fillRect(headerBounds.getX(),
                      headerBounds.getBottom() - 2,
                      headerBounds.getWidth(),
                      2);
        }

        // Draw status indicator dot
        auto dotX = static_cast<float>(headerBounds.getX() + GoodMeterLookAndFeel::cardPadding);
        auto dotY = static_cast<float>(headerBounds.getCentreY()) - dotDiameter * 0.5f;
        GoodMeterLookAndFeel::drawStatusDot(g, dotX, dotY, dotDiameter, statusColour);

        // Draw title text
        auto textBounds = headerBounds.withTrimmedLeft(GoodMeterLookAndFeel::cardPadding +
                                                       static_cast<int>(dotDiameter) + 12);
        g.setColour(GoodMeterLookAndFeel::textMain);
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText(cardTitle.toUpperCase(),
                  textBounds,
                  juce::Justification::centredLeft,
                  false);

        // Draw expand/collapse arrow
        auto arrowBounds = headerBounds.removeFromRight(40);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(isExpanded ? "▼" : "▶",
                  arrowBounds,
                  juce::Justification::centred,
                  false);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        // Skip header area
        bounds.removeFromTop(headerHeight);

        // Position content (if expanded and exists)
        if (contentComponent != nullptr)
        {
            auto contentBounds = bounds.reduced(GoodMeterLookAndFeel::cardPadding);
            contentComponent->setBounds(contentBounds);
            contentComponent->setVisible(isExpanded || isAnimating);
        }
    }

    //==========================================================================
    /**
     * Mouse handling for header clicks (replaces juce::TextButton)
     */
    void mouseDown(const juce::MouseEvent& event) override
    {
        // Check if click is within header area
        if (event.y <= headerHeight)
        {
            setExpanded(!isExpanded, true);
        }
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        bool wasHovered = isHeaderHovered;
        isHeaderHovered = (event.y <= headerHeight);

        if (wasHovered != isHeaderHovered)
            repaint();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (isHeaderHovered)
        {
            isHeaderHovered = false;
            repaint();
        }
    }

    //==========================================================================
    /**
     * Set the content component to display inside the card
     */
    void setContentComponent(std::unique_ptr<juce::Component> newContent)
    {
        if (contentComponent != nullptr)
            removeChildComponent(contentComponent.get());

        contentComponent = std::move(newContent);

        if (contentComponent != nullptr)
        {
            addAndMakeVisible(contentComponent.get());
            contentComponent->setVisible(isExpanded);

            // Recalculate heights with new content
            targetHeight = static_cast<float>(getDesiredHeight());
            currentHeight = targetHeight;
            setSize(getWidth(), static_cast<int>(currentHeight));

            resized();
        }
    }

    /**
     * Get the content component
     */
    juce::Component* getContentComponent() const
    {
        return contentComponent.get();
    }

    //==========================================================================
    /**
     * Toggle expand/collapse state with smooth animation
     */
    void setExpanded(bool shouldExpand, bool animate = true)
    {
        if (isExpanded == shouldExpand)
            return;

        isExpanded = shouldExpand;
        targetHeight = static_cast<float>(getDesiredHeight());

        if (animate)
        {
            // Start hand-rolled 60Hz animation
            isAnimating = true;
            startTimerHz(60);

            // Show content immediately for expand, hide after animation for collapse
            if (contentComponent != nullptr && shouldExpand)
                contentComponent->setVisible(true);
        }
        else
        {
            // Instant transition
            currentHeight = targetHeight;
            setSize(getWidth(), static_cast<int>(currentHeight));

            if (contentComponent != nullptr)
                contentComponent->setVisible(shouldExpand);

            // Trigger parent relayout immediately
            if (auto* parent = getParentComponent())
                parent->resized();
        }

        repaint();
    }

    bool getExpanded() const { return isExpanded; }

    /**
     * Calculate desired height based on expanded state
     * Defensive: returns fallback if content has no size
     */
    int getDesiredHeight() const
    {
        if (!isExpanded)
            return headerHeight;

        int contentHeight = 0;
        if (contentComponent != nullptr)
        {
            contentHeight = contentComponent->getHeight();

            // Defensive fallback: if content has no height, use default
            if (contentHeight <= 0)
                contentHeight = defaultContentHeight;

            contentHeight += GoodMeterLookAndFeel::cardPadding * 2;
        }

        return headerHeight + contentHeight;
    }

    //==========================================================================
    /**
     * Hand-rolled 60Hz animation timer callback
     * CRITICAL: Calls parent->resized() every frame for smooth push-down effect
     */
    void timerCallback() override
    {
        // Eased interpolation (ease-out cubic)
        const float delta = targetHeight - currentHeight;
        const float smoothingFactor = 0.2f;  // Higher = faster animation

        currentHeight += delta * smoothingFactor;

        // Update component size
        setSize(getWidth(), static_cast<int>(std::round(currentHeight)));

        // 🔥 CRITICAL: Force parent to relayout ALL cards every frame
        // This creates the smooth "push-down" effect for cards below
        if (auto* parent = getParentComponent())
            parent->resized();

        // Stop animation when close enough (< 1px difference)
        if (std::abs(delta) < 1.0f)
        {
            // Snap to target and stop
            currentHeight = targetHeight;
            setSize(getWidth(), static_cast<int>(currentHeight));

            stopTimer();
            isAnimating = false;

            // Hide content after collapse animation finishes
            if (contentComponent != nullptr && !isExpanded)
                contentComponent->setVisible(false);

            // Final parent relayout
            if (auto* parent = getParentComponent())
                parent->resized();

            repaint();
        }
    }

private:
    juce::String cardTitle;
    juce::Colour statusColour;
    bool isExpanded;
    bool isAnimating = false;
    bool isHeaderHovered = false;

    std::unique_ptr<juce::Component> contentComponent;

    // Animation state
    float currentHeight = 0.0f;
    float targetHeight = 0.0f;

    // Constants
    static constexpr int headerHeight = 48;
    static constexpr float dotDiameter = 14.0f;
    static constexpr int defaultContentHeight = 150;  // Fallback when content has no size

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterCardComponent)
};
