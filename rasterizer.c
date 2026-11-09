#include "rasterizer.h"
#include "clipping.h"
#include "math/math.h"
#include <math.h>
#include <limits.h>


/**
 * @brief Return the signed 2D cross product (b-a) x (p-a).
 * @param a Directed edge start.
 * @param b Directed edge end.
 * @param p Point to classify.
 * @return Signed doubled triangle area, used for coverage and barycentric weights.
 */
static float edge(Point2 a, Point2 b, Point2 p) {
    // signed 2d cross product
    return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);
}

/**
 * Test a triangle in view space, where the camera is at the origin.
 * Assumes consistently wound, one-sided geometry.
 */
static bool triangle_is_front_facing(Vector3 a, Vector3 b, Vector3 c)
{
    //produces a normal determined by the triangle vertex order
    Vector3 ab = {b.x - a.x, b.y - a.y, b.z - a.z};
    Vector3 ac = {c.x - a.x, c.y - a.y, c.z - a.z};
    // direction from vertex a toward the camera is -a camera is at zero in view space
    Vector3 normal = {ab.y * ac.z - ab.z * ac.y,ab.z * ac.x - ab.x * ac.z,ab.x * ac.y - ab.y * ac.x};
    //front-facing normal has normal * (-a) > 0, equivalently normal * a < 0
    float facing = normal.x * a.x
                 + normal.y * a.y
                 + normal.z * a.z;

    return facing < 0.0f;
}

/**
 * @brief Identify inclusive edges for the rasterizer's screen-space top-left fill rule.
 * @param a Directed edge start.
 * @param b Directed edge end.
 * @return True for upward edges or horizontal edges directed right.
 * @note Assumes screen Y increases downward and interior edge values are positive.
 */
static bool inclusive_edge(Point2 a, Point2 b) {
    float dx = b.x - a.x;
    float dy = b.y - a.y; 
    return dy < 0.0f || (dy == 0.0f && dx > 0.0f);
}

/**
 * @brief Draw a solid integer line, skipping samples outside the framebuffer.
 * @param pixels Non-null tightly packed 32-bit framebuffer.
 * @param screen_width Positive row width in pixels; also used as the pixel stride.
 * @param screen_height Positive row count.
 * @param x1 Start X in pixels.
 * @param y1 Start Y in pixels.
 * @param x2 End X in pixels.
 * @param y2 End Y in pixels.
 * @param color Packed pixel color.
 * @note Legacy wireframe path: no depth test or padded-row pitch support.
 * Coordinate differences and intermediate integer arithmetic must not overflow.
 */
