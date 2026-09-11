#include "ScriptMgr.h"
#include "MapManager.h"
#include "Map.h"
#include "Log.h"

class brewfest_grid_loader : public WorldScript
{
public:
    brewfest_grid_loader() : WorldScript("brewfest_grid_loader") { }

    // Entferne override, falls es einen Fehler gibt
    void OnStartup()
    {
        TC_LOG_INFO("server.loading", "brewfest_grid_loader: Lade Brewfest-Grids...");

        // Koordinaten Dun Morogh (Map 0)
        uint32 mapId1 = 0;
        float x1 = -5168.217f;
        float y1 = -602.419f;
        float z1 = 397.852f;

        // Koordinaten Felshauerhof (Map 1)
        uint32 mapId2 = 1;
        float x2 = 1191.031f;
        float y2 = -4308.875f;
        float z2 = 21.294f;

        if (Map* map1 = sMapMgr->CreateBaseMap(mapId1))
        {
            map1->LoadGrid(x1, y1);
            TC_LOG_INFO("server.loading", "brewfest_grid_loader: Grid für Dun Morogh (Map: {}) bei X: {}, Y: {}, Z: {} geladen.", mapId1, x1, y1, z1);
        }

        if (Map* map2 = sMapMgr->CreateBaseMap(mapId2))
        {
            map2->LoadGrid(x2, y2);
            TC_LOG_INFO("server.loading", "brewfest_grid_loader: Grid für Felshauerhof (Map: {}) bei X: {}, Y: {}, Z: {} geladen.", mapId2, x2, y2, z2);
        }
    }
};

void AddSC_brewfest_grid_loader()
{
    new brewfest_grid_loader();
}