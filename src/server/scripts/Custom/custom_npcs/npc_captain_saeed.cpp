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
#include "CreatureGroups.h"

/*
Support the Escort Quest Dimensius the All-Devouring
*/

enum Dimensius_the_All_Devouring
{
    NPC_SAEED_SAY_1                         = 0,
    NPC_SAEED_SAY_2                         = 1,
    NPC_SAEED_SAY_3                         = 2,
    NPC_SAEED_SAY_4                         = 3,
    NPC_FOLLOWER_SAY                        = 0,            /*Protectorate Regenerator SAY after reached WP19*/
    NPC_DIMENSIUS_SAY_1                     = 0,
    NPC_DIMENSIUS_SAY_2                     = 1,
    NPC_DIMENSIUS_SAY_3                     = 2,

    EVENT_EMOTE_ROAR                        = 1,            /*Handle Emote Command bevor starts follow (Blizzlike)*/
    EVENT_BEGIN_PATH                        = 2,
    EVENT_EMOTE_READY                       = 3,
    EVENT_RESUME_PATH                       = 4,
    EVENT_LINE_FORMATION                    = 5,
    EVENT_FRONT_FORMATION                   = 6,
    EVENT_ATTACK_READY                      = 7,
    EVENT_DIMENSIUS_REPLY                   = 8,
    EVENT_ATTACK_DIMENSIUS                  = 9,            /*Starts the Final Escort Waypoint to the BOSS*/
    EVENT_FOLLOWER_SAY                      = 10,
    EVENT_TELEPORT                          = 11,
    EVENT_DESPAWN                           = 12,           /*Delays despawn to cast spell*/
    EVENT_CHECK_DISTANCE                    = 13,

    EVENT_CAST_GLAIVE                       = 1,            // Protectorate Avenger
    EVENT_CAST_HOLY_BOLT                    = 2,            // Protectorate Regenerator

    QUEST_DIMENSIUS_THE_ALL_DEVOURING       = 10439,        /*QUEST ID*/

    SPELL_DIMENSIUS_SHADOW_SPIRAL           = 37500,
    SPELL_DIMENSIUS_TRANSFORM               = 35939,        /*comes from Triniticore spell_dbc and used in the SAI Script from Dimensius and remove the Aura from the Boss*/
    SPELL_DIMENSIUS_SHADOW_RAIN_1           = 37396,
    SPELL_DIMENSIUS_SHADOW_RAIN_2           = 37397,
    SPELL_DIMENSIUS_SHADOW_RAIN_3           = 37399,
    SPELL_DIMENSIUS_SHADOW_RAIN_4           = 37405,
    SPELL_DIMENSIUS_SHADOW_RAIN_5           = 37409,
    SPELL_TELEPORT_SPELL_VISUAL             = 51347,        /*Cast Saeed and follor bevore despawn (Blizzlike)*/

    SPELL_GLAIVE                            = 36500,        // Protectorate Avenger
    SPELL_HOLY_BOLT                         = 34232,        // Protectorate Regenerator

    NPC_SAEED_KILLCREDIT                    = 20985,        /*Give The Killcredit after Speaking with SAEED*/
    NPC_PROTECTORATE_AVENGER                = 21805,
    NPC_PROTECTORATE_DEFENDER               = 20984,
    NPC_PROTECTORATE_REGENERATOR            = 21783,
    NPC_DIMENSIUS                           = 19554,        /*Final Boss Entry ID*/

    TRINITY_STRING_TEXT_1                   = 30000,        /*Escort Start Button Text*/
    TRINITY_STRING_TEXT_2                   = 30001,        /*Escort Final Stage Button Text*/

    DATA_GOSSIP_STATE                       = 1,            /*Switch between NPC GOSSIP TEXT*/
    TEXT_NPC_SAEED_START_FIGHT              = 10232,        /*GOSSIP NPC TEXT Waypoint 16*/
    TEXT_NPC_SAEED_DEFAULT                  = 10229,        /*GOSSIP NPC TEXT FIRST SPEEKING WITH SAEED*/

