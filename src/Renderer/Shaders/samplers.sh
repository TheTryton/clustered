#ifndef SAMPLERS_SH_HEADER_GUARD
#define SAMPLERS_SH_HEADER_GUARD

// shared

#define SAMPLER_PBR_ALBEDO_LUT 0

#define SAMPLER_PBR_BASECOLOR 1
#define SAMPLER_PBR_METALROUGHNESS 2
#define SAMPLER_PBR_NORMAL 3
#define SAMPLER_PBR_OCCLUSION 4
#define SAMPLER_PBR_EMISSIVE 5

#define SAMPLER_LIGHTS_POINTLIGHTS 6

// per renderer

#define SAMPLER_DEFERRED_DIFFUSE_A 7
#define SAMPLER_DEFERRED_NORMAL 8
#define SAMPLER_DEFERRED_F0_METALLIC 9
#define SAMPLER_DEFERRED_EMISSIVE_OCCLUSION 10
#define SAMPLER_DEFERRED_DEPTH 11

#define SAMPLER_CLUSTERS_CLUSTERS 12
#define SAMPLER_CLUSTERS_LIGHTINDICES 13
#define SAMPLER_CLUSTERS_LIGHTGRID 14
#define SAMPLER_CLUSTERS_ATOMICINDEX 15

#define SAMPLER_TILES_TILES 12
#define SAMPLER_TILES_LIGHTINDICES 13
#define SAMPLER_TILES_LIGHTGRID 14
#define SAMPLER_TILES_ATOMICINDEX 15

#endif // SAMPLERS_SH_HEADER_GUARD
