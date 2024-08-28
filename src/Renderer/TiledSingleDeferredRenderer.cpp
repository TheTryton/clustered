#include "TiledSingleDeferredRenderer.h"

#include "Scene/Scene.h"
#include "Config.h"
#include "Renderer/Samplers.h"
#include <bigg.hpp>
#include <bx/string.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/matrix_relational.hpp>

TiledSingleDeferredRenderer::TiledSingleDeferredRenderer(const Scene* scene, const Config* config) :
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

bool TiledSingleDeferredRenderer::supported()
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

void TiledSingleDeferredRenderer::onInitialize()
{
    // OpenGL backend: uniforms must be created before loading shaders
    tiles.initialize();

    for(size_t i = 0; i < BX_COUNTOF(gBufferSamplers); i++)
    {
        gBufferSamplers[i] = bgfx::createUniform(gBufferSamplerNames[i], bgfx::UniformType::Sampler);
    }

    char csName[128], vsName[128], fsName[128];

    bx::snprintf(csName, BX_COUNTOF(csName), "%s%s", shaderDir(), "cs_tiled_tilebuilding.bin");
    tileBuildingComputeProgram = bgfx::createProgram(bigg::loadShader(csName), true);

    bx::snprintf(csName, BX_COUNTOF(csName), "%s%s", shaderDir(), "cs_tiled_lightculling_single_thread_per_tile.bin");
    lightCullingComputeProgram = bgfx::createProgram(bigg::loadShader(csName), true);

    bx::snprintf(vsName, BX_COUNTOF(vsName), "%s%s", shaderDir(), "vs_deferred_geometry.bin");
    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_deferred_geometry.bin");
    geometryProgram = bigg::loadProgram(vsName, fsName);

    bx::snprintf(vsName, BX_COUNTOF(vsName), "%s%s", shaderDir(), "vs_deferred_fullscreen.bin");
    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_tiled_deferred_fullscreen.bin");
    fullscreenProgram = bigg::loadProgram(vsName, fsName);

    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_tiled_debug_vis_deferred.bin");
    debugVisFullscreenProgram = bigg::loadProgram(vsName, fsName);

    bx::snprintf(vsName, BX_COUNTOF(vsName), "%s%s", shaderDir(), "vs_tiled_forward.bin");
    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_tiled_forward.bin");
    transparencyProgram = bigg::loadProgram(vsName, fsName);

    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_tiled_debug_vis_forward.bin");
    debugVisTransparencyProgram = bigg::loadProgram(vsName, fsName);
}

void TiledSingleDeferredRenderer::onReset()
{
    buffersNeedUpdate = true;

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

void TiledSingleDeferredRenderer::onRender(float dt)
{
    if(buffersNeedUpdate)
    {
        tiles.updateBuffers(width, height, config->maxLightsPerTileOrCluster, config->tilePixelSizeX, config->tilePixelSizeY);
        buffersNeedUpdate = false;
    }

    enum : bgfx::ViewId
    {
        vTileBuilding = 0,
        vLightCulling,
        vGeometry,          // write G-Buffer
        vFullscreenLights,  // write ambient + emissive to output buffer
        vTransparent        // forward pass for transparency
    };

    const uint32_t BLACK = 0x000000FF;

    bgfx::setViewName(vTileBuilding, "Tile building pass (compute)");
    // set u_viewRect for screen2Eye to work correctly
    bgfx::setViewRect(vTileBuilding, 0, 0, width, height);

    bgfx::setViewName(vLightCulling, "Tile light culling pass (compute)");
    bgfx::setViewRect(vLightCulling, 0, 0, width, height);

    bgfx::setViewName(vGeometry, "Deferred tiled geometry pass");
    bgfx::setViewClear(vGeometry, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, BLACK, 1.0f);
    bgfx::setViewRect(vGeometry, 0, 0, width, height);
    bgfx::setViewFrameBuffer(vGeometry, gBuffer);
    bgfx::touch(vGeometry);

    bgfx::setViewName(vFullscreenLights, "Deferred tiled light pass (point lights + ambient + emissive)");
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

    tiles.setUniforms(scene, width, height);

    // tile building needs u_invProj to transform screen coordinates to eye space
    setViewProjection(vTileBuilding);
    // light culling needs u_view to transform lights to eye space
    setViewProjection(vLightCulling);
    setViewProjection(vGeometry);
    setViewProjection(vFullscreenLights);
    setViewProjection(vTransparent);

    // tile building

    // only run this step if the camera parameters changed (aspect ratio, fov, near/far plane)
    // tile bounds are saved in camera coordinates, so they don't change with camera movement

    // ideally we'd compare the relative error here but a correct implementation would involve
    // a bunch of costly matrix operations: https://floating-point-gui.de/errors/comparison/
    // comparing the absolute error against a rather small epsilon here works as long as the values
    // in the projection matrix aren't getting too large
    const auto tilePixelSizes = tiles.getTilePixelSize();
    const auto tilePixelSizeX = std::get<0>(tilePixelSizes);
    const auto tilePixelSizeY = std::get<1>(tilePixelSizes);

    tiles.bindBuffers(false /*lightingPass*/); // write access, all buffers

    bgfx::dispatch(vTileBuilding,
                   tileBuildingComputeProgram,
                   (uint32_t)std::ceil(std::ceil((float)width / tilePixelSizeX) / TileShader::TILES_X_THREADS),
                   (uint32_t)std::ceil(std::ceil((float)height / tilePixelSizeY) / TileShader::TILES_Y_THREADS),
                   1);

    // light culling

    lights.bindLights(scene);
    tiles.bindBuffers(false);

    bgfx::dispatch(vLightCulling,
                   lightCullingComputeProgram,
                   (uint32_t)std::ceil(std::ceil((float)width / tilePixelSizeX) / TileShader::TILES_X_THREADS),
                   (uint32_t)std::ceil(std::ceil((float)height / tilePixelSizeY) / TileShader::TILES_Y_THREADS),
                   1);

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
    tiles.bindBuffers(true);

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

void TiledSingleDeferredRenderer::onOptionsChanged()
{
    buffersNeedUpdate = true;
}

void TiledSingleDeferredRenderer::onShutdown()
{
    tiles.shutdown();

    bgfx::destroy(tileBuildingComputeProgram);
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

    tileBuildingComputeProgram = lightCullingComputeProgram = geometryProgram =
        fullscreenProgram = transparencyProgram = debugVisFullscreenProgram = debugVisTransparencyProgram = BGFX_INVALID_HANDLE;
}

bgfx::FrameBufferHandle TiledSingleDeferredRenderer::createGBuffer()
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

void TiledSingleDeferredRenderer::bindGBuffer()
{
    for(size_t i = 0; i < GBufferAttachment::Count; i++)
    {
        bgfx::setTexture(gBufferTextureUnits[i], gBufferSamplers[i], gBufferTextures[i].handle);
    }
}