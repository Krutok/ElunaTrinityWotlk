#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "ScriptedEscortAI.h"
#include "MotionMaster.h"
#include "EventMap.h"
#include <list>
#include <map>
#include <cmath>
#include "GameObject.h"
#include "GameObjectAI.h"
#include "ScriptSystem.h"
#include "CreatureGroups.h"

enum LetollQuestData
{
    QUEST_DIGGING_THROUGH_BONES = 10922,

    NPC_RESEARCHER = 22464,
    NPC_BONE_SIFTER = 22466,

    SPELL_FUMPER_TRAP = 39217,
};

enum LetollEvents
{
    EVENT_ESCORT_CONTINUE = 1,
    EVENT_TALK_2 = 2,
    EVENT_TALK_3 = 3,
    EVENT_STOP_MINING = 4,
    EVENT_TALK_4 = 5,
    EVENT_TALK_5 = 6,
    EVENT_SPAWN_DRUM = 7,
    EVENT_FOLLOWER_78837_TALK0 = 8,
    EVENT_FOLLOWER_78838_TALK0 = 9,
    EVENT_FOLLOWER_78839_TALK0 = 10,
    EVENT_TALK_7 = 11,
    EVENT_FOLLOWER_78837_TALK3 = 12,
    EVENT_FOLLOWER_78839_TALK4 = 13,
    EVENT_FOLLOWER_78840_TALK5 = 14,
    EVENT_FOLLOWER_78838_TALK6 = 15,
    EVENT_TALK_8 = 16,
    EVENT_LEADER_ATTACK_SPELL = 17,
    EVENT_FOLLOWER_78840_TALK7 = 18,
    EVENT_TALK_9 = 19,
    EVENT_TALK_10 = 20,
    EVENT_CHECK_BONE_SIFTER_DEAD = 21,
    EVENT_TALK_11 = 22,
    EVENT_DESPAWN_ALL = 23
};

class npc_follow_leader : public ScriptedAI
{
public:
    npc_follow_leader(Creature* creature) : ScriptedAI(creature) {}

    void MovementInform(uint32 /*type*/, uint32 pointId) override
    {
        if (pointId == 99)
            me->AddUnitState(UNIT_STATE_NOT_MOVE);
    }
};

class npc_letoll : public EscortAI
{
public:
    npc_letoll(Creature* creature) : EscortAI(creature) {}

