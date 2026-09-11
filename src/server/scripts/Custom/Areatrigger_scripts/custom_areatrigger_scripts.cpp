#include "CellImpl.h"
#include "Containers.h"
#include "GameObjectAI.h"
#include "GridNotifiersImpl.h"
#include "InstanceScript.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "Item.h"

/*
Areatrigger Script für die tür auf Dun Alcaz!
Areatrigger ID: 10851
*/

class at_thing : public AreaTriggerScript
{
public:
    at_thing() : AreaTriggerScript("at_thing") { }

    bool OnTrigger(Player* player, AreaTriggerEntry const* /*trigger*/) override
    {
        if (!player)
            return true;

        // GameObject mit Entry 600178 innerhalb von 50 Yards finden
        GameObject* go = player->FindNearestGameObject(600178, 50.0f);
        if (go)
            go->ActivateObject(GameObjectActions::Open, player);

        return true;
    }
};

/*
Areatrigger Script für die Instanz Abgrund der Finsternis, damit die fraktion auch als toter Spieler funktioniert!
Areatrigger ID: 10854
*/

class at_smarttrigger : public AreaTriggerScript
{
public:
    at_smarttrigger() : AreaTriggerScript("at_smarttrigger") {}

    bool OnTrigger(Player* player, AreaTriggerEntry const* /*trigger*/) override
    {
        // Map 743:
        uint32 mapId = 743;
        float x = 0.0f, y = 0.0f, z = 0.0f, o = 0.0f;

        if (player->GetTeam() == ALLIANCE)
        {
            x = -904.591f;
            y = -1222.82f;
            z = 642.511f;
            o = 5.60109f;
        }
        else // Horde
        {
            x = -913.867f;
            y = -1720.11f;
            z = 591.553f;
            o = 0.384177f;
        }

        player->TeleportTo(mapId, x, y, z, o);

        return true;
    }
};

class at_trial_of_the_champion_exit : public AreaTriggerScript
{
public:
    at_trial_of_the_champion_exit() : AreaTriggerScript("at_trial_of_the_champion_exit") { }

    bool OnTrigger(Player* player, AreaTriggerEntry const* /*trigger*/) override
    {
       
        player->TeleportTo(571, 8577.85f, 792.042f, 558.581f, 0.0f);
        return true;
    }
};

void AddSC_custom_areascripts()
{
    new at_smarttrigger();
    new at_thing();
    new at_trial_of_the_champion_exit();
}
