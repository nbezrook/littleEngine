#include "assetsThinker.h"

#include <cstddef>
#include <cstring>
#include <memory>

using SurfacePtr = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;

CPUTexture *texture_book_load_png(TextureBook *book, const char *path)
{
    SurfacePtr image(SDL_LoadPNG(path), SDL_DestroySurface);

    if (!image) {
        SDL_Log("PNG load failed: %s", SDL_GetError()); 
        return nullptr;
    }

    SurfacePtr converted(SDL_ConvertSurface(image.get(), SDL_PIXELFORMAT_XRGB8888), SDL_DestroySurface);

    if (!converted) {
        SDL_Log("PNG conversion failed: %s", SDL_GetError());
        return nullptr;
    }

    if (!SDL_LockSurface(converted.get())) {
        SDL_Log("PNG pixel access failed: %s", SDL_GetError());
        return nullptr;
    }

    struct UnlockSurface {
        SDL_Surface *surface;
        ~UnlockSurface() {
            SDL_UnlockSurface(surface);
        }
    } unlock{converted.get()};

    CPUTexture *texture =
        texture_book_create(book, converted->w, converted->h);

    if (!texture) {
        SDL_Log("Could not create texture-book entry.");
        return nullptr;
    }

    std::size_t row_bytes =
        static_cast<std::size_t>(texture->width) * sizeof(*texture->pixels);

    const auto *source = static_cast<const unsigned char *> (converted->pixels);

    for (int y = 0; y < texture->height; ++y) {
        std::memcpy(texture->pixels + static_cast<std::size_t>(y) * texture->width,
        source + static_cast<std::size_t>(y) * converted->pitch, row_bytes);
    }
    
    return texture;
}