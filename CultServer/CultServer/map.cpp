#include "map.h"
#include <algorithm>
#include <cmath>

bool MAP::Load(const std::string& objPath, const Vec3& MapOffset)
{
    offset = MapOffset;
    vertices.clear();
    triangles.clear();
    tris.clear();
    triAABBs.clear();
    grid.clear();

    if (!LoadOBJAndComputeAABB(objPath, vertices, triangles, worldAABB))
        return false;

    BuildTriangles();
    BuildTriangleAABBs();
    BuildSpatialGrid();

    return true;
}

bool MAP::LoadOBJ(const std::string& path,
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
            float ox, oy, oz;
            ss >> ox >> oy >> oz;
            MapVertex v{};
            // OBJ: X,Z = 평면 / Y = 높이
            // UE : X,Y = 평면 / Z = 높이
            v.x = ox;
            v.y = oz;
            v.z = oy;
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

bool MAP::LoadOBJAndComputeAABB(
    const std::string& path,
    std::vector<MapVertex>& outVertices,
    std::vector<MapTriangle>& outTriangles,
    AABB& outAABB)
{
    if (!LoadOBJ(path, outVertices, outTriangles))
        return false;

    // AABB 초기화
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

void MAP::BuildTriangles()
{
    tris.clear();
    tris.reserve(triangles.size());

    for (const auto& t : triangles)
    {
        MapTri tri;
        tri.a = vertices[t.v0];
        tri.b = vertices[t.v1];
        tri.c = vertices[t.v2];
        tris.push_back(tri);
    }
}

void MAP::BuildTriangleAABBs()
{
    triAABBs.clear();
    triAABBs.reserve(tris.size());

    for (const auto& t : tris)
    {
        AABB aabb;
        aabb.minX = std::min({ t.a.x, t.b.x, t.c.x });
        aabb.minY = std::min({ t.a.y, t.b.y, t.c.y });
        aabb.minZ = std::min({ t.a.z, t.b.z, t.c.z });

        aabb.maxX = std::max({ t.a.x, t.b.x, t.c.x });
        aabb.maxY = std::max({ t.a.y, t.b.y, t.c.y });
        aabb.maxZ = std::max({ t.a.z, t.b.z, t.c.z });

        triAABBs.push_back(aabb);
    }
}

void MAP::BuildSpatialGrid()
{
    grid.clear();

    for (int i = 0; i < (int)triAABBs.size(); ++i)
    {
        const AABB& a = triAABBs[i];

        const int minX = (int)std::floor((a.minX - worldAABB.minX) / cellSize);
        const int maxX = (int)std::floor((a.maxX - worldAABB.minX) / cellSize);

        const int minZ = (int)std::floor((a.minZ - worldAABB.minZ) / cellSize);
        const int maxZ = (int)std::floor((a.maxZ - worldAABB.minZ) / cellSize);

        for (int x = minX; x <= maxX; ++x)
        {
            for (int z = minZ; z <= maxZ; ++z)
            {
                grid[{ x, z }].push_back(i);
            }
        }
    }
}

static bool RayAABB_XY(const Ray& r, const AABB& a, float maxDist)
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

    return slab(r.start.x, r.dir.x, a.minX, a.maxX) &&
        slab(r.start.y, r.dir.y, a.minY, a.maxY);
}

static bool RayTri(const Ray& r, const MapTri& t, float maxDist, float& outT)
{
    const float EPS = 1e-6f;

    float ax = t.a.x, ay = t.a.y, az = t.a.z;
    float bx = t.b.x, by = t.b.y, bz = t.b.z;
    float cx = t.c.x, cy = t.c.y, cz = t.c.z;

    float e1x = bx - ax, e1y = by - ay, e1z = bz - az;
    float e2x = cx - ax, e2y = cy - ay, e2z = cz - az;

    float px = r.dir.y * e2z - r.dir.z * e2y;
    float py = r.dir.z * e2x - r.dir.x * e2z;
    float pz = r.dir.x * e2y - r.dir.y * e2x;

    float det = e1x * px + e1y * py + e1z * pz;
    if (std::abs(det) < EPS) return false;

    float inv = 1.0f / det;
    float tx = r.start.x - ax, ty = r.start.y - ay, tz = r.start.z - az;

    float u = (tx * px + ty * py + tz * pz) * inv;
    if (u < 0 || u > 1) return false;

    float qx = ty * e1z - tz * e1y;
    float qy = tz * e1x - tx * e1z;
    float qz = tx * e1y - ty * e1x;

    float v = (r.dir.x * qx + r.dir.y * qy + r.dir.z * qz) * inv;
    if (v < 0 || u + v > 1) return false;

    float tHit = (e2x * qx + e2y * qy + e2z * qz) * inv;
    if (tHit > EPS && tHit <= maxDist) {
        outT = tHit;
        return true;
    }
    return false;
}

bool MAP::LineTrace(const Ray& worldRay, float maxDist, float& hitDist, int& hitTriIndex) const
{
    const Ray ray = ToLocalRay(worldRay);

    hitDist = maxDist;
    hitTriIndex = -1;

    const float endX = ray.start.x + ray.dir.x * maxDist;
    const float endZ = ray.start.z + ray.dir.z * maxDist;

    const int minCellX = (int)std::floor((std::min(ray.start.x, endX) - worldAABB.minX) / cellSize);
    const int maxCellX = (int)std::floor((std::max(ray.start.x, endX) - worldAABB.minX) / cellSize);

    const int minCellZ = (int)std::floor((std::min(ray.start.z, endZ) - worldAABB.minZ) / cellSize);
    const int maxCellZ = (int)std::floor((std::max(ray.start.z, endZ) - worldAABB.minZ) / cellSize);

    for (int x = minCellX; x <= maxCellX; ++x)
    {
        for (int z = minCellZ; z <= maxCellZ; ++z)
        {
            auto it = grid.find({ x, z });
            if (it == grid.end())
                continue;

            for (int triIdx : it->second)
            {
                if (!RayAABB_XY(ray, triAABBs[triIdx], hitDist))
                    continue;

                float tHit = 0.f;
                if (RayTri(ray, tris[triIdx], hitDist, tHit))
                {
                    hitDist = tHit;
                    hitTriIndex = triIdx;
                }
            }
        }
    }

    return hitTriIndex >= 0;
}

Ray MAP::ToLocalRay(const Ray& worldRay) const
{
    Ray local;
    local.start.x = worldRay.start.x - offset.x;
    local.start.y = worldRay.start.y - offset.y;
    local.start.z = worldRay.start.z - offset.z;
    local.dir = worldRay.dir;

    return local;
}