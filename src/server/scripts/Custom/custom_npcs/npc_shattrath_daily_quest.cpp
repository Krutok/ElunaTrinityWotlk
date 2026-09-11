#include "ScriptedCreature.h"
#include "ObjectAccessor.h"
#include "ScriptMgr.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"

enum ShattrathQuests
{
    // QuestID : Creature Template ID
    // Heroic Daily Quests
    QUEST_H_NAZZAN              = 11354, // 24410
    QUEST_H_KELIDAN             = 11362, // 24413
    QUEST_H_BLADEFIST           = 11363, // 24414
    QUEST_H_QUAG                = 11368, // 24419
    QUEST_H_BLACKSTALKER        = 11369, // 24420
    QUEST_H_WARLORD             = 11370, // 24421
    QUEST_H_IKISS               = 11372, // 24422
    QUEST_H_SHAFFAR             = 11373, // 24423
    QUEST_H_EXARCH              = 11374, // 24424
    QUEST_H_MURMUR              = 11375, // 24425
    QUEST_H_EPOCH               = 11378, // 24427
    QUEST_H_AEONUS              = 11382, // 24428
    QUEST_H_WARP                = 11384, // 24431
    QUEST_H_CALCULATOR          = 11386, // 21504
    QUEST_H_SKYRISS             = 11388, // 24435
    QUEST_H_KAEL                = 11499, // 24855
    // Normal Daily Quests
    QUEST_N_CENTURIONS          = 11364, // 24411
    QUEST_N_MYRMIDONS           = 11371, // 24415
    QUEST_N_INSTRUCTORS         = 11376, // 24426
    QUEST_N_LORDS               = 11383, // 24429
    QUEST_N_CHANNELERS          = 11385, // 24430
    QUEST_N_DESTROYERS          = 11387, // 24432
    QUEST_N_SENTINELS           = 11389, // 24434
    QUEST_N_SISTERS             = 11500, // 24854

    ACTION_UPDATE_QUEST_STATUS   = 1,

    POOL_SHATTRATH_DAILY_H      = 356,
    POOL_SHATTRATH_DAILY_N      = 357,

    // Image NPCs
    NPC_SHATTRATH_DAILY_H       = 24854,
    NPC_SHATTRATH_DAILY_N       = 24410
};

struct npc_shattrath_daily_quest : public CreatureAI
{
    npc_shattrath_daily_quest(Creature* c) : CreatureAI(c) {}

void UpdateAI(uint32 /*diff*/) override { }

void Reset() override
{
    DoAction(ACTION_UPDATE_QUEST_STATUS);
}

    void DoAction(int32 action) override
    {
        if (action == ACTION_UPDATE_QUEST_STATUS)
        {
            uint32 creature = me->GetEntry();
            QueryResult result = CharacterDatabase.PQuery("SELECT `quest_id` FROM `pool_quest_save` WHERE `pool_id` = '{}'", creature == NPC_SHATTRATH_DAILY_H ? POOL_SHATTRATH_DAILY_H : POOL_SHATTRATH_DAILY_N);
            if (result)
            {
                Field* fields = result->Fetch();
                uint32 quest_id = fields[0].GetUInt32();
		        uint32 templateID = 0;

                if (creature == NPC_SHATTRATH_DAILY_H)
                {
                    switch (quest_id)
                    {
                        case QUEST_H_NAZZAN:
                            templateID = 24410;
                            break;
                        case QUEST_H_KELIDAN:
                            templateID = 24413;
                            break;
                        case QUEST_H_BLADEFIST:
                            templateID = 24414;
                            break;
                        case QUEST_H_QUAG:
                            templateID = 24419;
                            break;
                        case QUEST_H_BLACKSTALKER:
                            templateID = 24420;
                            break;
                        case QUEST_H_WARLORD:
                            templateID = 24421;
                            break;
                        case QUEST_H_IKISS:
                            templateID = 24422;
                            break;
                        case QUEST_H_SHAFFAR:
                            templateID = 24423;
                            break;
                        case QUEST_H_EXARCH:
                            templateID = 24424;
                            break;
                        case QUEST_H_MURMUR:
                            templateID = 24425;
                            break;
                        case QUEST_H_EPOCH:
                            templateID = 24427;
                            break;
                        case QUEST_H_AEONUS:
                            templateID = 24428;
                            break;
                        case QUEST_H_WARP:
                            templateID = 24431;
                            break;
                        case QUEST_H_CALCULATOR:
                            templateID = 21504;
                            break;
                        case QUEST_H_SKYRISS:
                            templateID = 24435;
                            break;
                        case QUEST_H_KAEL:
                            templateID = 24855;
                            break;
                        default:
                            break;
                    }
                }

                if (creature == NPC_SHATTRATH_DAILY_N)
                {
                    switch (quest_id)
                    {
                        case QUEST_N_CENTURIONS:
                            templateID = 24411;
                            break;
                        case QUEST_N_MYRMIDONS:
                            templateID = 24415;
                            break;
                        case QUEST_N_INSTRUCTORS:
                            templateID = 24426;
                            break;
                        case QUEST_N_LORDS:
                            templateID = 24429;
                            break;
                        case QUEST_N_CHANNELERS:
                            templateID = 24430;
                            break;
                        case QUEST_N_DESTROYERS:
                            templateID = 24432;
                            break;
                        case QUEST_N_SENTINELS:
                            templateID = 24434;
                            break;
                        case QUEST_N_SISTERS:
                            templateID = 24854;
                            break;
                        default:
                            break;
                    }
                }

                if (CreatureTemplate const* ci = sObjectMgr->GetCreatureTemplate(templateID))
            {
                uint32 displayId = sObjectMgr->ChooseDisplayId(ci);
                me->SetDisplayId(displayId);
            }

            }
        }
    }
};

void AddSC_npc_shattrath_daily_quest()
{
RegisterCreatureAI(npc_shattrath_daily_quest);
}
