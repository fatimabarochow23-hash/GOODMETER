/*
  ==============================================================================
    MarathonNonoComponent.h
    GOODMETER iOS - Marathon art style character component

    iOS-specific component using dot matrix rendering
    Compatible interface with HoloNonoComponent
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../GoodMeterLookAndFeel.h"
#include "MarathonRenderer.h"
#include "MarathonCharacterData.h"

//==============================================================================
#ifndef GOODMETER_NONO_ANALYSIS_RESULT_DEFINED
#define GOODMETER_NONO_ANALYSIS_RESULT_DEFINED 1
struct NonoAnalysisResult
{
    float peakDBFS = -100.0f;
    float momentaryMaxLUFS = -100.0f;
    float shortTermMaxLUFS = -100.0f;
    float integratedLUFS = -100.0f;
    float centerLUFS = -100.0f;
    int numChannels = 0;
};
#endif

//==============================================================================
class MarathonNonoComponent : public juce::Component, public juce::Timer
{
public:
    enum class SkinType { Guoba, Nono };

    MarathonNonoComponent(GOODMETERAudioProcessor& processor)
        : audioProcessor(processor)
    {
        // Codex: 主人明确要求 ASCII 精灵不要再是“大而松散的一团字符”，
        // 所以我把画布放大，让同一只角色只占中间一小块区域，这样单字更密、
        // 轮廓更清楚、大小也更接近原 PNG 精灵。
        canvas = std::make_unique<DotMatrixCanvas>(48, 36);
        shapeRenderer = std::make_unique<ShapeRenderer>(*canvas);

        // Load character data
        currentSkin = SkinType::Guoba;
        characterData = MarathonCharacterData::createGuoba();

        startTimerHz(60);
    }

    ~MarathonNonoComponent() override
    {
        stopTimer();
    }

    // Compatible interface with HoloNonoComponent
    std::function<void(const juce::URL&)> onImportFileChosen;

    void setSkin(SkinType s)
    {
        if (s == currentSkin) return;
        currentSkin = s;
        characterData = (s == SkinType::Nono)
            ? MarathonCharacterData::createNono()
            : MarathonCharacterData::createGuoba();
        repaint();
    }

    SkinType getSkin() const { return currentSkin; }
    bool isGuoba() const { return currentSkin == SkinType::Guoba; }

    void triggerExtractExpression() { isExtractingVideo = true; }
    void stopExtractExpression() { isExtractingVideo = false; }

    void analyzeFile(const juce::File& file)
    {
        juce::ignoreUnused(file);
        currentState = State::Analyzing;
        repaint();
    }

    void setDarkTheme(bool dark)
    {
        isDarkTheme = dark;
        repaint();
    }

    void setSourceSpriteImage(const juce::Image& image)
    {
        if (image.isNull())
        {
            sourceSpriteImage = {};
            return;
        }

        sourceSpriteImage = image.createCopy();
    }

    void paint(juce::Graphics& g) override
    {
        // Normal pipeline
        canvas->clear(U' ');
        explicitCharacterBounds = {};

        drawCharacter();

        if (currentState == State::ShowingResults)
            drawResults();

        auto target = getLocalBounds().toFloat();
        const auto occupied = !explicitCharacterBounds.isEmpty() ? explicitCharacterBounds
                                                                 : getOccupiedBounds();

        if (!occupied.isEmpty())
        {
            const float occupiedAspect = static_cast<float>(occupied.getHeight())
                                       / static_cast<float>(juce::jmax(1, occupied.getWidth()));
            const float maxW = target.getWidth() * 0.68f;
            const float maxH = target.getHeight() * 0.44f;
            const float width = juce::jmin(maxW, maxH / occupiedAspect);
            const float height = width * occupiedAspect;
            const float x = target.getCentreX() - width * 0.5f;
            const float y = target.getY() + target.getHeight() * 0.20f;
            juce::Rectangle<float> spriteArea(x, y, width, height);

            juce::Font monoFont(juce::Font::getDefaultMonospacedFontName(), 22.0f, juce::Font::plain);
            drawCanvasSubset(g, spriteArea, monoFont, occupied);
        }
    }

    void resized() override {}

    void timerCallback() override
    {
        animationPhase += 0.016f;
        breathScale = 1.0f + 0.02f * std::sin(animationPhase * 2.0f);
        blinkPhase += 0.016f;
        orbitPhase += 0.010f;
        formationPhase += 0.022f;
        weavePhase += 0.013f;
        repaint();
    }

private:
    GOODMETERAudioProcessor& audioProcessor;
    std::unique_ptr<DotMatrixCanvas> canvas;
    std::unique_ptr<ShapeRenderer> shapeRenderer;

    enum class State { Idle, Analyzing, ShowingResults };
    State currentState = State::Idle;

    SkinType currentSkin;
    CharacterData characterData;

    float animationPhase = 0.0f;
    float breathScale = 1.0f;
    float blinkPhase = 0.0f;
    float orbitPhase = 0.0f;
    float formationPhase = 0.0f;
    float weavePhase = 0.0f;
    bool isExtractingVideo = false;
    bool isDarkTheme = false;
    juce::Image sourceSpriteImage;
    juce::Rectangle<int> explicitCharacterBounds;

    NonoAnalysisResult analysisResult;

    static constexpr int canvasCols = 48;
    static constexpr int canvasRows = 36;
    static constexpr int spriteCols = 20;
    static constexpr int spriteRows = 14;
    static constexpr int spriteOffsetX = (canvasCols - spriteCols) / 2;
    static constexpr int spriteOffsetY = 5;

    struct AsciiSample
    {
        float coverage = 0.0f;
        float luminance = 0.0f;
        float saturation = 0.0f;
        juce::Colour average = juce::Colours::transparentBlack;
    };

    juce::Colour remapAsciiColour(juce::Colour src, float luminance, float saturation, bool boundary, bool accentLike) const
    {
        const auto readableLight = juce::Colour(0xFF1F2731);
        const auto readableMid = juce::Colour(0xFF5E6772);
        const auto readableDark = juce::Colour(0xFFF2EBDD);
        const auto readableDarkMid = juce::Colour(0xFFC9C0B4);
        const auto guobaGoldDark = juce::Colour(0xFFFFD45B);
        const auto guobaGoldLight = juce::Colour(0xFFC68B10);
        const auto guobaWarmDark = juce::Colour(0xFFFFB27B);
        const auto guobaWarmLight = juce::Colour(0xFFD07757);
        const auto nonoBlueDark = juce::Colour(0xFF73ECFF);
        const auto nonoBlueLight = juce::Colour(0xFF2F75E8);
        const auto nonoCoolDark = juce::Colour(0xFFB8EEFF);
        const auto nonoCoolLight = juce::Colour(0xFF4D8DDA);

        auto targetLight = isDarkTheme ? readableDark : readableLight;
        auto targetMid = isDarkTheme ? readableDarkMid : readableMid;
        auto accentPrimary = currentSkin == SkinType::Guoba
            ? (isDarkTheme ? guobaGoldDark : guobaGoldLight)
            : (isDarkTheme ? nonoBlueDark : nonoBlueLight);
        auto accentSecondary = currentSkin == SkinType::Guoba
            ? (isDarkTheme ? guobaWarmDark : guobaWarmLight)
            : (isDarkTheme ? nonoCoolDark : nonoCoolLight);

        juce::Colour mapped = src;

        if (accentLike || saturation > 0.20f)
        {
            mapped = accentPrimary.interpolatedWith(accentSecondary, 0.22f + saturation * 0.38f);
        }
        else if (luminance > 0.72f)
        {
            mapped = targetLight;
        }
        else if (luminance < 0.18f)
        {
            mapped = targetMid.interpolatedWith(accentPrimary, currentSkin == SkinType::Guoba ? 0.10f : 0.14f);
        }
        else
        {
            mapped = targetMid.interpolatedWith(targetLight, juce::jlimit(0.0f, 1.0f, (luminance - 0.18f) / 0.54f));
        }

        if (boundary)
            mapped = mapped.interpolatedWith(targetLight, isDarkTheme ? 0.20f : 0.10f);

        return mapped;
    }

    static int brailleBitIndex(int localX, int localY)
    {
        static constexpr int bitLut[4][2] =
        {
            { 0, 3 }, // dots 1,4
            { 1, 4 }, // dots 2,5
            { 2, 5 }, // dots 3,6
            { 6, 7 }  // dots 7,8
        };
        return bitLut[juce::jlimit(0, 3, localY)][juce::jlimit(0, 1, localX)];
    }

    static char32_t makeBrailleGlyph(const std::array<float, 8>& dots, float threshold)
    {
        uint8_t mask = 0;
        float strongest = 0.0f;
        int strongestBit = 0;
        for (int i = 0; i < 8; ++i)
        {
            strongest = juce::jmax(strongest, dots[(size_t) i]);
            if (dots[(size_t) i] >= threshold)
                mask |= static_cast<uint8_t>(1u << i);
            if (dots[(size_t) i] >= dots[(size_t) strongestBit])
                strongestBit = i;
        }

        if (mask == 0 && strongest > 0.08f)
            mask = static_cast<uint8_t>(1u << strongestBit);

        return static_cast<char32_t>(0x2800u + mask);
    }

    juce::Rectangle<int> getOccupiedBounds() const
    {
        int minX = canvasCols;
        int minY = canvasRows;
        int maxX = -1;
        int maxY = -1;

        for (int y = 0; y < canvasRows; ++y)
        {
            for (int x = 0; x < canvasCols; ++x)
            {
                const auto cell = canvas->getCell(x, y);
                if (cell.symbol == U' ' || cell.brightness <= 0.04f)
                    continue;

                minX = juce::jmin(minX, x);
                minY = juce::jmin(minY, y);
                maxX = juce::jmax(maxX, x);
                maxY = juce::jmax(maxY, y);
            }
        }

        if (maxX < minX || maxY < minY)
            return {};

        auto bounds = juce::Rectangle<int>(minX, minY, maxX - minX + 1, maxY - minY + 1).expanded(1, 1);
        bounds.setPosition(juce::jlimit(0, canvasCols - 1, bounds.getX()),
                           juce::jlimit(0, canvasRows - 1, bounds.getY()));
        bounds.setSize(juce::jmin(bounds.getWidth(), canvasCols - bounds.getX()),
                       juce::jmin(bounds.getHeight(), canvasRows - bounds.getY()));
        return bounds;
    }

    void drawCanvasSubset(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Font font,
                          juce::Rectangle<int> subset)
    {
        static constexpr float sizeScales[] = { 0.60f, 0.86f, 1.08f, 1.30f };
        const float cellW = bounds.getWidth() / static_cast<float>(juce::jmax(1, subset.getWidth()));
        const float cellH = bounds.getHeight() / static_cast<float>(juce::jmax(1, subset.getHeight()));

        // Braille dot layout: 2 columns x 4 rows
        // Bit mapping matches brailleBitIndex():
        static constexpr int dotCol[] = { 0, 0, 0, 1, 1, 1, 0, 1 };
        static constexpr int dotRow[] = { 0, 1, 2, 0, 1, 2, 3, 3 };

        for (int y = subset.getY(); y < subset.getBottom(); ++y)
        {
            for (int x = subset.getX(); x < subset.getRight(); ++x)
            {
                const auto cell = canvas->getCell(x, y);
                if (cell.symbol == U' ' || cell.brightness <= 0.0f)
                    continue;

                const auto drawColour = cell.color.withMultipliedAlpha(cell.brightness);
                g.setColour(drawColour);

                const float px = bounds.getX() + (x - subset.getX()) * cellW;
                const float py = bounds.getY() + (y - subset.getY()) * cellH;
                const float scale = sizeScales[juce::jlimit<int>(0, 3, static_cast<int>(cell.sizeLevel))];

                const char32_t ch = cell.symbol;
                if (ch >= 0x2800u && ch <= 0x28FFu)
                {
                    // iOS system font renders Braille as tiny dots — draw programmatically instead
                    const uint8_t mask = static_cast<uint8_t>(ch - 0x2800u);
                    if (mask == 0) continue;

                    const float dotR = juce::jmin(cellW, cellH) * 0.14f * scale;
                    const float padX = cellW * 0.20f;
                    const float padY = cellH * 0.06f;
                    const float colSpacing = cellW - 2.0f * padX;
                    const float rowSpacing = (cellH - 2.0f * padY) / 3.0f;

                    for (int bit = 0; bit < 8; ++bit)
                    {
                        if (mask & (1u << bit))
                        {
                            const float cx = px + padX + dotCol[bit] * colSpacing;
                            const float cy = py + padY + dotRow[bit] * rowSpacing;
                            g.fillEllipse(cx - dotR, cy - dotR, dotR * 2.0f, dotR * 2.0f);
                        }
                    }
                }
                else
                {
                    // Non-Braille characters: use drawText as before
                    const float drawW = cellW * juce::jmin(1.42f, scale + 0.18f);
                    const float drawH = cellH * juce::jmin(1.42f, scale + 0.14f);
                    const auto drawBounds = juce::Rectangle<float>(px + (cellW - drawW) * 0.5f,
                                                                   py + (cellH - drawH) * 0.5f,
                                                                   drawW, drawH);
                    g.setFont(font.withHeight(font.getHeight() * scale));
                    g.drawText(juce::String::charToString(cell.symbol),
                               drawBounds.toNearestInt(),
                               juce::Justification::centred,
                               false);
                }
            }
        }
    }

    juce::Colour remapBrailleColour(const juce::Colour& src, float luminance,
                                    float saturation, bool edge, bool accentLike,
                                    float centerBias) const
    {
        juce::ignoreUnused(src, saturation, accentLike);

        const auto lightReadable = juce::Colour(0xFF1D232B);
        const auto lightMuted = juce::Colour(0xFF59616C);
        const auto darkReadable = juce::Colour(0xFFF2ECE2);
        const auto darkMuted = juce::Colour(0xFFC9C2B7);

        const auto neutralBase = isDarkTheme ? darkReadable : lightReadable;
        const auto neutralMid = isDarkTheme ? darkMuted : lightMuted;
        const auto shadowTone = currentSkin == SkinType::Guoba
            ? (isDarkTheme ? juce::Colour(0xFFA79F93) : juce::Colour(0xFF757D87))
            : (isDarkTheme ? juce::Colour(0xFFCCD7E2) : juce::Colour(0xFF687482));

        juce::Colour mapped = neutralMid.interpolatedWith(
            neutralBase,
            juce::jlimit(0.0f, 1.0f, 0.42f + centerBias * 0.26f + (1.0f - luminance) * 0.18f));

        if (luminance < 0.25f)
            mapped = mapped.interpolatedWith(shadowTone, isDarkTheme ? 0.18f : 0.24f);

        if (edge)
            mapped = mapped.interpolatedWith(neutralBase, isDarkTheme ? 0.30f : 0.18f);

        return mapped;
    }

    void drawImageDrivenAccentAnchors(int startX, int startY, int targetCols, int targetRows)
    {
        auto stamp = [this, startX, startY, targetCols, targetRows]
                     (int x, int y, char32_t glyph, juce::Colour colour,
                      float brightness, uint8_t sizeLevel)
        {
            if (x < 0 || x >= targetCols || y < 0 || y >= targetRows)
                return;
            canvas->setCell(startX + x, startY + y, glyph, colour, 1, brightness, sizeLevel);
        };

        if (currentSkin == SkinType::Guoba)
        {
            const auto eyeColour = isDarkTheme ? juce::Colour(0xFFFFDB63) : juce::Colour(0xFFB97D08);
            const auto hoodColour = isDarkTheme ? juce::Colour(0xFFE9E0D4) : juce::Colour(0xFF6A7078);
            stamp(9, 6, U'⠿', eyeColour, 0.98f, 3);
            stamp(14, 6, U'⠿', eyeColour, 0.98f, 3);
            stamp(12, 8, U'⠒', hoodColour, 0.70f, 2);
            stamp(10, 13, U'⠶', hoodColour.withAlpha(0.86f), 0.76f, 2);
            stamp(12, 13, U'⠶', hoodColour.withAlpha(0.86f), 0.76f, 2);
            stamp(14, 13, U'⠶', hoodColour.withAlpha(0.86f), 0.76f, 2);
        }
        else
        {
            const auto eyeColour = isDarkTheme ? juce::Colour(0xFF79F6F0) : juce::Colour(0xFF0A8C95);
            const auto edgeColour = isDarkTheme ? juce::Colour(0xFFD9EFFF) : juce::Colour(0xFF41659C);
            stamp(9, 6, U'⠿', eyeColour, 0.98f, 3);
            stamp(14, 6, U'⠿', eyeColour, 0.98f, 3);
            stamp(8, 2, U'⠉', edgeColour.withAlpha(0.92f), 0.82f, 2);
            stamp(15, 2, U'⠉', edgeColour.withAlpha(0.92f), 0.82f, 2);
            stamp(7, 8, U'⠂', eyeColour.withAlpha(0.72f), 0.58f, 1);
            stamp(16, 8, U'⠂', eyeColour.withAlpha(0.72f), 0.58f, 1);
        }
    }

    bool drawImageDrivenCharacter()
    {
        if (sourceSpriteImage.isNull())
            return false;

        struct BrailleCell
        {
            std::array<float, 8> dots {};
            float coverage = 0.0f;
            float luminance = 0.0f;
            float saturation = 0.0f;
            juce::Colour average = juce::Colours::transparentBlack;
            bool accentLike = false;
        };

        const int targetCols = 24;
        const int targetRows = 18;
        const int startX = (canvasCols - targetCols) / 2;
        const int startY = 6;
        explicitCharacterBounds = juce::Rectangle<int>(startX, startY, targetCols, targetRows).expanded(1, 1);

        std::vector<BrailleCell> cells(static_cast<size_t>(targetCols * targetRows));
        auto cellAt = [&cells, targetCols, targetRows](int x, int y) -> BrailleCell&
        {
            return cells[static_cast<size_t>(juce::jlimit(0, targetRows - 1, y) * targetCols
                                           + juce::jlimit(0, targetCols - 1, x))];
        };

        int populatedCells = 0;
        float maxCoverage = 0.0f;

        for (int gy = 0; gy < targetRows; ++gy)
        {
            for (int gx = 0; gx < targetCols; ++gx)
            {
                auto& cell = cellAt(gx, gy);
                float alphaAccum = 0.0f;
                float lumAccum = 0.0f;
                float satAccum = 0.0f;
                float rAccum = 0.0f;
                float gAccum = 0.0f;
                float bAccum = 0.0f;

                for (int dotY = 0; dotY < 4; ++dotY)
                {
                    for (int dotX = 0; dotX < 2; ++dotX)
                    {
                        float subAlpha = 0.0f;
                        float subLum = 0.0f;

                        for (int sy = 0; sy < 2; ++sy)
                        {
                            for (int sx = 0; sx < 2; ++sx)
                            {
                                const float u = (static_cast<float>(gx * 2 + dotX)
                                               + (static_cast<float>(sx) + 0.5f) * 0.5f)
                                              / static_cast<float>(targetCols * 2);
                                const float v = (static_cast<float>(gy * 4 + dotY)
                                               + (static_cast<float>(sy) + 0.5f) * 0.5f)
                                              / static_cast<float>(targetRows * 4);
                                const int px = juce::jlimit(0, sourceSpriteImage.getWidth() - 1,
                                                            static_cast<int>(std::floor(u * sourceSpriteImage.getWidth())));
                                const int py = juce::jlimit(0, sourceSpriteImage.getHeight() - 1,
                                                            static_cast<int>(std::floor(v * sourceSpriteImage.getHeight())));
                                const auto colour = sourceSpriteImage.getPixelAt(px, py);
                                const float alpha = colour.getFloatAlpha();
                                const float lum = colour.getPerceivedBrightness();

                                subAlpha += alpha;
                                subLum += lum * alpha;
                                alphaAccum += alpha;
                                lumAccum += lum * alpha;
                                satAccum += colour.getSaturation() * alpha;
                                rAccum += colour.getFloatRed() * alpha;
                                gAccum += colour.getFloatGreen() * alpha;
                                bAccum += colour.getFloatBlue() * alpha;
                            }
                        }

                        subAlpha *= 0.25f;
                        const int bitIndex = brailleBitIndex(dotX, dotY);
                        if (subAlpha > 0.02f)
                        {
                            const float subAvgLum = subLum / (subAlpha * 4.0f);
                            cell.dots[(size_t) bitIndex] = juce::jlimit(0.0f, 1.0f,
                                                                        subAlpha * 0.80f
                                                                        + (1.0f - subAvgLum) * 0.20f);
                        }
                    }
                }

                if (alphaAccum <= 0.01f)
                    continue;

                cell.coverage = juce::jlimit(0.0f, 1.0f, alphaAccum / 32.0f);
                cell.luminance = lumAccum / alphaAccum;
                cell.saturation = satAccum / alphaAccum;
                cell.average = juce::Colour::fromFloatRGBA(rAccum / alphaAccum,
                                                           gAccum / alphaAccum,
                                                           bAccum / alphaAccum,
                                                           1.0f);
                cell.accentLike = cell.saturation > 0.18f
                               || cell.average.getFloatBlue() - cell.average.getFloatRed() > 0.08f
                               || cell.average.getFloatRed() + cell.average.getFloatGreen()
                                  > cell.average.getFloatBlue() * 1.40f;

                if (cell.coverage > 0.022f)
                {
                    ++populatedCells;
                    maxCoverage = juce::jmax(maxCoverage, cell.coverage);
                }
            }
        }

        // If the captured PNG is too pale/transparent to survive ASCII conversion,
        // fall back to the built-in template skeleton so page 1 never looks empty.
        if (populatedCells < 6 || maxCoverage < 0.02f)
            return false;

        auto getCoverage = [&cellAt, targetCols, targetRows](int x, int y) -> float
        {
            if (x < 0 || x >= targetCols || y < 0 || y >= targetRows)
                return 0.0f;
            return cellAt(x, y).coverage;
        };

        const auto outlineLight = isDarkTheme ? juce::Colour(0xFFFFF7EE) : juce::Colour(0xFF2A3340);
        for (int gy = 0; gy < targetRows; ++gy)
        {
            for (int gx = 0; gx < targetCols; ++gx)
            {
                const auto& cell = cellAt(gx, gy);
                if (cell.coverage <= 0.022f)
                    continue;

                const bool edge = getCoverage(gx - 1, gy) < 0.035f || getCoverage(gx + 1, gy) < 0.035f
                               || getCoverage(gx, gy - 1) < 0.035f || getCoverage(gx, gy + 1) < 0.035f;
                const float centerBiasX = 1.0f - std::abs((static_cast<float>(gx) + 0.5f)
                                                          / static_cast<float>(targetCols) - 0.5f) * 2.0f;
                const float centerBiasY = 1.0f - std::abs((static_cast<float>(gy) + 0.5f)
                                                          / static_cast<float>(targetRows) - 0.48f) * 2.0f;
                const float centerBias = juce::jlimit(0.0f, 1.0f,
                                                      centerBiasX * 0.58f + centerBiasY * 0.42f);

                std::array<float, 8> boostedDots = cell.dots;
                for (auto& dot : boostedDots)
                {
                    dot = juce::jlimit(0.0f, 1.0f,
                                       dot
                                       + cell.coverage * 0.08f
                                       + centerBias * 0.10f
                                       + (edge ? 0.12f : 0.0f));
                }

                const float threshold = edge ? 0.18f : (cell.coverage > 0.42f ? 0.15f : 0.20f);
                const char32_t glyph = makeBrailleGlyph(boostedDots, threshold);
                auto colour = remapBrailleColour(cell.average, cell.luminance, cell.saturation,
                                                 edge, cell.accentLike, centerBias);
                if (edge)
                    colour = colour.interpolatedWith(outlineLight, isDarkTheme ? 0.20f : 0.12f);

                const float pulse = 0.5f + 0.5f * std::sin(animationPhase * 3.2f + gx * 0.21f - gy * 0.17f);
                const float energy = juce::jlimit(0.0f, 1.0f,
                                                  cell.coverage * 0.60f
                                                  + centerBias * 0.24f
                                                  + (edge ? 0.22f : 0.0f)
                                                  + (cell.accentLike ? 0.12f : 0.0f));
                float brightness = juce::jlimit(isDarkTheme ? 0.56f : 0.74f, 1.0f,
                                                (isDarkTheme ? 0.66f : 0.80f)
                                                + cell.coverage * (isDarkTheme ? 0.30f : 0.22f)
                                                + centerBias * 0.08f
                                                + (edge ? 0.10f : 0.0f)
                                                + pulse * (cell.accentLike ? 0.06f : 0.03f));
                const uint8_t sizeLevel = energy > 0.82f ? 3 : (energy > 0.58f ? 2 : (energy > 0.26f ? 1 : 0));

                canvas->setCell(startX + gx, startY + gy, glyph, colour, 1, brightness, sizeLevel);

                if (edge && cell.coverage > 0.18f)
                {
                    const float haloAlpha = isDarkTheme ? 0.10f : 0.07f;
                    if (gx > 0 && getCoverage(gx - 1, gy) < 0.025f)
                        canvas->setCell(startX + gx - 1, startY + gy, U'⠂', colour.withAlpha(haloAlpha), 0, haloAlpha, 0);
                    if (gx < targetCols - 1 && getCoverage(gx + 1, gy) < 0.025f)
                        canvas->setCell(startX + gx + 1, startY + gy, U'⠂', colour.withAlpha(haloAlpha), 0, haloAlpha, 0);
                }
            }
        }

        drawImageDrivenAccentAnchors(startX, startY, targetCols, targetRows);
        return true;
    }

    void drawCharacter()
    {
        if (!sourceSpriteImage.isNull() && drawImageDrivenCharacter())
            return;

        static const char32_t* guobaPixels[14] = {
            U"....................",
            U"......##....##......",
            U".....####..####.....",
            U"....############....",
            U"....##@######@##....",
            U"....############....",
            U".....###.##.###.....",
            U".....##########.....",
            U"....############....",
            U"...##############...",
            U"...#######..#####...",
            U"....###......###....",
            U".....##......##.....",
            U"...................."
        };

        static const char32_t* nonoPixels[14] = {
            U"....................",
            U"......##....##......",
            U".....####..####.....",
            U".....##########.....",
            U"....###@####@###....",
            U"....############....",
            U".....##########.....",
            U".....###.##.###.....",
            U".....##########.....",
            U"....#####..#####....",
            U"....####....####....",
            U".....##......##.....",
            U".....##......##.....",
            U"...................."
        };

        const auto** activePixels = currentSkin == SkinType::Guoba ? guobaPixels : nonoPixels;
        auto isMaskFilled = [activePixels](int x, int y) -> bool
        {
            if (x < 0 || x >= 20 || y < 0 || y >= 14)
                return false;
            return activePixels[y][x] != U'.';
        };

        const int targetCols = 24;
        const int targetRows = 18;
        const int startX = (canvasCols - targetCols) / 2;
        const int startY = 6;
        explicitCharacterBounds = juce::Rectangle<int>(startX, startY, targetCols, targetRows).expanded(1, 1);

        const auto neutralColour = currentSkin == SkinType::Guoba
            ? (isDarkTheme ? juce::Colour(0xFFE8E1D6) : juce::Colour(0xFF49453E))
            : (isDarkTheme ? juce::Colour(0xFFF2F7FB) : juce::Colour(0xFF2A3340));
        const auto edgeColour = currentSkin == SkinType::Guoba
            ? (isDarkTheme ? juce::Colour(0xFFFFFAF1) : juce::Colour(0xFF1A1E24))
            : (isDarkTheme ? juce::Colour(0xFFF7FBFF) : juce::Colour(0xFF171D25));
        const auto accentColour = currentSkin == SkinType::Guoba
            ? (isDarkTheme ? juce::Colour(0xFFFFD35A) : juce::Colour(0xFFB27A18))
            : (isDarkTheme ? juce::Colour(0xFF73E8FF) : juce::Colour(0xFF2A71D8));
        const auto accentSecondary = currentSkin == SkinType::Guoba
            ? (isDarkTheme ? juce::Colour(0xFFFFB07A) : juce::Colour(0xFFD17A53))
            : (isDarkTheme ? juce::Colour(0xFFB9ECFF) : juce::Colour(0xFF6B97EA));

        auto stamp = [this, startX, startY, targetCols, targetRows]
                     (int x, int y, char32_t glyph, juce::Colour colour,
                      float brightness, uint8_t sizeLevel)
        {
            if (x < 0 || x >= targetCols || y < 0 || y >= targetRows)
                return;
            canvas->setCell(startX + x, startY + y, glyph, colour, 1, brightness, sizeLevel);
        };

        auto sampleMask = [isMaskFilled](float sampleX, float sampleY) -> float
        {
            int hits = 0;
            for (int oy = 0; oy < 2; ++oy)
            {
                for (int ox = 0; ox < 2; ++ox)
                {
                    const float fx = sampleX + (static_cast<float>(ox) - 0.5f) * 0.34f;
                    const float fy = sampleY + (static_cast<float>(oy) - 0.5f) * 0.34f;
                    if (isMaskFilled(static_cast<int>(std::round(fx)),
                                     static_cast<int>(std::round(fy))))
                        ++hits;
                }
            }
            return static_cast<float>(hits) * 0.25f;
        };

        struct BrailleCell
        {
            std::array<float, 8> dots {};
            float coverage = 0.0f;
        };

        std::vector<BrailleCell> cells(static_cast<size_t>(targetCols * targetRows));
        auto cellAt = [&cells, targetCols, targetRows](int x, int y) -> BrailleCell&
        {
            return cells[static_cast<size_t>(juce::jlimit(0, targetRows - 1, y) * targetCols
                                           + juce::jlimit(0, targetCols - 1, x))];
        };

        for (int gy = 0; gy < targetRows; ++gy)
        {
            for (int gx = 0; gx < targetCols; ++gx)
            {
                auto& cell = cellAt(gx, gy);
                float coverage = 0.0f;
                for (int dotY = 0; dotY < 4; ++dotY)
                {
                    for (int dotX = 0; dotX < 2; ++dotX)
                    {
                        const float sampleX = ((static_cast<float>(gx * 2 + dotX) + 0.5f)
                                             / static_cast<float>(targetCols * 2))
                                             * static_cast<float>(spriteCols - 1);
                        const float sampleY = ((static_cast<float>(gy * 4 + dotY) + 0.5f)
                                             / static_cast<float>(targetRows * 4))
                                             * static_cast<float>(spriteRows - 1);
                        const float mask = sampleMask(sampleX, sampleY);
                        cell.dots[static_cast<size_t>(brailleBitIndex(dotX, dotY))] = mask;
                        coverage += mask;
                    }
                }
                cell.coverage = coverage / 8.0f;
            }
        }

        auto coverageAt = [&cellAt, targetCols, targetRows](int x, int y) -> float
        {
            if (x < 0 || x >= targetCols || y < 0 || y >= targetRows)
                return 0.0f;
            return cellAt(x, y).coverage;
        };

        for (int gy = 0; gy < targetRows; ++gy)
        {
            for (int gx = 0; gx < targetCols; ++gx)
            {
                const auto& cell = cellAt(gx, gy);
                if (cell.coverage <= 0.08f)
                    continue;

                const bool edge = coverageAt(gx - 1, gy) < 0.09f || coverageAt(gx + 1, gy) < 0.09f
                               || coverageAt(gx, gy - 1) < 0.09f || coverageAt(gx, gy + 1) < 0.09f;
                const float centerBiasX = 1.0f - std::abs((static_cast<float>(gx) + 0.5f)
                                                          / static_cast<float>(targetCols) - 0.5f) * 2.0f;
                const float centerBiasY = 1.0f - std::abs((static_cast<float>(gy) + 0.5f)
                                                          / static_cast<float>(targetRows) - 0.46f) * 2.0f;
                const float centerBias = juce::jlimit(0.0f, 1.0f, centerBiasX * 0.58f + centerBiasY * 0.42f);
                const bool torsoBand = currentSkin == SkinType::Guoba
                    ? (gy >= 10 && gy <= 13 && gx >= 7 && gx <= 16)
                    : (gy >= 9 && gy <= 13 && gx >= 7 && gx <= 16);
                const bool faceZone = (gy >= 4 && gy <= 8 && gx >= 7 && gx <= 16);

                auto dots = cell.dots;
                for (auto& dot : dots)
                    dot = juce::jlimit(0.0f, 1.0f, dot + cell.coverage * 0.24f + centerBias * 0.22f + (edge ? 0.18f : 0.0f));

                const char32_t glyph = makeBrailleGlyph(dots, edge ? 0.24f : 0.34f);
                auto colour = neutralColour;
                if (torsoBand)
                    colour = colour.interpolatedWith(accentColour, currentSkin == SkinType::Guoba ? 0.10f : 0.08f);
                if (faceZone && !edge)
                    colour = colour.interpolatedWith(accentSecondary, currentSkin == SkinType::Guoba ? 0.06f : 0.10f);
                if (edge)
                    colour = colour.interpolatedWith(edgeColour, isDarkTheme ? 0.52f : 0.28f);

                const float pulse = 0.5f + 0.5f * std::sin(animationPhase * 2.6f + gx * 0.24f - gy * 0.17f);
                float brightness = isDarkTheme
                    ? juce::jlimit(0.78f, 1.0f, 0.80f + cell.coverage * 0.24f + centerBias * 0.08f + pulse * 0.05f)
                    : juce::jlimit(0.90f, 1.0f, 0.92f + cell.coverage * 0.10f + centerBias * 0.04f + pulse * 0.03f);
                const uint8_t sizeLevel = edge ? 3 : (cell.coverage > 0.48f ? 2 : 1);
                stamp(gx, gy, glyph, colour, brightness, sizeLevel);
            }
        }

        if (currentSkin == SkinType::Guoba)
        {
            stamp(9, 5, U'⣿', accentColour, 1.0f, 3);
            stamp(14, 5, U'⣿', accentColour, 1.0f, 3);
            stamp(11, 7, U'⠒', edgeColour, isDarkTheme ? 0.92f : 0.86f, 2);
            stamp(10, 11, U'⣶', accentColour.interpolatedWith(accentSecondary, 0.18f), 0.96f, 3);
            stamp(11, 11, U'⣿', accentColour, 1.0f, 3);
            stamp(12, 11, U'⣿', accentColour, 1.0f, 3);
            stamp(13, 11, U'⣶', accentColour.interpolatedWith(accentSecondary, 0.18f), 0.96f, 3);
        }
        else
        {
            stamp(9, 5, U'⣿', accentColour, 1.0f, 3);
            stamp(14, 5, U'⣿', accentColour, 1.0f, 3);
            stamp(6, 6, U'⠂', accentSecondary, isDarkTheme ? 0.82f : 0.74f, 1);
            stamp(17, 6, U'⠂', accentSecondary, isDarkTheme ? 0.82f : 0.74f, 1);
            stamp(7, 2, U'⣀', accentColour, isDarkTheme ? 0.74f : 0.70f, 2);
            stamp(14, 2, U'⣀', accentColour, isDarkTheme ? 0.74f : 0.70f, 2);
            stamp(10, 10, U'⠒', edgeColour, isDarkTheme ? 0.82f : 0.76f, 2);
        }
    }

    void drawEye(const FacialFeature& eye, int cx, int cy, int scale)
    {
        int ex = cx + (int)(eye.center.x * scale);
        int ey = cy + (int)(eye.center.y * scale);
        int er = (int)(eye.radius * scale);

        char32_t symbol = (currentState == State::Analyzing) ? U'×' : U'□';
        shapeRenderer->drawCircle(ex, ey, er, symbol, juce::Colours::white, false);
    }

    void drawResults()
    {
        int x = 5;
        int y = canvas->getHeight() - 10;

        // Yellow labels + Pink numbers
        juce::Colour yellow(0xFFFFD700);
        juce::Colour pink(0xFFFF69B4);

        juce::String text = "Peak: " + juce::String(analysisResult.peakDBFS, 1) + " dB";
        for (int i = 0; i < text.length() && x + i < canvas->getWidth(); ++i)
            canvas->setCell(x + i, y, text[i], (i < 6) ? yellow : pink, 2);
    }
};
