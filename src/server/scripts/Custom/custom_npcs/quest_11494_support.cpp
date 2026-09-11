#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "ObjectAccessor.h"
#include "EventMap.h"

enum Spells
{
    SPELL_PLAYER_REACT = 44609,
    SPELL_FIRST_AGGRO = 49758,
    SPELL_REPEAT = 42580,
    SPELL_REPEAT_CAST = 49742
};

enum Talks
{
    SAY_TALK0 = 0,
    SAY_TALK1 = 1,
    SAY_TALK2 = 2
};

enum Events
{
    EVENT_TRIGGER_ATTACK = 1,
    EVENT_REPEAT_SPELL = 2
};

struct npc_two_phase : public ScriptedAI
{
    npc_two_phase(Creature* creature) : ScriptedAI(creature)
    {
        _timerActive = false;
        _targetCreature = nullptr;
        _triggerCreature = nullptr;
        _movelineCooldown = 0;
        _savedEmoteState = 0;
    }

    EventMap _events;

public:
    void Reset() override
    {
        _events.Reset();
        _timerActive = false;
        _targetCreature = nullptr;
        _triggerCreature = nullptr;
        _movelineCooldown = 0;
        _savedEmoteState = 0;
        me->RestoreFaction();
        me->SetWalk(false);
        me->GetMotionMaster()->MovePoint(3, me->GetHomePosition());
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (_timerActive || _movelineCooldown || me->IsInCombat())
            return;

        if (!who || who->GetTypeId() != TYPEID_UNIT)
            return;

        Creature* creature = who->ToCreature();
        if (!creature || creature->GetEntry() != 24825)
            return;

        if (me->IsWithinDistInMap(creature, 9.0f) && !me->IsHostileTo(creature))
        {
            if (urand(0, 99) < 20)
            {
                _triggerCreature = creature;
                _targetCreature = me;
                _timerActive = true;
                _movelineCooldown = 60 * IN_MILLISECONDS;

                _savedEmoteState = me->GetUInt32Value(UNIT_NPC_EMOTESTATE);
                me->SetUInt32Value(UNIT_NPC_EMOTESTATE, 0);

                float x, y, z;
                creature->GetClosePoint(x, y, z, 2.0f);
                me->GetMotionMaster()->MovePoint(1, x, y, z);
            }
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        switch (id)
        {
        case 1:
            Talk(SAY_TALK0);
            me->GetMotionMaster()->MoveIdle();

            if (_triggerCreature && _triggerCreature->IsAlive())
                me->GetMotionMaster()->MoveFollow(_triggerCreature, 2.0f, 0.0f);

            _events.ScheduleEvent(EVENT_TRIGGER_ATTACK, 10s);
            break;

        case 2:
            me->GetMotionMaster()->Initialize();
            me->SetWalk(false);
            break;

        case 3:
            me->GetMotionMaster()->Initialize();
            break;
        }
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spell) override
    {
        if (!_timerActive || !caster)
            return;

        if (spell->Id == SPELL_PLAYER_REACT)
        {
            if (_triggerCreature && _triggerCreature->IsAlive())
                _triggerCreature->AI()->Talk(SAY_TALK0);

            Talk(SAY_TALK1);

            _events.CancelEvent(EVENT_TRIGGER_ATTACK);
            _timerActive = false;
            _targetCreature = nullptr;
            _triggerCreature = nullptr;

            if (_savedEmoteState)
                me->SetUInt32Value(UNIT_NPC_EMOTESTATE, _savedEmoteState);

            me->SetWalk(true);
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MovePoint(2, me->GetHomePosition());
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        switch (me->GetEntry())
        {
        case 23672:
            _events.ScheduleEvent(EVENT_REPEAT_SPELL, 5s, 8s);
            break;

        case 23673:
            DoCast(who, SPELL_FIRST_AGGRO);
            _events.ScheduleEvent(EVENT_REPEAT_SPELL, 2s);
            break;

        case 23675:
            _events.ScheduleEvent(EVENT_REPEAT_SPELL, 7s, 10s);
            break;

        case 24271:
            _events.ScheduleEvent(EVENT_REPEAT_SPELL, 7s, 10s);
            break;
        }

        AttackStart(who);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (me->GetDistance(me->GetHomePosition()) > 100.0f)
        {
            me->AttackStop();
            me->CombatStop();
            me->GetMotionMaster()->Initialize();
            Reset();
        }

        if (_movelineCooldown)
        {
            if (_movelineCooldown <= diff)
                _movelineCooldown = 0;
            else
                _movelineCooldown -= diff;
        }

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_TRIGGER_ATTACK:
                if (_triggerCreature && _triggerCreature->IsAlive())
                {
                    Talk(SAY_TALK2);
                    me->GetMotionMaster()->MoveChase(_triggerCreature);
                    me->SetFaction(14);
                    me->Attack(_triggerCreature, true);
                }
                _timerActive = false;
                _targetCreature = nullptr;
                _triggerCreature = nullptr;
                break;

            case EVENT_REPEAT_SPELL:
            {
                if (Unit* target = me->GetVictim())
                {
                    switch (me->GetEntry())
                    {
                    case 23672:
                        DoCast(target, 49749);
                        _events.Repeat(5s, 8s);
                        break;

                    case 23673:
                        DoCast(target, SPELL_REPEAT);
                        _events.Repeat(30s);
                        break;

                    case 23675:
                        DoCast(target, 49753);
                        _events.Repeat(7s, 10s);
                        break;

                    case 24271:
                        DoCast(target, 49729);
                        _events.Repeat(7s, 10s);
                        break;
                    }
                }
                break;
            }
            }
        }
        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    bool _timerActive;
    Creature* _targetCreature;
    Creature* _triggerCreature;
    uint32 _movelineCooldown;
    uint32 _savedEmoteState;
};



CreatureAI* GetAI_npc_two_phase(Creature* creature)
{
    return new npc_two_phase(creature);
}

struct npc_simple_caster : public ScriptedAI
{
    npc_simple_caster(Creature* creature) : ScriptedAI(creature) {}

private:
    EventMap _events;

public:
    void Reset() override
    {
        _events.Reset();
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (!who)
            return;

        if (who->GetEntry() == 24825)
            return;

        ScriptedAI::MoveInLineOfSight(who);
    }

    void JustEngagedWith(Unit* who) override
    {
        _events.ScheduleEvent(1001, 6s, 8s);

        AttackStart(who);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
            case 1001:
                DoCast(SPELL_REPEAT_CAST);
                _events.Repeat(6s, 8s);
                break;
            default:
                break;
            }
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_simple_caster(Creature* creature)
{
    return new npc_simple_caster(creature);
}

void AddSC_npc_two_phase()
{
    RegisterCreatureAI(npc_two_phase);
    RegisterCreatureAI(npc_simple_caster);
}
