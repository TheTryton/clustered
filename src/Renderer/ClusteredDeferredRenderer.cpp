#include "ClusteredDeferredRenderer.h"

#include "Scene/Scene.h"
#include "Config.h"
#include "Renderer/Samplers.h"
#include <bigg.hpp>
#include <bx/string.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/matrix_relational.hpp>

ClusteredDeferredRenderer::ClusteredDeferredRenderer(const Scene* scene, const Config* config) :
    Renderer(scene, config),
    gBufferTextures { { BGFX_INVALID_HANDLE, "Diffuse + roughness" },
                      { BGFX_INVALID_HANDLE, "Normal" },
                      { BGFX_INVALID_HANDLE, "F0 + metallic" },
                      { BGFX_INVALID_HANDLE, "Emissive + occlusion" },
                      { BGFX_INVALID_HANDLE, "Depth" },
                      { BGFX_INVALID_HANDLE, nullptr } },
    gBufferTextureUnits { Samplers::DEFERRED_DIFFUSE_A,
                          Samplers::DEFERRED_NORMAL,
                          Samplers::DEFERRED_F0_METALLIC,
                          Samplers::DEFERRED_EMISSIVE_OCCLUSION,
                          Samplers::DEFERRED_DEPTH },
    gBufferSamplerNames { "s_texDiffuseA", "s_texNormal", "s_texF0Metallic", "s_texEmissiveOcclusion", "s_texDepth" }
{
    for(bgfx::UniformHandle& handle : gBufferSamplers)
    {
        handle = BGFX_INVALID_HANDLE;
    }
    buffers = gBufferTextures;
}

bool ClusteredDeferredRenderer::supported()
{
    const bgfx::Caps* caps = bgfx::getCaps();
    bool supported = Renderer::supported() &&
           // compute shader
           (caps->supported & BGFX_CAPS_COMPUTE) != 0 &&
           // 32-bit index buffers, used for light grid structure
           (caps->supported & BGFX_CAPS_INDEX32) != 0 &&
           // blitting depth texture after geometry pass
           (caps->supported & BGFX_CAPS_TEXTURE_BLIT) != 0 &&
           // multiple render targets
           // depth doesn't count as an attachment
           caps->limits.maxFBAttachments >= GBufferAttachment::Count - 1;
    if(!supported)
        return false;

    for(bgfx::TextureFormat::Enum format : gBufferAttachmentFormats)
    {
        if((caps->formats[format] & BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER) == 0)
            return false;
    }

    return true;
}

void ClusteredDeferredRenderer::onInitialize()
{
    // OpenGL backend: uniforms must be created before loading shaders
    clusters.initialize();

    for(size_t i = 0; i < BX_COUNTOF(gBufferSamplers); i++)
    {
        gBufferSamplers[i] = bgfx::createUniform(gBufferSamplerNames[i], bgfx::UniformType::Sampler);
    }

    char csName[128], vsName[128], fsName[128];

    bx::snprintf(csName, BX_COUNTOF(csName), "%s%s", shaderDir(), "cs_clustered_clusterbuilding.bin");
    clusterBuildingComputeProgram = bgfx::createProgram(bigg::loadShader(csName), true);

    bx::snprintf(csName, BX_COUNTOF(csName), "%s%s", shaderDir(), "cs_clustered_lightculling.bin");
    lightCullingComputeProgram = bgfx::createProgram(bigg::loadShader(csName), true);

    bx::snprintf(vsName, BX_COUNTOF(vsName), "%s%s", shaderDir(), "vs_deferred_geometry.bin");
    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_deferred_geometry.bin");
    geometryProgram = bigg::loadProgram(vsName, fsName);

    bx::snprintf(vsName, BX_COUNTOF(vsName), "%s%s", shaderDir(), "vs_deferred_fullscreen.bin");
    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_clustered_deferred_fullscreen.bin");
    fullscreenProgram = bigg::loadProgram(vsName, fsName);

    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_clustered_debug_vis_deferred.bin");
    debugVisFullscreenProgram = bigg::loadProgram(vsName, fsName);

    bx::snprintf(vsName, BX_COUNTOF(vsName), "%s%s", shaderDir(), "vs_clustered_forward.bin");
    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_clustered_forward.bin");
    transparencyProgram = bigg::loadProgram(vsName, fsName);

    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_clustered_debug_vis_forward.bin");
    debugVisTransparencyProgram = bigg::loadProgram(vsName, fsName);
}

