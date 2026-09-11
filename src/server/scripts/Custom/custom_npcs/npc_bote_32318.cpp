#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Player.h"

enum
{
    SPELL_CHANNEL = 60561,
    SPELL_TRIGGER = 60563,
    NPC_ID = 32314,
    ITEM_ID = 44434,
    ITEM_COUNT = 5
};

struct npc_bote_32318 : public ScriptedAI
{
    npc_bote_32318(Creature* creature) : ScriptedAI(creature), _triggeringPlayer(nullptr) {}


public:
    void SpellHit(WorldObject* caster, SpellInfo const* spell) override
    {
        if (!spell)
            return;

        if (spell->Id == SPELL_CHANNEL)
        {
            if (caster && caster->IsPlayer())
                _triggeringPlayer = caster->ToPlayer();
        }
        else if (spell->Id == SPELL_TRIGGER)
        {
            if (!_triggeringPlayer)
                return;

            Player* player = _triggeringPlayer;

            player->KilledMonsterCredit(NPC_ID);

            if (player->HasItemCount(ITEM_ID, ITEM_COUNT))
                player->DestroyItemCount(ITEM_ID, ITEM_COUNT, true);

            _triggeringPlayer = nullptr; // Reset
        }
    }
private:
    Player* _triggeringPlayer;
};

void AddSC_npc_bote_32318()
{
    RegisterCreatureAI(npc_bote_32318);
}
