#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "Player.h"
#include "SpellInfo.h"
#include "ObjectAccessor.h"
#include "ScriptMgr.h"
#include "MotionMaster.h"
#include "EventMap.h"
#include "TemporarySummon.h"
#include "DatabaseEnv.h"
#include "ObjectMgr.h"

enum eEris
{
    QUEST_BALANCE_OF_LIGHT_AND_SHADOW   = 7622,
    ITEM_EYE_OF_DIVINITY                = 18646,

    NPC_INJURED_PEASANT                 = 14484,
    NPC_PLAGUED_PEASANT                 = 14485,
    NPC_SCOURGE_ARCHER                  = 14489,

    EVENT_SUMMON_PEASANTS               = 1,
    EVENT_CHECK_PLAYER                  = 2,
    EVENT_SUMMON_ARCHERS                = 3,

    SPELL_SHOOT                         = 23073,
    SPELL_DEATHS_DOOR                   = 23127,
    SPELL_SEETHING_PLAGUE               = 23072,
    SPELL_ERIS_BLESSING                 = 23108,
};

// -----------------------------------
// npc_eris_hevenfire
// -----------------------------------

struct npc_eris_hevenfire : public ScriptedAI
{
    npc_eris_hevenfire(Creature* c) : ScriptedAI(c), summons(me) {}

    SummonList summons;
    EventMap events;
    ObjectGuid _lastSavedPeasantGUID;
    ObjectGuid _playerGUID;
    uint8 _counter;
    uint8 _savedCount;
    uint8 _deathCount;
    bool _spoken;
    uint32 _faction;

    void Reset() override
    {
        _faction = 0;
        _spoken = false;
        _savedCount = 0;
        _deathCount = 0;
        _counter = 0;
        _playerGUID.Clear();
        events.Reset();
        summons.DespawnAll();
        me->ReplaceAllNpcFlags(UNIT_NPC_FLAG_GOSSIP | UNIT_NPC_FLAG_QUESTGIVER);
    }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == QUEST_BALANCE_OF_LIGHT_AND_SHADOW)
        {
            _faction = player->GetFaction();
            _playerGUID = player->GetGUID();
            me->ReplaceAllNpcFlags(UNIT_NPC_FLAG_NONE);

            events.Reset();
            summons.DespawnAll();

            events.ScheduleEvent(EVENT_CHECK_PLAYER, 1s);
            events.ScheduleEvent(EVENT_SUMMON_ARCHERS, 4s);
            events.ScheduleEvent(EVENT_SUMMON_PEASANTS, 8s);
        }
    }

    bool CanBeSeen(Player const* player)
    {
        return player->HasItemOrGemWithIdEquipped(ITEM_EYE_OF_DIVINITY, 1);
    }

    void SummonArchers()
    {
        me->SummonCreature(NPC_SCOURGE_ARCHER, 3330.17f, -3078.96f, 172.23f, 0.799463f);
        me->SummonCreature(NPC_SCOURGE_ARCHER, 3328.32f, -3017.87f, 172.72f, 6.26976f);
        me->SummonCreature(NPC_SCOURGE_ARCHER, 3333.70f, -3052.39f, 174.91f, 0.391055f);
        me->SummonCreature(NPC_SCOURGE_ARCHER, 3316.21f, -3035.48f, 167.17f, 0.163288f);
        me->SummonCreature(NPC_SCOURGE_ARCHER, 3371.54f, -3067.75f, 174.942f, 1.96578f);
        me->SummonCreature(NPC_SCOURGE_ARCHER, 3377.37f, -3060.13f, 181.13f, 2.82186f);
        me->SummonCreature(NPC_SCOURGE_ARCHER, 3352.44f, -3079.01f, 179.07f, 1.32175f);
        me->SummonCreature(NPC_SCOURGE_ARCHER, 3363.07f, -3077.43f, 183.0f, 1.78121f);
        me->SummonCreature(NPC_SCOURGE_ARCHER, 3348.38f, -2989.09f, 174.58f, 4.07064f);
        me->SummonCreature(NPC_SCOURGE_ARCHER, 3377.82f, -3039.79f, 173.69f, 3.20671f);
        me->SummonCreature(NPC_SCOURGE_ARCHER, 3364.66f, -3007.18f, 188.13f, 3.81932f);
    }

    void SummonPeasants()
    {
        for (uint8 i = 0; i < 12; ++i)
        {
            float x = 3358 + frand(-6.0f, 6.0f);
            float y = -3049 + frand(-6.0f, 6.0f);
            float z = 165.25f;
            float o = 2.0f;
            me->SummonCreature(roll_chance_i(5) ? NPC_PLAGUED_PEASANT : NPC_INJURED_PEASANT, x, y, z, o, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 2min);
        }
    }

    void JustSummoned(Creature* creature) override
    {
        summons.Summon(creature);

        if (creature->GetEntry() == NPC_SCOURGE_ARCHER)
        {
            creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            return;
        }

        if (creature->GetEntry() == NPC_INJURED_PEASANT || creature->GetEntry() == NPC_PLAGUED_PEASANT)
        {
            creature->SetFaction(_faction);
            if (!_spoken)
            {
                _spoken = true;
                creature->AI()->Talk(0);
            }

            if (creature->GetEntry() == NPC_PLAGUED_PEASANT)
                creature->CastSpell(creature, SPELL_SEETHING_PLAGUE, true);

            float x = 3324 + frand(-3.0f, 3.0f);
            float y = -2966 + frand(-3.0f, 3.0f);
            float z = 159.65f;
            creature->SetRegenerateHealth(false);
            creature->SetWalk(true);
            creature->SetSpeed(MOVE_WALK, 1.2f);
            creature->GetMotionMaster()->MovePoint(0, x, y, z);
            creature->SetUnitFlag(UNIT_FLAG_PLAYER_CONTROLLED);
        }
    }

    void DoAction(int32 action) override
    {
        if (action == 1)
        {
            _savedCount++;

            if (!_spoken && !_lastSavedPeasantGUID.IsEmpty())
            {
                _spoken = true;

                if (Unit* creature = ObjectAccessor::GetUnit(*me, _lastSavedPeasantGUID))
                {
                    if (creature->ToCreature() && creature->ToCreature()->AI())
                        creature->ToCreature()->AI()->Talk(2);
                }

                me->CastSpell(me, SPELL_ERIS_BLESSING, false);
            }
        }
        else
        {
            _deathCount++;
        }

        if (_savedCount > 49)
        {
            Talk(0);
            if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                player->AreaExploredOrEventHappens(QUEST_BALANCE_OF_LIGHT_AND_SHADOW);
            EnterEvadeMode();
        }
        else if (_deathCount > 14)
        {
            Talk(1);
            if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                player->FailQuest(QUEST_BALANCE_OF_LIGHT_AND_SHADOW);
            EnterEvadeMode();
        }
    }


    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_CHECK_PLAYER:
                {
                    Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
                    if (!player || me->GetDistance2d(player) > 100.0f)
                    {
                        EnterEvadeMode();
                        return;
                    }
                    events.Repeat(2s);
                    break;
                }
                case EVENT_SUMMON_ARCHERS:
                    SummonArchers();
                    break;
                case EVENT_SUMMON_PEASANTS:
                    _spoken = false;
                    SummonPeasants();
                    _spoken = false;
                    events.Repeat(60s);
                    break;
            }
        }
    }
};

