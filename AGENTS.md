# littleEngine development conventions

- This is a learning project. Explain changes and let the user implement them unless they request implementation. Do not make unrelated refactors.
- Follow `docs/c-object-model.md` for new subsystem interfaces and changes to existing interfaces. Existing transparent subsystem structs migrate only as part of explicitly requested work; do not rewrite them wholesale.
- Keep C11 for engine data and kernels, C++17 for coordination and bindings. Public C interfaces must compile from both C and C++, with `extern "C"` guards around function declarations.
- Use opaque structs for subsystem encapsulation, first-member base embedding for necessary shallow inheritance, and explicit constant operation tables for necessary runtime polymorphism outside hot loops.
- Keep entities, transforms, commands, snapshots, configuration values, and borrowed buffer views as plain data. Prefer composition; do not create a universal object root or a hierarchy for every engine type.
- Document ownership, allocation, lifetime, error behavior, and thread access for public APIs. No hidden allocations in tick, render, input, or per-element processing paths.
- Server owns authoritative World state. Client submits commands and consumes detached snapshots. Keep SDL out of World, physics, math, and CPU rasterization.
- Organize new modules by responsibility. Keep implementation headers private. Do not move the source tree during another active refactor; update and verify the build whenever files move.
- Verify relevant changes with focused builds/checks. State what was actually tested; do not claim runtime correctness from compilation alone.
