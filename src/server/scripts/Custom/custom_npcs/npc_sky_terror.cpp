#include "ScriptedCreature.h"
#include "Creature.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include <set>
#include <map>
#include <vector>

enum VehicleSpells
{
    SPELL_CAST_PASSENGER        = 46598,
    SPELL_AURA_53031            = 53031,
    SPELL_AURA_53039            = 53039,

    POINT_TO_TARGET             = 1,
    POINT_BACK_HOME             = 2,

    NPC_TALK                    = 0,

    EVET_TO_NPC                 = 1,

    NPC_ENTRY_1                 = 28028,
    NPC_ENTRY_2                 = 28029
};

struct npc_sky_terror : public ScriptedAI
{
public:
    npc_sky_terror(Creature* creature) : ScriptedAI(creature) { Reset(); }

    void Reset() override
    {
        _homePos.Relocate(me);
        _hasFlown = false;
        _reserved = false;
        _targetGuid.Clear();
        _auraCheckTimers.clear();

        _events.Reset();
        _events.ScheduleEvent(1, 10s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        for (auto it = _auraCheckTimers.begin(); it != _auraCheckTimers.end();)
        {
            if (Creature* target = ObjectAccessor::GetCreature(*me, it->first))
            {
                if (!target->HasAura(SPELL_AURA_53039))
                {
                    target->DespawnOrUnsummon();
                    it = _auraCheckTimers.erase(it);
                    continue;
                }

                if (it->second <= diff)
                    it->second = 1000;
                else
                {
                    it->second -= diff;
                    ++it;
                }
            }
            else
            {
                it = _auraCheckTimers.erase(it);
            }
        }

        while (uint32 eventId = _events.ExecuteEvent())
        {
            if (eventId == EVET_TO_NPC && !_hasFlown)
            {
                _hasFlown = true;

                std::vector<ObjectGuid> validTargets;
                std::list<Creature*> list;
                me->GetCreatureListWithEntryInGrid(list, NPC_ENTRY_1, 100.0f);
                me->GetCreatureListWithEntryInGrid(list, NPC_ENTRY_2, 100.0f);

                for (Creature* target : list)
                {
                    if (!target->IsAlive())
                        continue;
                    if (!target->HasAura(SPELL_AURA_53031))
                        continue;
                    if (s_ReservedTargets.find(target->GetGUID()) != s_ReservedTargets.end())
                        continue;

                    validTargets.push_back(target->GetGUID());
                }

                if (!validTargets.empty())
                {
                    _targetGuid = validTargets[urand(0, validTargets.size() - 1)];
                    s_ReservedTargets.insert(_targetGuid);
                    _reserved = true;

                    if (Creature* target = ObjectAccessor::GetCreature(*me, _targetGuid))
                        me->GetMotionMaster()->MovePoint(POINT_TO_TARGET, target->GetPosition());
                }
                else
                {
                    _hasFlown = false;
                    _events.ScheduleEvent(1, 5s);
                }
            }
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        switch (id)
        {
        case POINT_TO_TARGET:
        {
            if (Creature* target = ObjectAccessor::GetCreature(*me, _targetGuid))
            {
                target->CastSpell(me, SPELL_CAST_PASSENGER, true);
                target->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_ATTACKABLE_1);
                target->SetReactState(REACT_PASSIVE);
                target->GetThreatManager().ClearAllThreat();
                target->CombatStop();
            }
            me->GetMotionMaster()->MovePoint(POINT_BACK_HOME, _homePos);
            break;
        }
        case POINT_BACK_HOME:
        {
            Talk(NPC_TALK);
            ReleaseTarget();

            me->RemoveAura(SPELL_CAST_PASSENGER);

            if (Creature* target = ObjectAccessor::GetCreature(*me, _targetGuid))
            {
                target->CastSpell(target, SPELL_AURA_53039, true);
                _auraCheckTimers[_targetGuid] = 1000;
            }

            _hasFlown = false;
            _events.ScheduleEvent(1, 5s);
            break;
        }
        }
    }

private:
    void ReleaseTarget()
    {
        if (_reserved)
        {
            s_ReservedTargets.erase(_targetGuid);
            _reserved = false;
        }
    }

    EventMap _events;
    Position _homePos;
    ObjectGuid _targetGuid;
    bool _hasFlown;
    bool _reserved;
    std::map<ObjectGuid, uint32> _auraCheckTimers;

    static std::set<ObjectGuid> s_ReservedTargets;
};

std::set<ObjectGuid> npc_sky_terror::s_ReservedTargets;

void AddSC_npc_sky_terror()
{
    RegisterCreatureAI(npc_sky_terror);
}
