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

class MAP {
public:
    bool Load(const std::string&, const Vec3&);

    bool LineTrace(
        const Ray& worldRay,
        float maxDist,
        float& hitDist,
        int& hitTriIndex
    ) const;

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
};
