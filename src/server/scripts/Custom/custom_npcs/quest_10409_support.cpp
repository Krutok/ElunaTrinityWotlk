#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "ObjectAccessor.h"
#include "ScriptedEscortAI.h"
#include "EventMap.h"
#include "MotionMaster.h"
#include "WorldSession.h"
#include "SpellScript.h"
#include "Group.h"

enum PathIds
{
    PATH_ADYEN = 14829600,
    PATH_ISHANAH = 14830400,
    PATH_KAYLAAN_INTRO = 16635200,
    PATH_KAYLAAN_ESCORT = 16635208,
    PATH_KAYLAAN_FINAL = 16635216
};

enum Spells
{
    EVENT_CAST_9734 = 1,
    SPELL_CANCEL_POWER_OF_THE_LEGION = 35596
};

#define NPC_KARJA_ENTRY 19467  // Entry von npc_karja eintragen
#define NPC_ISHANAH 18538

enum SimpleGossip
{
    QUEST_ID = 10409,
    MENU_TEXT_FIRST_BUTTON = 10210,
    NPC_GOSSIP_FIRST_GOSSIP = 8117,

    AREA_ID_BUTTON_ALLOWED = 3742,
    AREA_ID_QUEST_SHOWN = 3703,
    NPC_TEXT_ALT = 10051,

    TALK_BUTTON = 0,
    SENDER_MAIN_ADYEN = 1,
    ACTION_BUTTON_ADYEN = 1,
    NPC_SOCRETHAR = 20132,
    NPC_KAYLAAN = 20794
};

enum KarjaTalks
{
    SAY_TEST = 0
};

enum SocretharEvents
{
    EVENT_TALK0 = 1,
    EVENT_TALK1,
    EVENT_SUMMON_NPC,
    EVENT_TALK_SELF1,
    EVENT_SETDATA_TARGET,
    EVENT_SETDATA_20794_44,   // <--- Hier ergänzt

    EVENT_CAST_15496,   // 15-25s, auf Victim
    EVENT_CAST_37538,   // 40-60s, auf sich selbst
    EVENT_CAST_28448,   // 10-35s, auf sich selbst
    EVENT_CAST_37540,   // 30-40s, auf Victim
    EVENT_CAST_37537    // 35-50s, auf Victim
};

enum SocretharNPCs
{
    NPC_TARGET_SUMMON = 20794
};

enum KaylaanTalks
{
    KAYLAAN_TALK_0 = 0,
    KAYLAAN_TALK_1 = 1,
    KAYLAAN_TALK_2 = 2,
    KAYLAAN_TALK_3 = 3
};

enum KaylaanEvents
{
    KAYLAAN_EVENT_KNEEL_AND_TALK2 = 1,
    KAYLAAN_EVENT_STAND_UP,
    KAYLAAN_EVENT_START_PATH_PATH_KAYLAAN_ESCORT,
    KAYLAAN_EVENT_LOOK_AT_18537,
    KAYLAAN_EVENT_TALK0,
    KAYLAAN_EVENT_TALK1,
    KAYLAAN_EVENT_TALK2,
    KAYLAAN_EVENT_NPC18537_TALK3,
    KAYLAAN_EVENT_SELF_TALK3,
    KAYLAAN_EVENT_TARGET_20132_SETDATA,
    KAYLAAN_EVENT_FACTION_AND_NPC20132_TALK2,
    KAYLAAN_EVENT_ATTACK_18537,

    // Evade- / Home-Return Events
    KAYLAAN_EVENT_POST_EVADE_TALK3 = 20,
    KAYLAAN_EVENT_SUMMON_ISHANAH = 21,
    KAYLAAN_EVENT_SETDATA_ISHANAH = 22,
    KAYLAAN_EVENT_TALK4 = 23,

    KAYLAAN_EVENT_FINAL_FACTION = 30,
    KAYLAAN_EVENT_FINAL_CAST_13874,
    KAYLAAN_EVENT_FINAL_CAST_35599,
    KAYLAAN_EVENT_FINAL_SETDATA_5_5,
    KAYLAAN_EVENT_CAST_35600_ON_20794 = 40,
    KAYLAAN_EVENT_ATTACK_18538,
    KAYLAAN_EVENT_CAST_37552,
    KAYLAAN_EVENT_CAST_37553
};

