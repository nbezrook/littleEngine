#include "material.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// private
struct MaterialBook {
    MaterialDefinition *definitions;
    size_t count;
};

static bool valid_name(const char *name, size_t capacity)
{
    return name[0] != '\0' && memchr(name, '\0', capacity) != NULL;
}
// material book same concept as texturebook
MaterialBook *material_book_create(const MaterialDefinition *definitions,size_t count)
{
    if (count > MATERIAL_CAPACITY || (count && !definitions) || count > SIZE_MAX / sizeof(*definitions))
        return NULL;

    for (size_t i = 0; i < count; ++i) {
        if (!valid_name(definitions[i].name, sizeof(definitions[i].name)) || !valid_name(definitions[i].albedo_texture, sizeof(definitions[i].albedo_texture)))
            return NULL;
        for (size_t j = 0; j < i; ++j) {
            if (strcmp(definitions[i].name, definitions[j].name) == 0)
                return NULL;
        }
    }

    MaterialBook *book = malloc(sizeof(*book));
    if (!book) return NULL;
    book->definitions = NULL;
    book->count = count;
    if (count) {
        book->definitions = malloc(count * sizeof(*book->definitions));
        if (!book->definitions) {
            free(book);
            return NULL;
        }
        memcpy(book->definitions, definitions,
               count * sizeof(*book->definitions));
    }
    return book;
}

const MaterialDefinition *material_book_find(const MaterialBook *book,
                                            const char *name)
{
    if (!book || !name) return NULL;
    for (size_t i = 0; i < book->count; ++i) {
        if (strcmp(book->definitions[i].name, name) == 0)
            return &book->definitions[i];
    }
    return NULL;
}

size_t material_book_count(const MaterialBook *book)
{
    return book ? book->count : 0;
}

void material_book_destroy(MaterialBook *book)
{
    if (!book) return;
    free(book->definitions);
    free(book);
}
