#include "noteshrink.h"

#include <cassert>
#include <cmath>

#include <algorithm>
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
    for (size_t i = 0; i < inputSize; i += interval)
    {
        samples.push_back(input[i]);
    }
}

static void Quantize(std::vector<NSHRgb> const& image, int bitsPerChannel, std::vector<uint32_t>& quantized)
{
    uint8_t shift = 8 - bitsPerChannel;
    uint8_t halfbin = uint8_t((1 << shift) >> 1);

    quantized.clear();
    quantized.reserve(image.size());

    for (size_t i = 0; i < image.size(); i++)
    {
        uint32_t r = ((uint8_t(image[i].R) >> shift) << shift) + halfbin;
        uint32_t g = ((uint8_t(image[i].G) >> shift) << shift) + halfbin;
        uint32_t b = ((uint8_t(image[i].B) >> shift) << shift) + halfbin;
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
    if (h > 0)
    {
        if (max == r)
        {
            h = (g - b) / h;
            if (h < 0)
            {
                h += 6;
            }
        }
        else if (max == g)
        {
            h = 2 + (b - r) / h;
        }
        else
        {
            h = 4 + (r - g) / h;
        }
    }
    h /= 6;
    s = max - min;
    if (max > 0)
    {
        s /= max;
    }
    v = max;
}

static NSHRgb HsvToRgb(float h, float s, float v)
{
    float r = v;
    float g = v;
    float b = v;
    if (s > 0)
    {
        h *= 6.;
        int i = int(h);
        float f = h - float(i);
        switch (i)
        {
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
    for (size_t i = 1; i < quantized.size(); i++)
    {
        uint32_t v = quantized[i];
        int c = count[v] + 1;
        if (c > maxcount)
        {
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

static void CreateForegroundMask(NSHRgb bgColor, std::vector<NSHRgb> const& samples, NSHOption option, std::vector<bool>& mask)
{
    float hBg, sBg, vBg;
    RgbToHsv(bgColor, hBg, sBg, vBg);
    std::vector<float> sSamples;
    sSamples.reserve(samples.size());
    std::vector<float> vSamples;
    vSamples.reserve(samples.size());
    for (size_t i = 0; i < samples.size(); i++)
    {
        float h, s, v;
        RgbToHsv(samples[i], h, s, v);
        sSamples.push_back(s);
        vSamples.push_back(v);
    }

    mask.clear();
    mask.reserve(samples.size());
    for (size_t i = 0; i < samples.size(); i++)
    {
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
    for (size_t i = 0; i < means.size(); i++)
    {
        float squaredDistance = SquareDistance(p, means[i]);
        if (squaredDistance < minimum)
        {
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

    for (int i = 0; i < k; i++)
    {
        float h = float(i) / float(k - 1);
        NSHRgb p = HsvToRgb(h, 1, 1);
        float minval = 196608.0f;
        int k = 0;
        for (int j = 0; j < data.size(); j++)
        {
            float dist = SquareDistance(p, data[j]);
            if (dist < minval)
            {
                minval = dist;
                k = j;
            }
        }
        means.push_back(data[k]);
    }

    std::vector<int> clusters(data.size());
    for (size_t i = 0; i < data.size(); i++)
    {
        NSHRgb d = data[i];
        clusters[i] = Closest(d, means);
    }

    std::vector<int> mLen(k);
    for (int itr = 0; itr < maxItr; itr++)
    {
        for (size_t i = 0; i < k; i++)
        {
            NSHRgb p;
            p.R = p.G = p.B = 0;
            means[i] = p;
            mLen[i] = 0;
        }
        for (size_t i = 0; i < data.size(); i++)
        {
            NSHRgb p = data[i];
            int cluster = clusters[i];
            NSHRgb m = Add(means[cluster], p);
            means[cluster] = m;
            mLen[cluster] = mLen[cluster] + 1;
        }
        for (size_t i = 0; i < means.size(); i++)
        {
            int len = std::max(1, mLen[i]);
            NSHRgb m = Mul(means[i], 1 / float(len));
            means[i] = m;
        }
        int changes = 0;
        for (size_t i = 0; i < data.size(); i++)
        {
            NSHRgb p = data[i];
            int cluster = Closest(p, means);
            if (cluster != clusters[i])
            {
                changes++;
                clusters[i] = cluster;
            }
        }
        if (changes == 0)
        {
            break;
        }
    }
}

static void PaletteCreate(std::vector<NSHRgb> const& samples, NSHRgb bgColor, NSHOption option, std::vector<NSHRgb>& outPalette)
{
    std::vector<bool> fgMask;
    CreateForegroundMask(bgColor, samples, option, fgMask);
    std::vector<NSHRgb> data;

    for (int i = 0; i < samples.size(); i++)
    {
        if (fgMask[i])
        {
            NSHRgb v = samples[i];
            data.push_back(v);
        }
    }

    std::vector<NSHRgb> means;
    KMeans(data, outPalette.size() - 1, option.KmeansMaxIter, means);

    size_t idx = 0;
    outPalette[idx++] = bgColor;
    for (size_t i = 0; i < means.size(); i++)
    {
        NSHRgb c = means[i];
        c.R = round(c.R);
        c.G = round(c.G);
        c.B = round(c.B);
        outPalette[idx++] = c;
    }
}

static void PaletteApply(std::vector<NSHRgb>& img, std::vector<NSHRgb>& palette, NSHRgb origBgColor, NSHRgb bgColor, NSHOption option, std::vector<NSHRgb>& result, int width, int height)
{
    std::vector<bool> fgMask;
    CreateForegroundMask(origBgColor, img, option, fgMask);
    // Despeckle 3x3
    int k = width + 1;
    for (int y = 1; y < height - 1; y++)
    {
        for (int x = 1; x < width - 1; x++)
        {
            int l = 0;
            if (fgMask[k] == fgMask[k - width - 1]) l++;
            if (fgMask[k] == fgMask[k - width]) l++;
            if (fgMask[k] == fgMask[k - width + 1]) l++;
            if (fgMask[k] == fgMask[k - 1]) l++;
            if (fgMask[k] == fgMask[k + 1]) l++;
            if (fgMask[k] == fgMask[k + width - 1]) l++;
            if (fgMask[k] == fgMask[k + width]) l++;
            if (fgMask[k] == fgMask[k + width + 1]) l++;
            if (l < 4) fgMask[k] = !fgMask[k];
            k++;
        }
        k++;
        k++;
    }
    for (int i = 0; i < img.size(); i++)
    {
        if (!fgMask[i])
        {
            result.push_back(bgColor);
            continue;
        }
        NSHRgb p = img[i];
        int minIdx = Closest(p, palette);
        if (minIdx == 0)
        {
            result.push_back(bgColor);
        }
        else
        {
            result.push_back(palette[minIdx]);
        }
    }
}

static void PaletteSaturate(std::vector<NSHRgb>& palette, std::vector<NSHRgb>& resultPalette)
{
    float maxSat = 0;
    float minSat = 1;

    for (int i = 0; i < palette.size(); i++)
    {
        float h, s, v;
        RgbToHsv(palette[i], h, s, v);
        maxSat = std::max(maxSat, s);
        minSat = std::min(minSat, s);
    }

    for (int i = 0; i < palette.size(); i++)
    {
        float h, s, v;
        RgbToHsv(palette[i], h, s, v);
        float newSat = (s - minSat) / (maxSat - minSat);
        resultPalette[i] = HsvToRgb(h, newSat, v);
    }
    return;
    std::vector<float> pArray(palette.size() * 3);
    for (int i = 0; i < palette.size(); i++)
    {
        NSHRgb p = palette[i];
        int j = i * 3;
        pArray[j] = p.R;
        pArray[j + 1] = p.G;
        pArray[j + 2] = p.B;
    }

    float min = *std::min_element(pArray.begin(), pArray.end());
    float max = *std::max_element(pArray.begin(), pArray.end());

    float diff = max - min;
    for (int i = 0; i < palette.size(); i++)
    {
        resultPalette[i].R = (uint8_t)(255 * (palette[i].R - min) / diff);
        resultPalette[i].G = (uint8_t)(255 * (palette[i].G - min) / diff);
        resultPalette[i].B = (uint8_t)(255 * (palette[i].B - min) / diff);
    }
}

static void SaturatePaletteImg(std::vector<NSHRgb>& palette, std::vector<NSHRgb>& imPalette, std::vector<NSHRgb>& imgResultPalette)
{
    float maxSat = 0;
    float minSat = 1;

    for (int i = 0; i < palette.size(); i++)
    {
        float h, s, v;
        RgbToHsv(palette[i], h, s, v);
        maxSat = std::max(maxSat, s);
        minSat = std::min(minSat, s);
    }

    for (int i = 0; i < imPalette.size(); i++)
    {
        float h, s, v;
        RgbToHsv(imPalette[i], h, s, v);
        float newSat = (s - minSat) / (maxSat - minSat);
        imgResultPalette[i] = HsvToRgb(h, newSat, v);
    }
    return;
    std::vector<float> pArray(palette.size() * 3);
    for (int i = 0; i < palette.size(); i++)
    {
        NSHRgb p = palette[i];
        int j = i * 3;
        pArray[j] = p.R;
        pArray[j + 1] = p.G;
        pArray[j + 2] = p.B;
    }

    float min = *std::min_element(pArray.begin(), pArray.end());
    float max = *std::max_element(pArray.begin(), pArray.end());
    float diff = max - min;
    for (int i = 0; i < imPalette.size(); i++)
    {
        imgResultPalette[i].R = (uint8_t)(255 * (imPalette[i].R - min) / diff);
        imgResultPalette[i].G = (uint8_t)(255 * (imPalette[i].G - min) / diff);
        imgResultPalette[i].B = (uint8_t)(255 * (imPalette[i].B - min) / diff);
    }
}

extern "C" NSHOption NSHMakeDefaultOption()
{
    NSHOption o;
    o.SampleFraction = 0.05f;
    o.BrightnessThreshold = 0.25f;
    o.SaturationThreshold = 0.20f;
    o.KmeansMaxIter = 40;
    o.Saturate = false;
    o.WhiteBackground = false;
    o.NumColors = 6;
    return o;
}

extern "C" bool NSHCreatePalette(std::vector<NSHRgb>& input, size_t inputSize, NSHOption option, NSHRgb* palette, size_t paletteSize, std::vector<NSHRgb>& result, int width, int height)
{
    if (input.empty() || !palette)
    {
        return false;
    }
    if (paletteSize < 2)
    {
        return false;
    }

    std::vector<NSHRgb> samples;
    SamplePixels(input.data(), inputSize, option, samples);
    NSHRgb origBgColor = FindBackgroundColor(samples, bitsPerSample);
    std::vector<NSHRgb> pal(paletteSize);
    std::vector<NSHRgb> resultPal(paletteSize);
    PaletteCreate(samples, origBgColor, option, pal);
    NSHRgb bgColor = origBgColor;
    if (option.Saturate)
    {
        PaletteSaturate(pal, resultPal);
    }
    else
    {
        for (int k = 0; k < resultPal.size(); k++)
        {
            resultPal[k] = pal[k];
        }
    }
    if (option.WhiteBackground)
    {
        bgColor = NSHRgb{ 255, 255, 255 };
    }
    PaletteApply(input, resultPal, origBgColor, bgColor, option, result, width, height);

    return true;
}
