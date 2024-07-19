#include "DeferredRenderer.h"

#include "Scene/Scene.h"
#include "Renderer/Samplers.h"
#include <bigg.hpp>
#include <bx/string.h>
#include <glm/matrix.hpp>
#include <glm/gtc/type_ptr.hpp>

constexpr bgfx::TextureFormat::Enum DeferredRenderer::gBufferAttachmentFormats[DeferredRenderer::GBufferAttachment::Count - 1];

DeferredRenderer::DeferredRenderer(const Scene* scene) :
    Renderer(scene),
    gBufferTextures { { BGFX_INVALID_HANDLE, "Diffuse + roughness" },
                      { BGFX_INVALID_HANDLE, "Position" },
                      { BGFX_INVALID_HANDLE, "Normal" },
                      { BGFX_INVALID_HANDLE, "F0 + metallic" },
                      { BGFX_INVALID_HANDLE, "Emissive + occlusion" },
                      { BGFX_INVALID_HANDLE, "Depth" },
                      { BGFX_INVALID_HANDLE, nullptr } },
    gBufferTextureUnits { Samplers::DEFERRED_DIFFUSE_A,
                          Samplers::DEFERRED_POSITION,
                          Samplers::DEFERRED_NORMAL,
                          Samplers::DEFERRED_F0_METALLIC,
                          Samplers::DEFERRED_EMISSIVE_OCCLUSION,
                          Samplers::DEFERRED_DEPTH },
    gBufferSamplerNames { "s_texDiffuseA", "s_texPosition","s_texNormal", "s_texF0Metallic", "s_texEmissiveOcclusion", "s_texDepth" },
    gBuffer(BGFX_INVALID_HANDLE),
    lightDepthTexture(BGFX_INVALID_HANDLE),
    accumFrameBuffer(BGFX_INVALID_HANDLE),
    cameraPosition(BGFX_INVALID_HANDLE),
    geometryProgram(BGFX_INVALID_HANDLE),
    fullscreenProgram(BGFX_INVALID_HANDLE)
{
    for(bgfx::UniformHandle& handle : gBufferSamplers)
    {
        handle = BGFX_INVALID_HANDLE;
    }
    buffers = gBufferTextures;
}

bool DeferredRenderer::supported()
{
    const bgfx::Caps* caps = bgfx::getCaps();
    bool supported = Renderer::supported() &&
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

void DeferredRenderer::onInitialize()
{
    cameraPosition = bgfx::createUniform("u_cameraPosition", bgfx::UniformType::Vec4);

    for(size_t i = 0; i < BX_COUNTOF(gBufferSamplers); i++)
    {
        gBufferSamplers[i] = bgfx::createUniform(gBufferSamplerNames[i], bgfx::UniformType::Sampler);
    }

    char vsName[128], fsName[128];

    bx::snprintf(vsName, BX_COUNTOF(vsName), "%s%s", shaderDir(), "vs_deferred_geometry.bin");
    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_deferred_geometry.bin");
    geometryProgram = bigg::loadProgram(vsName, fsName);

    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "vs_deferred_fullscreen.bin");
    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_deferred_fullscreen.bin");
    fullscreenProgram = bigg::loadProgram(vsName, fsName);

    bx::snprintf(vsName, BX_COUNTOF(vsName), "%s%s", shaderDir(), "vs_forward.bin");
    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_forward.bin");
    transparencyProgram = bigg::loadProgram(vsName, fsName);
}

void DeferredRenderer::onReset()
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
        const uint64_t flags = BGFX_TEXTURE_BLIT_DST | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT |
                               BGFX_SAMPLER_MIP_POINT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
        bgfx::TextureFormat::Enum depthFormat = findDepthFormat(flags);
        lightDepthTexture = bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, false, 1, depthFormat, flags);

        gBufferTextures[GBufferAttachment::Depth].handle = lightDepthTexture;
    }

    if(!bgfx::isValid(accumFrameBuffer))
    {
        const bgfx::TextureHandle textures[2] = { bgfx::getTexture(frameBuffer, 0),
                                                  bgfx::getTexture(gBuffer, GBufferAttachment::Depth) };
        accumFrameBuffer = bgfx::createFrameBuffer(BX_COUNTOF(textures), textures); // don't destroy textures
    }
}

