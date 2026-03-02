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

    // Start 60Hz timer for UI updates
    startTimerHz(60);
}

GOODMETERAudioProcessorEditor::~GOODMETERAudioProcessorEditor()
{
    stopTimer();
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

    // Position viewport to fill entire editor
    viewport->setBounds(bounds);

    const int width = bounds.getWidth();
    const int spacing = GoodMeterLookAndFeel::cardSpacing;

    // Physical minimum height constants
    const int expandedMinHeight = 130;
    const int headerHeight = 48;
    const int minCardWidth = 380;
    const int dualColumnThreshold = 800;

    // ========================================================================
    // 🎯 智能三列触发逻辑：根据第二列卡片展开状态决定是否启用第三列
    // ========================================================================

    // 统计第二列（Spectrum, 3-Band, Stereo）中非瀑布表的展开数量
    int col2ExpandedCount = 0;
    if (spectrumCard && spectrumCard->getExpanded()) col2ExpandedCount++;
    if (threeBandCard && threeBandCard->getExpanded()) col2ExpandedCount++;
    if (stereoImageCard && stereoImageCard->getExpanded()) col2ExpandedCount++;

    bool spectrogramExpanded = spectrogramCard && spectrogramCard->getExpanded();

    // ✅ 三列启用条件（防止空列）：
    // 只有当【瀑布表是打开状态】且【第二列还有至少2个其他表打开】时，
    // 才把瀑布表挤到第三列去！
    bool shouldUseThreeColumns = spectrogramExpanded && (col2ExpandedCount >= 2);

    // ========================================================================
    // 三列专业模式（智能触发 + 严格等高网格 + 瀑布图'做自己'）
    // ========================================================================
    if (width >= dualColumnThreshold && shouldUseThreeColumns)
    {
        // 🎨 三列自动宽度分配
        const int minColumnWidth = 330;  // 每列最小宽度（降低阈值，更易触发）
        const int requiredWidth = minColumnWidth * 3 + spacing * 4;

        // 如果窗口宽度不足以容纳三列，降级到双列
        if (width < requiredWidth)
        {
            shouldUseThreeColumns = false;
        }
        else
        {
            // 计算三列均等宽度
            const int availableWidth = width - spacing * 4;
            const int columnWidth = availableWidth / 3;

            // 定义三列起始位置
            const int col1X = spacing;
            const int col2X = col1X + columnWidth + spacing;
            const int col3X = col2X + columnWidth + spacing;

            // 🎯 手风琴弹性排版（Accordion Flex Layout）
            const int availableHeight = bounds.getHeight() - spacing * 2;

            // ========== 第一列：手风琴排版 Levels, VU, Phase, PSR ==========
            std::vector<MeterCardComponent*> col1Cards;
            if (levelsCard != nullptr) col1Cards.push_back(levelsCard.get());
            if (vuMeterCard != nullptr) col1Cards.push_back(vuMeterCard.get());
            if (phaseCard != nullptr) col1Cards.push_back(phaseCard.get());
            if (psrCard != nullptr) col1Cards.push_back(psrCard.get());

            int col1ExpandedCount = 0;
            for (auto* card : col1Cards)
                if (card->getExpanded()) col1ExpandedCount++;

            int col1FoldedCount = static_cast<int>(col1Cards.size()) - col1ExpandedCount;
            int col1TotalSpacing = spacing * (static_cast<int>(col1Cards.size()) - 1);
            int col1FoldedTotal = col1FoldedCount * headerHeight;
            int col1AvailableForExpanded = availableHeight - col1TotalSpacing - col1FoldedTotal;
            int col1ExpandedHeight = col1ExpandedCount > 0
                ? juce::jmax(expandedMinHeight, col1AvailableForExpanded / col1ExpandedCount) : 0;

            int currentY1 = spacing;
            for (auto* card : col1Cards)
            {
                int h = card->getExpanded() ? col1ExpandedHeight : headerHeight;
                card->setBounds(col1X, currentY1, columnWidth, h);
                currentY1 += h + spacing;
            }

            int col1Y = currentY1;

            // Column 1 physical min height
            int minCol1Height = (col1ExpandedCount * expandedMinHeight)
                + (col1FoldedCount * headerHeight) + col1TotalSpacing + spacing * 2;

            // ========== 第二列：手风琴排版 Spectrum, 3-Band, Stereo ==========
            std::vector<MeterCardComponent*> col2Cards;
            if (spectrumCard != nullptr) col2Cards.push_back(spectrumCard.get());
            if (threeBandCard != nullptr) col2Cards.push_back(threeBandCard.get());
            if (stereoImageCard != nullptr) col2Cards.push_back(stereoImageCard.get());

            int col2ExpandedCount = 0;
            for (auto* card : col2Cards)
                if (card->getExpanded()) col2ExpandedCount++;

            int col2FoldedCount = static_cast<int>(col2Cards.size()) - col2ExpandedCount;
            int col2TotalSpacing = spacing * (static_cast<int>(col2Cards.size()) - 1);
            int col2FoldedTotal = col2FoldedCount * headerHeight;
            int col2AvailableForExpanded = availableHeight - col2TotalSpacing - col2FoldedTotal;
            int col2ExpandedHeight = col2ExpandedCount > 0
                ? juce::jmax(expandedMinHeight, col2AvailableForExpanded / col2ExpandedCount) : 0;

            int currentY2 = spacing;
            for (auto* card : col2Cards)
            {
                int h = card->getExpanded() ? col2ExpandedHeight : headerHeight;
                card->setBounds(col2X, currentY2, columnWidth, h);
                currentY2 += h + spacing;
            }

            int col2Y = currentY2;

            // Column 2 physical min height
            int minCol2Height = (col2ExpandedCount * expandedMinHeight)
                + (col2FoldedCount * headerHeight) + col2TotalSpacing + spacing * 2;

            // ========== 第三列：Spectrogram + NONO ==========
            int col3Y = spacing;
            if (spectrogramCard != nullptr && spectrogramCard->isVisible())
            {
                const int spectrogramHeight = availableHeight / 2;
                spectrogramCard->setBounds(col3X, col3Y, columnWidth, spectrogramHeight);
                col3Y += spectrogramHeight + spacing;
            }

            // NONO companion in remaining Col3 space
            if (nonoCard != nullptr && nonoCard->isVisible())
            {
                int nonoH = nonoCard->getExpanded()
                    ? juce::jmax(expandedMinHeight, availableHeight - (col3Y - spacing))
                    : headerHeight;
                nonoCard->setBounds(col3X, col3Y, columnWidth, nonoH);
                col3Y += nonoH + spacing;
            }

            // Dynamic resize limits (三列模式)
            int minCol3Height = spectrogramExpanded ? (expandedMinHeight + spacing * 2) : (headerHeight + spacing * 2);
            int dynamicMinHeight = juce::jmax(minCol1Height, juce::jmax(minCol2Height, minCol3Height));
            setResizeLimits(requiredWidth, dynamicMinHeight, 2400, 1600);

            // Content height = max of three columns
            int contentHeight = juce::jmax(col1Y, juce::jmax(col2Y, col3Y));
            contentComponent->setSize(width, contentHeight);

            return;  // 完成三列布局，提前返回
        }
    }

    // ========================================================================
    // 双列模式 (width >= 800 且不满足三列条件 + 严格等高网格 + 瀑布图'做自己')
    // ========================================================================
    if (width >= dualColumnThreshold)
    {
        // 计算双列均等宽度
        const int availableWidth = width - spacing * 3;
        const int columnWidth = availableWidth / 2;

        const int col1X = spacing;
        const int col2X = col1X + columnWidth + spacing;

        // 🎯 手风琴弹性排版
        const int availableHeight = bounds.getHeight() - spacing * 2;

        // ========== 左列：手风琴排版 Levels, VU, Phase, PSR ==========
        std::vector<MeterCardComponent*> col1Cards;
        if (levelsCard != nullptr) col1Cards.push_back(levelsCard.get());
        if (vuMeterCard != nullptr) col1Cards.push_back(vuMeterCard.get());
        if (phaseCard != nullptr) col1Cards.push_back(phaseCard.get());
        if (psrCard != nullptr) col1Cards.push_back(psrCard.get());

        int col1ExpandedCount = 0;
        for (auto* card : col1Cards)
            if (card->getExpanded()) col1ExpandedCount++;

        int col1FoldedCount = static_cast<int>(col1Cards.size()) - col1ExpandedCount;
        int col1TotalSpacing = spacing * (static_cast<int>(col1Cards.size()) - 1);
        int col1FoldedTotal = col1FoldedCount * headerHeight;
        int col1AvailableForExpanded = availableHeight - col1TotalSpacing - col1FoldedTotal;
        int col1ExpandedHeight = col1ExpandedCount > 0
            ? juce::jmax(expandedMinHeight, col1AvailableForExpanded / col1ExpandedCount) : 0;

        int currentY1 = spacing;
        for (auto* card : col1Cards)
        {
            int h = card->getExpanded() ? col1ExpandedHeight : headerHeight;
            card->setBounds(col1X, currentY1, columnWidth, h);
            currentY1 += h + spacing;
        }

        int col1Y = currentY1;

        int minCol1Height = (col1ExpandedCount * expandedMinHeight)
            + (col1FoldedCount * headerHeight) + col1TotalSpacing + spacing * 2;

        // ========== 右列：手风琴排版 Spectrum, 3-Band, Stereo + Spectrogram ==========
        std::vector<MeterCardComponent*> col2Cards;
        if (spectrumCard != nullptr) col2Cards.push_back(spectrumCard.get());
        if (threeBandCard != nullptr) col2Cards.push_back(threeBandCard.get());
        if (stereoImageCard != nullptr) col2Cards.push_back(stereoImageCard.get());
        if (spectrogramCard != nullptr) col2Cards.push_back(spectrogramCard.get());

        int col2ExpandedCount = 0;
        for (auto* card : col2Cards)
            if (card->getExpanded()) col2ExpandedCount++;

        int col2FoldedCount = static_cast<int>(col2Cards.size()) - col2ExpandedCount;
        int col2TotalSpacing = spacing * (static_cast<int>(col2Cards.size()) - 1);
        int col2FoldedTotal = col2FoldedCount * headerHeight;
        int col2AvailableForExpanded = availableHeight - col2TotalSpacing - col2FoldedTotal;
        int col2ExpandedHeight = col2ExpandedCount > 0
            ? juce::jmax(expandedMinHeight, col2AvailableForExpanded / col2ExpandedCount) : 0;

        int currentY2 = spacing;
        for (auto* card : col2Cards)
        {
            int h = card->getExpanded() ? col2ExpandedHeight : headerHeight;
            card->setBounds(col2X, currentY2, columnWidth, h);
            currentY2 += h + spacing;
        }

        int col2Y = currentY2;

        int minCol2Height = (col2ExpandedCount * expandedMinHeight)
            + (col2FoldedCount * headerHeight) + col2TotalSpacing + spacing * 2;

        // Dynamic resize limits (双列模式)
        int minWidth = minCardWidth * 2 + spacing * 3;
        int dynamicMinHeight = juce::jmax(minCol1Height, minCol2Height);
        setResizeLimits(minWidth, dynamicMinHeight, 2400, 1600);

        // Content height = max of both columns
        int contentHeight = juce::jmax(col1Y, col2Y);
        contentComponent->setSize(width, contentHeight);
    }
    // ========================================================================
    // 单列模式 (width < 800)
    // ========================================================================
    else
    {
        // Single column resize limits
        setResizeLimits(minCardWidth, 500, 2400, 1600);

        int yPos = spacing;

        auto layoutCard = [&](MeterCardComponent* card) {
            if (card != nullptr && card->isVisible())  // ✅ 必须检查可见性！
            {
                // CRITICAL: Use actual current height, not getDesiredHeight()
                // This preserves animation state during 60Hz timer callbacks
                int cardHeight = card->getHeight();

                // Only update X, Y, Width - preserve animated Height
                card->setBounds(spacing,
                              yPos,
                              width - spacing * 2,
                              cardHeight);
                yPos += cardHeight + spacing;
            }
        };

        layoutCard(levelsCard.get());
        layoutCard(vuMeterCard.get());
        layoutCard(threeBandCard.get());
        layoutCard(spectrumCard.get());
        layoutCard(phaseCard.get());
        layoutCard(stereoImageCard.get());
        layoutCard(spectrogramCard.get());
        layoutCard(psrCard.get());

        // Set content component size
        contentComponent->setSize(width, yPos);
    }
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
}
