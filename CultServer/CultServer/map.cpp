#include "map.h"

bool LoadOBJ(const std::string& path,
    std::vector<MapVertex>& outVertices,
    std::vector<MapTriangle>& outTriangles)
{
    std::ifstream file(path);
    if (!file.is_open())
        return false;

    std::string line;
    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (!type.empty() && (unsigned char)type[0] == 0xEF)
        {
            type = type.substr(3);
        }


        if (type == "v")
        {
            MapVertex v{};
            ss >> v.x >> v.y >> v.z;
            outVertices.push_back(v);
        }
        else if (type == "f")
        {
            MapTriangle t{};
            std::string s0, s1, s2;

            ss >> s0 >> s1 >> s2;

            auto parseIndex = [](const std::string& s) {
                size_t pos = s.find('/');
                return std::stoi(pos == std::string::npos ? s : s.substr(0, pos)) - 1;
                };

            t.v0 = parseIndex(s0);
            t.v1 = parseIndex(s1);
            t.v2 = parseIndex(s2);

            outTriangles.push_back(t);
        }
    }
    return true;
}

bool LoadOBJAndComputeAABB(
    const std::string& path,
    std::vector<MapVertex>& outVertices,
    std::vector<MapTriangle>& outTriangles,
    AABB& outAABB)
{
    if (!LoadOBJ(path, outVertices, outTriangles))
        return false;

    // AABB √ ±‚»≠
    outAABB.minX = outAABB.minY = outAABB.minZ = FLT_MAX;
    outAABB.maxX = outAABB.maxY = outAABB.maxZ = -FLT_MAX;

    for (const auto& v : outVertices)
    {
        outAABB.minX = std::min(outAABB.minX, v.x);
        outAABB.minY = std::min(outAABB.minY, v.y);
        outAABB.minZ = std::min(outAABB.minZ, v.z);

        outAABB.maxX = std::max(outAABB.maxX, v.x);
        outAABB.maxY = std::max(outAABB.maxY, v.y);
        outAABB.maxZ = std::max(outAABB.maxZ, v.z);
    }

    return true;
}

void BuildTriangles(
    const std::vector<MapVertex>& vertices,
    const std::vector<MapTriangle>& indices,
    std::vector<MapTri>& outTris)
{
    outTris.reserve(indices.size());

    for (const auto& t : indices)
    {
        MapTri tri;
        tri.a = vertices[t.v0];
        tri.b = vertices[t.v1];
        tri.c = vertices[t.v2];
        outTris.push_back(tri);
    }
}

void BuildTriangleAABBs(
    const std::vector<MapTri>& tris,
    std::vector<AABB>& outAABBs)
{
    outAABBs.clear();
    outAABBs.reserve(tris.size());

    for (const auto& t : tris)
    {
        AABB aabb;
        aabb.minX = std::min({ t.a.x, t.b.x, t.c.x });
        aabb.minY = std::min({ t.a.y, t.b.y, t.c.y });
        aabb.minZ = std::min({ t.a.z, t.b.z, t.c.z });

        aabb.maxX = std::max({ t.a.x, t.b.x, t.c.x });
        aabb.maxY = std::max({ t.a.y, t.b.y, t.c.y });
        aabb.maxZ = std::max({ t.a.z, t.b.z, t.c.z });

        outAABBs.push_back(aabb);
    }
}

void BuildSpatialGrid(
    const std::vector<AABB>& triAABBs, const AABB& mapAABB,
    float cellSize, SpatialGrid& outGrid)
{
    outGrid.clear();

    for (int i = 0; i < (int)triAABBs.size(); ++i)
    {
        const AABB& a = triAABBs[i];

        int minX = static_cast<int>(std::floor((a.minX - mapAABB.minX) / cellSize));
        int maxX = static_cast<int>(std::floor((a.maxX - mapAABB.minX) / cellSize));
        int minZ = static_cast<int>(std::floor((a.minZ - mapAABB.minZ) / cellSize));
        int maxZ = static_cast<int>(std::floor((a.maxZ - mapAABB.minZ) / cellSize));

        for (int x = minX; x <= maxX; ++x)
        {
            for (int z = minZ; z <= maxZ; ++z)
            {
                outGrid[{x, z}].push_back(i);
            }
        }
    }
}