void draw_line(uint32_t* pixels, int screen_width, int screen_height, int x1, int y1, int x2, int y2, uint32_t color) {
    int dx = cAbs(x2 - x1);
    int dy = cAbs(y2 - y1);
    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;
    int err = dx - dy;

    int x = x1;
    int y = y1;

    while (true) {
        // check bounds with local render dimensions
        if (x >= 0 && x < screen_width && y >= 0 && y < screen_height) {
            // calculate memory offset using dimensions from main
            pixels[y * screen_width + x] = color;
        }

        if (x == x2 && y == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
}

typedef struct {
    Point2 screen;
    float inverse_depth;
    float u_over_depth;
    float v_over_depth;
} RasterVertex;

/**
 * @brief Prepare attributes that interpolate linearly after perspective projection.
 * @param p Projected position with positive reciprocal camera depth.
 * @param uv Texture coordinates attached to this triangle corner.
 * @return Screen position, 1/Z, u/Z, and v/Z for rasterization.
 */
static RasterVertex make_raster_vertex(ProjectedVertex p, Point2 uv)
{
    return (RasterVertex){.screen = p.screen,.inverse_depth = p.inverse_depth,.u_over_depth = uv.x * p.inverse_depth, .v_over_depth = uv.y * p.inverse_depth};
}

/**
 * @brief Rasterize an opaque triangle with top-left coverage and reciprocal-depth testing.
 * @param pixels Writable 32-bit framebuffer, with rows separated by pitch bytes.
 * @param depth Initialized depth buffer with the same width/height; clear before the frame.
 * @param width Positive viewport width in pixels.
 * @param height Positive viewport height in pixels.
 * @param pitch Row stride in bytes, at least width * sizeof(uint32_t).
 * @param a Finite projected vertex with positive reciprocal depth.
 * @param b Finite projected vertex with positive reciprocal depth.
 * @param c Finite projected vertex with positive reciprocal depth.
 * @param color Solid fallback color if texture is NULL.
 * @param texture Valid initialized texture, or NULL for solid fill.
 * @note Either winding is accepted; zero-area triangles are skipped. Larger 1/Z wins.
 * UVs are recovered from interpolated u/Z and v/Z; no allocation or geometric clipping occurs.
 */
typedef struct {
    const SceneLighting *scene;
    const LightingLUT *lut;
    const ShadowScene *shadows;
    CameraPixelBasis basis;
} LightingContext;

static uint32_t shade_fragment(const LightingContext *context,uint32_t texel,
                               Vector3 point,Vector3 normal,size_t object_index,size_t triangle_index)
{
    Vector3 total=context->scene->ambient;
    for (size_t i=0;i<context->scene->light_count;++i) {
        const Light *light=&context->scene->lights[i];
        LightSample sample;
        if (!lighting_sample_light(light,point,normal,&sample)) continue;
        float visibility=1;
        if (light->casts_shadows && context->shadows) {
            RaySegment ray;
            if (lighting_shadow_ray(light,point,normal,context->scene->shadow_bias,&ray)
                && shadow_scene_occluded(context->shadows,&ray,object_index,triangle_index))
                visibility=0;
        }
        total.x+=sample.irradiance.x*visibility;
        total.y+=sample.irradiance.y*visibility;
        total.z+=sample.irradiance.z*visibility;
    }
    return lighting_apply_rgb(texel,total,context->lut);
}

static void fill_triangle(uint32_t *pixels,DepthBuffer *depth,int width,int height,int pitch,RasterVertex a,RasterVertex b,RasterVertex c,uint32_t color,const CPUTexture *texture, const LightingContext *context, Vector3 normal, size_t object_index, size_t triangle_index)
{   
    //incremental evaluation
    //calculate the three edge values at the first pixel center
    //advance across each row using three additions
    //advance the row’s starting values using three additions
    //keep the existing top-left rule, depth testing, and perspective-correct UV calculations
    float area = edge(a.screen,b.screen,c.screen);

    //collinear vertices don't enclose tri
    if (area == 0.0f) return;

    // interior edge values > 0
    if (area < 0.0f) {
        RasterVertex temp = b;
        b = c;
        c = temp;
        area = -area;
    }

    float inverse_area = 1.0f / area;

    // simple c comparison -- apparently the fastest way to do it, checked against assembly
    float min_x = min_finite(a.screen.x, min_finite(b.screen.x, c.screen.x));
    float min_y = min_finite(a.screen.y, min_finite(b.screen.y, c.screen.y));
    float max_x = max_finite(a.screen.x, max_finite(b.screen.x, c.screen.x));
    float max_y = max_finite(a.screen.y, max_finite(b.screen.y, c.screen.y));

    int x0 = (int)max_finite(0.0f, min_finite((float)width, floorf(min_x)));
    int y0 = (int)max_finite(0.0f, min_finite((float)height, floorf(min_y)));
    int x1 = (int)max_finite(0.0f, min_finite((float)width, ceilf(max_x)));
    int y1 = (int)max_finite(0.0f, min_finite((float)height, ceilf(max_y)));

    //calculate the three edge values at the first pixel center, once per triangle
    bool ab = inclusive_edge(a.screen, b.screen);
    bool bc = inclusive_edge(b.screen, c.screen);
    bool ca = inclusive_edge(c.screen, a.screen);
    

    if (x0 >= x1 || y0 >= y1) return;

    // delta edge value when moving pixel right
    float step_x0 = a.screen.y - b.screen.y;
    float step_x1 = b.screen.y - c.screen.y;
    float step_x2 = c.screen.y - a.screen.y;
    // delta edge value when moving pixel down
    float step_y0 = b.screen.x - a.screen.x;
    float step_y1 = c.screen.x - b.screen.x;
    float step_y2 = a.screen.x - c.screen.x;

    //evaluate the first pixel center
    Point2 first = {x0 + 0.5f, y0 + 0.5f};

    float row_e0 = edge(a.screen, b.screen, first);
    float row_e1 = edge(b.screen, c.screen, first);
    float row_e2 = edge(c.screen, a.screen, first);

    for (int y = y0; y < y1; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)pixels + (size_t)y * pitch);

        float e0 = row_e0;
        float e1 = row_e1;
        float e2 = row_e2;

        for (int x = x0; x < x1;
            ++x, e0 += step_x0, e1 += step_x1, e2 += step_x2) {
            bool inside = (e0 > 0.0f || (e0 == 0.0f && ab)) && (e1 > 0.0f || (e1 == 0.0f && bc)) && (e2 > 0.0f || (e2 == 0.0f && ca));

            if (inside) {
                float weight_a = e1 * inverse_area;
                float weight_b = e2 * inverse_area;
                float weight_c = e0 * inverse_area;

                float q = weight_a * a.inverse_depth + weight_b * b.inverse_depth + weight_c * c.inverse_depth;
                size_t index = (size_t)y * depth->width + x;
                if (q > depth->values[index]) {
                    depth->values[index] = q;
                    float view_z=1.0f/q;
                    Vector3 point=camera_pixel_world(&context->basis,x+.5f,y+.5f,view_z);
                    uint32_t texel=color;
                    if (texture) {
                        float uq =weight_a * a.u_over_depth + weight_b * b.u_over_depth + weight_c * c.u_over_depth;
                        float vq = weight_a * a.v_over_depth + weight_b * b.v_over_depth + weight_c * c.v_over_depth;
                        float u = uq * view_z;
                        float v = vq * view_z;
                        texel=texture_sample(texture,u,v);
                    }
                    row[x]=shade_fragment(context,texel,point,normal,object_index,triangle_index);
                } // depth test 
            }     // inside triangle 
        }         // x loop 
        // new row starts from these values n not from the edge values left at the right-hand end of the previous row -- incremental edge evaluation
        row_e0 += step_y0;
        row_e1 += step_y1;
        row_e2 += step_y2;
    }             // y loop 
}                 // fill_triangle 


