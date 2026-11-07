#ifndef CAMERA_H
#define CAMERA_H

#include <stdbool.h>
#include "math/math_types.h"

typedef struct {
    Vector3 position;
    float yaw;
    float pitch;
    float focal_length_pixels;
    float near_clip;
} Camera;

typedef struct {
    float *values;
    int width;
    int height;
} DepthBuffer;

// dancing with a little C 
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Move horizontally relative to the camera's yaw, without collision.
 * @param camera Non-null camera to update; its Y position is unchanged.
 * @param forward Forward input, normally in [-1, 1]; negative moves backward.
 * @param right Strafe input, normally in [-1, 1]; negative moves left.
 * @param speed Nonnegative movement speed in world units per second.
 * @param dt Nonnegative simulation step in seconds.
 * @note Input vectors longer than one are normalized to prevent faster diagonal movement.
 * Pitch does not affect walking direction. Inputs must be finite.
 */
void camera_move(
    Camera *camera,
    float forward,
    float right,
    float speed,
    double dt
);

/**
 * @brief Transform a world point into camera space and project it onto the screen.
 * @param camera Non-null camera; yaw/pitch are radians and near_clip must be positive.
 * @param world Finite world-space position to project.
 * @param width Positive viewport width in pixels.
 * @param height Positive viewport height in pixels.
 * @param out Non-null output for screen coordinates and reciprocal camera-space Z.
 * @return False if the point is closer than near_clip; out is then unchanged.
 * @note Positive yaw looks toward +X; positive pitch looks up (+Y).
 * A successful projection may lie outside the viewport. This does not clip triangles.
 */
bool camera_project(const Camera *camera, Vector3 world, int width, int height, ProjectedVertex *out);
/**
 * @brief Allocate uninitialized reciprocal-depth storage for width * height pixels.
 * @param buffer Non-null destination with no live allocation; zero-initialize before first use.
 * @param width Positive pixel width.
 * @param height Positive pixel height.
 * @return True on success; false for invalid dimensions, size overflow, or allocation failure.
 * On failure the destination is unchanged. Call depth_buffer_clear before drawing.
 * @note The buffer owns values; release it with depth_buffer_shutdown.
 */
bool depth_buffer_init(DepthBuffer *buffer, int width, int height);
/**
 * @brief Set every reciprocal-depth value to zero before rendering a new frame.
 * @param buffer Non-null initialized buffer. Larger positive values represent nearer pixels.
 */
void depth_buffer_clear(DepthBuffer *buffer);
/**
 * @brief Free the owned depth array and reset the structure; NULL is accepted.
 * @param buffer Initialized or zero-initialized buffer. The structure itself is not freed.
 */
void depth_buffer_shutdown(DepthBuffer *buffer);
/**
 * @brief Add angular displacement and clamp pitch to [-1.55, 1.55] radians.
 * @param camera Non-null camera to update.
 * @param yaw_delta Finite radians to turn right; negative turns left.
 * @param pitch_delta Finite radians to look up; negative looks down.
 * @note These are displacements, not rates: do not multiply mouse-derived deltas by dt.
 * Yaw accumulates without wrapping.
 */
void camera_apply_look(Camera *camera, float yaw_delta, float pitch_delta);

/**
 * @brief Express a world-space point in the camera's coordinate system.
 * @param camera Non-null camera.
 * @param world World-space position.
 * @return Camera-space position, including points behind the near plane.
 */
Vector3 camera_world_to_view(const Camera *camera, Vector3 world);

/**
 * @brief Project a camera-space point into screen coordinates.
 * @param camera Non-null camera with positive near_clip.
 * @param view Camera-space position; do not pass a world-space point.
 * @param width Positive viewport width in pixels.
 * @param height Positive viewport height in pixels.
 * @param out Non-null destination, unchanged if projection fails.
 * @return False when view.z is less than near_clip; true otherwise.
 */
bool camera_project_view(const Camera *camera,Vector3 view,int width,int height,ProjectedVertex *out);

#ifdef __cplusplus
}
#endif

#endif
