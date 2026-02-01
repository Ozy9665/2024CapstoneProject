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
    bool FindPath(const Vec3&, const Vec3&, std::vector<Vec3>&) const;
    void SmoothPath(std::vector<Vec3>&) const;

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

class NAVMESH : public MAP {
public:
    bool Load(const std::string&, const Vec3&) override;

private:
    bool LoadNavOBJ(const std::string&,
        std::vector<MapVertex>&,
        std::vector<MapTriangle>&
    );

    bool LoadOBJAndComputeAABB_Nav(
        const std::string&,
        std::vector<MapVertex>&,
        std::vector<MapTriangle>&,
        AABB&
    );


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
    float EPS = 0.001f; // 1mm

    void WeldVertices(std::vector<MapVertex>&, std::vector<MapTriangle>&);
};