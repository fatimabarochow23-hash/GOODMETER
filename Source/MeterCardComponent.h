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

    // Mobile list mode: no drag/resize/undock, only tap header to expand/collapse
    bool mobileListMode = false;
    bool mobileAllowHeaderToggle = true;

    // Dark theme mode
    bool isDarkTheme = false;

    // iOS page 2: dedicated editorial dark shell.
    // This is intentionally narrower than the shared "dark theme" concept so
    // page-2 can evolve without dragging standalone/plugin chrome along.
    bool useEditorialDarkStyle = false;

    // iOS page 2 light theme: keep the same framed system language as dark,
    // but with a translucent white plate so the Marathon background can breathe
    // through without affecting desktop/plugin cards.
    bool useEditorialLightStyle = false;

    // iOS page 2 / mobile chrome: unify English titles to the page-1 result
    // typography without touching shared desktop/plugin headers.
    bool useMonospacedTitleFont = false;

    // Floating mode callbacks (wired by StandaloneNonoEditor for undock/drag)
    std::function<void(MeterCardComponent*, const juce::MouseEvent&)> onUndockDragStarted;
    std::function<void(MeterCardComponent*, const juce::MouseEvent&)> onFloatingDragging;
    std::function<void(MeterCardComponent*, const juce::MouseEvent&)> onFloatingDragEnded;
    std::function<void(MeterCardComponent*)> onDetachRequested;

    // Arrow hover → ✕ detach mode (when card is in a snap group with showDetachButton)
    bool showDetachButton = false;

    // Resize callback (notifies editor when card is resized by user drag)
    std::function<void(MeterCardComponent*, int newW, int newH)> onResized;

    // Resize snap query: parent returns damped (snappedW, snappedH) for Apex-style sticky edges
    std::function<juce::Point<int>(MeterCardComponent*, int rawW, int rawH)> onResizeSnapQuery;

    // Resize ended: parent can commit final snap alignment on mouseUp
    std::function<void(MeterCardComponent*, int finalW, int finalH)> onResizeEnded;

    // Custom card width/height (set by resize drag; -1 = default)
    int customWidth = -1;
    int customContentHeight = -1;

    // Per-card preferred content height (set by parent; -1 = use defaultContentHeight)
    int preferredContentHeight = -1;

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
        auto bgCol = isDarkTheme ? GoodMeterLookAndFeel::bgPanelDark : GoodMeterLookAndFeel::bgPanel;
        auto borderCol = isDarkTheme ? GoodMeterLookAndFeel::borderDark : juce::Colour(0xFF1A1A24);

        if (useEditorialDarkStyle || useEditorialLightStyle)
        {
            auto b = bounds.toFloat();
            const float cr = 18.0f;
            const float maxShadow = 8.0f;
            const bool lightEditorial = useEditorialLightStyle && !useEditorialDarkStyle;

            if (b.getWidth() > maxShadow && b.getHeight() > maxShadow)
            {
                float cardX = b.getX() + (maxShadow - shadowOff);
                float cardY = b.getY() + (maxShadow - shadowOff);
                float cardW = b.getWidth() - maxShadow;
                float cardH = b.getHeight() - maxShadow;
                juce::Rectangle<float> cardRect(cardX, cardY, cardW, cardH);

                juce::Path cardPath;
                cardPath.addRoundedRectangle(cardRect, cr);

                auto shadowColour = lightEditorial
                                        ? juce::Colours::black.withAlpha(0.08f)
                                        : juce::Colours::black.withAlpha(0.24f);
                auto shellFill = lightEditorial
                                     ? juce::Colour(0xFFFFFFFF).withAlpha(0.56f)
                                     : juce::Colour(0xFF0B1017).withAlpha(0.30f);
                auto shellOutline = lightEditorial
                                        ? juce::Colour(0xFF1A1A24).withAlpha(0.16f)
                                        : juce::Colour(0xFFF2EEE7).withAlpha(0.22f);

                g.setColour(shadowColour);
                g.fillRoundedRectangle(cardRect.translated(shadowOff * 0.55f, shadowOff * 0.55f), cr);

                g.setColour(shellFill);
                g.fillPath(cardPath);

                g.setColour(shellOutline);
                g.drawRoundedRectangle(cardRect, cr, 1.2f);
            }
        }
        else
        {
            GoodMeterLookAndFeel::drawCard(g, bounds, shadowOff, bgCol, borderCol);
        }

        // All header elements drawn relative to the card body (not full bounds)
        auto cardRect = getCardRect();
        auto headerBounds = cardRect.removeFromTop(hh);

        // Header hover highlight
        if (isHeaderHovered)
        {
            auto hoverCol = useEditorialDarkStyle ? juce::Colour(0xFFF2EEE7).withAlpha(0.045f)
                            : useEditorialLightStyle ? juce::Colour(0xFF1A1A24).withAlpha(0.035f)
                            : isDarkTheme ? GoodMeterLookAndFeel::textMainDark.withAlpha(0.08f)
                                        : GoodMeterLookAndFeel::chartInk(0.12f);
            g.setColour(hoverCol);
            if (useEditorialDarkStyle || useEditorialLightStyle)
                g.fillRoundedRectangle(headerBounds.toFloat(), 14.0f);
            else
                g.fillRect(headerBounds.toFloat());
        }

        if (isExpanded)
        {
            // Bottom border of header
            auto headerBorderCol = useEditorialDarkStyle ? juce::Colour(0xFFF2EEE7).withAlpha(0.08f)
                                 : useEditorialLightStyle ? juce::Colour(0xFF1A1A24).withAlpha(0.10f)
                                 : isDarkTheme ? GoodMeterLookAndFeel::borderDark : juce::Colour(0xFF1A1A24);
            g.setColour(headerBorderCol);
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

        // Title + arrow
        {
            int cacheW = static_cast<int>(headerBounds.getWidth());
            bool arrowHidden = isArrowHovered && showDetachButton && !isDocked;
            if (GoodMeterLookAndFeel::preferDirectChartText())
            {
                float titleFont = GoodMeterLookAndFeel::chartFont(isMiniMode ? 10.0f : 15.0f);
                float localPad = isMiniMode ? 4.0f : GoodMeterLookAndFeel::cardPadding;
                float localDd = isMiniMode ? 8.0f : dotDiameter;
                auto textArea = juce::Rectangle<int>(
                    static_cast<int>(localPad + localDd + (isMiniMode ? 4.0f : 12.0f)), 0,
                    cacheW - static_cast<int>(localPad + localDd + (isMiniMode ? 4.0f : 12.0f)), hh);
                textArea.translate(static_cast<int>(headerBounds.getX()), static_cast<int>(headerBounds.getY()));
                auto textCol = useEditorialDarkStyle ? juce::Colour(0xFFF6EEE3)
                               : useEditorialLightStyle ? juce::Colour(0xFF1A1A24).withAlpha(0.96f)
                               : isDarkTheme ? GoodMeterLookAndFeel::textMainDark
                                           : (isDocked ? GoodMeterLookAndFeel::textMuted : GoodMeterLookAndFeel::textMain);
                g.setColour(textCol);
                g.setFont(useMonospacedTitleFont
                              ? GoodMeterLookAndFeel::iosEnglishMonoFont(titleFont, juce::Font::bold)
                              : juce::Font(titleFont, juce::Font::bold));
                g.drawText(cardTitle.toUpperCase(), textArea, juce::Justification::centredLeft, false);

                // Expand/collapse arrow — hidden when docked OR when showing ✕ detach
                if (!isDocked && !arrowHidden)
                {
                    float localArrowW = isMiniMode ? 20.0f : 40.0f;
                    auto arrowArea = juce::Rectangle<int>(cacheW - static_cast<int>(localArrowW), 0,
                                                           static_cast<int>(localArrowW), hh);
                    arrowArea.translate(static_cast<int>(headerBounds.getX()), static_cast<int>(headerBounds.getY()));
                    auto arrowCol = useEditorialDarkStyle ? juce::Colour(0xFFF6EEE3).withAlpha(0.96f)
                                    : useEditorialLightStyle ? juce::Colour(0xFF1A1A24).withAlpha(0.92f)
                                    : isDarkTheme ? GoodMeterLookAndFeel::textMainDark : GoodMeterLookAndFeel::textMain;
                    drawDisclosureArrow(g, arrowArea.toFloat(), isExpanded, arrowCol);
                }
                else if (isDocked)
                {
                    // ⋮⋮ Grip icon: 2×3 dot grid — visual drag handle hint
                    float localArrowW = isMiniMode ? 20.0f : 40.0f;
                    float gripCX = static_cast<float>(cacheW) - localArrowW * 0.5f;
                    float gripCY = static_cast<float>(hh) * 0.5f;
                    float dotR = isMiniMode ? 1.5f : 2.5f;
                    float gapX = isMiniMode ? 4.0f : 6.0f;
                    float gapY = isMiniMode ? 3.5f : 5.0f;
                    g.setColour(useEditorialDarkStyle ? juce::Colour(0xFFF6EEE3).withAlpha(0.62f)
                                                      : useEditorialLightStyle ? juce::Colour(0xFF1A1A24).withAlpha(0.42f)
                                                      : GoodMeterLookAndFeel::chartMuted());
                    for (int row = -1; row <= 1; ++row)
                        for (int col = 0; col < 2; ++col)
                            g.fillEllipse(headerBounds.getX() + gripCX + (col == 0 ? -gapX * 0.5f : gapX * 0.5f) - dotR,
                                          headerBounds.getY() + gripCY + static_cast<float>(row) * gapY - dotR,
                                          dotR * 2.0f, dotR * 2.0f);
                }
            }
            else
            {
                bool needsRebuild = headerTextCache.isNull() ||
                                    lastHeaderCacheW != cacheW ||
                                    lastHeaderCacheMini != isMiniMode ||
                                    lastHeaderCacheExpanded != isExpanded ||
                                    lastHeaderCacheDocked != isDocked ||
                                    lastHeaderCacheArrowHidden != arrowHidden ||
                                    std::abs(lastHeaderCacheScale - juce::Component::getApproximateScaleFactorForComponent(this)) > 0.01f;

                if (needsRebuild)
                {
                    const float scale = juce::Component::getApproximateScaleFactorForComponent(this);
                    lastHeaderCacheW = cacheW;
                    lastHeaderCacheMini = isMiniMode;
                    lastHeaderCacheExpanded = isExpanded;
                    lastHeaderCacheDocked = isDocked;
                    lastHeaderCacheArrowHidden = arrowHidden;
                    lastHeaderCacheScale = scale;
                    headerTextCache = juce::Image(juce::Image::ARGB,
                                                  juce::jmax(1, juce::roundToInt(static_cast<float>(cacheW) * scale)),
                                                  juce::jmax(1, juce::roundToInt(static_cast<float>(hh) * scale)),
                                                  true, juce::SoftwareImageType());
                    juce::Graphics tg(headerTextCache);
                    tg.addTransform(juce::AffineTransform::scale(scale));

                    float titleFont = GoodMeterLookAndFeel::chartFont(isMiniMode ? 10.0f : 15.0f);
                    float localPad = isMiniMode ? 4.0f : GoodMeterLookAndFeel::cardPadding;
                    float localDd = isMiniMode ? 8.0f : dotDiameter;
                    auto textArea = juce::Rectangle<int>(
                        static_cast<int>(localPad + localDd + (isMiniMode ? 4.0f : 12.0f)), 0,
                        cacheW - static_cast<int>(localPad + localDd + (isMiniMode ? 4.0f : 12.0f)), hh);
                    auto textCol = useEditorialDarkStyle ? juce::Colour(0xFFF6EEE3)
                                       : useEditorialLightStyle ? juce::Colour(0xFF1A1A24).withAlpha(0.96f)
                                       : isDarkTheme ? GoodMeterLookAndFeel::textMainDark
                                               : (isDocked ? GoodMeterLookAndFeel::textMuted : GoodMeterLookAndFeel::textMain);
                    tg.setColour(textCol);
                    tg.setFont(useMonospacedTitleFont
                                   ? GoodMeterLookAndFeel::iosEnglishMonoFont(titleFont, juce::Font::bold)
                                   : juce::Font(titleFont, juce::Font::bold));
                    tg.drawText(cardTitle.toUpperCase(), textArea, juce::Justification::centredLeft, false);

                    if (!isDocked && !arrowHidden)
                    {
                        float localArrowW = isMiniMode ? 20.0f : 40.0f;
                        auto arrowArea = juce::Rectangle<int>(cacheW - static_cast<int>(localArrowW), 0,
                                                               static_cast<int>(localArrowW), hh);
                        auto arrowCol = useEditorialDarkStyle ? juce::Colour(0xFFF6EEE3).withAlpha(0.96f)
                                        : useEditorialLightStyle ? juce::Colour(0xFF1A1A24).withAlpha(0.92f)
                                        : isDarkTheme ? GoodMeterLookAndFeel::textMainDark : GoodMeterLookAndFeel::textMain;
                        drawDisclosureArrow(tg, arrowArea.toFloat(), isExpanded, arrowCol);
                    }
                    else if (isDocked)
                    {
                        float localArrowW = isMiniMode ? 20.0f : 40.0f;
                        float gripCX = static_cast<float>(cacheW) - localArrowW * 0.5f;
                        float gripCY = static_cast<float>(hh) * 0.5f;
                        float dotR = isMiniMode ? 1.5f : 2.5f;
                        float gapX = isMiniMode ? 4.0f : 6.0f;
                        float gapY = isMiniMode ? 3.5f : 5.0f;
                        tg.setColour(useEditorialDarkStyle ? juce::Colour(0xFFF6EEE3).withAlpha(0.62f)
                                                           : useEditorialLightStyle ? juce::Colour(0xFF1A1A24).withAlpha(0.42f)
                                                           : GoodMeterLookAndFeel::chartMuted());
                        for (int row = -1; row <= 1; ++row)
                            for (int col = 0; col < 2; ++col)
                                tg.fillEllipse(gripCX + (col == 0 ? -gapX * 0.5f : gapX * 0.5f) - dotR,
                                               gripCY + static_cast<float>(row) * gapY - dotR,
                                               dotR * 2.0f, dotR * 2.0f);
                    }
                }

                g.drawImage(headerTextCache,
                            static_cast<int>(headerBounds.getX()),
                            static_cast<int>(headerBounds.getY()),
                            static_cast<int>(headerBounds.getWidth()),
                            static_cast<int>(headerBounds.getHeight()),
                            0, 0,
                            headerTextCache.getWidth(),
                            headerTextCache.getHeight());
            }
        }

        // Arrow area: compute hit rect + draw ✕ overlay when hovered (detach mode)
        {
            auto cr = getCardRect();
            float localArrowW = isMiniMode ? 20.0f : 40.0f;
            float arrowX = cr.getRight() - localArrowW;
            float arrowY = cr.getY();
            arrowHitRect = juce::Rectangle<float>(arrowX, arrowY, localArrowW, static_cast<float>(hh));

            // ✕ drawn directly on card — NO background fill, fully transparent
            if (isArrowHovered && showDetachButton && !isDocked)
            {
                float crossSz = isMiniMode ? 5.0f : 8.0f;
                float crossCX = arrowHitRect.getCentreX();
                float crossCY = arrowHitRect.getCentreY();

                // Layer 1: hard black drop shadow (1.5px down-right offset)
                g.setColour(juce::Colour(0xFF1A1A24));
                g.drawLine(crossCX - crossSz + 1.5f, crossCY - crossSz + 1.5f,
                           crossCX + crossSz + 1.5f, crossCY + crossSz + 1.5f, 2.5f);
                g.drawLine(crossCX + crossSz + 1.5f, crossCY - crossSz + 1.5f,
                           crossCX - crossSz + 1.5f, crossCY + crossSz + 1.5f, 2.5f);

                // Layer 2: card accent color core ✕ — matches status dot colour
                g.setColour(statusColour);
                g.drawLine(crossCX - crossSz, crossCY - crossSz,
                           crossCX + crossSz, crossCY + crossSz, 2.5f);
                g.drawLine(crossCX + crossSz, crossCY - crossSz,
                           crossCX - crossSz, crossCY + crossSz, 2.5f);
            }
        }

        // Resize grip triangle (bottom-right corner, only when expanded + floating + resize wired)
        // Guard: only show when parent has wired onResizeSnapQuery (Standalone floating only)
        if (isExpanded && !isDocked && onResizeSnapQuery)
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
            g.setColour(GoodMeterLookAndFeel::chartMuted(alpha));
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
            int widgetW = juce::jlimit(90, 168, static_cast<int>(cr.getWidth() * 0.38f));

            if (auto* combo = dynamic_cast<juce::ComboBox*>(headerWidget))
            {
                auto comboFont = combo->getLookAndFeel().getComboBoxFont(*combo);
                int preferredW = 108;
                for (int i = 0; i < combo->getNumItems(); ++i)
                    preferredW = juce::jmax(preferredW,
                                            static_cast<int>(std::ceil(comboFont.getStringWidthFloat(combo->getItemText(i)) + 42.0f)));

                const int maxAllowed = juce::jmax(108, static_cast<int>(cr.getWidth() * 0.46f));
                widgetW = juce::jlimit(108, maxAllowed, preferredW);
            }

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
        {
            headerWidget->setViewportIgnoreDragFlag(mobileListMode);
            addAndMakeVisible(headerWidget);
        }
    }

    void setMobileListMode(bool shouldUseMobileListMode)
    {
        mobileListMode = shouldUseMobileListMode;

        if (mobileListMode)
        {
            isCardHovered = false;
            isHeaderHovered = false;
            isArrowHovered = false;
            isResizeHovered = false;
            currentHoverOffset = 4.0f;
            stopTimer();
        }

        if (contentComponent != nullptr)
        {
            contentComponent->setInterceptsMouseClicks(!mobileListMode, !mobileListMode);
            contentComponent->setViewportIgnoreDragFlag(mobileListMode);
        }

        if (headerWidget != nullptr)
            headerWidget->setViewportIgnoreDragFlag(mobileListMode);
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

        if (mobileListMode)
        {
            auto cr = getCardRect();
            if (!cr.contains(static_cast<float>(event.x), static_cast<float>(event.y)))
                return;

            if (headerWidget != nullptr)
            {
                auto widgetBounds = headerWidget->getBounds();
                if (widgetBounds.contains(event.x, event.y))
                    return;
            }

            isDragTracking = true;
            isDragActivated = false;
            dragScreenStart = event.getScreenPosition();
            mobileScrollStartY = 0;
            if (auto* vp = findParentComponentOfClass<juce::Viewport>())
                mobileScrollStartY = vp->getViewPositionY();
            return;
        }

        // Check resize grip hit (highest priority — only when expanded + floating + resize wired)
        if (isExpanded && !isDocked && onResizeSnapQuery && !resizeGripRect.isEmpty())
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

        // Check arrow ✕ hit: if showing detach cross, clicking it triggers detach
        if (showDetachButton && !isDocked && isArrowHovered)
        {
            if (onDetachRequested)
                onDetachRequested(this);
            return;
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
        if (mobileListMode)
        {
            if (!isDragTracking) return;

            auto delta = event.getScreenPosition() - dragScreenStart;
            if (std::abs(delta.y) > 4 || std::abs(delta.x) > 6)
                isDragActivated = true;

            if (isDragActivated)
                if (auto* vp = findParentComponentOfClass<juce::Viewport>())
                    vp->setViewPosition(0,
                                        juce::jmax(0, mobileScrollStartY - delta.y));

            return;
        }

        // Resize drag takes priority
        if (isResizeDragging)
        {
            auto delta = event.getScreenPosition() - resizeDragStart;
            int newW = juce::jmax(minResizeW, resizeStartW + delta.x);
            int newH = juce::jmax(getActiveHeaderHeight() + minResizeContentH, resizeStartH + delta.y);

            // Apex-style edge-length snap: parent returns damped size
            if (onResizeSnapQuery)
            {
                auto snapped = onResizeSnapQuery(this, newW, newH);
                newW = snapped.x;
                newH = snapped.y;
            }

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
            // NOTE: onHeightChanged deliberately NOT called during resize drag
            // to avoid clamp + relayout + collision resolution each frame.
            // Full relayout happens on mouseUp via onResizeEnded.

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
        if (mobileListMode)
        {
            if (!isDragTracking) return;

            bool wasDragActivated = isDragActivated;
            isDragTracking = false;
            isDragActivated = false;

            if (!wasDragActivated && mobileAllowHeaderToggle)
            {
                auto cr = getCardRect();
                const int hh = getActiveHeaderHeight();
                float localY = static_cast<float>(event.y) - cr.getY();
                if (localY >= 0 && localY <= static_cast<float>(hh))
                    setExpanded(!isExpanded, true);
            }

            return;
        }

        if (isResizeDragging)
        {
            isResizeDragging = false;
            // Apex-style: commit snap alignment on mouseUp
            if (onResizeEnded)
                onResizeEnded(this, getWidth(), getHeight());
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
        if (mobileListMode) return;
        if (isDocked) return;

        bool wasHovered = isHeaderHovered;
        bool wasArrowHovered = isArrowHovered;
        bool wasResizeHovered = isResizeHovered;
        auto cr = getCardRect();
        const int hh = getActiveHeaderHeight();
        float localY = static_cast<float>(event.y) - cr.getY();
        isHeaderHovered = (localY >= 0 && localY <= static_cast<float>(hh)
                          && cr.contains(static_cast<float>(event.x), static_cast<float>(event.y)));

        // Track arrow area hover (for ✕ detach overlay)
        isArrowHovered = showDetachButton && !arrowHitRect.isEmpty()
                          && arrowHitRect.contains(static_cast<float>(event.x),
                                                    static_cast<float>(event.y));

        // Track resize grip hover (only when resize is wired)
        isResizeHovered = isExpanded && onResizeSnapQuery && !resizeGripRect.isEmpty()
                          && resizeGripRect.contains(static_cast<float>(event.x),
                                                      static_cast<float>(event.y));

        if (wasHovered != isHeaderHovered || wasArrowHovered != isArrowHovered
            || wasResizeHovered != isResizeHovered)
            repaint();
    }

    void mouseEnter(const juce::MouseEvent&) override
    {
        if (mobileListMode) return;
        if (isDocked) return;
        isCardHovered = true;
        ensureTimerRunning();
    }

    void mouseExit(const juce::MouseEvent&) override
    {
        if (mobileListMode) return;
        if (isDocked) return;
        isCardHovered = false;
        isHeaderHovered = false;
        isArrowHovered = false;
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
            contentComponent->setInterceptsMouseClicks(!mobileListMode, !mobileListMode);
            contentComponent->setViewportIgnoreDragFlag(mobileListMode);

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

        // On collapse, clear any height clamping so next expand uses preferredContentHeight
        if (!shouldExpand && customContentHeight > 0 && preferredContentHeight > 0)
            customContentHeight = -1;

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
        const int shadow = static_cast<int>(maxShadowOffset);
        if (!isExpanded)
            return hh + shadow;

        int contentHeight = 0;
        if (contentComponent != nullptr)
        {
            // Priority: customContentHeight (resize drag) > preferredContentHeight > fallback
            if (customContentHeight > 0)
                contentHeight = customContentHeight;
            else if (preferredContentHeight > 0)
                contentHeight = preferredContentHeight;
            else
                contentHeight = contentComponent->getHeight();

            // Defensive fallback: if content has no height, use default
            if (contentHeight <= 0)
                contentHeight = defaultContentHeight;

            // Cap at preferred height if set (prevents runaway expansion)
            if (preferredContentHeight > 0 && customContentHeight <= 0)
                contentHeight = juce::jmin(contentHeight, preferredContentHeight);

            int pad = isMiniMode ? 2 : static_cast<int>(GoodMeterLookAndFeel::cardPadding);
            contentHeight += pad * 2;
        }

        return hh + contentHeight + shadow;
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

    /** Immediately sync internal height animation targets to the given value.
     *  Prevents timer-driven animation from reverting an externally-committed size. */
    void syncAnimationHeight(float h)
    {
        targetHeight = h;
        currentHeight = h;
    }

private:
    juce::String cardTitle;
    juce::Colour statusColour;
    bool isExpanded;
    bool isAnimating = false;
    bool isHeaderHovered = false;
    bool isCardHovered = false;
    bool isArrowHovered = false;

    // Arrow hit rect (computed in paint, used for hover ✕ + detach click)
    mutable juce::Rectangle<float> arrowHitRect;

    // Resize grip rect (computed in paint, used for hit testing)
    mutable juce::Rectangle<float> resizeGripRect;
    bool isResizeHovered = false;
    bool isResizeDragging = false;
    juce::Point<int> resizeDragStart;
    int resizeStartW = 0;
    int resizeStartH = 0;
    int mobileScrollStartY = 0;

    // Drag tracking state (unified for undock-drag and floating-drag)
    bool isDragTracking = false;
    bool isDragActivated = false;
    juce::Point<int> dragScreenStart;

    std::unique_ptr<juce::Component> contentComponent;

    // Optional header widget (e.g. ComboBox for Levels card)
    juce::Component* headerWidget = nullptr;  // Non-owning pointer

    // === Offscreen header text cache (rebuild on resize/mode/expand/dock/arrowHover change) ===
    juce::Image headerTextCache;
    int lastHeaderCacheW = 0;
    float lastHeaderCacheScale = 0.0f;
    bool lastHeaderCacheMini = false;
    bool lastHeaderCacheExpanded = false;
    bool lastHeaderCacheDocked = false;
    bool lastHeaderCacheArrowHidden = false;

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

    static void drawDisclosureArrow(juce::Graphics& g,
                                    juce::Rectangle<float> area,
                                    bool expanded,
                                    juce::Colour colour)
    {
        auto icon = area.reduced(area.getWidth() * 0.40f, area.getHeight() * 0.36f);
        g.setColour(colour);

        if (GoodMeterLookAndFeel::preferDirectChartText())
        {
            juce::Path chevron;
            const float stroke = area.getHeight() <= 24.0f ? 1.35f : 1.6f;

            if (expanded)
            {
                chevron.startNewSubPath(icon.getX(), icon.getY());
                chevron.lineTo(icon.getCentreX(), icon.getBottom());
                chevron.lineTo(icon.getRight(), icon.getY());
            }
            else
            {
                chevron.startNewSubPath(icon.getX(), icon.getY());
                chevron.lineTo(icon.getRight(), icon.getCentreY());
                chevron.lineTo(icon.getX(), icon.getBottom());
            }

            g.strokePath(chevron, juce::PathStrokeType(stroke,
                                                       juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded));
        }
        else
        {
            juce::Path arrow;

            if (expanded)
            {
                arrow.addTriangle(icon.getX(), icon.getY(),
                                  icon.getRight(), icon.getY(),
                                  icon.getCentreX(), icon.getBottom());
            }
            else
            {
                arrow.addTriangle(icon.getX(), icon.getY(),
                                  icon.getX(), icon.getBottom(),
                                  icon.getRight(), icon.getCentreY());
            }

            g.fillPath(arrow);
        }
    }

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
