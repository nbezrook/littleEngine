#include "lighting.h"
#include "math/math.h"
#include <float.h>
#include <math.h>

static bool finite_vec(Vector3 v) { return isfinite(v.x) && isfinite(v.y) && isfinite(v.z); }
static bool color_valid(Vector3 v)
{
    return finite_vec(v) && v.x>=0 && v.x<=1 && v.y>=0 && v.y<=1 && v.z>=0 && v.z<=1;
}
static bool light_valid(const Light *light, bool prepared)
{
    /* Headroom ensures finite additive sums for every permitted light count. */
    if (!color_valid(light->color) || !isfinite(light->intensity)
        || light->intensity<0 || light->intensity>FLT_MAX/(4.0f*SCENE_LIGHT_CAPACITY))
        return false;
    if (light->type==LIGHT_POINT)
        return finite_vec(light->position) && isfinite(light->radius) && light->radius>0;
    if (light->type==LIGHT_DIRECTIONAL) {
        Vector3 v=light->direction_to_light;
        if (!finite_vec(v)) return false;
        float length=vec3_dot(v,v);
        return isfinite(length) && length>0 && (!prepared || fabsf(length-1)<.001f);
    }
    return false;
}
SceneLighting lighting_default_unlit(void)
{
    SceneLighting result={0};
    result.ambient=(Vector3){1,1,1};
    result.shadow_bias=.001f;
    return result;
}
bool lighting_prepare(SceneLighting *out, const SceneLighting *source)
{
    if (!out || !source || !color_valid(source->ambient)
        || source->light_count>SCENE_LIGHT_CAPACITY
        || !isfinite(source->shadow_bias) || source->shadow_bias<=0)
        return false;
    SceneLighting result=*source;
    for (size_t i=0;i<result.light_count;++i) {
        if (!light_valid(&result.lights[i],false)) return false;
        if (result.lights[i].type==LIGHT_DIRECTIONAL
            && !vec3_normalize(result.lights[i].direction_to_light,&result.lights[i].direction_to_light)) return false;
    }
    *out=result;
    return true;
}
bool lighting_validate(const SceneLighting *lighting)
{
    if (!lighting || !color_valid(lighting->ambient)
        || lighting->light_count>SCENE_LIGHT_CAPACITY
        || !isfinite(lighting->shadow_bias) || lighting->shadow_bias<=0) return false;
    for (size_t i=0;i<lighting->light_count;++i)
        if (!light_valid(&lighting->lights[i],true)) return false;
    return true;
}
static bool point_direction(Vector3 from, Vector3 to, Vector3 *direction, float *distance)
{
    double x=(double)to.x-from.x, y=(double)to.y-from.y, z=(double)to.z-from.z;
    double length=sqrt(x*x+y*y+z*z);
    if (!isfinite(length) || length<=0 || length>FLT_MAX) return false;

    // calculated reciprocal only once instead
    //*direction=(Vector3){(float)(x/length),(float)(y/length),(float)(z/length)};
    double inverse_length = 1.0 / length;

    *direction = (Vector3){
        (float)(x * inverse_length),
        (float)(y * inverse_length),
        (float)(z * inverse_length)
    };

    *distance=(float)length;
    return true;
}
bool lighting_sample_light(const Light *light, Vector3 point, Vector3 normal, LightSample *out)
{
    if (!light || !out) return false;
    LightSample result={0};
    float attenuation=1;
    if (light->type==LIGHT_POINT) {
        if (!point_direction(point,light->position,&result.direction_to_light,&result.distance)
            || result.distance>=light->radius) return false;
        float edge=1-result.distance/light->radius;
        attenuation=edge*edge; /* Smooth finite-radius artistic falloff. */
    } else if (light->type==LIGHT_DIRECTIONAL) {
        result.direction_to_light=light->direction_to_light;
        result.distance=FLT_MAX;
    } else return false;
    float cosine=clamp_float(vec3_dot(normal,result.direction_to_light),0,1);
    float strength=light->intensity*attenuation*cosine;
    if (strength<=0) return false;
    result.irradiance=(Vector3){light->color.x*strength,light->color.y*strength,light->color.z*strength};
    *out=result;
    return true;
}
bool lighting_shadow_ray(const Light *light, Vector3 point, Vector3 normal, float bias, RaySegment *out)
{
    if (!light || !out || !isfinite(bias) || bias<=0) return false;
    RaySegment ray;
    ray.min_distance=bias*.1f;
    ray.origin=(Vector3){point.x+normal.x*bias,point.y+normal.y*bias,point.z+normal.z*bias};
    if (!finite_vec(ray.origin)) return false;
    if (light->type==LIGHT_POINT) {
        if (!point_direction(ray.origin,light->position,&ray.direction,&ray.max_distance)) return false;
        ray.max_distance-=bias;
        if (ray.max_distance<=ray.min_distance) return false;
    } else if (light->type==LIGHT_DIRECTIONAL) {
        ray.direction=light->direction_to_light;
        ray.max_distance=FLT_MAX;
    } else return false;
    *out=ray;
    return true;
}
bool lighting_lut_init(LightingLUT *lut)
{
    if (!lut) return false;
    for (unsigned i=0;i<256;++i) {
        float v=(float)i/255;
        lut->to_linear[i]=v<=.04045f ? v/12.92f : powf((v+.055f)/1.055f,2.4f);
    }
    for (unsigned i=0;i<=LIGHTING_ENCODE_STEPS;++i) {
        float v=(float)i/LIGHTING_ENCODE_STEPS;
        float encoded=v<=.0031308f ? v*12.92f : 1.055f*powf(v,1/2.4f)-.055f;
        lut->to_srgb[i]=(uint8_t)(clamp_float(encoded,0,1)*255+.5f);
    }
    return true;
}
