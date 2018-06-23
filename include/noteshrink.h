#pragma once

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct {
    char R;
    char G;
    char B;
} NSHRgb;


typedef struct {
    float SampleFraction;
    float BrightnessThreshold;
    float SaturationThreshold;
    int NumColors;
    int KmeansMaxIter;
    bool Saturate;
    bool WhiteBackground;
} NSHOption;


extern NSHOption NSHMakeDefaultOption();

extern bool NSHCreatePalette(NSHRgb *input, size_t inputSize, NSHOption option, NSHRgb *palette, size_t paletteSize);

#ifdef __cplusplus
} // extern "C"
#endif // _\cplusplus
