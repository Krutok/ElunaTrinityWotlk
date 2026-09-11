#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ObjectAccessor.h"


enum AirForceBots_custom
{
    TRIPWIRE_CUSTOM, // do not attack flying players, smaller range
    ALARMBOT_CUSTOM, // attack flying players, casts guard's mark

    SPELL_GUARDS_MARK = 38067
};

float constexpr RANGE_TRIPWIRE_CUSTOM =  30.0f;
float constexpr RANGE_ALARMBOT_CUSTOM = 100.0f;

struct AirForceSpawn_custom
{
    uint32 myEntry;
    uint32 otherEntry;
    AirForceBots_custom type;
};

AirForceSpawn_custom constexpr AirForceSpawn_customs[] =
{
    {100788, 100790, ALARMBOT_CUSTOM}, // Ruft die Wachen in der Luft (Alliance)
    {100787, 100789, TRIPWIRE_CUSTOM}, // Ruft die Wachen auf dem Boden (Alliance)

    {100786, 100792, ALARMBOT_CUSTOM}, // Ruft die Wachen in der Luft (Horde)
    {100785, 100791, TRIPWIRE_CUSTOM}, // Ruft die Wachen auf dem Boden (Horde)
};

struct npc_city_guardsAI : public ScriptedAI
{
    npc_city_guardsAI(Creature* creature) : ScriptedAI(creature), _spawn(FindSpawnFor(creature->GetEntry())) {}

    void UpdateAI(uint32 /*diff*/) override
    {
        if (_toAttack.empty())
            return;

        Creature* guard = GetOrSummonGuard();
        if (!guard || !guard->IsAlive())
            return;

        for (ObjectGuid guid : _toAttack)
        {
            Unit* target = ObjectAccessor::GetUnit(*me, guid);
            if (!target || guard->IsEngagedBy(target))
                continue;

            guard->EngageWithTarget(target);
            if (_spawn.type == ALARMBOT_CUSTOM)
                guard->CastSpell(target, SPELL_GUARDS_MARK, true);
        }

        _toAttack.clear();
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (who->GetTypeId() != TYPEID_PLAYER)
            return;

        if (_toAttack.find(who->GetGUID()) != _toAttack.end())
            return;

        if (!who->IsWithinDistInMap(me, (_spawn.type == ALARMBOT_CUSTOM) ? RANGE_ALARMBOT_CUSTOM : RANGE_TRIPWIRE_CUSTOM))
            return;

        if (!(me->IsHostileTo(who) || who->IsHostileTo(me)))
            return;

        if (!me->IsValidAttackTarget(who))
            return;

        if ((_spawn.type == TRIPWIRE_CUSTOM) && who->IsFlying())
            return;

        _toAttack.insert(who->GetGUID());
    }

    static AirForceSpawn_custom const& FindSpawnFor(uint32 entry)
    {
        for (AirForceSpawn_custom const& spawn : AirForceSpawn_customs)
            if (spawn.myEntry == entry)
                return spawn;

        TC_LOG_ERROR("scripts", "Unhandled creature with entry %u in npc_city_guardsAI", entry);
        return AirForceSpawn_customs[0];
    }

    Creature* GetOrSummonGuard()
    {
        Creature* guard = ObjectAccessor::GetCreature(*me, _myGuard);
        if (!guard)
        {
            guard = me->SummonCreature(_spawn.otherEntry, 0.0f, 0.0f, 0.0f, 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 5min);
            if (guard)
                _myGuard = guard->GetGUID();
        }
        return guard;
    }
private:
    AirForceSpawn_custom const& _spawn;
    ObjectGuid _myGuard;
    std::unordered_set<ObjectGuid> _toAttack;
};

void AddSC_npc_city_guards()
{
    RegisterCreatureAI(npc_city_guardsAI);
}

