$input v_worldpos, v_normal, v_tangent, v_texcoord0

#include <bgfx_shader.sh>
#include "clusters.sh"
#include "colormap.sh"

void main()
{
    // show light count per cluster

    uint cluster = getClusterIndex(gl_FragCoord);
    uint lightCount = getLightGridCount(cluster);

    vec3 lightCountColor = turboColormap(float(lightCount) / u_maxLightsPerCluster);
    gl_FragColor = vec4(lightCountColor, 1.0);
}
