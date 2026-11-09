#include "shadow_maps.h"
#include "rasterizer.h"
#include "camera.h"
#include "lighting.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>

// not sure when youd need to change this
enum { CUBE_FACE_COUNT=6 };

typedef struct {
    DepthBuffer depth[CUBE_FACE_COUNT]; // depth buffer takes care of storage; allocation will need to be more dynamic to handle many lights
    CameraPixelBasis basis[CUBE_FACE_COUNT];
    Vector3 position;
    float radius;
    bool enabled;
} ShadowCube;

struct ShadowMaps {
    float *storage;
    ShadowCube cubes[SCENE_LIGHT_CAPACITY];
    size_t capacity,light_count;
    int face_size;
    float near_clip,bias,focal;
    bool ready;
};

//great candidate for multithreading or gpu
static float dot(Vector3 a,Vector3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
static bool finite_vec(Vector3 v) {return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}

ShadowMaps *shadow_maps_create(size_t capacity,int face_size,float near_clip)
{
    if(!capacity || capacity>SCENE_LIGHT_CAPACITY || face_size<4
       || !isfinite(near_clip) || near_clip<=0) return NULL;
    size_t side=(size_t)face_size;
    if(side>SIZE_MAX/side) return NULL;
    size_t face_pixels=side*side;
    size_t face_count=capacity*CUBE_FACE_COUNT;
    if(face_pixels>SIZE_MAX/face_count
       || face_pixels*face_count>SIZE_MAX/sizeof(float)) return NULL;
    ShadowMaps *maps=calloc(1,sizeof(*maps));
    if(!maps) return NULL;
    maps->storage=malloc(face_pixels*face_count*sizeof(float));
    if(!maps->storage) {free(maps);return NULL;}
    maps->capacity=capacity; maps->face_size=face_size; maps->near_clip=near_clip;
    maps->focal=(face_size-2)*.5f;
    for(size_t light=0;light<capacity;++light) for(size_t face=0;face<CUBE_FACE_COUNT;++face)
        maps->cubes[light].depth[face]=(DepthBuffer){
            maps->storage+(light*CUBE_FACE_COUNT+face)*face_pixels,face_size,face_size};
    return maps;
}


void shadow_maps_destroy(ShadowMaps *maps)
{
    if(!maps) return;
    // dont touch interior pointers, depth unit takes care of allocation
    free(maps->storage);free(maps);
}
bool shadow_maps_build(ShadowMaps *maps,RenderScratch *scratch,
    const SceneObject *objects,size_t object_count,const SceneLighting *lighting)
{
    if(!maps) return false;
    maps->ready=false;
    if(!scratch || !lighting_validate(lighting) || lighting->light_count>maps->capacity || (object_count && !objects)) return false;
    // validate light sources
    for(size_t i=0;i<lighting->light_count;++i)
        if(lighting->lights[i].casts_shadows && lighting->lights[i].type!=LIGHT_POINT) return false;

    // what is this?
    const float pi=3.14159265358979323846f;
    const float yaw[CUBE_FACE_COUNT]={pi*.5f,-pi*.5f,0,0,0,pi};
    const float pitch[CUBE_FACE_COUNT]={0,0,pi*.5f,-pi*.5f,0,0};

    maps->light_count=lighting->light_count; maps->bias=lighting->shadow_bias;
    for(size_t i=0;i<lighting->light_count;++i) {
        const Light *light=&lighting->lights[i];
        ShadowCube *cube=&maps->cubes[i];
        cube->enabled=light->casts_shadows;
        if(!cube->enabled) continue;
        cube->position=light->position; cube->radius=light->radius;
        for(size_t face=0;face<CUBE_FACE_COUNT;++face) {
            //one-pixel angular overlap protects face boundaries. lookup uses this exact projection, rather than assuming focal=size/2
            Camera view={light->position,yaw[face],pitch[face],maps->focal,maps->near_clip};
            if(!camera_build_pixel_basis(&cube->basis[face],&view,maps->face_size,maps->face_size) || !rasterizer_draw_depth_scene(&cube->depth[face],scratch,objects,object_count,&view)) return false;
        }
    }
    maps->ready=true;
    return true;
}

bool shadow_maps_occluded(const ShadowMaps *maps,size_t light_index,Vector3 point,Vector3 normal)
{
    // mess of awful code i know
    if(!maps || !maps->ready || light_index>=maps->light_count || !finite_vec(point) || !finite_vec(normal)) return false;

    const ShadowCube *cube=&maps->cubes[light_index];

    if(!cube->enabled) return false;

    Vector3 delta={point.x-cube->position.x,point.y-cube->position.y,point.z-cube->position.z};
    double distance2=(double)delta.x*delta.x+(double)delta.y*delta.y+(double)delta.z*delta.z;

    if(distance2>=(double)cube->radius*cube->radius) return false;

    delta.x+=normal.x*maps->bias; delta.y+=normal.y*maps->bias; delta.z+=normal.z*maps->bias;
    float ax=fabsf(delta.x),ay=fabsf(delta.y),az=fabsf(delta.z);
    size_t face;

    if(ax>=ay && ax>=az) face=delta.x>=0 ? 0 : 1;
    else if(ay>=az) face=delta.y>=0 ? 2 : 3;
    else face=delta.z>=0 ? 4 : 5;

    const CameraPixelBasis *basis=&cube->basis[face];

    float z=dot(delta,basis->forward);

    if(!isfinite(z) || z<maps->near_clip) return false;
    float inverse_z=1.0f/z;
    float sx=basis->center_x+maps->focal*dot(delta,basis->right)*inverse_z;
    float sy=basis->center_y-maps->focal*dot(delta,basis->up)*inverse_z;
    if(!isfinite(sx) || !isfinite(sy)) return false;

    int x=sx<=0 ? 0 : sx>=maps->face_size-1 ? maps->face_size-1 : (int)sx;
    int y=sy<=0 ? 0 : sy>=maps->face_size-1 ? maps->face_size-1 : (int)sy;
    const DepthBuffer *depth=&cube->depth[face];
    float stored_q=depth->values[(size_t)y*maps->face_size+x];
    if(stored_q<=0) return false;

    // compare to the receiver's plane at the sampled texel centre comparing only to the original pixel point causes self-shadowing on sloped floors
    float px=(x+.5f-basis->center_x)*basis->inverse_focal;
    float py=(basis->center_y-y-.5f)*basis->inverse_focal;
    Vector3 direction={
        basis->right.x*px+basis->up.x*py+basis->forward.x,
        basis->right.y*px+basis->up.y*py+basis->forward.y,
        basis->right.z*px+basis->up.z*py+basis->forward.z};
    float denominator=dot(normal,direction);
    if(fabsf(denominator)>1e-6f) {
        float plane_z=dot(normal,delta)/denominator;
        if(isfinite(plane_z) && plane_z>=maps->near_clip) z=plane_z;
    }
    // metric bias for positive depth, q*z>1 means the stored surface is closer; this avoids calculating another reciprocal
    float comparison_z=z-maps->bias;
    if(comparison_z<maps->near_clip) comparison_z=maps->near_clip;
    return stored_q*comparison_z>1.0f;
}
