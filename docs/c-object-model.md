# C subsystem object model

These conventions apply to new subsystem interfaces. Existing code is migrated incrementally when requested. They do not require changes to the framebuffer work currently in progress.

## Encapsulation

A public header declares `typedef struct Foo Foo;` and functions taking `Foo *self` or `const Foo *self`. Put the complete structure in its `.c` file, or in an implementation-only header if several implementations genuinely need its layout. Include a module's own public header in its implementation. Callers must not include implementation headers.

Opaque objects cannot be declared by value or sized by callers. Provide an explicit lifetime strategy: a documented startup `foo_create`/`foo_destroy` pair, a subsystem-owned pool, or a carefully specified caller-storage API. Opacity does not itself require heap allocation. Do not expose an arbitrary byte-buffer size or hide allocations in ordinary operations merely to imitate C++ syntax.

Ordinary values remain transparent: vectors, transforms, component records, commands, snapshots, configuration and borrowed buffer views. Keep resource ownership separate from freely copied values. Opaque structs prevent accidental field access through the public header; they are not a security boundary.

## Base embedding

Prefer composition. Use shallow, single-base embedding when two implementations actually need one interface. Define the complete base in an internal header, then make it the first ordinary member of each derived struct:

```c
struct FileSink {
    LogSink base;
    /* Concrete implementation data follows. */
};
_Static_assert(offsetof(struct FileSink, base) == 0, "base must be first");
```

Upcast with `&derived->base`. A callback may convert its base pointer back to the enclosing concrete type only when the object really is that concrete type. The C11 draft, section 6.7.2.1 paragraph 15, guarantees conversion between a structure pointer and its initial member. It does not authorize arbitrary casts between unrelated structures with similar fields.

Do not use packing pragmas, memcpy-based casts, or a universal Object base. First-member embedding means normal structure layout, not packed layout. Do not copy or slice owning objects by assigning their base or concrete structures.

## Runtime dispatch

Put a pointer to a `static const` operations table in the base. All callbacks use exactly the declared base-pointer signature; convert to the concrete type inside the callback. Never cast an incompatible function pointer to fit a table.

Use dispatch for interchangeable subsystem implementations: an asset source, a log destination, or a presentation backend. Keep per-pixel, per-vertex and per-entity inner loops as ordinary C functions over contiguous data. An interface call once per batch is acceptable when justified. Do not add a vtable to a subsystem that has no need for runtime substitution.

Initialize all state and the operations pointer before publishing an object. Required callbacks must be populated. Optional callbacks need an explicit documented fallback.

Destruction through the base must dispatch to the concrete implementation, which releases its resources and storage according to its allocation contract. Never generically `free(base)` without that contract. Document whether an API owns or borrows returned pointers. Destroying an object invalidates every borrowed alias; setting one pointer to NULL cannot clear other aliases.

## Ownership, errors, and concurrency

Each API documents allocation points, destruction, failure behavior, and permitted thread access. Constructors must clean up partial initialization. Prefer staged initialization that commits on success. Destruction may accept NULL; ordinary operations must specify valid inputs.

An opaque struct or vtable provides no automatic thread safety. Specify an owning thread or synchronization boundary. Workers borrow immutable inputs and disjoint outputs for a bounded job; finish all jobs before destroying or reconfiguring those resources. Keep SDL calls on threads required by the SDL API.

## Source organization

Folders express responsibility and header visibility; they do not change runtime performance by themselves. A possible gradual layout is:

```text
src/
    core/          engine orchestration and lifecycle
    server/        authoritative simulation and World
    client/        commands, snapshots, camera presentation
    math/          independent math
    render/        CPU rasterizer, textures, framebuffer
    assets/        catalog and file decoding
    platform/sdl/  SDL input, windows and presentation
include/littleengine/  deliberately supported public C interfaces
docs/
IndexOfMiracles/    demo game data, separate from engine code
build/             generated objects and executable
```

Module-private headers stay beside their implementations. Only supported external interfaces belong in the public include tree. Preserve third-party licenses and keep vendor code distinguishable. Do not move files solely to match this illustration.

When sources move, update explicit build inputs and include paths together. Preserve source paths in object paths, for example `src/render/texture.c` becomes `build/render/texture.o`; the current multi-source `gcc -c` command otherwise places objects by basename in one directory. Compile and link the C/C++ boundary after each migration.

## Verification

Check public headers from both C11 and C++17. For actual inheritance interfaces, assert first-member layout and exercise dispatch and destruction. Verify allocation failures and resource cleanup where relevant. No generic class macro framework is required.

Reference: https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1570.pdf
