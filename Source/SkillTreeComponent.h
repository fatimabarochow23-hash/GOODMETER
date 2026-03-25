/*
  ==============================================================================
    SkillTreeComponent.h
    GOODMETER - Configurable Skill Loadout System

    Provides an N-choose-5 skill slot system for Nono's hover buttons.
    Slot 0 (Gear/Settings) is permanently locked. Slots 1-4 are user-configurable
    via drag-and-drop in the settings dialog.

    Components:
      - SkillID enum + SkillInfo metadata
      - SkillTreeComponent: pentagon slot layout + pool grid, manual DnD
      - MoreSettingsContent: TabbedComponent wrapper (Audio + Skills tabs)

    Persistence: Skill loadout saved/loaded via PropertiesFile
    (~/Library/Application Support/GOODMETER.settings)
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "GoodMeterLookAndFeel.h"

//==============================================================================
// Skill ID — each represents a distinct hover button skill
//==============================================================================
enum class SkillID
{
    Gear         = 0,   // Settings menu (LOCKED to slot 0)
    Record       = 1,   // Live WAV recording
    Stow         = 2,   // Thanos snap stow
    Rewind       = 3,   // Retrospective 60s export
    VideoExtract = 4,   // Extract audio from video
    AudioLab     = 5,   // De-noise Lab (future)

    COUNT        = 6
};

//==============================================================================
// Metadata for each skill
//==============================================================================
struct SkillInfo
{
    SkillID id;
    juce::String name;
    juce::String shortName;     // 3-4 char abbreviation for fallback rendering
    juce::Colour glowColour;
    bool isLocked;              // true = cannot be removed from loadout (Gear only)
};

inline SkillInfo getSkillInfo(SkillID id)
{
    switch (id)
    {
        case SkillID::Gear:
            return { id, "Settings", "GEAR",
                     GoodMeterLookAndFeel::accentCyan, true };
        case SkillID::Record:
            return { id, "Record", "REC",
                     GoodMeterLookAndFeel::accentPink, false };
        case SkillID::Stow:
            return { id, "Stow", "STOW",
                     GoodMeterLookAndFeel::accentPurple, false };
        case SkillID::Rewind:
            return { id, "Rewind", "REW",
                     juce::Colour(0xFFF9E353), false };
        case SkillID::VideoExtract:
            return { id, "Video Extract", "VID",
                     juce::Colour(0xFFD2911E), false };
        case SkillID::AudioLab:
            return { id, "Audio Lab", "LAB",
                     juce::Colour(0xFF7B68EE), false };
        default:
            return { SkillID::Gear, "Unknown", "???",
                     juce::Colours::grey, false };
    }
}

inline std::array<SkillID, 5> getDefaultLoadout()
{
    return { SkillID::Gear, SkillID::Record, SkillID::Stow,
             SkillID::Rewind, SkillID::VideoExtract };
}

static constexpr int kNumSkillSlots = 5;
static constexpr int kNumTotalSkills = static_cast<int>(SkillID::COUNT);

//==============================================================================
// Skill Tree Component — pentagon slot layout + pool grid, manual DnD
//==============================================================================
class SkillTreeComponent : public juce::Component
{
public:
    std::function<void(const std::array<SkillID, 5>&)> onLoadoutChanged;

    SkillTreeComponent()
    {
        loadout = getDefaultLoadout();
        loadNonoIcon();
    }

    void setLoadout(const std::array<SkillID, 5>& newLoadout)
    {
        loadout = newLoadout;
        repaint();
    }

    const std::array<SkillID, 5>& getLoadout() const { return loadout; }

    //==========================================================================
    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // ── Base layer: blueprint paper background ──
        g.fillAll(juce::Colour(0xFFE8E4DD));

        // Subtle grid lines (engineering graph paper style)
        {
            float gridSmall = 16.0f;
            float gridLarge = 80.0f;

            g.setColour(juce::Colour(0x0C000000));
            for (float x = 0.0f; x < bounds.getWidth(); x += gridSmall)
                g.drawVerticalLine(static_cast<int>(x), 0.0f, bounds.getHeight());
            for (float y = 0.0f; y < bounds.getHeight(); y += gridSmall)
                g.drawHorizontalLine(static_cast<int>(y), 0.0f, bounds.getWidth());

            g.setColour(juce::Colour(0x18000000));
            for (float x = 0.0f; x < bounds.getWidth(); x += gridLarge)
                g.drawVerticalLine(static_cast<int>(x), 0.0f, bounds.getHeight());
            for (float y = 0.0f; y < bounds.getHeight(); y += gridLarge)
                g.drawHorizontalLine(static_cast<int>(y), 0.0f, bounds.getWidth());

            g.setColour(juce::Colour(0x30000000));
            g.drawRect(bounds.reduced(4.0f), 1.0f);
        }

        // ── Character layer: Nono sprite centered ──
        float nonoCenterX = bounds.getCentreX();
        float nonoCenterY = slotAreaHeight * 0.5f;

        if (nonoIcon.isValid())
        {
            float iconSz = 200.0f;
            g.setOpacity(0.9f);
            g.drawImage(nonoIcon,
                juce::Rectangle<float>(
                    nonoCenterX - iconSz / 2.0f,
                    nonoCenterY - iconSz / 2.0f,
                    iconSz, iconSz),
                juce::RectanglePlacement::centred);
            g.setOpacity(1.0f);
        }

        // ── Annotation lines: each skill slot → its own point on Nono's edge ──
        for (int i = 0; i < kNumSkillSlots; ++i)
        {
            auto slotRect = getSlotRect(i);
            float slotCY = slotRect.getCentreY();

            // Elbow point: horizontal extension from slot inner edge
            float fromX, elbowX;
            if (i < 3)
            {
                fromX = slotRect.getRight() + 2.0f;
                elbowX = fromX + 16.0f;
            }
            else
            {
                fromX = slotRect.getX() - 2.0f;
                elbowX = fromX - 16.0f;
            }

            // Direction from Nono center toward this slot's elbow point
            float dx = elbowX - nonoCenterX;
            float dy = slotCY - nonoCenterY;
            float dist = std::sqrt(dx * dx + dy * dy);

            // Edge point on Nono's visual boundary (~75px from center)
            float nonoVisualRadius = 75.0f;
            float edgeX = nonoCenterX + (dx / dist) * nonoVisualRadius;
            float edgeY = nonoCenterY + (dy / dist) * nonoVisualRadius;

            g.setColour(juce::Colour(0x60000000));

            // Horizontal segment from slot
            g.drawLine(fromX, slotCY, elbowX, slotCY, 1.2f);
            // Diagonal segment from elbow to Nono's edge
            g.drawLine(elbowX, slotCY, edgeX, edgeY, 1.2f);

            // Small dot at slot end
            g.fillEllipse(fromX - 2.0f, slotCY - 2.0f, 4.0f, 4.0f);
            // Small dot at Nono edge
            g.fillEllipse(edgeX - 2.5f, edgeY - 2.5f, 5.0f, 5.0f);
        }

        // Draw 5 skill slots: left column (3) + right column (2)
        for (int i = 0; i < kNumSkillSlots; ++i)
        {
            auto slotRect = getSlotRect(i);
            drawSkillSlot(g, slotRect, loadout[i], i, i == dragSourceSlot);
        }

        // ── Divider line ──
        float dividerY = slotAreaHeight + 10.0f;
        g.setColour(juce::Colour(0x30000000));
        g.drawHorizontalLine(static_cast<int>(dividerY), bounds.getX() + 16.0f, bounds.getRight() - 16.0f);

        // ── Pool section label ──
        g.setColour(juce::Colour(0x80000000));
        g.setFont(juce::Font(12.0f));
        g.drawText("AVAILABLE SKILLS", bounds.getX(), dividerY + 4.0f,
                    bounds.getWidth(), 20.0f, juce::Justification::centred);

        // ── Pool grid (unequipped skills) — 2 rows, 3 columns ──
        auto poolSkills = getPoolSkills();
        for (size_t i = 0; i < poolSkills.size(); ++i)
        {
            auto poolRect = getPoolSlotRect(static_cast<int>(i), dividerY, bounds.getCentreX());
            bool isDragSrc = (dragSourcePool >= 0 && static_cast<size_t>(dragSourcePool) == i);
            drawSkillSlot(g, poolRect, poolSkills[i], -1, isDragSrc, true);
        }

        // ── Draw drag ghost ──
        if (isDragging && dragSkill != SkillID::Gear)
        {
            auto ghostRect = juce::Rectangle<float>(
                dragPos.x - slotSize / 2.0f, dragPos.y - slotSize / 2.0f,
                slotSize, slotSize);
            auto info = getSkillInfo(dragSkill);

            // Shadow
            g.setColour(juce::Colours::black.withAlpha(0.2f));
            g.fillRoundedRectangle(ghostRect.translated(2.0f, 2.0f), 6.0f);

            // Body
            g.setColour(info.glowColour.withAlpha(0.9f));
            g.fillRoundedRectangle(ghostRect, 6.0f);

            // Icon or text
            drawSkillIcon(g, ghostRect.reduced(8.0f), dragSkill, 1.0f);

            // Border
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            g.drawRoundedRectangle(ghostRect, 6.0f, 2.0f);
        }
    }

    //==========================================================================
    void mouseDown(const juce::MouseEvent& event) override
    {
        float mx = static_cast<float>(event.x);
        float my = static_cast<float>(event.y);

        float cx = getWidth() / 2.0f;

        // Check equipped slots (left-3, right-2)
        for (int i = 0; i < kNumSkillSlots; ++i)
        {
            auto rect = getSlotRect(i);
            if (rect.contains(mx, my))
            {
                auto info = getSkillInfo(loadout[i]);
                if (info.isLocked) return;  // Gear is locked

                dragSkill = loadout[i];
                dragSourceSlot = i;
                dragSourcePool = -1;
                isDragging = true;
                dragPos = { mx, my };
                repaint();
                return;
            }
        }

        // Check pool slots (2-row grid)
        auto poolSkills = getPoolSkills();
        float dividerY = slotAreaHeight + 10.0f;

        for (size_t i = 0; i < poolSkills.size(); ++i)
        {
            auto rect = getPoolSlotRect(static_cast<int>(i), dividerY, cx);
            if (rect.contains(mx, my))
            {
                dragSkill = poolSkills[i];
                dragSourceSlot = -1;
                dragSourcePool = static_cast<int>(i);
                isDragging = true;
                dragPos = { mx, my };
                repaint();
                return;
            }
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!isDragging) return;
        dragPos = { static_cast<float>(event.x), static_cast<float>(event.y) };
        repaint();
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (!isDragging) { return; }
        isDragging = false;

        float mx = static_cast<float>(event.x);
        float my = static_cast<float>(event.y);

        float cx = getWidth() / 2.0f;

        // Check if dropped on an equipped slot
        for (int i = 0; i < kNumSkillSlots; ++i)
        {
            auto rect = getSlotRect(i);
            if (!rect.expanded(8.0f).contains(mx, my)) continue;

            if (i == 0) break;  // Can't drop on locked Gear slot

            if (dragSourceSlot >= 0)
            {
                // Swap two equipped slots
                std::swap(loadout[dragSourceSlot], loadout[i]);
            }
            else if (dragSourcePool >= 0)
            {
                // Pool skill replaces equipped slot; old skill returns to pool
                loadout[i] = dragSkill;
            }

            notifyChanged();
            break;
        }

        // Check if dropped on pool area (unequip from slot)
        if (dragSourceSlot > 0)  // slot 0 locked
        {
            float dividerY = slotAreaHeight + 10.0f;
            if (my > dividerY)
            {
                // Find a replacement from pool
                auto poolSkills = getPoolSkills();
                if (!poolSkills.empty())
                {
                    // Replace with first available pool skill
                    loadout[dragSourceSlot] = poolSkills[0];
                    notifyChanged();
                }
            }
        }

        dragSourceSlot = -1;
        dragSourcePool = -1;
        repaint();
    }

private:
    std::array<SkillID, 5> loadout;

    // Drag state
    bool isDragging = false;
    SkillID dragSkill = SkillID::Gear;
    int dragSourceSlot = -1;   // -1 = not from slot
    int dragSourcePool = -1;   // -1 = not from pool
    juce::Point<float> dragPos;

    // Layout constants
    static constexpr float slotAreaHeight = 300.0f;  // top area for Nono + skill columns
    static constexpr float slotSize = 56.0f;
    static constexpr float poolGap = 12.0f;
    static constexpr float slotGapV = 10.0f;  // vertical gap between stacked slots
    static constexpr float columnMargin = 55.0f;  // left/right margin for skill columns

    // Nono center icon + blueprint background
    juce::Image nonoIcon;
    juce::Image blueprintBg;

    void loadNonoIcon()
    {
        nonoIcon = juce::ImageCache::getFromMemory(
            BinaryData::no2nono_PNG, BinaryData::no2nono_PNGSize);
        blueprintBg = juce::ImageCache::getFromMemory(
            BinaryData::no2beijing_png, BinaryData::no2beijing_pngSize);
    }

    //==========================================================================
    // Slot position: Left column (3 slots) + Right column (2 slots)
    // Index 0-2 → left column (top to bottom)
    // Index 3-4 → right column (top to bottom)
    // Nono occupies the center space between columns
    //==========================================================================
    juce::Rectangle<float> getSlotRect(int index) const
    {
        float w = static_cast<float>(getWidth());
        float areaCenter = slotAreaHeight * 0.5f;

        if (index < 3)
        {
            // Left column: 3 slots
            float colX = columnMargin;
            float totalH = 3.0f * slotSize + 2.0f * slotGapV;
            float startY = areaCenter - totalH / 2.0f;
            float sy = startY + static_cast<float>(index) * (slotSize + slotGapV);
            return { colX, sy, slotSize, slotSize };
        }
        else
        {
            // Right column: 2 slots
            float colX = w - columnMargin - slotSize;
            float totalH = 2.0f * slotSize + 1.0f * slotGapV;
            float startY = areaCenter - totalH / 2.0f;
            float sy = startY + static_cast<float>(index - 3) * (slotSize + slotGapV);
            return { colX, sy, slotSize, slotSize };
        }
    }

    //==========================================================================
    // Pool slot position: 2-row grid, 3 columns per row
    //==========================================================================
    static constexpr int poolColumns = 3;

    juce::Rectangle<float> getPoolSlotRect(int index, float dividerY, float centerX) const
    {
        float poolStartY = dividerY + 28.0f;
        int row = index / poolColumns;
        int col = index % poolColumns;
        float totalRowW = static_cast<float>(poolColumns) * (slotSize + poolGap) - poolGap;
        float rowStartX = centerX - totalRowW / 2.0f;
        float px = rowStartX + static_cast<float>(col) * (slotSize + poolGap);
        float py = poolStartY + static_cast<float>(row) * (slotSize + poolGap + 18.0f);  // +18 for name label
        return { px, py, slotSize, slotSize };
    }

    //==========================================================================
    // Get skills NOT currently equipped (for pool display)
    //==========================================================================
    std::vector<SkillID> getPoolSkills() const
    {
        std::vector<SkillID> pool;
        for (int id = 0; id < kNumTotalSkills; ++id)
        {
            auto skill = static_cast<SkillID>(id);
            if (skill == SkillID::Gear) continue;  // Gear never in pool

            bool equipped = false;
            for (int s = 0; s < kNumSkillSlots; ++s)
            {
                if (loadout[s] == skill) { equipped = true; break; }
            }
            if (!equipped) pool.push_back(skill);
        }
        return pool;
    }

    //==========================================================================
    // Draw a single skill slot (pentagon or pool)
    //==========================================================================
    void drawSkillSlot(juce::Graphics& g, juce::Rectangle<float> rect,
                       SkillID skill, int slotIndex, bool isDragSource,
                       bool isPoolSlot = false)
    {
        auto info = getSkillInfo(skill);
        float alpha = isDragSource ? 0.3f : 1.0f;

        if (isPoolSlot)
        {
            // Pool slots: subtle background + border
            g.setColour(info.glowColour.withAlpha(0.15f * alpha));
            g.fillRoundedRectangle(rect, 6.0f);
        }
        // Pentagon slots: NO background, NO glow — icon only

        // Icon
        drawSkillIcon(g, rect.reduced(isPoolSlot ? 8.0f : 4.0f), skill, alpha);

        if (isPoolSlot)
        {
            // Pool slot border
            g.setColour(info.glowColour.withAlpha(0.4f * alpha));
            g.drawRoundedRectangle(rect, 6.0f, 1.5f);
        }
        else if (slotIndex == 0)
        {
            // Locked slot: small lock badge only (no border around icon)
            g.setColour(juce::Colour(0xFFFFD700).withAlpha(0.5f * alpha));
            g.setFont(juce::Font(9.0f));
            g.drawText("LOCK",
                       rect.getX(), rect.getBottom() - 12.0f,
                       rect.getWidth(), 12.0f,
                       juce::Justification::centred);
        }

        // Skill name below slot
        g.setColour(GoodMeterLookAndFeel::ink.withAlpha(0.65f * alpha));
        g.setFont(juce::Font(10.0f));
        g.drawText(info.name,
                   rect.getX() - 10.0f, rect.getBottom() + 2.0f,
                   rect.getWidth() + 20.0f, 14.0f,
                   juce::Justification::centred);
    }

    //==========================================================================
    // Draw skill icon (uses BinaryData PNGs, or text fallback)
    //==========================================================================
    void drawSkillIcon(juce::Graphics& g, juce::Rectangle<float> rect,
                       SkillID skill, float alpha)
    {
        const void* data = nullptr;
        int dataSize = 0;

        switch (skill)
        {
            case SkillID::Gear:
                data = BinaryData::btn_settings_png;
                dataSize = BinaryData::btn_settings_pngSize;
                break;
            case SkillID::Record:
                data = BinaryData::btn_record_png;
                dataSize = BinaryData::btn_record_pngSize;
                break;
            case SkillID::Stow:
                data = BinaryData::btn_stow_png;
                dataSize = BinaryData::btn_stow_pngSize;
                break;
            case SkillID::Rewind:
                data = BinaryData::rewind_skill_icon_png;
                dataSize = BinaryData::rewind_skill_icon_pngSize;
                break;
            case SkillID::VideoExtract:
                data = BinaryData::video_extract_icon_png;
                dataSize = BinaryData::video_extract_icon_pngSize;
                break;
            case SkillID::AudioLab:
                data = BinaryData::anfang_PNG;
                dataSize = BinaryData::anfang_PNGSize;
                break;
            default:
                break;
        }

        if (data != nullptr && dataSize > 0)
        {
            auto img = juce::ImageCache::getFromMemory(data, dataSize);
            if (img.isValid())
            {
                g.setOpacity(alpha * 0.9f);
                g.drawImage(img, rect,
                    juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
                g.setOpacity(1.0f);
                return;
            }
        }

        // Fallback: draw shortName text
        auto info = getSkillInfo(skill);
        g.setColour(info.glowColour.withAlpha(0.85f * alpha));
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(info.shortName, rect, juce::Justification::centred);
    }

    void notifyChanged()
    {
        if (onLoadoutChanged)
            onLoadoutChanged(loadout);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SkillTreeComponent)
};

//==============================================================================
// More Settings Dialog Content — TabbedComponent (Audio + Skills)
//==============================================================================
class MoreSettingsContent : public juce::Component
{
public:
    MoreSettingsContent(juce::AudioDeviceManager* dm,
                        const std::array<SkillID, 5>& currentLoadout,
                        juce::LookAndFeel* lnf)
    {
        tabs = std::make_unique<juce::TabbedComponent>(juce::TabbedButtonBar::TabsAtTop);

        // ── Tab 1: Audio Settings ──
        if (dm != nullptr)
        {
            audioSettings = std::make_unique<juce::AudioDeviceSelectorComponent>(
                *dm,
                0, 256,   // minInputChannels, maxInputChannels
                0, 0,     // minOutputChannels, maxOutputChannels = 0 (hides Output)
                false,    // showMidiInputOptions (GOODMETER doesn't use MIDI)
                false,    // showMidiOutputSelector
                true,     // showChannelsAsStereoPairs
                false);   // hideAdvancedOptions
            if (lnf) audioSettings->setLookAndFeel(lnf);
            audioSettings->setOpaque(false);
            tabs->addTab("Audio", juce::Colours::transparentBlack,
                          audioSettings.get(), false);
        }

        // ── Tab 2: Skill Tree ──
        skillTree = std::make_unique<SkillTreeComponent>();
        skillTree->setLoadout(currentLoadout);
        if (lnf) skillTree->setLookAndFeel(lnf);
        tabs->addTab("Skills", juce::Colours::transparentBlack,
                      skillTree.get(), false);

        addAndMakeVisible(tabs.get());
        if (lnf) tabs->setLookAndFeel(lnf);
    }

    void paint(juce::Graphics& g) override
    {
        // Blueprint paper background across entire settings panel
        g.fillAll(GoodMeterLookAndFeel::bgPaper);
        GoodMeterLookAndFeel::drawBlueprintGrid(g, getLocalBounds().toFloat());
    }

    void resized() override
    {
        tabs->setBounds(getLocalBounds());
    }

    SkillTreeComponent* getSkillTree() { return skillTree.get(); }

private:
    std::unique_ptr<juce::TabbedComponent> tabs;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> audioSettings;
    std::unique_ptr<SkillTreeComponent> skillTree;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MoreSettingsContent)
};

//==============================================================================
// Skill loadout persistence helpers
//==============================================================================
namespace SkillPersistence
{
    inline juce::File getSettingsFile()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("Application Support")
            .getChildFile(juce::String(juce::CharPointer_UTF8(JucePlugin_Name)) + ".settings");
    }

    inline std::array<SkillID, 5> loadLoadout()
    {
        auto file = getSettingsFile();
        if (!file.existsAsFile())
            return getDefaultLoadout();

        juce::XmlDocument doc(file);
        auto xml = doc.getDocumentElement();
        if (xml == nullptr)
            return getDefaultLoadout();

        // Look for <VALUE name="skillLoadout" val="1,2,3,4,5"/>
        for (auto* child : xml->getChildIterator())
        {
            if (child->getStringAttribute("name") == "skillLoadout")
            {
                auto val = child->getStringAttribute("val");
                if (val.isNotEmpty())
                {
                    juce::StringArray tokens;
                    tokens.addTokens(val, ",", "");
                    if (tokens.size() == kNumSkillSlots)
                    {
                        std::array<SkillID, 5> result;
                        result[0] = SkillID::Gear;  // always locked
                        for (int i = 1; i < kNumSkillSlots; ++i)
                        {
                            int id = tokens[i].getIntValue();
                            if (id >= 0 && id < kNumTotalSkills)
                                result[i] = static_cast<SkillID>(id);
                            else
                                result[i] = getDefaultLoadout()[i];
                        }
                        return result;
                    }
                }
            }
        }

        return getDefaultLoadout();
    }

    inline void saveLoadout(const std::array<SkillID, 5>& loadout)
    {
        auto file = getSettingsFile();

        // Load existing XML (or create new root)
        std::unique_ptr<juce::XmlElement> xml;
        if (file.existsAsFile())
        {
            juce::XmlDocument doc(file);
            xml = doc.getDocumentElement();
        }
        if (xml == nullptr)
            xml = std::make_unique<juce::XmlElement>("PROPERTIES");

        // Build comma-separated string
        juce::String val;
        for (int i = 0; i < kNumSkillSlots; ++i)
        {
            if (i > 0) val += ",";
            val += juce::String(static_cast<int>(loadout[i]));
        }

        // Find or create the VALUE element
        bool found = false;
        for (auto* child : xml->getChildIterator())
        {
            if (child->getStringAttribute("name") == "skillLoadout")
            {
                child->setAttribute("val", val);
                found = true;
                break;
            }
        }
        if (!found)
        {
            auto* elem = xml->createNewChildElement("VALUE");
            elem->setAttribute("name", "skillLoadout");
            elem->setAttribute("val", val);
        }

        // Write back
        file.getParentDirectory().createDirectory();
        xml->writeTo(file);
    }
}
