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

// XZ 기준 셀 키
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

struct Ray {
    float ox, oy, oz;
    float dx, dy, dz;   // direction
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

/*

할 일

OBJ 로드 -> v, f 파싱
전체 정점으로 맵 AABB 계산

삼각형 생성 (v + f)

삼각형별 AABB 계산

셀 크기 결정

맵 AABB 기준으로 Grid 생성

삼각형 AABB를 겹치는 셀에 등록

line_trace / sweep 시 셀 조회

Ray / Sweep vs 삼각형 충돌 검사

최종 히트 판정

*/