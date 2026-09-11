#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "EventMap.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "Random.h"
#include <vector>
#include <algorithm>
#include <random>

/*
QUEST SUPPORT ALLIANCE 13295, 13297 / HORDE 13279, 13281 
*/

enum Neutralizing_the_Plague
{
    EVENT_WAVE_1                    =   1,
    EVENT_WAVE_2                    =   2,
    EVENT_WAVE_3                    =   3,
    EVENT_KILL_CREDIT               =   4,

    NPC_KILL_CREDIT                 =   31767,
    TRIGGER_NPC_1                   =   32427,
    TRIGGER_NPC_2                   =   31773,
    TRIGGER_NPC_3                   =   32442,

    SPELL_SUMMON_RAMPAGING_GHOUL    =   60056,
    SPELL_NEUTRALIZING_DOSE_APPLIED =   59659,
    SPELL_SUMMON_LIVING_PLAGUE      =   60058,
    SPELL_ORANGE_RADIATION          =   45797,
    SPELL_HUGE_GREEN_SPLASH         =   60059,

    TRIGGER_BOSS_EMOTE_0            =   0,
    TRIGGER_BOSS_EMOTE_1            =   1,
    TRIGGER_BOSS_EMOTE_2            =   2,
    TRIGGER_BOSS_EMOTE_3            =   3, 
    TRIGGER_BOSS_EMOTE_4            =   4,
    TRIGGER_BOSS_EMOTE_5            =   5,

    SWITCH_PHASE_1                  =   1,
    SWITCH_PHASE_3                  =   3
};

