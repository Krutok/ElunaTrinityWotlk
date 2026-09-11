#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-source-encoding"
#include "DatabaseEnv.h"
#include "ScriptedGossip.h"
#include "WorldSession.h"
#include "ObjectMgr.h"
#include "GameObject.h"
#include "Group.h"

class DespawnChest : public BasicEvent
{
public:
    DespawnChest(Player* plr, ObjectGuid guid) : BasicEvent(), spawner(plr), chest(guid) { }

    bool Execute(uint64 /*time_t*/, uint32 /*time_e*/) override
    {
        if (auto gob = ObjectAccessor::GetGameObject(*spawner, chest))
            if (gob->IsInWorld())
                gob->DespawnOrUnsummon();

        return true;
    }

private:
    Player* spawner;
    ObjectGuid chest;
};

namespace
{
    enum class GossipOptions : uint32
    {
        OptionSummon,
        OptionCloseMenu,
    };

    enum Events
    {
        EVENT_DESPAWN_AND_CLEAR = 1,
        EVENT_SUMMON_BOSS,
        EVENT_DESPAWN_PORTAL
    };

    enum MovePoints
    {
        WALK_POINT = 1,
    };

    constexpr uint32 textId = 66000;
    constexpr uint32 portalCreature = 36737;

    std::unordered_map<uint32, ObjectGuid> SpawnedBossByPlayer;
    std::unordered_map<uint32 /*quest*/, std::pair<uint32 /*boss*/, std::string /*name*/>> BossInfoByQuest;
    std::unordered_map<uint32, std::pair<std::string /*bossWarning*/, uint32/*chestID*/>> bossInfo;
    std::vector<ObjectGuid> gateGuids;

    void LoadQuestBossData()
    {
        BossInfoByQuest.clear();
        bossInfo.clear();
        auto result = WorldDatabase.Query("SELECT quest_id, boss_id, boss_name, boss_info, boss_loot_chest FROM z_boss_spawning_system");
        if (!result)
            return;
        do
        {
            auto data = result->Fetch();
            BossInfoByQuest.insert({ data[0].GetUInt32(), std::make_pair(data[1].GetUInt32(), data[2].GetString()) });
            bossInfo.insert({ data[1].GetUInt32(), std::make_pair(data[3].GetString(), data[4].GetUInt32()) });
        } while (result->NextRow());
    }

    GameObject* spawnGobHelper(Player* spawner, uint32 objectId, Position pos, bool isGate = false, float rot3 = 0.0f, float rot4 = 0.0f)
    {
        GameObject* go = new GameObject();
        QuaternionData rot;
        if (isGate)
            rot = QuaternionData(0.0f, 0.0f, rot3, rot4);
        else
            rot = QuaternionData::fromEulerAnglesZYX(0.f, 0.f, 0.f);

        auto map = spawner->GetMap();
        if (!go->Create(map->GenerateLowGuid<HighGuid::GameObject>(), objectId, map, PHASEMASK_NORMAL, pos, rot, 255, GO_STATE_READY))
        {
            delete go;
            return nullptr;
        }
        map->AddToMap(go);
        auto guid = go->GetSpawnId();
        GameObjectData& data = sObjectMgr->NewOrExistGameObjectData(guid);
        data.spawnId = guid;
        data.id = objectId;
        data.dbData = false;
        data.spawntimesecs = 0;
        data.spawnGroupData = sObjectMgr->GetLegacySpawnGroup();
        go->SetLootRecipient(spawner, spawner->GetGroup());
        if (!isGate)
            go->m_Events.AddEvent(new DespawnChest(spawner, go->GetGUID()), go->m_Events.CalculateTime(Milliseconds(2 * MINUTE * IN_MILLISECONDS)));

        sObjectMgr->AddGameobjectToGrid(guid, &data);

        return go;
    }

}

