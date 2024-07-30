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

NUM_THREADS(TILES_X_THREADS, TILES_Y_THREADS, 1)
void main()
{
    uint visibleCount = 0;

    uint tileIndex = getComputeIndex(gl_GlobalInvocationID.xy);
    if(!isTileValid(tileIndex))
        return;

    Tile tile = getTile(tileIndex);
    uint tileOffset = getGridLightTileOffset(tileIndex);

    float halfZ = (u_zNear + u_zFar) / 2;

    uint lightCount = pointLightCount();
    for(int lightIndex = 0; lightIndex < lightCount; lightIndex++)
    {
        PointLight light = getPointLight(lightIndex);
        light.position = mul(u_view, vec4(light.position, 1.0)).xyz;
        light.radius = length(mul(u_view, vec4(light.radius, 0.0, 0.0, 0.0)));
        if(pointLightIntersectsTile(light, tile, halfZ))
        {
            b_tileLightIndices[tileOffset + visibleCount] = lightIndex;
            visibleCount++;
        }

        if(visibleCount == u_maxLightsPerTile)
            break;
    }

    b_tileLightGrid[tileIndex] = visibleCount;
}
