#include "shadow_maps.h"
#include "rasterizer.h"
#include "camera.h"
#include "lighting.h"
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <float.h>
// load intrinsics only if SSE2 (how old is your machine?)
#if defined(__SSE2__) && !defined(RASTER_FORCE_SCALAR)
#include <emmintrin.h>
#endif

/*shadow_maps_visibility()
    prepare one receiver
    sample four taps
    blend visibility

shadow_maps_visibility4()
    prepare four receivers
    sample four taps across SIMD lanes
    blend four visibility values
    */

// A cubemap always has the six +/-X, +/-Y, +/-Z faces.
enum { CUBE_FACE_COUNT=6 };

typedef struct {
    DepthBuffer depth[CUBE_FACE_COUNT]; // Borrowed slices; ShadowMaps owns storage.
    CameraPixelBasis basis[CUBE_FACE_COUNT];
    Vector3 position;
    float radius;
    bool enabled;
} ShadowCube;

typedef struct {
    size_t light;
    size_t face;
} ShadowFaceJob;


struct ShadowMaps {
    float *storage;
    ShadowCube cubes[SCENE_LIGHT_CAPACITY];
    size_t capacity,light_count;

    // delegate shadow work to jobs for workers to pick up
    ShadowFaceJob jobs[SCENE_LIGHT_CAPACITY * CUBE_FACE_COUNT];
    size_t job_count;
    bool building;

    int face_size;
    float near_clip,bias,focal;
    bool ready;
};
//encapsulates parameters related to shadow reception
typedef struct {
    float numerator;
    float denominator_x;
    float denominator_y;
    float denominator_constant;
    float fallback_z;
} ShadowReceiverPlane;

typedef struct {
    const DepthBuffer *depth; // shadow face
    ShadowReceiverPlane plane;
    int x0, x1;
    int y0, y1;
    float fx, fy;
} ShadowReceiver;


