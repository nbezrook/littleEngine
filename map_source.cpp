#include "map_source.h"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>


// thanks man good json reader saves us a lot of nothing work
namespace {
float read_number(const nlohmann::json &value)
{
    if (!value.is_number())
        throw std::runtime_error("Coordinate must be a number");
    double number = value.get<double>();
    double limit = std::numeric_limits<float>::max();
    if (!std::isfinite(number) || number < -limit || number > limit)
        throw std::runtime_error("Coordinate must fit a finite float");
    return static_cast<float>(number);
}

Vector3 read_vector3(const nlohmann::json &value)
{
    if (!value.is_array() || value.size() != 3)
        throw std::runtime_error("Position must contain exactly three numbers");
    return Vector3{read_number(value.at(0)), read_number(value.at(1)),read_number(value.at(2))};
}

void read_material_name(const nlohmann::json &value, char *out)
{
    if (!value.is_string())
        throw std::runtime_error("Material name must be a string");
    const auto &name = value.get_ref<const std::string &>();
    if (name.empty() || name.size() >= MAP_MATERIAL_NAME_BYTES || name.find('\0') != std::string::npos)
        throw std::runtime_error("Material name must contain 1 to 63 bytes and no NUL");
    std::memcpy(out, name.data(), name.size());
    out[name.size()] = '\0';
}

Point2 read_point2(const nlohmann::json &value)
{
    if (!value.is_array() || value.size() != 2)
        throw std::runtime_error("Mapping value must contain exactly two numbers");
    return Point2{read_number(value.at(0)), read_number(value.at(1))};
}

SurfaceDefinition read_surface(const nlohmann::json &value)
{
    SurfaceDefinition result{};
    read_material_name(value.at("material"), result.material_name);
    const auto &mapping = value.at("mapping");
    result.mapping.metres_per_repeat = read_point2(mapping.at("metres_per_repeat"));
    result.mapping.offset = read_point2(mapping.at("offset"));
    if (result.mapping.metres_per_repeat.x <= 0.0f || result.mapping.metres_per_repeat.y <= 0.0f)
        throw std::runtime_error("metres_per_repeat must be positive on both axes");
    float degrees = read_number(mapping.at("rotation_degrees"));
    constexpr float degrees_to_radians = 0.017453292519943295f;
    result.mapping.rotation_radians = std::remainder(degrees, 360.0f) * degrees_to_radians;
    return result;
}
}
// not glamourous 
bool box_room_definition_load_json(BoxRoomDefinition *out, const char *path)
{
    if (!out || !path || !path[0]) return false;

    try {
        std::ifstream file(path);
        if (!file) throw std::runtime_error("Cannot open map source");
        const auto document = nlohmann::json::parse(file);

        const auto &version = document.at("version");
        if (!version.is_number_integer() || version != 2)
            throw std::runtime_error("Expected integer map version 2");
        const auto &rooms = document.at("rooms");
        if (!rooms.is_array() || rooms.size() != 1)
            throw std::runtime_error("This checkpoint supports exactly one room");

        BoxRoomDefinition result{};
        const auto &room = rooms.at(0);
        result.player_spawn = read_vector3(document.at("player_spawn"));
        result.bounds.min = read_vector3(room.at("min"));
        result.bounds.max = read_vector3(room.at("max"));
        result.walls = read_surface(room.at("walls"));
        result.floor = read_surface(room.at("floor"));
        result.ceiling = read_surface(room.at("ceiling"));

        Vector3 lo = result.bounds.min;
        Vector3 hi = result.bounds.max;
        Vector3 spawn = result.player_spawn;
        if (!(lo.x < hi.x && lo.y < hi.y && lo.z < hi.z) || !std::isfinite(hi.x - lo.x) || !std::isfinite(hi.y - lo.y) || !std::isfinite(hi.z - lo.z))
            throw std::runtime_error("Room extents must be positive and finite");
        if (spawn.x < lo.x || spawn.x > hi.x || spawn.y < lo.y || spawn.y > hi.y || spawn.z < lo.z || spawn.z > hi.z)
            throw std::runtime_error("Spawn point must be inside room bounds");

        *out = result;
        return true;
    } catch (const std::exception &error) {
        SDL_Log("Map source '%s': %s", path, error.what());
        return false;
    }
}
