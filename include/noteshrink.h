#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct {
    float R; // [0, 255]
    float G;
    float B;
} NSHRgb;


typedef struct {
    float SampleFraction;
    float BrightnessThreshold;
    float SaturationThreshold;
    int KmeansMaxIter;
    bool Saturate;
    bool WhiteBackground;
} NSHOption;


extern NSHOption NSHMakeDefaultOption();

extern bool NSHCreatePalette(NSHRgb *input, size_t inputSize, NSHOption option, NSHRgb *palette, size_t paletteSize);

#ifdef __cplusplus
} // extern "C"
#endif // _\cplusplus