enum IshanahTalks
{
    ISHANAH_TALK_0 = 0,
    ISHANAH_TALK_1 = 1
};

enum IshanahEvents
{
    ISHANAH_EVENT_KAYLAAN_SETDATA_2_2 = 1,
    ISHANAH_EVENT_SOCRETHAR_SETDATA_3_3 = 2,
    ISHANAH_EVENT_KAYLAAN_TALK5 = 3,
    ISHANAH_EVENT_SETDATA3_AND_TALK0 = 4,
    ISHANAH_EVENT_TALK1 = 5,
    ISHANAH_EVENT_NPC20132_TALK4 = 6,
    ISHANAH_EVENT_SETDATA4 = 7,
    ISHANAH_EVENT_CAST_15238 = 8
};

enum OrelisEvents
{
    EVENT_CAST_29426 = 1, // auf Victim
    EVENT_CAST_16509,     // auf Victim
    EVENT_CAST_13730,      // auf sich selbst

    // Talk-Sequenz
    EVENT_TALK_SEQUENCE = 10,  // Start der Sequenz
    EVENT_TALK_SEQUENCE_1 = EVENT_TALK_SEQUENCE + 1,
    EVENT_TALK_SEQUENCE_2 = EVENT_TALK_SEQUENCE + 2,
    EVENT_TALK_SEQUENCE_3 = EVENT_TALK_SEQUENCE + 3,
    EVENT_TALK_SEQUENCE_4 = EVENT_TALK_SEQUENCE + 4,
    EVENT_TALK_SEQUENCE_5 = EVENT_TALK_SEQUENCE + 5,
    EVENT_TALK_SEQUENCE_6 = EVENT_TALK_SEQUENCE + 6,
    EVENT_TALK_SEQUENCE_7 = EVENT_TALK_SEQUENCE + 7,
    EVENT_TALK_SEQUENCE_8 = EVENT_TALK_SEQUENCE + 8,
    EVENT_TALK_SEQUENCE_9 = EVENT_TALK_SEQUENCE + 9
};

