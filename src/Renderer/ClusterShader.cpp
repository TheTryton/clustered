#include "ClusterShader.h"

#include "Scene/Scene.h"
#include "Renderer/Samplers.h"
#include <glm/common.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

bgfx::VertexLayout ClusterShader::ClusterVertex::layout;

ClusterShader::ClusterShader()
{
    static_assert(CLUSTERS_Z % CLUSTERS_Z_THREADS == 0,
                  "number of cluster depth slices must be divisible by thread count z-dimension");
}

void ClusterShader::initialize()
{
    ClusterVertex::init();

    clusterSizesVecUniform = bgfx::createUniform("u_clusterSizesVec", bgfx::UniformType::Vec4);
    zNearFarVecUniform = bgfx::createUniform("u_zNearFarVec", bgfx::UniformType::Vec4);
}

void ClusterShader::updateBuffers(uint32_t maxLightsPerCluster)
{
    if(currentMaxLightsPerCluster == maxLightsPerCluster)
    {
        return;
    }

    currentMaxLightsPerCluster = maxLightsPerCluster;

    if(isValid(clustersBuffer))
    {
        bgfx::destroy(clustersBuffer);
    }

    if(isValid(lightIndicesBuffer))
    {
        bgfx::destroy(lightIndicesBuffer);
    }

    if(isValid(lightGridBuffer))
    {
        bgfx::destroy(lightGridBuffer);
    }

    clustersBuffer =
        bgfx::createDynamicVertexBuffer(CLUSTER_COUNT, ClusterVertex::layout, BGFX_BUFFER_COMPUTE_READ_WRITE);
    lightIndicesBuffer = bgfx::createDynamicIndexBuffer(CLUSTER_COUNT * currentMaxLightsPerCluster,
                                                        BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_INDEX32);
    lightGridBuffer = bgfx::createDynamicIndexBuffer(CLUSTER_COUNT,
                                                     BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_INDEX32);
}

void ClusterShader::shutdown()
{
    bgfx::destroy(clusterSizesVecUniform);
    bgfx::destroy(zNearFarVecUniform);

    bgfx::destroy(clustersBuffer);
    bgfx::destroy(lightIndicesBuffer);
    bgfx::destroy(lightGridBuffer);

    clusterSizesVecUniform = zNearFarVecUniform = BGFX_INVALID_HANDLE;
    clustersBuffer = BGFX_INVALID_HANDLE;
    lightIndicesBuffer = lightGridBuffer = BGFX_INVALID_HANDLE;
}

void ClusterShader::setUniforms(const Scene* scene, uint16_t screenWidth, uint16_t screenHeight) const
{
    assert(scene != nullptr);

    float clusterSizesVec[4] = { std::ceil((float)screenWidth / CLUSTERS_X),
                                 std::ceil((float)screenHeight / CLUSTERS_Y),
                                    (float)currentMaxLightsPerCluster };

    bgfx::setUniform(clusterSizesVecUniform, clusterSizesVec);
    float zNearFarVec[4] = { scene->camera.zNear, scene->camera.zFar };
    bgfx::setUniform(zNearFarVecUniform, zNearFarVec);
}

void ClusterShader::bindBuffers(bool lightingPass) const
{
    // binding ReadWrite in the fragment shader doesn't work with D3D11/12
    bgfx::Access::Enum access = lightingPass ? bgfx::Access::Read : bgfx::Access::ReadWrite;
    if(!lightingPass)
    {
        bgfx::setBuffer(Samplers::CLUSTERS_CLUSTERS, clustersBuffer, access);
    }
    bgfx::setBuffer(Samplers::CLUSTERS_LIGHTINDICES, lightIndicesBuffer, access);
    bgfx::setBuffer(Samplers::CLUSTERS_LIGHTGRID, lightGridBuffer, access);
}
