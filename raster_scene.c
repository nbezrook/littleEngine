#include "raster_scene_internal.h"
#include "clipping.h"
#include "math/math.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>

static bool finite_vec(Vector3 v) {return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z);}

// needs to probably be made more dynamic 
static void *array_alloc(size_t count,size_t size) {return count && count<=SIZE_MAX/size?malloc(count*size):NULL;}

// nightmare code 
RasterScene *raster_scene_create(int width,int height,size_t triangles,size_t references)
{
    if(width<1 || height<1 || width>16384 || height>16384 || !triangles || triangles>=UINT32_MAX || !references) return NULL;
    RasterScene *s=calloc(1,sizeof(*s));if(!s) return NULL;
    s->width=width;s->height=height;
    // divide up and allocate memory for the the raster scene
    s->tiles_x=(width+RASTER_TILE_SIZE-1)/RASTER_TILE_SIZE;
    s->tiles_y=(height+RASTER_TILE_SIZE-1)/RASTER_TILE_SIZE;
    s->tile_count=(size_t)s->tiles_x*s->tiles_y;
    s->triangle_capacity=triangles;s->reference_capacity=references;
    s->triangles=array_alloc(triangles,sizeof(*s->triangles));
    s->references=array_alloc(references,sizeof(*s->references));
    s->winners=array_alloc((size_t)width*height,sizeof(*s->winners));
    s->offsets=array_alloc(s->tile_count+1,sizeof(*s->offsets));
    s->counts=array_alloc(s->tile_count,sizeof(*s->counts));
    s->cursors=array_alloc(s->tile_count,sizeof(*s->cursors));
    s->shaded=array_alloc(s->tile_count,sizeof(*s->shaded));

    if(!s->triangles || !s->references || !s->winners || !s->offsets || !s->counts || !s->cursors || !s->shaded) {
        raster_scene_destroy(s);return NULL;
    }
    return s;
}

void raster_scene_destroy(RasterScene *s)
{
    if(!s) return;
    free(s->triangles);free(s->references);free(s->winners);free(s->offsets);
    free(s->counts);free(s->cursors);free(s->shaded);free(s);
}

// check if plane is clipped 
/**  Copilot
The clip_distance function calculates the distance from a point p to a specified clipping plane based on the camera's properties and the dimensions of the viewport. 
It uses a switch statement to determine the appropriate distance calculation for different planes, returning a double value representing the clip distance. */
static double clip_distance(Vector3 p,int plane,const Camera *camera,int width,int height)
{
    switch(plane) {
    case 0:return (double)p.z-camera->near_clip;
    case 1:return (double)camera->focal_length_pixels*p.x+(width*.5)*p.z;
    case 2:return (width*.5)*p.z-(double)camera->focal_length_pixels*p.x;
    case 3:return (height*.5)*p.z-(double)camera->focal_length_pixels*p.y;
    default:return (height*.5)*p.z+(double)camera->focal_length_pixels*p.y;
    }
}

// interpolate 
static ClipVertex clip_lerp(ClipVertex a,ClipVertex b,double t)
{
    ClipVertex r;
    r.position=(Vector3){(float)(a.position.x+((double)b.position.x-a.position.x)*t),
        (float)(a.position.y+((double)b.position.y-a.position.y)*t),
        (float)(a.position.z+((double)b.position.z-a.position.z)*t)};
    r.uv=(Point2){(float)(a.uv.x+((double)b.uv.x-a.uv.x)*t),(float)(a.uv.y+((double)b.uv.y-a.uv.y)*t)};
    return r;
}

// full side clipping bounds fixed-point edge coordinates before conversion.
static int clip_frustum(const ClipVertex input[3],ClipVertex output[12],const Camera *camera,int w,int h)
{
    ClipVertex buffers[2][12];memcpy(buffers[0],input,3*sizeof(*input));
    int count=3,source=0;
    for(int plane=0;plane<5 && count;++plane) {
        int target=1-source,n=0;
        ClipVertex previous=buffers[source][count-1];
        double pd=clip_distance(previous.position,plane,camera,w,h);
        for(int i=0;i<count;++i) {
            ClipVertex current=buffers[source][i];double cd=clip_distance(current.position,plane,camera,w,h);
            if((pd>=0)!=(cd>=0)) {
                if(n>=12) return -1;
                buffers[target][n++]=clip_lerp(previous,current,pd/(pd-cd));
            }
            if(cd>=0) {if(n>=12) return -1;buffers[target][n++]=current;}
            previous=current;pd=cd;
        }
        count=n;source=target;
    }
    memcpy(output,buffers[source],(size_t)count*sizeof(*output));return count;
}

