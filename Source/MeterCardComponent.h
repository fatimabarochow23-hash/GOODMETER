/*
  ==============================================================================
    MeterCardComponent.h
    GOODMETER - Collapsible meter card container

    Translated from MeterCard.tsx
    Features: Thick border, status dot, expand/collapse animation
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"

//==============================================================================
/**
 * Collapsible card component for meter modules
 * Implements the MeterCard.tsx design with expand/collapse functionality
 */
class MeterCardComponent : public juce::Component,
                          public juce::Button::Listener
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
        // Create header button (for expand/collapse)
        headerButton = std::make_unique<juce::TextButton>();
        headerButton->setButtonText("");  // Custom painting
        headerButton->addListener(this);
        addAndMakeVisible(headerButton.get());

        // Create content container
        contentComponent = std::make_unique<juce::Component>();
        addAndMakeVisible(contentComponent.get());
        contentComponent->setVisible(isExpanded);
    }

    ~MeterCardComponent() override
    {
        headerButton->removeListener(this);
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();

        // Draw card background with thick border
        GoodMeterLookAndFeel::drawCard(g, bounds);

        // Draw header background (hover effect handled by button)
        auto headerBounds = bounds.removeFromTop(headerHeight);
        if (isExpanded)
        {
            // Draw bottom border of header
            g.setColour(GoodMeterLookAndFeel::border);
            g.fillRect(headerBounds.removeFromBottom(2));
        }

        // Draw status indicator dot
        auto dotX = headerBounds.getX() + GoodMeterLookAndFeel::cardPadding;
        auto dotY = headerBounds.getCentreY() - dotDiameter * 0.5f;
        GoodMeterLookAndFeel::drawStatusDot(g, dotX, dotY, dotDiameter, statusColour);

        // Draw title text
        auto textBounds = headerBounds.withTrimmedLeft(GoodMeterLookAndFeel::cardPadding + dotDiameter + 12);
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

        // Position header button
        auto headerBounds = bounds.removeFromTop(headerHeight);
        headerButton->setBounds(headerBounds);

        // Position content (if expanded)
        if (isExpanded && contentComponent != nullptr)
        {
            auto contentBounds = bounds.reduced(GoodMeterLookAndFeel::cardPadding);
            contentComponent->setBounds(contentBounds);
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

    /**
     * Toggle expand/collapse state
     */
    void setExpanded(bool shouldExpand, bool animate = true)
    {
        if (isExpanded == shouldExpand)
            return;

        isExpanded = shouldExpand;

        if (contentComponent != nullptr)
        {
            if (animate)
            {
                // Animate height change
                juce::Desktop::getInstance().getAnimator().animateComponent(
                    this,
                    getBounds().withHeight(getDesiredHeight()),
                    1.0f,
                    200,  // 200ms duration (matches MeterCard.tsx)
                    false,
                    1.0,
                    0.0
                );

                contentComponent->setVisible(shouldExpand);
            }
            else
            {
                contentComponent->setVisible(shouldExpand);
                setSize(getWidth(), getDesiredHeight());
            }
        }

        repaint();
    }

    bool getExpanded() const { return isExpanded; }

    /**
     * Calculate desired height based on expanded state
     */
    int getDesiredHeight() const
    {
        if (!isExpanded)
            return headerHeight;

        int contentHeight = 0;
        if (contentComponent != nullptr)
            contentHeight = contentComponent->getHeight() + GoodMeterLookAndFeel::cardPadding * 2;

        return headerHeight + contentHeight;
    }

    //==========================================================================
    void buttonClicked(juce::Button* button) override
    {
        if (button == headerButton.get())
        {
            setExpanded(!isExpanded, true);
        }
    }

private:
    juce::String cardTitle;
    juce::Colour statusColour;
    bool isExpanded;

    std::unique_ptr<juce::TextButton> headerButton;
    std::unique_ptr<juce::Component> contentComponent;

    static constexpr int headerHeight = 48;
    static constexpr float dotDiameter = 14.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MeterCardComponent)
};
