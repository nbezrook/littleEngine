#include "raycast.h"
#include <math.h>
// f-ing SLOW, exactly what a GPU should be doing
// https://fileadmin.cs.lth.se/cs/Personal/Tomas_Akenine-Moller/pubs/raytri_tam.pdf
bool raycast_triangle(const RaySegment *ray, Vector3 a, Vector3 b, Vector3 c, float *out_distance)
{
    if (!ray) return false;
    double ex=(double)b.x-a.x, ey=(double)b.y-a.y, ez=(double)b.z-a.z;
    double fx=(double)c.x-a.x, fy=(double)c.y-a.y, fz=(double)c.z-a.z;
    double px=(double)ray->direction.y*fz-(double)ray->direction.z*fy;
    double py=(double)ray->direction.z*fx-(double)ray->direction.x*fz;
    double pz=(double)ray->direction.x*fy-(double)ray->direction.y*fx;

    double determinant=ex*px+ey*py+ez*pz;
    double scale=sqrt((ex*ex+ey*ey+ez*ez)*(fx*fx+fy*fy+fz*fz));

    if (!isfinite(scale) || scale==0 || fabs(determinant)<=scale*1e-12) return false;
    double tx=(double)ray->origin.x-a.x, ty=(double)ray->origin.y-a.y, tz=(double)ray->origin.z-a.z;

    double inverse=1/determinant;
    double u=(tx*px+ty*py+tz*pz)*inverse;
    if (u < -1e-10 || u > 1+1e-10) return false;

    double qx=ty*ez-tz*ey, qy=tz*ex-tx*ez, qz=tx*ey-ty*ex;
    double v=((double)ray->direction.x*qx+(double)ray->direction.y*qy+(double)ray->direction.z*qz)*inverse;

    if (v < -1e-10 || u+v > 1+1e-10) return false;

    double distance=(fx*qx+fy*qy+fz*qz)*inverse;
    if (!isfinite(distance) || distance<ray->min_distance || distance>ray->max_distance) return false;
    if (out_distance) *out_distance=(float)distance;

    return true;
}
