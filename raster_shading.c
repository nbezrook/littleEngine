#include "raster_scene_internal.h"
#include <float.h>
#include <math.h>
/* Copilot

The sample_albedo function is a static utility that retrieves the color value from a texture associated with 
a PreparedTriangle based on the provided texture coordinates u and v. 
If either coordinate is not finite, it returns the triangle's default color instead.*/
static uint32_t sample_albedo(const PreparedTriangle *t,float u,float v)
{
    if(!isfinite(u) || !isfinite(v)) return t->color;
    return texture_sample(t->texture,u,v);
}

/* Copilot

The raster_shade_pixel function computes the color of a pixel in a rasterized scene by determining the pixel's world position, 
sampling texture color, and applying lighting effects from multiple light sources, including ambient and shadow considerations. 
It takes a RasterScene, a PreparedTriangle, pixel coordinates, and a depth parameter as inputs, returning the final color as a uint32_t.*/

uint32_t raster_shade_pixel(const RasterScene *s,const PreparedTriangle *t,int x,int y,float q)
{
    float z=1.0f/q;
    Vector3 point=camera_pixel_world(&s->basis,x+.5f,y+.5f,z);
    uint32_t texel=t->color;
    if(t->texture) texel=sample_albedo(t,raster_plane_at(t->uq,x+.5f,y+.5f)*z,raster_plane_at(t->vq,x+.5f,y+.5f)*z);
    Vector3 total=s->lighting.ambient;
    for(size_t i=0;i<s->lighting.light_count;++i) {
        const Light *light=&s->lighting.lights[i];LightSample sample;
        if(!lighting_sample_light(light,point,t->normal,&sample)) continue;
        float visibility = 1.0f;
        // visibility check
        if (light->casts_shadows && s->maps) {
            visibility = shadow_maps_visibility(
                s->maps, i, point, t->normal);
        }

        total.x += sample.irradiance.x * visibility;
        total.y += sample.irradiance.y * visibility;
        total.z += sample.irradiance.z * visibility;
    }
    return lighting_apply_rgb(texel,total,s->lut);
}

#if RASTER_HAS_SIMD
/* Copilot

The packet_plane function computes a new __m128 vector by applying a linear transformation based on the properties of 
a RasterPlane structure and a given vector x, scaling it by the plane's coefficient a 
and adjusting it with the product of y and the plane's coefficient c. 
This function utilizes SIMD operations for efficient parallel processing of floating-point values.*/
static __m128 packet_plane(RasterPlane p,__m128 x,float y)
{
    return _mm_add_ps(_mm_mul_ps(_mm_set1_ps(p.a),x),_mm_set1_ps(p.b*y+p.c));
}
/* Copilot

The packet_component function computes a new 3D vector by combining a given origin with directional components defined by the right,
 up, and forward parameters, using SIMD operations on the input vectors x, y, and z. 
It returns a packed __m128 vector that represents the resulting position in 3D space.*/
static __m128 packet_component(float origin,float right,float up,float forward,__m128 x,__m128 y,__m128 z)
{
    __m128 d=_mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(right),x),_mm_mul_ps(_mm_set1_ps(up),y)),_mm_set1_ps(forward));
    return _mm_add_ps(_mm_set1_ps(origin),_mm_mul_ps(d,z));
}

typedef struct {__m128 x,y,z,distance;unsigned valid;} PacketDirection;
//  2 doubles at a time preserve the scalar light sampler's normalization precision no approximate reciprocal/square root or fast-math is required
/* Copilot

The packet_direction function calculates the direction and distance from a given light source 
(represented by a Vector3 object) to two sets of 3D points defined by the __m128 parameters px, py, and pz. 
It returns a PacketDirection structure containing the computed direction vectors
 and a validity mask indicating which calculations were successful.*/