/* NPC Kaylaan AI */
struct npc_kaylaan : public ScriptedAI
{
    npc_kaylaan(Creature* creature) : ScriptedAI(creature) {}

private:
    EventMap _events;

public:
    void Reset() override
    {
        me->SetReactState(REACT_AGGRESSIVE);
        _events.Reset();
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (me->HealthBelowPctDamaged(25, damage))
        {
            me->CombatStop();
            me->SetReactState(REACT_PASSIVE);
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC);

            EnterEvadeMode(EVADE_REASON_OTHER);
        }
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        // Kampfspells starten, keine urand, nur Schedule mit Repeat
        _events.ScheduleEvent(KAYLAAN_EVENT_CAST_37552, 3s, 7s);
        _events.ScheduleEvent(KAYLAAN_EVENT_CAST_37553, 8s, 12s);
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spell) override
    {
        if (!caster || !spell)
            return;

        if (spell->Id == 35600)
            DoCastSelf(29266, true);
    }

    void SetData(uint32 id, uint32 value) override
    {
        switch (id)
        {
        case 1:
            if (value == 1)
            {
                if (me->GetMotionMaster())
                    me->GetMotionMaster()->MovePath(PATH_KAYLAAN_INTRO, false);
            }
            break;

        case 2:
            if (value == 2)
            {
                if (Creature* npc18538 = me->FindNearestCreature(18538, 100.0f))
                    me->SetFacingToObject(npc18538);
            }
            break;

        case 3:
            if (value == 3)
            {
                me->SetStandState(UNIT_STAND_STATE_KNEEL);
            }
            break;

        case 4:
            if (value == 4)
            {
                // Start des finalen Events
                Talk(6);
                me->SetStandState(UNIT_STAND_STATE_STAND);
                DoCast(me, 35597); // Spell auf sich selbst
                if (me->GetMotionMaster())
                    me->GetMotionMaster()->MovePath(PATH_KAYLAAN_FINAL, false);

                _events.ScheduleEvent(KAYLAAN_EVENT_FINAL_FACTION, 6s);
            }
            break;

        default:
            break;
        }
    }

    void WaypointReached(uint32 waypointId, uint32 pathId) override
    {
        if (pathId == PATH_KAYLAAN_INTRO && waypointId == 4)
        {
            me->SetHomePosition(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), me->GetOrientation());
            _events.ScheduleEvent(KAYLAAN_EVENT_KNEEL_AND_TALK2, 2s);
        }

        if (pathId == PATH_KAYLAAN_ESCORT && waypointId == 7)
        {
            me->SetHomePosition(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), me->GetOrientation());
        }
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        CreatureAI::Reset();

        if (me->IsInEvadeMode())
            return;

        if (!me->IsAlive())
        {
            EngagementOver();
            return;
        }

        EngagementOver();
        me->AddUnitState(UNIT_STATE_EVADE);
        me->GetMotionMaster()->MoveTargetedHome();
        _events.ScheduleEvent(KAYLAAN_EVENT_POST_EVADE_TALK3, 3s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 ev = _events.ExecuteEvent())
        {
            switch (ev)
            {
            case KAYLAAN_EVENT_KNEEL_AND_TALK2:
                me->SetStandState(UNIT_STAND_STATE_KNEEL);
                if (Creature* npc18537 = me->FindNearestCreature(18537, 100.0f))
                    npc18537->AI()->Talk(KAYLAAN_TALK_2);
                _events.ScheduleEvent(KAYLAAN_EVENT_STAND_UP, 4s);
                break;

            case KAYLAAN_EVENT_STAND_UP:
                me->SetStandState(UNIT_STAND_STATE_STAND);
                _events.ScheduleEvent(KAYLAAN_EVENT_START_PATH_PATH_KAYLAAN_ESCORT, 2s);
                break;

            case KAYLAAN_EVENT_START_PATH_PATH_KAYLAAN_ESCORT:
                if (me->GetMotionMaster())
                    me->GetMotionMaster()->MovePath(PATH_KAYLAAN_ESCORT, false);
                _events.ScheduleEvent(KAYLAAN_EVENT_LOOK_AT_18537, 2s);
                break;

            case KAYLAAN_EVENT_LOOK_AT_18537:
                if (Creature* npc18537 = me->FindNearestCreature(18537, 100.0f))
                    me->SetFacingToObject(npc18537);
                _events.ScheduleEvent(KAYLAAN_EVENT_TALK0, 4s);
                break;

            case KAYLAAN_EVENT_TALK0:
                Talk(KAYLAAN_TALK_0);
                _events.ScheduleEvent(KAYLAAN_EVENT_TALK1, 8s);
                break;

            case KAYLAAN_EVENT_TALK1:
                Talk(KAYLAAN_TALK_1);
                _events.ScheduleEvent(KAYLAAN_EVENT_TALK2, 8s);
                break;

            case KAYLAAN_EVENT_TALK2:
                Talk(KAYLAAN_TALK_2);
                _events.ScheduleEvent(KAYLAAN_EVENT_NPC18537_TALK3, 6s);
                break;

            case KAYLAAN_EVENT_NPC18537_TALK3:
                if (Creature* npc18537 = me->FindNearestCreature(18537, 100.0f))
                    npc18537->AI()->Talk(KAYLAAN_TALK_3);
                _events.ScheduleEvent(KAYLAAN_EVENT_SELF_TALK3, 7s);
                break;

            case KAYLAAN_EVENT_SELF_TALK3:
                Talk(KAYLAAN_TALK_3);
                _events.ScheduleEvent(KAYLAAN_EVENT_TARGET_20132_SETDATA, 7s);
                break;

            case KAYLAAN_EVENT_TARGET_20132_SETDATA:
                if (Creature* npc20132 = me->FindNearestCreature(20132, 100.0f))
                    npc20132->AI()->SetData(2, 2);
                _events.ScheduleEvent(KAYLAAN_EVENT_FACTION_AND_NPC20132_TALK2, 4s);
                break;

            case KAYLAAN_EVENT_FACTION_AND_NPC20132_TALK2:
                me->SetFaction(14);
                if (Creature* npc20132 = me->FindNearestCreature(20132, 100.0f))
                    npc20132->AI()->Talk(KAYLAAN_TALK_2);
                _events.ScheduleEvent(KAYLAAN_EVENT_ATTACK_18537, 1s);
                break;

            case KAYLAAN_EVENT_ATTACK_18537:
                if (Creature* npc18537 = me->FindNearestCreature(18537, 100.0f))
                    me->AI()->AttackStart(npc18537);
                break;

                // --- NEU: Evade / Ishanah Kette ---
            case KAYLAAN_EVENT_POST_EVADE_TALK3:
                if (Creature* npc20132 = me->FindNearestCreature(20132, 100.0f))
                    npc20132->AI()->Talk(KAYLAAN_TALK_3);
                _events.ScheduleEvent(KAYLAAN_EVENT_SUMMON_ISHANAH, 0s);
                break;

            case KAYLAAN_EVENT_SUMMON_ISHANAH:
                if (Creature* ishanah = me->SummonCreature(NPC_ISHANAH, 4866.2f, 3799.02f, 199.141f, 0.468058f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 180s))
                    _events.ScheduleEvent(KAYLAAN_EVENT_SETDATA_ISHANAH, 2s);
                _events.ScheduleEvent(KAYLAAN_EVENT_TALK4, 5s);
                break;

            case KAYLAAN_EVENT_SETDATA_ISHANAH:
                if (Creature* ishanah = me->FindNearestCreature(NPC_ISHANAH, 100.0f))
                    ishanah->AI()->SetData(1, 1);
                break;

            case KAYLAAN_EVENT_TALK4:
                Talk(4);
                break;

            case KAYLAAN_EVENT_FINAL_FACTION:
                me->SetFaction(290);
                _events.ScheduleEvent(KAYLAAN_EVENT_FINAL_CAST_13874, 4s);
                break;

            case KAYLAAN_EVENT_FINAL_CAST_13874:
                DoCast(me, 13874);
                _events.ScheduleEvent(KAYLAAN_EVENT_FINAL_CAST_35599, 2s);
                break;

            case KAYLAAN_EVENT_FINAL_CAST_35599:
                DoCast(me, 35599);
                _events.ScheduleEvent(KAYLAAN_EVENT_FINAL_SETDATA_5_5, 6s);
                break;

            case KAYLAAN_EVENT_FINAL_SETDATA_5_5:
                if (Creature* npc20132 = me->FindNearestCreature(20132, 100.0f))
                    npc20132->AI()->SetData(5, 5);
                break;

            case KAYLAAN_EVENT_CAST_37552:
                if (Unit* target = me->GetVictim())
                    DoCast(target, 37552);
                _events.Repeat(4s, 8s);
                break;

            case KAYLAAN_EVENT_CAST_37553:
                if (Unit* target = me->GetVictim())
                    DoCast(target, 37553);
                _events.Repeat(12s, 21s);
                break;

            }

        }

        if (!UpdateVictim())
            return;

        if (me->HasUnitState(UNIT_STATE_EVADE))
            return;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_kaylaan(Creature* creature)
{
    return new npc_kaylaan(creature);
}