    WAYPOINT_16                             = 16,
    WAYPOINT_18                             = 18,
    WAYPOINT_19                             = 19,

    ACTION_FOLLOW_LEADER                    = 1
};

class npc_saeed_follower : public ScriptedAI
{
public:
    npc_saeed_follower(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        me->setActive(true);

        _events.Reset();
    }

    void AttackStart(Unit* who) override
    {
        if (who->GetEntry() != NPC_DIMENSIUS || me->GetEntry() == NPC_PROTECTORATE_DEFENDER)
            ScriptedAI::AttackStart(who);
        else if (me->GetEntry() == NPC_PROTECTORATE_AVENGER)
            ScriptedAI::AttackStartCaster(who, frand(10.f, 25.f));
        else if (me->GetEntry() == NPC_PROTECTORATE_REGENERATOR)
            ScriptedAI::AttackStartCaster(who, frand(10.f, 40.f));
    }

    void JustEngagedWith(Unit* who) override
    {
        ScriptedAI::JustEngagedWith(who);

        if (who->GetEntry() != NPC_DIMENSIUS)
            return;

        if (me->GetEntry() == NPC_PROTECTORATE_AVENGER)
            _events.ScheduleEvent(EVENT_CAST_GLAIVE, 1s, 2s);
        else if (me->GetEntry() == NPC_PROTECTORATE_REGENERATOR)
            _events.ScheduleEvent(EVENT_CAST_HOLY_BOLT, 1s, 2s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        _events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 ev = _events.ExecuteEvent())
        {
            switch (ev)
            {
            case EVENT_CAST_GLAIVE:
                DoCastVictim(SPELL_GLAIVE);
                _events.ScheduleEvent(EVENT_CAST_GLAIVE, 1s, 2s);
                break;
            case EVENT_CAST_HOLY_BOLT:
                DoCastVictim(SPELL_HOLY_BOLT);
                _events.ScheduleEvent(EVENT_CAST_HOLY_BOLT, 1s, 2s);
                break;
            }
        }

        DoMeleeAttackIfReady();
    }

private:
    EventMap _events;
};

class npc_captain_saeed : public EscortAI
{
public:
    npc_captain_saeed(Creature* creature)
        : EscortAI(creature),
        _playersWithKillCredit(),
        _questStarterGUID(ObjectGuid::Empty),
        _playerAwayTime(0),
        _lastReachedWaypoint(0),
        _playerAway(false) { }

