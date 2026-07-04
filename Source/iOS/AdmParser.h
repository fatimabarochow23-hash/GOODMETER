/*
  ==============================================================================
    AdmParser.h
    GOODMETER iOS - ADM BWF (Dolby Atmos master WAV) reader + object renderer.

    Standards: ITU-R BS.2076 (ADM XML) + EBU Tech 3285 chna chunk — both open,
    no Dolby licensing involved. Strategy:
      1. Scan the RIFF chunk table ourselves (JUCE ignores axml/chna).
      2. chna maps wav track index -> audioTrackUID / pack format ID; pack ID
         prefix AP_0003 = Objects, AP_0001 = DirectSpeakers (bed).
      3. Object tracks: audioChannelFormat (AC_0003xxxx) audioBlockFormat list
         gives time-stamped azimuth/elevation/distance/gain (polar or cartesian).
      4. Render: encode beds (fixed speaker directions by layout) + objects
         (interpolated positions) into FOA (ambiX SN3D), then run the SAME
         octahedron binaural decoder the app monitors with -> stereo WAV.
    Defensive throughout: any parse failure degrades to bed-only rendering
    with generic directions — never worse than the live downmix.
    Header-only, no pbxproj registration.
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "FOABinauralDecoder.h"

namespace AdmParser
{

struct ObjectBlock
{
    double t = 0.0, dur = 0.0;      // seconds
    float az = 0.0f, el = 0.0f;     // radians, ambiX: +az = left, +el = up
    float gain = 1.0f;
};

struct AdmObject
{
    int channelIndex = -1;          // wav track
    std::vector<ObjectBlock> blocks;
};

struct Document
{
    bool hasAxml = false;           // "this is an ADM/Atmos master"
    bool hasObjects = false;
    int numChannels = 0;
    std::vector<AdmObject> objects;
    std::vector<int> bedChannels;   // wav tracks not claimed by objects
};

//==============================================================================
// RIFF scan: returns the raw bytes of a named chunk ("axml", "chna"), or {}.
inline juce::MemoryBlock readRiffChunk(const juce::File& f, const char* fourcc)
{
    juce::FileInputStream in(f);
    if (! in.openedOk())
        return {};

    char hdr[12];
    if (in.read(hdr, 12) != 12 || memcmp(hdr, "RIFF", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0)
    {
        // RF64 masters use "RF64" + "WAVE" with a ds64 chunk; chunk walk still works.
        if (memcmp(hdr, "RF64", 4) != 0 || memcmp(hdr + 8, "WAVE", 4) != 0)
            return {};
    }

    while (! in.isExhausted())
    {
        char id[4];
        if (in.read(id, 4) != 4)
            break;
        juce::uint32 sz = 0;
        if (in.read(&sz, 4) != 4)
            break;
        // ds64-style oversized data chunk: skip via file length guard below
        if (memcmp(id, fourcc, 4) == 0)
        {
            const size_t cap = (size_t) juce::jmin<juce::int64>((juce::int64) sz, 64 * 1024 * 1024);
            juce::MemoryBlock mb(cap);
            if (in.read(mb.getData(), (int) cap) != (int) cap)
                return {};
            return mb;
        }
        juce::int64 skip = (juce::int64) sz + (sz & 1);          // word aligned
        if (sz == 0xFFFFFFFFu)                                    // RF64 big data
            skip = in.getTotalLength() - in.getPosition();
        if (! in.setPosition(in.getPosition() + skip))
            break;
    }
    return {};
}

inline bool fileHasAxml(const juce::File& f)
{
    return readRiffChunk(f, "axml").getSize() > 0;
}

//==============================================================================
inline double parseAdmTime(const juce::String& s)
{
    // "hh:mm:ss.fffff" (fraction length varies)
    auto parts = juce::StringArray::fromTokens(s, ":", "");
    if (parts.size() != 3)
        return s.getDoubleValue();
    return parts[0].getDoubleValue() * 3600.0
         + parts[1].getDoubleValue() * 60.0
         + parts[2].getDoubleValue();
}

//==============================================================================
inline Document parse(const juce::File& f, int wavChannels)
{
    Document doc;
    doc.numChannels = wavChannels;

    const auto axml = readRiffChunk(f, "axml");
    doc.hasAxml = axml.getSize() > 0;

    // Default: everything is bed.
    for (int c = 0; c < wavChannels; ++c)
        doc.bedChannels.push_back(c);

    if (! doc.hasAxml)
        return doc;

    // chna: 4 bytes counts, then 40-byte entries:
    // u16 trackIndex(1-based), char UID[12], char trackFormatRef[14],
    // char packFormatRef[11], u8 pad
    struct TrackMap { int track; juce::String channelFormatId; bool isObject; };
    std::vector<TrackMap> maps;

    const auto chna = readRiffChunk(f, "chna");
    if (chna.getSize() >= 44)
    {
        const auto* d = static_cast<const juce::uint8*>(chna.getData());
        const size_t nEntries = (chna.getSize() - 4) / 40;
        for (size_t e = 0; e < nEntries; ++e)
        {
            const auto* p = d + 4 + e * 40;
            const int track = (int) (p[0] | (p[1] << 8)) - 1;      // -> 0-based
            const juce::String trackRef(juce::CharPointer_UTF8((const char*) p + 14), 14);
            const juce::String packRef(juce::CharPointer_UTF8((const char*) p + 28), 11);
            if (track < 0 || track >= wavChannels)
                continue;
            // AT_00031001_01 -> channel format AC_00031001 ; AP_0003... = Objects
            const bool isObj = packRef.startsWith("AP_0003");
            juce::String acId;
            if (trackRef.startsWith("AT_") && trackRef.length() >= 11)
                acId = "AC_" + trackRef.substring(3, 11);
            maps.push_back({ track, acId, isObj });
        }
    }

    juce::String xmlText(juce::CharPointer_UTF8((const char*) axml.getData()),
                         axml.getSize());
    auto xml = juce::XmlDocument::parse(xmlText);
    if (xml == nullptr)
        return doc;

    // Collect audioChannelFormat elements (any nesting depth).
    std::map<juce::String, const juce::XmlElement*> channelFormats;
    std::function<void(const juce::XmlElement&)> walk =
        [&](const juce::XmlElement& e)
    {
        for (auto* child : e.getChildIterator())
        {
            if (child->hasTagName("audioChannelFormat"))
                channelFormats[child->getStringAttribute("audioChannelFormatID")] = child;
            walk(*child);
        }
    };
    walk(*xml);

    for (const auto& m : maps)
    {
        if (! m.isObject || m.channelFormatId.isEmpty())
            continue;
        const auto it = channelFormats.find(m.channelFormatId);
        if (it == channelFormats.end())
            continue;

        AdmObject obj;
        obj.channelIndex = m.track;

        for (auto* blk : it->second->getChildWithTagNameIterator("audioBlockFormat"))
        {
            ObjectBlock b;
            b.t = parseAdmTime(blk->getStringAttribute("rtime", "0"));
            b.dur = parseAdmTime(blk->getStringAttribute("duration", "0"));

            float az = 0, el = 0;
            bool cartesian = false;
            float cx = 0, cy = 0, cz = 0;
            for (auto* pos : blk->getChildWithTagNameIterator("position"))
            {
                const auto coord = pos->getStringAttribute("coordinate");
                const float v = pos->getAllSubText().getFloatValue();
                if      (coord == "azimuth")   az = v;
                else if (coord == "elevation") el = v;
                else if (coord == "X") { cx = v; cartesian = true; }
                else if (coord == "Y") { cy = v; cartesian = true; }
                else if (coord == "Z") { cz = v; cartesian = true; }
            }
            if (cartesian)
            {
                // ADM cartesian: X right, Y front, Z up -> ambiX az/el
                b.az = std::atan2(-cx, cy);
                b.el = std::atan2(cz, std::sqrt(cx * cx + cy * cy) + 1.0e-9f);
            }
            else
            {
                // ADM polar: azimuth counterclockwise (+ = left) = ambiX ✓
                b.az = juce::degreesToRadians(az);
                b.el = juce::degreesToRadians(el);
            }
            if (auto* g = blk->getChildByName("gain"))
                b.gain = g->getAllSubText().getFloatValue();
            obj.blocks.push_back(b);
        }

        if (! obj.blocks.empty())
        {
            doc.objects.push_back(std::move(obj));
            doc.bedChannels.erase(std::remove(doc.bedChannels.begin(), doc.bedChannels.end(),
                                              m.track),
                                  doc.bedChannels.end());
            doc.hasObjects = true;
        }
    }

    return doc;
}

//==============================================================================
// Bed speaker direction (az, el in radians, gain) by wav track position within
// the bed, using SMPTE/Atmos order conventions per layout size.
struct BedDir { float az, el, gain; };

inline BedDir bedDirection(int posInBed, int bedSize)
{
    auto D = [](float azDeg, float elDeg, float g) {
        return BedDir { juce::degreesToRadians(azDeg), juce::degreesToRadians(elDeg), g };
    };
    // L R C LFE Lss Rss Lrs Rrs (Lw Rw) Ltf Rtf (Ltr Rtr)
    switch (posInBed)
    {
        case 0: return D( 30, 0, 1.0f);
        case 1: return D(-30, 0, 1.0f);
        case 2: return bedSize > 3 ? D(0, 0, 1.0f) : D(0, 0, 1.0f);       // C
        case 3: return bedSize >= 6 ? D(0, -30, 0.5f) : D(110, 0, 1.0f);  // LFE (down-ish) / 5.0 Ls
        default: break;
    }
    const int k = posInBed - 4;
    const bool left = (k & 1) == 0;
    const int pair = k / 2;
    float azd = 90.0f, eld = 0.0f;
    if (bedSize <= 6)            { azd = 110.0f; }                        // 5.1 surrounds
    else if (pair == 0)          { azd = 90.0f; }                         // sides
    else if (pair == 1)          { azd = 135.0f; }                        // rears
    else if (bedSize >= 14 && pair == 2) { azd = 60.0f; }                 // wides (9.1.4)
    else if (pair == 2 || pair == 3)
    {
        eld = 45.0f;
        azd = (pair == 2) ? 45.0f : 135.0f;                               // tops front/rear
    }
    else                         { eld = 45.0f; azd = 90.0f; }
    return D(left ? azd : -azd, eld, 0.7071f);
}

//==============================================================================
/** Offline: render ADM master (objects + beds) to binaural stereo WAV via the
    app's octahedron decoder. Returns true on success. */
