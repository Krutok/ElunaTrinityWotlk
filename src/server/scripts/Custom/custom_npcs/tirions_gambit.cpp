#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "GossipDef.h"
#include "ScriptedGossip.h"
#include "CellImpl.h"
#include "Containers.h"
#include "GridNotifiersImpl.h"
#include "Player.h"
#include "ScriptedEscortAI.h"
#include "SpellMgr.h"
#include "SpellScript.h"

enum TirionsGambit
{
    ACTION_START_EVENT          = 1,
    ACTION_SUMMON_MOVE_STRAIGHT = 2,
    ACTION_SUMMON_EMOTE         = 3,
    ACTION_SUMMON_DESPAWN       = 4,
    ACTION_SUMMON_ORIENTATION   = 5,
    ACTION_SUMMON_TALK          = 6,
    ACTION_SUMMON_STAND_STATE   = 7,

    EVENT_START_SCENE           = 1,
    EVENT_SCENE_0               = 2,

    NPC_DISGUISED_CRUSADER      = 32241,
    NPC_GAMBIT_TIRION_FORDRING  = 32239,
    NPC_INVOKER_BASALEPH        = 32272,
    NPC_CHOSEN_ZEALOT           = 32175,
    NPC_TIRION_LICH_KING        = 32184,
    NPC_TIRION_EBON_KNIGHT      = 32309,
    NPC_TIRION_THASSARIAN       = 32310,
    NPC_TIRION_KOLTIRA          = 32311,
    NPC_TIRION_MOGRAINE         = 32312,
    NPC_CRYSTAL_DUMMY           = 24042,

    GO_FROZEN_HEART             = 193794,
    GO_ESCAPE_PORTAL            = 193941,

    SPELL_TIRION_SMASH_HEART    = 60456,
    SPELL_HEART_EXPLOSION       = 60484,
    SPELL_HEART_EXPLOSION_EFF   = 60532,
    SPELL_LICH_KINGS_FURY       = 60536,
    SPELL_TIRIONS_GAMBIT_CREDIT = 61487,
    SPELL_CULTIST_HOOD          = 61131,
};

