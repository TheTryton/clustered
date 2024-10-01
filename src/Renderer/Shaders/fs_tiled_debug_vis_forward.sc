$input v_worldpos, v_normal, v_tangent, v_texcoord0

#define READ_MATERIAL

#include <bgfx_shader.sh>
#include "tiles.sh"
#include "lights.sh"
#include "colormap.sh"

void main()
{
    // show light count per tile
    uint tile = getTileIndex(gl_FragCoord);
    uint lightCount = getLightGridCount(tile);

    if(lightCount == u_maxLightsPerTile)
        ++lightCount;

    gl_FragColor = vec4(turboColormap(float(lightCount) / u_maxLightsPerTile), 1.0);
}
