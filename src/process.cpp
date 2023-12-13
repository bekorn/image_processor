#include "common.hpp"
#include "process.hpp"

namespace {
#line 1
///--- User Code Begin

void init(Image const & image)
{
    printf("DLL Init\n");
}

void process(Image & image)
{
    printf("DLL Processing image %ix%i\n", image.x, image.y);

    int const pixel_count = image.x * image.y;
    for (int i = 0; i < pixel_count; i++)
    {
        u8x4 & pixel = image.pixels[i];
        pixel[0] = 255;
    }
}

///--- User Code End
} // end anonymous namespace

#define EXPORT extern "C" __declspec(dllexport)

// These will check for function signature and make the user code cleaner
EXPORT void EXPORTED_INIT_NAME(Image const & image) { init(image); }
EXPORT void EXPORTED_PROCESS_NAME(Image & image) { process(image); }