class npc_tirions_gambit_tirion : public EscortAI
{
public:
    npc_tirions_gambit_tirion(Creature* creature) : EscortAI(creature), _zealots(), m_EventPlayer(), summons(nullptr) { }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 /*gossipListId*/) override
    {
        CloseGossipMenuFor(player);
        me->AI()->DoAction(ACTION_START_EVENT);
        m_EventPlayer = player->GetGUID();
        return true;
    }

    void Reset() override
    {
        summons.DespawnAll();
        summons.clear();
        _zealots.clear();
        m_EventPlayer = ObjectGuid::Empty;
        me->setActive(false);
        me->SetStandState(UNIT_STAND_STATE_STAND);
    }

    void SetData(uint32 type, uint32 data) override
    {
        if (type == 1 && data == 1)
            events.ScheduleEvent(EVENT_SCENE_0 + 30, 10s);
    }

    void DoAction(int32 param) override
    {
        if (param == ACTION_START_EVENT)
        {
            me->setActive(true);

            Talk(0);
            events.Reset();
            summons.DespawnAll();
            LoadPath(257914);
            if (Player* player = _GetPlayer())
    	    Start(false, player->GetGUID());

            int8 i = -1;
            std::list<Creature*> cList;
            GetCreatureListWithEntryInGrid(cList, me, NPC_DISGUISED_CRUSADER, 15.0f);
            for (std::list<Creature*>::const_iterator itr = cList.begin(); itr != cList.end(); ++itr, ++i)
            {
                (*itr)->SetWalk(true);
                (*itr)->GetMotionMaster()->MoveFollow(me, 1.0f, Position::NormalizeOrientation(M_PI * i / 2.0f));
                summons.Summon(*itr);
            }
        }
    }

    void JustSummoned(Creature* summon) override
    {
        summons.Summon(summon);
        if (summon->GetEntry() == NPC_CHOSEN_ZEALOT || summon->GetEntry() == NPC_TIRION_LICH_KING)
        {
            summon->SetImmuneToAll(true);
            summon->SetWalk(true);
        }
        else if (summon->GetEntry() != NPC_INVOKER_BASALEPH)
        {
            summon->SetUInt32Value(UNIT_NPC_EMOTESTATE, EMOTE_STATE_READY2H);
            summon->SetImmuneToAll(true);
            summon->GetMotionMaster()->MovePoint(4, 6135.97f, 2753.84f, 573.92f);
        }
    }

    void SummonedCreatureDespawn(Creature* summon) override
    {
        summons.Despawn(summon);
    }

    void WaypointReached(uint32 waypointId, uint32 /*pathId*/) override
    {
        switch (waypointId)
        {
        case 6:
            if (TempSummon* bph = me->SummonCreature(NPC_INVOKER_BASALEPH, 6130.26f, 2764.83f, 573.92f, 5.19f, TEMPSUMMON_TIMED_DESPAWN, 10min))
                bph->SetImmuneToAll(true);
            Talk(1);
            break;
        case 8:
            RespawnZealots();
            break;
        case 15:
        {
            uint8 i = 1;
            for (SummonList::const_iterator itr = summons.begin(); itr != summons.end(); ++itr, ++i)
                if (Creature* summon = ObjectAccessor::GetCreature(*me, *itr))
                    if (summon->GetEntry() == NPC_DISGUISED_CRUSADER)
                    {
                        summon->GetMotionMaster()->Clear();
                        summon->GetMotionMaster()->MovePoint(1, 6165.3f + 3 * i, 2759.85f + 1.5f * i, 573.914f);
                    }
            break;
        }
        case 17:
            SetEscortPaused(true);
            events.ScheduleEvent(EVENT_START_SCENE, 7s);
            break;
        case 19:
            SetEscortPaused(true);
            events.ScheduleEvent(EVENT_SCENE_0 + 8, 5s);
            break;
        }
    }
    
    void UpdateEscortAI(uint32 diff) override
    {
        events.Update(diff);

        switch (events.ExecuteEvent())
        {
        case EVENT_START_SCENE:
            Talk(2);
            DoSummonAction(NPC_DISGUISED_CRUSADER, ACTION_SUMMON_ORIENTATION, 200);

            me->SummonCreature(NPC_CHOSEN_ZEALOT, 6160.74f, 2695.90f, 573.92f, 2.04f, TEMPSUMMON_TIMED_DESPAWN, 5min);
            me->SummonCreature(NPC_CHOSEN_ZEALOT, 6164.98f, 2697.90f, 573.92f, 2.04f, TEMPSUMMON_TIMED_DESPAWN, 5min);
            me->SummonCreature(NPC_CHOSEN_ZEALOT, 6161.26f, 2700.05f, 573.92f, 2.04f, TEMPSUMMON_TIMED_DESPAWN, 5min);

            DoSummonAction(NPC_CHOSEN_ZEALOT, ACTION_SUMMON_MOVE_STRAIGHT, 27);
            events.ScheduleEvent(EVENT_SCENE_0, 30s);
            break;
        case EVENT_SCENE_0:
            DoSummonAction(NPC_CHOSEN_ZEALOT, ACTION_SUMMON_STAND_STATE, UNIT_STAND_STATE_KNEEL);
            me->SummonGameObject(GO_FROZEN_HEART, 6132.38f, 2760.76f, 574.0f, 0.0f, QuaternionData(), 3min);
            events.ScheduleEvent(EVENT_SCENE_0 + 1, 10s);
            break;
        case EVENT_SCENE_0 + 1:
            DoSummonAction(NPC_CHOSEN_ZEALOT, ACTION_SUMMON_STAND_STATE, UNIT_STAND_STATE_STAND);
            events.ScheduleEvent(EVENT_SCENE_0 + 2, 2s);
            break;
        case EVENT_SCENE_0 + 2:
            DoSummonAction(NPC_CHOSEN_ZEALOT, ACTION_SUMMON_MOVE_STRAIGHT, -27);
            DoSummonAction(NPC_CHOSEN_ZEALOT, ACTION_SUMMON_DESPAWN, 20000);
            events.ScheduleEvent(EVENT_SCENE_0 + 3, 2s);
            break;
        case EVENT_SCENE_0 + 3:
            Talk(3);
            if (Creature* cr = me->SummonCreature(NPC_TIRION_LICH_KING, 6161.26f, 2700.05f, 573.92f, 2.04f, TEMPSUMMON_TIMED_DESPAWN, 10min))
            {
                const Position pos(6131.93f, 2756.84f, 573.92f);
                cr->GetMotionMaster()->MovePoint(2, pos);
                cr->SetHomePosition(pos);
            }
            events.ScheduleEvent(EVENT_SCENE_0 + 4, 4s);
            break;
        case EVENT_SCENE_0 + 4:
            Talk(4);
            events.ScheduleEvent(EVENT_SCENE_0 + 5, 25s);
            break;
        case EVENT_SCENE_0 + 5:
            DoSummonAction(NPC_TIRION_LICH_KING, ACTION_SUMMON_ORIENTATION, 11);
            events.ScheduleEvent(EVENT_SCENE_0 + 6, 4s);
            break;
        case EVENT_SCENE_0 + 6:
            DoSummonAction(NPC_TIRION_LICH_KING, ACTION_SUMMON_TALK, 0);
            SetEscortPaused(false);
            events.ScheduleEvent(EVENT_SCENE_0 + 7, 6s);
            break;
        case EVENT_SCENE_0 + 7:
            DoSummonAction(NPC_TIRION_LICH_KING, ACTION_SUMMON_TALK, 1);
            break;
        case EVENT_SCENE_0 + 8:
            Talk(5);
            events.ScheduleEvent(EVENT_SCENE_0 + 9, 5s);
            break;
        case EVENT_SCENE_0 + 9:
            DoSummonAction(NPC_TIRION_LICH_KING, ACTION_SUMMON_TALK, 2);
            events.ScheduleEvent(EVENT_SCENE_0 + 10, 11s);
            break;
        case EVENT_SCENE_0 + 10:
            Talk(6);
            events.ScheduleEvent(EVENT_SCENE_0 + 11, 6s);
            break;
        case EVENT_SCENE_0 + 11:
            DoSummonAction(NPC_TIRION_LICH_KING, ACTION_SUMMON_TALK, 3);
            events.ScheduleEvent(EVENT_SCENE_0 + 12, 7s);
            break;
        case EVENT_SCENE_0 + 12:
            DoSummonAction(NPC_TIRION_LICH_KING, ACTION_SUMMON_TALK, 4);
            events.ScheduleEvent(EVENT_SCENE_0 + 13, 5s);
            break;
        case EVENT_SCENE_0 + 13:
            Talk(7);
            events.ScheduleEvent(EVENT_SCENE_0 + 14, 14s);
            break;
        case EVENT_SCENE_0 + 14:
            Talk(8);
            events.ScheduleEvent(EVENT_SCENE_0 + 15, 3s);
            break;
        case EVENT_SCENE_0 + 15:
        {
            me->LoadEquipment(1);
            if (Creature* crystal = me->FindNearestCreature(NPC_CRYSTAL_DUMMY, 40.f))
                me->CastSpell(crystal, SPELL_TIRION_SMASH_HEART, true);
            events.ScheduleEvent(EVENT_SCENE_0 + 16, 1200ms);
            uint8 i = 0;
            for (SummonList::iterator itr = summons.begin(); itr != summons.end(); ++itr, ++i)
                if (Creature* summon = ObjectAccessor::GetCreature(*me, *itr))
                    if (summon->GetEntry() == NPC_DISGUISED_CRUSADER)
                    {
                        summon->SetWalk(false);
                        summon->GetMotionMaster()->MovePoint(2, 6132.38f + 4 * cos(2 * M_PI * (i / 3.0)), 2760.76f + 4 * std::sin(2 * M_PI * (i / 3.0)), me->GetPositionZ());
                    }
            break;
        }
        case EVENT_SCENE_0 + 16:
            me->CastSpell(me, SPELL_HEART_EXPLOSION, true);
            me->CastSpell(me, SPELL_HEART_EXPLOSION_EFF, true);
            me->SetStandState(UNIT_STAND_STATE_DEAD);
            DoSummonAction(NPC_TIRION_LICH_KING, ACTION_SUMMON_TALK, 5);
            if (Creature* lk = me->FindNearestCreature(NPC_TIRION_LICH_KING, 20.f))
            {
                const float speedXY_z = 10.f;
                lk->GetMotionMaster()->MoveKnockbackFrom(me->GetPositionX(), me->GetPositionY(), speedXY_z, speedXY_z);
                lk->SetRegenerateHealth(false);
                lk->SetHealth(CalculatePct(lk->GetHealth(), 30));
                lk->SetFacingTo(lk->GetRelativeAngle(me));
            }
            if (Creature* bph = me->FindNearestCreature(NPC_INVOKER_BASALEPH, 20.f))
                bph->SetStandState(UNIT_STAND_STATE_DEAD);
            if (GameObject* go = me->FindNearestGameObject(GO_FROZEN_HEART, 20.0f))
                go->Delete();
            events.ScheduleEvent(EVENT_SCENE_0 + 17, 2s);
            break;
        case EVENT_SCENE_0 + 17:
            DoSummonAction(NPC_TIRION_LICH_KING, ACTION_SUMMON_STAND_STATE, UNIT_STAND_STATE_KNEEL);
            events.ScheduleEvent(EVENT_SCENE_0 + 170, 3s);
            break;
        case EVENT_SCENE_0 + 170:
            DoSummonAction(NPC_DISGUISED_CRUSADER, ACTION_SUMMON_ORIENTATION, 500);
            DoSummonAction(NPC_DISGUISED_CRUSADER, ACTION_SUMMON_EMOTE, EMOTE_STATE_READY2H);
            if (Creature* cr = me->FindNearestCreature(NPC_DISGUISED_CRUSADER, 10.0f))
                cr->AI()->Talk(0);
            events.ScheduleEvent(EVENT_SCENE_0 + 18, 1s);
            break;
        case EVENT_SCENE_0 + 18:
        {
            DoSummonAction(NPC_TIRION_LICH_KING, ACTION_SUMMON_TALK, 6);

            _GetZealots();
            for (std::list<Creature*>::const_iterator itr = _zealots.begin(); itr != _zealots.end(); ++itr)
            {
                Creature* zealot = *itr;
                zealot->SetStandState(UNIT_STAND_STATE_STAND);

                Position movePosition = *me;
                zealot->MovePosition(movePosition, -frand(8.f, 20.f), zealot->GetRelativeAngle(me));
                zealot->GetMotionMaster()->MovePoint(3, movePosition);
                zealot->SetHomePosition(movePosition);
            }

            events.ScheduleEvent(EVENT_SCENE_0 + 19, 10s);
            break;
        }
        case EVENT_SCENE_0 + 19:
            me->SummonCreatureGroup(1);
            events.ScheduleEvent(EVENT_SCENE_0 + 20, 7s);
            break;
        case EVENT_SCENE_0 + 20:
        {
            for (SummonList::const_iterator itr = summons.begin(); itr != summons.end(); ++itr)
                if (Creature* summon = ObjectAccessor::GetCreature(*me, *itr))
                {
                    if (summon->GetEntry() != NPC_TIRION_LICH_KING && summon->GetEntry() != NPC_INVOKER_BASALEPH)
                        summon->SetImmuneToAll(false);

                    if (summon->GetEntry() >= NPC_TIRION_EBON_KNIGHT && summon->GetEntry() <= NPC_TIRION_MOGRAINE)
                    {
                        if (summon->GetEntry() == NPC_TIRION_MOGRAINE)
                            summon->SetHomePosition(6135.97f, 2753.84f, 573.92f, 3.70f);
                        else
                            summon->SetHomePosition(6138.36f + frand(-2.0f, 2.0f), 2749.25f + frand(-2.0f, 2.0f), 573.92f, 2.03f);
                    }
                }

            Unit* target = me->FindNearestCreature(NPC_TIRION_MOGRAINE, 100.f);
            for (std::list<Creature*>::const_iterator itr = _zealots.begin(); itr != _zealots.end(); ++itr)
                if (Creature* zealot = *itr)
                {
                    zealot->SetImmuneToAll(false);

                    if (!target)
                        zealot->SelectNearestTarget(40.0f);
                    if (target)
                        zealot->AI()->AttackStart(target);
                }

            if (Player* player = _GetPlayer())
                player->RemoveAura(SPELL_CULTIST_HOOD);

            DoSummonAction(NPC_TIRION_THASSARIAN, ACTION_SUMMON_TALK, 0);
            break;
        }
        case EVENT_SCENE_0 + 30:
            for (SummonList::const_iterator itr = summons.begin(); itr != summons.end(); ++itr)
                if (Creature* summon = ObjectAccessor::GetCreature(*me, *itr))
                    if (summon->GetEntry() >= NPC_TIRION_EBON_KNIGHT && summon->GetEntry() <= NPC_TIRION_MOGRAINE)
                    {
                        if (summon->GetEntry() == NPC_TIRION_MOGRAINE)
                            summon->GetMotionMaster()->MovePoint(6, 6135.97f, 2753.84f, 573.92f);
                        else
                            summon->GetMotionMaster()->MovePoint(6, 6138.36f + frand(-2.0f, 2.0f), 2749.25f + frand(-2.0f, 2.0f), 573.92f);
                    }

            events.ScheduleEvent(EVENT_SCENE_0 + 310, 4s);

            break;
        case EVENT_SCENE_0 + 310:
            DoSummonAction(NPC_TIRION_MOGRAINE, ACTION_SUMMON_TALK, 0);
            DoSummonAction(NPC_TIRION_LICH_KING, ACTION_SUMMON_STAND_STATE, UNIT_STAND_STATE_STAND);
            me->SummonGameObject(GO_ESCAPE_PORTAL, 6133.83f, 2757.24f, 573.914f, 1.97f, QuaternionData(), 0s);
            me->CastSpell(me, SPELL_TIRIONS_GAMBIT_CREDIT, true);
            events.ScheduleEvent(EVENT_SCENE_0 + 31, 6s);
            DoSummonAction(NPC_DISGUISED_CRUSADER, ACTION_SUMMON_EMOTE, EMOTE_ONESHOT_NONE);
            break;
        case EVENT_SCENE_0 + 31:
            DoSummonAction(NPC_TIRION_THASSARIAN, ACTION_SUMMON_TALK, 1);
            events.ScheduleEvent(EVENT_SCENE_0 + 32, 7s);
            break;
        case EVENT_SCENE_0 + 32:
            DoSummonAction(NPC_TIRION_MOGRAINE, ACTION_SUMMON_TALK, 1);
            events.ScheduleEvent(EVENT_SCENE_0 + 33, 7s);
            break;
        case EVENT_SCENE_0 + 33:
            for (SummonList::const_iterator itr = summons.begin(); itr != summons.end(); ++itr)
                if (Creature* summon = ObjectAccessor::GetCreature(*me, *itr))
                {
                    const bool _IsLK = summon->GetEntry() == NPC_TIRION_LICH_KING;
                    if (_IsLK)
                        summon->CastSpell(summon, SPELL_LICH_KINGS_FURY, false);

                    summon->DespawnOrUnsummon(_IsLK ? 10s : 4s);
                }
            for (std::list<Creature*>::const_iterator itr = _zealots.begin(); itr != _zealots.end(); ++itr)
                if (Creature* zealot = *itr)
                    zealot->DespawnOrUnsummon(10s);
            me->DespawnOrUnsummon(10s);
            break;
        }
    }

