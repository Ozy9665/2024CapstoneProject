#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_map>

// Map
enum MAPTYPE { LANDMASS };

struct MapVertex {
    float x, y, z;
};

struct MapTriangle {
    int v0, v1, v2;
};

struct AABB {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
};

struct MapTri {
    MapVertex a, b, c;
};

struct Vec3 {
    float x, y, z;
};

struct Ray {
    Vec3 start;
    Vec3 dir;
};

constexpr Vec3 NewmapLandmassOffset{ -4280.f, 13000.f, -3120.f };

struct NavNode {
    int x, y;          // Grid 좌표 (XY 평면)
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
    bool Load(const std::string&, const Vec3&);

    bool LineTrace(
        const Ray& worldRay,
        float maxDist,
        float& hitDist,
        int& hitTriIndex
    ) const;
    bool FindPath(const Vec3&, const Vec3&, std::vector<Vec3>&) const;

private:
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
    float cellSize{ 200.f };
    Vec3 offset;

    static bool LoadOBJ(const std::string&,
        std::vector<MapVertex>&,
        std::vector<MapTriangle>&);

    static bool LoadOBJAndComputeAABB(
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
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

struct NavTri {
    int v[3];        // vertex index
    Vec3 center;     // 삼각형 중심 (A* 노드 위치)
};

struct AStarNode {
    int tri;
    float g, f;
    int parent;
};

class NEVMESH {
public:
    bool LoadFBX(const std::string&, const Vec3&);
    void BuildAdjacency();
    bool FindPath(int, int, std::vector<Vec3>&) const;
    int FindNearestTri(const Vec3& ) const;

private:
    std::vector<Vec3> navVertices;
    std::vector<NavTri> navTris;
    std::vector<std::vector<int>> adjTris;
    Vec3 offset;
};