inline bool renderToBinaural(const juce::File& src, const juce::File& dest)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(src));
    if (reader == nullptr || reader->numChannels < 3)
        return false;

    const int nCh = (int) reader->numChannels;
    const auto doc = parse(src, nCh);

    FOABinauralDecoder decoder;
    decoder.prepare(reader->sampleRate);

    dest.deleteFile();
    std::unique_ptr<juce::FileOutputStream> os(dest.createOutputStream());
    if (os == nullptr)
        return false;
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(os.get(), reader->sampleRate, 2, 24, {}, 0));
    if (writer == nullptr)
        return false;
    os.release();

    const int bs = juce::jmin(4096, (int) FOABinauralDecoder::maxBlockSize);
    juce::AudioBuffer<float> in(nCh, bs), foa(4, bs), out(2, bs);

    // Per-object playhead into its block list
    std::vector<size_t> cursor(doc.objects.size(), 0);

    juce::int64 pos = 0;
    const auto total = (juce::int64) reader->lengthInSamples;
    const double sr = reader->sampleRate;

    while (pos < total)
    {
        const int n = (int) juce::jmin<juce::int64>(bs, total - pos);
        reader->read(&in, 0, n, pos, true, true);
        foa.clear();

        auto encode = [&](const float* s, int num, float az, float el, float gain)
        {
            const float ux = std::cos(el) * std::cos(az);
            const float uy = std::cos(el) * std::sin(az);
            const float uz = std::sin(el);
            auto* w = foa.getWritePointer(0); auto* y = foa.getWritePointer(1);
            auto* z = foa.getWritePointer(2); auto* x = foa.getWritePointer(3);
            for (int i = 0; i < num; ++i)
            {
                const float v = s[i] * gain;
                w[i] += v;
                y[i] += v * uy; z[i] += v * uz; x[i] += v * ux;
            }
        };

        // Beds: fixed directions
        for (size_t b = 0; b < doc.bedChannels.size(); ++b)
        {
            const int ch = doc.bedChannels[b];
            if (ch >= nCh) continue;
            const auto d = bedDirection((int) b, (int) doc.bedChannels.size());
            encode(in.getReadPointer(ch), n, d.az, d.el, d.gain);
        }

        // Objects: position from the block active at this time (step values —
        // ADM blocks are short; per-block granularity is inaudible for QC).
        const double tNow = (double) pos / sr;
        for (size_t o = 0; o < doc.objects.size(); ++o)
        {
            const auto& obj = doc.objects[o];
            if (obj.channelIndex >= nCh) continue;
            auto& cur = cursor[o];
            while (cur + 1 < obj.blocks.size()
                   && obj.blocks[cur + 1].t <= tNow)
                ++cur;
            const auto& blk = obj.blocks[cur];
            encode(in.getReadPointer(obj.channelIndex), n, blk.az, blk.el, blk.gain);
        }

        out.clear();
        decoder.process(foa, out, n);
        // Mild headroom: dense masters can stack up
        out.applyGain(0, 0, n, 0.71f);
        out.applyGain(1, 0, n, 0.71f);
        writer->writeFromAudioSampleBuffer(out, 0, n);
        pos += n;
    }
    writer->flush();
    return true;
}

} // namespace AdmParser
