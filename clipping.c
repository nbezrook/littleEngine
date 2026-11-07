#include "clipping.h"
#include <stdbool.h>

static ClipVertex intersect_near (ClipVertex a, ClipVertex b, float near_z) {
    float t = (near_z - a.position.z) / (b.position.z - a.position.z);

    return (ClipVertex){.position = {a.position.x + t * (b.position.x - a.position.x), a.position.y + t * (b.position.y - a.position.y), near_z},
    .uv = {a.uv.x + t * (b.uv.x - a.uv.x),a.uv.y + t * (b.uv.y - a.uv.y)}};
}

int clip_triangle_near(const ClipVertex input[3], float near_z, ClipVertex output[4])
{
    int count = 0;

    ClipVertex previous = input[2];
    bool previous_inside = previous.position.z >= near_z;

    for (int i = 0; i < 3; ++i) {
        ClipVertex current = input[i];
        bool current_inside = current.position.z >= near_z;

        if (previous_inside != current_inside) {
            output[count++] =
                intersect_near(previous, current, near_z);
        }

        if (current_inside) {
            output[count++] = current;
        }

        previous = current;
        previous_inside = current_inside;
    }

    return count;
}