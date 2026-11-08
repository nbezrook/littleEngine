#ifndef LITTLEENGINE_TEXTURE_H
#define LITTLEENGINE_TEXTURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { 
    TEXTURE_CAPACITY = 64 
};

typedef struct {
    uint32_t *pixels;
    int width;
    int height;
    bool repeat;
} CPUTexture;

enum {
    TEXTURE_NAME_BYTES = 64,
    TEXTURE_PATH_BYTES = 512
};

typedef enum {
    TEXTURE_UNLOADED,
    TEXTURE_READY,
    TEXTURE_FAILED
} TextureLoadState;

typedef struct {
    char name[TEXTURE_NAME_BYTES];
    char filename[TEXTURE_PATH_BYTES];
    const CPUTexture *texture;
    bool repeat;
    TextureLoadState state;
} TextureRecord;

typedef struct {
    CPUTexture textures[TEXTURE_CAPACITY];
    size_t count;

    TextureRecord records[TEXTURE_CAPACITY];
    size_t record_count;

    const CPUTexture *fallback;
} TextureBook;

#ifdef __cplusplus
extern "C" {
#endif

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
 * @brief Sample one nearest texel, wrapping or clamping according to repeat.
 * @param texture Non-null texture with positive dimensions and initialized pixel data.
 * @param u Finite horizontal coordinate; zero selects the left edge.
 * @param v Finite vertical coordinate; zero selects the top edge.
 * @return Packed pixel color. Does not filter or blend alpha.
 */
uint32_t texture_sample(const CPUTexture *texture, float u, float v);

#ifdef __cplusplus
}
#endif

#endif