/* NPC Karja AI */

struct npc_karja : public ScriptedAI
{
    npc_karja(Creature* creature) : ScriptedAI(creature) {}

private:
    EventMap _events;

public:
    void Reset() override
    {
        _events.Reset();

        // Wenn sie sich in Area 3742 befindet
        if (me->GetAreaId() == 3742)
        {
            me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);
            me->RemoveUnitFlag(UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC);
            me->SetStandState(UNIT_STAND_STATE_STAND);
        }
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(EVENT_CAST_9734, 4s, 5s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        if (me->GetAreaId() == 3742)
            me->SetStandState(UNIT_STAND_STATE_STAND);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_CAST_9734:
                DoCastVictim(9734);
                _events.Repeat(7s, 15s);
                break;
            }
        }

        DoMeleeAttackIfReady();
    }
};

/* npc_orelis*/

struct npc_orelis : public ScriptedAI
{
    npc_orelis(Creature* creature) : ScriptedAI(creature) {}

private:
    EventMap _events;

public:
    void Reset() override
    {
        _events.Reset();

        // Wenn er sich in Area 3742 befindet
        if (me->GetAreaId() == 3742)
        {
            me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);
            me->RemoveUnitFlag(UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC);
        }

        // Talk-Event starten, nur wenn NPC in Area 3712
        if (me->GetAreaId() == 3712)
            _events.ScheduleEvent(EVENT_TALK_SEQUENCE, 0s);
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        // Kampfspells planen
        _events.ScheduleEvent(EVENT_CAST_29426, 3s);  // auf Victim
        _events.ScheduleEvent(EVENT_CAST_16509, 10s, 15s);  // auf Victim
        _events.ScheduleEvent(EVENT_CAST_13730, 7s, 21s);  // auf sich selbst
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        // --- Event-Handling ---
        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
                // Kampfspells
            case EVENT_CAST_29426:
                DoCastVictim(29426);
                _events.Repeat(15s, 21s);
                break;

