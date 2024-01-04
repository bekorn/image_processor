#include "common.hpp"
#include "process.hpp"

#include <unordered_set>

void init(Image const & image)
{
    // printf("[DLL] Init\n");
}

// TODO(bekorn): try weighted mean
//  Currently, background color is changing with quantization. But this is not expected,
//  since its a large area with a solid color. It shouldn't change. To make large areas
//  change less, while calculate the means, multiply colors with their counts'.

void process(Image & image)
{
    printf("[DLL] Processing image %ix%i\n", image.x, image.y);

    i32 const pixel_count = image.x * image.y;

    span<u8x4> pixels{image.pixels.things, pixel_count};
    span<u32> pixels_u32{(u32 *)pixels.ptr, pixels.size};

    // gather a unique set of colors to work with
    unique_array<u8x4> colors;
    size_t colors_size;
    {
        std::unordered_set<u32> set;
        set.reserve(pixel_count / 10);

        // reduce the color resolution to reduce the workload
        // from (2^8)^3 = 16'777'216
        // to   (2^7)^3 =  2'097'152
        // or   (2^6)^3 =    262'144
        u32 color_mask = 0b11111110'11111110'11111110'11111110;
        for (u32 & pixel : pixels_u32)
            set.insert(pixel & color_mask);

        colors_size = set.size();
        colors = new u8x4[colors_size];

        u32 * iter = (u32 *)colors.things;
        for (u32 color : set)
            *iter++ = color;

        printf("Pixel Count: %i\nUnique colors: %zi\n", pixel_count, colors_size);
    }

    srand(123*321);

    u8 const k = 255;
    u8x4 centers[k];
    for (u8x4 & center : centers)
        memcpy(center, colors[rand() % colors_size], 4);

    for (int _ = 0; _ < 64; ++_)
    {
        // assign center to each color
        for (int i = 0; i < colors_size; ++i)
        {
            u8x4 & color = colors[i];

            u8 closest_center;
            int min_dist = INT_MAX;
            for (int ci = 0; ci < k; ++ci)
            {
                u8x4 & center = centers[ci];
                int d0 = int(color[0]) - center[0];
                int d1 = int(color[1]) - center[1];
                int d2 = int(color[2]) - center[2];
                int dist = d0*d0 + d1*d1 + d2*d2;

                if (dist < min_dist)
                    min_dist = dist,
                    closest_center = ci;
            }

            color[3] = closest_center;
        }

        // find the centers' means
        u32x4 means[k] = {0};
        for (int i = 0; i < colors_size; ++i)
        {
            u8x4 & color = colors[i];

            u8 closest_center = color[3];
            u32x4 & mean = means[closest_center];

            mean[0] += color[0],
            mean[1] += color[1],
            mean[2] += color[2],
            mean[3] += 1;
        }

        // move centers to their means
        int active_center_count = 0;
        for (int ci = 0; ci < k; ++ci)
        {
            u32x4 & mean = means[ci];
            u8x4 & center = centers[ci];

            if (mean[3] != 0)
                active_center_count += 1;

            if (mean[3] != 0)
                center[0] = mean[0] / mean[3],
                center[1] = mean[1] / mean[3],
                center[2] = mean[2] / mean[3];
        }
        // printf("Iter %2i, Active centers: %3i / %i\n", _, active_center_count, k);
    }

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < pixel_count; i++)
    {
        u8x4 & pixel = pixels[i];

        u8x4 * closest_center;
        int min_dist = INT_MAX;
        for (u8x4 & center : centers)
        {
            int d0 = int(pixel[0]) - center[0];
            int d1 = int(pixel[1]) - center[1];
            int d2 = int(pixel[2]) - center[2];
            int dist = d0*d0 + d1*d1 + d2*d2;

            if (dist < min_dist)
                min_dist = dist,
                closest_center = &center;
        }

        memcpy(pixel, closest_center, 3);
    }
}