// Scalar arithmetic stays inside a tile/packet job, rather than becoming a job itself.
static float dot(Vector3 a,Vector3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
static bool finite_vec(Vector3 v) {return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}

// pack shadow plane into SIMD vector
#if defined(__SSE2__) && !defined(RASTER_FORCE_SCALAR)
typedef struct {
    __m128 numerator, a, b, c, fallback_z;
} ShadowPlanePacket;

// perform OR on shadow mask
static __m128 shadow_select(__m128 mask, __m128 yes, __m128 no)
{
    return _mm_or_ps(_mm_and_ps(mask, yes), _mm_andnot_ps(mask, no));
}

#if defined(__SSE2__) && !defined(RASTER_FORCE_SCALAR)
/* Copilot

The gather_component function retrieves the specified component (x, y, or z) from an array of four Vector3 structures 
based on the given axis index, and returns the components as a SIMD vector of type __m128. 
It uses conditional expressions to select the appropriate component for each vector in the array before loading them into the SIMD register.*/
static __m128 gather_component(const Vector3 v[4],int axis)
{
    float a[4];for(int i=0;i<4;++i)a[i]=axis==0?v[i].x:axis==1?v[i].y:v[i].z;
    return _mm_loadu_ps(a);
}
// should speak for itself
static __m128 vector_dot(__m128 x,__m128 y,__m128 z,Vector3 axis)
{
    return _mm_add_ps(_mm_add_ps(_mm_mul_ps(x,_mm_set1_ps(axis.x)),_mm_mul_ps(y,_mm_set1_ps(axis.y))),_mm_mul_ps(z,_mm_set1_ps(axis.z)));
}
/* Copilot

The packet_finite function is a static function that takes a single __m128 vector as input and returns a __m128 vector. 
It checks if the elements of the input vector are finite by comparing them to positive infinity and returns a mask indicating which elements are finite.
*/
static __m128 packet_finite(__m128 v)
{
    return _mm_cmple_ps(_mm_andnot_ps(_mm_set1_ps(-0.0f),v),_mm_set1_ps(FLT_MAX));
}
#endif

/* Copilot

computes shadow values for four shadow receivers based on their respective coordinates and depth values, utilizing SIMD operations for performance. 
It evaluates whether each receiver is valid, calculates a denominator using plane parameters, 
and determines the final shadow value while considering clipping and bias adjustments.*/
static __m128 shadow_tap4(const ShadowMaps *maps, const ShadowReceiver receivers[4],const ShadowPlanePacket *plane, unsigned valid, unsigned tap)
{
    float xs[4] = {0}, ys[4] = {0}, samples[4] = {0};

    // each lane may read a different cubemap face but only valid lanes read memory
    for (unsigned lane = 0; lane < 4; ++lane) {
        if (!(valid & (1u << lane))) continue;
        const ShadowReceiver *r = &receivers[lane];
        int x = (tap & 1u) ? r->x1 : r->x0;
        int y = (tap & 2u) ? r->y1 : r->y0;
        xs[lane] = (float)x;
        ys[lane] = (float)y;
        samples[lane] = r->depth->values[(size_t)y * r->depth->width + x];
    }
    // a lot of annoying spaghetti code but worth the speeed up
    __m128 denominator = _mm_add_ps(_mm_add_ps(_mm_mul_ps(plane->a, _mm_loadu_ps(xs)),_mm_mul_ps(plane->b, _mm_loadu_ps(ys))), plane->c);

    __m128 absolute = _mm_andnot_ps(_mm_set1_ps(-0.0f), denominator);
    __m128 use_plane = _mm_cmpgt_ps(absolute, _mm_set1_ps(1e-6f));
    __m128 safe = shadow_select(use_plane, denominator, _mm_set1_ps(1));
    __m128 plane_z = _mm_div_ps(plane->numerator, safe);
    use_plane = _mm_and_ps(use_plane,_mm_and_ps(packet_finite(plane_z),_mm_cmpge_ps(plane_z, _mm_set1_ps(maps->near_clip))));

    __m128 z = shadow_select(use_plane, plane_z, plane->fallback_z);
    __m128 comparison = _mm_max_ps(_mm_set1_ps(maps->near_clip), _mm_sub_ps(z, _mm_set1_ps(maps->bias)));
    __m128 q = _mm_loadu_ps(samples);
    __m128 blocked = _mm_and_ps(_mm_cmpgt_ps(q, _mm_setzero_ps()),_mm_cmpgt_ps(_mm_mul_ps(q, comparison), _mm_set1_ps(1)));

    return _mm_andnot_ps(blocked, _mm_set1_ps(1));
}
#endif




/* Copilot

The shadow_receiver_plane function computes a ShadowReceiverPlane structure based on the provided camera pixel basis, 
a delta vector, a normal vector, and a receiver depth value. 
It calculates the numerator and denominators for the plane equation using dot products and the inverse focal length from the camera basis.*/
static ShadowReceiverPlane shadow_receiver_plane(const CameraPixelBasis *basis,Vector3 delta,Vector3 normal,float receiver_z)
{
    float nr = dot(normal, basis->right);
    float nu = dot(normal, basis->up);
    float nf = dot(normal, basis->forward);
    float inverse_focal = basis->inverse_focal;

    return (ShadowReceiverPlane){
        .numerator = dot(normal, delta),

        .denominator_x = nr * inverse_focal,
        .denominator_y = -nu * inverse_focal,

        .denominator_constant = nf + nr * ((0.5f - basis->center_x) * inverse_focal) + nu * ((basis->center_y - 0.5f) * inverse_focal),
        .fallback_z = receiver_z
    };
}





/* Copilot

The shadow_prepare_receiver function prepares a shadow receiver by calculating the projected coordinates
 and depth based on the provided light index, point, and normal vectors, while ensuring the validity of the input parameters and the shadow map's state. 
It updates the ShadowReceiver structure with the computed values, including depth and texture coordinates
returns a boolean indicating success or failure.*/
static bool shadow_prepare_receiver(const ShadowMaps *maps,size_t light_index,Vector3 point,Vector3 normal,ShadowReceiver *out)
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

    // projection uses pixel-edge coordinates
    // subtract 0.5 so integer coordinates identify texel center
    float tx = sx - 0.5f;
    float ty = sy - 0.5f;

    float maximum = (float)(maps->face_size - 1);

    tx = fmaxf(0.0f, fminf(tx, maximum));
    ty = fmaxf(0.0f, fminf(ty, maximum));

    int x0 = (int)floorf(tx);
    int y0 = (int)floorf(ty);

    int x1 = x0 + 1;
    int y1 = y0 + 1;

    if (x1 >= maps->face_size)
        x1 = maps->face_size - 1;

    if (y1 >= maps->face_size)
        y1 = maps->face_size - 1;

    float fx = tx - (float)x0;
    float fy = ty - (float)y0;

    // add to shadow receiver class
    *out = (ShadowReceiver){
        .depth = &cube->depth[face],
        .plane = shadow_receiver_plane(basis, delta, normal, z),
        .x0 = x0,
        .x1 = x1,
        .y0 = y0,
        .y1 = y1,
        .fx = fx,
        .fy = fy
    };

    return true;
}



