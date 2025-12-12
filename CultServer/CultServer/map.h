#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

struct MapVertex {
    float x, y, z;
};

struct MapTriangle {
    int v0, v1, v2;
};

bool LoadOBJ(const std::string&,
    std::vector<MapVertex>&,
    std::vector<MapTriangle>&);


struct MapAABB {
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
};


bool LoadOBJAndComputeAABB(
    const std::string& path,
    std::vector<MapVertex>&,
    std::vector<MapTriangle>&,
    MapAABB&
);

struct MapTri {
    MapVertex a, b, c;
};

void BuildTriangles(
    const std::vector<MapVertex>&,
    const std::vector<MapTriangle>&,
    std::vector<MapTri>&
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