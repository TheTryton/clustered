#define WRITE_CLUSTERS

#include <bgfx_compute.sh>
#include "lights.sh"
#include "clusters.sh"

// compute shader to cull lights against cluster bounds
// builds a light grid that holds indices of lights for each cluster
// largely inspired by http://www.aortiz.me/2018/12/21/CG.html

float getSignedDistanceFromPlane(vec3 p, vec4 eqn)
{
    return dot(eqn.xyz, p);
}

bool pointLightIntersectsCluster(PointLight light, Cluster cluster, float halfZ)
{
    vec3 center = light.position;
    float r = light.radius;
    float near = cluster.depthNearFar.x;
    float far = cluster.depthNearFar.y;
    if(
        (getSignedDistanceFromPlane(center, cluster.frustrumPlanes[0]) < r) &&
        (getSignedDistanceFromPlane(center, cluster.frustrumPlanes[1]) < r) &&
        (getSignedDistanceFromPlane(center, cluster.frustrumPlanes[2]) < r) &&
        (getSignedDistanceFromPlane(center, cluster.frustrumPlanes[3]) < r)
    )
    {
        if(-center.z + near < r && center.z - halfZ < r)
            return true;
        if(-center.z + halfZ < r && center.z - far < r)
            return true;
    }

    return false;
}

/*bool pointLightIntersectsCluster(PointLight light, Cluster cluster)
{
    // NOTE: expects light.position to be in view space like the cluster bounds
    // global light list has world space coordinates, but we transform the
    // coordinates in the shared array of lights after copying

    // get closest point to sphere center
    vec3 closest = max(cluster.minBounds, min(light.position, cluster.maxBounds));
    // check if point is inside the sphere
    vec3 dist = closest - light.position;
    return dot(dist, dist) <= (light.radius * light.radius);
}*/

//#define gl_WorkGroupSize uvec3(CLUSTERS_X_THREADS, CLUSTERS_Y_THREADS, CLUSTERS_Z_THREADS)
#define GROUP_SIZE (CLUSTERS_X_THREADS * CLUSTERS_Y_THREADS * CLUSTERS_Z_THREADS)

// light cache for the current workgroup
// group shared memory has lower latency than global memory

// there's no guarantee on the available shared memory
// as a guideline the minimum value of GL_MAX_COMPUTE_SHARED_MEMORY_SIZE is 32KB
// with a workgroup size of 16*8*4 this is 64 bytes per light
// however, using all available memory would limit the compute shader invocation to only 1 workgroup
SHARED PointLight lights[GROUP_SIZE];

// each thread handles one cluster
NUM_THREADS(CLUSTERS_X_THREADS, CLUSTERS_Y_THREADS, CLUSTERS_Z_THREADS)
void main()
{
    uint visibleCount = 0;

    // the way we calculate the index doesn't really matter here since we write to the same index in the light grid as we read from the cluster buffer
    uint clusterIndex = getComputeIndex(gl_GlobalInvocationID);
    uint clusterOffset = getGridLightClusterOffset(clusterIndex);
    Cluster cluster = getCluster(clusterIndex);

    float halfZ = (cluster.depthNearFar.x + cluster.depthNearFar.y) / 2;

    // we have a cache of GROUP_SIZE lights
    // have to run this loop several times if we have more than GROUP_SIZE lights
    uint lightCount = pointLightCount();
    uint lightOffset = 0;
    while(lightOffset < lightCount)
    {
        // wait for all threads to enter this section in case multiple light
        // copies are required
        barrier();

        // read GROUP_SIZE lights into shared memory
        // each thread copies one light
        uint batchSize = min(GROUP_SIZE, lightCount - lightOffset);

        if(uint(gl_LocalInvocationIndex) < batchSize)
        {
            uint lightIndex = lightOffset + gl_LocalInvocationIndex;
            PointLight light = getPointLight(lightIndex);
            // transform to view space (expected by pointLightAffectsCluster)
            // do it here once rather than for each cluster later
            light.position = mul(u_view, vec4(light.position, 1.0)).xyz;
            lights[gl_LocalInvocationIndex] = light;
        }

        // wait for all threads to finish copying
        barrier();

        // each thread is one cluster and checks against all lights in the cache
        for(uint i = 0; i < batchSize && isClusterValid(clusterIndex); i++)
        {
            if(pointLightIntersectsCluster(lights[i], cluster, halfZ))
            {
                b_clusterLightIndices[clusterOffset + visibleCount] = lightOffset + i;
                visibleCount++;
            }

            if(visibleCount >= u_maxLightsPerCluster)
                break;
        }

        lightOffset += batchSize;
    }

    // wait for all threads to finish checking lights
    barrier();

    if(isClusterValid(clusterIndex))
    {
        // write light grid for this cluster
        b_clusterLightGrid[clusterIndex] = visibleCount;
    }
}
