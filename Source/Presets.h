#pragma once
#include "PluginProcessor.h"
#include <vector>

// Factory presets. Each band: { type, freq Hz, gain dB, Q, channel, slope dB/oct }.
// Slope: 6 ... 96 dB/oct, any value (only used by LowPass / HighPass)
namespace Presets
{
struct Band   { int type; float freq; float gain; float q; int channel; float slope; };
struct Preset { const char* name; std::vector<Band> bands; };

inline const std::vector<Preset>& factory()
{
    using namespace EQ;
    static const std::vector<Preset> list =
    {
        { "Vocal - Clean & Present", {
            { HighPass,    90.0f,  0.0f, 0.71f, Stereo, 12.0f },
            { Peak,       250.0f, -2.5f, 1.2f,  Stereo, 12.0f },
            { Peak,      3200.0f,  2.5f, 1.0f,  Stereo, 12.0f },
            { HighShelf, 11000.0f, 2.0f, 0.7f,  Stereo, 12.0f } } },

        { "Kick - Punch", {
            { HighPass,    30.0f,  0.0f, 0.71f, Stereo, 24.0f },
            { Peak,        60.0f,  3.0f, 1.0f,  Stereo, 12.0f },
            { Peak,       350.0f, -4.0f, 1.4f,  Stereo, 12.0f },
            { Peak,      4000.0f,  2.5f, 1.0f,  Stereo, 12.0f } } },

        { "Mix - Mud Cleaner", {
            { Peak,       250.0f, -2.5f, 1.0f,  Stereo, 12.0f },
            { Peak,       400.0f, -2.0f, 1.5f,  Stereo, 12.0f } } },

        { "Mix - Air", {
            { HighShelf, 12000.0f, 3.0f, 0.6f,  Stereo, 12.0f } } },

        { "Master - Gentle Polish", {
            { HighPass,    25.0f,  0.0f, 0.71f, Stereo, 12.0f },
            { HighPass,   120.0f,  0.0f, 0.71f, Side,   12.0f },
            { LowShelf,    80.0f,  1.0f, 0.7f,  Stereo, 12.0f },
            { HighShelf, 10000.0f, 1.5f, 0.6f,  Stereo, 12.0f } } },

        { "Widen - Side Air", {
            { HighPass,   200.0f,  0.0f, 0.71f, Side,   12.0f },
            { HighShelf,  6000.0f, 3.0f, 0.7f,  Side,   12.0f } } },
    };
    return list;
}
}