void DeferredRenderer::onRender(float dt)
{
    enum : bgfx::ViewId
    {
        vGeometry = 0,    // write G-Buffer
        vFullscreen,          // render lights + ambient + emissive to output buffer
        vTransparent      // forward pass for transparency
    };

    const uint32_t BLACK = 0x000000FF;

    bgfx::setViewName(vGeometry, "Deferred geometry pass");
    bgfx::setViewClear(vGeometry, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, BLACK, 1.0f);
    bgfx::setViewRect(vGeometry, 0, 0, width, height);
    bgfx::setViewFrameBuffer(vGeometry, gBuffer);
    bgfx::touch(vGeometry);

    bgfx::setViewName(vFullscreen, "Deferred fullscreen pass (point lights + ambient + emissive)");
    bgfx::setViewClear(vFullscreen, BGFX_CLEAR_COLOR);
    bgfx::setViewRect(vFullscreen, 0, 0, width, height);
    bgfx::setViewFrameBuffer(vFullscreen, BGFX_INVALID_HANDLE);
    bgfx::touch(vFullscreen);

    bgfx::setViewName(vTransparent, "Transparent forward pass");
    bgfx::setViewClear(vTransparent, BGFX_CLEAR_NONE);
    bgfx::setViewRect(vTransparent, 0, 0, width, height);
    bgfx::setViewFrameBuffer(vTransparent, accumFrameBuffer);
    bgfx::touch(vTransparent);

    if(!scene->loaded)
        return;

    setViewProjection(vGeometry);
    setViewProjection(vTransparent);

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
            bgfx::setInsta
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
    bgfx::blit(vFullscreen, lightDepthTexture, 0, 0, bgfx::getTexture(gBuffer, GBufferAttachment::Depth));

    // bind these once for all following submits
    // excluding BGFX_DISCARD_TEXTURE_SAMPLERS from the discard flags passed to submit makes sure
    // they don't get unbound
    bindGBuffer();
    lights.bindLights(scene);

    // point lights + ambient + emissive

    // full screen triangle
    // could also attach the accumulation buffer as a render target and write out during the geometry pass
    // this is a bit cleaner

    // move triangle to far plane (z = 1)
    // only render if the geometry is in front so we leave the background untouched
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_CULL_CW);
    bgfx::setVertexBuffer(0, blitTriangleBuffer);
    bgfx::submit(vFullscreen, fullscreenProgram);

    /*bgfx::setVertexBuffer(0, blitTriangleBuffer);
    glm::vec4 cameraPositionV = glm::vec4(scene->camera.position(), 0.0f);
    bgfx::setUniform(cameraPosition, glm::value_ptr(cameraPositionV));
    bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_CULL_CW);
    bgfx::submit(vFullscreen, fullscreenProgram, 0, ~BGFX_DISCARD_TEXTURE_SAMPLERS);*/
    // transparent

    /*for(const Mesh& mesh : scene->meshes)
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
            bgfx::submit(vTransparent, transparencyProgram, 0, ~BGFX_DISCARD_TEXTURE_SAMPLERS);
        }
    }*/

    bgfx::discard(BGFX_DISCARD_ALL);
}

void DeferredRenderer::onShutdown()
{
    bgfx::destroy(geometryProgram);
    bgfx::destroy(fullscreenProgram);
    bgfx::destroy(transparencyProgram);
    for(bgfx::UniformHandle& handle : gBufferSamplers)
    {
        bgfx::destroy(handle);
        handle = BGFX_INVALID_HANDLE;
    }
    bgfx::destroy(cameraPosition);
    if(bgfx::isValid(lightDepthTexture))
        bgfx::destroy(lightDepthTexture);
    if(bgfx::isValid(gBuffer))
        bgfx::destroy(gBuffer);
    if(bgfx::isValid(accumFrameBuffer))
        bgfx::destroy(accumFrameBuffer);

    geometryProgram = fullscreenProgram = transparencyProgram = BGFX_INVALID_HANDLE;
    cameraPosition = BGFX_INVALID_HANDLE;
    lightDepthTexture = BGFX_INVALID_HANDLE;
    gBuffer = BGFX_INVALID_HANDLE;
    accumFrameBuffer = BGFX_INVALID_HANDLE;
}

bgfx::FrameBufferHandle DeferredRenderer::createGBuffer()
{
    bgfx::TextureHandle textures[GBufferAttachment::Count];

    const uint64_t samplerFlags = BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT |
                                  BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;

    for(size_t i = 0; i < GBufferAttachment::Depth; i++)
    {
        assert(bgfx::isTextureValid(0, false, 1, gBufferAttachmentFormats[i], BGFX_TEXTURE_RT | samplerFlags));
        textures[i] = bgfx::createTexture2D(
            bgfx::BackbufferRatio::Equal, false, 1, gBufferAttachmentFormats[i], BGFX_TEXTURE_RT | samplerFlags);
    }

    bgfx::TextureFormat::Enum depthFormat = findDepthFormat(BGFX_TEXTURE_RT | samplerFlags);
    assert(depthFormat != bgfx::TextureFormat::Count);
    textures[Depth] =
        bgfx::createTexture2D(bgfx::BackbufferRatio::Equal, false, 1, depthFormat, BGFX_TEXTURE_RT | samplerFlags);

    bgfx::FrameBufferHandle gb = bgfx::createFrameBuffer((uint8_t)GBufferAttachment::Count, textures, true);

    if(!bgfx::isValid(gb))
        Log->error("Failed to create G-Buffer");
    else
        bgfx::setName(gb, "G-Buffer");

    return gb;
}

void DeferredRenderer::bindGBuffer()
{
    for(size_t i = 0; i < GBufferAttachment::Count; i++)
    {
        bgfx::setTexture(gBufferTextureUnits[i], gBufferSamplers[i], gBufferTextures[i].handle);
    }
}
