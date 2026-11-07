#include "renderer.h"
#include "math/math.c"
#include "math/math_types.h"
#include "math/primitives.h"
#include "camera.h"

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

static void fill_triangle(
    uint32_t *pixels,
    int width,
    int height,
    int pitch,
    Point2 a,
    Point2 b,
    Point2 c,
    uint32_t color)
{
    float area = edge(a,b,c);

    //collinear vertices don't enclose tri
    if (area == 0.0f) return;

    // interior edge values > 0
    if (area < 0.0f) {
        Point2 temp = b;
        b = c;
        c = temp;
    }

    // simple c comparison -- apparently the fastest way to do it, checked against assembly
    float min_x = min_finite(a.x, min_finite(b.x, c.x));
    float min_y = min_finite(a.y, min_finite(b.y, c.y));
    float max_x = max_finite(a.x, max_finite(b.x, c.x));
    float max_y = max_finite(a.y, max_finite(b.y, c.y));

    int x0 = (int)max_finite(0.0f, min_finite((float)width, floorf(min_x)));
    int y0 = (int)max_finite(0.0f, min_finite((float)height, floorf(min_y)));
    int x1 = (int)max_finite(0.0f, min_finite((float)width, ceilf(max_x)));
    int y1 = (int)max_finite(0.0f, min_finite((float)height, ceilf(max_y)));

    bool ab = inclusive_edge(a, b);
    bool bc = inclusive_edge(b, c);
    bool ca = inclusive_edge(c, a);

    for (int y = y0; y < y1; ++y) {
        uint32_t *row = (uint32_t *)((uint8_t *)pixels + (size_t)y * pitch);

        for (int x = x0; x < x1; ++x) {
            Point2 p = {x + 0.5f, y + 0.5f};

            float e0 = edge(a, b, p);
            float e1 = edge(b, c, p);
            float e2 = edge(c, a, p);

            bool inside = (e0 > 0.0f || (e0 == 0.0f && ab)) && (e1 > 0.0f || (e1 == 0.0f && bc)) && (e2 > 0.0f || (e2 == 0.0f && ca));

            if (inside) row[x] = color;
        }
    }
}


bool draw_wireframe_mesh(
    uint32_t *pixels, 
    int screen_width, 
    int screen_height, 
    WireframeMesh mesh, 
    Vector3 rotation, 
    uint32_t color,
    Vector3 position,
    const Camera *camera,
    RenderScratch *scratch) {
    
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

        // draw cute little cube
        draw_wireframe_mesh(
            pixels,
            renderer->width,
            renderer->height,
            *object->mesh,
            object->rotation,
            object->color,
            object->position,
            camera,
            &renderer->scratch
        );

        // draw little test triangle
        fill_triangle(
            pixels,
            renderer->width,
            renderer->height,
            pitch,
            (Point2){100, 100},
            (Point2){500, 140},
            (Point2){260, 420},
            0x0000FF00
        );

        SDL_UnlockTexture(renderer->screen_texture);
    }

    // Flush to the physical monitor screen
    SDL_SetRenderDrawColor(renderer->sdl_renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer->sdl_renderer);
    SDL_RenderTexture(renderer->sdl_renderer, renderer->screen_texture, NULL, NULL); 
    SDL_RenderPresent(renderer->sdl_renderer);
}

