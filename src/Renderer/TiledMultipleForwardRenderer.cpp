#include "TiledMultipleForwardRenderer.h"

#include "Scene/Scene.h"
#include "Config.h"
#include <bigg.hpp>
#include <bx/string.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/matrix_relational.hpp>

TiledMultipleForwardRenderer::TiledMultipleForwardRenderer(const Scene* scene, const Config* config) : Renderer(scene, config) { }

bool TiledMultipleForwardRenderer::supported()
{
    const bgfx::Caps* caps = bgfx::getCaps();
    return Renderer::supported() &&
           // compute shader
           (caps->supported & BGFX_CAPS_COMPUTE) != 0 &&
           // 32-bit index buffers, used for light grid structure
           (caps->supported & BGFX_CAPS_INDEX32) != 0;
}

void TiledMultipleForwardRenderer::onInitialize()
{
    // OpenGL backend: uniforms must be created before loading shaders
    tiles.initialize();

    char csName[128], vsName[128], fsName[128];

    bx::snprintf(csName, BX_COUNTOF(csName), "%s%s", shaderDir(), "cs_tiled_tilebuilding.bin");
    tileBuildingComputeProgram = bgfx::createProgram(bigg::loadShader(csName), true);

    bx::snprintf(csName, BX_COUNTOF(csName), "%s%s", shaderDir(), "cs_tiled_lightculling_multiple_thread_per_tile.bin");
    lightCullingComputeProgram = bgfx::createProgram(bigg::loadShader(csName), true);

    bx::snprintf(vsName, BX_COUNTOF(vsName), "%s%s", shaderDir(), "vs_tiled_forward.bin");
    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_tiled_forward.bin");
    lightingProgram = bigg::loadProgram(vsName, fsName);

    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_tiled_debug_vis_forward.bin");
    debugVisProgram = bigg::loadProgram(vsName, fsName);
}

void TiledMultipleForwardRenderer::onRender(float dt)
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
        vLighting
    };

    bgfx::setViewName(vTileBuilding, "Tile building pass (compute)");
    // set u_viewRect for screen2Eye to work correctly
    bgfx::setViewRect(vTileBuilding, 0, 0, width, height);

    bgfx::setViewName(vLightCulling, "Tiled light culling pass (compute)");
    bgfx::setViewRect(vLightCulling, 0, 0, width, height);

    bgfx::setViewName(vLighting, "Tiled lighting pass");
    bgfx::setViewClear(vLighting, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clearColor, 1.0f, 0);
    bgfx::setViewRect(vLighting, 0, 0, width, height);
    bgfx::setViewFrameBuffer(vLighting, frameBuffer);
    bgfx::touch(vLighting);

    if(!scene->loaded)
        return;

    tiles.setUniforms(scene, width, height);

    // tile building needs u_invProj to transform screen coordinates to eye space
    setViewProjection(vTileBuilding);
    // light culling needs u_view to transform lights to eye space
    setViewProjection(vLightCulling);
    setViewProjection(vLighting);

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
                   (uint32_t)std::ceil(std::ceil((float)width / tilePixelSizeX)),
                   (uint32_t)std::ceil(std::ceil((float)height / tilePixelSizeY)),
                   1);

    // light culling

    lights.bindLights(scene);
    tiles.bindBuffers(false);

    bgfx::dispatch(vLightCulling,
                   lightCullingComputeProgram,
                   (uint32_t)std::ceil(std::ceil((float)width / tilePixelSizeX)),
                   (uint32_t)std::ceil(std::ceil((float)height / tilePixelSizeY)),
                   1);
    // lighting

    bool debugVis = variables["DEBUG_VIS"] == "true";
    bgfx::ProgramHandle program = debugVis ? debugVisProgram : lightingProgram;

    uint64_t state = BGFX_STATE_DEFAULT & ~BGFX_STATE_CULL_MASK;

    pbr.bindAlbedoLUT();
    lights.bindLights(scene);
    tiles.bindBuffers(true /*lightingPass*/); // read access, only light grid and indices

    for(const Mesh& mesh : scene->meshes)
    {
        glm::mat4 model = glm::identity<glm::mat4>();
        bgfx::setTransform(glm::value_ptr(model));
        setNormalMatrix(model);
        bgfx::setVertexBuffer(0, mesh.vertexBuffer);
        bgfx::setIndexBuffer(mesh.indexBuffer);
        const Material& mat = scene->materials[mesh.material];
        uint64_t materialState = pbr.bindMaterial(mat);
        bgfx::setState(state | materialState);
        // preserve buffer bindings between submit calls
        bgfx::submit(vLighting, program, 0, ~BGFX_DISCARD_BINDINGS);
    }

    bgfx::discard(BGFX_DISCARD_ALL);
}

void TiledMultipleForwardRenderer::onReset()
{
    buffersNeedUpdate = true;
}

void TiledMultipleForwardRenderer::onOptionsChanged()
{
    buffersNeedUpdate = true;
}

void TiledMultipleForwardRenderer::onShutdown()
{
    tiles.shutdown();

    bgfx::destroy(tileBuildingComputeProgram);
    bgfx::destroy(lightCullingComputeProgram);
    bgfx::destroy(lightingProgram);
    bgfx::destroy(debugVisProgram);

    tileBuildingComputeProgram = lightCullingComputeProgram = lightingProgram = debugVisProgram = BGFX_INVALID_HANDLE;
}
