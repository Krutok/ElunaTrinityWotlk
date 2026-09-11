#include "ScriptMgr.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "Vehicle.h"
#include "Timer.h"
#include "ScriptedCreature.h"

struct MySpawnData
{
    uint32 entry;
    Position pos;

    MySpawnData(uint32 entry_, float x, float y, float z, float o)
        : entry(entry_)
    {
        pos.Relocate(x, y, z, o);
    }
};

class npc_nergeld : public CreatureScript
{
public:
    npc_nergeld() : CreatureScript("npc_nergeld") {}

    struct npc_nergeldAI : public ScriptedAI
    {
        npc_nergeldAI(Creature* creature) : ScriptedAI(creature), eventStarted(false), talked0(false)
        {
            // Beim Spawn direkt Spell 59037 auf sich selbst wirken
            if (me)
                me->CastSpell(me, 59037, true);
        }

        EventMap events;
        bool eventStarted;
        bool talked0;

        std::vector<MySpawnData> wave1;
        std::vector<MySpawnData> wave2;
        std::vector<MySpawnData> wave3;
        std::vector<MySpawnData> finalWave;

        std::list<Creature*> spawnedNpcs;

        void Reset() override
        {
            events.Reset();
            eventStarted = false;
            talked0 = false;

            ClearSpawnedNpcs();

            wave1 = {
                MySpawnData(30471, 7993.9f, 3336.91f, 632.396f, 0.14577f),
                MySpawnData(30471, 8003.72f, 3323.56f, 632.396f, 0.648783f),
                MySpawnData(30471, 8026.94f, 3307.58f, 632.396f, 1.48207f),
                MySpawnData(30471, 8001.77f, 3306.38f, 632.396f, 0.863447f),
                MySpawnData(30471, 7987.9f, 3308.9f, 632.396f, 0.68058f),
                MySpawnData(30471, 8016.52f, 3318.92f, 632.396f, 0.940311f)
            };

            wave2 = {
                MySpawnData(30432, 7996.66f, 3308.78f, 632.396f, 0.773231f),
                MySpawnData(30432, 8011.71f, 3315.36f, 632.396f, 0.901169f),
                MySpawnData(30471, 8000.67f, 3317.23f, 632.396f, 0.710591f),
                MySpawnData(30471, 8025.24f, 3313.55f, 632.396f, 1.28693f),
                MySpawnData(30471, 8007.71f, 3337.13f, 632.396f, 0.407285f),
                MySpawnData(30471, 8009.92f, 3319.81f, 632.396f, 0.804842f)
            };

            wave3 = {
                MySpawnData(30432, 8021.79f, 3312.45f, 632.396f, 1.13086f),
                MySpawnData(30432, 8001.36f, 3332.71f, 632.396f, 0.443351f),
                MySpawnData(30432, 7999.22f, 3302.52f, 632.396f, 0.872342f),
                MySpawnData(30471, 8000.5f, 3345.77f, 632.396f, 5.82389f),
                MySpawnData(30471, 8001.77f, 3311.95f, 632.396f, 0.797157f),
                MySpawnData(30471, 8012.21f, 3325.82f, 632.396f, 0.737667f)
            };

            finalWave = {
                MySpawnData(30404, 7985.9f, 3296.68f, 632.479f, 0.837758f),
                MySpawnData(30432, 7982.59f, 3301.81f, 632.479f, 0.698132f),
                MySpawnData(30432, 7991.37f, 3293.51f, 632.479f, 0.907571f)
            };
        }

        void SpawnWave(std::vector<MySpawnData>& wave)
        {
            for (auto const& data : wave)
            {
                if (Creature* summon = me->SummonCreature(data.entry,
                    data.pos.GetPositionX(),
                    data.pos.GetPositionY(),
                    data.pos.GetPositionZ(),
                    data.pos.GetOrientation(),
                    TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 60s))
                {
                    spawnedNpcs.push_back(summon);
                }
            }
        }

        bool AreAllSpawnedDead() const
        {
            for (Creature* npc : spawnedNpcs)
                if (npc && !npc->isDead())
                    return false;

            return true;
        }

        void ClearSpawnedNpcs()
        {
            for (Creature* npc : spawnedNpcs)
                if (npc)
                    npc->DespawnOrUnsummon();

            spawnedNpcs.clear();
        }

        void OnCharmed(bool /*isNew*/) override {}

        void PassengerBoarded(Unit* who, int8 /*seatId*/, bool apply) override
        {
            if (!who->IsPlayer())
                return;

            if (apply) // Spieler steigt ein
            {
                if (!talked0)
                {
                    Talk(0);
                    me->SetFaction(who->GetFaction());
                    talked0 = true;

                    eventStarted = true;
                    events.ScheduleEvent(1, 5s);
                }

                // Entferne Spell 59037 wenn Spieler einsteigt
                if (me->HasAura(59037))
                    me->RemoveAura(59037);
            }
            else // Spieler steigt aus
            {
                eventStarted = false;
                talked0 = false;
                events.Reset();

                ClearSpawnedNpcs();

                // Falls du m chtest, Spell 59037 beim Aussteigen wieder draufsetzen:
                if (!me->HasAura(29266))
                    me->CastSpell(me, 29266, true);
                Talk(1);
                me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE);
                me->DespawnOrUnsummon(10s, 20s);
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (!eventStarted)
                return;

            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                if (!AreAllSpawnedDead())
                {
                    events.ScheduleEvent(eventId, 1s);
                    return;
                }

                spawnedNpcs.clear();

                switch (eventId)
                {
                case 1: SpawnWave(wave1); break;
                case 2: SpawnWave(wave2); break;
                case 3: SpawnWave(wave3); break;
                case 4: SpawnWave(finalWave); break;
                }

                if (eventId < 4)
                    events.ScheduleEvent(eventId + 1, 1s);
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_nergeldAI(creature);
    }
};

void AddSC_npc_nergeld()
{
    new npc_nergeld();
}