            case EVENT_CAST_16509:
                DoCastVictim(16509);
                _events.Repeat(30s, 31s);
                break;

            case EVENT_CAST_13730:
                DoCast(me, 13730);
                _events.Repeat(20s, 45s);
                break;

                // Talk-Sequenz
            case EVENT_TALK_SEQUENCE:
                if (!me->IsInCombat() && me->GetAreaId() == 3712)
                {
                    if (Creature* npc19467 = me->FindNearestCreature(19467, 100.0f))
                        npc19467->AI()->Talk(0);
                    _events.ScheduleEvent(EVENT_TALK_SEQUENCE + 1, 8s);
                }
                _events.ScheduleEvent(EVENT_TALK_SEQUENCE, 200s); // Zyklus alle 200s
                break;

            case EVENT_TALK_SEQUENCE + 1:
                if (Creature* npc19468 = me->FindNearestCreature(19468, 100.0f))
                    npc19468->AI()->Talk(0);
                _events.ScheduleEvent(EVENT_TALK_SEQUENCE + 2, 8s);
                break;

            case EVENT_TALK_SEQUENCE + 2:
                Talk(0); // eigener NPC
                _events.ScheduleEvent(EVENT_TALK_SEQUENCE + 3, 8s);
                break;

            case EVENT_TALK_SEQUENCE + 3:
                if (Creature* npc19469 = me->FindNearestCreature(19469, 100.0f))
                    npc19469->AI()->Talk(0);
                _events.ScheduleEvent(EVENT_TALK_SEQUENCE + 4, 8s);
                break;

            case EVENT_TALK_SEQUENCE + 4:
                if (Creature* npc19467 = me->FindNearestCreature(19467, 100.0f))
                    npc19467->AI()->Talk(1);
                _events.ScheduleEvent(EVENT_TALK_SEQUENCE + 5, 8s);
                break;

            case EVENT_TALK_SEQUENCE + 5:
                if (Creature* npc19467 = me->FindNearestCreature(19467, 100.0f))
                    npc19467->AI()->Talk(2);
                _events.ScheduleEvent(EVENT_TALK_SEQUENCE + 6, 8s);
                break;

            case EVENT_TALK_SEQUENCE + 6:
                if (Creature* npc19469 = me->FindNearestCreature(19469, 100.0f))
                    npc19469->AI()->Talk(1);
                _events.ScheduleEvent(EVENT_TALK_SEQUENCE + 7, 8s);
                break;

            case EVENT_TALK_SEQUENCE + 7:
                if (Creature* npc19468 = me->FindNearestCreature(19468, 100.0f))
                    npc19468->AI()->Talk(1);
                _events.ScheduleEvent(EVENT_TALK_SEQUENCE + 8, 8s);
                break;

            case EVENT_TALK_SEQUENCE + 8:
                Talk(1); // eigener NPC
                _events.ScheduleEvent(EVENT_TALK_SEQUENCE + 9, 8s);
                break;

            case EVENT_TALK_SEQUENCE + 9:
                if (Creature* npc19468 = me->FindNearestCreature(19468, 100.0f))
                    npc19468->AI()->Talk(2);
                break;
            }
        }

        // --- Standardkampf ---
        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }
};

/* NPC Adyen AI */

struct npc_adyen : public ScriptedAI
{
    npc_adyen(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        if (me->GetAreaId() == AREA_ID_BUTTON_ALLOWED)
            me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
        else
            me->SetNpcFlag(UNIT_NPC_FLAG_QUESTGIVER);
    }

