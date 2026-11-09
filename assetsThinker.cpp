#include "assetsThinker.h"
#include <SDL3/SDL.h>
#include <cstddef>
#include <cstring>
#include <memory>

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

#include "include\nlohmann\json.hpp"

#include <cmath>
#include <limits>

using SurfacePtr = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;


namespace {
    /**
    The config_number function retrieves a numeric value from a JSON object using the specified key and ensures that the value is a finite number within the range of a float. 
    If the value is not a number or exceeds the float limits, it throws a runtime error with an appropriate message. 
    */
    float config_number(const nlohmann::json &object, const char *name)
    {
        const auto &field = object.at(name);
        if (!field.is_number())
            throw std::runtime_error(std::string("Expected number: ") + name);

        double value = field.get<double>();
        double limit = std::numeric_limits<float>::max();
        if (!std::isfinite(value) || value < -limit || value > limit)
            throw std::runtime_error(std::string("Number out of float range: ") + name);
        return static_cast<float>(value);
    }
}
/**
 * @note The game_config_load_json function loads game configuration settings from a JSON file specified by the path parameter into a GameConfig structure pointed to by out. 
It validates the JSON content, ensuring the schema version and various configuration values meet specified criteria, and returns true if successful or false if any errors occur during the process.
*/
bool game_config_load_json(GameConfig *out, const char *path)
{
    if (!out || !path || !path[0]) return false;
    // boilerplate code to wrangle json, nothing pretty about it
    try {
        std::ifstream file(path);
        if (!file) throw std::runtime_error("Cannot open game config");
        const auto document = nlohmann::json::parse(file);

        const auto &version = document.at("schema_version");
        if (!version.is_number_integer() || version != 1)
            throw std::runtime_error("Unsupported schema_version; expected integer 1");

        const auto &tick = document.at("simulation").at("tick_hz");
        // Initial supported range; changing tick rate requires a session restart.
        if (!tick.is_number_integer() || tick < 1 || tick > 240)
            throw std::runtime_error("tick_hz must be an integer from 1 to 240");

        GameConfig result{};

        const auto &field = document.at("start_map");

        if (!field.is_string())
            throw std::runtime_error("start_map must be a string");

        const auto &name = field.get_ref<const std::string &>();

        if (name.empty() || name.size() >= sizeof(result.start_map) || name.find('\0') != std::string::npos)
            throw std::runtime_error("Invalid start_map");

        std::memcpy(result.start_map, name.data(), name.size());
        result.start_map[name.size()] = '\0';



        result.tick_hz = tick.get<unsigned>();

        const auto &player = document.at("player");
        result.player.walk_speed = config_number(player, "walk_speed");
        result.player.half_width = config_number(player, "half_width");
        result.player.height = config_number(player, "height");
        result.player.eye_height = config_number(player, "eye_height");
        float pitch_degrees = config_number(player, "max_pitch_degrees");

        result.player.jump_speed = config_number(player, "jump_speed");

        if (result.player.jump_speed < 0.0f)
            throw std::runtime_error("jump_speed must be nonnegative");

        if (result.player.walk_speed < 0.0f)
            throw std::runtime_error("walk_speed must be nonnegative");
        if (result.player.half_width <= 0.0f || result.player.height <= 0.0f)
            throw std::runtime_error("Player dimensions must be positive");
        if (result.player.eye_height <= 0.0f ||
            result.player.eye_height >= result.player.height)
            throw std::runtime_error("eye_height must be between zero and height");
        if (pitch_degrees <= 0.0f || pitch_degrees >= 90.0f)
            throw std::runtime_error("max_pitch_degrees must be between zero and 90");

        const auto &view = document.at("view_defaults");
        float fov_degrees = config_number(view, "vertical_fov_degrees");
        result.near_clip = config_number(view, "near_clip");
        if (fov_degrees <= 0.0f || fov_degrees >= 180.0f)
            throw std::runtime_error("vertical_fov_degrees must be between zero and 180");
        if (result.near_clip <= 0.0f)
            throw std::runtime_error("near_clip must be positive");

        result.gravity = config_number(document.at("physics"), "gravity");
        if (result.gravity < 0.0f) 
            throw std::runtime_error("gravity must be nonnegative");

        constexpr float degrees_to_radians = 0.017453292519943295f;
        result.player.max_pitch_radians = pitch_degrees * degrees_to_radians;
        result.vertical_fov_radians = fov_degrees * degrees_to_radians;
        if (result.player.max_pitch_radians <= 0.0f ||
            result.vertical_fov_radians <= 0.0f)
            throw std::runtime_error("Angle is too small after conversion");

        *out = result;
        return true;
    } catch (const std::exception &error) {
        SDL_Log("Game config '%s': %s", path, error.what());
        return false;
    }
}


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

        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,"Texture '%s' failed to load; using checker.",name);
        return book->fallback;
    }

    SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Unknown texture '%s'; using checker.", name);
    return book->fallback;
}