    bool OnGossipHello(Player* player) override
    {
        uint32 gossipText = TEXT_NPC_SAEED_DEFAULT;

        if (player->GetQuestStatus(QUEST_DIMENSIUS_THE_ALL_DEVOURING) == QUEST_STATUS_INCOMPLETE)
        {
            if (HasEscortState(STATE_ESCORT_ESCORTING))
            {
                if (!_questStarterGUID || _questStarterGUID != player->GetGUID())
                    return false;

                gossipText = TEXT_NPC_SAEED_START_FIGHT;
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, player->GetSession()->GetTrinityString(TRINITY_STRING_TEXT_2), 0, GOSSIP_ACTION_INFO_DEF + 1);
            }
            else
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, player->GetSession()->GetTrinityString(TRINITY_STRING_TEXT_1), 0, GOSSIP_ACTION_INFO_DEF);
        }

        SendGossipMenuFor(player, gossipText, me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        const uint32 action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
        ClearGossipMenuFor(player);

        if (HasEscortState(STATE_ESCORT_ESCORTING) && _questStarterGUID != player->GetGUID())
            return false;

        switch (action)
        {
        case GOSSIP_ACTION_INFO_DEF:
        {
            _questStarterGUID = player->GetGUID();
            _playersWithKillCredit.clear();

            if (Group* group = player->GetGroup())
            {
                for (GroupReference* ref = group->GetFirstMember(); ref; ref = ref->next())
                    if (Player* member = ref->GetSource())
                        if (member->GetQuestStatus(QUEST_DIMENSIUS_THE_ALL_DEVOURING) == QUEST_STATUS_INCOMPLETE)
                        {
                            member->KilledMonsterCredit(NPC_SAEED_KILLCREDIT);
                            _playersWithKillCredit.insert(member->GetGUID());
                        }
            }
            else
            {
                player->KilledMonsterCredit(NPC_SAEED_KILLCREDIT);
                _playersWithKillCredit.insert(player->GetGUID());
            }

            Talk(NPC_SAEED_SAY_1, player);
            me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

            _followerGUIDs.clear();

            CreatureGroup* group = me->GetFormation();
            uint8 i = 0;
            uint32 const followerEntries[] = { NPC_PROTECTORATE_DEFENDER, NPC_PROTECTORATE_AVENGER, NPC_PROTECTORATE_REGENERATOR };
            for (uint32 entry : followerEntries)
            {
                std::list<Creature*> tempList;
                me->GetCreatureListWithEntryInGrid(tempList, entry, 100.f);

                for (Creature* cr : tempList)
                {
                    if (cr->IsAlive())
                    {
                        _followerGUIDs.push_back(cr->GetGUID());

                        if (group && group->HasMember(cr))
                            if (FormationInfo* info = sFormationMgr->GetFormationInfo(cr->GetSpawnId()))
                            {
                                const float angle = i * 40.f;

                                info->FollowAngle = angle;
                                info->FollowDist = 3.5f;
                            }
                    }

                    ++i;
                }
            }

            _events.ScheduleEvent(EVENT_EMOTE_ROAR, 7s);
            _events.ScheduleEvent(EVENT_CHECK_DISTANCE, 2s);
            break;
        }
        case GOSSIP_ACTION_INFO_DEF + 1:
        {
            me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

            Talk(NPC_SAEED_SAY_3, player);

            me->SetEmoteState(EMOTE_STATE_NONE);
            for (Creature* follower : GetFollowers())
                follower->SetEmoteState(EMOTE_STATE_NONE);

            _events.ScheduleEvent(EVENT_LINE_FORMATION, 2s);
            break;
        }

        default:
            break;
        }

        return true;
    }

    void JustEngagedWith(Unit* who) override
    {
        for (Creature* follower : GetFollowers())
            if (CreatureAI* ai = follower->AI())
                ai->AttackStart(who);
    }

    void UpdateAI(uint32 diff) override
    {
        EscortAI::UpdateAI(diff);
        _events.Update(diff);

        while (uint32 ev = _events.ExecuteEvent())
        {
            switch (ev)
            {
            case EVENT_EMOTE_ROAR:
                /*This is required so that everyone performs the emote and only starts running 3 seconds later. Blizzlike behavior*/
                me->HandleEmoteCommand(EMOTE_ONESHOT_ROAR);
                for (Creature* follower : GetFollowers())
                {
                    follower->HandleEmoteCommand(EMOTE_ONESHOT_ROAR);
                    follower->SetFaction(250);
                    follower->SetReactState(REACT_AGGRESSIVE);
                }

                me->SetFaction(250);
                me->SetReactState(REACT_AGGRESSIVE);

                _events.ScheduleEvent(EVENT_BEGIN_PATH, 1s);
                break;
            case EVENT_BEGIN_PATH:
                LoadPath(167882);
                Start(true, _questStarterGUID);
                break;
            case EVENT_EMOTE_READY:
                me->SetEmoteState(EMOTE_STATE_READY2H);
                for (Creature* follower : GetFollowers())
                    follower->SetEmoteState(EMOTE_STATE_READY_UNARMED);
                break;
            case EVENT_LINE_FORMATION:
                if (Creature* boss = me->FindNearestCreature(NPC_DIMENSIUS, 100.0f, false))
                    boss->Respawn();

                if (CreatureGroup* group = me->GetFormation())
                {
                    uint8 i = 0;
                    for (Creature* follower : GetFollowers())
                    {
                        if (group->HasMember(follower))
                            if (FormationInfo* info = sFormationMgr->GetFormationInfo(follower->GetSpawnId()))
                            {
                                const float dist = info->FollowDist * ++i;

                                info->FollowAngle = 0;
                                info->FollowDist = dist;
                            }
                    }

                    group->LeaderStartedMoving();
                }

                _events.ScheduleEvent(EVENT_RESUME_PATH, 2s);
                break;
            case EVENT_RESUME_PATH:
                SetEscortPaused(false);
                break;
            case EVENT_FRONT_FORMATION:
                if (CreatureGroup* group = me->GetFormation())
                {
                    uint8 i = 0;
                    for (Creature* follower : GetFollowers())
                        if (group->HasMember(follower))
                            if (FormationInfo* info = sFormationMgr->GetFormationInfo(follower->GetSpawnId()))
                            {
                                const float angle = (90.f + ++i * 20.f) * float(M_PI) / 180.0f;

                                info->FollowAngle = angle;
                                info->FollowDist = 3.5f;
                            }

                    group->LeaderStartedMoving();
                }

                _events.ScheduleEvent(EVENT_ATTACK_READY, 12s);
                _events.ScheduleEvent(EVENT_EMOTE_READY, 12s);
                break;
            case EVENT_ATTACK_READY:
                if (Creature* boss = me->FindNearestCreature(NPC_DIMENSIUS, 100.0f, true))
                    boss->RemoveAurasDueToSpell(SPELL_DIMENSIUS_TRANSFORM);

                Talk(NPC_SAEED_SAY_4);

                _events.ScheduleEvent(EVENT_DIMENSIUS_REPLY, 4s);
                break;
            case EVENT_DIMENSIUS_REPLY:
                if (Creature* boss = me->FindNearestCreature(NPC_DIMENSIUS, 100.0f, true))
                {
                    if (CreatureAI* ai = boss->AI())
                        ai->Talk(NPC_DIMENSIUS_SAY_2);

                    boss->SetImmuneToNPC(false);
                    boss->RemoveUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
                    boss->CastSpell(me, SPELL_DIMENSIUS_SHADOW_SPIRAL);
                }

                _events.ScheduleEvent(EVENT_ATTACK_DIMENSIUS, 4s);
                break;
            case EVENT_ATTACK_DIMENSIUS:
                SetEscortPaused(false);
                if (Creature* boss = me->FindNearestCreature(NPC_DIMENSIUS, 100.0f, true))
                {
                    boss->SetImmuneToPC(false);
                    AttackStart(boss);
                }
                break;
            case EVENT_FOLLOWER_SAY:
                if (Creature* follower = GetClosestCreatureWithEntry(me, NPC_PROTECTORATE_REGENERATOR, 100.f, true))
                    if (CreatureAI* ai = follower->AI())
                        ai->Talk(NPC_FOLLOWER_SAY);

                _events.ScheduleEvent(EVENT_TELEPORT, 4s);
                break;
            case EVENT_TELEPORT:
                DoCastSelf(SPELL_TELEPORT_SPELL_VISUAL, true);
                for (Creature* follower : GetFollowers())
                        follower->CastSpell(follower, SPELL_TELEPORT_SPELL_VISUAL, true);
                _events.ScheduleEvent(EVENT_DESPAWN, 2s);
                break;
            case EVENT_DESPAWN:
                for (Creature* follower : GetFollowers(false))
                    follower->DespawnOrUnsummon(0s, 5s);

                _followerGUIDs.clear();
                me->DespawnOrUnsummon(0s, 5s);
                break;
            case EVENT_CHECK_DISTANCE:
                CheckEscortDistance(diff);
                _events.ScheduleEvent(EVENT_CHECK_DISTANCE, 2s);
                break;
            default:
                break;
            }
        }
    }

    void WaypointReached(uint32 waypointId, uint32 /*pathId*/) override
    {
        me->SetHomePosition(me->GetPosition());
        _lastReachedWaypoint = waypointId;

        switch (waypointId)
        {
        case WAYPOINT_16:
            if (Player* player = GetPlayerForEscort())
                Talk(NPC_SAEED_SAY_2, player);

            SetEscortPaused(true);
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);

            _events.ScheduleEvent(EVENT_EMOTE_READY, 1s);
            break;
        case WAYPOINT_18:
            SetEscortPaused(true);
            _events.ScheduleEvent(EVENT_FRONT_FORMATION, 2s);
            break;
        case WAYPOINT_19:
            SetEscortPaused(true);
            _events.ScheduleEvent(EVENT_FOLLOWER_SAY, 2s);
            break;
        default:
            break;
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        for (ObjectGuid guid : _playersWithKillCredit)
            if (Player* player = ObjectAccessor::FindPlayer(guid))
                player->FailQuest(QUEST_DIMENSIUS_THE_ALL_DEVOURING);

        for (Creature* follower : GetFollowers(false))
            follower->DespawnOrUnsummon(1s, 5s);

        _followerGUIDs.clear();
        me->DespawnOrUnsummon(1s, 5s);
    }