/**
 * @brief Transform/project vertices into reusable scratch and draw mesh edges.
 * @param pixels Writable tightly packed framebuffer; this legacy path ignores padded pitch.
 * @param screen_width Positive framebuffer width in pixels.
 * @param screen_height Positive framebuffer height in pixels.
 * @param mesh Valid vertex/edge arrays and in-range indices.
 * @param rotation Euler angles in radians, applied Y then X then Z.
 * @param color Packed line color.
 * @param position World-space object translation.
 * @param camera Non-null camera.
 * @param scratch Non-null caller-owned workspace; must not be shared by concurrent calls.
 * @return False for excessive/negative vertex count or any vertex failing projection; true otherwise.
 * @note No depth testing or triangle clipping. A rejected vertex rejects the whole mesh.
 */
bool draw_wireframe_mesh(uint32_t *pixels, int screen_width, int screen_height, 
    WireframeMesh mesh, Vector3 rotation, uint32_t color,
    Vector3 position,const Camera *camera,RenderScratch *scratch) {
    
    // check vertex count against scratch buffer
    if (mesh.vertex_count < 0 || mesh.vertex_count > RENDER_VERTEX_CAPACITY) {
        return false;
    }


    // check edge points
    ProjectedVertex *points = scratch->projected;

    // project vertices to 2d plane
    for (int i = 0; i < mesh.vertex_count; ++i) {
        Vector3 v = mesh.vertices[i];

        v = vec3_rotate_y(v, rotation.y);
        v = vec3_rotate_x(v, rotation.x);
        v = vec3_rotate_z(v, rotation.z);

        Vector3 world = {
            v.x + position.x,
            v.y + position.y,
            v.z + position.z
        };

        if (!camera_project(camera,world,screen_width,screen_height,&points[i])) {
            return false;
            }
        }
    // draw edges
    for (int i = 0; i < mesh.edge_count; ++i) {
        int a = mesh.edges[i].start_index;
        int b = mesh.edges[i].end_index;

        draw_line(pixels,screen_width,screen_height,(int)points[a].screen.x,(int)points[a].screen.y,(int)points[b].screen.x,(int)points[b].screen.y,color);
    }
    return true;
}

/**
 * @brief Transform/project a mesh once and rasterize its textured or solid triangles.
 * @param pixels Writable 32-bit framebuffer.
 * @param screen_width Positive framebuffer width in pixels.
 * @param screen_height Positive framebuffer height in pixels.
 * @param pitch Framebuffer row stride in bytes.
 * @param mesh Valid vertex/triangle arrays and in-range indices.
 * @param rotation Euler angles in radians, applied Y then X then Z.
 * @param color Solid fallback color when texture is NULL.
 * @param position World-space object translation.
 * @param camera Non-null camera.
 * @param scratch Non-null exclusive workspace; no per-mesh allocation occurs.
 * @param depth Matching initialized depth buffer, cleared once before all frame objects.
 * @param texture Initialized texture, or NULL for solid color.
 * @return False for excessive/negative vertex count or any rejected vertex; true otherwise.
 * @note No backface culling or near-plane triangle clipping is performed.
 */

