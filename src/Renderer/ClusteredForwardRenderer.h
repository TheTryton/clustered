#pragma once

#include "Renderer.h"
#include "ClusterShader.h"

class ClusteredForwardRenderer : public Renderer
{
public:
    ClusteredForwardRenderer(const Scene* scene, const Config* config);

    static bool supported();

    virtual void onInitialize() override;
    virtual void onRender(float dt) override;
    virtual void onOptionsChanged() override;
    virtual void onShutdown() override;

private:
    bool buffersNeedUpdate = true;

    glm::mat4 oldProjMat = glm::mat4(0.0f);

    bgfx::ProgramHandle clusterBuildingComputeProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle lightCullingComputeProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle lightingProgram = BGFX_INVALID_HANDLE;
    bgfx::ProgramHandle debugVisProgram = BGFX_INVALID_HANDLE;

    ClusterShader clusters;
};