// take face front
static bool front_facing(Vector3 a,Vector3 b,Vector3 c)
{
    double x=(double)b.x-a.x,y=(double)b.y-a.y,z=(double)b.z-a.z;
    double u=(double)c.x-a.x,v=(double)c.y-a.y,w=(double)c.z-a.z;
    return (y*w-z*v)*a.x+(z*u-x*w)*a.y+(x*v-y*u)*a.z<0;
}

typedef struct {int64_t x,y;float q,uq,vq;} SetupVertex;

// needs to be changed for vertex shaders
static RasterPlane attribute_plane(const SetupVertex v[3],float a,float b,float c)
{
    double x0=(double)v[0].x/RASTER_SUBPIXEL,y0=(double)v[0].y/RASTER_SUBPIXEL;
    double x1=(double)v[1].x/RASTER_SUBPIXEL,y1=(double)v[1].y/RASTER_SUBPIXEL;
    double x2=(double)v[2].x/RASTER_SUBPIXEL,y2=(double)v[2].y/RASTER_SUBPIXEL;
    double area=(x1-x0)*(y2-y0)-(y1-y0)*(x2-x0);
    double dx=(((double)b-a)*(y2-y0)-((double)c-a)*(y1-y0))/area;
    double dy=((x1-x0)*((double)c-a)-(x2-x0)*((double)b-a))/area;
    return (RasterPlane){(float)dx,(float)dy,(float)(a-dx*x0-dy*y0)};
}
/**  Copilot

The plane_representable function determines if a given RasterPlane can be represented within specified dimensions (width and height) by calculating a bound based on the plane's coefficients and checking if it is finite and within a certain limit. 
It returns a boolean value indicating whether the plane is representable. */
static bool plane_representable(RasterPlane p,int width,int height)
{
    double bound=fabs((double)p.a)*(width+.5)+fabs((double)p.b)*(height+.5)+fabs((double)p.c);
    return isfinite(bound) && bound<=FLT_MAX*.5;
}
/* Copilot

The append_triangle function adds a triangle defined by three vertices to a RasterScene, calculating its area and ensuring it fits within the scene's dimensions and capacity. 
It prepares the triangle's attributes, including normal, texture, and color, while checking for representability in the rasterization process.*/
static bool append_triangle(RasterScene *s,SetupVertex v[3],Vector3 normal,const SceneObject *object)
{
    int64_t area=(v[1].x-v[0].x)*(v[2].y-v[0].y)-(v[1].y-v[0].y)*(v[2].x-v[0].x);
    if(!area) return true;
    if(area<0) {SetupVertex temp=v[1];v[1]=v[2];v[2]=temp;}
    if(s->triangle_count>=s->triangle_capacity) return false;
    PreparedTriangle t={0};t.normal=normal;t.texture=object->texture;t.color=object->color;
    int64_t minx=v[0].x,maxx=v[0].x,miny=v[0].y,maxy=v[0].y;
    for(int i=0;i<3;++i) {
        const SetupVertex a=v[i],b=v[(i+1)%3];
        t.edges[i]=(RasterEdge){a.y-b.y,b.x-a.x,a.x*b.y-a.y*b.x, b.y<a.y || (b.y==a.y && b.x>a.x)};
        if(a.x<minx) minx=a.x;
        if(a.x>maxx) maxx=a.x;
        if(a.y<miny) miny=a.y;
        if(a.y>maxy) maxy=a.y;
    }
    t.x0=(int)(minx/RASTER_SUBPIXEL);t.y0=(int)(miny/RASTER_SUBPIXEL);
    t.x1=(int)((maxx+RASTER_SUBPIXEL-1)/RASTER_SUBPIXEL);t.y1=(int)((maxy+RASTER_SUBPIXEL-1)/RASTER_SUBPIXEL);
    if(t.x0<0)t.x0=0;
    if(t.y0<0)t.y0=0;
    if(t.x1>s->width)t.x1=s->width;
    if(t.y1>s->height)t.y1=s->height;
    if(t.x0>=t.x1 || t.y0>=t.y1) return true;
    t.q=attribute_plane(v,v[0].q,v[1].q,v[2].q);
    t.uq=attribute_plane(v,v[0].uq,v[1].uq,v[2].uq);
    t.vq=attribute_plane(v,v[0].vq,v[1].vq,v[2].vq);
    if(!plane_representable(t.q,s->width,s->height) ||!plane_representable(t.uq,s->width,s->height) ||!plane_representable(t.vq,s->width,s->height)) return false;
    s->triangles[s->triangle_count++]=t;return true;
}

