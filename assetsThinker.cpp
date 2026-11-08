#include "assetsThinker.h"

#include <cstddef>
#include <cstring>
#include <memory>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "include\nlohmann\json.hpp"

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
        /**
         * @brief Unlock the borrowed converted surface before its owning smart pointer destroys it.
         */
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

// routine to read texture catalogue defined in textures.json -> TextureRecord -> TextureBook
bool texture_book_read_catalog(TextureBook *book, const char *path)
{
    if (!book || !path || !path[0] || book->record_count != 0) return false;
    // try to load json; boilerplate code
    try {
        std::ifstream file(path);
        if (!file) throw std::runtime_error("Cannot open catalog");
        const auto document = nlohmann::json::parse(file);
        const auto &entries = document.at("textures");
        if (!entries.is_array() || entries.size() > TEXTURE_CAPACITY)
            throw std::runtime_error("Expected a bounded textures array");

        //load json and prepare textures for loading
        TextureRecord pending[TEXTURE_CAPACITY]{};
        size_t count = 0;
        const auto directory = std::filesystem::absolute(path).parent_path();

        for (const auto &entry : entries) {
            const std::string name = entry.at("name").get<std::string>();
            const std::string filename = entry.at("filename").get<std::string>();
            if (name.empty() || name == "checker" ||
                name.size() >= TEXTURE_NAME_BYTES || filename.empty() ||
                name.find('\0') != std::string::npos ||
                filename.find('\0') != std::string::npos)
                throw std::runtime_error("Invalid texture name or filename");

            for (size_t i = 0; i < count; ++i)
                if (name == pending[i].name)
                    throw std::runtime_error("Duplicate texture name: " + name);

            const std::filesystem::path relative(filename);
            if (relative.has_root_path())
                throw std::runtime_error("Texture filenames must be relative");
            const std::string resolved =
                (directory / relative).lexically_normal().generic_string();
            if (resolved.size() >= TEXTURE_PATH_BYTES)
                throw std::runtime_error("Texture filename is too long");

            // let cpp handle this instead of littleEngine
            std::memcpy(pending[count].name, name.c_str(), name.size() + 1);
            std::memcpy(pending[count].filename, resolved.c_str(), resolved.size() + 1);

            // by default textures don't repeat, empty value implies false
            pending[count].repeat = entry.value("repeat", false);

            ++count;
        }

        // Commit only after every entry has passed validation.
        for (size_t i = 0; i < count; ++i)
            book->records[i] = pending[i];
        book->record_count = count;
        return true;
    } catch (const std::exception &error) {
        SDL_Log("Texture catalog '%s': %s", path, error.what());
        return false;
    }
}

//resolve filename into cached pixels. includes fallback to checker if load fails
const CPUTexture *texture_book_resolve(TextureBook *book, const char *name)
{
    if (!book || !name || !book->fallback)
        return nullptr;

    if (std::strcmp(name, "checker") == 0)
        return book->fallback;

    for (size_t i = 0; i < book->record_count; ++i) {
        TextureRecord &record = book->records[i];

        if (std::strcmp(record.name, name) != 0)
            continue;

        if (record.state == TEXTURE_READY)
            return record.texture;

        if (record.state == TEXTURE_FAILED)
            return book->fallback;

        CPUTexture *loaded = texture_book_load_png(book, record.filename);

        if (loaded) loaded->repeat = record.repeat;

        record.texture = loaded;

        if (record.texture) {
            record.state = TEXTURE_READY;
            return record.texture;
        }

        record.state = TEXTURE_FAILED;

        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Texture '%s' failed to load; using checker.",
            name);

        return book->fallback;
    }

    SDL_LogWarn(
        SDL_LOG_CATEGORY_APPLICATION,
        "Unknown texture '%s'; using checker.",
        name);

    return book->fallback;
}