    bool OnGossipHello(Player* player) override
    {
        ClearGossipMenuFor(player);

        uint32 areaId = me->GetAreaId();
        uint32 npcTextId = MENU_TEXT_FIRST_BUTTON;
        uint32 menuId = NPC_GOSSIP_FIRST_GOSSIP;

        // Prüfen, ob NPC 20132 lebt
        Creature* npc20132 = me->FindNearestCreature(20132, 200.0f);
        bool isNpc20132Alive = npc20132 && npc20132->IsAlive();

        if (areaId == AREA_ID_BUTTON_ALLOWED)
        {
            // Button nur anzeigen, wenn NPC 20132 lebt
            if (player->GetQuestStatus(QUEST_ID) == QUEST_STATUS_INCOMPLETE && isNpc20132Alive)
                AddGossipItemFor(player, menuId, 0, SENDER_MAIN_ADYEN, ACTION_BUTTON_ADYEN);

            SendGossipMenuFor(player, npcTextId, me->GetGUID());
        }
        else if (areaId == AREA_ID_QUEST_SHOWN)
        {
            npcTextId = NPC_TEXT_ALT;
            if (me->IsQuestGiver())
                player->PrepareQuestMenu(me->GetGUID());

            SendGossipMenuFor(player, npcTextId, me->GetGUID());
        }

        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        const uint32 sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);
        const uint32 action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
        CloseGossipMenuFor(player);

        if (sender == SENDER_MAIN_ADYEN && action == ACTION_BUTTON_ADYEN)
        {
            me->SetWalk(true);
            me->SetFaction(495);
            me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);
            if (Creature* npc1 = me->FindNearestCreature(19466, 50.0f))
                npc1->SetFaction(495);
            if (Creature* npc2 = me->FindNearestCreature(19467, 50.0f))
                npc2->SetFaction(495);

            if (me->GetMotionMaster())
                me->GetMotionMaster()->MovePath(PATH_ADYEN, false);
        }

        return true;
    }

    void WaypointReached(uint32 waypointId, uint32 pathId) override
    {
        if (pathId == PATH_ADYEN && waypointId == 7)
        {
            if (Creature* socrethar = me->FindNearestCreature(NPC_SOCRETHAR, 100.0f))
                socrethar->AI()->SetData(1, 1);
        }
    }
};

CreatureAI* GetAI_npc_adyen(Creature* creature)
{
    return new npc_adyen(creature);
}

/* NPC Socrethar AI */