/* Copilot

The prepare_object function processes a SceneObject within a RasterScene, validating its mesh and texture properties, 
transforming its vertices based on the object's position and rotation, and clipping triangles against the camera's frustum before projecting them for rendering. 
It returns a boolean indicating the success of these operations, ensuring that all geometric and texture data are valid and properly formatted for rendering.*/
static bool prepare_object(RasterScene *s,const SceneObject *object,const Camera *camera)
{
    const WireframeMesh *mesh=object->mesh;
    if(!mesh || mesh->vertex_count<0 || mesh->vertex_count>RENDER_VERTEX_CAPACITY || mesh->triangle_count<0 || (mesh->vertex_count && !mesh->vertices) || (mesh->triangle_count && !mesh->triangles) || !finite_vec(object->position)||!finite_vec(object->rotation)) return false;
    if(object->texture && (!object->texture->pixels || object->texture->width<=0 || object->texture->height<=0)) return false;
    for(int i=0;i<mesh->vertex_count;++i) {
        Vector3 p=mesh->vertices[i];if(!finite_vec(p)) return false;
        p=vec3_rotate_y(p,object->rotation.y);p=vec3_rotate_x(p,object->rotation.x);p=vec3_rotate_z(p,object->rotation.z);
        p=(Vector3){p.x+object->position.x,p.y+object->position.y,p.z+object->position.z};
        p=camera_world_to_view(camera,p);if(!finite_vec(p)) return false;s->scratch.view[i]=p;
    }
    for(int i=0;i<mesh->triangle_count;++i) {
        Triangle tri=mesh->triangles[i];
        if(tri.a<0 || tri.b<0 || tri.c<0 || tri.a>=mesh->vertex_count || tri.b>=mesh->vertex_count || tri.c>=mesh->vertex_count) return false;
        Vector3 a=s->scratch.view[tri.a],b=s->scratch.view[tri.b],c=s->scratch.view[tri.c];
        if(!front_facing(a,b,c)) continue;
        Point2 zero={0};
        ClipVertex input[3]={{a,object->texture?tri.uv_a:zero},{b,object->texture?tri.uv_b:zero},{c,object->texture?tri.uv_c:zero}},clipped[12];
        for(int k=0;k<3;++k) if(!isfinite(input[k].uv.x)||!isfinite(input[k].uv.y)) return false;
        int n=clip_frustum(input,clipped,camera,s->width,s->height);if(n<0) return false;if(n<3) continue;
        Vector3 normal;if(!triangle_unit_normal(mesh->vertices[tri.a],mesh->vertices[tri.b],mesh->vertices[tri.c],&normal)) continue;
        normal=vec3_rotate_y(normal,object->rotation.y);normal=vec3_rotate_x(normal,object->rotation.x);normal=vec3_rotate_z(normal,object->rotation.z);
        SetupVertex vertices[12];
        for(int k=0;k<n;++k) {
            // near-plane intersections can round just below the plane
            if(clipped[k].position.z<camera->near_clip) clipped[k].position.z=camera->near_clip;
            ProjectedVertex projected;if(!camera_project_view(camera,clipped[k].position,s->width,s->height,&projected)) return false;
            if(!isfinite(projected.screen.x)||!isfinite(projected.screen.y)||!isfinite(projected.inverse_depth)) return false;
            float x=projected.screen.x,y=projected.screen.y;
            if(x<0)x=0;
            if(x>s->width)x=(float)s->width;
            if(y<0)y=0;
            if(y>s->height)y=(float)s->height;
            vertices[k]=(SetupVertex){llroundf(x*RASTER_SUBPIXEL),llroundf(y*RASTER_SUBPIXEL), projected.inverse_depth,clipped[k].uv.x*projected.inverse_depth,clipped[k].uv.y*projected.inverse_depth};
        }
        for(int k=1;k<n-1;++k) {SetupVertex fan[3]={vertices[0],vertices[k],vertices[k+1]};if(!append_triangle(s,fan,normal,object)) return false;}
    }
    return true;
}
/* Copilot

The triangle_tiles function calculates the tile coordinates for a triangle defined by the PreparedTriangle structure, 
using the dimensions of the raster tiles specified by RASTER_TILE_SIZE. 
It takes pointers to the coordinates (x0, y0, x1, y1) as parameters and modifies them based on the triangle's vertex positions.*/
static void triangle_tiles(const RasterScene *s,const PreparedTriangle *t,int *x0,int *y0,int *x1,int *y1)
{
    (void)s;*x0=t->x0/RASTER_TILE_SIZE;*y0=t->y0/RASTER_TILE_SIZE;
    *x1=(t->x1-1)/RASTER_TILE_SIZE;*y1=(t->y1-1)/RASTER_TILE_SIZE;
}


