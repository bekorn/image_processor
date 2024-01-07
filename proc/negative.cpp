#include "common.hpp"
#include "process.hpp"

void init(Image const & image)
{
    // printf("Init\n");
}

void process(Image & image)
{
    printf("Processing image %ix%i\n", image.x, image.y);

    i32 const pixel_count = image.x * image.y;
    // #pragma omp parallel for schedule(static)
    for (i32 i = 0; i < pixel_count; i++)
    {
        u8x4 & pixel = image.pixels[i];

        pixel[0] = 255 - pixel[0];
        pixel[1] = 255 - pixel[1];
        pixel[2] = 255 - pixel[2];
        pixel[3] = 255 - pixel[3];
    }
}