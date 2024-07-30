#include "ClusteredForwardRenderer.h"

#include "Scene/Scene.h"
#include "Config.h"
#include <bigg.hpp>
#include <bx/string.h>
#include <glm/gtc/type_ptr.hpp>
#include <glm/ext/matrix_relational.hpp>

ClusteredForwardRenderer::ClusteredForwardRenderer(const Scene* scene, const Config* config) : Renderer(scene, config) { }

bool ClusteredForwardRenderer::supported()
{
    const bgfx::Caps* caps = bgfx::getCaps();
    return Renderer::supported() &&
           // compute shader
           (caps->supported & BGFX_CAPS_COMPUTE) != 0 &&
           // 32-bit index buffers, used for light grid structure
           (caps->supported & BGFX_CAPS_INDEX32) != 0;
}

void ClusteredForwardRenderer::onInitialize()
{
    // OpenGL backend: uniforms must be created before loading shaders
    clusters.initialize();

    char csName[128], vsName[128], fsName[128];

    bx::snprintf(csName, BX_COUNTOF(csName), "%s%s", shaderDir(), "cs_clustered_clusterbuilding.bin");
    clusterBuildingComputeProgram = bgfx::createProgram(bigg::loadShader(csName), true);

    bx::snprintf(csName, BX_COUNTOF(csName), "%s%s", shaderDir(), "cs_clustered_lightculling.bin");
    lightCullingComputeProgram = bgfx::createProgram(bigg::loadShader(csName), true);

    bx::snprintf(vsName, BX_COUNTOF(vsName), "%s%s", shaderDir(), "vs_clustered_forward.bin");
    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_clustered_forward.bin");
    lightingProgram = bigg::loadProgram(vsName, fsName);

    bx::snprintf(fsName, BX_COUNTOF(fsName), "%s%s", shaderDir(), "fs_clustered_debug_vis_forward.bin");
    debugVisProgram = bigg::loadProgram(vsName, fsName);
}

void ClusteredForwardRenderer::onRender(float dt)
{
    if(buffersNeedUpdate)
    {
        clusters.updateBuffers(config->maxLightsPerTileOrCluster);
        buffersNeedUpdate = false;
    }

    enum : bgfx::ViewId
    {
        vClusterBuilding = 0,
        vLightCulling,
        vLighting
    };

    bgfx::setViewName(vClusterBuilding, "Cluster building pass (compute)");
    // set u_viewRect for screen2Eye to work correctly
    bgfx::setViewRect(vClusterBuilding, 0, 0, width, height);

    bgfx::setViewName(vLightCulling, "Clustered light culling pass (compute)");
    bgfx::setViewRect(vLightCulling, 0, 0, width, height);

    bgfx::setViewName(vLighting, "Clustered lighting pass");
    bgfx::setViewClear(vLighting, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, clearColor, 1.0f, 0);
    bgfx::setViewRect(vLighting, 0, 0, width, height);
    bgfx::setViewFrameBuffer(vLighting, frameBuffer);
    bgfx::touch(vLighting);

    if(!scene->loaded)
        return;

    clusters.setUniforms(scene, width, height);

    // cluster building needs u_invProj to transform screen coordinates to eye space
    setViewProjection(vClusterBuilding);
    // light culling needs u_view to transform lights to eye space
    setViewProjection(vLightCulling);
    setViewProjection(vLighting);

    // cluster building

    // only run this step if the camera parameters changed (aspect ratio, fov, near/far plane)
    // cluster bounds are saved in camera coordinates so they don't change with camera movement

    // ideally we'd compare the relative error here but a correct implementation would involve
    // a bunch of costly matrix operations: https://floating-point-gui.de/errors/comparison/
    // comparing the absolute error against a rather small epsilon here works as long as the values
    // in the projection matrix aren't getting too large
    bool buildClusters = glm::any(glm::notEqual(projMat, oldProjMat, 0.00001f));
    if(buildClusters)
    {
        oldProjMat = projMat;

        clusters.bindBuffers(false /*lightingPass*/); // write access, all buffers

        bgfx::dispatch(vClusterBuilding,
                       clusterBuildingComputeProgram,
                       ClusterShader::CLUSTERS_X / ClusterShader::CLUSTERS_X_THREADS,
                       ClusterShader::CLUSTERS_Y / ClusterShader::CLUSTERS_Y_THREADS,
                       ClusterShader::CLUSTERS_Z / ClusterShader::CLUSTERS_Z_THREADS);
    }

    // light culling

    lights.bindLights(scene);
    clusters.bindBuffers(false);

    bgfx::dispatch(vLightCulling,
                   lightCullingComputeProgram,
                   ClusterShader::CLUSTERS_X / ClusterShader::CLUSTERS_X_THREADS,
                   ClusterShader::CLUSTERS_Y / ClusterShader::CLUSTERS_Y_THREADS,
                   ClusterShader::CLUSTERS_Z / ClusterShader::CLUSTERS_Z_THREADS);
    // lighting

    bool debugVis = variables["DEBUG_VIS"] == "true";
    bgfx::ProgramHandle program = debugVis ? debugVisProgram : lightingProgram;

    uint64_t state = BGFX_STATE_DEFAULT & ~BGFX_STATE_CULL_MASK;

    pbr.bindAlbedoLUT();
    lights.bindLights(scene);
    clusters.bindBuffers(true /*lightingPass*/); // read access, only light grid and indices

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

void ClusteredForwardRenderer::onOptionsChanged()
{
    buffersNeedUpdate = true;
}

void ClusteredForwardRenderer::onShutdown()
{
    clusters.shutdown();

    bgfx::destroy(clusterBuildingComputeProgram);
    bgfx::destroy(lightCullingComputeProgram);
    bgfx::destroy(lightingProgram);
    bgfx::destroy(debugVisProgram);

    clusterBuildingComputeProgram = lightCullingComputeProgram = lightingProgram = debugVisProgram = BGFX_INVALID_HANDLE;
}
