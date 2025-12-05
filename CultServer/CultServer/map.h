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
