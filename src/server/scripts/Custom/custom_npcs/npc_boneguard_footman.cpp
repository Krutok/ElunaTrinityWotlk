#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedEscortAI.h"
#include "Player.h"
#include "Creature.h"
#include "CombatAI.h"

enum
{
    NPC_CAMPAIGN_WARHORSE = 34125
};

struct npc_boneguard_footman : public ScriptedAI
{
    npc_boneguard_footman(Creature* creature) : ScriptedAI(creature), _checkTimer(0) {}


    void UpdateAI(uint32 diff) override
    {
        if (!me->IsInCombat())
            return;

        if (_checkTimer <= diff)
        {
            _checkTimer = 500;

            if (Unit* victim = me->GetVictim())
            {
                if (victim->GetEntry() == NPC_CAMPAIGN_WARHORSE &&
                    me->GetDistance2d(victim) < 3.0f &&
                    victim->isMoving())
                {
                    Unit::Kill(me, victim); // Sofortiger Tod des Reittiers
                    return;
                }
            }
        }
        else
        {
            _checkTimer -= diff;
        }

        ScriptedAI::UpdateAI(diff);
    }
private:
    uint32 _checkTimer;
};

void AddSC_npc_boneguard_footman()
{
    RegisterCreatureAI(npc_boneguard_footman);
}
