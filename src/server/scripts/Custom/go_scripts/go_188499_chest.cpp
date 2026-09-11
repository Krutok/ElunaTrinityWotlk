#include "GameObjectAI.h"
#include "GameObject.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include <list>

/*######
## GO Drakkari Canopic Jar (Quest: It Takes Guts.... 12116)
######*/

enum it_takes_guts
{
    ANCIENT_DRAKKARI_SOOTHSAYER = 26812,
    ANCIENT_DRAKKARI_WARMONGER = 26811,
    NPC_ANCIENT_DRAKKARI_TALK = 1
};

class go_188499_chest : public GameObjectScript
{
public:
    go_188499_chest() : GameObjectScript("go_188499_chest") {}

    struct go_188499_chestAI : public GameObjectAI
    {
        go_188499_chestAI(GameObject* go) : GameObjectAI(go) {}

        void OnLootStateChanged(uint32 state, Unit* who) override
        {
            if (state != GO_ACTIVATED || !who || who->GetTypeId() != TYPEID_PLAYER)
                return;

            Player* player = who->ToPlayer();
            if (!player)
                return;

            constexpr uint32 entries[] = { ANCIENT_DRAKKARI_SOOTHSAYER, ANCIENT_DRAKKARI_WARMONGER };
            constexpr size_t entryCount = sizeof(entries) / sizeof(entries[0]);

            std::list<Creature*> npcList;

            for (size_t i = 0; i < entryCount; ++i)
            {
                std::list<Creature*> tempList;
                GetCreatureListWithEntryInGrid(tempList, me, entries[i], 30.0f);

                for (Creature* c : tempList)
                    if (c && c->IsAlive())
                        npcList.push_back(c);
            }

            if (npcList.empty())
                return;

            size_t listSize = npcList.size();
            size_t randIndex = urand(0, listSize - 1);

            auto it = npcList.begin();
            std::advance(it, randIndex);

            Creature* target = *it;
            if (target)
            {
                target->SetFaction(14);
                target->AI()->AttackStart(player);
                target->AI()->Talk(NPC_ANCIENT_DRAKKARI_TALK);
            }
        }
    };

    GameObjectAI* GetAI(GameObject* go) const override
    {
        return new go_188499_chestAI(go);
    }
};

void AddSC_go_188499_chest()
{
    new go_188499_chest();
}