ShadowMaps *shadow_maps_create(size_t capacity,int face_size,float near_clip)
{
    if(!capacity || capacity>SCENE_LIGHT_CAPACITY || face_size<4 || !isfinite(near_clip) || near_clip<=0) return NULL;
    size_t side=(size_t)face_size;
    if(side>SIZE_MAX/side) return NULL;
    size_t face_pixels=side*side;
    size_t face_count=capacity*CUBE_FACE_COUNT;
    if(face_pixels>SIZE_MAX/face_count || face_pixels*face_count>SIZE_MAX/sizeof(float)) return NULL;
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
    // Free the backing allocation once; individual faces only borrow it.
    free(maps->storage);free(maps);
}

/* OLD IMPLEMENTATION
bool shadow_maps_build(ShadowMaps *maps,RenderScratch *scratch,
    const SceneObject *objects,size_t object_count,const SceneLighting *lighting)
{
    if(!maps) return false;
    maps->ready=false;
    if(!scratch || !lighting_validate(lighting) || lighting->light_count>maps->capacity || (object_count && !objects)) return false;
    // validate light sources
    for(size_t i=0;i<lighting->light_count;++i)
        if(lighting->lights[i].casts_shadows && lighting->lights[i].type!=LIGHT_POINT) return false;

    // Orientations matching the engine camera: +X,-X,+Y,-Y,+Z,-Z.
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
*/



bool shadow_maps_begin_build(ShadowMaps *maps, const SceneLighting *lighting)
{
    if (!maps) return false;
    maps->ready = false;
    maps->building = false;
    maps->job_count = 0;
    if (!lighting_validate(lighting) || lighting->light_count > maps->capacity)
        return false;
    for (size_t i = 0; i < lighting->light_count; ++i)
        if (lighting->lights[i].casts_shadows && lighting->lights[i].type != LIGHT_POINT)
            return false;

    maps->light_count = lighting->light_count;
    maps->bias = lighting->shadow_bias;
    for (size_t i = 0; i < lighting->light_count; ++i) {
        const Light *light = &lighting->lights[i];
        ShadowCube *cube = &maps->cubes[i];
        cube->enabled = light->casts_shadows;
        if (!cube->enabled) continue;
        cube->position = light->position;
        cube->radius = light->radius;
        for (size_t face = 0; face < CUBE_FACE_COUNT; ++face)
            maps->jobs[maps->job_count++] = (ShadowFaceJob){i, face};
    }
    maps->building = true;
    return true;
}

size_t shadow_maps_job_count(const ShadowMaps *maps)
{
    return maps && maps->building ? maps->job_count : 0;
}

bool shadow_maps_render_job(ShadowMaps *maps, size_t job, RenderScratch *scratch,const SceneObject *objects, size_t object_count)
{
    if (!maps || !maps->building || !scratch || job >= maps->job_count || (object_count && !objects)) return false;
    ShadowFaceJob entry = maps->jobs[job];
    ShadowCube *cube = &maps->cubes[entry.light];
    const float pi = 3.14159265358979323846f;
    const float yaw[CUBE_FACE_COUNT] = {pi*.5f, -pi*.5f, 0, 0, 0, pi};
    const float pitch[CUBE_FACE_COUNT] = {0, 0, pi*.5f, -pi*.5f, 0, 0};
    Camera view = {
        cube->position, yaw[entry.face], pitch[entry.face],
        maps->focal, maps->near_clip
    };
    return camera_build_pixel_basis(&cube->basis[entry.face], &view,maps->face_size, maps->face_size) 
    && rasterizer_draw_depth_scene(&cube->depth[entry.face], scratch,objects, object_count, &view);
}


bool shadow_maps_finish_build(ShadowMaps *maps, bool succeeded)
{
    if (!maps || !maps->building) return false;
    maps->building = false;
    maps->ready = succeeded;
    return succeeded;
}





// serial shadow map builder
/* Copilot

The shadow_maps_build function is responsible for constructing shadow maps by initiating the build process, 
rendering jobs for each object in the scene, and finalizing the build, returning a boolean indicating success or failure. 
It takes parameters for shadow maps, rendering scratch data, an array of scene objects, 
the count of those objects, and scene lighting information.*/
bool shadow_maps_build(ShadowMaps *maps, RenderScratch *scratch,const SceneObject *objects, size_t object_count,const SceneLighting *lighting)
{
    if (!shadow_maps_begin_build(maps, lighting)) return false;
    bool succeeded = scratch && (!object_count || objects);
    if (succeeded)
        for (size_t job = 0; job < shadow_maps_job_count(maps); ++job)
            if (!shadow_maps_render_job(maps, job, scratch, objects, object_count)) {
                succeeded = false;
                break;
            }
    return shadow_maps_finish_build(maps, succeeded);
}





