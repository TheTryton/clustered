#pragma once

#include <cstdint>

class Samplers
{
    // these should be the same as in samplers.sh for shaders

public:
    static const uint8_t PBR_ALBEDO_LUT = 0;

    static const uint8_t PBR_BASECOLOR = 1;
    static const uint8_t PBR_METALROUGHNESS = 2;
    static const uint8_t PBR_NORMAL = 3;
    static const uint8_t PBR_OCCLUSION = 4;
    static const uint8_t PBR_EMISSIVE = 5;

    static const uint8_t LIGHTS_POINTLIGHTS = 6;

    static const uint8_t DEFERRED_DIFFUSE_A = 7;
    static const uint8_t DEFERRED_NORMAL = 8;
    static const uint8_t DEFERRED_F0_METALLIC = 9;
    static const uint8_t DEFERRED_EMISSIVE_OCCLUSION = 10;
    static const uint8_t DEFERRED_DEPTH = 11;

    static const uint8_t TILES_TILES = 12;
    static const uint8_t TILES_LIGHTINDICES = 13;
    static const uint8_t TILES_LIGHTGRID = 14;
    static const uint8_t TILES_ATOMICINDEX = 15;

    static const uint8_t CLUSTERS_CLUSTERS = 12;
    static const uint8_t CLUSTERS_LIGHTINDICES = 13;
    static const uint8_t CLUSTERS_LIGHTGRID = 14;
    static const uint8_t CLUSTERS_ATOMICINDEX = 15;
};