    void EnterEvadeMode(EvadeReason why) override
    {
        EscortAI::EnterEvadeMode(why);
        me->SetWalk(false);
    }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() != QUEST_DIGGING_THROUGH_BONES)
            return;

        Talk(0, player);
        me->SetFaction(250);

        _npcs.clear();
        std::list<Creature*> list;
        me->GetCreatureListWithEntryInGrid(list, NPC_RESEARCHER, 100.0f);
        for (Creature* npc : list)
        {
            _npcs[npc->GetSpawnId()] = npc;
            npc->SetFaction(250);
        }
	    LoadPath(179666);
        Start(false, player->GetGUID());
    }

    void WaypointStarted(uint32 nodeId, uint32 /*pathId*/) override
    {
        switch (nodeId)
        {
        case 15:
            for (auto& pair : _npcs)
            {
                Creature* npc = pair.second;
                if (!npc || !npc->IsAlive())
                    continue;

                npc->StopMoving();
                npc->GetMotionMaster()->Clear();
                switch (npc->GetSpawnId())
                {
                case 78837:
                    npc->GetMotionMaster()->MovePoint(99, -3537.43f, 5452.85f, -12.43f, false);
                    break;
                case 78838:
                    npc->GetMotionMaster()->MovePoint(99, -3543.1f, 5468.93f, -12.31f, false);
                    break;
                case 78839:
                    npc->GetMotionMaster()->MovePoint(99, -3565.84f, 5460.68f, -6.35f, false);
                    break;
                case 78840:
                    npc->GetMotionMaster()->MovePoint(99, -3549.48f, 5470.51f, -10.15f, false);
                    break;
                }

                npc->SetWalk(true);
                npc->SetEmoteState(EMOTE_STATE_WORK_MINING);
            }
            break;
        }
    }

    void WaypointReached(uint32 waypointId, uint32 /*pathId*/) override
    {
        switch (waypointId)
        {
        case 0:
            SetEscortPaused(true);

            if (Player* player = GetPlayerForEscort())
                Talk(1, player);

            _events.ScheduleEvent(EVENT_TALK_2, 4s);
            break;
        case 14:
            SetEscortPaused(true);
            _events.ScheduleEvent(EVENT_TALK_3, 3s);
            break;
        case 15:
            SetEscortPaused(true);
            me->SetEmoteState(EMOTE_STATE_WORK_MINING);
            _events.ScheduleEvent(EVENT_STOP_MINING, 10s);
            break;
        case 16:
            SetEscortPaused(true);
            me->SetEmoteState(EMOTE_STATE_WORK_MINING);
            uint8 i = 0;
            for (auto& pair : _npcs)
            {
                Creature* npc = pair.second;
                if (!npc || !npc->IsAlive())
                    continue;

                npc->SetEmoteState(EMOTE_STATE_WORK_MINING);
                npc->GetMotionMaster()->Clear();
                npc->GetMotionMaster()->MovePoint(99,
                    me->GetPositionX() + 3.0f * cos(2.0f * M_PI * float(i) / float(_npcs.size())),
                    me->GetPositionY() + 3.0f * sin(2.0f * M_PI * float(i) / float(_npcs.size())),
                    me->GetPositionZ(), false);
                ++i;
            }
            _events.ScheduleEvent(EVENT_TALK_4, 3s);
            break;
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (Player* player = GetPlayerForEscort())
        {
            if (player->GetQuestStatus(QUEST_DIGGING_THROUGH_BONES) == QUEST_STATUS_INCOMPLETE)
                player->FailQuest(QUEST_DIGGING_THROUGH_BONES);
        }

        for (auto& pair : _npcs)
        {
            if (Creature* npc = pair.second)
                if (npc->IsAlive())
                    npc->DespawnOrUnsummon(0s, 3s);
        }

        _npcs.clear();
        me->DespawnOrUnsummon(0s, 3s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);
        EscortAI::UpdateAI(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_ESCORT_CONTINUE:
                SetEscortPaused(false);
                break;
            case EVENT_TALK_2:
                if (Player* player = GetPlayerForEscort())
                    Talk(2, player);
                _events.ScheduleEvent(EVENT_ESCORT_CONTINUE, 3s);
                break;
            case EVENT_TALK_3:
                if (Player* player = GetPlayerForEscort())
                    Talk(3, player);
                _events.ScheduleEvent(EVENT_ESCORT_CONTINUE, 2s);
                break;
            case EVENT_STOP_MINING:
                for (auto& pair : _npcs)
                    if (Creature* npc = pair.second)
                        if (npc->IsAlive())
                        {
                            npc->ClearUnitState(UNIT_STATE_NOT_MOVE);
                            npc->SetEmoteState(EMOTE_ONESHOT_NONE);
                        }

                me->SetEmoteState(EMOTE_ONESHOT_NONE);
                SetEscortPaused(false);
                break;
            case EVENT_TALK_4:
                if (Player* player = GetPlayerForEscort())
                    Talk(4, player);
                _events.ScheduleEvent(EVENT_TALK_5, 7s);
                break;
            case EVENT_TALK_5:
                if (Player* player = GetPlayerForEscort())
                    Talk(5, player);
                _events.ScheduleEvent(EVENT_SPAWN_DRUM, 5s);
                break;
            case EVENT_SPAWN_DRUM:
                me->SummonGameObject(185304, Position(-3547.79f, 5456.06f, -12.375f, 2.26f), QuaternionData(), 0s);
                if (Player* player = GetPlayerForEscort())
                    Talk(6, player);
                me->SetEmoteState(EMOTE_ONESHOT_NONE);
                for (auto& pair : _npcs)
                    if (Creature* npc = pair.second)
                        if (npc->IsAlive())
                            npc->SetEmoteState(EMOTE_ONESHOT_NONE);
                _events.ScheduleEvent(EVENT_FOLLOWER_78837_TALK0, 5s);
                break;
            case EVENT_FOLLOWER_78837_TALK0:
                _npcs[78837]->AI()->Talk(0);
                _events.ScheduleEvent(EVENT_FOLLOWER_78838_TALK0, 5s);
                break;
            case EVENT_FOLLOWER_78838_TALK0:
                _npcs[78838]->AI()->Talk(1);
                _events.ScheduleEvent(EVENT_FOLLOWER_78839_TALK0, 7s);
                break;
            case EVENT_FOLLOWER_78839_TALK0:
                _npcs[78839]->AI()->Talk(2);
                _events.ScheduleEvent(EVENT_TALK_7, 8s);
                break;
            case EVENT_TALK_7:
                if (Player* player = GetPlayerForEscort())
                    Talk(7, player);
                _events.ScheduleEvent(EVENT_FOLLOWER_78837_TALK3, 6s);
                break;
            case EVENT_FOLLOWER_78837_TALK3:
                _npcs[78837]->AI()->Talk(3);
                _events.ScheduleEvent(EVENT_FOLLOWER_78839_TALK4, 8s);
                break;
            case EVENT_FOLLOWER_78839_TALK4:
                _npcs[78839]->AI()->Talk(4);
                _events.ScheduleEvent(EVENT_FOLLOWER_78840_TALK5, 10s);
                break;
            case EVENT_FOLLOWER_78840_TALK5:
                _npcs[78840]->AI()->Talk(5);
                _events.ScheduleEvent(EVENT_FOLLOWER_78838_TALK6, 12s);
                break;
            case EVENT_FOLLOWER_78838_TALK6:
                _npcs[78838]->AI()->Talk(6);
                _events.ScheduleEvent(EVENT_TALK_8, 12s);
                break;
            case EVENT_TALK_8:
                if (Player* player = GetPlayerForEscort())
                    Talk(8, player);
                _events.ScheduleEvent(EVENT_LEADER_ATTACK_SPELL, 6s);
                break;
            case EVENT_LEADER_ATTACK_SPELL:
                me->HandleEmoteCommand(EMOTE_STATE_ATTACK_UNARMED);
                me->CastSpell(me, SPELL_FUMPER_TRAP, true);
                _events.ScheduleEvent(EVENT_FOLLOWER_78840_TALK7, 2s);
                break;
            case EVENT_FOLLOWER_78840_TALK7:
                _npcs[78840]->AI()->Talk(7);
                _events.ScheduleEvent(EVENT_TALK_9, 2s);
                break;
            case EVENT_TALK_9:
                if (Player* player = GetPlayerForEscort())
                    Talk(9, player);
                _events.ScheduleEvent(EVENT_TALK_10, 2s);
                break;
            case EVENT_TALK_10:
                if (Player* player = GetPlayerForEscort())
                    Talk(10, player);
                for (auto& pair : _npcs)
                    if (Creature* npc = pair.second)
                        if (npc->IsAlive())
                        {
                            npc->ClearUnitState(UNIT_STATE_NOT_MOVE);
                            npc->SetWalk(false);
                        }
                _events.ScheduleEvent(EVENT_CHECK_BONE_SIFTER_DEAD, 2s);
                break;
            case EVENT_CHECK_BONE_SIFTER_DEAD:
                if (!me->FindNearestCreature(NPC_BONE_SIFTER, 100.f, true))
                    _events.ScheduleEvent(EVENT_TALK_11, 3s);
                else
                    _events.ScheduleEvent(EVENT_CHECK_BONE_SIFTER_DEAD, 2s);
                break;
            case EVENT_TALK_11:
                if (Player* player = GetPlayerForEscort())
                    Talk(11, player);
                _events.ScheduleEvent(EVENT_DESPAWN_ALL, 7s);
                break;
            case EVENT_DESPAWN_ALL:
                for (auto& pair : _npcs)
                    if (Creature* npc = pair.second)
                        npc->DespawnOrUnsummon(0s, 3s);

                _npcs.clear();

                if (GameObject* drum = GetClosestGameObjectWithEntry(me, 185304, 100.f))
                    drum->DespawnOrUnsummon();

                std::list<Player*> players;
                GetPlayerListInGrid(players, me, 40.f);
                for (Player* player : players)
                    if (player->GetQuestStatus(QUEST_DIGGING_THROUGH_BONES) == QUEST_STATUS_INCOMPLETE)
                        player->AreaExploredOrEventHappens(QUEST_DIGGING_THROUGH_BONES);

                me->DespawnOrUnsummon(3s, 3s);
                break;
            }
        }

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    std::map<uint32, Creature*> _npcs;
};

void AddSC_npc_letoll()
{
    RegisterCreatureAI(npc_follow_leader);
    RegisterCreatureAI(npc_letoll);
}
