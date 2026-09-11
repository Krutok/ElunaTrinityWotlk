#include "ScriptMgr.h"
#include "CellImpl.h"
#include "Containers.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedEscortAI.h"
#include "ScriptedGossip.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "WorldSession.h"

enum ExorcismSpells
{
    SPELL_JULES_GOES_PRONE = 39283,
    SPELL_JULES_THREATENS_AURA = 39284,
    SPELL_JULES_GOES_UPRIGHT = 39294,
    SPELL_JULES_VOMITS_AURA = 39295,

    SPELL_BARADAS_COMMAND = 39277,
    SPELL_BARADA_FALTERS = 39278,
};

enum ExorcismTexts
{
    SAY_BARADA_1 = 0,
    SAY_BARADA_2 = 1,
    SAY_BARADA_3 = 2,
    SAY_BARADA_4 = 3,
    SAY_BARADA_5 = 4,
    SAY_BARADA_6 = 5,
    SAY_BARADA_7 = 6,
    SAY_BARADA_8 = 7,

    SAY_JULES_1 = 0,
    SAY_JULES_2 = 1,
    SAY_JULES_3 = 2,
    SAY_JULES_4 = 3,
    SAY_JULES_5 = 4,
};

Position const exorcismPos[12] =
{
    { -707.42f, 2747.98f, 101.59f, 4.577416f },
    { -711.20f, 2747.75f, 101.59f, 1.51f },
    { -710.84f, 2749.55f, 101.59f, 1.63f },
    { -710.332f, 2754.394f, 102.948f, 3.207566f },
    { -714.261f, 2747.754f, 103.391f, 0.0f },
    { -713.113f, 2750.194f, 103.391f, 0.0f },
    { -710.385f, 2750.896f, 103.391f, 0.0f },
    { -708.309f, 2750.062f, 103.391f, 0.0f },
    { -707.401f, 2747.696f, 103.391f, 0.0f },
    { -708.591f, 2745.266f, 103.391f, 0.0f },
    { -710.597f, 2744.035f, 103.391f, 0.0f },
    { -713.089f, 2745.302f, 103.391f, 0.0f },
};

enum ExorcismMisc
{
    NPC_COLONEL_JULES                           = 22432,
    NPC_DARKNESS_RELEASED                       = 22507,
    NPC_FOUL_PURGE                              = 22506,
    NPC_THE_EXORCISM_BUBBLING_SLIMER_BUNNY      = 22505,

    ACTION_START_EVENT                          = 1,
    ACTION_JULES_HOVER                          = 2,
    ACTION_JULES_FLIGHT                         = 3,
    ACTION_JULES_MOVE_HOME                      = 4,
};

enum ExorcismEvents
{
    EVENT_BARADAS_TALK = 1,
    EVENT_RESET = 2
};

/*######
## npc_colonel_jules
######*/

struct npc_colonel_jules_custom : public ScriptedAI
{
    npc_colonel_jules_custom(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        events.Reset();
        point = 4;
        wpreached = false;
        success = false;
        me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
        me->AddAura(SPELL_JULES_GOES_PRONE, me);
    }

