#ifndef LITTLEENGINE_MATERIAL_H
#define LITTLEENGINE_MATERIAL_H
#include <stddef.h>
// light weight little materials
enum {
    MATERIAL_NAME_BYTES = 64,
    MATERIAL_TEXTURE_NAME_BYTES = 64,
    MATERIAL_CAPACITY = 64
};

typedef struct {
    char name[MATERIAL_NAME_BYTES];
    char albedo_texture[MATERIAL_TEXTURE_NAME_BYTES];
} MaterialDefinition;

typedef struct MaterialBook MaterialBook;

#ifdef __cplusplus
extern "C" {
#endif
/**
 * Copy definitions into a newly owned immutable book.
 * Allocates at startup/loading time; count must be <= MATERIAL_CAPACITY.
 * count==0 permits NULL definitions. Rejects empty/unterminated names and
 * duplicate material names. Returns NULL on invalid input/allocation failure.
 */
MaterialBook *material_book_create(const MaterialDefinition *definitions,
                                  size_t count);
/** Load materials.json. Allocates a book plus temporary parser storage.
 * Returns NULL on failure; never lets a C++ exception cross this C API.
 * Does not load images or check texture-catalog membership.
 */
MaterialBook *material_book_load_json(const char *path);
/** Borrow a definition, or NULL for an unknown name/NULL argument.
 * Read-only lookup; no allocation. Result lives until book destruction.
 */
const MaterialDefinition *material_book_find(const MaterialBook *book,
                                            const char *name);
/** Returns zero for NULL. */
size_t material_book_count(const MaterialBook *book);
/** Free the book and its definitions; accepts NULL. Invalidates borrowed
 * pointers. Immutable reads may run concurrently after publication, but
 * destruction must wait until every reader has finished.
 */
void material_book_destroy(MaterialBook *book);
#ifdef __cplusplus
}
#endif
#endif