bool draw_mesh(uint32_t *pixels, int screen_width, int screen_height, 
    int pitch, WireframeMesh mesh, Vector3 rotation, uint32_t color,
    Vector3 position, const Camera *camera, RenderScratch *scratch, DepthBuffer *depth,  const CPUTexture *texture, const LightingContext *context, size_t object_index) {
            
    // check vertex count against scratch buffer
    if (mesh.vertex_count < 0 || mesh.vertex_count > RENDER_VERTEX_CAPACITY) {
        return false;
    }

    // check edge points
    Vector3 *points = scratch->view;

    // project vertices to 2d plane
    for (int i = 0; i < mesh.vertex_count; ++i) {
        Vector3 v = mesh.vertices[i];

        v = vec3_rotate_y(v, rotation.y);
        v = vec3_rotate_x(v, rotation.x);
        v = vec3_rotate_z(v, rotation.z);

        Vector3 world = {
            v.x + position.x,
            v.y + position.y,
            v.z + position.z
        };

        points[i] = camera_world_to_view(camera, world);
    }

        // fill triangle in mesh
        for (int i = 0; i < mesh.triangle_count; ++i) {
            Triangle triangle = mesh.triangles[i];

            // reject triangle if not front facing; rejected triangles skip clipping, projection, coverage tests, depth tests, and texture sampling
            if (!triangle_is_front_facing(points[triangle.a],points[triangle.b],points[triangle.c])) {continue;}

            ClipVertex input[3] = {{points[triangle.a], triangle.uv_a},{points[triangle.b], triangle.uv_b},{points[triangle.c], triangle.uv_c}};
            ClipVertex clipped[4];

            int count = clip_triangle_near(input, camera->near_clip, clipped);

            if (count < 3) continue;

            RasterVertex vertices[4];
            bool valid = true;

            for (int j = 0; j < count; ++j) {
                ProjectedVertex projected;

                if (!camera_project_view(camera, clipped[j].position, screen_width, screen_height, &projected)) {
                    valid = false;
                    break;
                }

                vertices[j] = make_raster_vertex(projected, clipped[j].uv);
            }

            if (!valid) continue;

            // triangle normal. clipped fan triangles share it
            Vector3 normal;
            if (!triangle_unit_normal(mesh.vertices[triangle.a], mesh.vertices[triangle.b],
                                       mesh.vertices[triangle.c], &normal))
                continue;
            // rotations match the vertex transform translation never rotates normals 
            normal = vec3_rotate_y(normal, rotation.y);
            normal = vec3_rotate_x(normal, rotation.x);
            normal = vec3_rotate_z(normal, rotation.z);

            for (int j = 1; j < count - 1; ++j) {
                fill_triangle(pixels,depth,screen_width,screen_height,pitch,vertices[0],vertices[j], vertices[j + 1],color, texture, context, normal, object_index, (size_t)i);
            }
        }

    return true;
}

/** 
rasterizer_draw_scene() then:
clears the color buffer.
clears depth once.
draws the scene’s objects using the supplied scratch storage.
returns success or failure.


raycast.h/c	        two-sided ray–triangle intersection
shadow_scene.h/c	own and query transformed scene triangles
lighting.h/c	    evaluate lights and construct shadow rays
 */

bool rasterizer_draw_scene(RasterTarget *target, RenderScratch *scratch,
    const SceneObject *objects, size_t object_count, const Camera *camera, const SceneLighting *lighting, const LightingLUT *lut, const ShadowScene *shadows)
{
    //big list of things to skip in rendering; better to make something else reason about this
    if (!target || !scratch || !camera || !lut || !lighting_validate(lighting) || (object_count && !objects) ||!target->pixels || !target->depth || !target->depth->values 
    ||target->width <= 0 || target->height <= 0 ||target->width > INT_MAX / (int)sizeof(uint32_t) 
    ||target->pitch < target->width * (int)sizeof(uint32_t) ||target->pitch % (int)sizeof(uint32_t) != 0 
    || target->depth->width != target->width || target->depth->height != target->height)
        return false;

    LightingContext context={0};
    context.scene=lighting;context.lut=lut;context.shadows=shadows;
    if (!camera_build_pixel_basis(&context.basis,camera,target->width,target->height)) return false;

    for (int y = 0; y < target->height; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)target->pixels + (size_t)y * target->pitch);
        for (int x = 0; x < target->width; ++x) row[x] = 0;
    }
    depth_buffer_clear(target->depth);
    for (size_t i = 0; i < object_count; ++i) {
        const SceneObject *object = &objects[i];
        if (!object->mesh || !draw_mesh(target->pixels, target->width, target->height, target->pitch, *object->mesh, object->rotation, object->color, object->position, camera, scratch, target->depth, object->texture, &context, i))
            return false;
    }
    return true;
}