protected:
    std::vector<Creature*> GetFollowers(bool onlyAlive = true)
    {
        std::vector<Creature*> followers;
        for (ObjectGuid guid : _followerGUIDs)
            if (Creature* follower = ObjectAccessor::GetCreature(*me, guid))
                if (!onlyAlive || follower->IsAlive())
                    followers.push_back(follower);

        return followers;
    }

    void CheckEscortDistance(uint32 diff)
    {
        Player* player = GetPlayerForEscort();
        if (!player)
            return;

        if (_lastReachedWaypoint >= WAYPOINT_18)
            return;

        bool _escortPaused = HasEscortState(STATE_ESCORT_PAUSED);

        if (me->GetDistance(player) > 20.f)
        {
            if (!_escortPaused)
            {
                SetEscortPaused(true);
                _playerAwayTime = 0;
                _playerAway = true;
            }
            else
            {
                _playerAwayTime += (diff + 2000);
                if (_playerAwayTime >= 20000)
                    me->DespawnOrUnsummon(1s, 5s);
            }
        }
        else if (_playerAway)
        {
            if (_escortPaused)
                SetEscortPaused(false);

            _playerAwayTime = 0;
            _playerAway = false;
        }
    }

private:
    EventMap _events;
    std::vector<ObjectGuid> _followerGUIDs;
    std::set<ObjectGuid> _playersWithKillCredit;
    ObjectGuid _questStarterGUID;
    uint32 _playerAwayTime;
    uint32 _lastReachedWaypoint;
    bool _playerAway;

};