struct npc_socrethar : public ScriptedAI
{
    npc_socrethar(Creature* creature) : ScriptedAI(creature) {}

public:
    void Reset() override
    {
        _events.Reset();
        DoCastSelf(37539);
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC);
    }

    void SetData(uint32 id, uint32 value) override
    {
        switch (id)
        {
        case 1:
            if (value == 1)
            {
                if (Creature* target18537 = me->FindNearestCreature(18537, 100.0f))
                    target18537->AI()->Talk(0);
                _events.ScheduleEvent(EVENT_TALK1, 6s);
            }
            break;

        case 2:
            if (value == 2)
                me->CastSpell(me, 35596, true);
            break;

        case 3:
            if (value == 3)
            {
                if (Creature* npc18538 = me->FindNearestCreature(18538, 100.0f))
                    me->SetFacingToObject(npc18538);
            }
            break;

        case 4:
            if (value == 4)
            {
                if (Creature* npc18538 = me->FindNearestCreature(18538, 100.0f))
                    me->CastSpell(npc18538, 35598, true);
                _events.ScheduleEvent(EVENT_SETDATA_20794_44, 7s);
            }
            break;

        case 5:
            if (value == 5)
            {
                Talk(5);
                _events.ScheduleEvent(KAYLAAN_EVENT_CAST_35600_ON_20794, 6s);
            }
            break;

        default:
            break;
        }
    }

    void JustEngagedWith(Unit* /*victim*/) override
    {
        // Start der Kampf-Events
        _events.ScheduleEvent(EVENT_CAST_15496, 3s, 7s);  // auf Victim
        _events.ScheduleEvent(EVENT_CAST_37538, 10s, 15s);  // auf sich selbst
        _events.ScheduleEvent(EVENT_CAST_28448, 17s, 24s);  // auf sich selbst
        _events.ScheduleEvent(EVENT_CAST_37540, 30s, 40s);  // auf Victim
        _events.ScheduleEvent(EVENT_CAST_37537, 35s, 45s);  // auf Victim
    }

    void JustDied(Unit* /*killer*/) override
    {
        DoCast(me, 35762);

        if (Creature* npc18538 = me->FindNearestCreature(18538, 100.0f))
            npc18538->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);

        std::vector<uint32> despawnNpcs = { 20794, 18538 };

        for (auto entry : despawnNpcs)
        {
            if (Creature* npc = me->FindNearestCreature(entry, 200.0f))
                npc->DespawnOrUnsummon(_delay, 10s);
        }

        Map* map = me->GetMap();
        if (map)
        {
            me->m_Events.AddEventAtOffset([this, map]()
                {
                    map->SpawnGroupDespawn(this->_groupId, this->_ignoreRespawn, nullptr);
                }, _delay);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (!me->HasAura(37539))
            DoCast(me, 37539);

        while (uint32 ev = _events.ExecuteEvent())
        {
            switch (ev)
            {
            case EVENT_TALK1:
                Talk(0);
                _events.ScheduleEvent(EVENT_SUMMON_NPC, 6s);
                break;

            case EVENT_SUMMON_NPC:
                if (Creature* target18537 = me->FindNearestCreature(18537, 100.0f))
                    target18537->AI()->Talk(1);

                if (Creature* socrethar = me->SummonCreature(NPC_KAYLAAN, 4955.08f, 3921.4f, 209.045f, 4.57013f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 180s))
                {
                    if (Creature* kaylaan = me->FindNearestCreature(NPC_KAYLAAN, 100.0f))
                        kaylaan->AI()->SetData(1, 1);
                }

                _events.ScheduleEvent(EVENT_TALK_SELF1, 7s);
                break;

            case EVENT_TALK_SELF1:
                Talk(1);
                _events.ScheduleEvent(EVENT_SETDATA_TARGET, 2s);
                break;

            case EVENT_SETDATA_TARGET:
                if (Creature* target = me->FindNearestCreature(NPC_TARGET_SUMMON, 100.0f))
                    target->AI()->SetData(1, 1);
                break;

            case EVENT_SETDATA_20794_44:
                if (Creature* npc20794 = me->FindNearestCreature(20794, 100.0f))
                    npc20794->AI()->SetData(4, 4);
                break;

            case KAYLAAN_EVENT_CAST_35600_ON_20794:
                if (Creature* npc20794 = me->FindNearestCreature(20794, 100.0f))
                    DoCast(npc20794, 35600);
                _events.ScheduleEvent(KAYLAAN_EVENT_ATTACK_18538, 4s);
                break;

            case KAYLAAN_EVENT_ATTACK_18538:
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC);
                if (Creature* npc18538 = me->FindNearestCreature(18538, 100.0f))
                    AttackStart(npc18538);
                break;

                // Kampfspells
            case EVENT_CAST_15496:
                if (Unit* victim = me->GetVictim())
                    DoCastVictim(15496);
                _events.Repeat(15s, 25s);
                break;

            case EVENT_CAST_37538:
                DoCastSelf(37538);
                _events.Repeat(40s, 60s);
                break;

            case EVENT_CAST_28448:
                DoCastSelf(28448);
                _events.Repeat(10s, 35s);
                break;

            case EVENT_CAST_37540:
                if (Unit* victim = me->GetVictim())
                    DoCastVictim(37540);
                _events.Repeat(30s, 40s);
                break;

            case EVENT_CAST_37537:
                if (Unit* victim = me->GetVictim())
                    DoCastVictim(37537);
                _events.Repeat(35s, 50s);
                break;
            }

        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
    bool _ignoreRespawn = true;
    uint32 _groupId = 824;
    std::chrono::seconds _delay = 60s; // Typ explizit, kein auto
};

CreatureAI* GetAI_npc_socrethar(Creature* creature)
{
    return new npc_socrethar(creature);
}

struct npc_ishanah : public ScriptedAI
{
    npc_ishanah(Creature* creature) : ScriptedAI(creature) {}

private:
    EventMap _events;

public:
    void Reset() override
    {
        _events.Reset();
    }

