#ifndef LITTLEENGINE_LIGHTING_H
#define LITTLEENGINE_LIGHTING_H
#include "lighting_types.h"
#include "raycast.h"
#include <stdint.h>

enum { LIGHTING_ENCODE_STEPS = 4096 };
typedef struct {
    float to_linear[256];
    uint8_t to_srgb[LIGHTING_ENCODE_STEPS + 1];
} LightingLUT;
typedef struct {
    Vector3 direction_to_light;
    float distance;           /* FLT_MAX for directional light. */
    Vector3 irradiance;       /* Includes color, strength, falloff, N dot L. */
} LightSample;

#ifdef __cplusplus
extern "C" {
#endif
SceneLighting lighting_default_unlit(void);
/** Stage and normalize a bounded light list; out unchanged on failure.
 * No allocation. Unused entries beyond light_count are ignored. */
bool lighting_prepare(SceneLighting *out, const SceneLighting *source);
bool lighting_validate(const SceneLighting *lighting);
/** Requires prepared light, finite point and unit normal. Returns false for
 * no direct contribution, outside radius, or coincident point/source.
 * Output is unchanged on false. No allocation or shadow query occurs here. */
bool lighting_sample_light(const Light *light, Vector3 point, Vector3 normal,LightSample *out);
/** Build a surface-normal-offset ray ending before a point light.
 * Prepared light/finite inputs required; output unchanged on failure. */
bool lighting_shadow_ray(const Light *light, Vector3 point, Vector3 normal, float bias, RaySegment *out);
bool lighting_lut_init(LightingLUT *lut);
#ifdef __cplusplus
}
#endif

static inline uint8_t lighting_encode_channel(float linear, const LightingLUT *lut)
{
    if (linear >= 1.0f) return 255;
    if (linear <= 0.0f) return 0;
    unsigned index = (unsigned)(linear * LIGHTING_ENCODE_STEPS + 0.5f);
    return lut->to_srgb[index];
}
/** Requires initialized LUT and finite nonnegative accumulated linear RGB.
 * Sum lights first. Clip only here, after multiplication by linear albedo.
 * Packed input/output XRGB8888; high byte unused. No allocation. */
static inline uint32_t lighting_apply_rgb(uint32_t rgb, Vector3 illumination, const LightingLUT *lut)
{
    uint32_t r = lighting_encode_channel(lut->to_linear[(rgb >> 16) & 255] * illumination.x,lut);
    uint32_t g = lighting_encode_channel(lut->to_linear[(rgb >> 8) & 255] * illumination.y,lut);
    uint32_t b = lighting_encode_channel(lut->to_linear[rgb & 255] * illumination.z,lut);
    return (r << 16) | (g << 8) | b;
}
#endif
