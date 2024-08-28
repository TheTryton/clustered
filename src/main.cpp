#include "Cluster.h"
#include "Config.h"
#include "assimp/DefaultLogger.hpp"
#include "Log/AssimpSource.h"
#include <fstream>
#include <sstream>

using namespace std;

struct tiled_parameter_group
{
    std::string name;
    int maxLightsPerTile;
    int tileSizeX, tileSizeY;
    int lightLimit;
};

struct clustered_parameter_group
{
    std::string name;
    int maxLightsPerCluster;
    int clusterCountX, clusterCountY, clusterCountZ;
    int lightLimit;
};

struct light_range
{
    int start;
    int end;
    int step;
};

struct resolution
{
    int width;
    int height;
};

static const vector<tiled_parameter_group> tiledParameterGroups = {
    tiled_parameter_group{"t32s", 32, 16, 16, 128},
    tiled_parameter_group{"t32m", 32, 32, 32, 128},
    tiled_parameter_group{"t32h", 32, 64, 64, 128},
    tiled_parameter_group{"t128s", 128, 16, 16, 512},
    tiled_parameter_group{"t128m", 128, 32, 32, 512},
    tiled_parameter_group{"t128h", 128, 64, 64, 512},
    tiled_parameter_group{"t512s", 512, 16, 16, 2048},
    tiled_parameter_group{"t512m", 512, 32, 32, 2048},
    tiled_parameter_group{"t512h", 512, 64, 64, 2048},
    tiled_parameter_group{"t2048s", 2048, 16, 16, 16384},
    tiled_parameter_group{"t2048m", 2048, 32, 32, 16384},
    tiled_parameter_group{"t2048h", 2048, 64, 64, 16384},
};

static const vector<clustered_parameter_group> clusteredParameterGroups = {
    clustered_parameter_group{"c32s", 32, 12, 8, 16, 128},
    clustered_parameter_group{"c32m", 32, 16, 8, 24, 128},
    clustered_parameter_group{"c32h", 32, 32, 16, 48, 128},
    clustered_parameter_group{"c32u", 32, 64, 64, 32, 128},

    clustered_parameter_group{"c128s", 128, 12, 8, 16, 512},
    clustered_parameter_group{"c128m", 128, 16, 8, 24, 512},
    clustered_parameter_group{"c128h", 128, 32, 16, 48, 512},
    clustered_parameter_group{"c128u", 128, 64, 64, 32, 512},

    clustered_parameter_group{"c512s", 512, 12, 8, 16, 2048},
    clustered_parameter_group{"c512m", 512, 16, 8, 24, 2048},
    clustered_parameter_group{"c512h", 512, 32, 16, 48, 2048},
    clustered_parameter_group{"c512u", 512, 64, 64, 32, 2048},

    clustered_parameter_group{"c2048s", 2048, 12, 8, 16, 16384},
    clustered_parameter_group{"c2048m", 2048, 16, 8, 24, 16384},
    clustered_parameter_group{"c2048h", 2048, 32, 16, 48, 16384},
    clustered_parameter_group{"c2048u", 2048, 64, 64, 32, 16384},
};

static const vector<int> lightCounts = {
    0,
    2,
    4,
    16,
    32,
    64,
    128,
    512,
    1024,
    2048,
    4096,
    8192,
    16384,
};

static const vector<resolution> resolutions = {
    resolution{1920, 1080},
    resolution{2560, 1440},
    resolution{3840, 2160},
};

struct render_path
{
    std::string name;
    Cluster::RenderPath renderPath;
};

static const vector<render_path> basicRenderPaths = {
    render_path{"forward", Cluster::RenderPath::Forward},
    render_path{"deferred", Cluster::RenderPath::Deferred},
};

static const vector<render_path> renderPathsForTiled = {
    render_path{"tiled_forward_single", Cluster::RenderPath::TiledSingleForward},
    render_path{"tiled_deferred_single", Cluster::RenderPath::TiledSingleDeferred},
    render_path{"tiled_forward_multiple", Cluster::RenderPath::TiledMultipleDeferred},
    render_path{"tiled_deferred_multiple", Cluster::RenderPath::TiledMultipleDeferred},
};

static const vector<render_path> renderPathsForClustered = {
    render_path{"clustered_forward", Cluster::RenderPath::ClusteredForward},
    render_path{"clustered_deferred", Cluster::RenderPath::ClusteredForward},
};

