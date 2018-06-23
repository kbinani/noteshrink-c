#include "noteshrink.h"

#include <algorithm>
#include <vector>

static void SamplePixels(NSHRgb* input, size_t inputSize, NSHOption o, std::vector<NSHRgb>& samples)
{
    samples.clear();
    size_t numSamples = (size_t)std::min(std::max(float(inputSize) * o.SampleFraction, float(0)), float(inputSize));
    size_t interval = std::max((size_t)1, inputSize / numSamples);
    for (size_t i = 0; i < inputSize; i += interval) {
        samples.push_back(input[i]);
    }
}


extern "C" NSHOption NSHMakeDefaultOption()
{
    NSHOption o;
    o.SampleFraction = 0.05f;
    o.BrightnessThreshold = 0.25f;
    o.SaturationThreshold = 0.20f;
    o.KmeansMaxIter = 40;
    o.Saturate = true;
    o.WhiteBackground = true;
    return o;
}


extern "C" bool NSHCreatePalette(NSHRgb* input, size_t inputSize, NSHOption option, NSHRgb* palette, size_t paletteSize)
{
    if (!input || !palette) {
        return false;
    }
    if (paletteSize < 2) {
        return false;
    }
    std::vector<NSHRgb> samples;
    SamplePixels(input, inputSize, option, samples);
}
