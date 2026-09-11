#include "ScriptMgr.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureAIImpl.h"
#include "Player.h"
#include "MoveSplineInit.h"
#include "ObjectAccessor.h"
#include "SharedDefines.h"

enum IronWatcherSpells
{
    SPELL_THUNDERING_STOMP = 60925,
    SPELL_STORM_HAMMER = 56448,
    SPELL_SHATTERED_EYES = 57290,
    SPELL_STORM_HAMMER_DUMMY = 60930
};

enum IronWatcherTexts
{
    SAY_CHARGE_PHASE = 0,
    SAY_INTERRUPT_CHARGE = 1
};

enum IronWatcherEvents
{
    EVENT_PLAY_STUN_ANIM = 1,
};

struct npc_iron_watcher : public ScriptedAI
{
    npc_iron_watcher(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        _spellTimer = 0;
        _hpTimer = 0;
        _charging = false;
        _damageImmune = false;
        _events.Reset();

        me->ClearUnitState(UNIT_STATE_STUNNED);
        me->SetControlled(false, UNIT_STATE_STUNNED);
        me->SetEmoteState(EMOTE_STATE_NONE);

        me->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_ALL, false);
    }

    void MovementInform(uint32 type, uint32 /*id*/) override
    {
        if (type == POINT_MOTION_TYPE)
        {
            me->SetControlled(true, UNIT_STATE_STUNNED);
            _damageImmune = true;

            me->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_ALL, true);

            me->GetMotionMaster()->Clear();
            me->SendMeleeAttackStop();

            _events.ScheduleEvent(EVENT_PLAY_STUN_ANIM, std::chrono::milliseconds(300));
            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC);
        }
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        if (!spellInfo)
            return;

        if (spellInfo->Id == SPELL_STORM_HAMMER && _charging && caster && caster->ToPlayer())
        {
            DoCastSelf(SPELL_STORM_HAMMER_DUMMY, true);
            me->RemoveAllAurasExceptType(SPELL_AURA_MECHANIC_IMMUNITY);
            Talk(SAY_INTERRUPT_CHARGE);

            caster->ToPlayer()->KilledMonsterCredit(me->GetEntry());
            me->DespawnOrUnsummon(8s);

            Position jumpPos(8721.94f, -1955.0f, 963.0f);
            me->GetMotionMaster()->MoveJump(jumpPos, 70.0f, 30.0f);

            _damageImmune = false;
            me->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_ALL, false);
            me->SetEmoteState(EMOTE_STATE_NONE);
        }
    }

    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* spellInfo) override
    {
        if (!_damageImmune)
            return;

        if (!attacker)
            return;

        Player* playerAttacker = attacker->GetCharmerOrOwnerPlayerOrPlayerItself();

        if (playerAttacker && (!spellInfo || spellInfo->Id != SPELL_STORM_HAMMER))
            damage = 0;
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_PLAY_STUN_ANIM:
                me->SetEmoteState(EMOTE_ONESHOT_STUN);
                break;
            }
        }

        if (_charging)
            return;

        if (!UpdateVictim())
            return;

        _spellTimer += diff;
        _hpTimer += diff;

        if (_spellTimer >= 10000)
        {
            DoCastSelf(SPELL_THUNDERING_STOMP, false);
            _spellTimer = 0;
        }

        if (_hpTimer >= 1000)
        {
            if (me->HealthBelowPct(40) && !_charging)
            {
                Talk(SAY_CHARGE_PHASE);

                me->RemoveAllAuras();
                DoCastSelf(SPELL_SHATTERED_EYES, true);
                me->ApplySpellImmune(SPELL_SHATTERED_EYES, IMMUNITY_MECHANIC, MECHANIC_STUN, false);
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC);
                me->GetMotionMaster()->MoveCharge(8536.154297f, -1958.307739f, 1467.757935f);
                _charging = true;
            }
            _hpTimer = 0;
        }

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    uint32 _spellTimer = 0;
    uint32 _hpTimer = 0;
    bool _charging = false;
    bool _damageImmune = false;
};

void AddSC_npc_iron_watcher()
{
    RegisterCreatureAI(npc_iron_watcher);
}
