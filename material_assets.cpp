#include "material.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>

// json materials wrangling, boilerplate

namespace {
void read_name(const nlohmann::json &value, char *out, size_t capacity)
{
    if (!value.is_string())
        throw std::runtime_error("Material and texture names must be strings");
    const auto &name = value.get_ref<const std::string &>();
    if (name.empty() || name.size() >= capacity || name.find('\0') != std::string::npos)
        throw std::runtime_error("Names must contain 1 to 63 bytes and no NUL");
    std::memcpy(out, name.data(), name.size());
    out[name.size()] = '\0';
}
}

MaterialBook *material_book_load_json(const char *path)
{
    if (!path || !path[0]) return nullptr;
    try {
        std::ifstream file(path);
        if (!file) throw std::runtime_error("Cannot open material catalog");
        const auto document = nlohmann::json::parse(file);
        const auto &entries = document.at("materials");
        if (!entries.is_array() || entries.size() > MATERIAL_CAPACITY)
            throw std::runtime_error("Expected at most 64 materials");

        MaterialDefinition pending[MATERIAL_CAPACITY]{};
        size_t count = 0;
        for (const auto &entry : entries) {
            read_name(entry.at("name"), pending[count].name,
                      sizeof(pending[count].name));
            read_name(entry.at("albedo_texture"), pending[count].albedo_texture,
                      sizeof(pending[count].albedo_texture));
            for (size_t i = 0; i < count; ++i) {
                if (std::strcmp(pending[count].name, pending[i].name) == 0)
                    throw std::runtime_error("Duplicate material name");
            }
            ++count;
        }

        MaterialBook *book = material_book_create(pending, count);
        if (!book) throw std::runtime_error("Cannot allocate material catalog");
        return book;
    } catch (const std::exception &error) {
        SDL_Log("Material catalog '%s': %s", path, error.what());
        return nullptr;
    }
}
