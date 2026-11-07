#ifndef LITTLEENGINE_RENDERER_H
#define LITTLEENGINE_RENDERER_H

#include "include/SDL3/SDL.h"
#include "math/math_types.h"
#include "camera.h"

enum {
    RENDER_VERTEX_CAPACITY = 4096,
    TEXTURE_CAPACITY = 64
};

typedef struct {
    uint32_t *pixels;
    int width;
    int height;
} CPUTexture;

typedef struct {
    CPUTexture textures[TEXTURE_CAPACITY];
    size_t count;
} TextureBook;

typedef struct {
    ProjectedVertex projected[RENDER_VERTEX_CAPACITY];
    Vector3 view[RENDER_VERTEX_CAPACITY];
} RenderScratch;

typedef struct {
    const WireframeMesh *mesh;

    Vector3 position;
    Vector3 rotation;       // radians
    
    const CPUTexture *texture;

    uint32_t color;
} SceneObject;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *sdl_renderer;
    SDL_Texture *screen_texture;
    int width;
    int height;
    RenderScratch scratch;
    DepthBuffer depth;
} EngineRenderer;

/**
 * @brief Initialize SDL video and create the window, renderer, streaming image, and depth buffer.
 * @param renderer Non-null destination with pointer fields initially zero and no live resources.
 * @param title Window title string.
 * @param width Positive framebuffer width in pixels.
 * @param height Positive framebuffer height in pixels.
 * @return True on success; false on SDL or depth-buffer failure.
 * @note Main thread only. Some early SDL failure paths currently leave resources allocated.
 */
bool renderer_init(
    EngineRenderer *renderer,
    const char *title,
    int width,
    int height
);

    #ifdef __cplusplus
    extern "C" {
    #endif

    /**
     * @brief Clear and rasterize one object, draw its texture preview, and present the image.
     * @param renderer Non-null initialized renderer, used on the main thread.
     * @param objects Objects retrieved from server snapshot.
     * @param object_count Self-explanatory
     * @param camera Non-null camera used for projection.
     * @note CPU rasterization uses pitch-aware rows and a depth buffer cleared once per frame.
     * There is no triangle near-plane clipping; an unprojectable vertex rejects the mesh.
     */
    void render_frame(EngineRenderer *renderer, const SceneObject *objects, size_t object_count, const Camera *camera);

    /**
     * @brief Free depth storage, destroy SDL rendering resources, and call SDL_Quit.
     * @param renderer Non-null initialized or appropriately zero-initialized renderer.
     * @note Main thread only. SDL handles are not reset; do not call twice on the same state.
     */
    void renderer_shutdown(EngineRenderer *renderer);

    /**
     * @brief Allocate an uninitialized width-by-height array of 32-bit pixels.
     * @param texture Non-null destination with no live allocation.
     * @param width Positive texture width in texels.
     * @param height Positive texture height in texels.
     * @return False for invalid size, overflow, or allocation failure; destination is then unchanged.
     * @note On success texture owns pixels until texture_shutdown. Fill pixels before sampling.
     */
    bool texture_init(CPUTexture *texture, int width, int height);
    /**
     * @brief Free the owned pixel array and zero the texture; accepts NULL.
     * @param texture Initialized or zero-initialized texture. Does not free the structure.
     */
    void texture_shutdown(CPUTexture *texture);

    /**
     * @brief Fill an allocated texture with white/purple checkerboard cells eight texels wide.
     * @param texture Non-null initialized texture whose pixels will be overwritten.
     */
    void texture_make_checker(CPUTexture *texture);

    /**
     * @brief Allocate a texture in the next free slot of a fixed-capacity book.
     * @param book Non-null zero-initialized or valid book; must remain at a stable address while borrowed.
     * @param width Positive texture width in texels.
     * @param height Positive texture height in texels.
     * @return Book-owned texture, or NULL on capacity/allocation/size failure without changing count.
     * @note Fill pixels before sampling. Returned pointers remain valid until book shutdown.
     * Do not individually destroy book entries; let texture_book_shutdown release them.
     */
    CPUTexture *texture_book_create(TextureBook *book, int width, int height);
    /**
     * @brief Destroy all book-owned textures in reverse creation order and set count to zero.
     * @param book Non-null zero-initialized or valid book; all borrowed texture pointers become invalid.
     */
    void texture_book_shutdown(TextureBook *book);

    /**
     * @brief Sample one nearest texel after clamping normalized UVs to [0, 1].
     * @param texture Non-null texture with positive dimensions and initialized pixel data.
     * @param u Finite horizontal coordinate; zero selects the left edge.
     * @param v Finite vertical coordinate; zero selects the top edge.
     * @return Packed pixel color. Does not filter, wrap, or blend alpha.
     */
    uint32_t texture_sample(const CPUTexture *texture, float u, float v);

    #ifdef __cplusplus
    }
    #endif

#endif