// delta is the already biased light-to-receiver vector
// x and y -> valid indices on face
/* Copilot
 Copilot

The shadow_texel_visibility function calculates the visibility of a texel in shadow mapping by comparing the stored depth value 
at specified coordinates with a computed receiver depth based on the shadow receiver plane's parameters. 
It returns a float indicating the visibility, where a value of 0.0f signifies full occlusion and 1.0f indicates full visibility.
*/
static float shadow_texel_visibility(const ShadowMaps *maps,const DepthBuffer *depth,const ShadowReceiverPlane *plane,int x,int y)
{
    float stored_q = depth->values[(size_t)y * depth->width + x];

    if (stored_q <= 0.0f)
        return 1.0f;

    float denominator = plane->denominator_x * (float)x + plane->denominator_y * (float)y + plane->denominator_constant;
    float receiver_z = plane->fallback_z;

    if (fabsf(denominator) > 1e-6f) {
        float plane_z = plane->numerator / denominator;

        if (isfinite(plane_z) && plane_z >= maps->near_clip)
            receiver_z = plane_z;
    }

    float comparison_z = receiver_z - maps->bias;

    if (comparison_z < maps->near_clip)
        comparison_z = maps->near_clip;

    return stored_q * comparison_z > 1.0f ? 0.0f : 1.0f;
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


/* Copilot

The shadow_maps_occluded4 function determines which of the four specified points are occluded by shadows in a given shadow map, 
utilizing SIMD operations for performance when available. 
It checks various conditions such as the validity of inputs and the readiness of shadow maps, 
and it returns a bitmask indicating which points are occluded.*/
unsigned shadow_maps_occluded4(const ShadowMaps *maps,size_t index,const Vector3 points[4],const Vector3 normals[4],unsigned active)
{
    active&=15;
    if(!active || !maps || !maps->ready || index>=maps->light_count || !points || !normals) return 0;
    const ShadowCube *cube=&maps->cubes[index];if(!cube->enabled) return 0;
#if defined(__SSE2__) && !defined(RASTER_FORCE_SCALAR)
    Vector3 deltas[4]={{0}};int common=-1;bool diverged=false;
    for(int lane=0;lane<4;++lane) if(active&(1u<<lane)) {
        if(!finite_vec(points[lane]) || !finite_vec(normals[lane])) {active&=~(1u<<lane);continue;}
        Vector3 d={points[lane].x-cube->position.x,points[lane].y-cube->position.y,points[lane].z-cube->position.z};
        double distance2=(double)d.x*d.x+(double)d.y*d.y+(double)d.z*d.z;
        if(distance2>=(double)cube->radius*cube->radius) {active&=~(1u<<lane);continue;}
        d.x+=normals[lane].x*maps->bias;d.y+=normals[lane].y*maps->bias;d.z+=normals[lane].z*maps->bias;
        float ax=fabsf(d.x),ay=fabsf(d.y),az=fabsf(d.z);int face;
        if(ax>=ay && ax>=az)face=d.x>=0?0:1;else if(ay>=az)face=d.y>=0?2:3;else face=d.z>=0?4:5;
        if(common<0)common=face;else if(common!=face)diverged=true;
        deltas[lane]=d;
    }
    if(!active) return 0;
    if(!diverged) {
        const CameraPixelBasis *basis=&cube->basis[common];
        __m128 dx=gather_component(deltas,0),dy=gather_component(deltas,1),dz=gather_component(deltas,2);
        __m128 z=vector_dot(dx,dy,dz,basis->forward);
        __m128 valid=_mm_and_ps(packet_finite(z),_mm_cmpge_ps(z,_mm_set1_ps(maps->near_clip)));
        __m128 safe_z=_mm_or_ps(_mm_and_ps(valid,z),_mm_andnot_ps(valid,_mm_set1_ps(1)));
        __m128 inverse=_mm_div_ps(_mm_set1_ps(1),safe_z);
        __m128 sx=_mm_add_ps(_mm_set1_ps(basis->center_x),_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(maps->focal),vector_dot(dx,dy,dz,basis->right)),inverse));
        __m128 sy=_mm_sub_ps(_mm_set1_ps(basis->center_y),_mm_mul_ps(_mm_mul_ps(_mm_set1_ps(maps->focal),vector_dot(dx,dy,dz,basis->up)),inverse));
        valid=_mm_and_ps(valid,_mm_and_ps(packet_finite(sx),packet_finite(sy)));
        __m128 maximum=_mm_set1_ps((float)(maps->face_size-1));
        sx=_mm_min_ps(maximum,_mm_max_ps(_mm_setzero_ps(),sx));
        sy=_mm_min_ps(maximum,_mm_max_ps(_mm_setzero_ps(),sy));
        __m128i ix=_mm_cvttps_epi32(sx),iy=_mm_cvttps_epi32(sy);
        int x[4],y[4];_mm_storeu_si128((__m128i*)x,ix);_mm_storeu_si128((__m128i*)y,iy);
        float sampled[4]={0};
        unsigned valid_bits=active&(unsigned)_mm_movemask_ps(valid);
        const DepthBuffer *depth=&cube->depth[common];
        for(int lane=0;lane<4;++lane)if(valid_bits&(1u<<lane))sampled[lane]=depth->values[(size_t)y[lane]*maps->face_size+x[lane]];
        __m128 px=_mm_mul_ps(_mm_sub_ps(_mm_add_ps(_mm_cvtepi32_ps(ix),_mm_set1_ps(.5f)),_mm_set1_ps(basis->center_x)),_mm_set1_ps(basis->inverse_focal));
        __m128 py=_mm_mul_ps(_mm_sub_ps(_mm_sub_ps(_mm_set1_ps(basis->center_y),_mm_cvtepi32_ps(iy)),_mm_set1_ps(.5f)),_mm_set1_ps(basis->inverse_focal));
        __m128 rx=_mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(basis->right.x),px),_mm_mul_ps(_mm_set1_ps(basis->up.x),py)),_mm_set1_ps(basis->forward.x));
        __m128 ry=_mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(basis->right.y),px),_mm_mul_ps(_mm_set1_ps(basis->up.y),py)),_mm_set1_ps(basis->forward.y));
        __m128 rz=_mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(basis->right.z),px),_mm_mul_ps(_mm_set1_ps(basis->up.z),py)),_mm_set1_ps(basis->forward.z));
        __m128 nx=gather_component(normals,0),ny=gather_component(normals,1),nz=gather_component(normals,2);
        __m128 denominator=_mm_add_ps(_mm_add_ps(_mm_mul_ps(nx,rx),_mm_mul_ps(ny,ry)),_mm_mul_ps(nz,rz));
        __m128 use_plane=_mm_cmpgt_ps(_mm_andnot_ps(_mm_set1_ps(-0.0f),denominator),_mm_set1_ps(1e-6f));
        __m128 safe_denominator=_mm_or_ps(_mm_and_ps(use_plane,denominator),_mm_andnot_ps(use_plane,_mm_set1_ps(1)));
        __m128 numerator=_mm_add_ps(_mm_add_ps(_mm_mul_ps(nx,dx),_mm_mul_ps(ny,dy)),_mm_mul_ps(nz,dz));
        __m128 plane_z=_mm_div_ps(numerator,safe_denominator);
        use_plane=_mm_and_ps(use_plane,_mm_and_ps(packet_finite(plane_z),_mm_cmpge_ps(plane_z,_mm_set1_ps(maps->near_clip))));
        z=_mm_or_ps(_mm_and_ps(use_plane,plane_z),_mm_andnot_ps(use_plane,z));
        __m128 comparison=_mm_max_ps(_mm_set1_ps(maps->near_clip),_mm_sub_ps(z,_mm_set1_ps(maps->bias)));
        __m128 q=_mm_loadu_ps(sampled);
        return valid_bits&(unsigned)_mm_movemask_ps(_mm_and_ps(_mm_cmpgt_ps(q,_mm_setzero_ps()),_mm_cmpgt_ps(_mm_mul_ps(q,comparison),_mm_set1_ps(1))));
    }
