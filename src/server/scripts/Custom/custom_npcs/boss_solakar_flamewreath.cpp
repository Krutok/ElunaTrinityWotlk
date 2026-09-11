#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "EventMap.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "InstanceScript.h"
#include "EasternKingdoms/BlackrockMountain/BlackrockSpire/blackrock_spire.h"

enum father_flame_event
{
    SPELL_WAR_STOMP                 = 16727,
    SPELL_HATCH_EGG                 = 15746,
    SPELL_ADDITION_1                = 15572,
    SPELL_ADDITION_2                = 15580,

    NPC_SOLAKAR                     = 10264,
    NPC_ROOKERY_GUARDIAN            = 10258,
    NPC_ROOKERY_HATCHER             = 10683,
    NPC_ROOKERY_WHELP               = 10161,
    NPC_FATHER_FLAME_TRIGGER        = 101007, /* Custom NPC to Manage the The Event Can't Manage its despawn after Loot */

    GO_ROOKERY_EGG                  = 175124,

    EVENT_SPELL_HATCH_EGG           = 1,
    EVENT_ADDITION_1                = 2,
    EVENT_ADDITION_2                = 3,
    EVENT_CHECK_WAVE                = 1,
    EVENT_SPAWN_NEXT_WAVE           = 2,

    SAY_SUMMON                      = 0
};

constexpr float RANGE_SPELL_HATCH_EGG = 3.0f;
constexpr float RANGE_WHELP_CALL_HELP = 15.0f;

/*######
## NPC Rookery Hatcher
######*/

struct npc_rookery_hatcher : public CreatureAI
{
    npc_rookery_hatcher(Creature* creature) : CreatureAI(creature) {}

    EventMap events;
    GameObject* targetEgg = nullptr;
    Position targetPosition;