    void DoAction(int32 action) override
    {
        switch (action)
        {
        case ACTION_JULES_HOVER:
            me->AddAura(SPELL_JULES_THREATENS_AURA, me);
            me->SetCanFly(true);
            me->SetWalk(true);
            me->SetFacingTo(3.207566f);
            me->GetMotionMaster()->MoveJump(exorcismPos[3], 2.0f, 2.0f);
            success = false;
            break;
        case ACTION_JULES_FLIGHT:
            me->RemoveAura(SPELL_JULES_GOES_PRONE);
            me->AddAura(SPELL_JULES_GOES_UPRIGHT, me);
            me->AddAura(SPELL_JULES_VOMITS_AURA, me);
            me->SetWalk(true);
            wpreached = true;
            me->GetMotionMaster()->MovePoint(point, exorcismPos[4]);
            break;
        case ACTION_JULES_MOVE_HOME:
            wpreached = false;
            me->SetWalk(true);
            me->GetMotionMaster()->MoveTargetedHome();
            me->SetCanFly(false);
            me->AddAura(SPELL_JULES_GOES_PRONE, me);
            me->RemoveAura(SPELL_JULES_GOES_UPRIGHT);
            me->RemoveAura(SPELL_JULES_VOMITS_AURA);
            me->RemoveAura(SPELL_JULES_THREATENS_AURA);

            std::list<uint32> despawnEntries = { NPC_DARKNESS_RELEASED, NPC_FOUL_PURGE, NPC_THE_EXORCISM_BUBBLING_SLIMER_BUNNY };
            for (uint32 entry : despawnEntries)
            {
                std::list<Creature*> npcs;
                me->GetCreatureListWithEntryInGrid(npcs, entry, 40.0f);
                for (Creature* npc : npcs)
                    npc->DespawnOrUnsummon();
            }
            break;
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        if (id < 9)
            wpreached = true;

        if (id == 9)
        {
            wpreached = true;
            point = 4;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (wpreached)
        {
            me->GetMotionMaster()->MovePoint(point, exorcismPos[point]);
            point++;
            wpreached = false;
        }

        events.Update(diff);
    }

    bool OnGossipHello(Player* player) override
    {
        if (success)
            player->KilledMonsterCredit(NPC_COLONEL_JULES, ObjectGuid::Empty);

        SendGossipMenuFor(player, player->GetGossipTextId(me), me->GetGUID());
        return true;
    }

private:
    EventMap events;
    uint8 point;
    bool wpreached;
public:
    bool success;
};

/*######
## npc_barada
######*/

struct npc_barada_custom : public ScriptedAI
{
    npc_barada_custom(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        events.Reset();
        step = 0;
        playerGUID.Clear();
        me->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
        me->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        ClearGossipMenuFor(player);
        if (gossipListId == 1)
        {
            if (Creature* jules = ObjectAccessor::GetCreature(*me, julesGUID))
                jules->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);

            player->PlayerTalkClass->SendCloseGossip();
            me->AI()->Talk(SAY_BARADA_1);
            me->AI()->DoAction(ACTION_START_EVENT);
        }
        return false;
    }

    void DoAction(int32 action) override
    {
        if (action != ACTION_START_EVENT)
            return;

        if (Creature* jules = me->FindNearestCreature(NPC_COLONEL_JULES, 20.0f, true))
        {
            julesGUID = jules->GetGUID();
            jules->AI()->Talk(SAY_JULES_1);
        }

        me->SetWalk(true);
        me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
        Talk(SAY_BARADA_2);
        me->SetUnitFlag(UNIT_FLAG_PACIFIED);
        me->GetMotionMaster()->MovePoint(0, exorcismPos[0]);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        if (id == 0)
            me->GetMotionMaster()->MovePoint(1, exorcismPos[1]);
        else if (id == 1)
            me->GetMotionMaster()->MovePoint(2, exorcismPos[2]);
        else if (id == 2)
            events.ScheduleEvent(EVENT_BARADAS_TALK, 2s);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Creature* jules = ObjectAccessor::GetCreature(*me, julesGUID))
        {
            jules->AI()->DoAction(ACTION_JULES_MOVE_HOME);
            jules->RemoveAllAuras();
            jules->AddAura(SPELL_JULES_GOES_PRONE, jules);
        }
        me->DespawnOrUnsummon(1s,10s);
        std::list<uint32> despawnEntries = { NPC_DARKNESS_RELEASED, NPC_FOUL_PURGE, NPC_THE_EXORCISM_BUBBLING_SLIMER_BUNNY };
        for (uint32 entry : despawnEntries)
        {
            std::list<Creature*> npcs;
            me->GetCreatureListWithEntryInGrid(npcs, entry, 40.0f);
            for (Creature* npc : npcs)
                npc->DespawnOrUnsummon();
        }
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_BARADAS_TALK:
                switch (step)
                {
                case 0:
                    me->SetFacingTo(1.513286f);
                    me->HandleEmoteCommand(EMOTE_ONESHOT_KNEEL);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 3s);
                    step++;
                    break;
                case 1:
                    DoCast(SPELL_BARADAS_COMMAND);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 5s);
                    step++;
                    break;
                case 2:
                    Talk(SAY_BARADA_3);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 7s);
                    step++;
                    break;
                case 3:
                    if (Creature* jules = ObjectAccessor::GetCreature(*me, julesGUID))
                        jules->AI()->Talk(SAY_JULES_2);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 18s);
                    step++;
                    break;
                case 4:
                    DoCast(SPELL_BARADA_FALTERS);
                    me->HandleEmoteCommand(EMOTE_STAND_STATE_NONE);
                    if (Creature* jules = ObjectAccessor::GetCreature(*me, julesGUID))
                        jules->AI()->DoAction(ACTION_JULES_HOVER);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 11s);
                    step++;
                    break;
                case 5:
                    if (Creature* jules = ObjectAccessor::GetCreature(*me, julesGUID))
                        jules->AI()->Talk(SAY_JULES_3);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 13s);
                    step++;
                    break;
                case 6:
                    Talk(SAY_BARADA_4);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 5s);
                    step++;
                    break;
                case 7:
                    if (Creature* jules = ObjectAccessor::GetCreature(*me, julesGUID))
                        jules->AI()->Talk(SAY_JULES_3);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 13s);
                    step++;
                    break;
                case 8:
                    Talk(SAY_BARADA_4);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 12s);
                    step++;
                    break;
                case 9:
                    if (Creature* jules = ObjectAccessor::GetCreature(*me, julesGUID))
                        jules->AI()->Talk(SAY_JULES_4);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 12s);
                    step++;
                    break;
                case 10:
                    Talk(SAY_BARADA_4);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 5s);
                    step++;
                    break;
                case 11:
                    if (Creature* jules = ObjectAccessor::GetCreature(*me, julesGUID))
                        jules->AI()->DoAction(ACTION_JULES_FLIGHT);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 10s);
                    step++;
                    break;
                case 12:
                    if (Creature* jules = ObjectAccessor::GetCreature(*me, julesGUID))
                        jules->AI()->Talk(SAY_JULES_4);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 8s);
                    step++;
                    break;
                case 13:
                    Talk(SAY_BARADA_5);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 10s);
                    step++;
                    break;
                case 14:
                    if (Creature* jules = ObjectAccessor::GetCreature(*me, julesGUID))
                        jules->AI()->Talk(SAY_JULES_4);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 10s);
                    step++;
                    break;
                case 15:
                    Talk(SAY_BARADA_6);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 10s);
                    step++;
                    break;
                case 16:
                    if (Creature* jules = ObjectAccessor::GetCreature(*me, julesGUID))
                        jules->AI()->Talk(SAY_JULES_5);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 10s);
                    step++;
                    break;
                case 17:
                    Talk(SAY_BARADA_7);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 10s);
                    step++;
                    break;
                case 18:
                    if (Creature* jules = ObjectAccessor::GetCreature(*me, julesGUID))
                        jules->AI()->Talk(SAY_JULES_3);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 10s);
                    step++;
                    break;
                case 19:
                    Talk(SAY_BARADA_7);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 10s);
                    step++;
                    break;
                case 20:
                    if (Creature* jules = ObjectAccessor::GetCreature(*me, julesGUID))
                        jules->AI()->DoAction(ACTION_JULES_MOVE_HOME);
                    events.ScheduleEvent(EVENT_BARADAS_TALK, 1s);
                    step++;
                    break;
                case 21:
                    if (Creature* jules = ObjectAccessor::GetCreature(*me, julesGUID))
                    {
                        ENSURE_AI(npc_colonel_jules_custom, jules->AI())->success = true;
                        jules->RemoveAllAuras();
                        jules->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                        jules->SetUnitFlag(UNIT_FLAG_STUNNED);
                        jules->AddAura(SPELL_JULES_GOES_PRONE, jules);
                    }
                    me->RemoveAura(SPELL_BARADAS_COMMAND);
                    me->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
                    Talk(SAY_BARADA_8);
                    me->GetMotionMaster()->MoveTargetedHome();
                    EnterEvadeMode();
                    me->SetWalk(true);
                    events.ScheduleEvent(EVENT_RESET, 45s);
                    break;
                }
                break;
            case EVENT_RESET:
                if (Creature* jules = ObjectAccessor::GetCreature(*me, julesGUID))
                {
                    ENSURE_AI(npc_colonel_jules_custom, jules->AI())->success = false;
                    jules->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                    jules->RemoveUnitFlag(UNIT_FLAG_STUNNED);
                }
                break;
            }
        }
    }

private:
    EventMap events;
    uint8 step;
    ObjectGuid julesGUID;
    ObjectGuid playerGUID;
};

void AddSC_npc_barada_custom()
{
    RegisterCreatureAI(npc_colonel_jules_custom);
    RegisterCreatureAI(npc_barada_custom);
}

