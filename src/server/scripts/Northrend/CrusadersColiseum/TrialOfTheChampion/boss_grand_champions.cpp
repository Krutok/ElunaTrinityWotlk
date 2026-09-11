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

#include "ScriptMgr.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedEscortAI.h"
#include "trial_of_the_champion.h"
#include "Vehicle.h"
#include "CombatAI.h"
#include "PassiveAI.h"
#include "ScriptedCreature.h"
#include "SpellInfo.h"
#include "SpellMgr.h"

enum MountSpells
{
    SPELL_LANCE_EQUIPPED = 62853,
    SPELL_PLAYER_VEHICLE_DEFEND = 66482,
    SPELL_MINIONS_DEFEND = 64100,
    SPELL_BOSS_DEFEND = 62719,
    SPELL_BOSS_DEFEND_PERIODIC = 64553,
    SPELL_SHIELD_LEVEL_1_VISUAL = 63130,
    SPELL_SHIELD_LEVEL_2_VISUAL = 63131,
    SPELL_SHIELD_LEVEL_3_VISUAL = 63132,

    SPELL_PLAYER_VEHICLE_SHIELD_BREAKER = 62575,
    SPELL_PLAYER_SHIELD_BREAKER_DAMAGE = 62626,
    SPELL_NPC_SHIELD_BREAKER = 68504,

    SPELL_PLAYER_VEHICLE_CHARGE = 68284,
    SPELL_CHARGE_DAMAGE_20000 = 68498,
    SPELL_MINIONS_CHARGE = 63010,
    SPELL_BOSS_CHARGE = 68301, // triggers SPELL_MINIONS_CHARGE (should be with custom damage?)

    SPELL_PLAYER_VEHICLE_THRUST = 68505,

    SPELL_TRAMPLE_AURA = 67865,
    SPELL_TRAMPLE_TRIGGERED_DUMMY = 67866,
    SPELL_TRAMPLE_STUN = 67867,
};

enum ChampionSpells
{
    // Mage (Ambrose Boltspark, Eressea Dawnsinger)
    SPELL_FIREBALL = 66042,
    SPELL_BLAST_WAVE = 66044,
    SPELL_HASTE = 66045,
    SPELL_POLYMORPH = 66043,

    // Shaman (Colosos, Runok Wildmane)
    SPELL_CHAIN_LIGHTNING = 67529,
    SPELL_EARTH_SHIELD = 67530,
    SPELL_HEALING_WAVE = 67528,
    SPELL_HEX_OF_MENDING = 67534,
    SPELL_HEX_OF_MENDING_HEAL = 67535,

    // Hunter (Jaelyne Evensong, Zul'tore)
    SPELL_DISENGAGE = 68339,
    SPELL_LIGHTNING_ARROWS = 66083,
    SPELL_MULTI_SHOT = 66081,
    SPELL_SHOOT = 65868,

    // Rogue (Lana Stouthammer Evensong, Deathstalker Visceri)
    SPELL_EVISCERATE = 67709,
    SPELL_FAN_OF_KNIVES = 67706,
    SPELL_POISON_BOTTLE = 67701,

    // Warrior (Marshal Jacob Alerius, Mokra the Skullcrusher)
    SPELL_MORTAL_STRIKE = 68783,
    SPELL_BLADESTORM = 63784,
    SPELL_INTERCEPT = 67540,
    SPELL_ROLLING_THROW = 67546, // not implemented yet!

    // Banners
    SPELL_BANNER_MOKRA = 63433,
    SPELL_BANNER_ERESSEA = 63403,
    SPELL_BANNER_RUNOK = 63436,
    SPELL_BANNER_ZULTORE = 63399,
    SPELL_BANNER_VISCERI = 63430,

    SPELL_BANNER_JACOB = 62594,
    SPELL_BANNER_AMBROSE = 63396,
    SPELL_BANNER_COLOSOS = 63423,
    SPELL_BANNER_JAELYNE = 63406,
    SPELL_BANNER_LANA = 63427,
};

enum Texts
{
    SAY_TRAMPLED = 0,
};

enum MountEvents
{
    EVENT_NONE = 0,
    EVENT_MOUNT_CHARGE,
    EVENT_SHIELD_BREAKER,
    EVENT_THRUST,
    EVENT_FIND_NEW_MOUNT,
};

enum ChampionEvents
{
    EVEMT_MAGE_SPELL_FIREBALL = 101,
    EVEMT_MAGE_SPELL_BLAST_WAVE,
    EVEMT_MAGE_SPELL_HASTE,
    EVEMT_MAGE_SPELL_POLYMORPH,

    EVENT_SHAMAN_SPELL_CHAIN_LIGHTNING,
    EVENT_SHAMAN_SPELL_EARTH_SHIELD,
    EVENT_SHAMAN_SPELL_HEALING_WAVE,
    EVENT_SHAMAN_SPELL_HEX_OF_MENDING,

    EVENT_HUNTER_SPELL_DISENGAGE,
    EVENT_HUNTER_SPELL_LIGHTNING_ARROWS,
    EVENT_HUNTER_SPELL_MULTI_SHOT,
    EVENT_HUNTER_SPELL_SHOOT,

