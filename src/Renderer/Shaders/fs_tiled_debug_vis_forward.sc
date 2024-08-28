$input v_worldpos, v_normal, v_tangent, v_texcoord0

#define READ_MATERIAL

#include <bgfx_shader.sh>
#include "tiles.sh"
#include "pbr.sh"
#include "lights.sh"
#include "colormap.sh"

void main()
{
    PBRMaterial mat = pbrMaterial(v_texcoord0);

    // show light count per tile

    uint tile = getTileIndex(gl_FragCoord);
    uint lightCount = getLightGridCount(tile);

    if(lightCount == u_maxLightsPerTile)
        ++lightCount;

    vec3 radianceOut = getAmbientLight().irradiance * mat.diffuseColor * mat.occlusion;
    vec3 lightCountColor = turboColormap(float(lightCount) / u_maxLightsPerTile);
    gl_FragColor = vec4(radianceOut * 0.8 + lightCountColor * 0.2, 1.0);
}