protected:
    std::list<Creature*> _zealots;
    ObjectGuid m_EventPlayer;

    Player* _GetPlayer()
    {
        if (!m_EventPlayer.IsEmpty())
            return ObjectAccessor::GetPlayer(*me, m_EventPlayer);

        return nullptr;
    }

    void _GetZealots() { _zealots.clear(); me->GetCreatureListWithEntryInGrid(_zealots, NPC_CHOSEN_ZEALOT, me->GetGridActivationRange()); }

private:
    EventMap events;
    SummonList summons;

    void DoSummonAction(uint32 entry, uint8 id, int32 param = 0)
    {
        for (SummonList::const_iterator itr = summons.begin(); itr != summons.end(); ++itr)
            if (Creature* summon = ObjectAccessor::GetCreature(*me, *itr))
                if (summon->GetEntry() == entry)
                {
                    switch (id)
                    {
                    case ACTION_SUMMON_MOVE_STRAIGHT:
                        summon->GetMotionMaster()->MovePoint(1, summon->GetPositionX() - float(param), summon->GetPositionY() + float(param) * 2 + 3, summon->GetPositionZ());
                        break;
                    case ACTION_SUMMON_EMOTE:
                        summon->SetUInt32Value(UNIT_NPC_EMOTESTATE, uint32(param));
                        break;
                    case ACTION_SUMMON_DESPAWN:
                        summon->DespawnOrUnsummon(Milliseconds(param));
                        break;
                    case ACTION_SUMMON_ORIENTATION:
                        summon->SetFacingTo(float(param) / 100.0f);
                        break;
                    case ACTION_SUMMON_TALK:
                        summon->AI()->Talk(uint8(param));
                        break;
                    case ACTION_SUMMON_STAND_STATE:
                        summon->SetStandState(UnitStandStateType(param));
                        break;
                    }
                }
    }

    void RespawnZealots()
    {
        _GetZealots();
        for (std::list<Creature*>::const_iterator itr = _zealots.begin(); itr != _zealots.end(); ++itr)
            if (Creature* zealot = *itr)
                zealot->Respawn();

        std::vector<RespawnInfo const*> data;
        me->GetMap()->GetRespawnInfo(data, SPAWN_TYPEMASK_CREATURE);
        if (!data.empty())
        {
            uint32 const gridId = Trinity::ComputeGridCoord(me->GetPositionX(), me->GetPositionY()).GetId();
            for (RespawnInfo const* info : data)
                if (info->gridId == gridId && info->entry == NPC_CHOSEN_ZEALOT)
                    me->GetMap()->Respawn(info->type, info->spawnId);
        }

        _GetZealots();
        for (std::list<Creature*>::const_iterator itr = _zealots.begin(); itr != _zealots.end(); ++itr)
            if (Creature* zealot = *itr)
                zealot->SetImmuneToAll(true);
    }
};

// 60456 - Tirion Smashes Heart
class spell_tirion_smashes_heart : public SpellScript
{
    PrepareSpellScript(spell_tirion_smashes_heart);

    void SetDest(SpellDestination& dest)
    {
        if (Creature* caster = GetCaster()->ToCreature())
            if (Creature* crystal = caster->FindNearestCreature(NPC_CRYSTAL_DUMMY, 40.f))
                dest.Relocate(*crystal);
    }

    void Register() override
    {
        OnDestinationTargetSelect += SpellDestinationTargetSelectFn(spell_tirion_smashes_heart::SetDest, EFFECT_0, TARGET_DEST_NEARBY_ENTRY);
    }
};

void Script_TirionsGambit()
{
    RegisterCreatureAI(npc_tirions_gambit_tirion);
    RegisterSpellScript(spell_tirion_smashes_heart);
}