    EVENT_ROGUE_SPELL_EVISCERATE,
    EVENT_ROGUE_SPELL_FAN_OF_KNIVES,
    EVENT_ROGUE_SPELL_POISON_BOTTLE,

    EVENT_WARRIOR_SPELL_MORTAL_STRIKE,
    EVENT_WARRIOR_SPELL_BLADESTORM,
    EVENT_WARRIOR_SPELL_INTERCEPT,
    EVENT_WARRIOR_SPELL_ROLLING_THROW,
};

class npc_toc5_player_vehicle : public CreatureScript
{
public:
    npc_toc5_player_vehicle() : CreatureScript("npc_toc5_player_vehicle") {}

    CreatureAI* GetAI(Creature* pCreature) const override
    {
        return GetTrialOfTheChampionAI<npc_toc5_player_vehicleAI>(pCreature);
    }

    struct npc_toc5_player_vehicleAI : public VehicleAI
    {
        npc_toc5_player_vehicleAI(Creature* creature) : VehicleAI(creature) {}

        void Reset() override
        {
            me->SetReactState(REACT_PASSIVE);

            InstanceScript* script = me->GetInstanceScript();
            if (!script)
                return;

            TeamId instanceTeamId = TeamId(script->GetData(DATA_TEAMID_IN_INSTANCE));

            if (me->GetEntry() == VEHICLE_ARGENT_WARHORSE)
            {
                if (instanceTeamId == TEAM_ALLIANCE)
                {
                    me->SetFaction(35);
                    me->SetImmuneToPC(false);
                }
                else
                {
                    me->SetFaction(14);
                    me->SetImmuneToPC(true);
                }
            }
            else
            {
                if (instanceTeamId == TEAM_HORDE)
                {
                    me->SetFaction(35);
                    me->SetImmuneToPC(false);
                }
                else
                {
                    me->SetFaction(14);
                    me->SetImmuneToPC(true);
                }
            }
        }

        void OnCharmed(bool apply) override
        {
            if (me->IsDuringRemoveFromWorld())
                return;

            if (apply)
            {
                me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                me->CastSpell(me, SPELL_TRAMPLE_AURA, true);
            }
            else
            {
                me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                me->StopMoving();
                me->RemoveAura(SPELL_TRAMPLE_AURA);
            }
        }

        // just in case, should be done in spell_gen_defend
        void PassengerBoarded(Unit* who, int8  /*seat*/, bool apply) override
        {
            if (me->IsDuringRemoveFromWorld())
                return;

            if (!apply)
            {
                me->RemoveAura(SPELL_PLAYER_VEHICLE_DEFEND);
                who->RemoveAura(SPELL_PLAYER_VEHICLE_DEFEND);
                for (uint8 i = 0; i < 3; ++i)
                    who->RemoveAura(SPELL_SHIELD_LEVEL_1_VISUAL + i);
            }
            else
            {
                me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                me->CastSpell(me, SPELL_TRAMPLE_AURA, true);
            }
        }

        bool BeforeSpellClick(Unit* clicker) override
        {
            if (!clicker->IsPlayer())
                return true;

            if (clicker->IsInDisallowedMountForm())
                clicker->RemoveAurasByType(SPELL_AURA_MOD_SHAPESHIFT);

            if (clicker->HasAura(SPELL_LANCE_EQUIPPED))
                return true;

            WorldPacket data(SMSG_CAST_FAILED, 1 + 4 + 1);
            data << uint8(0); // single cast or multi 2.3 (0/1)
            data << uint32(VEHICLE_SPELL_RIDE_HARDCODED);
            data << uint8(SPELL_FAILED_CUSTOM_ERROR);
            data << uint32(SPELL_CUSTOM_ERROR_MUST_HAVE_LANCE_EQUIPPED);
            clicker->ToPlayer()->SendDirectMessage(&data);
            return false;
        }
    };
};

class npc_toc5_grand_champion_minion : public CreatureScript
{
public:
    npc_toc5_grand_champion_minion() : CreatureScript("npc_toc5_grand_champion_minion") {}

    CreatureAI* GetAI(Creature* pCreature) const override
    {
        return GetTrialOfTheChampionAI<npc_toc5_grand_champion_minionAI>(pCreature);
    }

    struct npc_toc5_grand_champion_minionAI : public ScriptedAI
    {
        npc_toc5_grand_champion_minionAI(Creature* pCreature) : ScriptedAI(pCreature)
        {
            pInstance = pCreature->GetInstanceScript();
        }

        InstanceScript* pInstance;
        int32 ShieldTimer;
        EventMap events;

        void Reset() override
        {
            ShieldTimer = 0;
            events.Reset();
        }

        void MoveInLineOfSight(Unit* who) override
        {
            if (pInstance && pInstance->GetData(DATA_INSTANCE_PROGRESS) >= INSTANCE_PROGRESS_GRAND_CHAMPIONS_REACHED_DEST)
                ScriptedAI::MoveInLineOfSight(who);
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.Reset();
            events.ScheduleEvent(EVENT_MOUNT_CHARGE, 2500ms, 4s);
            events.ScheduleEvent(EVENT_SHIELD_BREAKER, 5s, 8s);
            events.ScheduleEvent(EVENT_THRUST, 3s, 5s);
            me->CastSpell(me, SPELL_TRAMPLE_AURA, true);
        }

