#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "ObjectAccessor.h"

enum Spells
{
    SPELL_REGEN_BLOCKER = 45008,
    SPELL_ON_DEATH = 45070
};

struct npc_sorlof : public ScriptedAI
{
    npc_sorlof(Creature* creature) : ScriptedAI(creature), _regenBlocked(false), _noRegenTimer(0) {}

    void Reset() override
    {
        _regenBlocked = false;
        _noRegenTimer = 0;
        me->SetRegenerateHealth(true);
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_REGEN_BLOCKER)
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
            if (!spellInfo || spellInfo->Id != SPELL_REGEN_BLOCKER)
                me->Attack(attacker, true);
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        me->CastSpell(me, SPELL_ON_DEATH, true);
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

        DoMeleeAttackIfReady();
    }

private:
    bool _regenBlocked;
    uint32 _noRegenTimer;
};

// Registrieren
void AddSC_npc_sorlof()
{
    RegisterCreatureAI(npc_sorlof);
}
