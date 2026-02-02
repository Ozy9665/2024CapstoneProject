#define NOMINMAX

#include "map.h"
#include <algorithm>
#include <cmath>
#include <queue>

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

    std::cout << "cellSize = " << cellSize << std::endl;
    std::cout
        << "verts: " << vertices.size()
        << " tris: " << triangles.size()
        << " grid: " << grid.size()
        << std::endl;
    std::cout
        << "AABB X: " << worldAABB.minX << " ~ " << worldAABB.maxX
        << " Y: " << worldAABB.minY << " ~ " << worldAABB.maxY
        << std::endl;

    Vec3 a{ -10219.0, 2560.0, -3009.0 };
    Vec3 b{ -10000, 2000, -3000.0 };

    std::cout << "CanMove: " << CanMove(a, b) << std::endl;

    std::vector<Vec3> path;
    bool ok = FindPath(a, b, path);

    std::cout << "path ok: " << ok
        << " count: " << path.size()
        << std::endl;

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
    std::cout
        << "[OBJ AABB] "
        << "X(" << outAABB.minX << " ~ " << outAABB.maxX << ") "
        << "Y(" << outAABB.minY << " ~ " << outAABB.maxY << ") "
        << "Z(" << outAABB.minZ << " ~ " << outAABB.maxZ << ")\n";

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

bool MAP::CanMove(const Vec3& from, const Vec3& to) const
{
    Ray ray;
    ray.start = from;

    Vec3 d{
        to.x - from.x,
        to.y - from.y,
        0.f // 높이 무시
    };

    float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-3f)
        return true;

    ray.dir.x = d.x / len;
    ray.dir.y = d.y / len;
    ray.dir.z = 0.f;

    float hitDist;
    int hitTri;

    return !LineTrace(ray, len, hitDist, hitTri);
}

int MAP::WorldToGridX(float x) const
{
    return (int)std::floor((x - worldAABB.minX) / cellSize);
}

int MAP::WorldToGridY(float y) const
{
    return (int)std::floor((y - worldAABB.minY) / cellSize);
}

Vec3 MAP::GridToWorld(int gx, int gy) const
{
    Vec3 w;
    w.x = worldAABB.minX + (gx + 0.5f) * cellSize;
    w.y = worldAABB.minY + (gy + 0.5f) * cellSize;
    w.z = offset.z; // 고정 높이
    return w;
}

static const int DIRS[4][2] = {
    { 1, 0 }, { -1, 0 },
    { 0, 1 }, { 0, -1 }
};

bool MAP::FindPath(
    const Vec3& start, const Vec3& goal,
    std::vector<Vec3>& outPath) const
{
    outPath.clear();

    int sx = WorldToGridX(start.x);
    int sy = WorldToGridY(start.y);
    int gx = WorldToGridX(goal.x);
    int gy = WorldToGridY(goal.y);

    std::vector<NavNode> nodes;
    NodeCompare comp{ &nodes };
    std::priority_queue<int, std::vector<int>, NodeCompare> open(comp);
    std::unordered_map<long long, int> visited;

    auto key = [](int x, int y) {
        return (static_cast<long long>(x) << 32) | (unsigned)y;
        };

    nodes.push_back({ sx, sy, 0.f, static_cast<float>(std::abs(gx - sx) + std::abs(gy - sy)), -1 });
    open.push(0);
    // visited[key(sx, sy)] = 0;

    int goalIndex = -1;

    while (!open.empty())
    {
        int curIdx = open.top();
        open.pop();

        auto& cur = nodes[curIdx];

        long long ck = key(cur.x, cur.y);
        if (visited.count(ck))
            continue;

        visited[ck] = curIdx;

        if (cur.x == gx && cur.y == gy) {
            goalIndex = curIdx;
            break;
        }

        Vec3 curWorld = GridToWorld(cur.x, cur.y);

        for (auto& d : DIRS)
        {
            int nx = cur.x + d[0];
            int ny = cur.y + d[1];

            long long k = key(nx, ny);
            if (visited.count(k))
                continue;

            Vec3 nextWorld = GridToWorld(nx, ny);

            if (!CanMove(curWorld, nextWorld))
                continue;

            float g = cur.g + cellSize;
            float h = static_cast<float>(std::abs(gx - nx) + std::abs(gy - ny));

            int idx = (int)nodes.size();
            nodes.push_back({ nx, ny, g, h, curIdx });
            //visited[k] = idx;
            open.push(idx);
        }
    }

    if (goalIndex < 0)
        return false;

    // 경로 복원
    for (int i = goalIndex; i >= 0; i = nodes[i].parent)
        outPath.push_back(GridToWorld(nodes[i].x, nodes[i].y));

    std::reverse(outPath.begin(), outPath.end());
    return true;
}

