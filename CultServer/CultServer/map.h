#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_map>

struct MapVertex {
    float x, y, z;
};

struct MapTriangle {
    int v0, v1, v2;
};

bool LoadOBJ(const std::string&,
    std::vector<MapVertex>&,
    std::vector<MapTriangle>&);


struct AABB {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
};


bool LoadOBJAndComputeAABB(
    const std::string& path,
    std::vector<MapVertex>&,
    std::vector<MapTriangle>&,
    AABB&
);

struct MapTri {
    MapVertex a, b, c;
};

void BuildTriangles(
    const std::vector<MapVertex>&,
    const std::vector<MapTriangle>&,
    std::vector<MapTri>&
);

void BuildTriangleAABBs(
    const std::vector<MapTri>&,
    std::vector<AABB>&
);

// XZ ±‚¡ÿ ºø ≈∞
struct CellKey {
    int x, z;
    bool operator==(const CellKey& o) const {
        return x == o.x && z == o.z;
    }
};

struct CellKeyHash {
    size_t operator()(const CellKey& k) const {
        return (static_cast<size_t>(k.x) << 32) ^ static_cast<size_t>(k.z);
    }
};

using SpatialGrid = std::unordered_map<CellKey, std::vector<int>, CellKeyHash>;

void BuildSpatialGrid(
    const std::vector<AABB>&,
    const AABB&,
    float,
    SpatialGrid&
);

struct Vec3 {
    float x, y, z;
};

struct Ray {
    Vec3 start;
    Vec3 dir;
};

struct Transform {
    Vec3 location;
    Vec3 scale;
};

bool LineTraceMap(
    const Ray&,
    float,
    const std::vector<MapTri>&,
    const std::vector<AABB>&,
    const SpatialGrid&,
    const AABB&,
    float,
    float&,
    int&
);

Vec3 WorldToLocalPoint(const Vec3&, const Transform&);

Vec3 WorldToLocalDir(const Vec3&, const Transform&);

Ray WorldToLocalRay(const Ray&, const Transform&);