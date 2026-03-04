/*
  ==============================================================================
    PluginEditor.cpp
    GOODMETER - Main plugin editor implementation

    60Hz Timer-driven UI with vertical meter layout
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GOODMETERAudioProcessorEditor::GOODMETERAudioProcessorEditor(GOODMETERAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Set custom LookAndFeel
    setLookAndFeel(&customLookAndFeel);

    // Create viewport and content container
    viewport = std::make_unique<juce::Viewport>();
    contentComponent = std::make_unique<juce::Component>();

    addAndMakeVisible(viewport.get());
    viewport->setViewedComponent(contentComponent.get(), false);
    viewport->setScrollBarsShown(true, false);  // Vertical scroll only

    //==========================================================================
    // Create meter cards
    //==========================================================================

    levelsCard = std::make_unique<MeterCardComponent>(
        "LEVELS",
        GoodMeterLookAndFeel::accentPink,
        true  // Default expanded
    );

    // Create Levels Meter and transfer ownership to card
    levelsMeter = new LevelsMeterComponent(audioProcessor);
    levelsMeter->setupTargetMenu();
    levelsCard->setContentComponent(std::unique_ptr<juce::Component>(levelsMeter));

    // Embed the target ComboBox into the Levels card header
    levelsCard->setHeaderWidget(&levelsMeter->getTargetMenu());

    vuMeterCard = std::make_unique<MeterCardComponent>(
        "VU METER",
        GoodMeterLookAndFeel::accentYellow,
        true  // ✅ Expanded to show VU meter
    );

    // Create VU Meter and transfer ownership to card
    vuMeter = new VUMeterComponent();
    vuMeterCard->setContentComponent(std::unique_ptr<juce::Component>(vuMeter));

    threeBandCard = std::make_unique<MeterCardComponent>(
        "3-BAND",
        GoodMeterLookAndFeel::accentPurple,
        true  // ✅ Expanded to show chemical vessels
    );

    // Create 3-Band Analyzer and transfer ownership to card
    band3Meter = new Band3Component(audioProcessor);
    threeBandCard->setContentComponent(std::unique_ptr<juce::Component>(band3Meter));

    spectrumCard = std::make_unique<MeterCardComponent>(
        "SPECTRUM",
        GoodMeterLookAndFeel::accentCyan,
        true  // ✅ Expanded to show spectrum analyzer
    );

    // Create Spectrum Analyzer and transfer ownership to card
    spectrumAnalyzer = new SpectrumAnalyzerComponent(audioProcessor);
    spectrumCard->setContentComponent(std::unique_ptr<juce::Component>(spectrumAnalyzer));

    phaseCard = std::make_unique<MeterCardComponent>(
        "PHASE",
        GoodMeterLookAndFeel::accentGreen,
        true  // Expanded for testing
    );

    // Create Phase Correlation Meter
    phaseMeter = new PhaseCorrelationComponent();
    phaseCard->setContentComponent(std::unique_ptr<juce::Component>(phaseMeter));

    stereoImageCard = std::make_unique<MeterCardComponent>(
        "STEREO",
        GoodMeterLookAndFeel::accentPink,
        true  // ✅ Expanded to show stereo field visualization
    );

    // Create Stereo Image Meter and transfer ownership to card
    stereoImageMeter = new StereoImageComponent(audioProcessor);
    stereoImageCard->setContentComponent(std::unique_ptr<juce::Component>(stereoImageMeter));

    spectrogramCard = std::make_unique<MeterCardComponent>(
        "SPECTROGRAM",
        GoodMeterLookAndFeel::accentYellow,
        true  // ✅ Expanded to show waterfall spectrogram
    );

    // Create Spectrogram and transfer ownership to card
    spectrogramMeter = new SpectrogramComponent(audioProcessor);
    spectrogramCard->setContentComponent(std::unique_ptr<juce::Component>(spectrogramMeter));

    psrCard = std::make_unique<MeterCardComponent>(
        "PSR",
        juce::Colour(0xFF20C997),  // Electro-cyan
        true  // Expanded by default
    );

    // Create PSR Meter and transfer ownership to card
    psrMeter = new PsrMeterComponent(audioProcessor);
    psrCard->setContentComponent(std::unique_ptr<juce::Component>(psrMeter));

    nonoCard = std::make_unique<MeterCardComponent>(
        "NONO",
        juce::Colour(0xFF00E5FF),  // Holo-cyan
        true  // Expanded by default
    );

    // Create HoloNono companion robot and transfer ownership to card
    holoNono = new HoloNonoComponent(audioProcessor);
    nonoCard->setContentComponent(std::unique_ptr<juce::Component>(holoNono));

    // Wire NONO test tube double-click → enter jiggle mode
    holoNono->onTestTubeDoubleClicked = [this]() { enterJiggleMode(); };

    // Wire NONO body double-click (while in edit mode) → exit jiggle mode
    holoNono->onExitJiggleMode = [this]() { exitJiggleMode(); };

    // Wire NONO right-double-click → toggle Mini Mode
    holoNono->onRightDoubleClick = [this]() { toggleMiniMode(); };

    // Initialize jiggle phases with random offsets
    {
        auto& rng = juce::Random::getSystemRandom();
        for (int i = 0; i < 9; ++i)
            jigglePhases[i] = rng.nextFloat() * juce::MathConstants<float>::twoPi;
    }

    // Bind height change callbacks to all cards
    // Each callback: relayout + notify NONO of fold/unfold event
    auto makeCardCallback = [this](MeterCardComponent* card, const juce::String& name) {
        return [this, card, name]() {
            this->resized();
            if (holoNono != nullptr)
            {
                if (card->getExpanded())
                    holoNono->onCardUnfolded(name);
                else
                    holoNono->onCardFolded(name);
            }
        };
    };

    levelsCard->onHeightChanged = makeCardCallback(levelsCard.get(), "LEVELS");
    vuMeterCard->onHeightChanged = makeCardCallback(vuMeterCard.get(), "VU METER");
    threeBandCard->onHeightChanged = makeCardCallback(threeBandCard.get(), "3-BAND");
    spectrumCard->onHeightChanged = makeCardCallback(spectrumCard.get(), "SPECTRUM");
    phaseCard->onHeightChanged = makeCardCallback(phaseCard.get(), "PHASE");
    stereoImageCard->onHeightChanged = makeCardCallback(stereoImageCard.get(), "STEREO");
    spectrogramCard->onHeightChanged = makeCardCallback(spectrogramCard.get(), "SPECTROGRAM");
    psrCard->onHeightChanged = makeCardCallback(psrCard.get(), "PSR");
    nonoCard->onHeightChanged = [this]() { this->resized(); };

    // Add cards to content component
    contentComponent->addAndMakeVisible(levelsCard.get());
    contentComponent->addAndMakeVisible(vuMeterCard.get());
    contentComponent->addAndMakeVisible(threeBandCard.get());
    contentComponent->addAndMakeVisible(spectrumCard.get());
    contentComponent->addAndMakeVisible(phaseCard.get());
    contentComponent->addAndMakeVisible(stereoImageCard.get());
    contentComponent->addAndMakeVisible(spectrogramCard.get());
    contentComponent->addAndMakeVisible(psrCard.get());
    contentComponent->addAndMakeVisible(nonoCard.get());

    // Set initial size (matches typical plugin dimensions)
    setSize(500, 700);

    // 🎨 开启自由横向缩放（对标专业插件）
    setResizable(true, true);
    setResizeLimits(760, 600,    // 🧱 Brick Wall: 最小宽度 760px, 最小高度 600px（保护 VU 表不被裁切）
                    2400, 1600); // 最大宽度 2400px（支持三列布局）, 最大高度 1600px

    // Register mouse listener on contentComponent to receive drag events from cards
    contentComponent->addMouseListener(this, true);

    // Start 60Hz timer for UI updates
    startTimerHz(60);
}

GOODMETERAudioProcessorEditor::~GOODMETERAudioProcessorEditor()
{
    stopTimer();
    contentComponent->removeMouseListener(this);
    setLookAndFeel(nullptr);
}

//==============================================================================
void GOODMETERAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Fill background
    g.fillAll(GoodMeterLookAndFeel::bgMain);
}

void GOODMETERAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    viewport->setBounds(bounds);

    // 【防跳顶】缓存当前滚动位置
    auto prevScrollPos = viewport->getViewPosition();

    const int width = bounds.getWidth();
    const int spacing = isMiniMode ? 2 : static_cast<int>(GoodMeterLookAndFeel::cardSpacing);

    // ===== panelOrder 驱动的面板收集 =====
    std::vector<MeterCardComponent*> allCards;
    for (int idx : panelOrder)
    {
        auto* card = getCardByIndex(idx);
        if (card != nullptr)
            allCards.push_back(card);
    }

    const int activePanels = static_cast<int>(allCards.size());
    if (activePanels == 0) return;

    // ==========================================================
    // 双套响应式断点 (Mini Mode vs Normal Mode)
    // ==========================================================
    int numColumns;
    if (isMiniMode)
    {
        numColumns = (width >= 600) ? 3 : ((width >= 400) ? 2 : 1);
    }
    else
    {
        numColumns = (width >= 1000) ? 3 : ((width >= 650) ? 2 : 1);
    }

    numColumns = juce::jmin(numColumns, activePanels);

    // ==========================================================
    // 按列分配卡片 (panelOrder 序列 → 顺序填入各列)
    // ==========================================================
    std::vector<std::vector<MeterCardComponent*>> columns(numColumns);

    if (numColumns >= 3)
    {
        // 3列: 每3张一组分到 A/B/C
        for (int i = 0; i < activePanels; ++i)
            columns[i % 3].push_back(allCards[static_cast<size_t>(i)]);
    }
    else if (numColumns == 2)
    {
        // 2列: 前4张左列, 后面右列 (4 vs 5 平衡)
        for (int i = 0; i < activePanels; ++i)
        {
            if (i < 4)
                columns[0].push_back(allCards[static_cast<size_t>(i)]);
            else
                columns[1].push_back(allCards[static_cast<size_t>(i)]);
        }
    }
    else
    {
        columns[0] = allCards;
    }

    // ==========================================================
    // 弹性空间分配 (Flex-Grow): 每列独立计算展开高度
    // ==========================================================
    if (isMiniMode)
        setResizeLimits(300, 300, 2400, 1600);
    else
        setResizeLimits(380, 400, 2400, 1600);

    const int minExpandedHeight = isMiniMode ? 150 : 280;

    // 折叠高度绝对锁死：headerHeight + maxShadowOffset（与 MeterCardComponent 一致）
    const int strictCollapsedHeight = isMiniMode ? (24 + 8) : (48 + 8);

    const int columnWidth = (numColumns == 1)
        ? (width - spacing * 2)
        : (width - spacing * (numColumns + 1)) / numColumns;

    // ===== 第一步: 统计每列展开/折叠面板数 =====
    std::vector<int> expandedCounts(numColumns, 0);

    for (int col = 0; col < numColumns; ++col)
        for (auto* card : columns[col])
            if (card->getExpanded()) expandedCounts[col]++;

    // ===== 第二步: 按列计算弹性展开高度 =====
    const int availableViewportHeight = bounds.getHeight();
    std::vector<int> flexExpandedHeight(numColumns, minExpandedHeight);

    for (int col = 0; col < numColumns; ++col)
    {
        if (expandedCounts[col] > 0)
        {
            int collapsedTotal = 0;
            for (auto* card : columns[col])
                if (!card->getExpanded())
                    collapsedTotal += strictCollapsedHeight;

            int totalCards = static_cast<int>(columns[col].size());
            int totalMargins = spacing * (totalCards + 1);
            int deadSpace = collapsedTotal + totalMargins;
            int spaceLeft = availableViewportHeight - deadSpace;
            int calculated = spaceLeft / expandedCounts[col];

            flexExpandedHeight[col] = juce::jmax(minExpandedHeight, calculated);
        }
    }

    // ===== 第三步: 应用弹性高度排版 + 记录 slotTargetBounds =====
    slotTargetBounds.clear();
    slotTargetBounds.resize(static_cast<size_t>(activePanels));
    std::vector<int> columnHeights(numColumns, spacing);

    for (int col = 0; col < numColumns; ++col)
    {
        int colX = (numColumns == 1)
            ? spacing
            : spacing + col * (columnWidth + spacing);

        for (auto* card : columns[col])
        {
            int h = card->getExpanded() ? flexExpandedHeight[col] : strictCollapsedHeight;
            auto cardBounds = juce::Rectangle<int>(colX, columnHeights[col], columnWidth, h);

            // Don't move the card being dragged
            if (draggedPanelSlot < 0 || card != getCardByIndex(panelOrder[static_cast<size_t>(draggedPanelSlot)]))
                card->setBounds(cardBounds);

            // 身份反查：找到此 card 在 allCards 中的原始下标，精确写入 slotTargetBounds
            for (int i = 0; i < activePanels; ++i)
            {
                if (allCards[static_cast<size_t>(i)] == card)
                {
                    slotTargetBounds[static_cast<size_t>(i)] = cardBounds;
                    break;
                }
            }

            columnHeights[col] += h + spacing;
        }
    }

    // 最长列高度 → contentComponent 高度 → 超出 Viewport 激活滚轮
    int maxContentHeight = 0;
    for (int h : columnHeights)
        maxContentHeight = juce::jmax(maxContentHeight, h);

    int finalHeight = juce::jmax(bounds.getHeight() + 1, maxContentHeight);
    contentComponent->setBounds(0, 0, width, finalHeight);

    // 【防跳顶】恢复滚动位置（Viewport 自动钳制越界）
    viewport->setViewPosition(prevScrollPos);
}

//==============================================================================
void GOODMETERAudioProcessorEditor::timerCallback()
{
    // Read atomic values from processor (thread-safe)
    float peakL = audioProcessor.peakLevelL.load(std::memory_order_relaxed);
    float peakR = audioProcessor.peakLevelR.load(std::memory_order_relaxed);
    float rmsL = audioProcessor.rmsLevelL.load(std::memory_order_relaxed);
    float rmsR = audioProcessor.rmsLevelR.load(std::memory_order_relaxed);
    float momentary = audioProcessor.lufsLevel.load(std::memory_order_relaxed);
    float shortTerm = audioProcessor.lufsShortTerm.load(std::memory_order_relaxed);
    float integrated = audioProcessor.lufsIntegrated.load(std::memory_order_relaxed);
    float phase = audioProcessor.phaseCorrelation.load(std::memory_order_relaxed);

    // Push Short-Term LUFS to LRA history (~60Hz = every ~16ms)
    // We want ~100ms intervals, so push every 6th frame
    static int lraFrameCounter = 0;
    if (++lraFrameCounter >= 6)
    {
        audioProcessor.pushShortTermLUFSForLRA(shortTerm);
        audioProcessor.calculateLRARealtime();
        lraFrameCounter = 0;
    }

    float luRangeVal = audioProcessor.luRange.load(std::memory_order_relaxed);

    // Update Levels Meter
    if (levelsMeter != nullptr)
    {
        levelsMeter->updateMetrics(peakL, peakR, momentary, shortTerm, integrated, luRangeVal);
    }

    // Update VU Meter
    if (vuMeter != nullptr)
    {
        vuMeter->updateVU(rmsL, rmsR);
    }

    // Update Phase Correlation Meter
    if (phaseMeter != nullptr)
    {
        phaseMeter->updateCorrelation(phase);
    }

    // Jiggle animation: apply micro-rotation transforms to all cards
    if (isJiggleMode)
    {
        float ms = static_cast<float>(juce::Time::getMillisecondCounterHiRes());
        for (int i = 0; i < 9; ++i)
        {
            auto* card = getCardByIndex(panelOrder[static_cast<size_t>(i)]);
            if (card == nullptr) continue;

            // Skip the card being dragged (it follows the mouse directly)
            if (i == draggedPanelSlot) continue;

            float angle = std::sin(ms * 0.006f + jigglePhases[i]) * 0.012f;
            auto cardCentre = card->getBounds().getCentre().toFloat();
            card->setTransform(juce::AffineTransform::rotation(
                angle, cardCentre.x, cardCentre.y));
        }

        // Hover-to-swap: only SET readyToSwap flag + NONO wink, NO swap here
        if (draggedPanelSlot >= 0 && dragActivated && currentHoverTarget >= 0
            && currentHoverTarget != draggedPanelSlot)
        {
            auto now = juce::Time::getMillisecondCounter();
            if (now - hoverStartTime >= swapHoverMs)
            {
                if (!readyToSwap)
                {
                    readyToSwap = true;
                    if (holoNono != nullptr) holoNono->setWinking(true);
                }
            }
        }
        else
        {
            // Target lost or changed — cancel readyToSwap
            if (readyToSwap)
            {
                readyToSwap = false;
                if (holoNono != nullptr) holoNono->setWinking(false);
            }
        }
    }
}

//==============================================================================
// Jiggle / 1v1 Swap Drag Engine
//==============================================================================

MeterCardComponent* GOODMETERAudioProcessorEditor::getCardByIndex(int idx)
{
    switch (idx)
    {
        case 0: return levelsCard.get();
        case 1: return vuMeterCard.get();
        case 2: return phaseCard.get();
        case 3: return spectrumCard.get();
        case 4: return threeBandCard.get();
        case 5: return stereoImageCard.get();
        case 6: return spectrogramCard.get();
        case 7: return psrCard.get();
        case 8: return nonoCard.get();
        default: return nullptr;
    }
}

void GOODMETERAudioProcessorEditor::enterJiggleMode()
{
    if (isJiggleMode) return;
    isJiggleMode = true;
    jiggleEnteredTime = juce::Time::getMillisecondCounter();

    if (holoNono != nullptr)
        holoNono->isEditMode = true;

    for (int i = 0; i < 9; ++i)
    {
        auto* card = getCardByIndex(i);
        if (card != nullptr)
        {
            card->inJiggleMode = true;
            card->setInterceptsMouseClicks(false, false);
        }
    }
}

void GOODMETERAudioProcessorEditor::exitJiggleMode()
{
    if (!isJiggleMode) return;
    isJiggleMode = false;

    if (holoNono != nullptr)
    {
        holoNono->isEditMode = false;
        holoNono->setWinking(false);
    }

    draggedPanelSlot = -1;
    currentHoverTarget = -1;
    readyToSwap = false;
    dragActivated = false;

    for (int i = 0; i < 9; ++i)
    {
        auto* card = getCardByIndex(i);
        if (card != nullptr)
        {
            card->inJiggleMode = false;
            card->setInterceptsMouseClicks(true, true);
            card->setTransform(juce::AffineTransform());
            card->setAlpha(1.0f);
        }
    }

    resized();
}

void GOODMETERAudioProcessorEditor::toggleMiniMode()
{
    isMiniMode = !isMiniMode;

    // Propagate to all cards
    for (int i = 0; i < 9; ++i)
    {
        auto* card = getCardByIndex(i);
        if (card != nullptr)
            card->isMiniMode = isMiniMode;
    }

    resized();
    repaint();
}

int GOODMETERAudioProcessorEditor::findPanelSlotAt(juce::Point<int> posInContent)
{
    for (int i = 0; i < static_cast<int>(panelOrder.size()); ++i)
    {
        if (i == draggedPanelSlot) continue;

        if (i < static_cast<int>(slotTargetBounds.size()) &&
            slotTargetBounds[static_cast<size_t>(i)].contains(posInContent))
        {
            return i;
        }
    }
    return -1;
}

//==============================================================================
// Mouse handlers: 1v1 swap drag
//==============================================================================

void GOODMETERAudioProcessorEditor::mouseDown(const juce::MouseEvent& event)
{
    if (!isJiggleMode) return;

    auto posInContent = event.getEventRelativeTo(contentComponent.get()).position.toInt();

    for (int i = static_cast<int>(panelOrder.size()) - 1; i >= 0; --i)
    {
        auto* card = getCardByIndex(panelOrder[static_cast<size_t>(i)]);
        if (card == nullptr) continue;

        if (card->getBounds().contains(posInContent))
        {
            draggedPanelSlot = i;
            dragOffset = posInContent - card->getBounds().getPosition();
            dragActivated = false;
            currentHoverTarget = -1;
            hoverStartTime = 0;
            readyToSwap = false;

            if (i < static_cast<int>(slotTargetBounds.size()))
                dragOriginSlotBounds = slotTargetBounds[static_cast<size_t>(i)];
            else
                dragOriginSlotBounds = card->getBounds();

            break;
        }
    }
}

void GOODMETERAudioProcessorEditor::mouseDrag(const juce::MouseEvent& event)
{
    if (!isJiggleMode || draggedPanelSlot < 0) return;
    if (!event.mouseWasDraggedSinceMouseDown()) return;

    auto* dragCard = getCardByIndex(panelOrder[static_cast<size_t>(draggedPanelSlot)]);
    if (dragCard == nullptr) return;

    if (!dragActivated)
    {
        dragActivated = true;
        dragCard->toFront(false);
        dragCard->setAlpha(0.6f);
        dragCard->setTransform(juce::AffineTransform());
    }

    auto posInContent = event.getEventRelativeTo(contentComponent.get()).position.toInt();
    dragCard->setTopLeftPosition(posInContent - dragOffset);

    int newTarget = findPanelSlotAt(posInContent);

    if (newTarget != currentHoverTarget)
    {
        currentHoverTarget = newTarget;
        hoverStartTime = juce::Time::getMillisecondCounter();
        // Cancel readyToSwap when target changes
        if (readyToSwap)
        {
            readyToSwap = false;
            if (holoNono != nullptr) holoNono->setWinking(false);
        }
    }
}

void GOODMETERAudioProcessorEditor::mouseUp(const juce::MouseEvent&)
{
    if (!isJiggleMode || draggedPanelSlot < 0) return;

    int mySlot = draggedPanelSlot;
    int targetSlot = currentHoverTarget;
    bool wasDragging = dragActivated;
    bool doSwap = readyToSwap && targetSlot >= 0 && targetSlot != mySlot;

    // Get the dragged card
    auto* dragCard = getCardByIndex(panelOrder[static_cast<size_t>(mySlot)]);

    if (doSwap)
    {
        // Get bounds BEFORE swap
        auto dragSlotBounds = dragOriginSlotBounds;
        auto targetSlotBounds = (targetSlot < static_cast<int>(slotTargetBounds.size()))
            ? slotTargetBounds[static_cast<size_t>(targetSlot)]
            : juce::Rectangle<int>();

        auto* targetCard = getCardByIndex(panelOrder[static_cast<size_t>(targetSlot)]);

        // Safe 1v1 swap in panelOrder
        std::swap(panelOrder[static_cast<size_t>(mySlot)],
                  panelOrder[static_cast<size_t>(targetSlot)]);

        // Animate target card to the dragged card's old slot
        if (targetCard != nullptr && !dragSlotBounds.isEmpty())
            animator.animateComponent(targetCard, dragSlotBounds, 1.0f, 250, false, 1.0, 1.0);

        // Animate dragged card to target's old slot
        if (dragCard != nullptr && !targetSlotBounds.isEmpty())
        {
            dragCard->setAlpha(1.0f);
            animator.animateComponent(dragCard, targetSlotBounds, 1.0f, 200, false, 1.0, 1.0);
        }

    }
    else if (wasDragging && dragCard != nullptr)
    {
        // No swap — snap dragged card back to its original slot
        dragCard->setAlpha(1.0f);
        animator.animateComponent(dragCard, dragOriginSlotBounds, 1.0f, 200, false, 0.8, 0.0);
    }

    // Full state reset
    draggedPanelSlot = -1;
    currentHoverTarget = -1;
    hoverStartTime = 0;
    dragActivated = false;
    readyToSwap = false;
    if (holoNono != nullptr) holoNono->setWinking(false);
}

void GOODMETERAudioProcessorEditor::mouseDoubleClick(const juce::MouseEvent&)
{
    if (!isJiggleMode) return;
    if (juce::Time::getMillisecondCounter() - jiggleEnteredTime < 500) return;
    exitJiggleMode();
}
