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
    frustrum[0] = screen2Eye(vec4((gl_GlobalInvocationID.xy + vec2(0, 0)) * u_tileSize.xy, 1.0, 1.0));
    frustrum[1] = screen2Eye(vec4((gl_GlobalInvocationID.xy + vec2(1, 0)) * u_tileSize.xy, 1.0, 1.0));
    frustrum[2] = screen2Eye(vec4((gl_GlobalInvocationID.xy + vec2(1, 1)) * u_tileSize.xy, 1.0, 1.0));
    frustrum[3] = screen2Eye(vec4((gl_GlobalInvocationID.xy + vec2(0, 1)) * u_tileSize.xy, 1.0, 1.0));

    b_tiles[4 * tileIndex + 0] = createPlaneEquation(frustrum[0], frustrum[1]);
    b_tiles[4 * tileIndex + 1] = createPlaneEquation(frustrum[1], frustrum[2]);
    b_tiles[4 * tileIndex + 2] = createPlaneEquation(frustrum[2], frustrum[3]);
    b_tiles[4 * tileIndex + 3] = createPlaneEquation(frustrum[3], frustrum[0]);
}
