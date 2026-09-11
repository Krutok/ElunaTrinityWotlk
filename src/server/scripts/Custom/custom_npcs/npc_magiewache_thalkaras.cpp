#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "Player.h"

enum Spells
{
    SPELL_TRESPASSER_A = 81514,
    SPELL_TRESPASSER_H = 81515
};

enum Events
{
    EVENT_SCAN = 1
};

struct npc_base_guard : public ScriptedAI
{
    npc_base_guard(Creature* creature) : ScriptedAI(creature) { }

private:
    EventMap _events;

    Position _hordeEntrance =
    {
        6382.674805f,
        -3286.543213f,
        184.094864f,
        0.151064f
    };

    Position _allianceEntrance =
    {
        7059.916016f,
        -3700.014648f,
        287.405334f,
        2.33804f
    };

    // 🔴 FIX: klare Rollenbestimmung
    bool IsHordeSide()
    {
        return me->GetDistance(_hordeEntrance) < 200.0f;
    }

    bool IsAllianceSide()
    {
        return me->GetDistance(_allianceEntrance) < 200.0f;
    }

    bool IsEntranceGuard()
    {
        return me->GetDistance(_hordeEntrance) < 30.0f ||
               me->GetDistance(_allianceEntrance) < 30.0f;
    }

    void Reset() override
    {
        _events.Reset();

        if (!IsEntranceGuard())
            _events.ScheduleEvent(EVENT_SCAN, 3s);
    }

    // 🔴 Dalaran Entry Logic (nur beim echten Reinlaufen)
    void MoveInLineOfSight(Unit* who) override
    {
        if (!IsEntranceGuard())
            return;

        if (!who || !who->IsInWorld())
            return;

        if (!me->IsWithinDist(who, 25.0f, false))
            return;

        Player* player = who->GetCharmerOrOwnerPlayerOrPlayerItself();

        if (!player || player->IsGameMaster() || player->IsBeingTeleported())
            return;

        if (player->HasAura(SPELL_TRESPASSER_A) || player->HasAura(SPELL_TRESPASSER_H))
            return;

        // 🔴 Dalaran Core Trick
        if (!me->isInBackInMap(player, 12.0f))
            return;

        // HORDE Entrance Guard
        if (IsHordeSide() && player->GetTeam() == ALLIANCE)
            DoCast(player, SPELL_TRESPASSER_H);

        // ALLIANCE Entrance Guard
        if (IsAllianceSide() && player->GetTeam() == HORDE)
            DoCast(player, SPELL_TRESPASSER_A);
    }

    // 🔴 Inner Guards = reine Sicherheitsschicht
    void UpdateAI(uint32 diff) override
    {
        if (IsEntranceGuard())
            return;

        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            if (eventId == EVENT_SCAN)
            {
                std::list<Player*> players;
                GetPlayerListInGrid(players, me, 100.0f);

                for (Player* player : players)
                {
                    if (!player || player->IsGameMaster() || player->IsBeingTeleported())
                        continue;

                    if (player->HasAura(SPELL_TRESPASSER_A) || player->HasAura(SPELL_TRESPASSER_H))
                        continue;

                    // 🔴 deterministisch: keine Rate, keine Tricks
                    if (IsHordeSide() && player->GetTeam() == ALLIANCE)
                        DoCast(player, SPELL_TRESPASSER_H);

                    if (IsAllianceSide() && player->GetTeam() == HORDE)
                        DoCast(player, SPELL_TRESPASSER_A);
                }

                _events.Repeat(3s);
            }
        }
    }
};

void AddSC_base_guard()
{
    RegisterCreatureAI(npc_base_guard);
}