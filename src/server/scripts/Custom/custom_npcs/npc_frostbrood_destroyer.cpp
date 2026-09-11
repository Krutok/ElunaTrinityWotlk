#include "ScriptMgr.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "Unit.h"
#include "SpellInfo.h"
#include "EventMap.h"

enum Spells
{
    SPELL_REGEN_BLOCKER_1 = 57385,
    SPELL_REGEN_BLOCKER_2 = 57412,
    SPELL_COMBAT_1 = 69649,
    SPELL_COMBAT_2 = 53363
};

enum Events
{
    EVENT_COMBAT_1 = 1,
    EVENT_COMBAT_2
};

struct npc_frostbrood_destroyer : public ScriptedAI
{
    npc_frostbrood_destroyer(Creature* creature) : ScriptedAI(creature), _regenBlocked(false), _noRegenTimer(0) {}


    void Reset() override
    {
        _regenBlocked = false;
        _noRegenTimer = 0;
        me->SetRegenerateHealth(true);
        _events.Reset();
    }

    void JustEngagedWith(Unit* who) override
    {
        if (!who || !who->IsAlive())
            return;

        _events.ScheduleEvent(EVENT_COMBAT_1, 5s, 10s);
        _events.ScheduleEvent(EVENT_COMBAT_2, 10s, 15s);
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_REGEN_BLOCKER_1 || spell->Id == SPELL_REGEN_BLOCKER_2)
        {
            _noRegenTimer = 0;
            if (!_regenBlocked)
            {
                _regenBlocked = true;
                me->SetRegenerateHealth(false);
            }
        }
    }

    void DamageTaken(Unit* attacker, uint32& /*damage*/, DamageEffectType /*damageType*/, SpellInfo const* spellInfo) override
    {
        if (!me->IsInCombat() && attacker && attacker->IsPlayer())
        {
            if (!spellInfo || (spellInfo->Id != SPELL_REGEN_BLOCKER_1 && spellInfo->Id != SPELL_REGEN_BLOCKER_2))
                me->Attack(attacker, true);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (_regenBlocked)
        {
            _noRegenTimer += diff;
            if (_noRegenTimer >= 5000)
            {
                _regenBlocked = false;
                me->SetRegenerateHealth(true);
            }
        }

        if (!UpdateVictim())
            return;

        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_COMBAT_1:
                me->CastSpell(me->GetVictim(), SPELL_COMBAT_1, false);
                _events.ScheduleEvent(EVENT_COMBAT_1, 10s, 14s);
                break;

            case EVENT_COMBAT_2:
                me->CastSpell(me->GetVictim(), SPELL_COMBAT_2, false);
                _events.ScheduleEvent(EVENT_COMBAT_2, 15s, 25s);
                break;
            }
        }

        DoMeleeAttackIfReady();
    }
private:
    EventMap _events;
    bool _regenBlocked;
    uint32 _noRegenTimer;
};

void AddSC_npc_frostbrood_destroyer()
{
    RegisterCreatureAI(npc_frostbrood_destroyer);
}