bool raster_scene_prepare(RasterScene *s,const SceneObject *objects,size_t count,const Camera *camera,
    const SceneLighting *lighting,const LightingLUT *lut,const ShadowMaps *maps)
{
    if(!s) return false;
    s->ready=false;s->triangle_count=0;s->reference_count=0;
    if(!camera || !lut || !lighting_validate(lighting) || (count && !objects) || !isfinite(camera->near_clip)||camera->near_clip<=0 || !camera_build_pixel_basis(&s->basis,camera,s->width,s->height)) return false;
    s->lighting=*lighting;s->lut=lut;s->maps=maps;
    for(size_t i=0;i<count;++i) if(!prepare_object(s,&objects[i],camera)) return false;
    memset(s->counts,0,s->tile_count*sizeof(*s->counts));
    for(size_t i=0;i<s->triangle_count;++i) {
        int x0,y0,x1,y1;triangle_tiles(s,&s->triangles[i],&x0,&y0,&x1,&y1);
        for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
            if(s->reference_count>=s->reference_capacity) return false;
            ++s->counts[(size_t)y*s->tiles_x+x];++s->reference_count;
        }
    }
    s->offsets[0]=0;
    for(size_t i=0;i<s->tile_count;++i) {s->offsets[i+1]=s->offsets[i]+s->counts[i];s->cursors[i]=s->offsets[i];}
    // preserve primitive order within every bin, defining equal-depth ownership
    for(size_t i=0;i<s->triangle_count;++i) {
        int x0,y0,x1,y1;triangle_tiles(s,&s->triangles[i],&x0,&y0,&x1,&y1);
        for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
            size_t tile=(size_t)y*s->tiles_x+x;s->references[s->cursors[tile]++]=(uint32_t)i;
        }
    }
    memset(s->shaded,0,s->tile_count*sizeof(*s->shaded));s->ready=true;return true;
}
size_t raster_scene_job_count(const RasterScene *s) {return s&&s->ready?s->tile_count:0;}

// this should never not be true
bool raster_scene_simd_available(void) {return RASTER_HAS_SIMD!=0;}

