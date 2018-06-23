#include "noteshrink.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <iostream>
#include <map>
#include <vector>

namespace
{
int const bitsPerSample = 6;
}

static void SamplePixels(NSHRgb* input, size_t inputSize, NSHOption o, std::vector<NSHRgb>& samples)
{
    samples.clear();
    size_t numSamples = (size_t)std::min(std::max(float(inputSize) * o.SampleFraction, float(0)), float(inputSize));
    size_t interval = std::max((size_t)1, inputSize / numSamples);
    for (size_t i = 0; i < inputSize; i += interval) {
        samples.push_back(input[i]);
    }
}


static void Quantize(std::vector<NSHRgb> const& image, int bitsPerChannel, std::vector<uint32_t>& quantized)
{
    uint8_t shift = 8 - bitsPerChannel;
    uint8_t halfbin = uint8_t((1 << shift) >> 1);

    quantized.clear();
    quantized.reserve(image.size());

    for (size_t i = 0; i < image.size(); i++) {
        uint32_t r = (uint8_t(image[i].R) >> shift) << shift + halfbin;
        uint32_t g = (uint8_t(image[i].G) >> shift) << shift + halfbin;
        uint32_t b = (uint8_t(image[i].B) >> shift) << shift + halfbin;
        uint32_t p = (((r << 8) | g) << 8) | b;
        quantized.push_back(p);
    }
}


static void RgbToHsv(NSHRgb p, float& h, float& s, float& v)
{
    float r = p.R / 255.0f;
    float g = p.G / 255.0f;
    float b = p.B / 255.0f;
    float max = std::max(std::max(r, g), b);
    float min = std::min(std::min(r, g), b);
    h = max - min;
    if (h > 0) {
        if (max == r) {
            h = (g - b) / h;
            if (h < 0) {
                h += 6;
            }
        } else if (max == g) {
            h = 2 + (b - r) / h;
        } else {
            h = 4 + (r - g) / h;
        }
    }
    h /= 6;
    s = max - min;
    if (max > 0) {
        s /= max;
    }
    v = max;
}


static NSHRgb HsvToRgb(float h, float s, float v)
{
    float r = v;
    float g = v;
    float b = v;
    if (s > 0) {
        h *= 6.;
        int i = int(h);
        float f = h - float(i);
        switch (i) {
            default:
            case 0:
                g *= 1 - s * (1 - f);
                b *= 1 - s;
                break;
            case 1:
                r *= 1 - s * f;
                b *= 1 - s;
                break;
            case 2:
                r *= 1 - s;
                b *= 1 - s * (1 - f);
                break;
            case 3:
                r *= 1 - s;
                g *= 1 - s * f;
                break;
            case 4:
                r *= 1 - s * (1 - f);
                g *= 1 - s;
                break;
            case 5:
                g *= 1 - s;
                b *= 1 - s * f;
                break;
        }
    }
    NSHRgb p;
    p.R = r * 255;
    p.G = g * 255;
    p.B = b * 255;
    return p;
}



static NSHRgb FindBackgroundColor(std::vector<NSHRgb> const& image, int bitsPerChannel)
{
    std::vector<uint32_t> quantized;
    Quantize(image, bitsPerChannel, quantized);
    std::map<uint32_t, int> count;
    int maxcount = 1;
    uint32_t maxvalue = quantized[0];
    for (size_t i = 1; i < quantized.size(); i++) {
        uint32_t v = quantized[i];
        int c = count[v] + 1;
        if (c > maxcount) {
            maxcount = c;
            maxvalue = v;
        }
        count[v] = c;
    }

    uint8_t shift = 8 - bitsPerChannel;
    uint8_t r = (maxvalue >> 16) & 0xff;
    uint8_t g = (maxvalue >> 8) & 0xff;
    uint8_t b = maxvalue & 0xff;

    NSHRgb bg;
    bg.R = r;
    bg.G = g;
    bg.B = b;

    return bg;
}


void CreateForegroundMask(NSHRgb bgColor, std::vector<NSHRgb> const& samples, NSHOption option, std::vector<bool>& mask)
{
    float hBg, sBg, vBg;
    RgbToHsv(bgColor, hBg, sBg, vBg);
    std::vector<float> sSamples;
    sSamples.reserve(samples.size());
    std::vector<float> vSamples;
    vSamples.reserve(samples.size());
    for (size_t i = 0; i < samples.size(); i++) {
        float h, s, v;
        RgbToHsv(samples[i], h, s, v);
        sSamples.push_back(s);
        vSamples.push_back(v);
    }

    mask.clear();
    mask.reserve(samples.size());
    for (size_t i = 0; i < samples.size(); i++) {
        float sDiff = fabs(sBg - sSamples[i]);
        float vDiff = fabs(vBg - vSamples[i]);
        bool fg = vDiff >= option.BrightnessThreshold || sDiff >= option.SaturationThreshold;
        mask.push_back(fg);
    }
}


