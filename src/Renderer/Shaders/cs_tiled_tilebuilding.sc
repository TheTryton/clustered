#define WRITE_TILES

#include <bgfx_compute.sh>
#include "tiles.sh"
#include "util.sh"

NUM_THREADS(TILES_X_THREADS, TILES_Y_THREADS, 1)
void main()
{
    // index calculation must match the inverse operation in the fragment shader (see getTileIndex)
    uint tileIndex = getComputeIndex(gl_GlobalInvocationID.xy);

    if(!isTileValid(tileIndex))
        return;

    // calculate min (bottom left) and max (top right) xy in screen coordinates
    vec4 minScreen = vec4( gl_GlobalInvocationID.xy               * u_tileSize.xy, 1.0, 1.0);
    vec4 maxScreen = vec4((gl_GlobalInvocationID.xy + vec2(1, 1)) * u_tileSize.xy, 1.0, 1.0);

    // -> eye coordinates
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
    vec3 maxBounds = max(max(minNear, minFar), max(maxNear, maxFar));

    b_tiles[2 * tileIndex + 0] = vec4(minBounds, 1.0);
    b_tiles[2 * tileIndex + 1] = vec4(maxBounds, 1.0);
}
