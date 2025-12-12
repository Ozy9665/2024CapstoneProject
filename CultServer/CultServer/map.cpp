#include "map.h"

bool LoadOBJ(const std::string& path,
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
            MapVertex v{};
            ss >> v.x >> v.y >> v.z;
            outVertices.push_back(v);
        }
        else if (type == "f")
        {
            MapTriangle t{};
            char slash;

            ss >> t.v0 >> t.v1 >> t.v2;

            t.v0--; t.v1--; t.v2--;

            outTriangles.push_back(t);
        }
    }
    return true;
}
