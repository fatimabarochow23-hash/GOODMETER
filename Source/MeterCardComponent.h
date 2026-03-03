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
    // Callback for height changes (to notify PluginEditor)
    std::function<void()> onHeightChanged;

    //==========================================================================
    MeterCardComponent(const juce::String& title,
                      const juce::Colour& indicatorColour = GoodMeterLookAndFeel::accentPink,
                      bool defaultExpanded = false)
        : cardTitle(title),
          statusColour(indicatorColour),
          isExpanded(defaultExpanded)
    {
        // Initialize animation state
        // Note: getDesiredHeight() will be called again in setContentComponent()
        // after content is set, so this is just the header-only initial state
        currentHeight = static_cast<float>(headerHeight);
        targetHeight = currentHeight;

        // Set initial size (header-only at construction)
        setSize(500, headerHeight);
    }

    ~MeterCardComponent() override
    {
        stopTimer();
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        // Draw Neo-Brutalist card (hard shadow + offset body)
        GoodMeterLookAndFeel::drawCard(g, bounds, currentHoverOffset);

        // All header elements drawn relative to the card body (not full bounds)
        auto cardRect = getCardRect();
        auto headerBounds = cardRect.removeFromTop(headerHeight);

        // Header hover highlight
        if (isHeaderHovered)
        {
            g.setColour(GoodMeterLookAndFeel::border.withAlpha(0.08f));
            g.fillRect(headerBounds.toFloat());
        }

        if (isExpanded)
        {
            // Bottom border of header
            g.setColour(juce::Colour(0xFF1A1A24));
            g.fillRect(static_cast<int>(headerBounds.getX()),
                      static_cast<int>(headerBounds.getBottom()) - 2,
                      static_cast<int>(headerBounds.getWidth()),
                      2);
        }

        // Status indicator dot
        auto dotX = headerBounds.getX() + GoodMeterLookAndFeel::cardPadding;
        auto dotY = headerBounds.getCentreY() - dotDiameter * 0.5f;
        GoodMeterLookAndFeel::drawStatusDot(g, dotX, dotY, dotDiameter, statusColour);

        // Title text
        auto textBounds = headerBounds.withTrimmedLeft(
            static_cast<int>(GoodMeterLookAndFeel::cardPadding + dotDiameter + 12.0f));
        g.setColour(GoodMeterLookAndFeel::textMain);
        g.setFont(juce::Font(15.0f, juce::Font::bold));
        g.drawText(cardTitle.toUpperCase(),
                  textBounds.toNearestInt(),
                  juce::Justification::centredLeft,
                  false);

        // Expand/collapse arrow
        auto arrowBounds = headerBounds.removeFromRight(40.0f);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(isExpanded ? juce::String(juce::CharPointer_UTF8(u8"\xe2\x96\xbc"))
                              : juce::String(juce::CharPointer_UTF8(u8"\xe2\x96\xb6")),
                  arrowBounds.toNearestInt(),
                  juce::Justification::centred,
                  false);

        // Position header widget inside card body
        if (headerWidget != nullptr)
        {
            auto cr = getCardRect();
            const int widgetW = juce::jlimit(80, 140, static_cast<int>(cr.getWidth() * 0.3f));
            const int widgetH = 26;
            headerWidget->setBounds(
                static_cast<int>(cr.getRight()) - widgetW - 40,
                static_cast<int>(cr.getY()) + (headerHeight - widgetH) / 2,
                widgetW,
                widgetH
            );
        }
    }

    void resized() override
    {
        // Content positioned inside the card body (accounting for shadow offset)
        if (contentComponent != nullptr)
        {
            auto cr = getCardRect();
            const int padding = static_cast<int>(GoodMeterLookAndFeel::cardPadding);
            const int availableHeight = juce::jmax(0, static_cast<int>(cr.getHeight()) - headerHeight - padding * 2);
            contentComponent->setBounds(
                static_cast<int>(cr.getX()) + padding,
                static_cast<int>(cr.getY()) + headerHeight,
                static_cast<int>(cr.getWidth()) - padding * 2,
                availableHeight
            );
            contentComponent->setVisible(isExpanded || isAnimating);
        }
    }

    //==========================================================================
    /**
     * Set an optional widget to display in the header (e.g. ComboBox for Levels)
     * The card positions it in the header area, to the right of the title.
     * Clicks on this widget do NOT trigger expand/collapse.
     */
    void setHeaderWidget(juce::Component* widget)
    {
        headerWidget = widget;
        if (headerWidget != nullptr)
            addAndMakeVisible(headerWidget);
    }

    //==========================================================================
    /**
     * Mouse handling for header clicks (replaces juce::TextButton)
     * Skip toggle if click is on the header widget (ComboBox)
     */
    void mouseDown(const juce::MouseEvent& event) override
    {
        // Check if click is within card body's header area
        auto cr = getCardRect();
        float localY = static_cast<float>(event.y) - cr.getY();
        if (localY >= 0 && localY <= static_cast<float>(headerHeight))
        {
            // Don't toggle if clicking on the header widget
            if (headerWidget != nullptr)
            {
                auto widgetBounds = headerWidget->getBounds();
                if (widgetBounds.contains(event.x, event.y))
                    return;
            }
            setExpanded(!isExpanded, true);
        }
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        bool wasHovered = isHeaderHovered;
        auto cr = getCardRect();
        float localY = static_cast<float>(event.y) - cr.getY();
        isHeaderHovered = (localY >= 0 && localY <= static_cast<float>(headerHeight)
                          && cr.contains(static_cast<float>(event.x), static_cast<float>(event.y)));

        if (wasHovered != isHeaderHovered)
            repaint();
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        isCardHovered = true;
        ensureTimerRunning();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        isCardHovered = false;
        isHeaderHovered = false;
        ensureTimerRunning();
        repaint();
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

            // CRITICAL: Recalculate heights with new content
            // This must happen AFTER content is set, not in constructor
            int desiredHeight = getDesiredHeight();
            targetHeight = static_cast<float>(desiredHeight);
            currentHeight = targetHeight;  // Snap to target (no animation on initial set)

            setSize(getWidth(), desiredHeight);

            resized();

            // Notify PluginEditor to relayout all cards
            if (onHeightChanged)
                onHeightChanged();
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
            ensureTimerRunning();

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

            // Notify PluginEditor to relayout
            if (onHeightChanged)
                onHeightChanged();
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
        bool needsMoreFrames = false;

        // --- Collapse/expand height animation ---
        const float heightDelta = targetHeight - currentHeight;
        if (std::abs(heightDelta) >= 1.0f)
        {
            currentHeight += heightDelta * 0.2f;
            setSize(getWidth(), static_cast<int>(std::round(currentHeight)));

            if (onHeightChanged)
                onHeightChanged();

            needsMoreFrames = true;
        }
        else if (isAnimating)
        {
            // Snap to target
            currentHeight = targetHeight;
            setSize(getWidth(), static_cast<int>(currentHeight));
            isAnimating = false;

            if (contentComponent != nullptr && !isExpanded)
                contentComponent->setVisible(false);

            if (onHeightChanged)
                onHeightChanged();
        }

        // --- Hover offset animation (Neo-Brutalism shadow depth) ---
        float targetOffset = isCardHovered ? 8.0f : 4.0f;
        float hoverDelta = targetOffset - currentHoverOffset;
        if (std::abs(hoverDelta) > 0.1f)
        {
            currentHoverOffset += hoverDelta * 0.3f;
            needsMoreFrames = true;
        }
        else
        {
            currentHoverOffset = targetOffset;
        }

        repaint();

        // Stop timer when all animations are done
        if (!needsMoreFrames && !isAnimating)
            stopTimer();
    }

