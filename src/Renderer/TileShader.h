#pragma once

#include <bgfx/bgfx.h>
#include <tuple>

class Scene;

class TileShader
{
public:
    TileShader();

    void initialize();
    void shutdown();

    void setUniforms(const Scene* scene, uint16_t screenWidth, uint16_t screenHeight) const;
    void bindBuffers(bool lightingPass = true) const;
    void updateBuffers(uint16_t screenWidth, uint16_t screenHeight, uint32_t maxLightsPerTile, uint32_t tilePixelSizeX, uint32_t tilePixelSizeY);

    std::tuple<uint32_t, uint32_t> getTilePixelSize() const;
    // limit number of threads (D3D only allows up to 1024, there might also be shared memory limitations)
    // shader will be run by 6 work groups
    static constexpr uint32_t TILES_X_THREADS = 16;
    static constexpr uint32_t TILES_Y_THREADS = 16;
private:
    struct TileVertex
    {
        // w is padding
        float frustrumPlanes[4][4];

        static void init()
        {
            layout.begin()
                .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord2, 4, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord3, 4, bgfx::AttribType::Float)
                .end();
        }
        static bgfx::VertexLayout layout;
    };

    uint16_t currentWidth;
    uint16_t currentHeight;
    uint32_t currentMaxLightsPerTile;
    uint32_t currentTilePixelSizeX;
    uint32_t currentTilePixelSizeY;

    bgfx::UniformHandle tileSizeVecUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle tileCountVecUniform = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle zNearFarVecUniform = BGFX_INVALID_HANDLE;

    // dynamic buffers can be created empty
    bgfx::DynamicVertexBufferHandle tilesBuffer = BGFX_INVALID_HANDLE;
    bgfx::DynamicIndexBufferHandle lightIndicesBuffer = BGFX_INVALID_HANDLE;
    bgfx::DynamicIndexBufferHandle lightGridBuffer = BGFX_INVALID_HANDLE;
};
