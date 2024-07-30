#include <bgfx_shader.sh>
#include "samplers.sh"
#include "clusters.sh"
#include "lights.sh"
#include "colormap.sh"

// G-Buffer
SAMPLER2D(s_texDepth,             SAMPLER_DEFERRED_DEPTH);

void main()
{
    vec2 texcoord = gl_FragCoord.xy / u_viewRect.zw;

    // get fragment position
    // rendering happens in view space
    vec4 screen = gl_FragCoord;
    screen.z = texture2D(s_texDepth, texcoord).x;

    // show light count per cluster

    uint cluster = getClusterIndex(screen);
    uint lightCount = getLightGridCount(cluster);

    vec3 lightCountColor = turboColormap(float(lightCount) / u_maxLightsPerCluster);
    gl_FragColor = vec4(lightCountColor, 1.0);
}
