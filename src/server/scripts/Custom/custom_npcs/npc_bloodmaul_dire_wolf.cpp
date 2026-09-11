#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "EventMap.h"

enum A_DIRE_SITUATION
{
SPELL_RINAS_DIMINUTION_POWDER = 36310,
SPELL_REND = 13443,

EVENT_CAST_REND = 1
};

struct npc_bloodmaul_dire_wolf : public ScriptedAI
{
    npc_bloodmaul_dire_wolf(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        _events.Reset();
        _friendly = false;

        me->SetFaction(1781);
        me->SetReactState(REACT_AGGRESSIVE);

        _events.ScheduleEvent(EVENT_CAST_REND, 5s, 10s);
    }

    void SpellHit(WorldObject* /*caster*/, const SpellInfo* spell) override
    {
        if (spell->Id == SPELL_RINAS_DIMINUTION_POWDER)
        {
            _friendly = true;

            if (me->IsInCombat())
                me->CombatStop();

            me->GetMotionMaster()->MovePoint(0, me->GetHomePosition());
        }
    }

    void EnterEvadeMode(EvadeReason why = EVADE_REASON_OTHER) override
    {
        if (!_friendly || !me->HasAura(SPELL_RINAS_DIMINUTION_POWDER))
            ScriptedAI::EnterEvadeMode(why);
        else
            me->CombatStop();
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            if (eventId == EVENT_CAST_REND)
            {
                if (Unit* target = me->GetVictim())
                    DoCastVictim(SPELL_REND, false);

                _events.Repeat(15s, 20s);
            }
        }

        if (_friendly && me->HasAura(SPELL_RINAS_DIMINUTION_POWDER))
        {
            me->SetFaction(35);
            me->SetReactState(REACT_PASSIVE);
        }
        else
        {
            _friendly = false;
            me->SetFaction(1781);
            me->SetReactState(REACT_AGGRESSIVE);
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    bool _friendly = false;
};

CreatureAI* GetAI_npc_bloodmaul_dire_wolf(Creature* creature)
{
    return new npc_bloodmaul_dire_wolf(creature);
}

void AddSC_npc_bloodmaul_dire_wolf()
{
    RegisterCreatureAI(npc_bloodmaul_dire_wolf);
}