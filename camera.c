#include "camera.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

bool camera_project(const Camera *camera, Vector3 world, int width, int height, ProjectedVertex *out)
{
    Vector3 delta = {world.x - camera->position.x, world.y - camera->position.y, world.z - camera->position.z};

    float sy = sinf(camera->yaw);
    float cy = cosf(camera->yaw);
    float sp = sinf(camera->pitch);
    float cp = cosf(camera->pitch);

    float x = cy * delta.x - sy * delta.z;
    float z = sy * delta.x + cy * delta.z;


    Vector3 view = {x, cp * delta.y - sp * z, sp * delta.y + cp * z};
    // setting up camera projection for depth testing
    if (view.z < camera->near_clip)
        return false;

    float q = 1.0f / view.z;

    out->screen.x = width * 0.5f
        + camera->focal_length_pixels * view.x * q;

    out->screen.y = height * 0.5f
        - camera->focal_length_pixels * view.y * q;

    out->inverse_depth = q;
    return true;
}

bool depth_buffer_init(DepthBuffer *buffer, int width, int height) {
    if (width <= 0 || height <= 0) return false;

    size_t w = (size_t)width;
    size_t h = (size_t)height;

    if (w > SIZE_MAX / h) return false;

    size_t count = w * h;
    
    if (count > SIZE_MAX / sizeof(*buffer->values)) return false;

    float *values = malloc(count * sizeof(*values));

    if (!values) return false;

    buffer->width = width;
    buffer->height = height;
    buffer->values = values;

    return true;
}

void depth_buffer_clear(DepthBuffer *buffer) {
    size_t count =(size_t)buffer->width*(size_t)buffer->height;
    for (size_t i = 0; i < count; ++i)
        buffer->values[i] = 0.0f;
}
void depth_buffer_shutdown(DepthBuffer *buffer) {
    if (buffer != NULL) {
        free(buffer->values);
        *buffer = (DepthBuffer){0};
    }
}

void camera_move(Camera *camera, float forward, float right, float speed, double dt){
    float length_squared = forward *forward + right * right;

    if (length_squared > 1.0f) {
        float inverse_length = 1.0f / sqrtf(length_squared);
        forward *= inverse_length;
        right *= inverse_length;
    }

    float distance = speed * (float)dt;

    float sy = sinf(camera->yaw);
    float cy = cosf(camera->yaw);

    camera->position.x += (forward * sy + right * cy) * distance;

    camera->position.z += (forward * cy - right * sy) * distance; 
}

void camera_apply_look(Camera *camera, float yaw_delta, float pitch_delta)
{
    const float pitch_limit = 1.55f;

    camera->yaw += yaw_delta;
    camera->pitch += pitch_delta;

    if (camera->pitch > pitch_limit) camera->pitch = pitch_limit;

    if (camera->pitch < -pitch_limit) camera->pitch = -pitch_limit;
}