static bool edge_inside(int64_t e,bool inclusive) {return e>0 || (e==0 && inclusive);}
/* Copilot

The classify_rectangle function determines the classification of a rectangle defined by its corner coordinates (x0, y0) and (x1, y1) with respect to a prepared triangle t. 
It calculates values based on the triangle's edges and returns an integer indicating whether the rectangle is fully inside the triangle (2), partially inside (1), or outside (0).*/
static int classify_rectangle(const PreparedTriangle *t,int x0,int y0,int x1,int y1,int64_t values[3])
{
    bool full=true;
    for(int i=0;i<3;++i) {
        RasterEdge e=t->edges[i];
        values[i]=e.a*((int64_t)x0*RASTER_SUBPIXEL+RASTER_SUBPIXEL/2) +e.b*((int64_t)y0*RASTER_SUBPIXEL+RASTER_SUBPIXEL/2)+e.c;
        int64_t dx=e.a*RASTER_SUBPIXEL*(x1-x0-1),dy=e.b*RASTER_SUBPIXEL*(y1-y0-1);
        int64_t low=values[i]+(dx<0?dx:0)+(dy<0?dy:0);
        int64_t high=values[i]+(dx>0?dx:0)+(dy>0?dy:0);
        if(!edge_inside(high,e.inclusive)) return 0;
        if(!edge_inside(low,e.inclusive)) full=false;
    }
    return full?2:1;
}
/* Copilot

The resolve_triangle function processes a triangle in a raster scene by determining the pixel coverage and depth values within a specified rectangular area, 
utilizing SIMD instructions for performance optimization when enabled. 
It updates the depth and winner buffers based on the triangle's edges and the classification of the rectangle relative to the triangle.*/

static void resolve_triangle(RasterScene *s,const RasterTarget *target,uint32_t id,
    int x0,int y0,int x1,int y1,bool simd)
{
    const PreparedTriangle *t=&s->triangles[id];
    if(x0<t->x0)x0=t->x0;
    if(y0<t->y0)y0=t->y0;
    if(x1>t->x1)x1=t->x1;
    if(y1>t->y1)y1=t->y1;
    if(x0>=x1 || y0>=y1) return;
    int64_t row[3];int classification=classify_rectangle(t,x0,y0,x1,y1,row);if(!classification)return;
    for(int y=y0;y<y1;++y) {
        size_t start=(size_t)y*s->width;
        float *depth=target->depth->values+start;uint32_t *winner=s->winners+start;
        int64_t edges[3]={row[0],row[1],row[2]};
        float row_q=t->q.b*(y+.5f)+t->q.c;
        int x=x0;
#if RASTER_HAS_SIMD
        if(simd) for(;x+3<x1;x+=4) {
            unsigned mask=classification==2?15u:0u;
            if(classification!=2) for(int lane=0;lane<4;++lane) {
                bool inside=true;
                for(int edge=0;edge<3;++edge)
                    if(!edge_inside(edges[edge]+t->edges[edge].a*RASTER_SUBPIXEL*lane,t->edges[edge].inclusive)) inside=false;
                if(inside)mask|=1u<<lane;
            }
            // a little assembly lite, this lets us use SIMD instructions which are like ALUs working parallel to the CPU
            // each __m128 has four lanes of 32-bit float
            // we divide up the work into lane masks
            //equal to the following
            /*for (int lane = 0; lane < 4; ++lane) {
                if (!(mask & (1u << lane)))
                    continue;

                int pixel = x + lane;
                float xs = pixel + 0.5f;
                float q = t->q.a * xs + row_q;

                if (q > depth[pixel] && q >= FLT_MIN) {
                    depth[pixel] = q;
                    winner[pixel] = id;
                }
            }*/

            if(mask) {
                //four pixel-centre coordinates
                __m128 xs=_mm_set_ps(x+3.5f,x+2.5f,x+1.5f,x+.5f);
                //evaluate reciprocal depth, xs = [x+0.5, x+1.5, x+2.5, x+3.5]
                __m128 q=_mm_add_ps(_mm_mul_ps(_mm_set1_ps(t->q.a),xs),_mm_set1_ps(row_q));
                // old = [depth[x], depth[x+1], depth[x+2], depth[x+3]]
                __m128 old=_mm_loadu_ps(depth+x);
                //coverage = [all ones, zero, all ones, zero]
                __m128 coverage=_mm_castsi128_ps(_mm_set_epi32(mask&8?-1:0,mask&4?-1:0,mask&2?-1:0,mask&1?-1:0));
                // wins = covered && closer && sufficiently positive
                __m128 wins=_mm_and_ps(coverage,_mm_and_ps(_mm_cmpgt_ps(q,old),_mm_cmpge_ps(q,_mm_set1_ps(FLT_MIN))));
                if(_mm_movemask_ps(wins)) {
                    // find winning triangles
                    _mm_storeu_ps(depth+x,_mm_or_ps(_mm_and_ps(wins,q),_mm_andnot_ps(wins,old)));
                    __m128i old_id=_mm_loadu_si128((const __m128i*)(winner+x)),bits=_mm_castps_si128(wins);
                    _mm_storeu_si128((__m128i*)(winner+x),_mm_or_si128(_mm_and_si128(bits,_mm_set1_epi32((int)id)),_mm_andnot_si128(bits,old_id)));
                }
            }
            for(int i=0;i<3;++i) edges[i]+=t->edges[i].a*RASTER_SUBPIXEL*4;
        }
#else
        (void)simd;
#endif
        for(;x<x1;++x) {
            bool inside=classification==2;
            if(!inside)inside=edge_inside(edges[0],t->edges[0].inclusive)&&edge_inside(edges[1],t->edges[1].inclusive)&&edge_inside(edges[2],t->edges[2].inclusive);
            if(inside) {float q=t->q.a*(x+.5f)+row_q;if(q>depth[x] && q>=FLT_MIN) {depth[x]=q;winner[x]=id;}}
            for(int i=0;i<3;++i)edges[i]+=t->edges[i].a*RASTER_SUBPIXEL;
        }
        for(int i=0;i<3;++i)row[i]+=t->edges[i].b*RASTER_SUBPIXEL;
    }
}