void ClusteredDeferredRenderer::onReset()
{
    if(!bgfx::isValid(gBuffer))
    {
        gBuffer = createGBuffer();

        for(size_t i = 0; i < GBufferAttachment::Depth; i++)
        {
            gBufferTextures[i].handle = bgfx::getTexture(gBuffer, (uint8_t)i);
        }

        // we can't use the G-Buffer's depth texture in the light pass framebuffer
        // binding a texture for reading in the shader and attaching it to a framebuffer
        // at the same time is undefined behaviour in most APIs
        // https://www.khronos.org/opengl/wiki/Memory_Model#Framebuffer_objects
        // we use a different depth texture and just blit it between the geometry and light pass
        const uint64_t flags = BGFX_TEXTURE_BLIT_DST | gBufferSamplerFlags;
        bgfx::TextureFormat::Enum depthFormat = findDepthFormat(flags);
        lightDepthTexture = bgfx::createTexture2D(width, height, false, 1, depthFormat, flags);

        gBufferTextures[GBufferAttachment::Depth].handle = lightDepthTexture;
    }

    if(!bgfx::isValid(accumFrameBuffer))
    {
        const bgfx::TextureHandle textures[2] = { bgfx::getTexture(frameBuffer, 0),
                                                  bgfx::getTexture(gBuffer, GBufferAttachment::Depth) };
        accumFrameBuffer = bgfx::createFrameBuffer(BX_COUNTOF(textures), textures); // don't destroy textures
    }
}

