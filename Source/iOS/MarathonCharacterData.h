/*
  ==============================================================================
    MarathonCharacterData.h
    GOODMETER iOS - Character outline anchor points for Marathon art style

    Defines normalized coordinates for Nono and Guoba characters
  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
struct CharacterOutline
{
    std::vector<juce::Point<float>> points;  // Normalized [-1.0, 1.0]
    bool closed;
};

struct FacialFeature
{
    juce::Point<float> center;
    float radius;
    std::vector<juce::Point<float>> detailPoints;
};

struct CharacterData
{
    CharacterOutline bodyOutline;
    FacialFeature leftEye;
    FacialFeature rightEye;
    FacialFeature nose;
    FacialFeature leftEar;
    FacialFeature rightEar;
    juce::Point<float> testTube;  // Test tube position
};

//==============================================================================
namespace MarathonCharacterData
{
    // Nono character - ellipse-based, mathematically generated
    static CharacterData createNono()
    {
        CharacterData nono;

        // Body: ellipse outline (24 points)
        nono.bodyOutline.closed = true;
        for (int i = 0; i < 24; ++i)
        {
            float angle = (i / 24.0f) * juce::MathConstants<float>::twoPi;
            float x = 0.5f * std::cos(angle);
            float y = 0.8f * std::sin(angle);
            nono.bodyOutline.points.push_back({x, y});
        }

        // Eyes
        nono.leftEye = {{-0.2f, -0.3f}, 0.08f, {}};
        nono.rightEye = {{0.2f, -0.3f}, 0.08f, {}};

        // Nose (small)
        nono.nose = {{0.0f, -0.1f}, 0.04f, {}};

        // Ears (blade-shaped)
        nono.leftEar = {{-0.4f, -0.6f}, 0.12f, {}};
        nono.rightEar = {{0.4f, -0.6f}, 0.12f, {}};

        // Test tube
        nono.testTube = {0.6f, 0.0f};

        return nono;
    }

    // Guoba character - full traced contour from sprite
    static CharacterData createGuoba()
    {
        CharacterData guoba;

        // Body outline (81 points, full circle)
        guoba.bodyOutline.closed = true;
        guoba.bodyOutline.points = {
            {-0.396f, -0.001f}, {-0.415f, -0.053f}, {-0.438f, -0.104f}, {-0.451f, -0.158f},
            {-0.444f, -0.212f}, {-0.443f, -0.268f}, {-0.454f, -0.321f}, {-0.475f, -0.377f},
            {-0.487f, -0.432f}, {-0.496f, -0.487f}, {-0.495f, -0.543f}, {-0.492f, -0.597f},
            {-0.478f, -0.652f}, {-0.438f, -0.702f}, {-0.389f, -0.712f}, {-0.334f, -0.700f},
            {-0.282f, -0.676f}, {-0.229f, -0.648f}, {-0.178f, -0.615f}, {-0.124f, -0.596f},
            {-0.069f, -0.605f}, {-0.014f, -0.608f}, {0.042f, -0.606f}, {0.093f, -0.603f},
            {0.146f, -0.600f}, {0.198f, -0.635f}, {0.251f, -0.665f}, {0.306f, -0.687f},
            {0.361f, -0.699f}, {0.417f, -0.695f}, {0.468f, -0.668f}, {0.489f, -0.612f},
            {0.499f, -0.557f}, {0.498f, -0.501f}, {0.493f, -0.445f}, {0.479f, -0.390f},
            {0.461f, -0.335f}, {0.436f, -0.281f}, {0.443f, -0.229f}, {0.447f, -0.176f},
            {0.445f, -0.122f}, {0.422f, -0.069f}, {0.386f, -0.022f}, {0.387f, 0.033f},
            {0.390f, 0.089f}, {0.403f, 0.143f}, {0.426f, 0.196f}, {0.443f, 0.252f},
            {0.469f, 0.306f}, {0.477f, 0.359f}, {0.486f, 0.415f}, {0.470f, 0.471f},
            {0.456f, 0.526f}, {0.437f, 0.581f}, {0.385f, 0.616f}, {0.331f, 0.630f},
            {0.278f, 0.629f}, {0.224f, 0.613f}, {0.178f, 0.567f}, {0.124f, 0.552f},
            {0.069f, 0.546f}, {0.014f, 0.557f}, {-0.042f, 0.555f}, {-0.096f, 0.553f},
            {-0.150f, 0.561f}, {-0.191f, 0.589f}, {-0.242f, 0.622f}, {-0.298f, 0.630f},
            {-0.350f, 0.628f}, {-0.403f, 0.608f}, {-0.446f, 0.564f}, {-0.459f, 0.510f},
            {-0.476f, 0.456f}, {-0.484f, 0.402f}, {-0.480f, 0.348f}, {-0.460f, 0.292f},
            {-0.442f, 0.237f}, {-0.425f, 0.183f}, {-0.399f, 0.129f}, {-0.397f, 0.074f},
            {-0.395f, 0.020f},
        };

        // Eyes (larger, rounder)
        guoba.leftEye = {{-0.18f, -0.40f}, 0.10f, {}};
        guoba.rightEye = {{0.18f, -0.40f}, 0.10f, {}};

        // Nose (Guoba's pink triangle nose)
        guoba.nose = {{0.0f, -0.20f}, 0.06f, {}};

        // Ears (large triangular)
        guoba.leftEar = {{-0.45f, -0.75f}, 0.12f, {}};
        guoba.rightEar = {{0.45f, -0.75f}, 0.12f, {}};

        // Test tube
        guoba.testTube = {0.65f, 0.0f};

        return guoba;
    }
}