bool raster_scene_render_tile(RasterScene *s,const RasterTarget *target,size_t tile,RasterKernel kernel)
{
    if(!s || !s->ready || !target || !target->pixels || !target->depth || !target->depth->values || tile>=s->tile_count || target->width!=s->width || target->height!=s->height || target->depth->width!=s->width || target->depth->height!=s->height || target->pitch<s->width*(int)sizeof(uint32_t) || target->pitch%(int)sizeof(uint32_t) || (kernel!=RASTER_SCALAR && kernel!=RASTER_SIMD)) return false;
    int x0=(int)(tile%s->tiles_x)*RASTER_TILE_SIZE,y0=(int)(tile/s->tiles_x)*RASTER_TILE_SIZE;
    int x1=x0+RASTER_TILE_SIZE,y1=y0+RASTER_TILE_SIZE;if(x1>s->width)x1=s->width;if(y1>s->height)y1=s->height;
    for(int y=y0;y<y1;++y) {
        uint32_t *pixels=(uint32_t*)((uint8_t*)target->pixels+(size_t)y*target->pitch);
        size_t base=(size_t)y*s->width;
        for(int x=x0;x<x1;++x) {pixels[x]=0;target->depth->values[base+x]=0;s->winners[base+x]=UINT32_MAX;}
    }
    bool simd=kernel==RASTER_SIMD && RASTER_HAS_SIMD;
    for(size_t i=s->offsets[tile];i<s->offsets[tile+1];++i)
        resolve_triangle(s,target,s->references[i],x0,y0,x1,y1,simd);
    uint64_t shaded=0;
    for(int y=y0;y<y1;++y) {
        uint32_t *pixels=(uint32_t*)((uint8_t*)target->pixels+(size_t)y*target->pitch);
        size_t base=(size_t)y*s->width;int x=x0;
        for(;x<x1;) {
            uint32_t id=s->winners[base+x];
            if(simd && x+3<x1 && id!=UINT32_MAX && s->winners[base+x+1]==id && s->winners[base+x+2]==id && s->winners[base+x+3]==id) {
                uint32_t out[4];raster_shade_four(s,&s->triangles[id],x,y,target->depth->values+base+x,out);
                memcpy(pixels+x,out,sizeof(out));x+=4;shaded+=4;
            } else {
                if(id!=UINT32_MAX) {pixels[x]=raster_shade_pixel(s,&s->triangles[id],x,y,target->depth->values[base+x]);++shaded;}
                ++x;
            }
        }
    }
    s->shaded[tile]=shaded;return true;
}

bool raster_scene_stats(const RasterScene *s,RasterSceneStats *out)
{
    if(!s || !s->ready || !out) return false;
    RasterSceneStats r={s->triangle_count,s->reference_count,0};
    for(size_t i=0;i<s->tile_count;++i)r.shaded_pixels+=s->shaded[i];
    *out=r;return true;
}
