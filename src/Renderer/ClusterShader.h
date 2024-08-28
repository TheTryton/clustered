#pragma once

#include <bgfx/bgfx.h>
#include <tuple>

class Scene;

class ClusterShader
{
public:
    ClusterShader();

    void initialize();
    void shutdown();

    void setUniforms(const Scene* scene, uint16_t screenWidth, uint16_t screenHeight) const;
    void bindBuffers(bool lightingPass = true) const;
    void updateBuffers(uint32_t maxLightsPerCluster, uint16_t screenWidth, uint16_t screenHeight, bool clustersXYAsPixelSizes, uint32_t clustersX, uint32_t clustersY, uint32_t clustersZ);

    std::tuple<uint32_t, uint32_t, uint32_t> getClusterCount() const;

    //static constexpr uint32_t CLUSTERS_X = 16;
    //static constexpr uint32_t CLUSTERS_Y = 8;
    //static constexpr uint32_t CLUSTERS_Z = 48;

    // limit number of threads (D3D only allows up to 1024, there might also be shared memory limitations)
    // shader will be run by 6 work groups
    static constexpr uint32_t CLUSTERS_X_THREADS = 16;
    static constexpr uint32_t CLUSTERS_Y_THREADS = 8;
    static constexpr uint32_t CLUSTERS_Z_THREADS = 4;

    //static constexpr uint32_t CLUSTER_COUNT = CLUSTERS_X * CLUSTERS_Y * CLUSTERS_Z;

    static constexpr uint32_t MAX_LIGHTS_PER_CLUSTER = 2048;

private:
    struct ClusterVertex
    {
        // w is padding
        float minBounds[4];
        float maxBounds[4];

        static void init()
        {
            layout.begin()
                .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord2, 4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord4, 4, bgfx::AttribType::Float)
                .end();
        }
        static bgfx::VertexLayout layout;
    };

    uint32_t currentMaxLightsPerCluster{};
    bool currentClustersXYAsPixelSizes{};
    uint32_t currentClustersX{};
    uint32_t currentClustersY{};
    uint32_t currentClustersZ{};

    bgfx::UniformHandle clusterCountVecUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle clusterSizeVecUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle zNearFarVecUniform = BGFX_INVALID_HANDLE;

    // dynamic buffers can be created empty
    bgfx::DynamicVertexBufferHandle clustersBuffer = BGFX_INVALID_HANDLE;
    bgfx::DynamicIndexBufferHandle lightIndicesBuffer = BGFX_INVALID_HANDLE;
    bgfx::DynamicIndexBufferHandle lightGridBuffer = BGFX_INVALID_HANDLE;
};
