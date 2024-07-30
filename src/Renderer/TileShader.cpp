#include "TileShader.h"

#include "Scene/Scene.h"
#include "Renderer/Samplers.h"
#include <glm/common.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

bgfx::VertexLayout TileShader::TileVertex::layout;

TileShader::TileShader()
{
}

void TileShader::initialize()
{
    TileVertex::init();

    tileSizeVecUniform = bgfx::createUniform("u_tileSizeVec", bgfx::UniformType::Vec4);
    tileCountVecUniform = bgfx::createUniform("u_tileCountVec", bgfx::UniformType::Vec4);
    zNearFarVecUniform = bgfx::createUniform("u_zNearFarVec", bgfx::UniformType::Vec4);

    atomicIndexBuffer = bgfx::createDynamicIndexBuffer(1, BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_INDEX32);
}

void TileShader::updateBuffers(uint16_t screenWidth, uint16_t screenHeight)
{
    if(currentWidth == screenWidth && currentHeight == screenHeight)
    {
        return;
    }

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

    const uint16_t tilesX = (uint16_t)std::ceil((float)screenWidth / TILE_PIXEL_SIZE);
    const uint16_t tilesY = (uint16_t)std::ceil((float)screenHeight / TILE_PIXEL_SIZE);
    const uint32_t tilesCount = tilesX * tilesY;
    tilesBuffer = bgfx::createDynamicVertexBuffer(tilesCount, TileVertex::layout, BGFX_BUFFER_COMPUTE_READ_WRITE);
    lightIndicesBuffer = bgfx::createDynamicIndexBuffer(tilesCount * MAX_LIGHTS_PER_TILE,
                                                        BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_INDEX32);
    // we have to specify the compute buffer format here since we need uvec4
    // not needed for the rest, the default format for vertex/index buffers is vec4/uint
    lightGridBuffer =
        bgfx::createDynamicIndexBuffer(tilesCount * 4,
                                       BGFX_BUFFER_COMPUTE_READ_WRITE | BGFX_BUFFER_INDEX32 |
                                           BGFX_BUFFER_COMPUTE_FORMAT_32X4 | BGFX_BUFFER_COMPUTE_TYPE_UINT);

    currentWidth = screenWidth;
    currentHeight = screenHeight;
}

void TileShader::shutdown()
{
    bgfx::destroy(tileSizeVecUniform);
    bgfx::destroy(zNearFarVecUniform);

    bgfx::destroy(tilesBuffer);
    bgfx::destroy(lightIndicesBuffer);
    bgfx::destroy(lightGridBuffer);
    bgfx::destroy(atomicIndexBuffer);

    tileSizeVecUniform = zNearFarVecUniform = BGFX_INVALID_HANDLE;
    tilesBuffer = BGFX_INVALID_HANDLE;
    lightIndicesBuffer = lightGridBuffer = atomicIndexBuffer = BGFX_INVALID_HANDLE;
}

void TileShader::setUniforms(const Scene* scene, uint16_t screenWidth, uint16_t screenHeight) const
{
    assert(scene != nullptr);

    const uint16_t tilesX = (uint16_t)std::ceil((float)screenWidth / TILE_PIXEL_SIZE);
    const uint16_t tilesY = (uint16_t)std::ceil((float)screenHeight / TILE_PIXEL_SIZE);

    float tileCountVec[4] = { (float)tilesX, (float)tilesY };
    bgfx::setUniform(tileCountVecUniform, tileCountVec);

    float tileSizeVec[4] = { TILE_PIXEL_SIZE, TILE_PIXEL_SIZE };
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
        bgfx::setBuffer(Samplers::TILES_ATOMICINDEX, atomicIndexBuffer, access);
    }
    bgfx::setBuffer(Samplers::TILES_LIGHTINDICES, lightIndicesBuffer, access);
    bgfx::setBuffer(Samplers::TILES_LIGHTGRID, lightGridBuffer, access);
}
