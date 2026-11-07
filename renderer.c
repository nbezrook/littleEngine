#include "renderer.h"
#include "math/math.c"
#include "math/math_types.h"
#include "math/primitives.h"
#include "camera.h"

bool texture_init(CPUTexture *texture, int width, int height){
    if (width <= 0 || height <= 0) return false;

    size_t w = (size_t)width;
    size_t h = (size_t)height;

    if (w > SIZE_MAX / h) return false;

    size_t count = w * h;
    
    if (count > SIZE_MAX / sizeof(*texture->pixels)) return false;

    uint32_t *pixels = malloc(count * sizeof(*pixels));

    if (!pixels) return false;

    texture->width = width;
    texture->height = height;
    texture->pixels = pixels;

    return true;
}
void texture_shutdown(CPUTexture *texture) {
    if (texture != NULL) {
        free(texture->pixels);
        *texture = (CPUTexture){0};
    }
}

void texture_make_checker(CPUTexture *texture) {
    for (int y = 0; y < texture->height; ++y) {
        for (int x = 0; x < texture->width; ++x) {
            bool alternate = ((x / 8) + (y / 8)) & 1;

            size_t index = (size_t)y * texture->width + x;

            texture->pixels[index] =
                alternate ? 0x00FFFFFF : 0x00800080;
        }
    }
}

CPUTexture *texture_book_create(TextureBook *book, int width, int height)
{
    if (book->count >= TEXTURE_CAPACITY) return NULL;
    
    CPUTexture *texture = &book->textures[book->count];

    if (!texture_init(texture, width, height)) return NULL;

    ++book->count;

    return texture;
}

void texture_book_shutdown(TextureBook *book) {
    while (book->count > 0) {
        --book->count;
        texture_shutdown(&book->textures[book->count]);
    }
}

uint32_t texture_sample(const CPUTexture *texture, float u, float v)
{
    // assume valid texel and finite uv -- will error out otherwise
    u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
    v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);

    int x = (int)(u * texture->width);
    int y = (int)(v * texture->height);

    if (x >= texture->width)
        x = texture->width - 1;
    if (y >= texture->height)
        y = texture->height - 1;

    size_t index = (size_t)y * texture->width + x;
    return texture->pixels[index];
}

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

static RasterVertex make_raster_vertex(ProjectedVertex p, Point2 uv)
{
    return (RasterVertex){.screen = p.screen,.inverse_depth = p.inverse_depth,.u_over_depth = uv.x * p.inverse_depth,
        .v_over_depth = uv.y * p.inverse_depth};
}

static void fill_triangle(
    uint32_t *pixels,
    DepthBuffer *depth,
    int width,
    int height,
    int pitch,
    RasterVertex a,
    RasterVertex b,
    RasterVertex c,
    uint32_t color,
    const CPUTexture *texture)
{
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

    bool ab = inclusive_edge(a.screen, b.screen);
    bool bc = inclusive_edge(b.screen, c.screen);
    bool ca = inclusive_edge(c.screen, a.screen);

    for (int y = y0; y < y1; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)pixels + (size_t)y * pitch);

        for (int x = x0; x < x1; ++x) {
            Point2 p = {x + 0.5f, y + 0.5f};

            float e0 = edge(a.screen, b.screen, p);
            float e1 = edge(b.screen, c.screen, p);
            float e2 = edge(c.screen, a.screen, p);

            bool inside = (e0 > 0.0f || (e0 == 0.0f && ab)) && (e1 > 0.0f || (e1 == 0.0f && bc)) && (e2 > 0.0f || (e2 == 0.0f && ca));

            if (inside) {
                float weight_a = e1 * inverse_area;
                float weight_b = e2 * inverse_area;
                float weight_c = e0 * inverse_area;

                float q = weight_a * a.inverse_depth + weight_b * b.inverse_depth + weight_c * c.inverse_depth;
                size_t index = (size_t)y * depth->width + x;
                if (q > depth->values[index]) {
                    depth->values[index] = q;
                if (texture) {
                    float uq =weight_a * a.u_over_depth + weight_b * b.u_over_depth + weight_c * c.u_over_depth;
                    float vq = weight_a * a.v_over_depth + weight_b * b.v_over_depth + weight_c * c.v_over_depth;
                    float u = uq / q;
                    float v = vq / q;
                    row[x] = texture_sample(texture, u, v);
                }
                else {
                    row[x] = color;
                }
            }
        }
    }
}
}