        void UpdateAI(uint32 diff) override
        {
            if (ShieldTimer <= (int32)diff)
            {
                me->CastSpell(me, SPELL_MINIONS_DEFEND, true);
                ShieldTimer = 5000;
            }
            else
                ShieldTimer -= diff;

            if (!UpdateVictim())
                return;

            events.Update(diff);

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            switch (events.ExecuteEvent())
            {
            case 0:
                break;
            case EVENT_MOUNT_CHARGE:
            {
                GuidVector LIST;
                Map::PlayerList const& pl = me->GetMap()->GetPlayers();
                for (Map::PlayerList::const_iterator itr = pl.begin(); itr != pl.end(); ++itr)
                    if (Player* plr = itr->GetSource())
                    {
                        if (me->GetExactDist(plr) < 8.0f || me->GetExactDist(plr) > 25.0f || plr->isDead())
                            continue;
                        if (!plr->GetVehicle())
                            LIST.push_back(plr->GetGUID());
                        else if (Vehicle* v = plr->GetVehicle())
                        {
                            if (Unit* mount = v->GetBase())
                                LIST.push_back(mount->GetGUID());
                        }
                    }
                if (!LIST.empty())
                {
                    uint8 rnd = LIST.size() > 1 ? urand(0, LIST.size() - 1) : 0;
                    if (Unit* target = ObjectAccessor::GetUnit(*me, LIST.at(rnd)))
                    {
                        me->GetThreatManager().ResetAllThreat();
                        me->GetThreatManager().AddThreat(target, 10000.f);
                        AttackStart(target);
                        me->CastSpell(target, SPELL_MINIONS_CHARGE, false);
                    }
                }
                events.Repeat(4500ms, 6s);
            }
            break;
            case EVENT_SHIELD_BREAKER:
            {
                GuidVector LIST;
                Map::PlayerList const& pl = me->GetMap()->GetPlayers();
                for (Map::PlayerList::const_iterator itr = pl.begin(); itr != pl.end(); ++itr)
                    if (Player* plr = itr->GetSource())
                    {
                        if (me->GetExactDist(plr) < 10.0f || me->GetExactDist(plr) > 30.0f)
                            continue;
                        if (Vehicle* v = plr->GetVehicle())
                            if (Unit* mount = v->GetBase())
                                LIST.push_back(mount->GetGUID());
                    }
                if (!LIST.empty())
                {
                    uint8 rnd = LIST.size() > 1 ? urand(0, LIST.size() - 1) : 0;
                    if (Unit* target = ObjectAccessor::GetCreature(*me, LIST.at(rnd)))
                        me->CastSpell(target, SPELL_NPC_SHIELD_BREAKER, false);
                }
                events.Repeat(6s, 8s);
            }
            break;
            case EVENT_THRUST:
                if (me->GetVictim() && me->GetExactDist(me->GetVictim()) <= 5.5f)
                    me->CastSpell(me->GetVictim(), SPELL_PLAYER_VEHICLE_THRUST, false);
                events.Repeat(3s, 5s);
                break;
            }
        }

        void JustDied(Unit* /*pKiller*/) override
        {
            me->SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 0);
            me->DespawnOrUnsummon(10s);
            if (pInstance)
                pInstance->SetData(DATA_MOUNT_DIED, 0);
        }
    };
};

class boss_grand_champion : public CreatureScript
{
public:
    boss_grand_champion() : CreatureScript("boss_grand_champion") {}

    struct boss_grand_championAI : public EscortAI
    {
        boss_grand_championAI(Creature* pCreature) : EscortAI(pCreature)
        {
            pInstance = pCreature->GetInstanceScript();
            MountPhase = true;
            SetDespawnAtEnd(false);
            me->SetReactState(REACT_PASSIVE);
            BossOrder = 0;
            NewMountGUID.Clear();
            me->CastSpell(me, SPELL_BOSS_DEFEND_PERIODIC, true);

            events.Reset();
            events.ScheduleEvent(EVENT_MOUNT_CHARGE, 2500ms, 4s);
            events.ScheduleEvent(EVENT_SHIELD_BREAKER, 5s, 8s);
            events.ScheduleEvent(EVENT_THRUST, 3s, 5s);

            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_CHARM, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_DISORIENTED, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_DISTRACT, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_FEAR, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_GRIP, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_ROOT, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SLEEP, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SNARE, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_FREEZE, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_KNOCKOUT, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_POLYMORPH, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_BANISH, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SHACKLE, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_TURN, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_HORROR, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_INTERRUPT, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_DAZE, true);
            me->ApplySpellImmune(0, IMMUNITY_MECHANIC, MECHANIC_SAPPED, true);
        }

        InstanceScript* pInstance;
        EventMap events;
        uint32 BossOrder;
        bool MountPhase;
        ObjectGuid NewMountGUID;
        ObjectGuid UnitTargetGUID;