private:
    juce::String cardTitle;
    juce::Colour statusColour;
    bool isExpanded;
    bool isAnimating = false;
    bool isHeaderHovered = false;
    bool isCardHovered = false;

    std::unique_ptr<juce::Component> contentComponent;

    // Optional header widget (e.g. ComboBox for Levels card)
    juce::Component* headerWidget = nullptr;  // Non-owning pointer

    // Animation state
    float currentHeight = 0.0f;
    float targetHeight = 0.0f;

    // Neo-Brutalism hover offset (4.0 = resting, 8.0 = hovered/lifted)
    float currentHoverOffset = 4.0f;

    // Constants
    static constexpr int headerHeight = 48;
    static constexpr float dotDiameter = 14.0f;
    static constexpr int defaultContentHeight = 150;
    static constexpr float maxShadowOffset = 8.0f;

    //==========================================================================
    /** Compute the card body rectangle (excludes shadow area) */
    juce::Rectangle<float> getCardRect() const
    {
        auto b = getLocalBounds().toFloat();
        float cardX = b.getX() + (maxShadowOffset - currentHoverOffset);
        float cardY = b.getY() + (maxShadowOffset - currentHoverOffset);
        float cardW = b.getWidth() - maxShadowOffset;
        float cardH = b.getHeight() - maxShadowOffset;
        return { cardX, cardY, cardW, cardH };
    }

    /** Start the 60Hz timer if not already running */
    void ensureTimerRunning()
    {
        if (!isTimerRunning())
            startTimerHz(60);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterCardComponent)
};