bool draw_wireframe_mesh(uint32_t *pixels, int screen_width, int screen_height, 
    WireframeMesh mesh, Vector3 rotation, uint32_t color,
    Vector3 position,const Camera *camera,RenderScratch *scratch) {
    
    // check vertex count against scratch buffer
    if (mesh.vertex_count < 0 || mesh.vertex_count > RENDER_VERTEX_CAPACITY) {
        SDL_Log("Vertex count %d exceeds capacity %d", mesh.vertex_count, RENDER_VERTEX_CAPACITY);
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

        if (!camera_project(
            camera,
            world,
            screen_width,
            screen_height,
            &points[i])) {
                return false;
            }
        }
    // draw edges
    for (int i = 0; i < mesh.edge_count; ++i) {
        int a = mesh.edges[i].start_index;
        int b = mesh.edges[i].end_index;

        draw_line(
            pixels,
            screen_width,
            screen_height,
            (int)points[a].screen.x,
            (int)points[a].screen.y,
            (int)points[b].screen.x,
            (int)points[b].screen.y,
            color
        );
    }
    return true;
}

bool draw_mesh(uint32_t *pixels, int screen_width, int screen_height, 
    int pitch, WireframeMesh mesh, Vector3 rotation, uint32_t color,
    Vector3 position, const Camera *camera, RenderScratch *scratch, DepthBuffer *depth,  const CPUTexture *texture) {
            
    // check vertex count against scratch buffer
    if (mesh.vertex_count < 0 || mesh.vertex_count > RENDER_VERTEX_CAPACITY) {
        SDL_Log("Vertex count %d exceeds capacity %d", mesh.vertex_count, RENDER_VERTEX_CAPACITY);
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

        if (!camera_project(
            camera,
            world,
            screen_width,
            screen_height,
            &points[i])) {
                return false;
            }
        }
        // fill triangle in mesh
        for (int i = 0; i < mesh.triangle_count; ++i) {
            Triangle triangle = mesh.triangles[i];
            fill_triangle(
                pixels,
                depth,
                screen_width,
                screen_height,
                pitch,
                make_raster_vertex(points[triangle.a], triangle.uv_a),
                make_raster_vertex(points[triangle.b], triangle.uv_b),
                make_raster_vertex(points[triangle.c], triangle.uv_c),
                color,
                texture
            );
        }
    return true;
}


bool renderer_init(EngineRenderer* renderer, const char* title, int width, int height) {
    renderer->width = width;
    renderer->height = height;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    renderer->window = SDL_CreateWindow(title, width, height, 0);
    if (!renderer->window) return false;

    renderer->sdl_renderer = SDL_CreateRenderer(renderer->window, NULL);
    if (!renderer->sdl_renderer) return false;

    renderer->screen_texture = SDL_CreateTexture(
        renderer->sdl_renderer, 
        SDL_PIXELFORMAT_XRGB8888, 
        SDL_TEXTUREACCESS_STREAMING, 
        width, height
    );
    if (!renderer->screen_texture) return false;

    if (!depth_buffer_init(&renderer->depth, width, height)) {
        SDL_Log("Depth buffer initialization failed? Why?");
        renderer_shutdown(renderer);
        return false;
    }

    return true;
}

void renderer_shutdown(EngineRenderer* renderer) {
    depth_buffer_shutdown(&renderer->depth);
    SDL_DestroyTexture(renderer->screen_texture);
    SDL_DestroyRenderer(renderer->sdl_renderer);
    SDL_DestroyWindow(renderer->window);
    SDL_Quit();
}

void render_frame(EngineRenderer* renderer, const SceneObject *object, const Camera *camera) {
    uint32_t* pixels;
    int pitch;

    if (SDL_LockTexture(renderer->screen_texture, NULL, (void**)&pixels, &pitch)) {
        // Clear back-buffer using the context's dynamic dimensions
        for (int y = 0; y < renderer->height; ++y) {
            uint32_t *row = (uint32_t *)((uint8_t *)pixels + (size_t)y * pitch);
            for (int x = 0; x < renderer->width; ++x)
                row[x] = 0;
            }
        // clear depth buffer
        depth_buffer_clear(&renderer->depth);

        draw_mesh(
            pixels,
            renderer->width,
            renderer->height,
            pitch,
            *object->mesh,
            object->rotation,
            object->color,
            object->position,
            camera,
            &renderer->scratch,
            &renderer->depth,
            object->texture
        );
const CPUTexture *preview = object->texture;

if (preview) {
    for (int y = 0;
         y < preview->height && y + 10 < renderer->height;
         ++y) {

        uint32_t *row = (uint32_t *)(
            (uint8_t *)pixels + (size_t)(y + 10) * pitch
        );

        for (int x = 0;
             x < preview->width && x + 10 < renderer->width;
             ++x) {

            float u = (x + 0.5f) / preview->width;
            float v = (y + 0.5f) / preview->height;

            row[x + 10] = texture_sample(preview, u, v);
        }
    }
}
        SDL_UnlockTexture(renderer->screen_texture);
    }

    // Flush to the physical monitor screen
    SDL_SetRenderDrawColor(renderer->sdl_renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer->sdl_renderer);
    SDL_RenderTexture(renderer->sdl_renderer, renderer->screen_texture, NULL, NULL); 
    SDL_RenderPresent(renderer->sdl_renderer);
}

