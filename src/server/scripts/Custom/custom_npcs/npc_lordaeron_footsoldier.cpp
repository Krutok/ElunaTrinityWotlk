#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "SpellMgr.h"
#include "SpellInfo.h"

enum Spells_npc_lordaeron_footsoldier
{
    SPELL_INVIS_TRIGGER = 58916,
    SPELL_ON_PLAYER = 6660
};

enum Texts
{
    SAY_AGGRO = 0
};

enum Events
{
    EVENT_CAST_SPELL = 1
};

struct npc_lordaeron_footsoldier : public ScriptedAI
{
    npc_lordaeron_footsoldier(Creature* creature) : ScriptedAI(creature) {}

public:
    void Reset() override
    {
        me->SetVisible(true);
        _events.Reset();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        Talk(SAY_AGGRO);
        _events.ScheduleEvent(EVENT_CAST_SPELL, 5s, 6s);
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_INVIS_TRIGGER)
            me->SetVisible(false);
    }

    void AttackStart(Unit* who) override
    {
        if (!who)
            return;

        ScriptedAI::AttackStart(who);

        std::list<Creature*> nearbyCreatures;
        me->GetCreatureListWithEntryInGrid(nearbyCreatures, me->GetEntry(), 3.0f);

        for (Creature* npc : nearbyCreatures)
        {
            if (npc == me || !npc->IsAlive())
                continue;

            if (!npc->IsInCombat() || npc->GetVictim() != who)
            {
                if (CreatureAI* ai = npc->AI())
                    ai->AttackStart(who);
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            if (eventId == EVENT_CAST_SPELL)
            {
                Unit* victim = me->GetVictim();
                if (!victim)
                    break;

                SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(SPELL_ON_PLAYER);
                if (!spellInfo)
                    break;

                float maxRange = spellInfo->GetMaxRange(false);

                if (me->GetDistance(victim) > maxRange)
                    me->CastSpell(victim, SPELL_ON_PLAYER, true);

                _events.ScheduleEvent(EVENT_CAST_SPELL, 5s, 6s);
            }
        }

        DoMeleeAttackIfReady();
    }
private:
    EventMap _events;
};

void AddSC_npc_lordaeron_footsoldier()
{
    RegisterCreatureAI(npc_lordaeron_footsoldier);
}
