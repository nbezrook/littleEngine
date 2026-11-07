#ifndef LITTLEENGINE_ASSETS_THINKER_H
#define LITTLEENGINE_ASSETS_THINKER_H

#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decode a PNG, convert to XRGB8888, and copy pixels into book-owned storage.
 * @param book Non-null initialized texture book with a free slot.
 * @param path Non-null file path; relative paths use the process working directory.
 * @return Book-owned texture on success; NULL on load/conversion/lock/allocation failure.
 * @note Failure does not add a book entry. Temporary SDL surfaces are released automatically.
 * Alpha is discarded. The caller chooses a fallback on failure; this function does not create one.
 * The returned texture lives until texture_book_shutdown; no pixels alias an SDL surface.
 */
CPUTexture *texture_book_load_png(TextureBook *book, const char *path);

#ifdef __cplusplus
}
#endif

#endif