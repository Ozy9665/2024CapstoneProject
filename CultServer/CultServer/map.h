#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include "Protocol.h"

// Map
struct NavNode {
    int x, y;          // Grid ÁÂÇ¥ (XY Æò¸é)
    float g, h;        // cost
    int parent;        // index
};

struct NodeCompare
{
    const std::vector<NavNode>* nodes;

    bool operator()(int a, int b) const
    {
        return (*nodes)[a].g + (*nodes)[a].h >
            (*nodes)[b].g + (*nodes)[b].h;
    }
};

class MAP {
public:
    virtual bool Load(const std::string&, const Vec3&);

    bool LineTrace(
        const Ray& worldRay,
        float maxDist,
        float& hitDist,
        int& hitTriIndex
    ) const;

protected:
    struct CellKey {
        int x, z;
        bool operator==(const CellKey& o) const {
            return x == o.x && z == o.z;
        }
    };

    struct CellKeyHash {
        size_t operator()(const CellKey& k) const {
            return (static_cast<size_t>(k.x) << 32)
                ^ static_cast<size_t>(k.z);
        }
    };

    using SpatialGrid =
        std::unordered_map<CellKey, std::vector<int>, CellKeyHash>;

    std::vector<MapVertex> vertices;
    std::vector<MapTriangle> triangles;
    std::vector<MapTri> tris;

    std::vector<AABB> triAABBs;
    SpatialGrid grid;
    AABB worldAABB;
    float cellSize{ 100.f };
    Vec3 offset;

    bool LoadOBJ(const std::string&,
        std::vector<MapVertex>&,
        std::vector<MapTriangle>&
    );

    bool LoadOBJAndComputeAABB(
        const std::string&,
        std::vector<MapVertex>&,
        std::vector<MapTriangle>&,
        AABB&
    );

    void BuildTriangles();
    void BuildTriangleAABBs();
    void BuildSpatialGrid();
    Ray ToLocalRay(const Ray& worldRay) const;
    bool CanMove(const Vec3&, const Vec3&) const;
    int WorldToGridX(float x) const;
    int WorldToGridY(float y) const;
    Vec3 GridToWorld(int gx, int gy) const;
};

// NevMesh

struct QV {
    int x, y, z;
    bool operator==(const QV& o) const {
        return x == o.x && y == o.y && z == o.z;
    }
};

struct QVHash {
    size_t operator()(const QV& v) const {
        return (static_cast<size_t>(v.x) * 73856093) ^
            (static_cast<size_t>(v.y) * 19349663) ^
            (static_cast<size_t>(v.z) * 83492791);
    }
};

struct EdgeKey {
    int a, b; // Ç×»ó a < b

    bool operator==(const EdgeKey& o) const {
        return a == o.a && b == o.b;
    }
};

struct EdgeKeyHash {
    size_t operator()(const EdgeKey& e) const {
        return (static_cast<size_t>(e.a) << 32) ^ static_cast<size_t>(e.b);
    }
};

struct TriNode {
    int tri;
    float g, f;
    int parent;
};

struct TriNodeCompare
{
    const std::vector<TriNode>* nodes;

    bool operator()(int a, int b) const
    {
        return (*nodes)[a].f > (*nodes)[b].f;
    }
};

struct Portal
{
    Vec3 left;
    Vec3 right;
};

struct Triangle
{
    Vec3 v0, v1, v2;
    int neighbors[3];  // °¢ edgeÀÇ ÀÌ¿ô »ï°¢Çü ÀÎµ¦½º
};

class NAVMESH : public MAP {
public:
    bool Load(const std::string&, const Vec3&) override;
    bool FindTriPath(const Vec3&, const Vec3&, std::vector<int>&);
    Vec3 GetTriCenter(int) const;
    void BuildPortals(const std::vector<int>& triPath,
        std::vector<std::pair<Vec3, Vec3>>& portals);
    bool SmoothPath(const Vec3&, const Vec3&,
        const std::vector<std::pair<Vec3, Vec3>>&,
        std::vector<Vec3>&) const;
    static bool PointInTri2D(const Vec3&, const MapTri&);
    int FindContainingTriangle(const Vec3&) const;
    bool ClipSegmentToTriangle(const Vec3& from,
        const Vec3& to, int triIdx, Vec3& out) const;
    const MapTri& GetTri(int) const;
    int GetRandomTriangle(int, int) const;

private:
    bool LoadNavOBJ(const std::string&,
        std::vector<MapVertex>&,
        std::vector<MapTriangle>&);

    bool LoadOBJAndComputeAABB_Nav(const std::string&,
        std::vector<MapVertex>&, std::vector<MapTriangle>&, AABB&);

    void WeldVertices(std::vector<MapVertex>&, std::vector<MapTriangle>&);

    std::vector<std::array<int, 3>> triNeighbors;
    std::vector<Vec3> triCenters;
    std::vector<Portal> portals;

    void BuildAdjacency();

    void BuildTriCenters();

    bool GetSharedEdge(int, int, Vec3&, Vec3&) const;

    float TriHeightAtXY(int, float, float) const;

};

constexpr float EPS = 0.001f; // 1mm
constexpr float SNAP_MAX = 80.f;
constexpr float snapMax2 = SNAP_MAX * SNAP_MAX;