static PacketDirection packet_direction(Vector3 light,__m128 px,__m128 py,__m128 pz)
{
    __m128 result_x[2],result_y[2],result_z[2],distance[2];unsigned valid=0;
    for(int pair=0;pair<2;++pair) {
        __m128 x=pair?_mm_movehl_ps(px,px):px,y=pair?_mm_movehl_ps(py,py):py,z=pair?_mm_movehl_ps(pz,pz):pz;
        __m128d dx=_mm_sub_pd(_mm_set1_pd(light.x),_mm_cvtps_pd(x));
        __m128d dy=_mm_sub_pd(_mm_set1_pd(light.y),_mm_cvtps_pd(y));
        __m128d dz=_mm_sub_pd(_mm_set1_pd(light.z),_mm_cvtps_pd(z));
        __m128d length=_mm_sqrt_pd(_mm_add_pd(_mm_add_pd(_mm_mul_pd(dx,dx),_mm_mul_pd(dy,dy)),_mm_mul_pd(dz,dz)));
        __m128d good=_mm_and_pd(_mm_cmpgt_pd(length,_mm_setzero_pd()),_mm_cmple_pd(length,_mm_set1_pd(FLT_MAX)));
        valid|=(unsigned)_mm_movemask_pd(good)<<(pair*2);
        __m128d safe=_mm_or_pd(_mm_and_pd(good,length),_mm_andnot_pd(good,_mm_set1_pd(1)));
        __m128d inverse=_mm_div_pd(_mm_set1_pd(1),safe);
        //
        result_x[pair]=_mm_cvtpd_ps(_mm_and_pd(good,_mm_mul_pd(dx,inverse)));
        result_y[pair]=_mm_cvtpd_ps(_mm_and_pd(good,_mm_mul_pd(dy,inverse)));
        result_z[pair]=_mm_cvtpd_ps(_mm_and_pd(good,_mm_mul_pd(dz,inverse)));
        distance[pair]=_mm_cvtpd_ps(_mm_and_pd(good,length));
    }
    return (PacketDirection){_mm_movelh_ps(result_x[0],result_x[1]),_mm_movelh_ps(result_y[0],result_y[1]),
        _mm_movelh_ps(result_z[0],result_z[1]),_mm_movelh_ps(distance[0],distance[1]),valid};
}
/* Copilot

The bit_mask function takes an unsigned integer bits as input and returns a __m128 type, 
which represents a 128-bit SIMD vector. It sets the elements of the vector based on the individual bits of the input, 
where each bit determines if the corresponding element is set to -1 (if the bit is 1) or 0 (if the bit is 0).*/
static __m128 bit_mask(unsigned bits)
{
    return _mm_castsi128_ps(_mm_set_epi32(bits&8?-1:0,bits&4?-1:0,bits&2?-1:0,bits&1?-1:0));
}
#endif
/* Copilot

The raster_shade_four function performs shading calculations for four pixels in a rasterized scene, 
utilizing SIMD instructions for efficiency when available. 
It computes the final color for each pixel based on lighting, texture, and geometry information, 
and stores the results in the provided output array.*/
void raster_shade_four(const RasterScene *s,const PreparedTriangle *t,int x,int y,const float q[4],uint32_t out[4])
{
#if RASTER_HAS_SIMD // maybe update your cpu
    __m128 xs=_mm_set_ps(x+3.5f,x+2.5f,x+1.5f,x+.5f);
    __m128 z=_mm_div_ps(_mm_set1_ps(1),_mm_loadu_ps(q));
    __m128 vx=_mm_mul_ps(_mm_sub_ps(xs,_mm_set1_ps(s->basis.center_x)),_mm_set1_ps(s->basis.inverse_focal));
    __m128 vy=_mm_set1_ps((s->basis.center_y-y-.5f)*s->basis.inverse_focal);
    __m128 px=packet_component(s->basis.origin.x,s->basis.right.x,s->basis.up.x,s->basis.forward.x,vx,vy,z);
    __m128 py=packet_component(s->basis.origin.y,s->basis.right.y,s->basis.up.y,s->basis.forward.y,vx,vy,z);
    __m128 pz=packet_component(s->basis.origin.z,s->basis.right.z,s->basis.up.z,s->basis.forward.z,vx,vy,z);
    float world_x[4],world_y[4],world_z[4],u[4],v[4];
    _mm_storeu_ps(world_x,px);_mm_storeu_ps(world_y,py);_mm_storeu_ps(world_z,pz);
    if(t->texture) {
        _mm_storeu_ps(u,_mm_mul_ps(packet_plane(t->uq,xs,y+.5f),z));
        _mm_storeu_ps(v,_mm_mul_ps(packet_plane(t->vq,xs,y+.5f),z));
    }
    Vector3 points[4],normals[4];uint32_t texels[4];
    for(int lane=0;lane<4;++lane) {
        points[lane]=(Vector3){world_x[lane],world_y[lane],world_z[lane]};normals[lane]=t->normal;
        texels[lane]=t->texture?sample_albedo(t,u[lane],v[lane]):t->color;
    }
    __m128 total_x=_mm_set1_ps(s->lighting.ambient.x),total_y=_mm_set1_ps(s->lighting.ambient.y),total_z=_mm_set1_ps(s->lighting.ambient.z);
    for(size_t i=0;i<s->lighting.light_count;++i) {
        const Light *light=&s->lighting.lights[i];PacketDirection d;
        __m128 attenuation=_mm_set1_ps(1);unsigned active=15;
        if(light->type==LIGHT_POINT) {
            d=packet_direction(light->position,px,py,pz);
            active=d.valid&(unsigned)_mm_movemask_ps(_mm_cmplt_ps(d.distance,_mm_set1_ps(light->radius)));
            __m128 edge=_mm_sub_ps(_mm_set1_ps(1),_mm_div_ps(d.distance,_mm_set1_ps(light->radius)));
            attenuation=_mm_mul_ps(edge,edge);
        } else {
            d=(PacketDirection){_mm_set1_ps(light->direction_to_light.x),_mm_set1_ps(light->direction_to_light.y),
                _mm_set1_ps(light->direction_to_light.z),_mm_set1_ps(FLT_MAX),15};
        }
        __m128 cosine=_mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(t->normal.x),d.x),_mm_mul_ps(_mm_set1_ps(t->normal.y),d.y)),_mm_mul_ps(_mm_set1_ps(t->normal.z),d.z));
        cosine=_mm_min_ps(_mm_set1_ps(1),_mm_max_ps(_mm_setzero_ps(),cosine));
        __m128 strength=_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(light->intensity),attenuation),cosine);
        active&=(unsigned)_mm_movemask_ps(_mm_cmpgt_ps(strength,_mm_setzero_ps()));
        if(!active) continue;

        // have shadows cast to lanes to allow for pcf through SIMD
        if (light->casts_shadows && s->maps) {
            float visibility[4];
            shadow_maps_visibility4(s->maps, i, points, normals, active, visibility);
            strength = _mm_mul_ps(strength, _mm_loadu_ps(visibility));
        }
        
        __m128 mask=bit_mask(active);
        total_x=_mm_add_ps(total_x,_mm_and_ps(mask,_mm_mul_ps(_mm_set1_ps(light->color.x),strength)));
        total_y=_mm_add_ps(total_y,_mm_and_ps(mask,_mm_mul_ps(_mm_set1_ps(light->color.y),strength)));
        total_z=_mm_add_ps(total_z,_mm_and_ps(mask,_mm_mul_ps(_mm_set1_ps(light->color.z),strength)));
    }
    float r[4],g[4],b[4];_mm_storeu_ps(r,total_x);_mm_storeu_ps(g,total_y);_mm_storeu_ps(b,total_z);
    for(int lane=0;lane<4;++lane)out[lane]=lighting_apply_rgb(texels[lane],(Vector3){r[lane],g[lane],b[lane]},s->lut);
#else
    for(int lane=0;lane<4;++lane)out[lane]=raster_shade_pixel(s,t,x+lane,y,q[lane]);

    #endif
}
