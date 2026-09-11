#include "ScriptedCreature.h"
#include "ObjectAccessor.h"


enum Spells
{
    SPELL_FORKED_LIGHTNING = 63541,
    SPELL_OUT_OF_RANGE_PING = 63539,
    SPELL_OTHER_DEAD_BUFF = 63528,
    SPELL_SUMMON_SPHERE = 63527,
    SPELL_VENGEFUL_SURGE = 63630
};

enum Events
{
    EVENT_CHECK_DISTANCE = 1,
    EVENT_FORKED_LIGHTNING = 2,
    EVENT_SUMMON_SPHERE = 3
};

struct npc_ulduar_storm_tempered_keeper : public ScriptedAI
{
    npc_ulduar_storm_tempered_keeper(Creature* creature) : ScriptedAI(creature) {}

public:
    void Reset() override
    {
        _events.Reset();
        _otherGUID.Clear();
    }

    void JustEngagedWith(Unit* who) override
    {
        _events.ScheduleEvent(EVENT_CHECK_DISTANCE, 2s);
        _events.ScheduleEvent(EVENT_FORKED_LIGHTNING, 5s, 8s);
        _events.ScheduleEvent(EVENT_SUMMON_SPHERE, (me->GetEntry() == 33722 ? 20s : 50s));

        uint32 otherEntry = (me->GetEntry() == 33722) ? 33699 : 33722;
        if (Creature* other = me->FindNearestCreature(otherEntry, 30.0f, true))
        {
            _otherGUID = other->GetGUID();
            if (!other->IsInCombat())
            {
                other->SetInCombatWith(who);
                other->AI()->AttackStart(who);
            }
        }
        else
        {
            me->CastSpell(me, SPELL_VENGEFUL_SURGE, true);
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Creature* other = ObjectAccessor::GetCreature(*me, _otherGUID))
            other->CastSpell(other, SPELL_VENGEFUL_SURGE, true);
    }

    void JustSummoned(Creature* summon) override
    {
        if (Creature* other = ObjectAccessor::GetCreature(*me, _otherGUID))
        {
            float x = other->GetPositionX();
            float y = other->GetPositionY();
            float z = other->GetPositionZ() + 1.0f;
            summon->GetMotionMaster()->MovePoint(1, x, y, z);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_CHECK_DISTANCE:
            {
                if (Creature* other = ObjectAccessor::GetCreature(*me, _otherGUID))
                    if (other->IsAlive() && me->GetExactDist2d(other) > 45.0f)
                        me->CastSpell(me, SPELL_OUT_OF_RANGE_PING, true);

                if (Creature* sphere = me->FindNearestCreature(33715, 2.0f, true))
                    if (sphere->IsSummon())
                        if (TempSummon* temp = sphere->ToTempSummon())
                            if (temp->GetSummonerGUID() != me->GetGUID())
                                me->CastSpell(me, SPELL_OTHER_DEAD_BUFF, true);

                _events.ScheduleEvent(EVENT_CHECK_DISTANCE, 2s);
                break;
            }

            case EVENT_FORKED_LIGHTNING:
                me->CastSpell(me->GetVictim(), SPELL_FORKED_LIGHTNING, false);
                _events.ScheduleEvent(EVENT_FORKED_LIGHTNING, 10s, 14s);
                break;

            case EVENT_SUMMON_SPHERE:
            {
                std::list<Creature*> spheres;
                GetCreatureListWithEntryInGrid(spheres, me, 33715, 100.0f);

                bool sphereAlive = false;
                for (Creature* sphere : spheres)
                    if (sphere->IsAlive())
                    {
                        sphereAlive = true;
                        break;
                    }

                if (!sphereAlive && !me->HasAura(SPELL_VENGEFUL_SURGE))
                    me->CastSpell(me, SPELL_SUMMON_SPHERE, false);

                _events.ScheduleEvent(EVENT_SUMMON_SPHERE, 60s);
                break;
            }
            }
        }

        DoMeleeAttackIfReady();
    }
private:
    EventMap _events;
    ObjectGuid _otherGUID;
};

struct npc_ulduar_charged_sphere : public ScriptedAI
{
    npc_ulduar_charged_sphere(Creature* creature) : ScriptedAI(creature)
    {
        me->SetReactState(REACT_PASSIVE);
    }

    void AttackStart(Unit* /*target*/) override {}
    void MoveInLineOfSight(Unit* /*who*/) override {}
    void EnterCombat(Unit* /*who*/) {}

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE || id != 1)
            return;

        Creature* targetKeeper = nullptr;
        std::list<Creature*> keepers;
        GetCreatureListWithEntryInGrid(keepers, me, 33699, 10.0f);
        GetCreatureListWithEntryInGrid(keepers, me, 33722, 10.0f);

        for (Creature* keeper : keepers)
            if (keeper && keeper->IsAlive())
            {
                targetKeeper = keeper;
                break;
            }

        if (targetKeeper)
            targetKeeper->AddAura(63528, targetKeeper);

        me->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 /*diff*/) override {}
};

void AddSC_ulduar_scripts()
{
    RegisterCreatureAI(npc_ulduar_storm_tempered_keeper);
    RegisterCreatureAI(npc_ulduar_charged_sphere);
}
