#include "ScriptedCreature.h"
#include "ScriptMgr.h"

enum eNPCs
{
    NPC_INJURED_7TH_LEGION_SOLDIER = 27788
};

struct npc_mindless_ghoul : public ScriptedAI
{
    npc_mindless_ghoul(Creature* creature) : ScriptedAI(creature)
    {
        me->SetCorpseDelay(1);
    }

    bool CanAIAttack(Unit const* who) const override
    {
        return who->GetEntry() == NPC_INJURED_7TH_LEGION_SOLDIER;
    }

    void JustDied(Unit* /*killer*/) override
    {
        me->SetCorpseDelay(1);
    }
};

// Registrieren
void AddSC_npc_mindless_ghoul()
{
    RegisterCreatureAI(npc_mindless_ghoul);
}
