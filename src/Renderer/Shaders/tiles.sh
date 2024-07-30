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

uniform vec4 u_tileSizeVec; // tile size in screen coordinates (pixels)
uniform vec4 u_tileCountVec; // count of tiles
uniform vec4 u_zNearFarVec;

#define u_maxLightsPerTile   ((uint)u_tileSizeVec.z)
#define u_tileSize           ((uvec2)u_tileSizeVec.xy)
#define u_tileCount          ((uvec2)u_tileCountVec.xy)
#define u_zNear              u_zNearFarVec.x
#define u_zFar               u_zNearFarVec.y

#ifdef WRITE_TILES
    #define TILE_BUFFER BUFFER_RW
#else
    #define TILE_BUFFER BUFFER_RO
#endif

// light indices belonging to tiles
TILE_BUFFER(b_tileLightIndices, uint, SAMPLER_TILES_LIGHTINDICES);
// for each tile: number of point lights
TILE_BUFFER(b_tileLightGrid, uint, SAMPLER_TILES_LIGHTGRID);

// these are only needed for building tiles and light culling, not in the fragment shader
#ifdef WRITE_TILES
// list of tiles (4 vec4's each, frustrum planes)
TILE_BUFFER(b_tiles, vec4, SAMPLER_TILES_TILES);
#endif

struct Tile
{
    vec4 frustrumPlanes[4];
};

#ifdef WRITE_TILES
bool isTileValid(uint tileIndex)
{
    return tileIndex < u_tileCount.x * u_tileCount.y;
}

uint getComputeIndex(uvec2 tileIndex2D)
{
    uint tileIndex = tileIndex2D.y * u_tileCount.x + tileIndex2D.x;
    if(tileIndex2D.x >= u_tileCount.x || tileIndex2D.y >= u_tileCount.y)
        return u_tileCount.x * u_tileCount.y;
    return tileIndex;
}

Tile getTile(uint index)
{
    Tile tile;
    if(!isTileValid(index))
    {
        tile.frustrumPlanes[0] = vec4_splat(0.0);
        tile.frustrumPlanes[1] = vec4_splat(0.0);
        tile.frustrumPlanes[2] = vec4_splat(0.0);
        tile.frustrumPlanes[3] = vec4_splat(0.0);
    }
    else
    {
        tile.frustrumPlanes[0] = b_tiles[4 * index + 0];
        tile.frustrumPlanes[1] = b_tiles[4 * index + 1];
        tile.frustrumPlanes[2] = b_tiles[4 * index + 2];
        tile.frustrumPlanes[3] = b_tiles[4 * index + 3];
    }
    return tile;
}
#endif

uint getLightGridCount(uint tile)
{
    return b_tileLightGrid[tile];
}

uint getGridLightTileOffset(uint tile)
{
    return tile * u_maxLightsPerTile;
}

uint getGridLightIndex(uint tileOffset, uint offset)
{
    return b_tileLightIndices[tileOffset + offset];
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