void ClusteredDeferredRenderer::onRender(float dt)
{
    if(buffersNeedUpdate)
    {
        clusters.updateBuffers(config->maxLightsPerTileOrCluster,
                               width, height, config->treatClusterXYasPixelSize,
                               config->clustersX, config->clustersY, config->clustersZ);
        buffersNeedUpdate = false;
    }

    enum : bgfx::ViewId
    {
        vClusterBuilding = 0,
        vLightCulling,
        vGeometry,          // write G-Buffer
        vFullscreenLights,  // write ambient + emissive to output buffer
        vTransparent        // forward pass for transparency
    };

    const uint32_t BLACK = 0x000000FF;

    bgfx::setViewName(vClusterBuilding, "Cluster building pass (compute)");
    // set u_viewRect for screen2Eye to work correctly
    bgfx::setViewRect(vClusterBuilding, 0, 0, width, height);

    bgfx::setViewName(vLightCulling, "Clustered light culling pass (compute)");
    bgfx::setViewRect(vLightCulling, 0, 0, width, height);

    bgfx::setViewName(vGeometry, "Deferred clustered geometry pass");
    bgfx::setViewClear(vGeometry, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, BLACK, 1.0f);
    bgfx::setViewRect(vGeometry, 0, 0, width, height);
    bgfx::setViewFrameBuffer(vGeometry, gBuffer);
    bgfx::touch(vGeometry);

    bgfx::setViewName(vFullscreenLights, "Deferred clustered light pass (point lights + ambient + emissive)");
    bgfx::setViewClear(vFullscreenLights, BGFX_CLEAR_COLOR, clearColor);
    bgfx::setViewRect(vFullscreenLights, 0, 0, width, height);
    bgfx::setViewFrameBuffer(vFullscreenLights, accumFrameBuffer);
    bgfx::touch(vFullscreenLights);

    bgfx::setViewName(vTransparent, "Transparent forward pass");
    bgfx::setViewClear(vTransparent, BGFX_CLEAR_NONE);
    bgfx::setViewRect(vTransparent, 0, 0, width, height);
    bgfx::setViewFrameBuffer(vTransparent, accumFrameBuffer);
    bgfx::touch(vTransparent);

    if(!scene->loaded)
        return;

    clusters.setUniforms(scene, width, height);

    // cluster building needs u_invProj to transform screen coordinates to eye space
    setViewProjection(vClusterBuilding);
    // light culling needs u_view to transform lights to eye space
    setViewProjection(vLightCulling);
    setViewProjection(vGeometry);
    setViewProjection(vFullscreenLights);
    setViewProjection(vTransparent);

    // cluster building

    // only run this step if the camera parameters changed (aspect ratio, fov, near/far plane)
    // cluster bounds are saved in camera coordinates so they don't change with camera movement

    // ideally we'd compare the relative error here but a correct implementation would involve
    // a bunch of costly matrix operations: https://floating-point-gui.de/errors/comparison/
    // comparing the absolute error against a rather small epsilon here works as long as the values
    // in the projection matrix aren't getting too large
    const auto clusterCount = clusters.getClusterCount();
    const auto clustersX = std::get<0>(clusterCount);
    const auto clustersY = std::get<1>(clusterCount);
    const auto clustersZ = std::get<2>(clusterCount);

    clusters.bindBuffers(false /*lightingPass*/); // write access, all buffers

    bgfx::dispatch(vClusterBuilding,
                   clusterBuildingComputeProgram,
                   (uint32_t)std::ceil((float)clustersX / ClusterShader::CLUSTERS_X_THREADS),
                   (uint32_t)std::ceil((float)clustersY / ClusterShader::CLUSTERS_Y_THREADS),
                   (uint32_t)std::ceil((float)clustersZ / ClusterShader::CLUSTERS_Z_THREADS));

    // light culling

    lights.bindLights(scene);
    clusters.bindBuffers(false);

    bgfx::dispatch(vLightCulling,
                   lightCullingComputeProgram,
                   (uint32_t)std::ceil((float)clustersX / ClusterShader::CLUSTERS_X_THREADS),
                   (uint32_t)std::ceil((float)clustersY / ClusterShader::CLUSTERS_Y_THREADS),
                   (uint32_t)std::ceil((float)clustersZ / ClusterShader::CLUSTERS_Z_THREADS));

    // render geometry, write to G-Buffer

    const uint64_t state = BGFX_STATE_DEFAULT & ~BGFX_STATE_CULL_MASK;

    for(const Mesh& mesh : scene->meshes)
    {
        const Material& mat = scene->materials[mesh.material];
        // transparent materials are rendered in a separate forward pass (view vTransparent)
        if(!mat.blend)
        {
            glm::mat4 model = glm::identity<glm::mat4>();
            bgfx::setTransform(glm::value_ptr(model));
            setNormalMatrix(model);
            bgfx::setVertexBuffer(0, mesh.vertexBuffer);
            bgfx::setIndexBuffer(mesh.indexBuffer);
            uint64_t materialState = pbr.bindMaterial(mat);
            bgfx::setState(state | materialState);
            bgfx::submit(vGeometry, geometryProgram);
        }
    }

    // copy G-Buffer depth attachment to depth texture for sampling in the light pass
    // we can't attach it to the frame buffer and read it in the shader (unprojecting world position) at the same time
    // blit happens before any compute or draw calls
    bgfx::blit(vFullscreenLights, lightDepthTexture, 0, 0, bgfx::getTexture(gBuffer, GBufferAttachment::Depth));

    // bind these once for all following submits
    // excluding BGFX_DISCARD_TEXTURE_SAMPLERS from the discard flags passed to submit makes sure
    // they don't get unbound
    bindGBuffer();
    pbr.bindAlbedoLUT();
    lights.bindLights(scene);
    clusters.bindBuffers(true);

    // point lights + ambient light + emissive

    // full screen triangle, moved to far plane in the shader
    // only render if the geometry is in front so we leave the background untouched
    bool debugVis = variables["DEBUG_VIS"] == "true";
    bgfx::ProgramHandle programFullscreen = debugVis ? debugVisFullscreenProgram : fullscreenProgram;
    bgfx::setVertexBuffer(0, blitTriangleBuffer);
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_GREATER | BGFX_STATE_CULL_CW);
    bgfx::submit(vFullscreenLights, programFullscreen, 0, ~BGFX_DISCARD_BINDINGS);

    // transparent

    bgfx::ProgramHandle programTransparency = debugVis ? debugVisTransparencyProgram : transparencyProgram;
    for(const Mesh& mesh : scene->meshes)
    {
        const Material& mat = scene->materials[mesh.material];
        if(mat.blend)
        {
            glm::mat4 model = glm::identity<glm::mat4>();
            bgfx::setTransform(glm::value_ptr(model));
            setNormalMatrix(model);
            bgfx::setVertexBuffer(0, mesh.vertexBuffer);
            bgfx::setIndexBuffer(mesh.indexBuffer);
            uint64_t materialState = pbr.bindMaterial(mat);
            bgfx::setState(state | materialState);
            bgfx::submit(vTransparent, programTransparency, 0, ~BGFX_DISCARD_BINDINGS);
        }
    }

    bgfx::discard(BGFX_DISCARD_ALL);
}

