#ifndef MATH_SH_HEADER_GUARD
#define MATH_SH_HEADER_GUARD

float distanceAttenuation(float distance)
{
    return 1.0 / max(distance * distance, 0.01 * 0.01);
}

float smoothAttenuation(float distance, float radius)
{
    float nom = saturate(1.0 - pow(distance / radius, 4.0));
    return nom * nom * distanceAttenuation(distance);
}

#endif