#ifndef POINT_LIGHT_SH_HEADER_GUARD
#define POINT_LIGHT_SH_HEADER_GUARD

struct PointLight
{
    vec3 position;
    float __padding;
    vec3 intensity;
    float radius;
};

#endif