// -----------------------------------
// npc_balance_of_light_and_shadow
// -----------------------------------

struct npc_balance_of_light_and_shadow : public ScriptedAI
{
    npc_balance_of_light_and_shadow(Creature* creature) : ScriptedAI(creature)
    {
        timer = 0;
        _targetGUID.Clear();
    }

    bool CanBeSeen(Player const* player)
    {
        return player->HasItemOrGemWithIdEquipped(ITEM_EYE_OF_DIVINITY, 1);
    }

    uint32 timer;
    ObjectGuid _targetGUID;

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        (void)caster;
        if (!spellInfo)
            return;

        if (spellInfo->Id == SPELL_SHOOT && roll_chance_i(7))
            me->CastSpell(me, SPELL_DEATHS_DOOR, true);
    }

    void MovementInform(uint32 type, uint32 /*pointId*/) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        if (TempSummon* summon = me->ToTempSummon())
        {
            if (Unit* creature = summon->GetSummonerUnit())
            {
                auto ai = creature->GetAI();
                if (ai)
                {
                    static_cast<npc_eris_hevenfire*>(ai)->_lastSavedPeasantGUID = me->GetGUID();
                    ai->DoAction(1);
                }
            }
        }

        me->DespawnOrUnsummon(1s);
    }

    void JustDied(Unit*) override
    {
        if (TempSummon* summon = me->ToTempSummon())
            if (Unit* creature = summon->GetSummonerUnit())
                creature->GetAI()->DoAction(2);
    }

    void UpdateAI(uint32 diff) override
    {
        if (me->GetEntry() != NPC_SCOURGE_ARCHER)
            return;

        timer += diff;
        if (timer >= 4000)
        {
            Unit* target = !_targetGUID.IsEmpty() ? ObjectAccessor::GetUnit(*me, _targetGUID) : nullptr;
            if (!target)
                target = me->FindNearestCreature(NPC_INJURED_PEASANT, 60.0f);

            if (target)
            {
                _targetGUID = target->GetGUID();
                me->CastSpell(target, SPELL_SHOOT, true);
            }

            timer = urand(0, 3000);
        }
    }
};


void AddSC_npc_eris_hevenfire()
{
    RegisterCreatureAI(npc_eris_hevenfire);
    RegisterCreatureAI(npc_balance_of_light_and_shadow);
}