void ClusteredDeferredRenderer::onOptionsChanged()
{
    buffersNeedUpdate = true;
}

void ClusteredDeferredRenderer::onShutdown()
{
    clusters.shutdown();

    bgfx::destroy(clusterBuildingComputeProgram);
    bgfx::destroy(lightCullingComputeProgram);
    bgfx::destroy(geometryProgram);
    bgfx::destroy(fullscreenProgram);
    bgfx::destroy(transparencyProgram);
    bgfx::destroy(debugVisFullscreenProgram);
    bgfx::destroy(debugVisTransparencyProgram);

    for(bgfx::UniformHandle& handle : gBufferSamplers)
    {
        bgfx::destroy(handle);
        handle = BGFX_INVALID_HANDLE;
    }
    if(bgfx::isValid(lightDepthTexture))
        bgfx::destroy(lightDepthTexture);
    if(bgfx::isValid(gBuffer))
        bgfx::destroy(gBuffer);
    if(bgfx::isValid(accumFrameBuffer))
        bgfx::destroy(accumFrameBuffer);

    lightDepthTexture = BGFX_INVALID_HANDLE;
    gBuffer = BGFX_INVALID_HANDLE;
    accumFrameBuffer = BGFX_INVALID_HANDLE;

    clusterBuildingComputeProgram = lightCullingComputeProgram = geometryProgram =
        fullscreenProgram = transparencyProgram = debugVisFullscreenProgram = debugVisTransparencyProgram = BGFX_INVALID_HANDLE;
}

bgfx::FrameBufferHandle ClusteredDeferredRenderer::createGBuffer()
{
    bgfx::TextureHandle textures[GBufferAttachment::Count];

    const uint64_t flags = BGFX_TEXTURE_RT | gBufferSamplerFlags;

    for(size_t i = 0; i < GBufferAttachment::Depth; i++)
    {
        assert(bgfx::isTextureValid(0, false, 1, gBufferAttachmentFormats[i], flags));
        textures[i] = bgfx::createTexture2D(width, height, false, 1, gBufferAttachmentFormats[i], flags);
    }

    bgfx::TextureFormat::Enum depthFormat = findDepthFormat(flags);
    assert(depthFormat != bgfx::TextureFormat::Count);
    textures[Depth] = bgfx::createTexture2D(width, height, false, 1, depthFormat, flags);

    bgfx::FrameBufferHandle gb = bgfx::createFrameBuffer((uint8_t)GBufferAttachment::Count, textures, true);

    if(!bgfx::isValid(gb))
        Log->error("Failed to create G-Buffer");
    else
        bgfx::setName(gb, "G-Buffer");

    return gb;
}

void ClusteredDeferredRenderer::bindGBuffer()
{
    for(size_t i = 0; i < GBufferAttachment::Count; i++)
    {
        bgfx::setTexture(gBufferTextureUnits[i], gBufferSamplers[i], gBufferTextures[i].handle);
    }
}