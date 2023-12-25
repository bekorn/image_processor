#include "common.hpp"
#include "process.hpp"

namespace {
///--- User Code Begin

void init(Image const & image)
{
    // printf("[DLL] Init\n");
}

void process(Image & image)
{
    printf("[DLL] Processing image %ix%i\n", image.x, image.y);

    srand(123*321);

    i32 const pixel_count = image.x * image.y;
    #pragma omp parallel for schedule(static)
    for (i32 i = 0; i < pixel_count; i++)
    {
        u8x4 & pixel = image.pixels[i];

        // TODO(bekorn): if I ever add Time parameter, add [0, 0.4] to the blue's factor
        f32 luminance = (
            powf(pixel[0] / 255.f, 2.2f) * 0.2126f +
            powf(pixel[1] / 255.f, 2.2f) * 0.7152f +
            powf(pixel[2] / 255.f, 2.2f) * 0.0722f
        );

        f32 luminance_diff = abs(
            (luminance - pixel[0] / 255.f) +
            (luminance - pixel[1] / 255.f) +
            (luminance - pixel[2] / 255.f)
        );

        luminance *= max(0.f, -0.2f + powf(max(0.f, -0.04f + float(rand()) / RAND_MAX), 0.05f));

        if (luminance < 0.1f) luminance = 0.04f;
        else if (luminance < 0.2f) luminance = 0.13f;

        luminance *= (luminance_diff < 0.85f);
        luminance *= 0.04f + powf(luminance_diff, 1.5f);

        luminance = saturate(-0.84f + powf(luminance + 0.84f, 2.2f));
        luminance *= 1.6f;
        luminance = saturate(powf(luminance, 0.8f));

        pixel[0] = pixel[1] = pixel[2] = u8(luminance * 255);
    }
}

///--- User Code End
} // end anonymous namespace

#define EXPORT extern "C" __declspec(dllexport)

// These will check for function signature and make the user code cleaner
EXPORT void EXPORTED_INIT_NAME(Image const & image) { init(image); }
EXPORT void EXPORTED_PROCESS_NAME(Image & image) { process(image); }
