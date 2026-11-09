#include "camera.h"
#include <stdbool.h>
#include <math.h>


bool camera_project(const Camera *camera, Vector3 world, int width, int height, ProjectedVertex *out)
{
    Vector3 view = camera_world_to_view(camera, world);
    return camera_project_view(camera, view, width, height, out);
}

Vector3 camera_world_to_view(const Camera *camera, Vector3 world) {
    Vector3 delta = {world.x - camera->position.x, world.y - camera->position.y, world.z - camera->position.z};

    float sy = sinf(camera->yaw);
    float cy = cosf(camera->yaw);
    float sp = sinf(camera->pitch);
    float cp = cosf(camera->pitch);

    float x = cy * delta.x - sy * delta.z;
    float z = sy * delta.x + cy * delta.z;
    
    Vector3 view = {x, cp * delta.y - sp * z, sp * delta.y + cp * z};
    return view;
}

bool camera_project_view(const Camera *camera, Vector3 view, int width, int height, ProjectedVertex *out){
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
bool camera_build_pixel_basis(CameraPixelBasis *out, const Camera *camera, int width, int height)
{
    if (!out || !camera || width<=0 || height<=0
        || !isfinite(camera->position.x) || !isfinite(camera->position.y)
        || !isfinite(camera->position.z) || !isfinite(camera->yaw)
        || !isfinite(camera->pitch) || !isfinite(camera->focal_length_pixels)
        || camera->focal_length_pixels<=0) return false;
    float sy=sinf(camera->yaw),cy=cosf(camera->yaw);
    float sp=sinf(camera->pitch),cp=cosf(camera->pitch);
    CameraPixelBasis result;
    result.origin=camera->position;
    result.right=(Vector3){cy,0,-sy};
    result.up=(Vector3){-sy*sp,cp,-cy*sp};
    result.forward=(Vector3){sy*cp,sp,cy*cp};
    result.inverse_focal=1/camera->focal_length_pixels;
    if (!isfinite(result.inverse_focal)) return false;
    result.center_x=width*.5f;result.center_y=height*.5f;
    *out=result;
    return true;
}