struct npc_quest_13297 : public ScriptedAI
{
    npc_quest_13297(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        _ownerGuid.Clear();
        events.Reset();
        _phase = 0;
        _eventsQueue.clear();
        _phase1Finished = false;
        _phase2Finished = false;
        _phase3Finished = false;
        _phase2Timer = 0;
        _phase2SpellHitReceived = false;
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spell) override
    {
        if (!caster || !caster->IsPlayer())
            return;

        Player* playerCaster = caster->ToPlayer();
        if (!playerCaster)
            return;

        uint32 entry = me->GetEntry();
        if (!(entry == TRIGGER_NPC_1 || entry == TRIGGER_NPC_2 || entry == TRIGGER_NPC_3))
            return;

        if (spell->Id != SPELL_NEUTRALIZING_DOSE_APPLIED) /*Spellhit spell comes from Itemspell 59655 Neutralize Plague*/
            return;

        if (_phase == 0)
        {
            _ownerGuid = playerCaster->GetGUID();
            DoCastSelf(SPELL_ORANGE_RADIATION, false);
            Talk(TRIGGER_BOSS_EMOTE_0);

            _phase = 1;
            _phase1Finished = false;

            PreparePhaseEvents();
            if (!_eventsQueue.empty())
            {
                uint32 nextEvent = _eventsQueue.front();
                _eventsQueue.erase(_eventsQueue.begin());
                events.ScheduleEvent(nextEvent, 10s); /*First wave phase 1 after 10s*/
            }
        }
        else if (_phase == 2)
        {
            _phase2SpellHitReceived = true; /*Register Phase2 SpellHit*/
            Talk(TRIGGER_BOSS_EMOTE_2);

            _phase = 3;
            PreparePhaseEvents();

            if (!_eventsQueue.empty())
            {
                uint32 nextEvent = _eventsQueue.front();
                _eventsQueue.erase(_eventsQueue.begin());
                events.ScheduleEvent(nextEvent, 10s); /*First Phase3 wave after 10s*/
            }
        }
    }
    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);

        if (_phase == 2 && _phase2Timer > 0)
        {
            if (_phase2Timer <= diff)
            {
                _phase2Timer = 0;
                if (!_phase2SpellHitReceived)
                {
                    ResetPhase2Failure();
                    return;
                }
                else
                {
                    _phase2SpellHitReceived = false; /*Successful SpellHit ? Start Phase3*/
                    _phase = 3;
                    PreparePhaseEvents();
                    if (!_eventsQueue.empty())
                    {
                        uint32 nextEvent = _eventsQueue.front();
                        _eventsQueue.erase(_eventsQueue.begin());
                        events.ScheduleEvent(nextEvent, 10s);
                    }
                }
            }
            else
            {
                _phase2Timer -= diff;
            }
        }

        while (uint32 eventId = events.ExecuteEvent())
        {
            if (!me->HasAura(SPELL_ORANGE_RADIATION))
            {
                events.Reset();
                _phase = 0;
                _eventsQueue.clear();
                _phase1Finished = _phase2Finished = _phase3Finished = false;
                return;
            }

            Player* player = ObjectAccessor::GetPlayer(*me, _ownerGuid);
            if (!player)
                continue;

            switch (eventId)
            {
            case EVENT_WAVE_1:
                Talk(TRIGGER_BOSS_EMOTE_1);
                DoCastSelf(SPELL_HUGE_GREEN_SPLASH, false);
                for (int i = 0; i < 2; ++i)
                    player->CastSpell(player, SPELL_SUMMON_RAMPAGING_GHOUL, false);
                break;

            case EVENT_WAVE_2:
                DoCastSelf(SPELL_HUGE_GREEN_SPLASH, false);
                Talk(TRIGGER_BOSS_EMOTE_1);
                for (int i = 0; i < 5; ++i)
                    player->CastSpell(player, SPELL_SUMMON_LIVING_PLAGUE, false);
                break;

            case EVENT_WAVE_3:
                DoCastSelf(SPELL_HUGE_GREEN_SPLASH, false);
                Talk(TRIGGER_BOSS_EMOTE_1);
                player->CastSpell(player, SPELL_SUMMON_RAMPAGING_GHOUL, false);
                for (int i = 0; i < 5; ++i)
                    player->CastSpell(player, SPELL_SUMMON_LIVING_PLAGUE, false);
                break;

            case EVENT_KILL_CREDIT:
            {
                Talk(TRIGGER_BOSS_EMOTE_3);
                std::list<Player*> playerList;
                me->GetPlayerListInGrid(playerList, 40.0f);
                for (Player* p : playerList)
                    p->KilledMonsterCredit(NPC_KILL_CREDIT);

                me->RemoveAurasDueToSpell(SPELL_ORANGE_RADIATION);
                events.Reset();
                _phase = 0;
                _eventsQueue.clear();
                _phase1Finished = _phase2Finished = _phase3Finished = true;
                return;
                }
            }
            ScheduleNextPhaseEvents();
        }
    }

    void PreparePhaseEvents()
    {
        _eventsQueue.clear();
        for (int i = 0; i < 2; ++i)
            _eventsQueue.push_back(urand(1, 3));

        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(_eventsQueue.begin(), _eventsQueue.end(), g);
    }

    void ScheduleNextPhaseEvents()
    {
        if (!_eventsQueue.empty())
        {
            uint32 nextEvent = _eventsQueue.front();
            _eventsQueue.erase(_eventsQueue.begin());
            events.ScheduleEvent(nextEvent, 30s); /*Phase 1 and Phase 3 interval: 30s after previous wave*/
            Talk(TRIGGER_BOSS_EMOTE_2);
        }
        else
        {
            switch (_phase)
            {
            case SWITCH_PHASE_1:
                _phase1Finished = true;
                _phase = 2;
                StartPhase2();
                break;

            case SWITCH_PHASE_3:
                _phase3Finished = true;
                events.ScheduleEvent(EVENT_KILL_CREDIT, 30s); /*EVENT_KILL_CREDIT 30s after last Phase3 wave*/
                break;
            }
        }
    }

    void StartPhase2()
    {
        _phase2Timer = 60 * IN_MILLISECONDS; /*Time limit 60 Seconds for using the quest item*/
        _phase2SpellHitReceived = false;
        Talk(TRIGGER_BOSS_EMOTE_4);
    }

    void ResetPhase2Failure()
    {
        Talk(TRIGGER_BOSS_EMOTE_5);
        _phase2Timer = 0;
        _phase2SpellHitReceived = false;
        _phase2Finished = false;
        _eventsQueue.clear();
        _phase = 0;
        me->RemoveAurasDueToSpell(SPELL_ORANGE_RADIATION);
    }

private:
    ObjectGuid _ownerGuid;
    EventMap events;
    uint8 _phase;
    std::vector<uint32> _eventsQueue;
    bool _phase1Finished;
    bool _phase2Finished;
    bool _phase3Finished;
    uint32 _phase2Timer;
    bool _phase2SpellHitReceived;
};

CreatureAI* GetAI_npc_quest_13297(Creature* creature)
{
    return new npc_quest_13297(creature);
}

void AddSC_npc_quest_13297()
{
    RegisterCreatureAI(npc_quest_13297);
}