#endif
    unsigned result=0;
    for(int lane=0;lane<4;++lane)if((active&(1u<<lane)) && shadow_maps_occluded(maps,index,points[lane],normals[lane]))result|=1u<<lane;
    return result;
}


float shadow_maps_visibility(const ShadowMaps *maps,size_t light_index,Vector3 point,Vector3 normal)
{
    // take from encapsulated receiver
    ShadowReceiver receiver;

    if (!shadow_prepare_receiver(maps, light_index, point, normal, &receiver))return 1.0f;

    const ShadowReceiver *r = &receiver;

    // scalar approach
    float v00 = shadow_texel_visibility(maps, r->depth, &r->plane, r->x0, r->y0);
    float v10 = shadow_texel_visibility(maps, r->depth, &r->plane, r->x1, r->y0);
    float v01 = shadow_texel_visibility(maps, r->depth, &r->plane, r->x0, r->y1);
    float v11 = shadow_texel_visibility(maps, r->depth, &r->plane, r->x1, r->y1);

    float top = v00 + r->fx * (v10 - v00);
    float bottom = v01 + r->fx * (v11 - v01);

    return top + r->fy * (bottom - top);
}


// parallelized for SIMD
/* Copilot

The shadow_maps_visibility4 function computes the visibility of shadow maps for up to four light sources in parallel 
using SIMD instructions, optimizing performance through vectorized operations. It prepares shadow receivers based on the provided points and normals, 
applies a bilinear PCF filter, and updates the visibility array accordingly, while falling back to a scalar approach if SIMD is not available.*/
void shadow_maps_visibility4(const ShadowMaps *maps, size_t light_index,const Vector3 points[4], const Vector3 normals[4],unsigned active_mask, float visibility[4])
{
    unsigned active = active_mask & 15u;
    #if defined(__SSE2__) && !defined(RASTER_FORCE_SCALAR) // seriously get a new cpu
    // prepare receivers. each receiver gets a lane to process
    ShadowReceiver receivers[4] = {0};
    unsigned valid = 0;
    for (unsigned lane = 0; lane < 4; ++lane) {
        visibility[lane] = (active & (1u << lane)) ? 1.0f : 0.0f;
        if ((active & (1u << lane)) && shadow_prepare_receiver(maps, light_index, points[lane], normals[lane], &receivers[lane]))
            valid |= 1u << lane;
    }
    if (!valid) return;
    // pack coefficients
    ShadowPlanePacket plane = {
        .numerator = _mm_set_ps(receivers[3].plane.numerator, receivers[2].plane.numerator,receivers[1].plane.numerator, receivers[0].plane.numerator),
        .a = _mm_set_ps(receivers[3].plane.denominator_x, receivers[2].plane.denominator_x,receivers[1].plane.denominator_x, receivers[0].plane.denominator_x),
        .b = _mm_set_ps(receivers[3].plane.denominator_y, receivers[2].plane.denominator_y,receivers[1].plane.denominator_y, receivers[0].plane.denominator_y),
        .c = _mm_set_ps(receivers[3].plane.denominator_constant, receivers[2].plane.denominator_constant,receivers[1].plane.denominator_constant, receivers[0].plane.denominator_constant),
        .fallback_z = _mm_set_ps(receivers[3].plane.fallback_z, receivers[2].plane.fallback_z,receivers[1].plane.fallback_z, receivers[0].plane.fallback_z)
    };

    __m128 fx = _mm_set_ps(receivers[3].fx, receivers[2].fx, receivers[1].fx, receivers[0].fx);
    __m128 fy = _mm_set_ps(receivers[3].fy, receivers[2].fy, receivers[1].fy, receivers[0].fy);
    __m128 v00 = shadow_tap4(maps, receivers, &plane, valid, 0);
    __m128 v10 = shadow_tap4(maps, receivers, &plane, valid, 1);
    __m128 v01 = shadow_tap4(maps, receivers, &plane, valid, 2);
    __m128 v11 = shadow_tap4(maps, receivers, &plane, valid, 3);
    // blend receivers 
    __m128 top = _mm_add_ps(v00, _mm_mul_ps(fx, _mm_sub_ps(v10, v00)));
    __m128 bottom = _mm_add_ps(v01, _mm_mul_ps(fx, _mm_sub_ps(v11, v01)));

    float filtered[4];
    // pcf filter bilinear
    _mm_storeu_ps(filtered, _mm_add_ps(top, _mm_mul_ps(fy, _mm_sub_ps(bottom, top))));
    for (unsigned lane = 0; lane < 4; ++lane)
        if (valid & (1u << lane)) visibility[lane] = filtered[lane];
    #else
        for (unsigned lane = 0; lane < 4; ++lane) {
            visibility[lane] = (active & (1u << lane))
                ? shadow_maps_visibility(maps, light_index, points[lane], normals[lane]) : 0.0f;
        }
    #endif
}