struct npc_world_boss_spawner : public ScriptedAI
{
public:
    npc_world_boss_spawner(Creature* creature)
        : ScriptedAI(creature)
    {}

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);

        while (auto eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_DESPAWN_AND_CLEAR:
            {
                if (SpawnedBossByPlayer.find(plrSummoner->GetGUID().GetCounter()) != SpawnedBossByPlayer.end())
                    if (auto creature = ObjectAccessor::GetCreature(*me, SpawnedBossByPlayer.at(plrSummoner->GetGUID().GetCounter())))
                    {
                        if (creature->IsInWorld())
                            creature->DespawnOrUnsummon();
                    }

                bossId = 0;
                SpawnedBossByPlayer.erase(plrSummoner->GetGUID().GetCounter());
                events.Reset();
            }
            break;

            case EVENT_SUMMON_BOSS:
            {
                summonedBoss = me->GetMap()->SummonCreature(bossId, spawnPosition, nullptr, 6 * MINUTE * IN_MILLISECONDS);
                summonedBoss->SetTempSummonType(TEMPSUMMON_CORPSE_TIMED_DESPAWN);
                summonedBoss->Yell("Ihr wagt es mich zu rufen? Das werdet ihr bitter bereuen.", Language::LANG_UNIVERSAL);
                summonedBoss->SetWalk(true);
                summonedBoss->GetMotionMaster()->MovePoint(WALK_POINT, walkPosition);
                SpawnedBossByPlayer[plrSummoner->GetGUID().GetCounter()] = summonedBoss->GetGUID();

                for (const auto& [pos, rot3, rot4] : GatePositions)
                {
                    if (auto gob = spawnGobHelper(plrSummoner, gateId, pos, true, rot3, rot4))
                        gateGuids.push_back(gob->GetGUID());
                }
            }
            break;

            case EVENT_DESPAWN_PORTAL:
            {
                portal->DespawnOrUnsummon();
            }
            break;
            }
        }
    }

    bool OnGossipHello(Player* player) override
    {
        if (SpawnedBossByPlayer.find(player->GetGUID().GetCounter()) != SpawnedBossByPlayer.end())
        {
            player->GetSession()->SendNotification("Du hast bereits einen Boss beschworen");
            CloseGossipMenuFor(player);
            return true;
        }

        if (SpawnedBossByPlayer.size() >= 1)
        {
            player->GetSession()->SendNotification("Ein Kampf ist bereits im gange, daher gedulde dich ein wenig.");
            CloseGossipMenuFor(player);
            return true;
        }

        player->PrepareQuestMenu(me->GetGUID());
        ClearGossipMenuFor(player);
        for (const auto& info : BossInfoByQuest)
        {
            for (uint16 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
                if (player->GetQuestSlotQuestId(i) == info.first)
                {
                    auto pProto = sObjectMgr->GetQuestTemplate(info.first);
                    if (!pProto)
                        continue;
                    if (player->CanRewardQuest(pProto, false))
                        continue;

                    AddGossipItemFor(player, GossipOptionIcon(0), info.second.second, info.second.first, static_cast<uint32>(GossipOptions::OptionSummon));
                }
        }
        AddGossipItemFor(player, GossipOptionIcon(0), "Zurück", GOSSIP_SENDER_MAIN, static_cast<uint32>(GossipOptions::OptionCloseMenu));
        SendGossipMenuFor(player, textId, me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*send*/, uint32 gossipActionInfo) override
    {
        if (!SpawnedBossByPlayer.empty())
        {
            CloseGossipMenuFor(player);
            return false;
        }

        auto action = player->PlayerTalkClass->GetGossipOptionAction(gossipActionInfo);
        auto bossEntry = player->PlayerTalkClass->GetGossipOptionSender(gossipActionInfo);
        auto sourcedEnum = static_cast<GossipOptions>(action);

        switch (sourcedEnum)
        {
        case GossipOptions::OptionSummon:
        {
            auto visualPortal = me->GetMap()->SummonCreature(portalCreature, spawnPosition, nullptr, 6 * MINUTE * IN_MILLISECONDS);
            visualPortal->SetObjectScale(0.3f);
            visualPortal->CastSpell(visualPortal, 51807);
            events.ScheduleEvent(EVENT_DESPAWN_PORTAL, 20s);
            plrSummoner = player;

            bossId = bossEntry;
            events.ScheduleEvent(EVENT_SUMMON_BOSS, 5s);

            SpawnedBossByPlayer.insert({ plrSummoner->GetGUID().GetCounter(), player->GetGUID() });

            me->TextEmote(bossInfo[bossEntry].first, player, true);
            events.ScheduleEvent(EVENT_DESPAWN_AND_CLEAR, 25min);

            portal = visualPortal;
            CloseGossipMenuFor(player);
        }
        break;

        case GossipOptions::OptionCloseMenu:
            CloseGossipMenuFor(player);
            break;
        }
        return true;
    }

private:
    // This is the location where the boss spawns
    Position spawnPosition{ 5762.110840f /*pos_x*/ , -2959.337646f /*pos_y*/ , 274.023773f /*pos_z*/ , 5.165698f /*orient*/ };
    // This is the location of the portal, and the location to which the boss will walk after spawning
    Position walkPosition{ 5765.526367f /*pos_x*/ , -2966.348389f /*pos_y*/ , 273.985626f /*pos_z*/ , 5.146064f /*orient*/ };
    EventMap events;
    TempSummon* summonedBoss = nullptr;
    uint32 bossId = 0;
    TempSummon* portal = nullptr;
    Player* plrSummoner = nullptr;
    static inline const std::array<std::tuple<Position, float, float>, 4> GatePositions =
    {
        std::make_tuple(Position(5749.801758, -3016.970947, 274.050873, 5.244368), -0.837766f, -0.546029f),
        std::make_tuple(Position(5742.445801, -3001.312500, 273.897400, 5.282509), -0.541633, 0.840615),
        std::make_tuple(Position(5810.437012, -2968.844727, 273.933228, 5.113654), -0.54035, 0.84144),
        std::make_tuple(Position(5817.977539, -2984.217041, 273.823059, 2.082009), -0.840118, -0.542404)
    };
    uint32 gateId = 600003;
};

class SpawnLoot : public BasicEvent
{
public:
    SpawnLoot(Player* plr, Position pos, uint32 chest) : BasicEvent(), player(plr), position(pos), chestId(chest) {}

    bool Execute(uint64 /*time_t*/, uint32 /*time_e*/) override
    {
        spawnGobHelper(player, chestId, position);
        return true;
    }

protected:
    Player* player;
    Position position;
    uint32 chestId;
};

class TimedDespawn : public BasicEvent
{
public:
    TimedDespawn(Creature* cr, bool spawn = false) : BasicEvent(), boss(cr), spawned(spawn) {}

    bool Execute(uint64 /*t*/, uint32 /*e*/) override
    {
        for (const auto& guids : gateGuids)
        {
            if (boss)
            {
                if (GameObject* gob = ObjectAccessor::GetGameObject(*boss, guids))
                {
                    gob->DespawnOrUnsummon();
                }
            }
        }
        gateGuids.clear();

        if (boss && boss->IsInWorld())
            boss->DespawnOrUnsummon();

        SpawnedBossByPlayer.clear();
        spawned = false;
        return true;
    }

private:
    Creature* boss;
    bool spawned;
};
class ps_boss_loot : public PlayerScript
{
public:
    ps_boss_loot() : PlayerScript("ps_boss_loot") {}

    void OnPlayerKilledByCreature(Creature* creature, Player* player) override
    {
        uint32 aliveCount = 0;
        bool foundPuller = 0;

        if (SpawnedBossByPlayer.find(player->GetGUID().GetCounter()) != SpawnedBossByPlayer.end())
        {
            if (creature->GetGUID() == SpawnedBossByPlayer.at(player->GetGUID().GetCounter()))
            {
                if (Group* group = player->GetGroup())
                {
                    for (auto itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
                    {
                        if (auto groupMember = itr->GetSource())
                        {
                            if (!creature)
                                return;

                            if (groupMember->GetDistance(creature) < 150.0f && groupMember->IsAlive())
                                aliveCount++;

                            if (groupMember == creature->GetLootRecipient())
                                foundPuller = true;

                            if (aliveCount == 0 && foundPuller)
                                creature->m_Events.AddEvent(new TimedDespawn(creature), creature->m_Events.CalculateTime(Milliseconds(5 * IN_MILLISECONDS)));
                        }
                    }
                }
            }
        }
    }

    void OnCreatureKill(Player* player, Creature* creature) override
    {
        if (SpawnedBossByPlayer.find(player->GetGUID().GetCounter()) != SpawnedBossByPlayer.end())
        {
            if (creature->GetGUID() != SpawnedBossByPlayer[player->GetGUID().GetCounter()])
                return;

            for (const auto& guids : gateGuids)
            {
                if (player)
                {
                    if (GameObject* gob = ObjectAccessor::GetGameObject(*player, guids))
                    {
                        gob->DespawnOrUnsummon();
                    }
                }
            }
            gateGuids.clear();

            for (const auto& data : BossInfoByQuest)
            {
                if (creature->GetEntry() == data.second.first)
                {
                    player->CompleteQuest(data.first);
                    player->RewardQuest(sObjectMgr->GetQuestTemplate(data.first), 0, nullptr, true);

                    if (Group* group = player->GetGroup())
                    {
                        for (auto itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
                        {
                            if (auto groupMember = itr->GetSource())
                            {
                                if (groupMember == player)
                                    continue;

                                groupMember->CompleteQuest(data.first);
                                groupMember->RewardQuest(sObjectMgr->GetQuestTemplate(data.first), 0, nullptr, true);
                            }
                        }
                    }

                    auto chest = bossInfo[data.second.first].second;
                    if (player)
                    {
                        player->m_Events.AddEvent(new SpawnLoot(player, chestSpawn, chest),
                            player->m_Events.CalculateTime(Milliseconds(5 * IN_MILLISECONDS)));
                    }

                    player->GetSession()->SendAreaTriggerMessage("Herzlichen Gl�ckwunsch, Sie haben diesen Boss besiegt. Eine Truhe mit Ihrer Beute wird in 5 Sekunden erscheinen.");

                }
            }
        }
    }

    void OnQuestStatusChange(Player* player, uint32 /*quest*/) override
    {
        auto guid = player->GetGUID().GetCounter();
        for (const auto& guids : gateGuids)
        {
            if (player)
            {
                if (GameObject* gob = ObjectAccessor::GetGameObject(*player, guids))
                {
                    gob->DespawnOrUnsummon();
                }
            }
        }
        gateGuids.clear();

        if (SpawnedBossByPlayer.find(guid) != SpawnedBossByPlayer.end())
        {
            for (const auto& data : BossInfoByQuest)
            {
                bool found = false;
                for (uint16 i = 0; i < MAX_QUEST_LOG_SIZE; ++i)
                {
                    if (player && player->GetQuestSlotQuestId(i) == data.first)
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                {
                    if (auto boss = ObjectAccessor::GetCreature(*player, SpawnedBossByPlayer.at(guid)))
                    {
                        boss->m_Events.AddEvent(new TimedDespawn(boss), boss->m_Events.CalculateTime(Milliseconds(5 * IN_MILLISECONDS)));
                        break;
                    }
                }
            }
        }
    }

private:
    // Chest spawn location
    Position chestSpawn{ 5771.977539f/*pos_x*/ , -2979.055664f /*pos_y*/ , 273.080444f /*pos_z*/ , 5.177478f /*orient*/ };
};

class ws_boss_spawner_loader : public WorldScript
{
public:
    ws_boss_spawner_loader() : WorldScript("ws_boss_spawner_loader") {}

    void OnConfigLoad(bool /*reload*/) override
    {
        LoadQuestBossData();
    }
};

void AddSC_world_boss_spawner()
{
    RegisterCreatureAI(npc_world_boss_spawner);
    new ws_boss_spawner_loader();
    new ps_boss_loot();
}
