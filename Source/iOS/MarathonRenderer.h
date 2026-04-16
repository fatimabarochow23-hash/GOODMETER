/*
  ==============================================================================
    MarathonRenderer.h
    GOODMETER iOS - Marathon Art Style Dot Matrix Rendering Engine

    Core components:
    - DotMatrixCanvas: Grid-based character canvas
    - ShapeRenderer: Bresenham line/circle algorithms for abstract shapes
    - SymbolDensityMapper: Density-based symbol selection
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
// Single dot matrix cell
struct DotCell
{
    char32_t symbol = U'.';
    juce::Colour color = juce::Colours::white;
    float brightness = 1.0f;
    uint8_t layer = 0;  // 0=background, 1=character, 2=effects
    uint8_t sizeLevel = 1; // 0=tiny, 1=small, 2=medium, 3=large
};

//==============================================================================
// Dot matrix canvas - grid of Unicode characters
class DotMatrixCanvas
{
public:
    DotMatrixCanvas(int cols, int rows)
        : gridWidth(cols), gridHeight(rows)
    {
        cells.resize(gridWidth * gridHeight);
        clear();
    }

    void clear(char32_t bgSymbol = U'.')
    {
        for (auto& cell : cells)
        {
            cell.symbol = bgSymbol;
            cell.color = juce::Colours::white;
            cell.brightness = 0.6f;
            cell.layer = 0;
            cell.sizeLevel = 1;
        }
        dirtyRegion = juce::Rectangle<int>(0, 0, gridWidth, gridHeight);
    }

    void setCell(int x, int y, char32_t symbol, juce::Colour color, uint8_t layer = 1,
                 float brightness = 1.0f, uint8_t sizeLevel = 1)
    {
        if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return;

        int idx = y * gridWidth + x;
        cells[idx].symbol = symbol;
        cells[idx].color = color;
        cells[idx].brightness = brightness;
        cells[idx].layer = layer;
        cells[idx].sizeLevel = juce::jlimit<uint8_t>(0, 3, sizeLevel);

        dirtyRegion = dirtyRegion.getUnion(juce::Rectangle<int>(x, y, 1, 1));
    }

    DotCell getCell(int x, int y) const
    {
        if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight)
            return DotCell{};
        return cells[y * gridWidth + x];
    }

    void drawToGraphics(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Font font)
    {
        static constexpr float sizeScales[] = { 0.58f, 0.82f, 1.02f, 1.24f };
        float cellW = bounds.getWidth() / gridWidth;
        float cellH = bounds.getHeight() / gridHeight;

        for (int y = 0; y < gridHeight; ++y)
        {
            for (int x = 0; x < gridWidth; ++x)
            {
                const auto& cell = cells[y * gridWidth + x];
                if (cell.symbol == U' ' || cell.brightness <= 0.0f)
                    continue;

                juce::Colour drawColor = cell.color.withMultipliedAlpha(cell.brightness);
                g.setColour(drawColor);

                float px = bounds.getX() + x * cellW;
                float py = bounds.getY() + y * cellH;
                const float scale = sizeScales[juce::jlimit<int>(0, 3, static_cast<int>(cell.sizeLevel))];
                const float drawW = cellW * juce::jmin(1.34f, scale + 0.16f);
                const float drawH = cellH * juce::jmin(1.34f, scale + 0.12f);
                juce::Rectangle<float> drawBounds(px + (cellW - drawW) * 0.5f,
                                                  py + (cellH - drawH) * 0.5f,
                                                  drawW, drawH);

                juce::String str = juce::String::charToString(cell.symbol);
                g.setFont(font.withHeight(font.getHeight() * scale));
                g.drawText(str, drawBounds.toNearestInt(), juce::Justification::centred, false);
            }
        }

        dirtyRegion = juce::Rectangle<int>();
    }

    int getWidth() const { return gridWidth; }
    int getHeight() const { return gridHeight; }

private:
    int gridWidth, gridHeight;
    std::vector<DotCell> cells;
    juce::Rectangle<int> dirtyRegion;
};

//==============================================================================
// Symbol density mapper - select symbol based on density/brightness
class SymbolDensityMapper
{
public:
    static char32_t getSymbolForDensity(float density)
    {
        static const char32_t symbols[] = {
            U' ',   // 0.0 - 0.1: blank
            U'.',   // 0.1 - 0.2: dot
            U'·',   // 0.2 - 0.3: middle dot
            U'×',   // 0.3 - 0.5: cross
            U'+',   // 0.5 - 0.7: plus
            U'▪',   // 0.7 - 0.9: small square
            U'●'    // 0.9 - 1.0: filled circle
        };
        int idx = juce::jlimit(0, 6, (int)(density * 7.0f));
        return symbols[idx];
    }
};

//==============================================================================
// Shape renderer - Bresenham algorithms for drawing abstract shapes
class ShapeRenderer
{
public:
    ShapeRenderer(DotMatrixCanvas& canvas) : canvas(canvas) {}

    void drawLine(int x0, int y0, int x1, int y1, char32_t symbol, juce::Colour color)
    {
        int dx = std::abs(x1 - x0);
        int dy = std::abs(y1 - y0);
        int sx = (x0 < x1) ? 1 : -1;
        int sy = (y0 < y1) ? 1 : -1;
        int err = dx - dy;

        while (true)
        {
            // Auto-select symbol based on line direction
            char32_t drawSymbol = symbol;
            if (symbol == U'+')  // Auto mode
            {
                if (dx > dy * 2)
                    drawSymbol = U'+';  // Horizontal
                else if (dy > dx * 2)
                    drawSymbol = U'+';  // Vertical
                else if ((sx > 0 && sy > 0) || (sx < 0 && sy < 0))
                    drawSymbol = U'\\';  // Diagonal \
                else
                    drawSymbol = U'/';   // Diagonal /
            }

            canvas.setCell(x0, y0, drawSymbol, color);
            if (x0 == x1 && y0 == y1) break;

            int e2 = 2 * err;
            if (e2 > -dy) { err -= dy; x0 += sx; }
            if (e2 < dx)  { err += dx; y0 += sy; }
        }
    }

    void drawCircle(int cx, int cy, int radius, char32_t symbol, juce::Colour color, bool filled = false)
    {
        int x = 0, y = radius;
        int d = 1 - radius;

        while (x <= y)
        {
            if (filled)
            {
                for (int i = -x; i <= x; ++i)
                {
                    canvas.setCell(cx + i, cy + y, symbol, color);
                    canvas.setCell(cx + i, cy - y, symbol, color);
                }
                for (int i = -y; i <= y; ++i)
                {
                    canvas.setCell(cx + i, cy + x, symbol, color);
                    canvas.setCell(cx + i, cy - x, symbol, color);
                }
            }
            else
            {
                canvas.setCell(cx + x, cy + y, symbol, color);
                canvas.setCell(cx - x, cy + y, symbol, color);
                canvas.setCell(cx + x, cy - y, symbol, color);
                canvas.setCell(cx - x, cy - y, symbol, color);
                canvas.setCell(cx + y, cy + x, symbol, color);
                canvas.setCell(cx - y, cy + x, symbol, color);
                canvas.setCell(cx + y, cy - x, symbol, color);
                canvas.setCell(cx - y, cy - x, symbol, color);
            }

            if (d < 0)
                d += 2 * x + 3;
            else
            {
                d += 2 * (x - y) + 5;
                --y;
            }
            ++x;
        }
    }

    void drawEllipse(int cx, int cy, int rx, int ry, char32_t symbol, juce::Colour color, bool filled = false)
    {
        if (rx == ry)
        {
            drawCircle(cx, cy, rx, symbol, color, filled);
            return;
        }

        int rxSq = rx * rx;
        int rySq = ry * ry;
        int x = 0, y = ry;
        int px = 0, py = 2 * rxSq * y;

        auto plot4 = [&](int px, int py) {
            canvas.setCell(cx + px, cy + py, symbol, color);
            canvas.setCell(cx - px, cy + py, symbol, color);
            canvas.setCell(cx + px, cy - py, symbol, color);
            canvas.setCell(cx - px, cy - py, symbol, color);
        };

        // Region 1
        int p = (int)(rySq - (rxSq * ry) + (0.25 * rxSq));
        while (px < py)
        {
            if (filled)
            {
                for (int i = -x; i <= x; ++i)
                {
                    canvas.setCell(cx + i, cy + y, symbol, color);
                    canvas.setCell(cx + i, cy - y, symbol, color);
                }
            }
            else
            {
                plot4(x, y);
            }

            ++x;
            px += 2 * rySq;
            if (p < 0)
                p += rySq + px;
            else
            {
                --y;
                py -= 2 * rxSq;
                p += rySq + px - py;
            }
        }

        // Region 2
        p = (int)(rySq * (x + 0.5) * (x + 0.5) + rxSq * (y - 1) * (y - 1) - rxSq * rySq);
        while (y >= 0)
        {
            if (filled)
            {
                for (int i = -x; i <= x; ++i)
                {
                    canvas.setCell(cx + i, cy + y, symbol, color);
                    canvas.setCell(cx + i, cy - y, symbol, color);
                }
            }
            else
            {
                plot4(x, y);
            }

            --y;
            py -= 2 * rxSq;
            if (p > 0)
                p += rxSq - py;
            else
            {
                ++x;
                px += 2 * rySq;
                p += rxSq - py + px;
            }
        }
    }

private:
    DotMatrixCanvas& canvas;
};

namespace MarathonField
{
    enum class Preset
    {
        hero = 0,
        audio,
        settings,
        history,
        video
    };

    inline float normalizedCoord(int index, int size)
    {
        return size > 1 ? static_cast<float>(index) / static_cast<float>(size - 1) : 0.5f;
    }

    inline float gaussian(float dx, float dy)
    {
        return std::exp(-(dx * dx + dy * dy));
    }

    inline float visibilityForCell(int x, int y, int width, int height, Preset preset)
    {
        const float nx = normalizedCoord(x, width);
        const float ny = normalizedCoord(y, height);

        const float rowCadence = (y % 6 == 0) ? 1.00f
                               : (y % 6 == 1 || y % 6 == 4) ? 0.82f
                               : 0.60f;
        const float columnCadence = (x % 7 == 0) ? 1.00f
                                  : (x % 4 == 0) ? 0.84f
                                  : 0.66f;
        const float guideLift = (x % 7 == 0 || y % 6 == 0) ? 1.15f : 1.0f;
        const float edgeLift = 0.80f + 0.20f * juce::jlimit(0.0f, 1.0f, std::abs(ny - 0.5f) * 1.85f);

        float quietMask = 1.0f;

        switch (preset)
        {
            case Preset::hero:
            {
                const float dx = (nx - 0.5f) / 0.26f;
                const float dy = (ny - 0.52f) / 0.36f;
                quietMask = 1.0f - 0.76f * gaussian(dx, dy);
                break;
            }
            case Preset::audio:
            {
                const float dx = (nx - 0.5f) / 0.24f;
                const float dy = (ny - 0.56f) / 0.44f;
                quietMask = 1.0f - 0.70f * gaussian(dx, dy);

                const float transportDx = (nx - 0.5f) / 0.42f;
                const float transportDy = (ny - 0.95f) / 0.10f;
                quietMask *= (1.0f - 0.42f * gaussian(transportDx, transportDy));
                break;
            }
            case Preset::settings:
            {
                const float dx = (nx - 0.5f) / 0.34f;
                const float dy = (ny - 0.54f) / 0.42f;
                quietMask = 1.0f - 0.58f * gaussian(dx, dy);
                break;
            }
            case Preset::history:
            {
                const float dx = (nx - 0.52f) / 0.37f;
                const float dy = (ny - 0.58f) / 0.48f;
                quietMask = 1.0f - 0.54f * gaussian(dx, dy);
                break;
            }
            case Preset::video:
            {
                const float dx = (nx - 0.5f) / 0.30f;
                const float dy = (ny - 0.50f) / 0.32f;
                quietMask = 1.0f - 0.78f * gaussian(dx, dy);
                break;
            }
        }

        const float flow = 0.86f + 0.14f * std::abs(std::sin((nx * 9.0f + ny * 5.0f) * juce::MathConstants<float>::pi));
        return juce::jlimit(0.08f, 1.0f, rowCadence * columnCadence * guideLift * edgeLift * quietMask * flow);
    }

    inline float brightnessForCell(int x, int y, int width, int height, Preset preset)
    {
        const float visibility = visibilityForCell(x, y, width, height, preset);
        return juce::jlimit(0.05f, 0.28f, 0.055f + visibility * 0.18f);
    }

    inline bool shouldLeaveBlank(int x, int y, int width, int height, Preset preset)
    {
        const float visibility = visibilityForCell(x, y, width, height, preset);
        if (visibility < 0.24f)
            return ((x + y * 2) % 3) != 0;
        if (visibility < 0.38f)
            return ((x * 3 + y) % 5) == 0;
        return false;
    }
}
