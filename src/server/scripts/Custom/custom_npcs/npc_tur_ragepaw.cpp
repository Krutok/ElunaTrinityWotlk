#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "ScriptedEscortAI.h"
#include "ScriptedGossip.h"
#include "EventMap.h"
#include "Player.h"
#include "SpellHistory.h"
#include "SpellAuras.h"
#include "WaypointManager.h"

/*####
### SUPPORT QUEST Ursoc, the Bear God
#####*/

enum Ursoc_the_bear_god
{
    QUEST_ID_ALLIANCE                           = 12249,
    QUEST_ID_HORDE                              = 12236,

    MENU_TEXT_FIRST_BUTTON                      = 12785,
    MENU_TEXT_SECOND_BUTTON                     = 12787,

    NPC_GOSSIP_FIRST_GOSSIP_ID                  = 9496,
    NPC_GOSSIP_SECOND_GOSSIP_ID                 = 9497,

    SPELL_BEAR_FORM                             = 48368,
    SPELL_RAGEPAWS_PRESENCE                     = 52507,
    SPELL_LACERATE                              = 52504,
    SPELL_MAUL                                  = 52506,
    SPELL_GROWL                                 = 6795,
    SPELL_MOONKIN_FORM                          = 48369,
    SPELL_MOONKIN_AURA                          = 24907,
    SPELL_EMPOWERED_MOONKIN_AURA                = 52503,
    SPELL_WRATH                                 = 52501,
    SPELL_MOONFIRE                              = 52502,
    SPELL_LIFEBLOOM                             = 52551,
    SPELL_NOURISH                               = 52554,
    SPELL_TREE_OF_LIFE                          = 48371,
    SPELL_EMPOWERED_TREE_OF_LIFE                = 52553,

    EVENT_LACERATE                              = 1,
    EVENT_MAUL                                  = 2,
    EVENT_GROWL                                 = 3,
    EVENT_WRATH                                 = 4,
    EVENT_MOONFIRE                              = 5,
    EVENT_LIFEBLOOM                             = 6,
    EVENT_NOURISH                               = 7,
    EVENT_BOSS_SPAWN                            = 8,

    TRIGGER_NPC                                 = 24921,
    BOSS_URSOC                                  = 26633,

    TALK_TANK                                   = 0,
    TALK_DD                                     = 1,
    TALK_HEAL                                   = 2,
    TALK_SUMMON_URSOC                           = 3,

    PHASE_NONE                                  = 0,
    PHASE_TANK                                  = 1,
    PHASE_DD                                    = 2,
    PHASE_HEAL                                  = 3,

    WAYPOINT_9                                  = 9
};

struct npc_tur_ragepaw : public EscortAI
{
    npc_tur_ragepaw(Creature* creature) : EscortAI(creature), _phase(PHASE_NONE) {}

    void InitializeAI() override
    {
        me->SetStandState(UNIT_STAND_STATE_KNEEL);
    }

    void EnterEvadeMode(EvadeReason) override
    {
        EscortAI::Reset();
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
    }

