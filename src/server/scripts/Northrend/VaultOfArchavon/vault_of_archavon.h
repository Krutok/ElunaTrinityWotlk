/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DEF_ARCHAVON_H
#define DEF_ARCHAVON_H

#include "CreatureAIImpl.h"

#define VoAScriptName "instance_vault_of_archavon"
#define DataHeader "VA"

uint32 const EncounterCount = 4;

enum VAData
{
    DATA_ARCHAVON = 0,
    DATA_EMALON = 1,
    DATA_KORALON = 2,
    DATA_TORAVON = 3,
    DATA_VOA_TRIGGER = 4,
};

enum VACreatureIds
{
    NPC_ARCHAVON = 31125,
    NPC_EMALON = 33993,
    NPC_KORALON = 35013,
    NPC_TORAVON = 38433
};

enum VAAchievementCriteriaIds
{
    CRITERIA_EARTH_WIND_FIRE_10 = 12018,
    CRITERIA_EARTH_WIND_FIRE_25 = 12019
};

enum VAAchievementSpells
{
    SPELL_EARTH_WIND_FIRE_ACHIEVEMENT_CHECK = 68308
};

enum VASpells
{
    SPELL_VA_STONE_FORM = 70733,
    SPELL_VA_STONE_FORM_1 = 34712
};

struct VaultOfArchavonBossAI : public BossAI
{
    VaultOfArchavonBossAI(Creature* creature, uint32 bossId) : BossAI(creature, bossId), _closeEncounterPending(false) {}

    virtual void CloseEncounter()
    {

        events.Reset();

        me->InterruptNonMeleeSpells(true);
        me->AttackStop();
        me->SetReactState(REACT_PASSIVE);

        _closeEncounterPending = true;

        if (!me->IsInEvadeMode())
        {
            if (!_EnterEvadeMode(EVADE_REASON_OTHER))
            {
                _closeEncounterPending = false;
                return;
            }
        }

        me->AddUnitState(UNIT_STATE_EVADE);
        me->GetMotionMaster()->MoveTargetedHome();
    }

    void JustReachedHome() override
    {
        BossAI::JustReachedHome();

        if (!_closeEncounterPending)
            return;

        _closeEncounterPending = false;

        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC);
        me->CastSpell(me, 65124, true);
        me->CastSpell(me, SPELL_VA_STONE_FORM, true);
        me->CastSpell(me, SPELL_VA_STONE_FORM_1, true);
    }

private:
    bool _closeEncounterPending;
};

template <class AI, class T>
inline AI* GetVaultOfArchavonAI(T* obj)
{
    return GetInstanceAI<AI>(obj, VoAScriptName);
}

#define RegisterVaultOfArchavonCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetVaultOfArchavonAI)

#endif
