#include "common.hpp"
#include "process.hpp"

#include <unordered_map>

void init(Image const & image)
{
    // printf("Init\n");
}

void process(Image & image)
{
    printf("Processing image %ix%i\n", image.x, image.y);

    i32 const pixel_count = image.x * image.y;

    span<u8x4> pixels{image.pixels.things, pixel_count};
    span<u32> pixels_u32{(u32 *)pixels.ptr, pixels.size};

    // gather a unique set of colors to work with
    unique_array<u8x4> colors;
    unique_array<u32> counts;
    size_t colors_size;
    {
        std::unordered_map<u32, u32> color_count;
        color_count.reserve(pixel_count / 10);

        // reduce the color resolution to reduce the workload
        // from (2^8)^3 = 16'777'216
        // to   (2^7)^3 =  2'097'152
        // or   (2^6)^3 =    262'144
        // u32 color_mask = 0b11111111'11111111'11111111'11111111;
        u32 color_mask = 0b11111110'11111110'11111110'11111110;
        // u32 color_mask = 0b11111100'11111100'11111100'11111100;

        for (u32 & pixel : pixels_u32)
            color_count[pixel & color_mask] += 1;

        colors_size = color_count.size();
        colors = new u8x4[colors_size];
        counts = new u32[colors_size];

        u32 * iter_color = (u32 *)colors.things;
        u32 * iter_count = counts.things;
        for (auto [color, count] : color_count)
            *iter_color++ = color,
            *iter_count++ = count;

        printf("Pixel Count: %i\nUnique colors: %zi\n", pixel_count, colors_size);
    }

    srand(123*321);

    int const k = 28;
    u8x4 centers[k];
    for (u8x4 & center : centers)
        memcpy(center, colors[rand() % colors_size], 4);

    for (int iteration = 0; iteration < 64; ++iteration)
    {
        // find the means
        u32x4 means[k] = {0};
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

            u32x4 & mean = means[closest_center];
            u32 count = counts[i];

            mean[0] += color[0] * count,
            mean[1] += color[1] * count,
            mean[2] += color[2] * count,
            mean[3] += count;
        }

        // move centers to their means
        int max_center_movement = 0;
        for (int ci = 0; ci < k; ++ci)
        {
            u32x4 & mean = means[ci];
            u8x4 & center = centers[ci];

            if (mean[3] != 0)
            {
                u8x4 new_center = {
                    mean[0] / mean[3],
                    mean[1] / mean[3],
                    mean[2] / mean[3],
                };

                int d0 = abs(int(new_center[0]) - center[0]);
                int d1 = abs(int(new_center[1]) - center[1]);
                int d2 = abs(int(new_center[2]) - center[2]);
                int dist = d0 + d1 + d2;

                max_center_movement = max(max_center_movement, dist);

                memcpy(center, new_center, 4);
            }
            else
            {
                //TODO(bekorn): maybe move center to somewhere it can be useful
            }
        }
        // printf("Iter %2i, Max center movement %4i\n", iteration, max_center_movement);

        if (max_center_movement < 4) // arbitrary threshold
        {
            printf("Breaking early due to low movement, after iteration %i.\n", iteration);
            break;
        }
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

        memcpy(pixel, closest_center, 4);
    }


    /// Debug

    if (true) // visualize centers
    for (int i = 0; i < k; ++i)
    {
        u8x4 & center = centers[i];
        int const dim = 16;
        int const grid_dim = 32;
        u8x4 * iter = pixels.begin();
        iter += dim * (i % grid_dim); // x
        iter += dim * (i / grid_dim) * image.y; // y
        for (int y = 0; y < dim; ++y)
        {
            for (int x = 0; x < dim; ++x)
                memcpy(iter + x, center, 4);

            iter += image.x;
        }
    }
}