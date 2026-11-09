#include "shadow_scene.h"

#include <math.h>

#include "math/math.h"
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    Vector3 a,b,c;
    Vector3 min,max;
    size_t object_index,triangle_index;
} ShadowTriangle;

//SHADOW GROUPS put triangles of a bound within one group to be skipped if ray falls out of bound
typedef struct {
    ShadowTriangle bounds;
    size_t first,end;
} ShadowGroup;

struct ShadowScene {
    ShadowGroup *groups;
    size_t group_count;
    ShadowTriangle *triangles;
    size_t count,capacity;
};

static bool finite_vec(Vector3 v) { return isfinite(v.x)&&isfinite(v.y)&&isfinite(v.z); }
ShadowScene *shadow_scene_create(size_t capacity)
{
    // introduce shadow groups to test if comparison ray misses bounds of object entirely, skipping pixel calc when ray is out of bounds
    if (capacity>SIZE_MAX/sizeof(ShadowTriangle) || capacity>SIZE_MAX/sizeof(ShadowGroup)) return NULL;
    ShadowScene *scene=malloc(sizeof(*scene));

    if (!scene) return NULL;
    *scene=(ShadowScene){0};
    
    if (capacity) {
        scene->triangles=malloc(capacity*sizeof(*scene->triangles));
        if (!scene->triangles) {free(scene);return NULL;}
    }
    if (capacity) {
        scene->groups=malloc(capacity*sizeof(*scene->groups));
        if (!scene->groups) {free(scene->groups);free(scene->triangles);free(scene);return NULL;}
    }
    scene->capacity=capacity;
    return scene;
}

void shadow_scene_destroy(ShadowScene *scene)
{
    if (!scene) return;
    free(scene->groups);free(scene->triangles);free(scene);
}

static Vector3 transform(Vector3 v,const SceneObject *object)
{
    v=vec3_rotate_y(v,object->rotation.y);
    v=vec3_rotate_x(v,object->rotation.x);
    v=vec3_rotate_z(v,object->rotation.z);
    return (Vector3){v.x+object->position.x,v.y+object->position.y,v.z+object->position.z};
}
bool shadow_scene_build(ShadowScene *scene,const SceneObject *objects,size_t count)
{
    if (!scene) return false;
    scene->count=0;
    scene->group_count=0;
    if (count && !objects) return false;
    size_t required=0;
    for (size_t i=0;i<count;++i) {
        const WireframeMesh *mesh=objects[i].mesh;
        if (!mesh || mesh->vertex_count<0 || mesh->triangle_count<0
            || (mesh->vertex_count && !mesh->vertices) || (mesh->triangle_count && !mesh->triangles)
            || !finite_vec(objects[i].position) || !finite_vec(objects[i].rotation)
            || (size_t)mesh->triangle_count>scene->capacity-required) return false;
        required+=(size_t)mesh->triangle_count;
    }
    size_t used=0;
    for (size_t i=0;i<count;++i) {
        const WireframeMesh *mesh=objects[i].mesh;
        for (int j=0;j<mesh->triangle_count;++j) {
            const Triangle *t=&mesh->triangles[j];
            if (t->a<0 || t->b<0 || t->c<0 || t->a>=mesh->vertex_count
                || t->b>=mesh->vertex_count || t->c>=mesh->vertex_count) return false;

            ShadowTriangle result;

            result.a=transform(mesh->vertices[t->a],&objects[i]);
            result.b=transform(mesh->vertices[t->b],&objects[i]);
            result.c=transform(mesh->vertices[t->c],&objects[i]);
            if (!finite_vec(result.a)||!finite_vec(result.b)||!finite_vec(result.c)) return false;
            result.min=(Vector3){min_finite(result.a.x,min_finite(result.b.x,result.c.x)),
                min_finite(result.a.y,min_finite(result.b.y,result.c.y)),min_finite(result.a.z,min_finite(result.b.z,result.c.z))};
            result.max=(Vector3){max_finite(result.a.x,max_finite(result.b.x,result.c.x)),
                max_finite(result.a.y,max_finite(result.b.y,result.c.y)),max_finite(result.a.z,max_finite(result.b.z,result.c.z))};
            result.object_index=i;result.triangle_index=(size_t)j;
            scene->triangles[used++]=result;
        }
    }
    // not empty objects contigious in range
    size_t groups=0;
    for (size_t first=0;first<used;) {
        ShadowGroup g={0};
        g.first=first; g.bounds=scene->triangles[first];
        size_t end=first+1;
        while (end<used && scene->triangles[end].object_index==g.bounds.object_index) {
            const ShadowTriangle *t=&scene->triangles[end++];
            g.bounds.min=(Vector3){min_finite(g.bounds.min.x,t->min.x),min_finite(g.bounds.min.y,t->min.y),min_finite(g.bounds.min.z,t->min.z)};
            g.bounds.max=(Vector3){max_finite(g.bounds.max.x,t->max.x),max_finite(g.bounds.max.y,t->max.y),max_finite(g.bounds.max.z,t->max.z)};
        }
        g.end=end; scene->groups[groups++]=g; first=end;
    }
    scene->group_count=groups;
    scene->count=used;
    return true;
}

size_t shadow_scene_triangle_count(const ShadowScene *scene) {return scene?scene->count:0;}

static bool intersects_bounds(const RaySegment *ray,const ShadowTriangle *t)
{
    double origin[3]={ray->origin.x,ray->origin.y,ray->origin.z};
    double direction[3]={ray->direction.x,ray->direction.y,ray->direction.z};
    double lo[3]={t->min.x,t->min.y,t->min.z},hi[3]={t->max.x,t->max.y,t->max.z};
    double begin=ray->min_distance,end=ray->max_distance;
    for (int i=0;i<3;++i) {
        if (direction[i]==0) {if (origin[i]<lo[i] || origin[i]>hi[i]) return false;}
        else {
            double a=(lo[i]-origin[i])/direction[i],b=(hi[i]-origin[i])/direction[i];
            if (a>b) {double temp=a;a=b;b=temp;}
            if (a>begin) begin=a;
            if (b<end) end=b;
            if (begin>end) return false;
        }
    }
    return true;
}
bool shadow_scene_occluded(const ShadowScene *scene,const RaySegment *ray, size_t skip_object,size_t skip_triangle)
{
    if (!scene || !ray) return false;
    for (size_t group=0;group<scene->group_count;++group) {
        const ShadowGroup *g=&scene->groups[group];
        if (!intersects_bounds(ray,&g->bounds)) continue;
            for (size_t i=g->first;i<g->end;++i) {
                const ShadowTriangle *t=&scene->triangles[i];
                if (t->object_index==skip_object && t->triangle_index==skip_triangle) continue;
                if (intersects_bounds(ray,t) && raycast_triangle(ray,t->a,t->b,t->c,NULL)) return true;
        }
    }
    return false;
}