/*Controll the SCRIPT EFFECT Spell 37425 Logic Tick (Dimensius) and used in the SAI Script from Dimensius*/
class spell_37425_logic_tick : public SpellScript
{
    PrepareSpellScript(spell_37425_logic_tick);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DIMENSIUS_SHADOW_RAIN_1, SPELL_DIMENSIUS_SHADOW_RAIN_2, SPELL_DIMENSIUS_SHADOW_RAIN_3, SPELL_DIMENSIUS_SHADOW_RAIN_4, SPELL_DIMENSIUS_SHADOW_RAIN_5 });
    }

    void HandleScriptEffect(SpellEffIndex /*effIndex*/)
    {
        uint32 spells[5] = { SPELL_DIMENSIUS_SHADOW_RAIN_1, SPELL_DIMENSIUS_SHADOW_RAIN_2, SPELL_DIMENSIUS_SHADOW_RAIN_3, SPELL_DIMENSIUS_SHADOW_RAIN_4, SPELL_DIMENSIUS_SHADOW_RAIN_5 };
        if (Unit* caster = GetCaster())
            caster->CastSpell(caster, spells[urand(0, 4)], true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_37425_logic_tick::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_gen_throw_back : public SpellScript
{
    PrepareSpellScript(spell_gen_throw_back);

    void HandleScriptEffect(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        if (Unit* target = GetHitUnit())
            target->CastSpell(GetCaster(), uint32(GetEffectValue()), true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_gen_throw_back::HandleScriptEffect, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

void AddSC_npc_captain_saeed()
{
    RegisterCreatureAI(npc_captain_saeed);
    RegisterCreatureAI(npc_saeed_follower);
    RegisterSpellScript(spell_37425_logic_tick);
    RegisterSpellScript(spell_gen_throw_back);
}

