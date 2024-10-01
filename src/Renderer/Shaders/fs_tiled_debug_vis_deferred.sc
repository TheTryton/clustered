#include <bgfx_shader.sh>
#include "tiles.sh"
#include "lights.sh"
#include "colormap.sh"

SAMPLER2D(s_texDepth,             SAMPLER_DEFERRED_DEPTH);

void main()
{
    vec2 texcoord = gl_FragCoord.xy / u_viewRect.zw;

    // get fragment position
    // rendering happens in view space
    vec4 screen = gl_FragCoord;
    screen.z = texture2D(s_texDepth, texcoord).x;

    // show light count per tile

    uint tile = getTileIndex(screen);
    uint lightCount = getLightGridCount(tile);

    if(lightCount == u_maxLightsPerTile)
        ++lightCount;

    gl_FragColor = vec4(turboColormap(float(lightCount) / u_maxLightsPerTile), 1.0);
}
