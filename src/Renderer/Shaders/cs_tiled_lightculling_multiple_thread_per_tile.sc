#define WRITE_TILES

#include <bgfx_compute.sh>
#include "lights.sh"
#include "tiles.sh"

float getSignedDistanceFromPlane(vec3 p, vec4 eqn)
{
    return dot(eqn.xyz, p);
}

bool pointLightIntersectsTile(PointLight light, Tile tile, float halfZ)
{
    vec3 center = light.position;
    float r = light.radius;
    if(
        (getSignedDistanceFromPlane(center, tile.frustrumPlanes[0]) < r) &&
        (getSignedDistanceFromPlane(center, tile.frustrumPlanes[1]) < r) &&
        (getSignedDistanceFromPlane(center, tile.frustrumPlanes[2]) < r) &&
        (getSignedDistanceFromPlane(center, tile.frustrumPlanes[3]) < r)
    )
    {
        if(-center.z + u_zNear < r && center.z - halfZ < r)
            return true;
        if(-center.z + halfZ < r && center.z - u_zFar < r)
            return true;
    }

    return false;
}

#define NUM_THREADS_PER_TILE (TILES_X_THREADS * TILES_Y_THREADS)

SHARED uint sharedVisibleCount;

NUM_THREADS(TILES_X_THREADS, TILES_Y_THREADS, 1)
void main()
{
    uint tileIndex = getComputeIndex(gl_WorkGroupID.xy);

    Tile tile = getTile(tileIndex);
    uint tileOffset = getGridLightTileOffset(tileIndex);

    float halfZ = (u_zNear + u_zFar) / 2;

    if(gl_LocalInvocationIndex == 0)
    {
        sharedVisibleCount = 0;
    }

    barrier();

    uint lightCount = pointLightCount();
    for(int lightIndex = gl_LocalInvocationIndex; lightIndex < lightCount; lightIndex += NUM_THREADS_PER_TILE)
    {
        PointLight light = getPointLight(lightIndex);
        light.position = mul(u_view, vec4(light.position, 1.0)).xyz;
        light.radius = length(mul(u_view, vec4(light.radius, 0.0, 0.0, 0.0)));
        if(pointLightIntersectsTile(light, tile, halfZ))
        {
            uint offset;
            atomicFetchAndAdd(sharedVisibleCount, 1, offset);
            if(offset >= u_maxLightsPerTile)
                break;

            b_tileLightIndices[tileOffset + offset] = lightIndex;
        }
    }

    barrier();

    if(gl_LocalInvocationIndex == 0)
    {
        b_tileLightGrid[tileIndex] = sharedVisibleCount;
    }
}
