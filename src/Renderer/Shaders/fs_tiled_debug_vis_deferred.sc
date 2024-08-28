#include <bgfx_shader.sh>
#include "tiles.sh"
#include "lights.sh"
#include "colormap.sh"

// G-Buffer
SAMPLER2D(s_texDiffuseA,          SAMPLER_DEFERRED_DIFFUSE_A);
SAMPLER2D(s_texEmissiveOcclusion, SAMPLER_DEFERRED_EMISSIVE_OCCLUSION);
SAMPLER2D(s_texDepth,             SAMPLER_DEFERRED_DEPTH);

void main()
{
    vec2 texcoord = gl_FragCoord.xy / u_viewRect.zw;
    vec3 diffuse = texture2D(s_texDiffuseA, texcoord).xyz;
    float occlusion = texture2D(s_texEmissiveOcclusion, texcoord).w;

    // get fragment position
    // rendering happens in view space
    vec4 screen = gl_FragCoord;
    screen.z = texture2D(s_texDepth, texcoord).x;

    // show light count per tile

    uint tile = getTileIndex(screen);
    uint lightCount = getLightGridCount(tile);

    if(lightCount == u_maxLightsPerTile)
        ++lightCount;

    vec3 radianceOut = getAmbientLight().irradiance * diffuse * occlusion;
    vec3 lightCountColor = turboColormap(float(lightCount) / u_maxLightsPerTile);
    gl_FragColor = vec4(radianceOut * 0.8 + lightCountColor * 0.2, 1.0);
}
