$input v_worldpos, v_normal, v_tangent, v_texcoord0

#define READ_MATERIAL

#include <bgfx_shader.sh>
#include "tiles.sh"
#include "pbr.sh"
#include "lights.sh"
#include "colormap.sh"

void main()
{
    // show light count per tile

    PBRMaterial mat = pbrMaterial(v_texcoord0);

    uint tile = getTileIndex(gl_FragCoord);
    LightGrid grid = getLightGrid(tile);

    int lights = int(grid.pointLights);
    // show possible clipping
    if(lights == 0)
        lights--;
    else if(lights == MAX_LIGHTS_PER_TILE)
        lights++;

    vec3 radianceOut = getAmbientLight().irradiance * mat.diffuseColor * mat.occlusion;
    vec3 lightCountColor = turboColormap(float(lights) / MAX_LIGHTS_PER_TILE);
    gl_FragColor = vec4(radianceOut * 0.8 + lightCountColor * 0.2, 1.0);
    //gl_FragColor = vec4((vec2)uvec2(gl_FragCoord.xy / u_tileSize) / u_tileCount, 1.0, 1.0);
}
