#pragma once

#include "Renderer.h"
#include "TileShader.h"

class TiledRenderer : public Renderer
{
public:
    TiledRenderer(const Scene* scene);

    static bool supported();

    virtual void onInitialize() override;
    virtual void onRender(float dt) override;
    virtual void onShutdown() override;

private:
    glm::mat4 oldProjMat = glm::mat4(0.0f);

    bgfx::ProgramHandle tileBuildingComputeProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle resetCounterComputeProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle lightCullingComputeProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle lightingProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle debugVisProgram = BGFX_INVALID_HANDLE;

    TileShader tiles;
};
