#define WRITE_TILES

#include <bgfx_compute.sh>
#include "lights.sh"
#include "tiles.sh"

// compute shader to cull lights against tile bounds
// builds a light grid that holds indices of lights for each tile
// largely inspired by http://www.aortiz.me/2018/12/21/CG.html

// point lights only for now
bool pointLightIntersectsTile(PointLight light, Tile tile);

#define GROUP_SIZE (TILES_X_THREADS * TILES_Y_THREADS)

// light cache for the current workgroup
// group shared memory has lower latency than global memory

// there's no guarantee on the available shared memory
// as a guideline the minimum value of GL_MAX_COMPUTE_SHARED_MEMORY_SIZE is 32KB
// with a workgroup size of 16*8*4 this is 64 bytes per light
// however, using all available memory would limit the compute shader invocation to only 1 workgroup
SHARED PointLight lights[GROUP_SIZE];

NUM_THREADS(TILES_X_THREADS, TILES_Y_THREADS, 1)
void main()
{
    // local thread variables
    // hold the result of light culling for this tile
    uint visibleLights[MAX_LIGHTS_PER_TILE];
    uint visibleCount = 0;

    // the way we calculate the index doesn't really matter here since we write to the same index in the light grid as we read from the tile buffer
    uint tileIndex = getComputeIndex(gl_GlobalInvocationID.xy);
    Tile tile = getTile(tileIndex);

    // we have a cache of GROUP_SIZE lights
    // have to run this loop several times if we have more than GROUP_SIZE lights
    uint lightCount = pointLightCount();
    uint lightOffset = 0;
    while(lightOffset < lightCount)
    {
        // wait for all threads to enter this section in case multiple light
        // copies are required
        barrier();

        // read GROUP_SIZE lights into shared memory
        // each thread copies one light
        uint batchSize = min(GROUP_SIZE, lightCount - lightOffset);

        if(uint(gl_LocalInvocationIndex) < batchSize)
        {
            uint lightIndex = lightOffset + gl_LocalInvocationIndex;
            PointLight light = getPointLight(lightIndex);
            // transform to view space (expected by pointLightAffectsTile)
            // do it here once rather than for each tile later
            light.position = mul(u_view, vec4(light.position, 1.0)).xyz;
            light.radius = length(mul(u_view, vec4(light.radius, 0.0, 0.0, 0.0)));
            lights[gl_LocalInvocationIndex] = light;
        }

        // wait for all threads to finish copying
        barrier();

        // each thread is one tile and checks against all lights in the cache
        for(uint i = 0; i < batchSize; i++)
        {
            if(visibleCount < MAX_LIGHTS_PER_TILE && pointLightIntersectsTile(lights[i], tile))
            {
                visibleLights[visibleCount] = lightOffset + i;
                visibleCount++;
            }
        }

        lightOffset += batchSize;
    }

    // wait for all threads to finish checking lights
    barrier();

    if(!isTileValid(tileIndex))
        return;

    // get a unique index into the light index list where we can write this tile's lights
    uint offset = 0;
    atomicFetchAndAdd(b_globalIndex[0], visibleCount, offset);
    // copy indices of lights
    for(uint i = 0; i < visibleCount; i++)
    {
        b_tileLightIndices[offset + i] = visibleLights[i];
    }
    // write light grid for this tile
    b_tileLightGrid[tileIndex] = uvec4(offset, visibleCount, 0, 0);
}

// check if light radius extends into the tile
bool pointLightIntersectsTile(PointLight light, Tile tile)
{
    // NOTE: expects light.position to be in view space like the tile bounds
    // global light list has world space coordinates, but we transform the
    // coordinates in the shared array of lights after copying

    // get closest point to sphere center
    vec3 closest = max(tile.minBounds, min(light.position, tile.maxBounds));
    // check if point is inside the sphere
    vec3 dist = closest - light.position;
    return dot(dist, dist) <= (light.radius * light.radius);
}
