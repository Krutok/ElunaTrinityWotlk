#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "DatabaseEnv.h"
#include <cmath>


enum DalaranWeeklyTrigger
{
    NPC_WEEKLY_TRIGGER = 32265,
    WEEKLY_POOL_ID = 5678
};

struct npc_weekly_trigger : public CreatureAI
{
    npc_weekly_trigger(Creature* creature) : CreatureAI(creature), displaySet(false), currentOrientation(0.0f) {}

    void Reset() override
    {
        displaySet = false;
        currentOrientation = 0.0f;

        me->StopMoving();
        me->SetReactState(REACT_PASSIVE);

        uint32 activeQuestId = GetWeeklyQuestFromPool();
        if (!activeQuestId)
            return;

        uint32 displayId = GetDisplayIdForQuest(activeQuestId);
        if (displayId)
        {
            me->SetDisplayId(displayId);
            displaySet = true;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        const float rotationSpeed = 0.3f; // Radiant pro Sekunde (~17°/s)
        float deltaRadians = rotationSpeed * (diff / 1000.0f);

        currentOrientation += deltaRadians;
        if (currentOrientation > 2.0f * M_PI)
            currentOrientation -= 2.0f * M_PI;

        me->SetFacingTo(currentOrientation);
        me->StopMoving();
    }

    uint32 GetWeeklyQuestFromPool()
    {
        QueryResult result = CharacterDatabase.PQuery(
            "SELECT quest_id FROM pool_quest_save WHERE pool_id = {}", WEEKLY_POOL_ID);

        if (!result)
            return 0;

        return result->Fetch()[0].GetUInt32();
    }

    uint32 GetDisplayIdForQuest(uint32 questId)
    {
        QueryResult result = WorldDatabase.PQuery(
            "SELECT display_id FROM custom_weekly_npc_displays WHERE quest_id = {}", questId);

        if (!result)
            return 0;

        return result->Fetch()[0].GetUInt32();
    }
private:
    bool displaySet;
    float currentOrientation;
};

CreatureAI* GetAI_npc_weekly_trigger(Creature* creature)
{
    if (creature->GetEntry() != NPC_WEEKLY_TRIGGER)
        return nullptr;

    return new npc_weekly_trigger(creature);
}

void AddSC_npc_weekly_trigger()
{
    RegisterCreatureAI(npc_weekly_trigger);
}
