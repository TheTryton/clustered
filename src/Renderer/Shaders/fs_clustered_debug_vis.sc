$input v_worldpos, v_normal, v_tangent, v_texcoord0

#include <bgfx_shader.sh>
#include "clusters.sh"
#include "colormap.sh"

void main()
{
    // show light count per cluster

    uint cluster = getClusterIndex(gl_FragCoord);
    LightGrid grid = getLightGrid(cluster);

    int lights = int(grid.pointLights);
    // show possible clipping
    if(lights == 0)
        lights--;
    else if(lights == MAX_LIGHTS_PER_CLUSTER)
        lights++;

    vec3 lightCountColor = turboColormap(float(lights) / MAX_LIGHTS_PER_CLUSTER);
    gl_FragColor = vec4(lightCountColor, 1.0);
}
