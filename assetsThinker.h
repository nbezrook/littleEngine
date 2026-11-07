#ifndef LITTLEENGINE_ASSETS_THINKER_H
#define LITTLEENGINE_ASSETS_THINKER_H

#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

CPUTexture *texture_book_load_png(
    TextureBook *book,
    const char *path
);

#ifdef __cplusplus
}
#endif

#endif