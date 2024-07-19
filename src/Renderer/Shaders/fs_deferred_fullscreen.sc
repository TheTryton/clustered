#include "common.sh"
#include <bgfx_shader.sh>
#include "samplers.sh"
#include "pbr.sh"
#include "lights.sh"
#include "util.sh"

// G-Buffer
SAMPLER2D(s_texDiffuseA,          SAMPLER_DEFERRED_DIFFUSE_A);
SAMPLER2D(s_texPosition,          SAMPLER_DEFERRED_POSITION);
SAMPLER2D(s_texNormal,            SAMPLER_DEFERRED_NORMAL);
SAMPLER2D(s_texF0Metallic,        SAMPLER_DEFERRED_F0_METALLIC);
SAMPLER2D(s_texEmissiveOcclusion, SAMPLER_DEFERRED_EMISSIVE_OCCLUSION);
SAMPLER2D(s_texDepth,             SAMPLER_DEFERRED_DEPTH);

uniform vec4 u_cameraPosition;

void main()
{
    vec2 texcoord = gl_FragCoord.xy / u_viewRect.zw;
    vec4 diffuseA = texture2D(s_texDiffuseA, texcoord);
    vec3 position = texture2D(s_texPosition, texcoord).xyz;
    vec3 normal = unpackNormal(texture2D(s_texNormal, texcoord).xy);
    vec4 F0metallic = texture2D(s_texF0Metallic, texcoord);
    vec4 emissiveOcclusion = texture2D(s_texEmissiveOcclusion, texcoord);

    vec3 diffuse = diffuseA.xyz;
    vec3 emissive = emissiveOcclusion.xyz;
    float occlusion = emissiveOcclusion.w;

    // unpack material parameters used by the PBR BRDF function
    PBRMaterial mat;
    mat.diffuseColor = diffuse;
    mat.a = diffuseA.w;
    mat.F0 = F0metallic.xyz;
    mat.metallic = F0metallic.w;

    vec3 V = normalize(u_cameraPosition.xyz - position);

    // lighting

    vec3 radianceOut = vec3_splat(0.0);

    // point lights

    for(uint lightIndex = 0; lightIndex < u_pointLightCount; ++lightIndex)
    {
        PointLight light = getPointLight(lightIndex);

        float dist = distance(light.position, position);
        float attenuation = smoothAttenuation(dist, light.radius);
        if(attenuation > 0.0)
        {
            vec3 L = normalize(light.position - position);
            vec3 radianceIn = light.intensity * attenuation;
            float NoL = saturate(dot(normal, L));
            radianceOut += BRDF(V, L, normal, mat) * radianceIn * NoL;
        }
    }

    // ambient + emissive

    radianceOut += getAmbientLight().irradiance * diffuse * occlusion;
    radianceOut += emissive;

    gl_FragColor = vec4(texcoord, 0.0, 1.0);
}
