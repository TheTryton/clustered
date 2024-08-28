#pragma once

#include <bigg.hpp>
#include <bx/string.h>
#include <spdlog/spdlog.h>
#include <map>
#include <memory>
#include <random>
#include <chrono>

class ClusterUI;
class Config;
class Scene;
class Renderer;

struct stats
{
    struct view
    {
        double avgCpuTime{};
        double avgGpuTime{};
    };

    std::map<std::string, view> views;
    double avgFrameTimeCpu{};
    double avgFrameTimeGpu{};
};

class Cluster : public bigg::Application
{
    friend class ClusterUI;

public:
    Cluster(const Config& config);
    ~Cluster();

    int run(int argc, char* argv[]);

    // bigg callbacks

    void initialize(int _argc, char* _argv[]) override;
    void onReset() override;
    void onKey(int key, int scancode, int action, int mods) override;
    void onCursorPos(double xpos, double ypos) override;
    void onCursorEnter(int entered) override;
    void onScroll(double xoffset, double yoffset) override;
    void update(float dt) override;
    int shutdown() override;

    //

    void close();
    void toggleFullscreen();

    enum class RenderPath : int
    {
        Forward,
        Deferred,
        TiledSingleForward,
        TiledSingleDeferred,
        TiledMultipleForward,
        TiledMultipleDeferred,
        ClusteredForward,
        ClusteredDeferred
    };
    void setRenderPath(RenderPath path);

    void generateLights(unsigned int count);
    void moveLights(float t, float dt);
    stats getFrameTimeStatistics() const;

private:
    class BgfxCallbacks : public bgfx::CallbackI
    {
    public:
        BgfxCallbacks(Cluster& app) : app(app) { }
        virtual ~BgfxCallbacks() { }
        virtual void fatal(const char*, uint16_t, bgfx::Fatal::Enum, const char*) override;
        virtual void traceVargs(const char*, uint16_t, const char*, va_list) override;
        virtual void profilerBegin(const char*, uint32_t, const char*, uint16_t) override { }
        virtual void profilerBeginLiteral(const char*, uint32_t, const char*, uint16_t) override { }
        virtual void profilerEnd() override { }
        virtual uint32_t cacheReadSize(uint64_t) override
        {
            return 0;
        }
        virtual bool cacheRead(uint64_t, void*, uint32_t) override
        {
            return false;
        }
        virtual void cacheWrite(uint64_t, const void*, uint32_t) override { }
        virtual void captureBegin(uint32_t, uint32_t, uint32_t, bgfx::TextureFormat::Enum, bool) override { }
        virtual void captureEnd() override { }
        virtual void captureFrame(const void*, uint32_t) override { }
        virtual void screenShot(const char*, uint32_t, uint32_t, uint32_t, const void*, uint32_t, bool yflip) override
        {
        }

    private:
        Cluster& app;
    };

    std::mt19937 mt{1337};
    std::uniform_real_distribution<float> dist{0.0f, 1.0f};

    spdlog::sink_ptr logFileSink = nullptr;

    double mouseX = -1.0, mouseY = -1.0;

    BgfxCallbacks callbacks;

    // pointers to avoid circular dependencies
    // Cluster has Config member, Config needs enum definitions

    std::unique_ptr<Config> config;
    std::unique_ptr<ClusterUI> ui;
    std::unique_ptr<Scene> scene;

    std::unique_ptr<Renderer> renderer;

    std::chrono::high_resolution_clock::time_point startMeasurement;
    stats frameTimeStatistics{};
    uint64_t completedFrames{};
};