    void WaypointReached(uint32 waypointId, uint32 pathId) override
    {
        if (pathId == PATH_ISHANAH && waypointId == 18)
        {
            me->SetHomePosition(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), me->GetOrientation());
            _events.ScheduleEvent(ISHANAH_EVENT_KAYLAAN_SETDATA_2_2, 0s);
        }
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.ScheduleEvent(ISHANAH_EVENT_CAST_15238, 1s, 3s);
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spell) override
    {
        if (!caster || !spell)
            return;

        if (spell->Id == 35598)
            DoCastSelf(29266, true);

        else if (spell->Id == 35599)
            me->RemoveAurasDueToSpell(29266);
    }

    void SetData(uint32 id, uint32 value) override
    {
        if (id == 1 && value == 1)
        {
            me->SetFaction(250);
            me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_QUESTGIVER);

            if (me->GetMotionMaster())
                me->GetMotionMaster()->MovePath(PATH_ISHANAH, false);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 ev = _events.ExecuteEvent())
        {
            switch (ev)
            {
                // 1) Kaylaan SetData 2:2
            case ISHANAH_EVENT_KAYLAAN_SETDATA_2_2:
                if (Creature* kaylaan = me->FindNearestCreature(NPC_KAYLAAN, 100.0f))
                    kaylaan->AI()->SetData(2, 2);
                _events.ScheduleEvent(ISHANAH_EVENT_SOCRETHAR_SETDATA_3_3, 0s);
                break;

                // 2) Socrethar SetData 3:3
            case ISHANAH_EVENT_SOCRETHAR_SETDATA_3_3:
                if (Creature* socrethar = me->FindNearestCreature(NPC_SOCRETHAR, 100.0f))
                    socrethar->AI()->SetData(3, 3);
                _events.ScheduleEvent(ISHANAH_EVENT_KAYLAAN_TALK5, 0s);
                break;

                // 3) Kaylaan Talk5
            case ISHANAH_EVENT_KAYLAAN_TALK5:
                if (Creature* kaylaan = me->FindNearestCreature(NPC_KAYLAAN, 100.0f))
                    kaylaan->AI()->Talk(5);
                _events.ScheduleEvent(ISHANAH_EVENT_SETDATA3_AND_TALK0, 6s);
                break;

                // 4) Kaylaan SetData 3:3 + Ishanah Talk0
            case ISHANAH_EVENT_SETDATA3_AND_TALK0:
                if (Creature* kaylaan = me->FindNearestCreature(NPC_KAYLAAN, 100.0f))
                    kaylaan->AI()->SetData(3, 3);
                Talk(0);
                _events.ScheduleEvent(ISHANAH_EVENT_TALK1, 6s);
                break;

                // 5) Ishanah Talk1
            case ISHANAH_EVENT_TALK1:
                Talk(1);
                _events.ScheduleEvent(ISHANAH_EVENT_NPC20132_TALK4, 7s);
                break;

                // 6) NPC 20132 Talk4
            case ISHANAH_EVENT_NPC20132_TALK4:
                if (Creature* npc20132 = me->FindNearestCreature(20132, 100.0f))
                    npc20132->AI()->Talk(4);
                _events.ScheduleEvent(ISHANAH_EVENT_SETDATA4, 4s);
                break;

                // 7) NPC 20132 SetData 4:4
            case ISHANAH_EVENT_SETDATA4:
                if (Creature* npc20132 = me->FindNearestCreature(20132, 100.0f))
                    npc20132->AI()->SetData(4, 4);
                break;

            case ISHANAH_EVENT_CAST_15238:
                if (Unit* target = me->GetVictim())
                    DoCast(target, 15238);
                _events.Repeat(3s, 7s);
                break;
            }
        }

        // --- Standardkampfverhalten ---
        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_ishanah(Creature* creature)
{
    return new npc_ishanah(creature);
}

class cancel_power_of_the_legion : public SpellScript
{
    PrepareSpellScript(cancel_power_of_the_legion);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CANCEL_POWER_OF_THE_LEGION });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            target->RemoveAurasDueToSpell(SPELL_CANCEL_POWER_OF_THE_LEGION);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(cancel_power_of_the_legion::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

void AddSC_quest_10409_support()
{
    RegisterCreatureAI(npc_kaylaan);
    RegisterCreatureAI(npc_karja);
    RegisterCreatureAI(npc_adyen);
    RegisterCreatureAI(npc_socrethar);
    RegisterCreatureAI(npc_ishanah);
    RegisterCreatureAI(npc_orelis);
    RegisterSpellScript(cancel_power_of_the_legion);
}
