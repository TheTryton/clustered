#ifndef TILES_SH_HEADER_GUARD
#define TILES_SH_HEADER_GUARD

#include <bgfx_compute.sh>
#include "samplers.sh"
#include "util.sh"

// workgroup size of the culling compute shader
// D3D compute shaders only allow up to 1024 threads per workgroup
// GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS also only guarantees 1024
#define TILES_X_THREADS 16
#define TILES_Y_THREADS 16

#define MAX_LIGHTS_PER_TILE 2048

uniform vec4 u_tileSizeVec; // tile size in screen coordinates (pixels)
uniform vec4 u_tileCountVec; // count of tiles
uniform vec4 u_zNearFarVec;

#define u_tileSize     ((uvec2)u_tileSizeVec.xy)
#define u_tileCount    ((uvec2)u_tileCountVec.xy)
#define u_zNear        u_zNearFarVec.x
#define u_zFar         u_zNearFarVec.y

#ifdef WRITE_TILES
    #define TILE_BUFFER BUFFER_RW
#else
    #define TILE_BUFFER BUFFER_RO
#endif

// light indices belonging to tiles
TILE_BUFFER(b_tileLightIndices, uint, SAMPLER_TILES_LIGHTINDICES);
// for each tile: (start index in b_tileLightIndices, number of point lights, empty, empty)
TILE_BUFFER(b_tileLightGrid, uvec4, SAMPLER_TILES_LIGHTGRID);

// these are only needed for building tiles and light culling, not in the fragment shader
#ifdef WRITE_TILES
// list of tiles (2 vec4's each, min + max pos for AABB)
TILE_BUFFER(b_tiles, vec4, SAMPLER_TILES_TILES);
// atomic counter for building the light grid
// must be reset to 0 every frame
TILE_BUFFER(b_globalIndex, uint, SAMPLER_TILES_ATOMICINDEX);
#endif

struct Tile
{
    vec3 minBounds;
    vec3 maxBounds;
};

struct LightGrid
{
    uint offset;
    uint pointLights;
};

#ifdef WRITE_TILES
bool isTileValid(uint tileIndex)
{
    return tileIndex < u_tileCount.x * u_tileCount.y;
}

uint getComputeIndex(uvec2 globalInvocationID)
{
    uint tileIndex = globalInvocationID.y * u_tileCount.x + globalInvocationID.x;
    if(globalInvocationID.x >= u_tileCount.x || globalInvocationID.y >= u_tileCount.y)
        return u_tileCount.x * u_tileCount.y;
    return tileIndex;
}

Tile getTile(uint index)
{
    Tile tile;
    if(!isTileValid(index))
    {
        tile.minBounds = vec3(0.0, 0.0, 0.0);
        tile.maxBounds = vec3(0.0, 0.0, 0.0);
    }
    else
    {
        tile.minBounds = b_tiles[2 * index + 0].xyz;
        tile.maxBounds = b_tiles[2 * index + 1].xyz;
    }
    return tile;
}
#endif

LightGrid getLightGrid(uint tile)
{
    uvec4 gridvec = b_tileLightGrid[tile];
    LightGrid grid;
    grid.offset = gridvec.x;
    grid.pointLights = gridvec.y;
    return grid;
}

uint getGridLightIndex(uint start, uint offset)
{
    return b_tileLightIndices[start + offset];
}

// tile index from fragment position in window coordinates (gl_FragCoord)
uint getTileIndex(vec4 fragCoord)
{
    uvec2 indices = uvec2(fragCoord.xy / u_tileSize);
    uint tile = u_tileCount.x * indices.y +
                indices.x;
    return tile;
}

#endif // TILES_SH_HEADER_GUARD
