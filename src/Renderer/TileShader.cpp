#include "TileShader.h"

#include "Scene/Scene.h"
#include "Renderer/Samplers.h"
#include <glm/common.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

bgfx::VertexLayout TileShader::TileVertex::layout;

TileShader::TileShader() = default;

void TileShader::initialize()
{
    TileVertex::init();

    tileSizeVecUniform = bgfx::createUniform("u_tileSizeVec", bgfx::UniformType::Vec4);
    tileCountVecUniform = bgfx::createUniform("u_tileCountVec", bgfx::UniformType::Vec4);
    zNearFarVecUniform = bgfx::createUniform("u_zNearFarVec", bgfx::UniformType::Vec4);
}

void TileShader::updateBuffers(uint16_t screenWidth, uint16_t screenHeight, uint32_t maxLightsPerTile, uint32_t tilePixelSizeX, uint32_t tilePixelSizeY)
{
    if(currentWidth == screenWidth && currentHeight == screenHeight && currentMaxLightsPerTile == maxLightsPerTile &&
       currentTilePixelSizeX == tilePixelSizeX && currentTilePixelSizeY == tilePixelSizeY)
    {
        return;
    }

    currentWidth = screenWidth;
    currentHeight = screenHeight;
    currentMaxLightsPerTile = maxLightsPerTile;
    currentTilePixelSizeX = tilePixelSizeX;
    currentTilePixelSizeY = tilePixelSizeY;

    const uint16_t currentTilesX = (uint16_t)std::ceil((float)currentWidth / currentTilePixelSizeX);
    const uint16_t currentTilesY = (uint16_t)std::ceil((float)currentHeight / currentTilePixelSizeY);
    const uint32_t currentTilesCount = currentTilesX * currentTilesY;

    if(isValid(tilesBuffer))
    {
        bgfx::destroy(tilesBuffer);
    }

    if(isValid(lightIndicesBuffer))
    {
        bgfx::destroy(lightIndicesBuffer);
    }

    if(isValid(lightGridBuffer))
    {
        bgfx::destroy(lightGridBuffer);
    }

    tilesBuffer =
        bgfx::createDynamicVertexBuffer(currentTilesCount, TileVertex::layout, BGFX_BUFFER_COMPUTE_READ_WRITE);
    lightIndicesBuffer = bgfx::createDynamicIndexBuffer(currentTilesCount * maxLightsPerTile,
                                                        BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_INDEX32);
    lightGridBuffer = bgfx::createDynamicIndexBuffer(currentTilesCount,
                                                     BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_INDEX32);
}

void TileShader::shutdown()
{
    bgfx::destroy(tileSizeVecUniform);
    bgfx::destroy(zNearFarVecUniform);

    bgfx::destroy(tilesBuffer);
    bgfx::destroy(lightIndicesBuffer);
    bgfx::destroy(lightGridBuffer);

    tileSizeVecUniform = zNearFarVecUniform = BGFX_INVALID_HANDLE;
    tilesBuffer = BGFX_INVALID_HANDLE;
    lightIndicesBuffer = lightGridBuffer = BGFX_INVALID_HANDLE;
}

void TileShader::setUniforms(const Scene* scene, uint16_t screenWidth, uint16_t screenHeight) const
{
    assert(scene != nullptr);

    const uint16_t tilesX = (uint16_t)std::ceil((float)screenWidth / currentTilePixelSizeX);
    const uint16_t tilesY = (uint16_t)std::ceil((float)screenHeight / currentTilePixelSizeY);

    float tileCountVec[4] = { (float)tilesX, (float)tilesY };
    bgfx::setUniform(tileCountVecUniform, tileCountVec);

    float tileSizeVec[4] = { (float)currentTilePixelSizeX, (float)currentTilePixelSizeY, (float)currentMaxLightsPerTile };
    bgfx::setUniform(tileSizeVecUniform, tileSizeVec);

    float zNearFarVec[4] = { scene->camera.zNear, scene->camera.zFar };
    bgfx::setUniform(zNearFarVecUniform, zNearFarVec);
}

void TileShader::bindBuffers(bool lightingPass) const
{
    // binding ReadWrite in the fragment shader doesn't work with D3D11/12
    bgfx::Access::Enum access = lightingPass ? bgfx::Access::Read : bgfx::Access::ReadWrite;
    if(!lightingPass)
    {
        bgfx::setBuffer(Samplers::TILES_TILES, tilesBuffer, access);
    }
    bgfx::setBuffer(Samplers::TILES_LIGHTINDICES, lightIndicesBuffer, access);
    bgfx::setBuffer(Samplers::TILES_LIGHTGRID, lightGridBuffer, access);
}

std::tuple<uint32_t, uint32_t> TileShader::getTilePixelSize() const
{
    return std::make_tuple(currentTilePixelSizeX, currentTilePixelSizeY);
}
