#pragma once

#include "Renderer.h"
#include "TileShader.h"

class TiledMultipleForwardRenderer : public Renderer
{
public:
    TiledMultipleForwardRenderer(const Scene* scene, const Config* config);

    static bool supported();

    virtual void onInitialize() override;
    virtual void onRender(float dt) override;
    virtual void onReset() override;
    virtual void onOptionsChanged() override;
    virtual void onShutdown() override;

private:
    bool buffersNeedUpdate = true;
    glm::mat4 oldProjMat = glm::mat4(0.0f);

    bgfx::ProgramHandle tileBuildingComputeProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle lightCullingComputeProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle lightingProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle debugVisProgram = BGFX_INVALID_HANDLE;

    TileShader tiles;
};
