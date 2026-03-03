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
    viewport->setBounds(bounds);

    const int width = bounds.getWidth();
    const int spacing = static_cast<int>(GoodMeterLookAndFeel::cardSpacing);

    // ===== 收集活跃面板 =====
    auto tryAdd = [](std::vector<MeterCardComponent*>& vec, MeterCardComponent* c) {
        if (c != nullptr) vec.push_back(c);
    };

    std::vector<MeterCardComponent*> allCards;
    tryAdd(allCards, levelsCard.get());
    tryAdd(allCards, vuMeterCard.get());
    tryAdd(allCards, phaseCard.get());
    tryAdd(allCards, spectrumCard.get());
    tryAdd(allCards, threeBandCard.get());
    tryAdd(allCards, stereoImageCard.get());
    tryAdd(allCards, spectrogramCard.get());
    tryAdd(allCards, psrCard.get());
    tryAdd(allCards, nonoCard.get());

    const int activePanels = static_cast<int>(allCards.size());
    if (activePanels == 0) return;

    // ==========================================================
    // 精确物理断点 (800 / 1200)
    // ==========================================================
    int numColumns;
    if (width >= 1000)
        numColumns = 3;
    else if (width >= 650)
        numColumns = 2;
    else
        numColumns = 1;

    numColumns = juce::jmin(numColumns, activePanels);

    // ==========================================================
    // 按列分配卡片 (固定3组分法)
    //   A: Levels, VU, Phase
    //   B: Spectrum, 3-Band, Stereo
    //   C: Spectrogram, PSR, NONO
    // ==========================================================
    std::vector<std::vector<MeterCardComponent*>> columns(numColumns);

    if (numColumns >= 3)
    {
        tryAdd(columns[0], levelsCard.get());
        tryAdd(columns[0], vuMeterCard.get());
        tryAdd(columns[0], phaseCard.get());
        tryAdd(columns[1], spectrumCard.get());
        tryAdd(columns[1], threeBandCard.get());
        tryAdd(columns[1], stereoImageCard.get());
        tryAdd(columns[2], spectrogramCard.get());
        tryAdd(columns[2], psrCard.get());
        tryAdd(columns[2], nonoCard.get());
    }
    else if (numColumns == 2)
    {
        // 2列: 4 vs 5 平衡划分 (Spectrum 归入左列)
        tryAdd(columns[0], levelsCard.get());
        tryAdd(columns[0], vuMeterCard.get());
        tryAdd(columns[0], phaseCard.get());
        tryAdd(columns[0], spectrumCard.get());
        tryAdd(columns[1], threeBandCard.get());
        tryAdd(columns[1], stereoImageCard.get());
        tryAdd(columns[1], spectrogramCard.get());
        tryAdd(columns[1], psrCard.get());
        tryAdd(columns[1], nonoCard.get());
    }
    else
    {
        columns[0] = allCards;
    }

    // ==========================================================
    // 弹性空间分配 (Flex-Grow): 每列独立计算展开高度
    // ==========================================================
    setResizeLimits(380, 400, 2400, 1600);

    const int minExpandedHeight = 280;
    const int collapsedHeight = 48;

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
            // 折叠面板用实际动画高度 (保留收起动画平滑过渡)
            int collapsedTotal = 0;
            for (auto* card : columns[col])
                if (!card->getExpanded())
                    collapsedTotal += card->getHeight();

            int totalCards = static_cast<int>(columns[col].size());
            int totalMargins = spacing * (totalCards + 1);
            int deadSpace = collapsedTotal + totalMargins;
            int spaceLeft = availableViewportHeight - deadSpace;
            int calculated = spaceLeft / expandedCounts[col];

            flexExpandedHeight[col] = juce::jmax(minExpandedHeight, calculated);
        }
    }

    // ===== 第三步: 应用弹性高度排版 =====
    std::vector<int> columnHeights(numColumns, spacing);

    for (int col = 0; col < numColumns; ++col)
    {
        int colX = (numColumns == 1)
            ? spacing
            : spacing + col * (columnWidth + spacing);

        for (auto* card : columns[col])
        {
            int h = card->getExpanded() ? flexExpandedHeight[col] : card->getHeight();
            card->setBounds(colX, columnHeights[col], columnWidth, h);
            columnHeights[col] += h + spacing;
        }
    }

    // 最长列高度 → contentComponent 高度 → 超出 Viewport 激活滚轮
    int maxContentHeight = 0;
    for (int h : columnHeights)
        maxContentHeight = juce::jmax(maxContentHeight, h);

    int finalHeight = juce::jmax(bounds.getHeight() + 1, maxContentHeight);
    contentComponent->setBounds(0, 0, width, finalHeight);
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