        void Reset() override
        {
            if (pInstance && pInstance->GetData(DATA_INSTANCE_PROGRESS) == INSTANCE_PROGRESS_CHAMPIONS_UNMOUNTED)
            {
                DoAction(1);
                me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                me->SetImmuneToAll(false);
                me->SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 0);
                me->SetReactState(REACT_AGGRESSIVE);
                me->SetSheath(SHEATH_STATE_MELEE);
            }
        }

        void MoveInLineOfSight(Unit* who) override
        {
            if (pInstance && pInstance->GetData(DATA_INSTANCE_PROGRESS) >= INSTANCE_PROGRESS_GRAND_CHAMPIONS_REACHED_DEST)
                EscortAI::MoveInLineOfSight(who);
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
	
		ScheduleAbilitiesEvents();

            if (pInstance && pInstance->GetData(DATA_INSTANCE_PROGRESS) == INSTANCE_PROGRESS_CHAMPIONS_UNMOUNTED)
                pInstance->SetData(DATA_GRAND_CHAMPION_ENGAGE, 0);
        }

        void ScheduleAbilitiesEvents()
        {
            me->m_spellImmune[IMMUNITY_MECHANIC].clear();
            events.Reset();
            switch (me->GetEntry())
            {
            case NPC_AMBROSE: // Ambrose Boltspark
            case NPC_ERESSEA: // Eressea Dawnsinger
                events.RescheduleEvent(EVEMT_MAGE_SPELL_FIREBALL, 5s);
                events.RescheduleEvent(EVEMT_MAGE_SPELL_BLAST_WAVE, 12s);
                events.RescheduleEvent(EVEMT_MAGE_SPELL_HASTE, 22s);
                events.RescheduleEvent(EVEMT_MAGE_SPELL_POLYMORPH, 8s);
                break;
            case NPC_COLOSOS: // Colosos
            case NPC_RUNOK: // Runok Wildmane
                events.RescheduleEvent(EVENT_SHAMAN_SPELL_CHAIN_LIGHTNING, 16s);
                events.RescheduleEvent(EVENT_SHAMAN_SPELL_EARTH_SHIELD, 30s, 35s);
                events.RescheduleEvent(EVENT_SHAMAN_SPELL_HEALING_WAVE, 12s);
                events.RescheduleEvent(EVENT_SHAMAN_SPELL_HEX_OF_MENDING, 20s, 25s);
                break;
            case NPC_JAELYNE: // Jaelyne Evensong
            case NPC_ZULTORE: // Zul'tore
                events.RescheduleEvent(EVENT_HUNTER_SPELL_LIGHTNING_ARROWS, 7s);
                events.RescheduleEvent(EVENT_HUNTER_SPELL_MULTI_SHOT, 12s);
                break;
            case NPC_LANA: // Lana Stouthammer
            case NPC_VISCERI: // Deathstalker Visceri
                events.RescheduleEvent(EVENT_ROGUE_SPELL_EVISCERATE, 8s);
                events.RescheduleEvent(EVENT_ROGUE_SPELL_FAN_OF_KNIVES, 14s);
                events.RescheduleEvent(EVENT_ROGUE_SPELL_POISON_BOTTLE, 19s);
                break;
            case NPC_JACOB: // Marshal Jacob Alerius
            case NPC_MOKRA: // Mokra the Skullcrusher
                events.RescheduleEvent(EVENT_WARRIOR_SPELL_MORTAL_STRIKE, 8s, 12s);
                events.RescheduleEvent(EVENT_WARRIOR_SPELL_BLADESTORM, 15s, 20s);
                events.RescheduleEvent(EVENT_WARRIOR_SPELL_INTERCEPT, 7s);
                break;
            default:
                break;
            }
        }

        void AddChampionBanner()
        {
            uint32 banner = 0;
            switch (me->GetEntry())
            {
            case NPC_MOKRA:
                banner = SPELL_BANNER_MOKRA;
                break;
            case NPC_ERESSEA:
                banner = SPELL_BANNER_ERESSEA;
                break;
            case NPC_RUNOK:
                banner = SPELL_BANNER_RUNOK;
                break;
            case NPC_ZULTORE:
                banner = SPELL_BANNER_ZULTORE;
                break;
            case NPC_VISCERI:
                banner = SPELL_BANNER_VISCERI;
                break;

            case NPC_JACOB:
                banner = SPELL_BANNER_JACOB;
                break;
            case NPC_AMBROSE:
                banner = SPELL_BANNER_AMBROSE;
                break;
            case NPC_COLOSOS:
                banner = SPELL_BANNER_COLOSOS;
                break;
            case NPC_JAELYNE:
                banner = SPELL_BANNER_JAELYNE;
                break;
            case NPC_LANA:
                banner = SPELL_BANNER_LANA;
                break;
            }

            if (banner && !me->HasAura(banner))
                me->AddAura(banner, me);
        }

        void DoAction(int32 param) override
        {
            if (param == 1)
            {
                MountPhase = false;
                NewMountGUID.Clear();
                me->SetHealth(me->GetMaxHealth());
                me->SetRegenerateHealth(true);
                me->RemoveUnitFlag(UNIT_FLAG_PACIFIED);
                me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                me->SetImmuneToAll(true);
                me->SetWalk(false);
                me->RemoveAllAuras();
                events.Reset();
            }
            else if (param == 2)
                ScheduleAbilitiesEvents();
            else if (param == 3)
                AddChampionBanner();
        }

        void SetData(uint32 uiType, uint32 uiData) override
        {
            AddChampionBanner();

            BossOrder = uiType;
            if (uiData > 1)
            {
                me->SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 0);
                return;
            }

            switch (BossOrder)
            {
            case 0:
                if (uiData == 0) // 1 == short version
                {
                    AddWaypoint(0, 747.36f, 634.07f, 411.572f, true);
                    AddWaypoint(1, 780.43f, 607.15f, 411.82f, true);
                }
                AddWaypoint(2, 785.99f, 599.41f, 411.92f, true);
                AddWaypoint(3, 778.44f, 601.64f, 411.79f, true);
                break;
            case 1:
                if (uiData == 0) // 1 == short version
                {
                    AddWaypoint(0, 747.35f, 634.07f, 411.57f, true);
                    AddWaypoint(1, 768.72f, 581.01f, 411.92f, true);
                }
                AddWaypoint(2, 763.55f, 590.52f, 411.71f, true);
                break;
            case 2:
                if (uiData == 0) // 1 == short version
                {
                    AddWaypoint(0, 747.35f, 634.07f, 411.57f, true);
                    AddWaypoint(1, 784.02f, 645.33f, 412.39f, true);
                }
                AddWaypoint(2, 775.67f, 641.91f, 411.91f, true);
                break;
            default:
                return;
            }

            Start(false);
        }

        void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
        {
            if (MountPhase)
            {
                if (me->GetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID) == 0)
                    damage = 0;
                else if (damage >= me->GetHealth())
                {
                    events.Reset();
                    damage = me->GetHealth() - 1;
                    me->SetReactState(REACT_PASSIVE);
                    me->RemoveAllAuras();
                    AddChampionBanner();
                    me->GetThreatManager().ClearAllThreat();
                    me->CombatStop(true);
                    me->GetMotionMaster()->Clear();
                    me->StopMoving();
                    me->SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 0);
                    me->SetRegenerateHealth(false);
                    me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                    me->SetImmuneToAll(true);
                    me->SetWalk(true);
                    if (pInstance)
                    {
                        pInstance->SetData(DATA_MOUNT_DIED, BossOrder);

                        std::list<Creature*> vehicles;
                        me->GetCreatureListWithEntryInGrid(vehicles, pInstance->GetData(DATA_TEAMID_IN_INSTANCE) == TEAM_HORDE ? VEHICLE_ARGENT_WARHORSE : VEHICLE_ARGENT_BATTLEWORG, 100.f);

                        std::erase_if(vehicles, [](auto& vehicle) { return vehicle->isDead(); });

                        if (uint32(vehicles.size()) <= 3 || vehicles.empty())
                            pInstance->SetData(DATA_RESPAWN_MOUNTS, 0);
                        else if (Creature* mount = vehicles.front())
                        {
                            NewMountGUID = mount->GetGUID();
                            me->GetMotionMaster()->MovePoint(7, *mount);
                        }

                        events.RescheduleEvent(EVENT_FIND_NEW_MOUNT, 1s);
                    }
                }
            }
            else
            {
                if (damage >= me->GetHealth())
                {
                    MountPhase = true;
                    events.Reset();
                    damage = me->GetHealth() - 1;
                    me->SetReactState(REACT_PASSIVE);
                    me->RemoveAllAuras();
                    AddChampionBanner();
                    me->GetThreatManager().ClearAllThreat();
                    me->CombatStop(true);
                    me->GetMotionMaster()->Clear();
                    me->SetRegenerateHealth(false);
                    me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                    me->SetImmuneToAll(true);
                    me->SetStandState(UNIT_STAND_STATE_KNEEL);
                    if (pInstance)
                        pInstance->SetData(DATA_GRAND_CHAMPION_DIED, BossOrder);
                }
            }
        }

        void EnterEvadeMode(EvadeReason /*why*/) override {}

        void WaypointReached(uint32 waypointId, uint32 /*pathId*/) override
        {
            if (!pInstance)
                return;

            if ((waypointId == 2 && (BossOrder == 1 || BossOrder == 2)) || (waypointId == 3 && BossOrder == 0))
                pInstance->SetData(DATA_GRAND_CHAMPION_REACHED_DEST, BossOrder);
        }

        void MovementInform(uint32 type, uint32 id) override
        {
            if (id < 4)
                EscortAI::MovementInform(type, id);

            if (type == POINT_MOTION_TYPE)
            {
                if (id == 5)
                    me->SetFacingTo(float(3 * M_PI / 2));
                else if (id == 7) // reached new mount!
                {
                    if (NewMountGUID != ObjectGuid::Empty)
                        if (Creature* mount = ObjectAccessor::GetCreature(*me, NewMountGUID))
                        {
                            mount->DespawnOrUnsummon();
                            me->SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, mount->GetDisplayId());
                            me->SetWalk(false);
                            me->SetFullHealth();
                            me->CastSpell(me, SPELL_BOSS_DEFEND_PERIODIC, true);
                            me->SetRegenerateHealth(true);
                            events.Reset();
                            events.ScheduleEvent(EVENT_MOUNT_CHARGE, 2500ms, 4s);
                            events.ScheduleEvent(EVENT_SHIELD_BREAKER, 5s, 8s);
                            events.ScheduleEvent(EVENT_THRUST, 3s, 5s);
                            me->SetReactState(REACT_AGGRESSIVE);
                            me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                            me->SetImmuneToAll(false);
                            if (Unit* target = me->SelectNearestTarget(200.0f))
                                AttackStart(target);
                            DoZoneInCombat();
                            me->CastSpell(me, SPELL_TRAMPLE_AURA, true);
                            if (pInstance)
                                pInstance->SetData(DATA_REACHED_NEW_MOUNT, 0);
                            NewMountGUID.Clear();
                            events.CancelEvent(EVENT_FIND_NEW_MOUNT);
                        }
                }
                else if (id == 9)
                    me->DespawnOrUnsummon();
                else if (id == 10)
                {
                    if (pInstance)
                        pInstance->SetData(DATA_GRAND_CHAMPION_REACHED_REAPPEAR_DEST, 0);

                    me->DespawnOrUnsummon();
                }
            }
        }

        void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
        {
            if (spellInfo->Id == SPELL_TRAMPLE_STUN)
                Talk(SAY_TRAMPLED, me);
        }

        void UpdateAI(uint32 diff) override
        {
            EscortAI::UpdateAI(diff);

            if (!UpdateVictim() && !NewMountGUID)
                return;

            events.Update(diff);

            if (me->HasUnitState(UNIT_STATE_CASTING) || ((me->GetEntry() == NPC_JACOB || me->GetEntry() == NPC_MOKRA) && me->HasAura(SPELL_BLADESTORM)))
                return;

            switch (events.ExecuteEvent())
            {
            case 0:
                break;
            case EVENT_FIND_NEW_MOUNT:
            {
                if (me->HasAura(SPELL_TRAMPLE_STUN))
                {
                    events.Repeat(200ms);
                    break;
                }

                // hackfix, trample won't hit grand champions because of UNIT_FLAG_NON_ATTACKABLE
                if (pInstance)
                {
                    bool trample = false;
                    Map::PlayerList const& pl = me->GetMap()->GetPlayers();
                    for (Map::PlayerList::const_iterator itr = pl.begin(); itr != pl.end(); ++itr)
                        if (Player* plr = itr->GetSource())
                            if (me->GetExactDist(plr) <= 5.0f)
                                if (Vehicle* v = plr->GetVehicle())
                                    if (Unit* c = v->GetBase())
                                        if (c->IsCreature() && c->ToCreature()->GetEntry() == uint32((pInstance->GetData(DATA_TEAMID_IN_INSTANCE) == TEAM_HORDE ? VEHICLE_ARGENT_BATTLEWORG : VEHICLE_ARGENT_WARHORSE)))
                                        {
                                            me->GetMotionMaster()->MoveIdle();
                                            me->StopMoving();
                                            me->CastSpell(me, SPELL_TRAMPLE_STUN, false);
                                            trample = true;
                                            break;
                                        }

                    if (trample)
                    {
                        events.Repeat(15s);
                        break;
                    }
                }

                if (Creature* mount = ObjectAccessor::GetCreature(*me, NewMountGUID))
                    if (mount->IsAlive())
                    {
                        if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() != POINT_MOTION_TYPE)
                            me->GetMotionMaster()->MovePoint(7, *mount);
                        events.Repeat(200ms);
                        break;
                    }

                if (pInstance)
                {
                    if (Creature* mount = me->FindNearestCreature(pInstance->GetData(DATA_TEAMID_IN_INSTANCE) == TEAM_HORDE ? VEHICLE_ARGENT_WARHORSE : VEHICLE_ARGENT_BATTLEWORG, 100.0f, true))
                    {
                        me->SetWalk(true);
                        NewMountGUID = mount->GetGUID();
                        me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
                        me->SetImmuneToAll(true);
                        me->GetMotionMaster()->MovePoint(7, *mount);
                        break;
                    }

                    events.Repeat(200ms);
                }
            }
            break;
            case EVENT_MOUNT_CHARGE:
            {
                GuidVector LIST;
                Map::PlayerList const& pl = me->GetMap()->GetPlayers();
                for (Map::PlayerList::const_iterator itr = pl.begin(); itr != pl.end(); ++itr)
                    if (Player* plr = itr->GetSource())
                    {
                        if (me->GetExactDist(plr) < 8.0f || me->GetExactDist(plr) > 25.0f || plr->isDead())
                            continue;
                        if (!plr->GetVehicle())
                            LIST.push_back(plr->GetGUID());
                        else if (Vehicle* v = plr->GetVehicle())
                        {
                            if (Unit* mount = v->GetBase())
                                LIST.push_back(mount->GetGUID());
                        }
                    }
                if (!LIST.empty())
                {
                    uint8 rnd = LIST.size() > 1 ? urand(0, LIST.size() - 1) : 0;
                    if (Unit* target = ObjectAccessor::GetUnit(*me, LIST.at(rnd)))
                    {
                        me->GetThreatManager().ResetAllThreat();
                        me->GetThreatManager().AddThreat(target, 10000.0f);
                        AttackStart(target);
                        me->CastSpell(target, SPELL_MINIONS_CHARGE, false);
                    }
                }
                events.Repeat(4500ms, 6s);
            }
            break;
            case EVENT_SHIELD_BREAKER:
            {
                GuidVector LIST;
                Map::PlayerList const& pl = me->GetMap()->GetPlayers();
                for (Map::PlayerList::const_iterator itr = pl.begin(); itr != pl.end(); ++itr)
                    if (Player* plr = itr->GetSource())
                    {
                        if (me->GetExactDist(plr) < 10.0f || me->GetExactDist(plr) > 30.0f)
                            continue;
                        if (Vehicle* v = plr->GetVehicle())
                            if (Unit* mount = v->GetBase())
                                LIST.push_back(mount->GetGUID());
                    }
                if (!LIST.empty())
                {
                    uint8 rnd = LIST.size() > 1 ? urand(0, LIST.size() - 1) : 0;
                    if (Unit* target = ObjectAccessor::GetCreature(*me, LIST.at(rnd)))
                        me->CastSpell(target, SPELL_NPC_SHIELD_BREAKER, false);
                }
                events.Repeat(6s, 8s);
            }
            break;
            case EVENT_THRUST:
                if (Unit* victim = me->GetVictim())
                    if (me->GetExactDist(victim) <= 6.0f)
                        me->CastSpell(victim, SPELL_PLAYER_VEHICLE_THRUST, false);
                events.Repeat(3s, 5s);
                break;

                /******************* MAGE *******************/
            case EVEMT_MAGE_SPELL_FIREBALL:
                if (me->GetVictim())
                    me->CastSpell(me->GetVictim(), SPELL_FIREBALL, false);
                events.Repeat(5s);
                break;
            case EVEMT_MAGE_SPELL_BLAST_WAVE:
                me->CastSpell((Unit*)nullptr, SPELL_BLAST_WAVE, false);
                events.Repeat(13s);
                break;
            case EVEMT_MAGE_SPELL_HASTE:
                me->CastSpell(me, SPELL_HASTE, false);
                events.Repeat(22s);
                break;
            case EVEMT_MAGE_SPELL_POLYMORPH:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 30.0f, true))
                    me->CastSpell(target, SPELL_POLYMORPH, false);
                events.Repeat(8s);
                break;
                /***************** MAGE END *****************/

                /****************** SHAMAN ******************/
            case EVENT_SHAMAN_SPELL_CHAIN_LIGHTNING:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 30.0f, true))
                    me->CastSpell(target, SPELL_CHAIN_LIGHTNING, false);
                events.Repeat(16s);
                break;
            case EVENT_SHAMAN_SPELL_EARTH_SHIELD:
                me->CastSpell(me, SPELL_EARTH_SHIELD, false);
                events.Repeat(30s, 35s);
                break;
            case EVENT_SHAMAN_SPELL_HEALING_WAVE:
            {
                Unit* target = nullptr;
                if (urand(0, 1))
                {
                    target = DoSelectLowestHpFriendly(40.0f);
                    if (!target)
                        target = me;
                }
                else
                    target = me;
                me->CastSpell(target, SPELL_HEALING_WAVE, false);
                events.Repeat(22s);
            }
            break;
            case EVENT_SHAMAN_SPELL_HEX_OF_MENDING:
                if (me->GetVictim())
                    me->CastSpell(me->GetVictim(), SPELL_HEX_OF_MENDING, false);
                events.Repeat(20s, 25s);
                break;
                /**************** SHAMAN END ****************/

                /****************** HUNTER ******************/
            case EVENT_HUNTER_SPELL_DISENGAGE:

                break;
            case EVENT_HUNTER_SPELL_LIGHTNING_ARROWS:
                me->CastSpell((Unit*)nullptr, SPELL_LIGHTNING_ARROWS, false);
                events.Repeat(20s, 25s);
                break;
            case EVENT_HUNTER_SPELL_MULTI_SHOT:
            {
                if (!UnitTargetGUID)
                {
                    if (Unit* target = SelectTarget(SelectTargetMethod::MinDistance, 0, 30.0f, true))
                    {
                        me->CastSpell(target, SPELL_SHOOT, false);
                        UnitTargetGUID = target->GetGUID();
                    }
                    events.Repeat(2s);
                    break;
                }
                else
                {
                    Unit* target = ObjectAccessor::GetUnit(*me, UnitTargetGUID);
                    if (target && me->IsInRange(target, 5.0f, 30.0f, false))
                        me->CastSpell(target, SPELL_MULTI_SHOT, false);
                    else
                    {
                        Map::PlayerList const& pl = me->GetMap()->GetPlayers();
                        for (Map::PlayerList::const_iterator itr = pl.begin(); itr != pl.end(); ++itr)
                        {
                            Player* player = itr->GetSource();
                            if (player && me->IsInRange(player, 5.0f, 30.0f, false))
                            {
                                me->CastSpell(player, SPELL_MULTI_SHOT, false);
                                break;
                            }
                        }
                    }
                    UnitTargetGUID.Clear();
                }
                events.Repeat(15s, 20s);
            }
            break;
            /**************** HUNTER END ****************/

            /****************** ROGUE *******************/
            case EVENT_ROGUE_SPELL_EVISCERATE:
                if (me->GetVictim())
                    me->CastSpell(me->GetVictim(), SPELL_EVISCERATE, false);
                events.Repeat(8s);
                break;
            case EVENT_ROGUE_SPELL_FAN_OF_KNIVES:
                me->CastSpell((Unit*)nullptr, SPELL_FAN_OF_KNIVES, false);
                events.Repeat(14s);
                break;
            case EVENT_ROGUE_SPELL_POISON_BOTTLE:
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 30.0f, true))
                    me->CastSpell(target, SPELL_POISON_BOTTLE, false);
                events.Repeat(19s);
                break;
                /**************** ROGUE END *****************/

                /***************** WARRIOR ******************/
            case EVENT_WARRIOR_SPELL_MORTAL_STRIKE:
                if (me->GetVictim())
                    me->CastSpell(me->GetVictim(), SPELL_MORTAL_STRIKE, false);
                events.Repeat(8s, 12s);
                break;
            case EVENT_WARRIOR_SPELL_BLADESTORM:
                if (me->GetVictim())
                    me->CastSpell(me->GetVictim(), SPELL_BLADESTORM, false);
                events.Repeat(15s, 20s);
                break;
            case EVENT_WARRIOR_SPELL_INTERCEPT:
            {
                Map::PlayerList const& pl = me->GetMap()->GetPlayers();
                for (Map::PlayerList::const_iterator itr = pl.begin(); itr != pl.end(); ++itr)
                {
                    Player* player = itr->GetSource();
                    if (player && me->IsInRange(player, 8.0f, 25.0f, false))
                    {
                        me->GetThreatManager().ResetAllThreat();
                        me->GetThreatManager().AddThreat(player, 5.0f);
                        me->CastSpell(player, SPELL_INTERCEPT, false);
                        break;
                    }
                }
                events.Repeat(7s);
            }
            break;
            case EVENT_WARRIOR_SPELL_ROLLING_THROW:

                break;
                /*************** WARRIOR END ****************/
            }

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* pCreature) const override
    {
        return GetTrialOfTheChampionAI<boss_grand_championAI>(pCreature);
    }
};