static float SquareDistance(NSHRgb a, NSHRgb b)
{
    float squareDistance = 0;
    squareDistance += (a.R - b.R) * (a.R - b.R);
    squareDistance += (a.G - b.G) * (a.G - b.G);
    squareDistance += (a.B - b.B) * (a.B - b.B);
    return squareDistance;
}


static int Closest(NSHRgb p, std::vector<NSHRgb> const& means)
{
    int idx = 0;
    float minimum = SquareDistance(p, means[0]);
    for (size_t i = 0; i < means.size(); i++) {
        float squaredDistance = SquareDistance(p, means[i]);
        if (squaredDistance < minimum) {
            minimum = squaredDistance;
            idx = i;
        }
    }
    return idx;
}


static NSHRgb Add(NSHRgb a, NSHRgb b)
{
    NSHRgb r;
    r.R = a.R + b.R;
    r.G = a.G + b.G;
    r.B = a.B + b.B;
    return r;
}


static NSHRgb Mul(NSHRgb a, float scalar)
{
    NSHRgb r;
    r.R = a.R * scalar;
    r.G = a.G * scalar;
    r.B = a.B * scalar;
    return r;
}


static void KMeans(std::vector<NSHRgb> const& data, int k, int maxItr, std::vector<NSHRgb>& means)
{
    means.clear();
    means.reserve(k);
    for (int i = 0; i < k; i++) {
        float h = float(i) / float(k - 1);
        NSHRgb p = HsvToRgb(h, 1, 1);
        means.push_back(p);
    }

    std::vector<int> clusters(data.size());
    for (size_t i = 0; i < data.size(); i++) {
        NSHRgb d = data[i];
        clusters[i] = Closest(d, means);
    }

    std::vector<int> mLen(k);
    for (int itr = 0; itr < maxItr; itr++) {
        for (size_t i = 0; i < k; i++) {
            NSHRgb p;
            p.R = p.G = p.B = 0;
            means[i] = p;
            mLen[i] = 0;
        }
        for (size_t i = 0; i < data.size(); i++) {
            NSHRgb p = data[i];
            int cluster = clusters[i];
            NSHRgb m = Add(means[cluster], p);
            means[cluster] = m;
            mLen[cluster] = mLen[cluster] + 1;
        }
        for (size_t i = 0; i < means.size(); i++) {
            int len = std::max(1, mLen[i]);
            NSHRgb m = Mul(means[i], 1 / float(len));
            means[i] = m;
        }
        int changes = 0;
        for (size_t i = 0; i < data.size(); i++) {
            NSHRgb p = data[i];
            int cluster = Closest(p, means);
            if (cluster != clusters[i]) {
                changes++;
                clusters[i] = cluster;
            }
        }
        if (changes == 0) {
            break;
        }
    }
}


static void CreatePalette(std::vector<NSHRgb> const& samples, NSHOption option, std::vector<NSHRgb>& outPalette, NSHRgb& bgColor)
{
    bgColor = FindBackgroundColor(samples, bitsPerSample);

    std::vector<bool> fgMask;
    CreateForegroundMask(bgColor, samples, option, fgMask);

    std::vector<NSHRgb> means;
    KMeans(samples, outPalette.size() - 1, option.KmeansMaxIter, means);

    size_t idx = 0;
    outPalette[idx++] = bgColor;
    for (size_t i = 0; i < means.size(); i++) {
        NSHRgb c = means[i];
        c.R = round(c.R);
        c.G = round(c.G);
        c.B = round(c.B);
        outPalette[idx++] = c;
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
    auto start = std::chrono::high_resolution_clock::now();

    if (!input || !palette) {
        return false;
    }
    if (paletteSize < 2) {
        return false;
    }
    std::vector<NSHRgb> samples;
    SamplePixels(input, inputSize, option, samples);
    NSHRgb bgColor;
    std::vector<NSHRgb> pal(paletteSize);
    CreatePalette(samples, option, pal, bgColor);

    auto end = std::chrono::high_resolution_clock::now();
    auto elapsed = end - start;
    std::cout << (1e9 / std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count()) << " fps" << std::endl;
}
