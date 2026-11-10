// dont expose preperation of raster outside of process
// encapsulation and delegation 

#ifndef LITTLEENGINE_RASTER_SCENE_INTERNAL_H
#define LITTLEENGINE_RASTER_SCENE_INTERNAL_H
#include "raster_scene.h"
#if defined(__SSE2__) && !defined(RASTER_FORCE_SCALAR)
#define RASTER_HAS_SIMD 1
#include <emmintrin.h>
#else
// check if its been defined to prevent clashing
#define RASTER_HAS_SIMD 0
#endif


enum { RASTER_TILE_SIZE=32,RASTER_SUBPIXEL=256 };

// structs (classes really) keeping track of what job does what
typedef struct {float a,b,c;} RasterPlane;
typedef struct {int64_t a,b,c;bool inclusive;} RasterEdge;
typedef struct {
    RasterPlane q,uq,vq;
    RasterEdge edges[3];
    int x0,y0,x1,y1;
    Vector3 normal;
    uint32_t color;
    const CPUTexture *texture;
} PreparedTriangle;


struct RasterScene {
    int width,height,tiles_x,tiles_y;
    size_t tile_count,triangle_capacity,reference_capacity,triangle_count,reference_count;
    PreparedTriangle *triangles;
    uint32_t *references,*winners;
    size_t *offsets,*counts,*cursors;
    uint64_t *shaded;
    RenderScratch scratch;
    CameraPixelBasis basis;
    SceneLighting lighting;
    const LightingLUT *lut;
    const ShadowMaps *maps;
    bool ready;
};


static inline float raster_plane_at(RasterPlane p,float x,float y) {return p.a*x+(p.b*y+p.c);}
uint32_t raster_shade_pixel(const RasterScene *scene,const PreparedTriangle *triangle,int x,int y,float q);
void raster_shade_four(const RasterScene *scene,const PreparedTriangle *triangle,int x,int y, const float q[4],uint32_t out[4]);

#endif
