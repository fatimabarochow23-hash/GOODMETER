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
        sourceSpriteImage = {};
        explicitCharacterBounds = {};
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
            const float maxW = target.getWidth() * 0.96f;
            const float maxH = target.getHeight() * 0.66f;
            const float width = juce::jmin(maxW, maxH / occupiedAspect);
            const float height = width * occupiedAspect;
            const float x = target.getCentreX() - width * 0.5f;
            const float y = target.getY() + target.getHeight() * 0.10f;
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

    static bool isPrimitiveGlyph(char32_t ch)
    {
        switch (ch)
        {
            case U'•':
            case U'●':
            case U'▪':
            case U'■':
            case U'│':
            case U'─':
            case U'╱':
            case U'╲':
            case U'⌒':
            case U'⌣':
            case U'∧':
            case U'∨':
            case U'+':
            case U'×':
                return true;
            default:
                return false;
        }
    }

    static bool drawPrimitiveGlyph(juce::Graphics& g, juce::Rectangle<float> cellBounds,
                                   char32_t ch, juce::Colour colour, float scale)
    {
        const float cellW = cellBounds.getWidth();
        const float cellH = cellBounds.getHeight();
        const float cx = cellBounds.getCentreX();
        const float cy = cellBounds.getCentreY();
        const float minDim = juce::jmin(cellW, cellH);

        g.setColour(colour);

        switch (ch)
        {
            case U'•':
            {
                const float d = minDim * 0.24f * scale;
                g.fillEllipse(cx - d * 0.5f, cy - d * 0.5f, d, d);
                return true;
            }

            case U'●':
            {
                const float d = minDim * 0.52f * scale;
                g.fillEllipse(cx - d * 0.5f, cy - d * 0.5f, d, d);
                return true;
            }

            case U'▪':
            case U'■':
            {
                const float w = minDim * ((ch == U'■') ? 0.68f : 0.52f) * scale;
                const float radius = w * 0.14f;
                g.fillRoundedRectangle(cx - w * 0.5f, cy - w * 0.5f, w, w, radius);
                return true;
            }

            case U'│':
            {
                const float w = juce::jmax(1.6f, cellW * 0.16f * scale);
                const float h = cellH * 0.92f;
                g.fillRoundedRectangle(cx - w * 0.5f, cy - h * 0.5f, w, h, w * 0.5f);
                return true;
            }

            case U'─':
            {
                const float h = juce::jmax(1.6f, cellH * 0.16f * scale);
                const float w = cellW * 0.92f;
                g.fillRoundedRectangle(cx - w * 0.5f, cy - h * 0.5f, w, h, h * 0.5f);
                return true;
            }

            case U'╱':
            case U'╲':
            case U'∧':
            case U'∨':
            case U'+':
            case U'×':
            {
                auto stroke = juce::PathStrokeType(juce::jmax(1.5f, minDim * 0.12f * scale),
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded);
                juce::Path p;
                if (ch == U'╱' || ch == U'×')
                {
                    p.startNewSubPath(cellBounds.getX() + cellW * 0.22f, cellBounds.getBottom() - cellH * 0.18f);
                    p.lineTo(cellBounds.getRight() - cellW * 0.22f, cellBounds.getY() + cellH * 0.18f);
                }
                if (ch == U'╲' || ch == U'×')
                {
                    p.startNewSubPath(cellBounds.getX() + cellW * 0.22f, cellBounds.getY() + cellH * 0.18f);
                    p.lineTo(cellBounds.getRight() - cellW * 0.22f, cellBounds.getBottom() - cellH * 0.18f);
                }
                if (ch == U'∧')
                {
                    p.startNewSubPath(cellBounds.getX() + cellW * 0.20f, cellBounds.getBottom() - cellH * 0.18f);
                    p.lineTo(cx, cellBounds.getY() + cellH * 0.18f);
                    p.lineTo(cellBounds.getRight() - cellW * 0.20f, cellBounds.getBottom() - cellH * 0.18f);
                }
                if (ch == U'∨')
                {
                    p.startNewSubPath(cellBounds.getX() + cellW * 0.20f, cellBounds.getY() + cellH * 0.18f);
                    p.lineTo(cx, cellBounds.getBottom() - cellH * 0.18f);
                    p.lineTo(cellBounds.getRight() - cellW * 0.20f, cellBounds.getY() + cellH * 0.18f);
                }
                if (ch == U'+')
                {
                    p.startNewSubPath(cx, cellBounds.getY() + cellH * 0.18f);
                    p.lineTo(cx, cellBounds.getBottom() - cellH * 0.18f);
                    p.startNewSubPath(cellBounds.getX() + cellW * 0.18f, cy);
                    p.lineTo(cellBounds.getRight() - cellW * 0.18f, cy);
                }
                g.strokePath(p, stroke);
                return true;
            }

            case U'⌒':
            case U'⌣':
            {
                auto stroke = juce::PathStrokeType(juce::jmax(1.4f, minDim * 0.10f * scale),
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded);
                juce::Path p;
                const float left = cellBounds.getX() + cellW * 0.16f;
                const float right = cellBounds.getRight() - cellW * 0.16f;

                if (ch == U'⌒')
                {
                    p.startNewSubPath(left, cy + cellH * 0.06f);
                    p.quadraticTo(cx, cellBounds.getY() + cellH * 0.14f,
                                  right, cy + cellH * 0.06f);
                }
                else
                {
                    p.startNewSubPath(left, cy - cellH * 0.06f);
                    p.quadraticTo(cx, cellBounds.getBottom() - cellH * 0.14f,
                                  right, cy - cellH * 0.06f);
                }

                g.strokePath(p, stroke);
                return true;
            }

            default:
                return false;
        }
    }

    static bool isBrushLetter(char32_t ch)
    {
        switch (ch)
        {
            case U'A':
            case U'C':
            case U'D':
            case U'G':
            case U'J':
            case U'M':
            case U'O':
            case U'P':
            case U'Q':
            case U'R':
            case U'S':
            case U'U':
            case U'V':
            case U'W':
            case U'X':
            case U'Y':
                return true;
            default:
                return false;
        }
    }

    static int brushLetterIndex(char32_t ch)
    {
        if (ch >= U'A' && ch <= U'Z')
            return static_cast<int>(ch - U'A');
        return -1;
    }

    static char32_t takeCappedLetter(std::initializer_list<char32_t> candidates,
                                     std::array<uint8_t, 26>& counts,
                                     char32_t fallback)
    {
        for (auto ch : candidates)
        {
            const int idx = brushLetterIndex(ch);
            if (idx >= 0 && counts[static_cast<size_t>(idx)] < 2)
            {
                ++counts[static_cast<size_t>(idx)];
                return ch;
            }
        }

        return fallback;
    }

    static char32_t chooseEdgeGlyph(float leftCoverage, float rightCoverage,
                                    float upCoverage, float downCoverage,
                                    float diagUL, float diagUR,
                                    float diagDL, float diagDR,
                                    int gx, int gy, int targetCols, int targetRows)
    {
        const bool openLeft = leftCoverage < 0.05f;
        const bool openRight = rightCoverage < 0.05f;
        const bool openUp = upCoverage < 0.05f;
        const bool openDown = downCoverage < 0.05f;
        const float nx = (static_cast<float>(gx) + 0.5f) / static_cast<float>(targetCols);
        const float ny = (static_cast<float>(gy) + 0.5f) / static_cast<float>(targetRows);
        juce::ignoreUnused(nx);

        if ((openUp || openDown) && !(openLeft || openRight))
            return openUp ? U'⌒' : U'⌣';

        if ((openLeft || openRight) && !(openUp || openDown))
            return U'│';

        if ((openUp && openLeft) || (openDown && openRight))
            return openUp ? U'∧' : (diagUL < diagDR ? U'╲' : U'⌣');

        if ((openUp && openRight) || (openDown && openLeft))
            return openUp ? U'∧' : (diagUR < diagDL ? U'╱' : U'⌣');

        if (((gx + gy) % 6) == 0)
            return gy < targetRows / 2 ? U'⌒' : U'⌣';

        return gy < targetRows / 2 ? U'⌒' : U'⌣';
    }

    static char32_t chooseInteriorGlyph(float coverage, bool accentLike,
                                        bool denseZone, bool faceZone,
                                        bool torsoBand, int gx, int gy)
    {
        if (accentLike)
            return denseZone ? U'■' : U'▪';

        if (faceZone)
        {
            switch ((gx + gy) % 6)
            {
                case 0: return coverage > 0.64f ? U'●' : U'•';
                case 1: return U'⌒';
                case 2: return U'•';
                case 3: return coverage > 0.52f ? U'⌣' : U'●';
                case 4: return U'●';
                default: return coverage > 0.50f ? U'•' : U'⌣';
            }
        }

        if (torsoBand)
        {
            switch ((gx + gy) % 5)
            {
                case 0: return U'●';
                case 1: return U'•';
                case 2: return U'⌣';
                case 3: return coverage > 0.56f ? U'⌣' : U'●';
                default: return U'•';
            }
        }

        if (denseZone)
        {
            switch ((gx * 3 + gy) % 8)
            {
                case 0: return U'●';
                case 1: return U'•';
                case 2: return U'•';
                case 3: return coverage > 0.56f ? U'⌣' : U'╱';
                case 4: return U'⌣';
                case 5: return coverage > 0.56f ? U'⌒' : U'•';
                case 6: return U'•';
                case 7: return U'⌒';
                default: return U'•';
            }
        }

        if (coverage > 0.58f)
            return ((gx + gy) & 1) == 0 ? U'●' : U'•';
        if (coverage > 0.34f)
        {
            switch ((gx + gy) % 4)
            {
                case 0: return U'•';
                case 1: return U'╱';
                case 2: return U'╲';
                default: return U'●';
            }
        }

        return ((gx + gy) & 1) == 0 ? U'•' : U'⌒';
    }

    static float radialMass(float nx, float ny, float cx, float cy, float rx, float ry)
    {
        const float dx = (nx - cx) / juce::jmax(0.0001f, rx);
        const float dy = (ny - cy) / juce::jmax(0.0001f, ry);
        const float dist = std::sqrt(dx * dx + dy * dy);
        return juce::jlimit(0.0f, 1.0f, 1.0f - dist);
    }

    struct CharacterGuide
    {
        float face = 0.0f;
        float jaw = 0.0f;
        float ear = 0.0f;
        float foot = 0.0f;
        float body = 0.0f;
        float core = 0.0f;
        float silhouette = 0.0f;
    };

    static CharacterGuide computeCharacterGuide(SkinType skin, float nx, float ny)
    {
        CharacterGuide guide;

        if (skin == SkinType::Guoba)
        {
            float halfWidth = 0.0f;
            float verticalEnvelope = 0.0f;
            if (ny >= 0.28f && ny <= 0.84f)
            {
                if (ny < 0.44f)
                    halfWidth = juce::jmap(ny, 0.28f, 0.44f, 0.15f, 0.23f);
                else if (ny < 0.60f)
                    halfWidth = juce::jmap(ny, 0.44f, 0.60f, 0.23f, 0.31f);
                else
                    halfWidth = juce::jmap(ny, 0.60f, 0.84f, 0.31f, 0.19f);

                verticalEnvelope = ny < 0.60f
                    ? juce::jmap(ny, 0.28f, 0.60f, 0.55f, 1.0f)
                    : juce::jmap(ny, 0.60f, 0.84f, 1.0f, 0.58f);
            }

            const float bodyBand = halfWidth > 0.0f
                ? juce::jlimit(0.0f, 1.0f,
                               (1.0f - std::abs(nx - 0.5f) / halfWidth) * verticalEnvelope)
                : 0.0f;
            const float crown = radialMass(nx, ny, 0.50f, 0.44f, 0.18f, 0.16f);
            const float cheekL = radialMass(nx, ny, 0.37f, 0.58f, 0.15f, 0.17f);
            const float cheekR = radialMass(nx, ny, 0.63f, 0.58f, 0.15f, 0.17f);
            const float muzzle = radialMass(nx, ny, 0.50f, 0.63f, 0.20f, 0.15f);
            guide.face = juce::jmax(bodyBand, juce::jmax(crown, juce::jmax(muzzle, juce::jmax(cheekL, cheekR))));
            guide.jaw = radialMass(nx, ny, 0.50f, 0.80f, 0.20f, 0.11f);
            guide.ear = juce::jmax(radialMass(nx, ny, 0.31f, 0.17f, 0.050f, 0.080f),
                                   radialMass(nx, ny, 0.69f, 0.17f, 0.050f, 0.080f));
            guide.foot = juce::jmax(radialMass(nx, ny, 0.39f, 0.92f, 0.065f, 0.040f),
                                    radialMass(nx, ny, 0.61f, 0.92f, 0.065f, 0.040f));
            guide.body = juce::jmax(guide.face, guide.jaw * 0.96f);
            guide.core = juce::jmax(crown, muzzle * 0.92f);
            guide.silhouette = juce::jmax(guide.body, juce::jmax(guide.ear * 0.84f, guide.foot * 0.82f));
        }
        else
        {
            const float crown = radialMass(nx, ny, 0.50f, 0.47f, 0.21f, 0.20f);
            const float cheekL = radialMass(nx, ny, 0.38f, 0.55f, 0.15f, 0.18f);
            const float cheekR = radialMass(nx, ny, 0.62f, 0.55f, 0.15f, 0.18f);
            const float chin = radialMass(nx, ny, 0.50f, 0.68f, 0.20f, 0.14f);
            guide.face = juce::jmax(crown, juce::jmax(chin, juce::jmax(cheekL, cheekR)));
            guide.jaw = radialMass(nx, ny, 0.50f, 0.70f, 0.18f, 0.12f);
            guide.ear = juce::jmax(radialMass(nx, ny, 0.31f, 0.17f, 0.045f, 0.075f),
                                   radialMass(nx, ny, 0.69f, 0.17f, 0.045f, 0.075f));
            guide.foot = juce::jmax(radialMass(nx, ny, 0.40f, 0.90f, 0.055f, 0.036f),
                                    radialMass(nx, ny, 0.60f, 0.90f, 0.055f, 0.036f));
            guide.body = juce::jmax(guide.face, guide.jaw * 0.92f);
            guide.core = juce::jmax(crown, chin * 0.84f);
            guide.silhouette = juce::jmax(guide.body, juce::jmax(guide.ear * 0.80f, guide.foot * 0.76f));
        }

        return guide;
    }

    static char32_t enrichGlyphWithLetterBrush(char32_t glyph, bool edge,
                                               bool faceZone, bool torsoBand,
                                               bool denseZone,
                                               int gx, int gy,
                                               int targetCols, int targetRows,
                                               std::array<uint8_t, 26>& counts)
    {
        const float nx = (static_cast<float>(gx) + 0.5f) / static_cast<float>(targetCols);
        const float ny = (static_cast<float>(gy) + 0.5f) / static_cast<float>(targetRows);
        const bool upperCornerBand = ny < 0.28f && (nx < 0.32f || nx > 0.68f);
        const bool lowerCornerBand = ny > 0.72f && (nx < 0.34f || nx > 0.66f);
        const bool leftCheekBand = nx < 0.30f && ny > 0.28f && ny < 0.70f;
        const bool rightCheekBand = nx > 0.70f && ny > 0.28f && ny < 0.70f;

        if (edge)
        {
            if (upperCornerBand && ((gx + gy) % 7 == 0))
                return takeCappedLetter({ U'A', U'M', U'W', U'Y' }, counts, glyph);
            if (lowerCornerBand && ((gx + 2 * gy) % 9 == 2))
                return takeCappedLetter({ U'V', U'W', U'Y', U'U' }, counts, glyph);
            if (leftCheekBand && ((gx + gy) % 5 == 0))
                return takeCappedLetter({ U'C', U'G', U'J', U'S' }, counts, glyph);
            if (rightCheekBand && ((gx + gy) % 5 == 0))
                return takeCappedLetter({ U'D', U'Q', U'R', U'U' }, counts, glyph);
        }

        if (faceZone && ((gx + 2 * gy) % 17 == 4) && ny > 0.42f && ny < 0.62f)
            return takeCappedLetter({ U'O', U'Q', U'C', U'D' }, counts, glyph);

        if (torsoBand && ((gx * 2 + gy) % 19 == 7))
            return takeCappedLetter({ U'R', U'S', U'U', U'P' }, counts, glyph);

        if (denseZone && ((gx + gy) % 21 == 6) && ny > 0.60f)
            return takeCappedLetter({ U'V', U'W', U'X', U'Y' }, counts, glyph);

        return glyph;
    }

    static uint8_t chooseGlyphSizeLevel(char32_t glyph, float coverage, bool edge,
                                        bool accentLike, bool denseZone,
                                        bool faceZone, bool torsoBand,
                                        float faceMass, float jawMass, float earMass,
                                        int gx, int gy)
    {
        juce::ignoreUnused(gx, gy);
        const float bodyMass = juce::jmax(faceMass, jawMass);
        uint8_t level = 0;

        if (accentLike)
            level = 2;
        else if (edge)
            level = bodyMass > 0.74f ? 1 : 0;
        else if (bodyMass > 0.88f && coverage > 0.70f)
            level = 2;
        else if (bodyMass > 0.68f)
            level = 1;
        else if (bodyMass > 0.48f)
            level = 1;
        else if (denseZone)
            level = coverage > 0.46f ? 1 : 0;
        else if (faceZone)
            level = bodyMass > 0.60f ? 1 : 0;
        else if (torsoBand)
            level = bodyMass > 0.58f ? 1 : 0;
        else
            level = coverage > 0.58f ? 1 : 0;

        if (jawMass > 0.68f)
            level = juce::jmax<uint8_t>(level, 1);

        if (earMass > 0.16f)
            level = juce::jmin<uint8_t>(level, static_cast<uint8_t>(0));
        else if (earMass > 0.10f)
            level = juce::jmin<uint8_t>(level, static_cast<uint8_t>(1));

        if (glyph == U'•')
            level = static_cast<uint8_t>(level > 0 ? level - 1 : 0);
        else if (glyph == U'●')
            level = juce::jmin<uint8_t>(3, static_cast<uint8_t>(level + 0));
        else if (glyph == U'▪')
            level = juce::jmin<uint8_t>(3, static_cast<uint8_t>(level + 0));
        else if (glyph == U'■')
            level = juce::jmin<uint8_t>(2, static_cast<uint8_t>(level + 0));
        else if (isBrushLetter(glyph))
            level = juce::jmin<uint8_t>(2, static_cast<uint8_t>(level + (edge ? 0 : 0)));
        else if (glyph == U'⌒' || glyph == U'⌣' || glyph == U'∧' || glyph == U'∨')
            level = juce::jmin<uint8_t>(3, static_cast<uint8_t>(level + 0));

        return juce::jlimit<uint8_t>(0, 3, level);
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
        static constexpr float sizeScales[] = { 0.32f, 0.54f, 0.78f, 1.04f };
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
                const auto cellBounds = juce::Rectangle<float>(px, py, cellW, cellH);

                const char32_t ch = cell.symbol;
                if (isPrimitiveGlyph(ch))
                {
                    drawPrimitiveGlyph(g, cellBounds, ch, drawColour, scale);
                }
                else if (ch >= 0x2800u && ch <= 0x28FFu)
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
            stamp(9, 6, U'▪', eyeColour, 0.98f, 3);
            stamp(14, 6, U'▪', eyeColour, 0.98f, 3);
            stamp(12, 8, U'•', hoodColour, 0.64f, 1);
            stamp(10, 13, U'─', hoodColour.withAlpha(0.86f), 0.72f, 1);
            stamp(12, 13, U'─', hoodColour.withAlpha(0.86f), 0.72f, 1);
            stamp(14, 13, U'─', hoodColour.withAlpha(0.86f), 0.72f, 1);
            stamp(8, 14, U'⌣', hoodColour.withAlpha(0.80f), 0.70f, 1);
            stamp(10, 15, U'⌣', hoodColour.withAlpha(0.88f), 0.76f, 1);
            stamp(12, 15, U'⌣', hoodColour.withAlpha(0.88f), 0.76f, 1);
            stamp(14, 14, U'⌣', hoodColour.withAlpha(0.80f), 0.70f, 1);
        }
        else
        {
            const auto eyeColour = isDarkTheme ? juce::Colour(0xFF79F6F0) : juce::Colour(0xFF0A8C95);
            const auto edgeColour = isDarkTheme ? juce::Colour(0xFFD9EFFF) : juce::Colour(0xFF41659C);
            stamp(9, 6, U'▪', eyeColour, 0.98f, 3);
            stamp(14, 6, U'▪', eyeColour, 0.98f, 3);
            stamp(8, 2, U'─', edgeColour.withAlpha(0.92f), 0.76f, 1);
            stamp(15, 2, U'─', edgeColour.withAlpha(0.92f), 0.76f, 1);
            stamp(7, 8, U'•', eyeColour.withAlpha(0.72f), 0.58f, 0);
            stamp(16, 8, U'•', eyeColour.withAlpha(0.72f), 0.58f, 0);
        }
    }

    bool drawGuidedProceduralGuoba()
    {
        if (currentSkin != SkinType::Guoba)
            return false;

        const int targetCols = 22;
        const int targetRows = 17;
        const int startX = (canvasCols - targetCols) / 2;
        const int startY = 5;
        explicitCharacterBounds = {};

        const auto neutralColour = isDarkTheme ? juce::Colour(0xFFEDE5DA) : juce::Colour(0xFF343B45);
        const auto edgeColour = isDarkTheme ? juce::Colour(0xFFFFF8EE) : juce::Colour(0xFF20262E);
        const auto accentColour = isDarkTheme ? juce::Colour(0xFFFFD35A) : juce::Colour(0xFFB27A18);
        auto stamp = [this, startX, startY, targetCols, targetRows]
                     (int x, int y, char32_t glyph, juce::Colour colour,
                      float brightness, uint8_t sizeLevel)
        {
            if (x < 0 || x >= targetCols || y < 0 || y >= targetRows)
                return;
            canvas->setCell(startX + x, startY + y, glyph, colour, 1, brightness, sizeLevel);
        };

        auto placeBody = [&](int x, int y, int inset)
        {
            char32_t glyph = inset >= 3 ? U'■' : (inset == 2 ? U'■' : U'▪');
            uint8_t sizeLevel = inset >= 3 ? 3 : (inset == 2 ? 2 : 1);
            auto colour = neutralColour;
            if (inset >= 3)
                colour = colour.interpolatedWith(edgeColour, isDarkTheme ? 0.08f : 0.04f);
            stamp(x, y, glyph, colour, isDarkTheme ? 0.94f : 0.92f, sizeLevel);
        };

        const int bodyLeft = 5;
        const int bodyRight = 14;
        const int bodyTop = 2;
        const int bodyBottom = 11;
        for (int y = bodyTop; y <= bodyBottom; ++y)
        {
            for (int x = bodyLeft; x <= bodyRight; ++x)
            {
                const int inset = juce::jmin(juce::jmin(x - bodyLeft, bodyRight - x),
                                             juce::jmin(y - bodyTop, bodyBottom - y));
                placeBody(x, y, inset);
            }
        }

        // Head top and ears
        stamp(5, 0, U'─', juce::Colour(0xFF8DB5EA), 0.88f, 1);
        stamp(14, 0, U'─', juce::Colour(0xFF8DB5EA), 0.88f, 1);
        stamp(2, 1, U'╲', edgeColour, 0.86f, 1);
        stamp(3, 1, U'∧', edgeColour, 0.86f, 1);
        stamp(4, 1, U'╱', edgeColour, 0.86f, 1);
        stamp(15, 1, U'∧', edgeColour, 0.86f, 1);
        stamp(16, 1, U'╲', edgeColour, 0.86f, 1);
        stamp(17, 1, U'╱', edgeColour, 0.86f, 1);
        stamp(6, 1, U'⌒', edgeColour, 0.80f, 1);
        stamp(7, 1, U'⌒', edgeColour, 0.80f, 1);
        stamp(8, 1, U'⌒', edgeColour, 0.80f, 1);
        stamp(9, 1, U'⌒', edgeColour, 0.80f, 1);
        stamp(10, 1, U'⌒', edgeColour, 0.80f, 1);

        // Outer sides
        for (int y = 3; y <= 9; ++y)
        {
            stamp(4, y, U'│', edgeColour, 0.82f, 1);
            stamp(15, y, U'│', edgeColour, 0.82f, 1);
        }

        // Eyes
        stamp(7, 4, U'▪', juce::Colour(0xFF3CCED8), 1.0f, 2);
        stamp(12, 4, U'▪', juce::Colour(0xFF3CCED8), 1.0f, 2);

        // Lower jaw and feet
        for (int x = 6; x <= 11; ++x)
            stamp(x, 12, U'⌣', edgeColour, 0.74f, 1);
        stamp(2, 11, U'╲', edgeColour, 0.80f, 1);
        stamp(3, 11, U'V', edgeColour, 0.86f, 1);
        stamp(4, 11, U'V', edgeColour, 0.86f, 1);
        stamp(15, 11, U'V', edgeColour, 0.86f, 1);
        stamp(16, 11, U'╱', edgeColour, 0.80f, 1);
        stamp(4, 13, U'∧', edgeColour, 0.78f, 0);
        stamp(5, 13, U'∧', edgeColour, 0.78f, 0);
        stamp(14, 13, U'∧', edgeColour, 0.78f, 0);
        stamp(15, 13, U'∧', edgeColour, 0.78f, 0);

        // Right-side capsule
        stamp(18, 2, U'╲', edgeColour, 0.80f, 1);
        stamp(19, 2, U'⌒', edgeColour, 0.80f, 1);
        stamp(20, 2, U'╱', edgeColour, 0.80f, 1);
        for (int y = 3; y <= 6; ++y)
        {
            stamp(18, y, U'│', edgeColour, 0.78f, 1);
            stamp(20, y, U'│', edgeColour, 0.78f, 1);
        }
        stamp(19, 3, U'■', neutralColour, isDarkTheme ? 0.92f : 0.90f, 2);
        stamp(19, 4, U'■', neutralColour, isDarkTheme ? 0.92f : 0.90f, 2);
        stamp(19, 5, U'▪', neutralColour, isDarkTheme ? 0.88f : 0.86f, 1);
        stamp(18, 7, U'∨', edgeColour, 0.78f, 0);
        stamp(19, 7, U'⌣', edgeColour, 0.78f, 0);
        stamp(20, 7, U'∧', edgeColour, 0.78f, 0);

        explicitCharacterBounds = juce::Rectangle<int>(startX + 1, startY, 20, 15);
        return true;
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
        std::array<uint8_t, 26> letterUseCounts {};

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

                auto colour = remapBrailleColour(cell.average, cell.luminance, cell.saturation,
                                                 edge, cell.accentLike, centerBias);
                if (edge)
                    colour = colour.interpolatedWith(outlineLight, isDarkTheme ? 0.20f : 0.12f);

                const float nx = (static_cast<float>(gx) + 0.5f) / static_cast<float>(targetCols);
                const float ny = (static_cast<float>(gy) + 0.5f) / static_cast<float>(targetRows);
                const auto guide = computeCharacterGuide(currentSkin, nx, ny);
                const float faceMass = guide.face;
                const float jawMass = guide.jaw;
                const float earMass = guide.ear;
                const float footMass = guide.foot;
                const float bodyMass = guide.body;
                const float coreMass = guide.core;
                const float silhouette = guide.silhouette;
                const bool faceZone = faceMass > 0.16f;
                const bool torsoBand = jawMass > 0.10f || (ny > 0.56f && bodyMass > 0.34f);
                const bool denseZone = coreMass > 0.32f || jawMass > 0.18f;
                if (silhouette < 0.10f && cell.coverage < 0.40f)
                    continue;
                if (bodyMass < 0.16f && cell.coverage < 0.58f && footMass < 0.12f)
                    continue;
                const float sourceWeight = currentSkin == SkinType::Guoba ? 0.38f : 0.50f;
                const float guideWeight = currentSkin == SkinType::Guoba ? 0.66f : 0.54f;
                const float shapedCoverage = juce::jlimit(0.0f, 1.0f,
                                                          cell.coverage * sourceWeight
                                                          + silhouette * guideWeight
                                                          + coreMass * 0.16f
                                                          + jawMass * 0.12f
                                                          + footMass * 0.12f
                                                          - earMass * 0.04f);
                if (shapedCoverage < 0.22f)
                    continue;
                const char32_t glyph = edge
                    ? chooseEdgeGlyph(getCoverage(gx - 1, gy), getCoverage(gx + 1, gy),
                                      getCoverage(gx, gy - 1), getCoverage(gx, gy + 1),
                                      getCoverage(gx - 1, gy - 1), getCoverage(gx + 1, gy - 1),
                                      getCoverage(gx - 1, gy + 1), getCoverage(gx + 1, gy + 1),
                                      gx, gy, targetCols, targetRows)
                    : chooseInteriorGlyph(shapedCoverage, cell.accentLike,
                                          denseZone, faceZone, torsoBand, gx, gy);
                const char32_t finalGlyph = enrichGlyphWithLetterBrush(glyph, edge, faceZone, torsoBand,
                                                                       denseZone, gx, gy,
                                                                       targetCols, targetRows,
                                                                       letterUseCounts);
                const float pulse = 0.5f + 0.5f * std::sin(animationPhase * 3.2f + gx * 0.21f - gy * 0.17f);
                const float energy = juce::jlimit(0.0f, 1.0f,
                                                  shapedCoverage * 0.60f
                                                  + centerBias * 0.24f
                                                  + (edge ? 0.22f : 0.0f)
                                                  + (cell.accentLike ? 0.12f : 0.0f));
                float brightness = juce::jlimit(isDarkTheme ? 0.56f : 0.74f, 1.0f,
                                                (isDarkTheme ? 0.66f : 0.80f)
                                                + shapedCoverage * (isDarkTheme ? 0.30f : 0.22f)
                                                + centerBias * 0.08f
                                                + jawMass * 0.04f
                                                + footMass * 0.03f
                                                - earMass * 0.04f
                                                + (edge ? 0.10f : 0.0f)
                                                + pulse * (cell.accentLike ? 0.06f : 0.03f));
                juce::ignoreUnused(energy);
                const uint8_t sizeLevel = chooseGlyphSizeLevel(finalGlyph, shapedCoverage, edge,
                                                               cell.accentLike, denseZone,
                                                               faceZone, torsoBand,
                                                               faceMass, jawMass, earMass,
                                                               gx, gy);

                canvas->setCell(startX + gx, startY + gy, finalGlyph, colour, 1, brightness, sizeLevel);

                if (edge && cell.coverage > 0.18f)
                {
                    const float haloAlpha = isDarkTheme ? 0.10f : 0.07f;
                    if (gx > 0 && getCoverage(gx - 1, gy) < 0.025f)
                        canvas->setCell(startX + gx - 1, startY + gy, U'•', colour.withAlpha(haloAlpha), 0, haloAlpha, 0);
                    if (gx < targetCols - 1 && getCoverage(gx + 1, gy) < 0.025f)
                        canvas->setCell(startX + gx + 1, startY + gy, U'•', colour.withAlpha(haloAlpha), 0, haloAlpha, 0);
                }
            }
        }

        drawImageDrivenAccentAnchors(startX, startY, targetCols, targetRows);
        return true;
    }

    void drawCharacter()
    {
        if (drawGuidedProceduralGuoba())
            return;

        if (!sourceSpriteImage.isNull() && drawImageDrivenCharacter())
            return;

        static const char32_t* guobaPixels[14] = {
            U"....................",
            U".......#....#.......",
            U"......###..###......",
            U".....##########.....",
            U"....###@####@###....",
            U"...##############...",
            U"...##############...",
            U"...##############...",
            U"...##############...",
            U"....############....",
            U"....############....",
            U".....##########.....",
            U"......########......",
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
        std::array<uint8_t, 26> letterUseCounts {};

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
                const bool faceZone = (gy >= 4 && gy <= 11 && gx >= 5 && gx <= 18);
                const float nx = (static_cast<float>(gx) + 0.5f) / static_cast<float>(targetCols);
                const float ny = (static_cast<float>(gy) + 0.5f) / static_cast<float>(targetRows);
                const auto guide = computeCharacterGuide(currentSkin, nx, ny);
                const float faceMass = guide.face;
                const float jawMass = guide.jaw;
                const float earMass = guide.ear;
                const float footMass = guide.foot;
                const float bodyMass = guide.body;
                const float coreMass = guide.core;
                const float silhouette = guide.silhouette;
                const bool denseZone = coreMass > 0.30f || jawMass > 0.18f || torsoBand;
                const bool dynamicFaceZone = faceZone || faceMass > 0.16f;
                const bool dynamicTorsoBand = torsoBand || (jawMass > 0.10f && ny > 0.54f);
                if (silhouette < 0.10f && cell.coverage < 0.42f)
                    continue;
                if (bodyMass < 0.16f && cell.coverage < 0.58f && footMass < 0.12f)
                    continue;
                const float sourceWeight = currentSkin == SkinType::Guoba ? 0.34f : 0.46f;
                const float guideWeight = currentSkin == SkinType::Guoba ? 0.72f : 0.58f;
                const float shapedCoverage = juce::jlimit(0.0f, 1.0f,
                                                          cell.coverage * sourceWeight
                                                          + silhouette * guideWeight
                                                          + coreMass * 0.18f
                                                          + jawMass * 0.14f
                                                          + footMass * 0.14f
                                                          - earMass * 0.04f);
                if (shapedCoverage < 0.20f)
                    continue;

                auto colour = neutralColour;
                if (dynamicTorsoBand)
                    colour = colour.interpolatedWith(accentColour, currentSkin == SkinType::Guoba ? 0.10f : 0.08f);
                if (dynamicFaceZone && !edge)
                    colour = colour.interpolatedWith(accentSecondary, currentSkin == SkinType::Guoba ? 0.06f : 0.10f);
                if (edge)
                    colour = colour.interpolatedWith(edgeColour, isDarkTheme ? 0.52f : 0.28f);

                const char32_t glyph = edge
                    ? chooseEdgeGlyph(coverageAt(gx - 1, gy), coverageAt(gx + 1, gy),
                                      coverageAt(gx, gy - 1), coverageAt(gx, gy + 1),
                                      coverageAt(gx - 1, gy - 1), coverageAt(gx + 1, gy - 1),
                                      coverageAt(gx - 1, gy + 1), coverageAt(gx + 1, gy + 1),
                                      gx, gy, targetCols, targetRows)
                    : chooseInteriorGlyph(shapedCoverage, false,
                                          denseZone, dynamicFaceZone, dynamicTorsoBand, gx, gy);
                const char32_t finalGlyph = enrichGlyphWithLetterBrush(glyph, edge, dynamicFaceZone, dynamicTorsoBand,
                                                                       denseZone, gx, gy,
                                                                       targetCols, targetRows,
                                                                       letterUseCounts);
                const float pulse = 0.5f + 0.5f * std::sin(animationPhase * 2.6f + gx * 0.24f - gy * 0.17f);
                float brightness = isDarkTheme
                    ? juce::jlimit(0.78f, 1.0f, 0.80f + shapedCoverage * 0.24f + centerBias * 0.08f + jawMass * 0.04f + footMass * 0.03f + pulse * 0.05f - earMass * 0.05f)
                    : juce::jlimit(0.90f, 1.0f, 0.92f + shapedCoverage * 0.10f + centerBias * 0.04f + jawMass * 0.04f + footMass * 0.03f + pulse * 0.03f - earMass * 0.05f);
                const uint8_t sizeLevel = chooseGlyphSizeLevel(finalGlyph, shapedCoverage, edge,
                                                               false, denseZone,
                                                               dynamicFaceZone, dynamicTorsoBand,
                                                               faceMass, jawMass, earMass,
                                                               gx, gy);
                stamp(gx, gy, finalGlyph, colour, brightness, sizeLevel);
            }
        }

        if (currentSkin == SkinType::Guoba)
        {
            stamp(9, 5, U'▪', accentColour, 1.0f, 3);
            stamp(14, 5, U'▪', accentColour, 1.0f, 3);
            stamp(11, 7, U'⌣', edgeColour, isDarkTheme ? 0.88f : 0.80f, 1);
            stamp(10, 11, U'∧', accentColour.interpolatedWith(accentSecondary, 0.18f), 0.90f, 1);
            stamp(11, 11, U'▪', accentColour, 0.98f, 2);
            stamp(12, 11, U'▪', accentColour, 0.98f, 2);
            stamp(13, 11, U'∨', accentColour.interpolatedWith(accentSecondary, 0.18f), 0.90f, 1);
        }
        else
        {
            stamp(9, 5, U'▪', accentColour, 1.0f, 3);
            stamp(14, 5, U'▪', accentColour, 1.0f, 3);
            stamp(6, 6, U'•', accentSecondary, isDarkTheme ? 0.82f : 0.74f, 0);
            stamp(17, 6, U'•', accentSecondary, isDarkTheme ? 0.82f : 0.74f, 0);
            stamp(7, 2, U'⌒', accentColour, isDarkTheme ? 0.74f : 0.70f, 1);
            stamp(14, 2, U'⌒', accentColour, isDarkTheme ? 0.74f : 0.70f, 1);
            stamp(10, 10, U'⌣', edgeColour, isDarkTheme ? 0.82f : 0.76f, 1);
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