stats run_benchmark(int argc, char* argv[], const Config& config)
{
    Cluster app{config};
    app.run(argc, argv);
    const auto stats = app.getFrameTimeStatistics();
    return stats;
}

std::string join_parameter_group(const tiled_parameter_group& parameterGroup)
{
    std::stringstream ss;
    ss << parameterGroup.name << ";" << parameterGroup.maxLightsPerTile << ";";
    ss << parameterGroup.tileSizeX << ";" << parameterGroup.tileSizeY;
    return ss.str();
}

std::string join_parameter_group(const clustered_parameter_group& parameterGroup)
{
    std::stringstream ss;
    ss << parameterGroup.name << ";" << parameterGroup.maxLightsPerCluster << ";";
    ss << parameterGroup.clusterCountX << ";" << parameterGroup.clusterCountY << ";" << parameterGroup.clusterCountZ;
    return ss.str();
}

std::string join_views(const stats& stats)
{
    std::stringstream ss;
    for(const auto& view : stats.views)
    {
        ss << view.first << ";" << view.second.avgCpuTime << ";" << view.second.avgGpuTime << ";";
    }
    return ss.str();
}

static AssimpLogSource logSource;

int main(int argc, char* argv[])
{
    Assimp::DefaultLogger::set(&logSource);

    // CSV format
    // resolutionx, resolutiony, light_count, render_path_type, render_properties (;separated), cpuTime, gpuTime, view_timings (key;value; ;separated)
    Config config;
    if(argc >= 3)
    {
        config.backbufferResolutionX = stoi(argv[1]);
        config.backbufferResolutionY = stoi(argv[2]);
    }

    Cluster app{config};
    return app.run(argc, argv);

    ofstream output("measurements.csv");
    config.showUI = false;
    config.measureOverSeconds = 2;

    for(const auto& res : resolutions)
    {
        config.backbufferResolutionX = res.width;
        config.backbufferResolutionY = res.height;

        for(const auto& lightCount : lightCounts)
        {
            config.lights = lightCount;

            for(const auto& renderPath : basicRenderPaths)
            {
                config.renderPath = renderPath.renderPath;

                const auto stats = run_benchmark(argc, argv, config);

                output << res.width << "," << res.height << ",";
                output << lightCount << "," << renderPath.name << ",";
                output << "N/A"
                       << "," << stats.avgFrameTimeCpu << "," << stats.avgFrameTimeGpu << ",";
                output << join_views(stats) << endl;
            }

            for(const auto& renderPath : renderPathsForTiled)
            {
                config.renderPath = renderPath.renderPath;

                for(const auto& parameterGroup : tiledParameterGroups)
                {
                    if(lightCount >= parameterGroup.lightLimit)
                        continue;

                    config.maxLightsPerTileOrCluster = parameterGroup.maxLightsPerTile;
                    config.tilePixelSizeX = parameterGroup.tileSizeX;
                    config.tilePixelSizeY = parameterGroup.tileSizeY;

                    const auto stats = run_benchmark(argc, argv, config);

                    output << res.width << "," << res.height << ",";
                    output << lightCount << "," << renderPath.name << ",";
                    output << join_parameter_group(parameterGroup) << "," << stats.avgFrameTimeCpu << ","
                           << stats.avgFrameTimeGpu << ",";
                    output << join_views(stats) << endl;
                }
            }

            for(const auto& renderPath : renderPathsForClustered)
            {
                config.renderPath = renderPath.renderPath;

                for(const auto& parameterGroup : clusteredParameterGroups)
                {
                    if(lightCount >= parameterGroup.lightLimit)
                        continue;

                    config.maxLightsPerTileOrCluster = parameterGroup.maxLightsPerCluster;
                    config.clustersX = parameterGroup.clusterCountX;
                    config.clustersY = parameterGroup.clusterCountY;
                    config.clustersZ = parameterGroup.clusterCountZ;

                    const auto stats = run_benchmark(argc, argv, config);

                    output << res.width << "," << res.height << ",";
                    output << lightCount << "," << renderPath.name << ",";
                    output << join_parameter_group(parameterGroup) << "," << stats.avgFrameTimeCpu << ","
                           << stats.avgFrameTimeGpu << ",";
                    output << join_views(stats) << endl;
                }
            }
        }
    }

    return 0;
}
