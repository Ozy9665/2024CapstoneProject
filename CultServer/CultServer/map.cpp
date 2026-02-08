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

static EdgeKey MakeEdge(int v0, int v1)
{
    return (v0 < v1) ? EdgeKey{ v0, v1 } : EdgeKey{ v1, v0 };
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

bool NAVMESH::GetSharedEdge(int t0, int t1, Vec3& outA, Vec3& outB) const
{
    const MapTriangle& A = triangles[t0];
    const MapTriangle& B = triangles[t1];

    int a[3] = { A.v0, A.v1, A.v2 };
    int b[3] = { B.v0, B.v1, B.v2 };

    int shared[2];
    int count = 0;

    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (a[i] == b[j])
                shared[count++] = a[i];

    if (count != 2)
        return false;

    outA = Vec3{
        vertices[shared[0]].x,
        vertices[shared[0]].y,
        vertices[shared[0]].z
    };

    outB = Vec3{
        vertices[shared[1]].x,
        vertices[shared[1]].y,
        vertices[shared[1]].z
    };
    return true;
}

bool NAVMESH::PointInTri2D(const Vec3& p, const MapTri& t)
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

static float Dist2_PointSeg2D(const Vec3& p, const Vec3& a, const Vec3& b)
{
    const float abx = b.x - a.x;
    const float aby = b.y - a.y;
    const float apx = p.x - a.x;
    const float apy = p.y - a.y;

    const float ab2 = abx * abx + aby * aby;
    if (ab2 < 1e-8f) {
        const float dx = p.x - a.x;
        const float dy = p.y - a.y;
        return dx * dx + dy * dy;
    }

    float t = (apx * abx + apy * aby) / ab2;
    t = std::max(0.f, std::min(1.f, t));

    const float cx = a.x + abx * t;
    const float cy = a.y + aby * t;

    const float dx = p.x - cx;
    const float dy = p.y - cy;
    return dx * dx + dy * dy;
}

static float Dist2_PointTri2D(const Vec3& p, const MapTri& t)
{
    const Vec3 a{ t.a.x, t.a.y, 0.f };
    const Vec3 b{ t.b.x, t.b.y, 0.f };
    const Vec3 c{ t.c.x, t.c.y, 0.f };

    // 이미 안에 들어가면 0
    if (NAVMESH::PointInTri2D(p, t))
        return 0.f;

    const float d0 = Dist2_PointSeg2D(p, a, b);
    const float d1 = Dist2_PointSeg2D(p, b, c);
    const float d2 = Dist2_PointSeg2D(p, c, a);
    return std::min(d0, std::min(d1, d2));
}

float NAVMESH::TriHeightAtXY(int triIdx, float x, float y) const
{
    const MapTri& t = tris[triIdx];

    const Vec3 a{ t.a.x, t.a.y, 0.f };
    const Vec3 b{ t.b.x, t.b.y, 0.f };
    const Vec3 c{ t.c.x, t.c.y, 0.f };
    const Vec3 p{ x,     y,     0.f };

    // 바리센트릭 계산
    float denom =
        (b.y - c.y) * (a.x - c.x) +
        (c.x - b.x) * (a.y - c.y);

    if (std::abs(denom) < 1e-6f)
        return t.a.z; // degenerate fallback

    float w1 =
        ((b.y - c.y) * (p.x - c.x) +
            (c.x - b.x) * (p.y - c.y)) / denom;

    float w2 =
        ((c.y - a.y) * (p.x - c.x) +
            (a.x - c.x) * (p.y - c.y)) / denom;

    float w3 = 1.f - w1 - w2;

    return
        w1 * t.a.z +
        w2 * t.b.z +
        w3 * t.c.z;
}

int NAVMESH::FindContainingTriangle(const Vec3& pos) const
{
    int best = -1;
    float bestDz = FLT_MAX;

    for (int i = 0; i < tris.size(); ++i)
    {
        if (!PointInTri2D(pos, tris[i]))
            continue;

        float z = TriHeightAtXY(i, pos.x, pos.y);
        float dz = std::abs(z - pos.z);

        if (dz < bestDz)
        {
            bestDz = dz;
            best = i;
        }
    }

    if (best >= 0)
        return best;

    int snap = -1;
    float bestD2 = FLT_MAX;

    for (int i = 0; i < (int)tris.size(); ++i)
    {
        const float d2 = Dist2_PointTri2D(pos, tris[i]);
        if (d2 < bestD2) { bestD2 = d2; snap = i; }
    }

    if (snap >= 0 && bestD2 <= snapMax2)
    {
        return snap;
    }

    return -1;
}

bool NAVMESH::FindTriPath(
    const Vec3& start, const Vec3& goal,
    std::vector<int>& outTriPath)
{
    outTriPath.clear();

    int startTri = FindContainingTriangle(start);
    int goalTri = FindContainingTriangle(goal);

    auto degree = [&](int t) {
        int c = 0;
        for (int nb : triNeighbors[t])
            if (nb >= 0) ++c;
        return c;
        };

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

    if (goalIdx < 0) {
        std::cout << "[TRI A* FAIL] explored=" << visited.size()
            << " startTri=" << startTri
            << " goalTri=" << goalTri << "\n";
        return false;
    }

    for (int i = goalIdx; i >= 0; i = nodes[i].parent)
        outTriPath.push_back(nodes[i].tri);

    std::reverse(outTriPath.begin(), outTriPath.end());
    return true;
}

Vec3 NAVMESH::GetTriCenter(int triIdx) const
{
    return triCenters[triIdx];
}

void NAVMESH::BuildPortals(
    const std::vector<int>& triPath,
    std::vector<std::pair<Vec3, Vec3>>& portals)
{
    portals.clear();

    for (size_t i = 0; i + 1 < triPath.size(); ++i)
    {
        Vec3 a, b;
        if (!GetSharedEdge(triPath[i], triPath[i + 1], a, b))
            continue;

        // 삼각형 진행 방향
        Vec3 from = GetTriCenter(triPath[i]);
        Vec3 to = GetTriCenter(triPath[i + 1]);
        Vec3 dir{
            to.x - from.x,
            to.y - from.y,
            0.f
        };

        // edge 방향
        Vec3 edge{
            b.x - a.x,
            b.y - a.y,
            0.f
        };

        // left/right 결정
        float cross = dir.x * edge.y - dir.y * edge.x;

        if (cross > 0.f)
        {
            // a가 left, b가 right
            portals.emplace_back(a, b);
        }
        else
        {
            // b가 left, a가 right
            portals.emplace_back(b, a);
        }
    }
}

bool NAVMESH::SmoothPath(
    const Vec3& start, const Vec3& goal,
    const std::vector<std::pair<Vec3, Vec3>>& portals,
    std::vector<Vec3>& outPath) const
{
    outPath.clear();
    if (portals.empty())
        return false;

    Vec3 apex = start;
    Vec3 left = portals[0].first;
    Vec3 right = portals[0].second;

    int apexIndex = 0;
    int leftIndex = 0;
    int rightIndex = 0;

    outPath.push_back(apex);

    auto triArea2 = [](const Vec3& a, const Vec3& b, const Vec3& c) {
        return (b.x - a.x) * (c.y - a.y)
            - (b.y - a.y) * (c.x - a.x);
        };

    for (int i = 1; i < (int)portals.size(); ++i)
    {
        const Vec3& newLeft = portals[i].first;
        const Vec3& newRight = portals[i].second;

        // 오
        if (triArea2(apex, right, newRight) <= 0.f)
        {
            if (apexIndex == rightIndex ||
                triArea2(apex, left, newRight) > 0.f)
            {
                right = newRight;
                rightIndex = i;
            }
            else
            {
                outPath.push_back(left);
                apex = left;
                apexIndex = leftIndex;

                left = apex;
                right = apex;
                leftIndex = apexIndex;
                rightIndex = apexIndex;
                i = apexIndex;
                continue;
            }
        }

        // 왼
        if (triArea2(apex, left, newLeft) >= 0.f)
        {
            if (apexIndex == leftIndex ||
                triArea2(apex, right, newLeft) < 0.f)
            {
                left = newLeft;
                leftIndex = i;
            }
            else
            {
                outPath.push_back(right);
                apex = right;
                apexIndex = rightIndex;

                left = apex;
                right = apex;
                leftIndex = apexIndex;
                rightIndex = apexIndex;
                i = apexIndex;
                continue;
            }
        }
    }

    outPath.push_back(goal);
    return outPath.size() >= 2;
}

bool NAVMESH::ClipSegmentToTriangle(
    const Vec3& from, const Vec3& to,
    int triIdx, Vec3& out) const
{
    const MapTri& t = tris[triIdx];
    const Vec3 A{ t.a.x, t.a.y, 0.f };
    const Vec3 B{ t.b.x, t.b.y, 0.f };
    const Vec3 C{ t.c.x, t.c.y, 0.f };

    float tMin = 1.0f;

    auto clipEdge = [&](const Vec3& a, const Vec3& b)
        {
            Vec3 edge{ b.x - a.x, b.y - a.y, 0.f };
            Vec3 normal{ edge.y, -edge.x, 0.f }; // outward

            float d0 = (from.x - a.x) * normal.x + (from.y - a.y) * normal.y;
            float d1 = (to.x - a.x) * normal.x + (to.y - a.y) * normal.y;

            if (d0 >= 0 && d1 >= 0) 
                return true;
            if (d0 < 0 && d1 < 0) 
                return false;

            float tHit = d0 / (d0 - d1);
            tMin = std::min(tMin, tHit);
            return true;
        };

    if (!clipEdge(A, B)) 
        return false;
    if (!clipEdge(B, C)) 
        return false;
    if (!clipEdge(C, A)) 
        return false;

    out.x = from.x + (to.x - from.x) * tMin;
    out.y = from.y + (to.y - from.y) * tMin;
    out.z = from.z;
    return true;
}

const MapTri& NAVMESH::GetTri(int idx) const
{ 
    return tris[idx]; 
}