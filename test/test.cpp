#include "noteshrink.h"
#include <stb/stb_image.h>
#include <string>
#include <vector>

int main()
{
    std::string file("notesA1.png");
    int width, height, bpp;
    stbi_uc* pixels = stbi_load(file.c_str(), &width, &height, &bpp, STBI_rgb_alpha);
    std::vector<NSHRgb> img(width * height);
    size_t i = 0;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            size_t idx = (height - y - 1) * width * 4 + x * 4;
            NSHRgb p;
            p.R = pixels[idx];
            p.G = pixels[idx + 1];
            p.B = pixels[idx + 2];
            img[i++] = p;
        }
    }
    free(pixels);

    NSHOption o = NSHMakeDefaultOption();
    std::vector<NSHRgb> palette(8);
    NSHCreatePalette(img.data(), img.size(), o, palette.data(), palette.size());
}
