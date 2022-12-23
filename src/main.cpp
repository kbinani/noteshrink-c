#include "noteshrink.h"
#include <stb/stb_image.h>
#include <stb/stb_image_write.h>
#include <string>
#include <vector>
#include <unistd.h>

void NoteshrinkUsage(char* prog, NSHOption o)
{
    printf("NoteShrink.\n");
    printf("usage: %s [options] image_im image_out\n", prog);
    printf("options:\n");
    printf("  -k NUM_ITERS      number of iterations KMeans (default %d)\n", o.KmeansMaxIter);
    printf("  -n NUM_COLORS     number of output colors (default %d)\n", o.NumColors);
    printf("  -p NUM            part of pixels to sample (default %f)\n", o.SampleFraction);
    printf("  -s NUM            background saturation threshold (default %f)\n", o.SaturationThreshold);
    printf("  -v NUM            background value threshold (default %f)\n", o.BrightnessThreshold);
    printf("  -w                make background white (default %d)\n", o.WhiteBackground);
    printf("  -S                do not saturate colors (default %d)\n", o.Saturate);
    printf("  -h                show this help message and exit\n");
}

int main(int argc, char **argv)
{
    NSHOption o = NSHMakeDefaultOption();
    int fhelp = 0;
    if (argc < 3)
    {
        NoteshrinkUsage(argv[0], o);
        return 0;
    }
    else
    {
        int opt;
        while ((opt = getopt(argc, argv, ":k:n:p:s:v:wSh")) != -1)
        {
            switch(opt)
            {
            case 'k':
                o.KmeansMaxIter = atoi(optarg);
                break;
            case 'n':
                o.NumColors = atoi(optarg);
                break;
            case 'p':
                o.SampleFraction = atof(optarg);
                break;
            case 's':
                o.SaturationThreshold = atof(optarg);
                break;
            case 'v':
                o.BrightnessThreshold = atof(optarg);
                break;
            case 'w':
                o.WhiteBackground = !o.WhiteBackground;
                break;
            case 'S':
                o.Saturate = !o.Saturate;
                break;
            case 'h':
                fhelp = 1;
                break;
            case ':':
                printf("option needs a value\n");
                break;
            case '?':
                printf("unknown option: %c\n", optopt);
                break;
            }
        }
        if(optind + 2 > argc || fhelp)
        {
            NoteshrinkUsage(argv[0], o);
            return 0;
        }
        std::string filein(argv[optind]);
        std::string fileout(argv[optind + 1]);

        int width, height, bpp;
        stbi_uc* pixels = stbi_load(filein.c_str(), &width, &height, &bpp, STBI_rgb_alpha);
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

        std::vector<NSHRgb> palette(o.NumColors);
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
        stbi_write_png(fileout.c_str(), width, height, numberOfChannels, data, width * numberOfChannels);
    }
    return 0;
}
