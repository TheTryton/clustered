#ifndef CLUSTERS_SH_HEADER_GUARD
#define CLUSTERS_SH_HEADER_GUARD

#include <bgfx_compute.sh>
#include "samplers.sh"
#include "util.sh"

// taken from Doom
// http://advances.realtimerendering.com/s2016/Siggraph2016_idTech6.pdf

// workgroup size of the culling compute shader
// D3D compute shaders only allow up to 1024 threads per workgroup
// GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS also only guarantees 1024
#define CLUSTERS_X_THREADS 16
#define CLUSTERS_Y_THREADS 8
#define CLUSTERS_Z_THREADS 4

uniform vec4 u_clusterCountVec; // clusters count
uniform vec4 u_clusterSizeVec; // cluster size in screen coordinates (pixels)
uniform vec4 u_zNearFarVec;

#define u_maxLightsPerCluster ((uint)u_clusterSizeVec.z)
#define u_clusterCount        ((uvec3)u_clusterCountVec.xyz)
#define u_clusterSize         ((uvec2)u_clusterSizeVec.xy)
#define u_zNear               u_zNearFarVec.x
#define u_zFar                u_zNearFarVec.y

#ifdef WRITE_CLUSTERS
    #define CLUSTER_BUFFER BUFFER_RW
#else
    #define CLUSTER_BUFFER BUFFER_RO
#endif

// light indices belonging to clusters
CLUSTER_BUFFER(b_clusterLightIndices, uint, SAMPLER_CLUSTERS_LIGHTINDICES);
// for each cluster: number of point lights
CLUSTER_BUFFER(b_clusterLightGrid, uint, SAMPLER_CLUSTERS_LIGHTGRID);

// these are only needed for building clusters and light culling, not in the fragment shader
#ifdef WRITE_CLUSTERS
// list of clusters (5 vec4's each, frustrum planes and depthNearFar)
CLUSTER_BUFFER(b_clusters, vec4, SAMPLER_CLUSTERS_CLUSTERS);
#endif

struct Cluster
{
    vec4 frustrumPlanes[4];
    vec4 depthNearFar;
};

struct LightGrid
{
    uint offset;
    uint pointLights;
};

#ifdef WRITE_CLUSTERS
bool isClusterValid(uint clusterIndex)
{
    return clusterIndex < u_clusterCount.x * u_clusterCount.y * u_clusterCount.z;
}

uint getComputeIndex(uvec3 clusterIndex3D)
{
    uint clusterIndex = clusterIndex3D.z * u_clusterCount.x * u_clusterCount.y +
                        clusterIndex3D.y * u_clusterCount.x +
                        clusterIndex3D.x;
    if(clusterIndex3D.x >= u_clusterCount.x || clusterIndex3D.y >= u_clusterCount.y || clusterIndex3D.z >= u_clusterCount.z)
        return u_clusterCount.x * u_clusterCount.y * u_clusterCount.z;
    return clusterIndex;
}

Cluster getCluster(uint index)
{
    Cluster cluster;
    if(!isClusterValid(index))
    {
        cluster.frustrumPlanes[0] = vec4_splat(0.0);
        cluster.frustrumPlanes[1] = vec4_splat(0.0);
        cluster.frustrumPlanes[2] = vec4_splat(0.0);
        cluster.frustrumPlanes[3] = vec4_splat(0.0);
        cluster.depthNearFar = vec4_splat(0.0);
    }
    else
    {
        cluster.frustrumPlanes[0] = b_clusters[5 * index + 0];
        cluster.frustrumPlanes[1] = b_clusters[5 * index + 1];
        cluster.frustrumPlanes[2] = b_clusters[5 * index + 2];
        cluster.frustrumPlanes[3] = b_clusters[5 * index + 3];
        cluster.depthNearFar = b_clusters[5 * index + 4];
    }
    return cluster;
}
#endif

uint getLightGridCount(uint cluster)
{
    return b_clusterLightGrid[cluster];
}

uint getGridLightClusterOffset(uint cluster)
{
    return cluster * u_maxLightsPerCluster;
}

uint getGridLightIndex(uint clusterOffset, uint offset)
{
    return b_clusterLightIndices[clusterOffset + offset];
}

// cluster depth index from depth in screen coordinates (gl_FragCoord.z)
uint getClusterZIndex(float screenDepth)
{
    // this can be calculated on the CPU and passed as a uniform
    // only leaving it here to keep most of the relevant code in the shaders for learning purposes
    float scale = float(u_clusterCount.z) / log(u_zFar / u_zNear);
    float bias = -(float(u_clusterCount.z) * log(u_zNear) / log(u_zFar / u_zNear));

    float eyeDepth = screen2EyeDepth(screenDepth, u_zNear, u_zFar);
    uint zIndex = uint(max(log(eyeDepth) * scale + bias, 0.0));
    return zIndex;
}

// cluster index from fragment position in window coordinates (gl_FragCoord)
uint getClusterIndex(vec4 fragCoord)
{
    uint zIndex = getClusterZIndex(fragCoord.z);
    uvec3 indices = uvec3(uvec2(fragCoord.xy / u_clusterSize.xy), zIndex);
    uint cluster = u_clusterCount.x * u_clusterCount.y * indices.z +
                   u_clusterCount.x * indices.y +
                   indices.x;
    return cluster;
}

#endif // CLUSTERS_SH_HEADER_GUARD