static bool RayAABB(const Ray& r, const AABB& a, float maxDist)
{
    float tmin = 0.0f, tmax = maxDist;

    auto slab = [&](float o, float d, float minv, float maxv) {
        if (std::abs(d) < 1e-6f) return (o >= minv && o <= maxv);
        float inv = 1.0f / d;
        float t1 = (minv - o) * inv;
        float t2 = (maxv - o) * inv;
        if (t1 > t2) std::swap(t1, t2);
        tmin = std::max(tmin, t1);
        tmax = std::min(tmax, t2);
        return tmin <= tmax;
        };

    return slab(r.ox, r.dx, a.minX, a.maxX) &&
        slab(r.oy, r.dy, a.minY, a.maxY) &&
        slab(r.oz, r.dz, a.minZ, a.maxZ);
}

static bool RayTri(const Ray& r, const MapTri& t, float maxDist, float& outT)
{
    const float EPS = 1e-6f;

    float ax = t.a.x, ay = t.a.y, az = t.a.z;
    float bx = t.b.x, by = t.b.y, bz = t.b.z;
    float cx = t.c.x, cy = t.c.y, cz = t.c.z;

    float e1x = bx - ax, e1y = by - ay, e1z = bz - az;
    float e2x = cx - ax, e2y = cy - ay, e2z = cz - az;

    float px = r.dy * e2z - r.dz * e2y;
    float py = r.dz * e2x - r.dx * e2z;
    float pz = r.dx * e2y - r.dy * e2x;

    float det = e1x * px + e1y * py + e1z * pz;
    if (std::abs(det) < EPS) return false;

    float inv = 1.0f / det;
    float tx = r.ox - ax, ty = r.oy - ay, tz = r.oz - az;

    float u = (tx * px + ty * py + tz * pz) * inv;
    if (u < 0 || u > 1) return false;

    float qx = ty * e1z - tz * e1y;
    float qy = tz * e1x - tx * e1z;
    float qz = tx * e1y - ty * e1x;

    float v = (r.dx * qx + r.dy * qy + r.dz * qz) * inv;
    if (v < 0 || u + v > 1) return false;

    float tHit = (e2x * qx + e2y * qy + e2z * qz) * inv;
    if (tHit > EPS && tHit <= maxDist) {
        outT = tHit;
        return true;
    }
    return false;
}

bool LineTraceMap(
    const Ray& ray, float maxDist,
    const std::vector<MapTri>& tris,
    const std::vector<AABB>& triAABBs,
    const SpatialGrid& grid, const AABB& mapAABB,
    float cellSize, float& outHitDist, int& outTriIndex)
{
    outHitDist = maxDist;
    outTriIndex = -1;

    int minX = (int)std::floor((std::min(ray.ox, ray.ox + ray.dx * maxDist) - mapAABB.minX) / cellSize);
    int maxX = (int)std::floor((std::max(ray.ox, ray.ox + ray.dx * maxDist) - mapAABB.minX) / cellSize);
    int minZ = (int)std::floor((std::min(ray.oz, ray.oz + ray.dz * maxDist) - mapAABB.minZ) / cellSize);
    int maxZ = (int)std::floor((std::max(ray.oz, ray.oz + ray.dz * maxDist) - mapAABB.minZ) / cellSize);

    for (int x = minX; x <= maxX; ++x)
        for (int z = minZ; z <= maxZ; ++z)
        {
            auto it = grid.find({ x,z });
            if (it == grid.end()) continue;

            for (int triIdx : it->second)
            {
                if (!RayAABB(ray, triAABBs[triIdx], outHitDist))
                    continue;

                float tHit;
                if (RayTri(ray, tris[triIdx], outHitDist, tHit))
                {
                    outHitDist = tHit;
                    outTriIndex = triIdx;
                }
            }
        }
    return outTriIndex >= 0;
}
