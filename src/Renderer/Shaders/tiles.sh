#ifndef TILES_SH_HEADER_GUARD
#define TILES_SH_HEADER_GUARD

#include <bgfx_shader.sh>
#include <bgfx_compute.sh>
#include "samplers.sh"
#include "util.sh"

#define TILES_X_THREADS 16
#define TILES_Y_THREADS 16
#define TILES_Z_THREADS 1

#define MAX_LIGHTS_PER_TILE 2048

#ifdef WRITE_TILES
    #define TILES_BUFFER BUFFER_RW
#else
    #define TILES_BUFFER BUFFER_RO
#endif

CLUSTER_BUFFER(b_tileLightIndices, uint, SAMPLER_CLUSTERS_LIGHTINDICES);


#endif