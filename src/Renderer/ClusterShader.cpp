#include "ClusterShader.h"

#include "Scene/Scene.h"
#include "Renderer/Samplers.h"
#include <glm/common.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

bgfx::VertexLayout ClusterShader::ClusterVertex::layout;

ClusterShader::ClusterShader() = default;
/*{
    static_assert(CLUSTERS_Z % CLUSTERS_Z_THREADS == 0,
                  "number of cluster depth slices must be divisible by thread count z-dimension");
}*/

void ClusterShader::initialize()
{
    ClusterVertex::init();

    clusterCountVecUniform = bgfx::createUniform("u_clusterCountVec", bgfx::UniformType::Vec4);
    clusterSizeVecUniform = bgfx::createUniform("u_clusterSizeVec", bgfx::UniformType::Vec4);
    zNearFarVecUniform = bgfx::createUniform("u_zNearFarVec", bgfx::UniformType::Vec4);
}

void ClusterShader::updateBuffers(uint32_t maxLightsPerCluster, uint16_t screenWidth, uint16_t screenHeight, bool clustersXYAsPixelSizes, uint32_t clustersX, uint32_t clustersY, uint32_t clustersZ)
{
    uint32_t actualClustersX;
    uint32_t actualClustersY;

    if(clustersXYAsPixelSizes)
    {
        actualClustersX = (uint32_t)std::ceil((float)screenWidth / clustersX);
        actualClustersY = (uint32_t)std::ceil((float)screenHeight / clustersY);
    }
    else
    {
        actualClustersX = clustersX;
        actualClustersY = clustersY;
    }

    if(currentClustersXYAsPixelSizes == clustersXYAsPixelSizes && currentMaxLightsPerCluster == maxLightsPerCluster &&
       currentClustersX == actualClustersX && currentClustersY == actualClustersY && currentClustersZ == clustersZ)
    {
        return;
    }

    currentClustersXYAsPixelSizes = clustersXYAsPixelSizes;
    currentMaxLightsPerCluster = maxLightsPerCluster;
    currentClustersX = actualClustersX;
    currentClustersY = actualClustersY;
    currentClustersZ = clustersZ;

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

    const auto currentClusterCount = currentClustersX * currentClustersY * currentClustersZ;
    if((size_t)currentClusterCount * currentMaxLightsPerCluster > 4ull * 1024 * 1024 * 1024)
        terminate();

    clustersBuffer =
        bgfx::createDynamicVertexBuffer(currentClusterCount, ClusterVertex::layout, BGFX_BUFFER_COMPUTE_READ_WRITE);
    lightIndicesBuffer = bgfx::createDynamicIndexBuffer(currentClusterCount * currentMaxLightsPerCluster,
                                                        BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_INDEX32);
    lightGridBuffer = bgfx::createDynamicIndexBuffer(currentClusterCount,
                                                     BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_INDEX32);
}

void ClusterShader::shutdown()
{
    bgfx::destroy(clusterCountVecUniform);
    bgfx::destroy(clusterSizeVecUniform);
    bgfx::destroy(zNearFarVecUniform);

    bgfx::destroy(clustersBuffer);
    bgfx::destroy(lightIndicesBuffer);
    bgfx::destroy(lightGridBuffer);

    clusterCountVecUniform = clusterSizeVecUniform = zNearFarVecUniform = BGFX_INVALID_HANDLE;
    clustersBuffer = BGFX_INVALID_HANDLE;
    lightIndicesBuffer = lightGridBuffer = BGFX_INVALID_HANDLE;
}

void ClusterShader::setUniforms(const Scene* scene, uint16_t screenWidth, uint16_t screenHeight) const
{
    assert(scene != nullptr);

    float clusterCountVec[4] = { (float)currentClustersX,
                                 (float)currentClustersY,
                                 (float)currentClustersZ };
    bgfx::setUniform(clusterCountVecUniform, clusterCountVec);

    float clusterSizesVec[4] = { std::ceil((float)screenWidth / (float)currentClustersX),
                                 std::ceil((float)screenHeight / (float)currentClustersY),
                                    (float)currentMaxLightsPerCluster };
    bgfx::setUniform(clusterSizeVecUniform, clusterSizesVec);

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

std::tuple<uint32_t, uint32_t, uint32_t> ClusterShader::getClusterCount() const
{
    return std::make_tuple(currentClustersX, currentClustersY, currentClustersZ);
}
