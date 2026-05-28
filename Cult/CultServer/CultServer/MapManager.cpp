#include "MapManager.h"

extern std::array<std::pair<Room, MAPTYPE>, MAX_ROOM> g_rooms;
extern MAP NewmapLandmassMap;
extern NAVMESH NewmapLandmassNavMesh;
extern MAP Level3Map;
extern NAVMESH Level3NavMesh;

MAP* GetMap(int room)
{
    if (room < 0 || room >= MAX_ROOM)
        return nullptr;

    switch (g_rooms[room].second)
    {
    case MAPTYPE::LANDMASS: return &NewmapLandmassMap;
    case MAPTYPE::LEVEL3:   return &Level3Map;
    }

    return nullptr;
}

NAVMESH* GetNavMesh(int room)
{
    if (room < 0 || room >= MAX_ROOM)
        return nullptr;

    switch (g_rooms[room].second)
    {
    case MAPTYPE::LANDMASS: return &NewmapLandmassNavMesh;
    case MAPTYPE::LEVEL3:   return &Level3NavMesh;
    }

    return nullptr;
}