    bool OnGossipHello(Player* player) override
    {
        uint32 npcTextId = MENU_TEXT_FIRST_BUTTON;
        uint32 menuId = NPC_GOSSIP_FIRST_GOSSIP_ID;

        if (player->GetQuestStatus(QUEST_ID_HORDE) == QUEST_STATUS_INCOMPLETE || player->GetQuestStatus(QUEST_ID_ALLIANCE) == QUEST_STATUS_INCOMPLETE)
        {
            AddGossipItemFor(player, menuId, 0, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
        }
        SendGossipMenuFor(player, npcTextId, me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        const uint32 sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);
        const uint32 action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
        CloseGossipMenuFor(player);

        uint32 menuId = NPC_GOSSIP_SECOND_GOSSIP_ID;
        uint32 mainMenuTextId = MENU_TEXT_SECOND_BUTTON;

        if (sender == GOSSIP_SENDER_MAIN)
        {
            ClearGossipMenuFor(player);

            AddGossipItemFor(player, menuId, 0, GOSSIP_SENDER_INN_INFO, GOSSIP_ACTION_INFO_DEF);
            AddGossipItemFor(player, menuId, 1, GOSSIP_SENDER_INN_INFO, GOSSIP_ACTION_INFO_DEF + 1);
            AddGossipItemFor(player, menuId, 2, GOSSIP_SENDER_INN_INFO, GOSSIP_ACTION_INFO_DEF + 2);

            SendGossipMenuFor(player, mainMenuTextId, me->GetGUID());
        }
        else if (sender == GOSSIP_SENDER_INN_INFO)
        {
            switch (action)
            {
            case GOSSIP_ACTION_INFO_DEF:
            {
                Talk(TALK_TANK, player);
                DoCastSelf(SPELL_BEAR_FORM);
                DoCastSelf(SPELL_RAGEPAWS_PRESENCE);
                me->SetFaction(250);
                me->SetReactState(REACT_DEFENSIVE);
                me->SetStandState(UNIT_STAND_STATE_STAND);
                LoadPath(218626);
                Start(true, player->GetGUID());
                StartPhase(PHASE_TANK);

                std::list<Creature*> creatureList;
                GetCreatureListWithEntryInGrid(creatureList, me, TRIGGER_NPC, 40.0f);
                for (Creature* creature : creatureList)
                    creature->DespawnOrUnsummon(0s, 60s);

                break;
            }

            case GOSSIP_ACTION_INFO_DEF + 1:
            {
                Talk(TALK_DD, player);
                DoCastSelf(SPELL_MOONKIN_FORM);
                DoCastSelf(SPELL_MOONKIN_AURA);
                DoCastSelf(SPELL_EMPOWERED_MOONKIN_AURA);
                me->SetFaction(250);
                me->SetReactState(REACT_DEFENSIVE);
                me->SetStandState(UNIT_STAND_STATE_STAND);
                LoadPath(218626);
                Start(true, player->GetGUID());
                StartPhase(PHASE_DD);

                std::list<Creature*> creatureList;
                GetCreatureListWithEntryInGrid(creatureList, me, TRIGGER_NPC, 40.0f);
                for (Creature* creature : creatureList)
                    creature->DespawnOrUnsummon(0s, 60s);

                break;
            }

            case GOSSIP_ACTION_INFO_DEF + 2:
            {
                Talk(TALK_HEAL, player);
                DoCastSelf(SPELL_TREE_OF_LIFE);
                DoCastSelf(SPELL_EMPOWERED_TREE_OF_LIFE);
                me->SetFaction(113);
                me->SetReactState(REACT_DEFENSIVE);
                me->SetStandState(UNIT_STAND_STATE_STAND);
                LoadPath(218626);
                Start(true, player->GetGUID());
                StartPhase(PHASE_HEAL);

                std::list<Creature*> creatureList;
                GetCreatureListWithEntryInGrid(creatureList, me, TRIGGER_NPC, 40.0f);
                for (Creature* creature : creatureList)
                    creature->DespawnOrUnsummon(0s, 60s);

                break;
                }
            }
        }

        return true;
    }

    void Reset() override
    {
        EscortAI::Reset();
    }

    void StartPhase(uint32 phase)
    {
        _phase = phase;
        _events.Reset();

        switch (_phase)
        {
        case PHASE_TANK:
            _events.ScheduleEvent(EVENT_LACERATE, 2s, 4s);
            _events.ScheduleEvent(EVENT_MAUL, 6s, 8s);
            _events.ScheduleEvent(EVENT_GROWL, 20s);
            break;

        case PHASE_DD:
            _events.ScheduleEvent(EVENT_WRATH, 3s, 4s);
            _events.ScheduleEvent(EVENT_MOONFIRE, 13s, 14s);
            break;

        case PHASE_HEAL:
            _events.ScheduleEvent(EVENT_LIFEBLOOM, 5s);
            _events.ScheduleEvent(EVENT_NOURISH, 7s);
            break;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        EscortAI::UpdateAI(diff);
        _events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 ev = _events.ExecuteEvent())
        {
            switch (ev)
            {
                // Phase 1
            case EVENT_LACERATE:
                if (Unit* target = me->GetVictim())
                    DoCast(target, SPELL_LACERATE, false);
                _events.Repeat(2s, 4s);
                break;

            case EVENT_MAUL:
                if (Unit* target = me->GetVictim())
                    DoCast(target, SPELL_MAUL, false);
                _events.Repeat(6s, 8s);
                break;

            case EVENT_GROWL:
                if (Unit* target = me->GetVictim())
                    DoCast(target, SPELL_GROWL, false);
                _events.Repeat(20s);
                break;

                // Phase 2
            case EVENT_WRATH:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                    DoCast(target, SPELL_WRATH, false);
                _events.Repeat(3s, 4s);
                break;

            case EVENT_MOONFIRE:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0))
                    DoCast(target, SPELL_MOONFIRE, false);
                _events.Repeat(13s, 14s);
                break;

                // Phase 3
            case EVENT_LIFEBLOOM:
            {
                auto CastLifebloomIfNeeded = [this](Unit* target) -> bool
                    {
                        if (!target)
                            return false;

                        Aura* aura = target->GetAura(SPELL_LIFEBLOOM, me->GetGUID());
                        if (!aura || aura->GetStackAmount() < 3)
                        {
                            DoCast(target, SPELL_LIFEBLOOM, false);
                            return true;
                        }
                        return false;
                    };
                Unit* playerTarget = me->SelectNearestPlayer(40.0f);
                bool castedPlayer = CastLifebloomIfNeeded(playerTarget);

                if (me->GetHealthPct() <= 80.0f)
                    CastLifebloomIfNeeded(me);

                _events.Repeat(castedPlayer ? 1s : 12s);
                break;
            }
            case EVENT_NOURISH:
            {
                auto CastLifebloomIfNeeded = [this](Unit* target) -> bool
                    {
                        if (!target)
                            return false;

                        Aura* aura = target->GetAura(SPELL_LIFEBLOOM, me->GetGUID());
                        if (!aura || aura->GetStackAmount() < 3)
                        {
                            DoCast(target, SPELL_LIFEBLOOM, false);
                            return true;
                        }
                        return false;
                    };
                Unit* playerTarget = me->SelectNearestPlayer(40.0f);
                Aura* aura = playerTarget ? playerTarget->GetAura(SPELL_LIFEBLOOM, me->GetGUID()) : nullptr;

                if (aura && aura->GetStackAmount() >= 3)
                {
                    DoCast(playerTarget, SPELL_NOURISH, false);
                    _events.Repeat(10s);
                }
                else
                {
                    _events.Repeat(1s);
                }
                if (me->GetHealthPct() <= 80.0f)
                    CastLifebloomIfNeeded(me);
                break;
            }
            case EVENT_BOSS_SPAWN:
                Talk(TALK_SUMMON_URSOC);
                if (Creature* spawned = me->SummonCreature(BOSS_URSOC, 4893.27f, -3842.42f, 337.648f, 3.12414f, TEMPSUMMON_MANUAL_DESPAWN))
                    spawned->setActive(true);
                break;
            }
        }

        DoMeleeAttackIfReady();
    }

    void WaypointReached(uint32 waypointId, uint32 /*pathId*/) override
    {
        me->SetHomePosition(*me);

        if (!me->IsInCombat())
            me->SetFullHealth();

        switch (waypointId)
        {
        case WAYPOINT_9:
        {
            if (!me->FindNearestCreature(BOSS_URSOC, 100.f))
                _events.ScheduleEvent(EVENT_BOSS_SPAWN, 6s);

            SetEscortPaused(true);
            break;
        }
        default:
            break;
        }
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (_phase == PHASE_TANK)
        {
            damage = uint32(damage * 0.3f);
        }
    }


private:
    uint32 _phase;
    EventMap _events;
};

void AddSC_npc_tur_ragepaw()
{
    RegisterCreatureAI(npc_tur_ragepaw);
}
