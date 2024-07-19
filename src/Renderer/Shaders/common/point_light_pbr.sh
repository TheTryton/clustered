#ifndef POINT_LIGHT_LOGIC_SH_HEADER_GUARD
#define POINT_LIGHT_LOGIC_SH_HEADER_GUARD

#include "point_light.sh"
#include "math.sh"
#include "pbr.sh"

vec3 calculateRadiance(vec3 position, vec3 normal, vec3 normalizedPositionToEye, Material material, PointLight pointLight)
{
    float dist = distance(pointLight.position, position);
    float attenuation = smoothAttenuation(dist, pointLight.radius);
    if(attenuation > 0.0)
    {
        vec3 L = normalize(pointLight.position - position);
        vec3 radianceIn = pointLight.intensity * attenuation;
        float NoL = saturate(dot(normal, L));
        return BRDF(normalizedPositionToEye, L, normal, material) * radianceIn * NoL;
    }

    return vec3_splat(0.0);
}

#endif