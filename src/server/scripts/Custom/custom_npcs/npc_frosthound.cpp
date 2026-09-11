#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedEscortAI.h"

enum qSniffing
{
    SPELL_SUMMON_PURSUERS_PERIODIC = 54993,
    SPELL_SNIFFING_CREDIT = 55477,
};

struct npc_frosthound : public EscortAI
{
    npc_frosthound(Creature* creature) : EscortAI(creature) {}

    void AttackStart(Unit* /*who*/) override {}
    void JustEngagedWith(Unit* /*who*/) override {}
    void EnterEvadeMode(EvadeReason /*why*/) override {}

    void Reset() override
    {
        me->SetReactState(REACT_PASSIVE);
    }

    void PassengerBoarded(Unit* who, int8 /*seatId*/, bool apply) override
    {
        if (who->IsPlayer() && apply)
        {
            me->SetFaction(who->GetFaction());
            me->CastSpell(me, SPELL_SUMMON_PURSUERS_PERIODIC, true);
            LoadPath(237417);
            Start(false, who->GetGUID());
        }
    }

    void JustDied(Unit* /*killer*/) override {}

    void OnCharmed(bool /*apply*/) override {}

    void UpdateAI(uint32 diff) override
    {
        EscortAI::UpdateAI(diff);

        if (!UpdateVictim())
            return;
    }

    void WaypointReached(uint32 waypointId, uint32 /*pathId*/) override
    {
        Player* player = GetPlayerForEscort();
        if (!player)
            return;

        switch (waypointId)
        {
        case 0:
            Talk(0);
            break;
        case 1:
            Talk(1);
            break;
        case 12:
            Talk(3);
            break;
        case 19:
            Talk(2);
            if (Unit* summoner = me->ToTempSummon()->GetSummonerUnit())
                if (Player* summonerPlayer = summoner->ToPlayer())
                    summonerPlayer->KilledMonsterCredit(29677);
            break;
        }
    }

    void JustSummoned(Creature* summon) override
    {
        summon->ToTempSummon()->SetTempSummonType(TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT);
        summon->ToTempSummon()->InitStats(20000);

        if (urand(0, 1))
            summon->GetMotionMaster()->MoveFollow(me, 0.0f, 0.0f);
        else if (summon->AI())
            summon->AI()->AttackStart(me);
    }
};

void AddSC_npc_frosthound()
{
    RegisterCreatureAI(npc_frosthound);
}