void MAP::SmoothPath(std::vector<Vec3>& path) const
{
    if (path.size() < 3)
        return;

    std::vector<Vec3> smoothed;
    smoothed.reserve(path.size());

    size_t i = 0;
    smoothed.push_back(path[0]);

    while (i < path.size() - 1)
    {
        size_t farthest = i + 1;

        // i에서 시작해서 최대한 멀리 직선 이동 가능한 노드 찾기
        for (size_t j = i + 1; j < path.size(); ++j)
        {
            if (CanMove(path[i], path[j]))
            {
                farthest = j;
            }
            else
            {
                break;
            }
        }

        smoothed.push_back(path[farthest]);
        i = farthest;
    }

    path.swap(smoothed);
}

// NevMesh
bool NAVMESH::Load(const std::string& objPath, const Vec3& MapOffset)
{
    offset = MapOffset;
    vertices.clear();
    triangles.clear();
    tris.clear();
    triAABBs.clear();
    grid.clear();

    if (!LoadOBJAndComputeAABB_Nav(objPath, vertices, triangles, worldAABB))
        return false;
    
    WeldVertices(vertices, triangles);
    BuildAdjacency();
    BuildTriCenters();

    BuildTriangles();
    BuildTriangleAABBs();
    BuildSpatialGrid();

    Vec3 a{ -10219.0, 2560.0, -3009.0 };
    Vec3 b{ -5000, 2000, -3000.0 };

    std::vector<int> triPath;
    bool ok = FindTriPath(a, b, triPath);

    std::cout
        << "[TRI A*] ok=" << ok
        << " len=" << triPath.size()
        << std::endl;

    return true;
}