    void InitializeAI() override
    {
        CreatureAI::InitializeAI();
        DoZoneInCombat();
        targetEgg = nullptr;
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        Talk(SAY_SUMMON);
        events.ScheduleEvent(EVENT_SPELL_HATCH_EGG, 6s, 8s);
        events.ScheduleEvent(EVENT_ADDITION_1, 19s, 22s);
        events.ScheduleEvent(EVENT_ADDITION_2, 11s, 12s);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {

        if (!me->IsAlive())
            return;

        if (InstanceScript* instance = me->GetInstanceScript())
            instance->SetBossState(DATA_SOLAKAR_FLAMEWREATH, NOT_STARTED);

        me->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        std::list<Creature*> nearbyWhelps;
        GetCreatureListWithEntryInGrid(nearbyWhelps, me, NPC_ROOKERY_WHELP, RANGE_WHELP_CALL_HELP);
        for (auto whelp : nearbyWhelps)
        {
            if (!whelp->IsInCombat())
            {
                whelp->SetInCombatWith(me->GetVictim());
                whelp->AI()->AttackStart(me->GetVictim());
            }
        }

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        events.Update(diff);

        while (uint32 event = events.ExecuteEvent())
        {
            switch (event)
            {
            case EVENT_SPELL_HATCH_EGG:
            {
                if (!targetEgg)
                {
                    float minDist = 50.f;
                    std::list<GameObject*> nearbyEggs;
                    me->GetGameObjectListWithEntryInGrid(nearbyEggs, GO_ROOKERY_EGG, 40.f);
                    for (auto egg : nearbyEggs)
                    {
                        if (egg->isSpawned() && egg->getLootState() == GO_READY)
                        {
                            float dist = me->GetDistance2d(egg);
                            if (dist < minDist)
                            {
                                minDist = dist;
                                targetEgg = egg;
                            }
                        }
                    }
                }

                if (targetEgg)
                    me->GetMotionMaster()->MovePoint(0, targetEgg->GetPosition());
                events.Repeat(5s, 6s);
                break;
            }
            case EVENT_ADDITION_1:
                DoCastVictim(SPELL_ADDITION_1);
                events.Repeat(19s, 22s);
                break;
            case EVENT_ADDITION_2:
                DoCastVictim(SPELL_ADDITION_2);
                events.Repeat(11s, 12s);
                break;
            default:
                break;
            }
        }

        if (targetEgg && targetEgg->getLootState() == GO_READY && me->GetDistance2d(targetEgg) < RANGE_SPELL_HATCH_EGG)
        {
            me->StopMoving();
            me->SetFacingToObject(targetEgg);
            targetPosition = me->GetPosition();
            DoCast(SPELL_HATCH_EGG);
            targetEgg = nullptr;
            events.ScheduleEvent(SPELL_HATCH_EGG, 6s, 8s);
        }
        else if (!me->HasUnitState(UNIT_STATE_CASTING) && !targetEgg)
        {
            if (Unit* victim = me->GetVictim())
            {
                AttackStart(victim);
                if (me->GetDistance2d(victim) > me->GetAttackDistance(victim))
                    me->GetMotionMaster()->MovePoint(0, victim->GetPosition());
            }
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_rookery_hatcher(Creature* creature)
{
    return new npc_rookery_hatcher(creature);
}

/*######
## Boss Solakar Flamewreath
######*/

struct boss_solakar_flamewreath : public BossAI
{
    boss_solakar_flamewreath(Creature* creature) : BossAI(creature, DATA_SOLAKAR_FLAMEWREATH) {}

    uint32 resetTimer = 10000;

    void Reset() override
    {
        _Reset();
        resetTimer = 10000;

        if (InstanceScript* instance = me->GetInstanceScript())
            instance->SetBossState(DATA_SOLAKAR_FLAMEWREATH, NOT_STARTED);
    }

    void InitializeAI() override
    {
        BossAI::InitializeAI();
        DoZoneInCombat();
        Talk(SAY_SUMMON);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoZoneInCombat();
        events.ScheduleEvent(SPELL_WAR_STOMP, 17s, 20s);

        if (InstanceScript* instance = me->GetInstanceScript())
            instance->SetBossState(DATA_SOLAKAR_FLAMEWREATH, IN_PROGRESS);

        resetTimer = 0;
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();

        if (InstanceScript* instance = me->GetInstanceScript())
            instance->SetBossState(DATA_SOLAKAR_FLAMEWREATH, DONE);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {

        if (!me->IsAlive())
            return;

        if (InstanceScript* instance = me->GetInstanceScript())
            instance->SetBossState(DATA_SOLAKAR_FLAMEWREATH, NOT_STARTED);

        me->DespawnOrUnsummon();
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
        {
            if (resetTimer > diff)
                resetTimer -= diff;
            else if (instance)
                instance->SetData(DATA_SOLAKAR_FLAMEWREATH, FAIL);

            return;
        }

        resetTimer = 10000;
        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 event = events.ExecuteEvent())
        {
            switch (event)
            {
            case SPELL_WAR_STOMP:
                DoCastVictim(SPELL_WAR_STOMP);
                events.Repeat(17s, 20s);
                break;
            default:
                break;
            }
        }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_boss_solakar_flamewreath(Creature* creature)
{
    return new boss_solakar_flamewreath(creature);
}

/*######
## Father Flamme Wave Trigger
######*/

struct npc_father_flamme_bunny_trigger : public ScriptedAI
{
    npc_father_flamme_bunny_trigger(Creature* creature) : ScriptedAI(creature) {}

    std::vector<Position> _spawnPositions = {
        {68.041550f, -272.686798f, 92.641273f, 0.0f},
        {91.586212f, -284.880524f, 91.445862f, 0.0f},
        {98.587265f, -306.547272f, 91.444130f, 0.0f}
    };

public:

    void StartEvent(ObjectGuid playerGUID)
    {

        if (InstanceScript* instance = me->GetInstanceScript())
        {
            if (instance->GetBossState(DATA_SOLAKAR_FLAMEWREATH) != IN_PROGRESS)
                return;

            if (_eventActive)
                return;

            _playerGUID = playerGUID;
            _currentWave = 0;
            _spawnedNPCs.clear();
            _eventActive = true;
            SpawnNextWave();
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (InstanceScript* instance = me->GetInstanceScript())
        {
            if (instance->GetBossState(DATA_SOLAKAR_FLAMEWREATH) != IN_PROGRESS)
            {
                ResetEvent();
                return;
            }
        }

        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_CHECK_WAVE:
                CheckWaveCompletion();
                break;
            case EVENT_SPAWN_NEXT_WAVE:
                SpawnNextWave();
                break;
            }
        }
    }

    void SpawnNextWave()
    {

        if (_currentWave < _totalWaves)
        {
            for (int i = 0; i < 2; ++i)
            {
                Position spawnPos = _spawnPositions[urand(0, _spawnPositions.size() - 1)];
                uint32 entry = (i == 0) ? _waveNPCs.first : _waveNPCs.second;

                if (Creature* npc = me->SummonCreature(entry, spawnPos))
                {
                    _spawnedNPCs.push_back(npc->GetGUID());

                    if (Player* target = ObjectAccessor::GetPlayer(*me, _playerGUID))
                    {
                        npc->SetInCombatWith(target);
                        npc->AI()->AttackStart(target);
                    }
                }
            }
            _events.ScheduleEvent(EVENT_CHECK_WAVE, 1s);
        }
        else
        {
            Position bossPos = _spawnPositions[urand(0, _spawnPositions.size() - 1)];
            if (Creature* boss = me->SummonCreature(_bossEntry, bossPos))
            {
                if (Player* target = ObjectAccessor::GetPlayer(*me, _playerGUID))
                {
                    boss->SetInCombatWith(target);
                    boss->AI()->AttackStart(target);
                }
            }
        }

        ++_currentWave;
    }

    void CheckWaveCompletion()
    {

        for (auto it = _spawnedNPCs.begin(); it != _spawnedNPCs.end();)
        {
            if (Creature* npc = ObjectAccessor::GetCreature(*me, *it))
            {
                if (!npc->IsAlive())
                    it = _spawnedNPCs.erase(it);
                else
                    ++it;
            }
            else
            {
                it = _spawnedNPCs.erase(it);
            }
        }

        if (_spawnedNPCs.empty())
            _events.ScheduleEvent(EVENT_SPAWN_NEXT_WAVE, 10s);
        else
            _events.ScheduleEvent(EVENT_CHECK_WAVE, 1s);
    }

    void ResetEvent()
    {
        _events.Reset();
        _currentWave = 0;
        _eventActive = false;

        for (auto guid : _spawnedNPCs)
        {
            if (Creature* npc = ObjectAccessor::GetCreature(*me, guid))
                npc->DespawnOrUnsummon();
        }
        _spawnedNPCs.clear();
    }


private:
    EventMap _events;
    uint8 _currentWave = 0;
    uint8 _totalWaves = 5;
    std::pair<uint32, uint32> _waveNPCs = { NPC_ROOKERY_GUARDIAN, NPC_ROOKERY_HATCHER };
    uint32 _bossEntry = NPC_SOLAKAR;
    std::list<ObjectGuid> _spawnedNPCs;
    ObjectGuid _playerGUID;
    bool _eventActive = false;
};

CreatureAI* GetAI_npc_father_flamme_bunny_trigger(Creature* creature)
{
    return new npc_father_flamme_bunny_trigger(creature);
}

/*######
## GO Father Flamme
######*/

struct go_father_flame : public GameObjectAI
{
    go_father_flame(GameObject* go) : GameObjectAI(go) {}

public:
    void OnLootStateChanged(uint32 state, Unit* who) override
    {
        if (state != GO_ACTIVATED || !who || who->GetTypeId() != TYPEID_PLAYER)
            return;

        Player* player = who->ToPlayer();
        if (!player)
            return;

        if (InstanceScript* instance = me->GetInstanceScript())
        {
            EncounterState bossState = instance->GetBossState(DATA_SOLAKAR_FLAMEWREATH);
            if (bossState == IN_PROGRESS || bossState == DONE)
                return;

            instance->SetBossState(DATA_SOLAKAR_FLAMEWREATH, IN_PROGRESS);

            std::list<Creature*> creatureList;
            GetCreatureListWithEntryInGrid(creatureList, me, NPC_FATHER_FLAME_TRIGGER, 50.0f);

            for (Creature* trigger : creatureList)
            {
                if (!trigger || !trigger->AI())
                    continue;

                if (npc_father_flamme_bunny_trigger* waveAI = dynamic_cast<npc_father_flamme_bunny_trigger*>(trigger->AI()))
                    waveAI->StartEvent(player->GetGUID());
            }
        }
    }
};

GameObjectAI* GetAI_go_father_flame(GameObject* go)
{
    return new go_father_flame(go);
}

void AddSC_boss_solakar_flamewreath()
{
    RegisterCreatureAI(boss_solakar_flamewreath);
    RegisterCreatureAI(npc_rookery_hatcher);
    RegisterCreatureAI(npc_father_flamme_bunny_trigger);
    RegisterGameObjectAI(go_father_flame);
}
