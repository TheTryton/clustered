#pragma once

#include "Renderer.h"
#include "ClusterForwardShader.h"

class ClusteredForwardRenderer : public Renderer
{
public:
    ClusteredForwardRenderer(const Scene* scene);

    static bool supported();

    virtual void onInitialize() override;
    virtual void onRender(float dt) override;
    virtual void onShutdown() override;

private:
    glm::mat4 oldProjMat;

    bgfx::ProgramHandle clusterBuildingComputeProgram;
    bgfx::ProgramHandle lightCullingComputeProgram;
    bgfx::ProgramHandle lightingProgram;
    bgfx::ProgramHandle debugVisProgram;

    ClusterForwardShader clusters;
};
