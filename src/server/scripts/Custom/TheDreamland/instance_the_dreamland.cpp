#include "InstanceScript.h"
#include "the_dreamland.h"
#include "Player.h"
#include "QuestDef.h"
#include "ObjectAccessor.h"
#include "TemporarySummon.h"

Position const NPC1_POS = { 3140.97f, 518.602f, 72.8862f, 1.10298f };
Position const NPC2_POS = { 3140.97f, 518.602f, 72.8862f, 1.10298f };

class instance_the_dreamland : public InstanceMapScript
{
public:
    instance_the_dreamland() : InstanceMapScript(TheDreamlandScriptName, 731) {}

    struct instance_the_dreamland_InstanceScript : public InstanceScript
    {
        instance_the_dreamland_InstanceScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
        }

        uint32 questId = 302043;               // Quest-ID
        uint32 phaseMaskWithQuest = 2;         // Phase, wenn Quest aktiv und unvollständig
        std::set<ObjectGuid> trackedPlayers;   // Typensicherer

        ObjectGuid npc1Guid;
        ObjectGuid npc2Guid;

        void OnPlayerEnter(Player* player) override
        {
            if (!player)
                return;

            trackedPlayers.insert(player->GetGUID());
            UpdatePlayerPhase(player);

            // NPCs spawnen nur, wenn sie noch nicht existieren
            if (npc1Guid.IsEmpty() && npc2Guid.IsEmpty())
            {
                if (player->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE)
                {
                    if (TempSummon* npc1 = player->SummonCreature(100253, NPC1_POS, TEMPSUMMON_MANUAL_DESPAWN))
                    {
                        npc1->SetPhaseMask(phaseMaskWithQuest, true);
                        npc1Guid = npc1->GetGUID();
                    }

                    if (TempSummon* npc2 = player->SummonCreature(100306, NPC2_POS, TEMPSUMMON_MANUAL_DESPAWN))
                    {
                        npc2->SetPhaseMask(phaseMaskWithQuest, true);
                        npc2Guid = npc2->GetGUID();
                    }
                }
            }
        }

        void OnPlayerLeave(Player* player) override
        {
            if (!player)
                return;

            trackedPlayers.erase(player->GetGUID());
            player->SetPhaseMask(1, true); // Standardphase zurücksetzen
        }

        void Update(uint32 /*diff*/) override
        {
            for (auto const& guid : trackedPlayers)
            {
                if (Player* player = ObjectAccessor::FindPlayer(guid))
                    UpdatePlayerPhase(player);
            }
        }

        void UpdatePlayerPhase(Player* player)
        {
            if (!player)
                return;

            if (player->GetQuestStatus(questId) == QUEST_STATUS_INCOMPLETE)
                player->SetPhaseMask(phaseMaskWithQuest, true);
            else
                player->SetPhaseMask(1, true);
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_the_dreamland_InstanceScript(map);
    }
};

void AddSC_instance_the_dreamland()
{
    new instance_the_dreamland();
}
