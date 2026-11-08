#include "texture.h"
#include <stdlib.h>
#include <math.h>

bool texture_init(CPUTexture *texture, int width, int height){
    if (width <= 0 || height <= 0) return false;

    size_t w = (size_t)width;
    size_t h = (size_t)height;

    if (w > SIZE_MAX / h) return false;

    size_t count = w * h;
    
    if (count > SIZE_MAX / sizeof(*texture->pixels)) return false;

    uint32_t *pixels = malloc(count * sizeof(*pixels));

    if (!pixels) return false;

    texture->width = width;
    texture->height = height;
    texture->pixels = pixels;

    //by default textures do not repeat
    texture->repeat = false;

    return true;
}

void texture_shutdown(CPUTexture *texture) {
    if (texture != NULL) {
        free(texture->pixels);
        *texture = (CPUTexture){0};
    }
}

void texture_make_checker(CPUTexture *texture) {
    for (int y = 0; y < texture->height; ++y) {
        for (int x = 0; x < texture->width; ++x) {
            bool alternate = ((x / 8) + (y / 8)) & 1;

            size_t index = (size_t)y * texture->width + x;

            texture->pixels[index] =
                alternate ? 0x00FFFFFF : 0x00800080;
        }
    }
}

CPUTexture *texture_book_create(TextureBook *book, int width, int height)
{
    if (book->count >= TEXTURE_CAPACITY) return NULL;
    
    CPUTexture *texture = &book->textures[book->count];

    if (!texture_init(texture, width, height)) return NULL;

    ++book->count;

    return texture;
}

void texture_book_shutdown(TextureBook *book) {
    while (book->count > 0) {
        --book->count;
        texture_shutdown(&book->textures[book->count]);
    }
    *book = (TextureBook){0};
}

uint32_t texture_sample(const CPUTexture *texture, float u, float v)
{
    // if texture repeat flag is set, trigger uv wrapping (keeps the fractional part)
    if (texture->repeat) {
        u -= floorf(u);
        v -= floorf(v);
    } else {
        u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
        v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
    }

    int x = (int)(u * texture->width);
    int y = (int)(v * texture->height);

    if (x >= texture->width)
        x = texture->width - 1;
    if (y >= texture->height)
        y = texture->height - 1;

    size_t index = (size_t)y * texture->width + x;
    return texture->pixels[index];
}
