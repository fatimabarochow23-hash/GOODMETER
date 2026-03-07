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

    // Jiggle mode: suppress expand/collapse clicks during drag-sort
    bool inJiggleMode = false;

    // Docked mode: card is in read-only shelf, no interaction allowed
    bool isDocked = false;

    // Mini mode: compact layout with aggressive space squeezing
    bool isMiniMode = false;

    // Floating mode callbacks (wired by StandaloneNonoEditor for undock/drag)
    std::function<void(MeterCardComponent*, const juce::MouseEvent&)> onUndockDragStarted;
    std::function<void(MeterCardComponent*, const juce::MouseEvent&)> onFloatingDragging;
    std::function<void(MeterCardComponent*, const juce::MouseEvent&)> onFloatingDragEnded;
    std::function<void(MeterCardComponent*)> onDetachRequested;

    // Show detach button (×) when card is in a snap group
    bool showDetachButton = false;

    // Resize callback (notifies editor when card is resized by user drag)
    std::function<void(MeterCardComponent*, int newW, int newH)> onResized;

    // Custom card width/height (set by resize drag; -1 = default)
    int customWidth = -1;
    int customContentHeight = -1;

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
        currentHeight = static_cast<float>(getActiveHeaderHeight());
        targetHeight = currentHeight;

        // Set initial size (header-only at construction)
        setSize(500, getActiveHeaderHeight());
    }

    ~MeterCardComponent() override
    {
        stopTimer();
    }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds();
        const int hh = getActiveHeaderHeight();

        // Draw Neo-Brutalist card (hard shadow + offset body)
        float shadowOff = isMiniMode ? juce::jmin(currentHoverOffset, 2.0f) : currentHoverOffset;
        GoodMeterLookAndFeel::drawCard(g, bounds, shadowOff);

        // All header elements drawn relative to the card body (not full bounds)
        auto cardRect = getCardRect();
        auto headerBounds = cardRect.removeFromTop(hh);

        // Header hover highlight
        if (isHeaderHovered)
        {
            g.setColour(GoodMeterLookAndFeel::border.withAlpha(0.08f));
            g.fillRect(headerBounds.toFloat());
        }

        if (isExpanded)
        {
            // Bottom border of header — fillRect 画在 header 底边上方，防止被 content 遮盖
            g.setColour(juce::Colour(0xFF1A1A24));
            int borderH = isMiniMode ? 1 : 2;
            g.fillRect(static_cast<int>(headerBounds.getX()),
                      static_cast<int>(headerBounds.getBottom()) - borderH - 1,
                      static_cast<int>(headerBounds.getWidth()),
                      borderH);
        }

        // Status indicator dot
        float dd = isMiniMode ? 8.0f : dotDiameter;
        float pad = isMiniMode ? 4.0f : GoodMeterLookAndFeel::cardPadding;
        auto dotX = headerBounds.getX() + pad;
        auto dotY = headerBounds.getCentreY() - dd * 0.5f;
        GoodMeterLookAndFeel::drawStatusDot(g, dotX, dotY, dd, statusColour);

        // Title + arrow: blit from pre-rendered cache (zero drawText in hot path)
        {
            int cacheW = static_cast<int>(headerBounds.getWidth());
            bool needsRebuild = headerTextCache.isNull() ||
                                lastHeaderCacheW != cacheW ||
                                lastHeaderCacheMini != isMiniMode ||
                                lastHeaderCacheExpanded != isExpanded ||
                                lastHeaderCacheDocked != isDocked;

            if (needsRebuild)
            {
                lastHeaderCacheW = cacheW;
                lastHeaderCacheMini = isMiniMode;
                lastHeaderCacheExpanded = isExpanded;
                lastHeaderCacheDocked = isDocked;
                headerTextCache = juce::Image(juce::Image::ARGB, cacheW, hh, true, juce::SoftwareImageType());
                juce::Graphics tg(headerTextCache);

                // Title text
                float titleFont = isMiniMode ? 10.0f : 15.0f;
                float localPad = isMiniMode ? 4.0f : GoodMeterLookAndFeel::cardPadding;
                float localDd = isMiniMode ? 8.0f : dotDiameter;
                auto textArea = juce::Rectangle<int>(
                    static_cast<int>(localPad + localDd + (isMiniMode ? 4.0f : 12.0f)), 0,
                    cacheW - static_cast<int>(localPad + localDd + (isMiniMode ? 4.0f : 12.0f)), hh);
                tg.setColour(isDocked ? GoodMeterLookAndFeel::textMuted : GoodMeterLookAndFeel::textMain);
                tg.setFont(juce::Font(titleFont, juce::Font::bold));
                tg.drawText(cardTitle.toUpperCase(), textArea, juce::Justification::centredLeft, false);

                // Expand/collapse arrow — hidden when docked; grip icon shown instead
                if (!isDocked)
                {
                    float localArrowW = isMiniMode ? 20.0f : 40.0f;
                    auto arrowArea = juce::Rectangle<int>(cacheW - static_cast<int>(localArrowW), 0,
                                                           static_cast<int>(localArrowW), hh);
                    float arrowFont = isMiniMode ? 9.0f : 14.0f;
                    tg.setFont(juce::Font(arrowFont, juce::Font::bold));
                    tg.drawText(isExpanded ? juce::String(juce::CharPointer_UTF8(u8"\xe2\x96\xbc"))
                                           : juce::String(juce::CharPointer_UTF8(u8"\xe2\x96\xb6")),
                                arrowArea, juce::Justification::centred, false);
                }
                else
                {
                    // ⋮⋮ Grip icon: 2×3 dot grid — visual drag handle hint
                    float localArrowW = isMiniMode ? 20.0f : 40.0f;
                    float gripCX = static_cast<float>(cacheW) - localArrowW * 0.5f;
                    float gripCY = static_cast<float>(hh) * 0.5f;
                    float dotR = isMiniMode ? 1.5f : 2.5f;
                    float gapX = isMiniMode ? 4.0f : 6.0f;
                    float gapY = isMiniMode ? 3.5f : 5.0f;
                    tg.setColour(GoodMeterLookAndFeel::textMuted);
                    for (int row = -1; row <= 1; ++row)
                        for (int col = 0; col < 2; ++col)
                            tg.fillEllipse(gripCX + (col == 0 ? -gapX * 0.5f : gapX * 0.5f) - dotR,
                                           gripCY + static_cast<float>(row) * gapY - dotR,
                                           dotR * 2.0f, dotR * 2.0f);
                }
            }
            g.drawImageAt(headerTextCache,
                          static_cast<int>(headerBounds.getX()),
                          static_cast<int>(headerBounds.getY()));
        }

        // Detach button (×) — drawn OVER the header cache, only when showDetachButton
        if (showDetachButton && !isDocked)
        {
            auto cr = getCardRect();
            float btnSz = isMiniMode ? 14.0f : 20.0f;
            float btnX = cr.getRight() - btnSz - 4.0f;
            float btnY = cr.getY() + (static_cast<float>(hh) - btnSz) * 0.5f;

            // Store for hit testing
            detachButtonRect = juce::Rectangle<float>(btnX, btnY, btnSz, btnSz);

            // Background circle
            bool hovered = isDetachHovered;
            g.setColour(hovered ? juce::Colour(0xFFE6335F).withAlpha(0.9f)
                                : juce::Colour(0xFF2A2A35).withAlpha(0.6f));
            g.fillEllipse(btnX, btnY, btnSz, btnSz);

            // × cross
            g.setColour(juce::Colours::white.withAlpha(hovered ? 1.0f : 0.8f));
            float m = btnSz * 0.3f;
            g.drawLine(btnX + m, btnY + m, btnX + btnSz - m, btnY + btnSz - m, 2.0f);
            g.drawLine(btnX + btnSz - m, btnY + m, btnX + m, btnY + btnSz - m, 2.0f);
        }
        else
        {
            detachButtonRect = {};
        }

        // Resize grip triangle (bottom-right corner, only when expanded + floating)
        if (isExpanded && !isDocked)
        {
            auto cr2 = getCardRect();
            float gripSz = isMiniMode ? 10.0f : 14.0f;
            float gx = cr2.getRight() - gripSz - 2.0f;
            float gy = cr2.getBottom() - gripSz - 2.0f;

            // Store rect for hit testing
            resizeGripRect = juce::Rectangle<float>(gx - 4.0f, gy - 4.0f,
                                                      gripSz + 6.0f, gripSz + 6.0f);

            // Draw 3 diagonal lines (classic resize grip)
            float alpha = isResizeHovered ? 0.7f : 0.35f;
            g.setColour(GoodMeterLookAndFeel::textMuted.withAlpha(alpha));
            for (int ln = 0; ln < 3; ++ln)
            {
                float off = static_cast<float>(ln) * (gripSz / 3.0f);
                g.drawLine(gx + gripSz - off, gy + gripSz,
                           gx + gripSz, gy + gripSz - off, 1.5f);
            }
        }
        else
        {
            resizeGripRect = {};
        }

        // Position header widget inside card body
        if (headerWidget != nullptr)
        {
            auto cr = getCardRect();
            const int widgetW = juce::jlimit(60, 140, static_cast<int>(cr.getWidth() * 0.3f));
            const int widgetH = isMiniMode ? 18 : 26;
            headerWidget->setBounds(
                static_cast<int>(cr.getRight()) - widgetW - static_cast<int>(isMiniMode ? 20.0f : 40.0f),
                static_cast<int>(cr.getY()) + (hh - widgetH) / 2,
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
            const int hh = getActiveHeaderHeight();
            const int padding = isMiniMode ? 2 : static_cast<int>(GoodMeterLookAndFeel::cardPadding);
            const int availableHeight = juce::jmax(0, static_cast<int>(cr.getHeight()) - hh - padding * 2);
            contentComponent->setBounds(
                static_cast<int>(cr.getX()) + padding,
                static_cast<int>(cr.getY()) + hh,
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
     * Mouse handling: unified drag-or-click system
     * - Docked: drag → undock; click → nothing (locked)
     * - Floating: drag → reposition; click → expand/collapse toggle
     * Distinguished by 4px threshold (same pattern as HoloNono window drag)
     */
    void mouseDown(const juce::MouseEvent& event) override
    {
        if (inJiggleMode) return;

        // Check resize grip hit (highest priority — only when expanded + floating)
        if (isExpanded && !isDocked && !resizeGripRect.isEmpty())
        {
            if (resizeGripRect.contains(static_cast<float>(event.x),
                                          static_cast<float>(event.y)))
            {
                isResizeDragging = true;
                resizeDragStart = event.getScreenPosition();
                resizeStartW = getWidth();
                resizeStartH = getHeight();
                return;
            }
        }

        auto cr = getCardRect();
        const int hh = getActiveHeaderHeight();
        float localY = static_cast<float>(event.y) - cr.getY();

        // Only handle clicks in the header area
        if (localY < 0 || localY > static_cast<float>(hh)) return;

        // Check detach button hit (highest priority)
        if (showDetachButton && !isDocked && !detachButtonRect.isEmpty())
        {
            if (detachButtonRect.contains(static_cast<float>(event.x),
                                           static_cast<float>(event.y)))
            {
                if (onDetachRequested)
                    onDetachRequested(this);
                return;
            }
        }

        if (isDocked)
        {
            // Start tracking for potential undock drag
            isDragTracking = true;
            isDragActivated = false;
            dragScreenStart = event.getScreenPosition();
            return;
        }

        // Not docked: skip if clicking on header widget (ComboBox)
        if (headerWidget != nullptr)
        {
            auto widgetBounds = headerWidget->getBounds();
            if (widgetBounds.contains(event.x, event.y))
                return;
        }

        // Start tracking for floating drag OR click-to-toggle
        isDragTracking = true;
        isDragActivated = false;
        dragScreenStart = event.getScreenPosition();
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        // Resize drag takes priority
        if (isResizeDragging)
        {
            auto delta = event.getScreenPosition() - resizeDragStart;
            int newW = juce::jmax(minResizeW, resizeStartW + delta.x);
            int newH = juce::jmax(getActiveHeaderHeight() + minResizeContentH, resizeStartH + delta.y);

            customWidth = newW - static_cast<int>(maxShadowOffset);  // store without shadow
            customContentHeight = newH - getActiveHeaderHeight() - static_cast<int>(maxShadowOffset);

            // Update target height to match new content height
            targetHeight = static_cast<float>(newH);
            currentHeight = targetHeight;
            setSize(newW, newH);

            if (contentComponent != nullptr)
            {
                contentComponent->setVisible(true);
                resized();
            }

            if (onResized)
                onResized(this, newW, newH);
            if (onHeightChanged)
                onHeightChanged();

            repaint();
            return;
        }

        if (!isDragTracking) return;

        auto delta = event.getScreenPosition() - dragScreenStart;

        if (!isDragActivated && delta.getDistanceFromOrigin() > 4)
        {
            isDragActivated = true;
            // Fire undock/drag-start for BOTH docked and floating cards
            if (onUndockDragStarted)
                onUndockDragStarted(this, event);
        }

        if (isDragActivated && onFloatingDragging)
            onFloatingDragging(this, event);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (isResizeDragging)
        {
            isResizeDragging = false;
            return;
        }

        if (!isDragTracking) return;

        bool wasDragActivated = isDragActivated;
        isDragTracking = false;
        isDragActivated = false;

        if (wasDragActivated)
        {
            // Drag ended — notify parent
            if (onFloatingDragEnded)
                onFloatingDragEnded(this, event);
        }
        else if (!isDocked)
        {
            // Click without drag — toggle expand/collapse
            setExpanded(!isExpanded, true);
        }
    }

    void mouseMove(const juce::MouseEvent& event) override
    {
        if (isDocked) return;

        bool wasHovered = isHeaderHovered;
        bool wasDetachHovered = isDetachHovered;
        bool wasResizeHovered = isResizeHovered;
        auto cr = getCardRect();
        const int hh = getActiveHeaderHeight();
        float localY = static_cast<float>(event.y) - cr.getY();
        isHeaderHovered = (localY >= 0 && localY <= static_cast<float>(hh)
                          && cr.contains(static_cast<float>(event.x), static_cast<float>(event.y)));

        // Track detach button hover
        isDetachHovered = showDetachButton && !detachButtonRect.isEmpty()
                          && detachButtonRect.contains(static_cast<float>(event.x),
                                                        static_cast<float>(event.y));

        // Track resize grip hover
        isResizeHovered = isExpanded && !resizeGripRect.isEmpty()
                          && resizeGripRect.contains(static_cast<float>(event.x),
                                                      static_cast<float>(event.y));

        if (wasHovered != isHeaderHovered || wasDetachHovered != isDetachHovered
            || wasResizeHovered != isResizeHovered)
            repaint();
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        if (isDocked) return;
        isCardHovered = true;
        ensureTimerRunning();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (isDocked) return;
        isCardHovered = false;
        isHeaderHovered = false;
        isDetachHovered = false;
        isResizeHovered = false;
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
        const int hh = getActiveHeaderHeight();
        if (!isExpanded)
            return hh;

        int contentHeight = 0;
        if (contentComponent != nullptr)
        {
            // Use custom content height if set by resize drag
            if (customContentHeight > 0)
                contentHeight = customContentHeight;
            else
                contentHeight = contentComponent->getHeight();

            // Defensive fallback: if content has no height, use default
            if (contentHeight <= 0)
                contentHeight = defaultContentHeight;

            int pad = isMiniMode ? 2 : static_cast<int>(GoodMeterLookAndFeel::cardPadding);
            contentHeight += pad * 2;
        }

        return hh + contentHeight;
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
        if (std::abs(targetHeight - currentHeight) < 0.5f)
        {
            // Snap to target
            currentHeight = targetHeight;
            int finalH = static_cast<int>(currentHeight);
            if (getHeight() != finalH)
            {
                setSize(getWidth(), finalH);
                if (onHeightChanged) onHeightChanged();
            }

            if (isAnimating)
            {
                isAnimating = false;
                if (contentComponent != nullptr && !isExpanded)
                    contentComponent->setVisible(false);
            }
        }
        else
        {
            currentHeight += (targetHeight - currentHeight) * 0.2f;
            int newH = static_cast<int>(std::round(currentHeight));
            if (getHeight() != newH)
            {
                setSize(getWidth(), newH);
                if (onHeightChanged) onHeightChanged();
            }
            needsMoreFrames = true;
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

        // Only repaint if still animating; stop timer when all done
        if (needsMoreFrames || isAnimating)
        {
            repaint();
        }
        else
        {
            repaint();  // Final frame
            stopTimer();
        }
    }

private:
    juce::String cardTitle;
    juce::Colour statusColour;
    bool isExpanded;
    bool isAnimating = false;
    bool isHeaderHovered = false;
    bool isCardHovered = false;
    bool isDetachHovered = false;

    // Detach button rect (computed in paint, used for hit testing)
    mutable juce::Rectangle<float> detachButtonRect;

    // Resize grip rect (computed in paint, used for hit testing)
    mutable juce::Rectangle<float> resizeGripRect;
    bool isResizeHovered = false;
    bool isResizeDragging = false;
    juce::Point<int> resizeDragStart;
    int resizeStartW = 0;
    int resizeStartH = 0;

    // Drag tracking state (unified for undock-drag and floating-drag)
    bool isDragTracking = false;
    bool isDragActivated = false;
    juce::Point<int> dragScreenStart;

    std::unique_ptr<juce::Component> contentComponent;

    // Optional header widget (e.g. ComboBox for Levels card)
    juce::Component* headerWidget = nullptr;  // Non-owning pointer

    // === Offscreen header text cache (STATIC — only rebuild on resize/mode/expand/dock change) ===
    juce::Image headerTextCache;
    int lastHeaderCacheW = 0;
    bool lastHeaderCacheMini = false;
    bool lastHeaderCacheExpanded = false;
    bool lastHeaderCacheDocked = false;

    // Animation state
    float currentHeight = 0.0f;
    float targetHeight = 0.0f;

    // Neo-Brutalism hover offset (4.0 = resting, 8.0 = hovered/lifted)
    float currentHoverOffset = 4.0f;

    // Constants
    static constexpr int normalHeaderHeight = 48;
    static constexpr int miniHeaderHeight = 24;
    static constexpr float dotDiameter = 14.0f;
    static constexpr int defaultContentHeight = 150;
    static constexpr float maxShadowOffset = 8.0f;
    static constexpr int minResizeW = 180;            // minimum width during resize
    static constexpr int minResizeContentH = 80;      // minimum content height during resize

    int getActiveHeaderHeight() const { return isMiniMode ? miniHeaderHeight : normalHeaderHeight; }

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