class spell_grand_champion_hex_of_mending_unitscript : public UnitScript
{
public:
    spell_grand_champion_hex_of_mending_unitscript() : UnitScript("spell_grand_champion_hex_of_mending_unitscript") { }

    void OnBeforeHeal(Unit* /*healer*/, Unit* receiver, uint32& gain) override
    {
        if (receiver->HasAura(SPELL_HEX_OF_MENDING))
            if (InstanceScript* script = receiver->GetInstanceScript())
                if (script->instance->IsHeroic())
                    gain = 0;
    }
};

//67534
class spell_grand_champion_hex_of_mending : public AuraScript
{
    PrepareAuraScript(spell_grand_champion_hex_of_mending);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_HEX_OF_MENDING_HEAL });
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (!eventInfo.GetHealInfo())
            return false;
        
        if (!eventInfo.GetHealInfo()->GetHeal() && !eventInfo.GetHealInfo()->GetAbsorb())
            return false;

        return true;
    }

    void HandleProc(AuraEffect const* aurEff, ProcEventInfo& eventInfo)
    {
        PreventDefaultAction();

        HealInfo* healInfo = eventInfo.GetHealInfo();
        if (!healInfo)
            return;

        uint32 healAmount = healInfo->GetHeal() + healInfo->GetAbsorb();
        if (!healAmount)
            return;

        Unit* healer = healInfo->GetHealer();
        if (!healer)
            return;

        Unit* target = healInfo->GetTarget();
        if (!target)
            return;

        CastSpellExtraArgs args(aurEff);
        args.AddSpellBP0(healAmount);
        args.SetOriginalCaster(healer->GetGUID());
        target->CastSpell(nullptr, SPELL_HEX_OF_MENDING_HEAL, args);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_grand_champion_hex_of_mending::CheckProc);
        OnEffectProc += AuraEffectProcFn(spell_grand_champion_hex_of_mending::HandleProc, EFFECT_0, SPELL_AURA_DUMMY);
    }
};


void AddSC_boss_grand_champions()
{
    new boss_grand_champion();
    new npc_toc5_grand_champion_minion();
    new npc_toc5_player_vehicle();
    new spell_grand_champion_hex_of_mending_unitscript();
    RegisterSpellScript(spell_grand_champion_hex_of_mending);
}

