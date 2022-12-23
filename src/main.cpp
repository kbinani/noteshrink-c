#include "noteshrink.h"
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#include <string>
#include <vector>

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        printf("usage: %s image_im image_out\n", argv[0]);
    }
    else
    {
        std::string file(argv[1]);

        int width, height, bpp;
        stbi_uc* pixels = stbi_load(file.c_str(), &width, &height, &bpp, STBI_rgb_alpha);
        std::vector<NSHRgb> img(width * height);

        size_t i = 0;
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
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
        std::vector<NSHRgb> result;
        NSHCreatePalette(img, img.size(), o, palette.data(), palette.size(), result, width, height);
        std::vector<NSHRgb> temVector;
        int numberOfChannels = 3;

        uint8_t* data = new uint8_t[width * height * numberOfChannels];
        uint8_t* datafg = new uint8_t[width * height * numberOfChannels];


        i = 0;
        for (int y = 0; y < height; y++)
        {
            for (int x = 0; x < width; x++)
            {
                size_t idx = (height - y - 1) * width * numberOfChannels + x * numberOfChannels;
                NSHRgb p = result[i++];
                data[idx] = (uint8_t)p.R;
                data[idx + 1] = (uint8_t)p.G;
                data[idx + 2] = (uint8_t)p.B;
            }
        }
        printf("done");
        stbi_write_png(argv[2], width, height, numberOfChannels, data, width * numberOfChannels);
    }
    return 0;
}