bool NAVMESH::LoadNavOBJ(const std::string& path,
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
            v.x = ox;
            v.y = oy;
            v.z = oz;
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

bool NAVMESH::LoadOBJAndComputeAABB_Nav(
    const std::string& path,
    std::vector<MapVertex>& outVertices,
    std::vector<MapTriangle>& outTriangles,
    AABB& outAABB)
{
    if (!LoadNavOBJ(path, outVertices, outTriangles))
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

void NAVMESH::WeldVertices(
    std::vector<MapVertex>& vertices,
    std::vector<MapTriangle>& triangles)
{
    std::unordered_map<QV, int, QVHash> map;
    std::vector<MapVertex> newVerts;
    newVerts.reserve(vertices.size());

    // old index → new index
    std::vector<int> remap(vertices.size(), -1);

    for (int i = 0; i < (int)vertices.size(); ++i)
    {
        const auto& v = vertices[i];

        QV q{
            (int)std::round(v.x / EPS),
            (int)std::round(v.y / EPS),
            (int)std::round(v.z / EPS)
        };

        auto it = map.find(q);
        if (it == map.end())
        {
            int newIdx = (int)newVerts.size();
            map[q] = newIdx;
            remap[i] = newIdx;
            newVerts.push_back(v);
        }
        else
        {
            remap[i] = it->second;
        }
    }

    // triangle index remap
    for (auto& t : triangles)
    {
        t.v0 = remap[t.v0];
        t.v1 = remap[t.v1];
        t.v2 = remap[t.v2];
    }

    vertices.swap(newVerts);
}

void NAVMESH::BuildAdjacency()
{
    triNeighbors.clear();
    triNeighbors.resize(triangles.size(), { -1, -1, -1 });

    std::unordered_map<EdgeKey, int, EdgeKeyHash> edgeOwner;

    for (int ti = 0; ti < (int)triangles.size(); ++ti)
    {
        const auto& t = triangles[ti];
        int vs[3] = { t.v0, t.v1, t.v2 };

        for (int e = 0; e < 3; ++e)
        {
            int v0 = vs[e];
            int v1 = vs[(e + 1) % 3];
            EdgeKey key = MakeEdge(v0, v1);

            auto it = edgeOwner.find(key);
            if (it == edgeOwner.end())
            {
                edgeOwner[key] = ti;
            }
            else
            {
                int other = it->second;

                // 서로 이웃으로 등록
                for (int k = 0; k < 3; ++k)
                {
                    if (triNeighbors[ti][k] == -1) {
                        triNeighbors[ti][k] = other;
                        break;
                    }
                }
                for (int k = 0; k < 3; ++k)
                {
                    if (triNeighbors[other][k] == -1) {
                        triNeighbors[other][k] = ti;
                        break;
                    }
                }
            }
        }
    }
}

void NAVMESH::BuildTriCenters()
{
    triCenters.resize(triangles.size());

    for (int i = 0; i < (int)triangles.size(); ++i)
    {
        const auto& t = triangles[i];
        const auto& a = vertices[t.v0];
        const auto& b = vertices[t.v1];
        const auto& c = vertices[t.v2];

        triCenters[i] = {
            (a.x + b.x + c.x) / 3.f,
            (a.y + b.y + c.y) / 3.f,
            (a.z + b.z + c.z) / 3.f
        };
    }
}

static bool PointInTri2D(const Vec3& p, const MapTri& t)
{
    auto sign = [](const Vec3& p1, const Vec3& p2, const Vec3& p3)
        {
            return (p1.x - p3.x) * (p2.y - p3.y)
                - (p2.x - p3.x) * (p1.y - p3.y);
        };

    Vec3 a{ t.a.x, t.a.y, 0.f };
    Vec3 b{ t.b.x, t.b.y, 0.f };
    Vec3 c{ t.c.x, t.c.y, 0.f };

    bool b1 = sign(p, a, b) < 0.f;
    bool b2 = sign(p, b, c) < 0.f;
    bool b3 = sign(p, c, a) < 0.f;

    return (b1 == b2) && (b2 == b3);
}

int NAVMESH::FindContainingTriangle(const Vec3& pos) const
{
    for (int i = 0; i < (int)tris.size(); ++i)
    {
        if (PointInTri2D(pos, tris[i]))
            return i;
    }
    return -1;
}

bool NAVMESH::FindTriPath(
    const Vec3& start,
    const Vec3& goal,
    std::vector<int>& outTriPath)
{
    outTriPath.clear();

    int startTri = FindContainingTriangle(start);
    int goalTri = FindContainingTriangle(goal);

    if (startTri < 0 || goalTri < 0)
        return false;

    std::vector<TriNode> nodes;
    TriNodeCompare comp{ &nodes };
    std::priority_queue<int, std::vector<int>, TriNodeCompare> open(comp);
    std::unordered_map<int, int> visited;

    auto heuristic = [&](int t) {
        Vec3 d{
            triCenters[t].x - goal.x,
            triCenters[t].y - goal.y,
            0.f
        };
        return std::sqrt(d.x * d.x + d.y * d.y);
        };

    nodes.push_back({ startTri, 0.f, heuristic(startTri), -1 });
    open.push(0);

    int goalIdx = -1;

    while (!open.empty())
    {
        int curIdx = open.top();
        open.pop();

        auto& cur = nodes[curIdx];
        if (visited.count(cur.tri))
            continue;

        visited[cur.tri] = curIdx;

        if (cur.tri == goalTri)
        {
            goalIdx = curIdx;
            break;
        }

        for (int nb : triNeighbors[cur.tri])
        {
            if (nb < 0 || visited.count(nb))
                continue;

            Vec3 d{
                triCenters[nb].x - triCenters[cur.tri].x,
                triCenters[nb].y - triCenters[cur.tri].y,
                0.f
            };
            float dist = std::sqrt(d.x * d.x + d.y * d.y);

            float g = cur.g + dist;
            float f = g + heuristic(nb);

            int idx = (int)nodes.size();
            nodes.push_back({ nb, g, f, curIdx });
            open.push(idx);
        }
    }

    if (goalIdx < 0)
        return false;

    for (int i = goalIdx; i >= 0; i = nodes[i].parent)
        outTriPath.push_back(nodes[i].tri);

    std::reverse(outTriPath.begin(), outTriPath.end());
    return true;
}

