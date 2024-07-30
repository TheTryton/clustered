#define WRITE_TILES

#include <bgfx_compute.sh>
#include "tiles.sh"
#include "util.sh"

vec4 createPlaneEquation(vec4 b, vec4 c)
{
    return vec4(normalize(cross(b.xyz, c.xyz)), 0.0);
}

NUM_THREADS(TILES_X_THREADS, TILES_Y_THREADS, 1)
void main()
{
    // index calculation must match the inverse operation in the fragment shader (see getTileIndex)
    uint tileIndex = getComputeIndex(gl_GlobalInvocationID.xy);

    if(!isTileValid(tileIndex))
        return;

    vec4 frustrum[4];
    // calculate min (bottom left) and max (top right) xy in screen coordinates
    frustrum[0] = screen2Eye(vec4((gl_GlobalInvocationID.xy + vec2(0, 0)) * u_tileSize.xy, 1.0, 1.0));
    frustrum[1] = screen2Eye(vec4((gl_GlobalInvocationID.xy + vec2(1, 0)) * u_tileSize.xy, 1.0, 1.0));
    frustrum[2] = screen2Eye(vec4((gl_GlobalInvocationID.xy + vec2(1, 1)) * u_tileSize.xy, 1.0, 1.0));
    frustrum[3] = screen2Eye(vec4((gl_GlobalInvocationID.xy + vec2(0, 1)) * u_tileSize.xy, 1.0, 1.0));

    /*// -> eye coordinates
    // z is the camera far plane (1 in screen coordinates)
    vec3 minEye = screen2Eye(minScreen).xyz;
    vec3 maxEye = screen2Eye(maxScreen).xyz;

    // this calculates the intersection between:
    // - a line from the camera (origin) to the eye point (at the camera's far plane)
    // - the tile's z-planes (near + far)
    // we could divide by u_zFar as well
    vec3 minNear = minEye * u_zNear / u_zFar;
    vec3 minFar  = minEye * u_zFar  / u_zFar;
    vec3 maxNear = maxEye * u_zNear / u_zFar;
    vec3 maxFar  = maxEye * u_zFar  / u_zFar;

    // get extent of the tile in all dimensions (axis-aligned bounding box)
    // there is some overlap here but it's easier to calculate intersections with AABB
    vec3 minBounds = min(min(minNear, minFar), min(maxNear, maxFar));
    vec3 maxBounds = max(max(minNear, minFar), max(maxNear, maxFar));*/

    b_tiles[4 * tileIndex + 0] = createPlaneEquation(frustrum[0], frustrum[1]);
    b_tiles[4 * tileIndex + 1] = createPlaneEquation(frustrum[1], frustrum[2]);
    b_tiles[4 * tileIndex + 2] = createPlaneEquation(frustrum[2], frustrum[3]);
    b_tiles[4 * tileIndex + 3] = createPlaneEquation(frustrum[3], frustrum[0]);
}
