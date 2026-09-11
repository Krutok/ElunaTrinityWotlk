#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-source-encoding"
#include "ScriptMgr.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "WorldSession.h"
#include "Chat.h"

class item_summon_npc : public ItemScript
{
public:
    item_summon_npc() : ItemScript("item_summon_npc") { }

    bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
    {
        if (!item)
            return false;

        if (player->IsInCombat())
        {
            player->GetSession()->SendNotification("Du kannst diesen Gegenstand nicht im Kampf verwenden");
            return true;
        }
        if (player->InArena())
        {
            player->GetSession()->SendNotification("You cannot use that item in Arena");
            return true;
        }
        if (player->InBattleground())
        {
            player->GetSession()->SendNotification("You cannot use that item in Battleground");
            return true;
        }
        //Replace 9999 with your Npc entry
        if (player->FindNearestCreature(100000, 50.0f))
        {
            player->GetSession()->SendNotification("Der Name der Kreatur kann nicht beschworen werden, da sich derselbe NPC in der Nähe befindet.");
            return false;
        }

        //Replace 9999 with your Npc entry
        const Milliseconds despawnTimer = 60000ms; // 5 seconds
        Creature* creature = player->SummonCreature(100000,
            { player->GetPositionX(), player->GetPositionY() - 2, player->GetPositionZ(), player->GetOrientation() }, TEMPSUMMON_TIMED_DESPAWN, despawnTimer);
        creature->Say("1 Minute bin ich da", LANG_UNIVERSAL, NULL);

        return false;
    }

};

void AddSC_item_summon_npc() // Add to custom_script_loader.cpp normally
{
    new item_summon_npc;
}