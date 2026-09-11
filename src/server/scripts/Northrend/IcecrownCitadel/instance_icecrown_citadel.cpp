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

#include "icecrown_citadel.h"
#include "AreaBoundary.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "EventMap.h"
#include "InstanceScript.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestPools.h"
#include "ScriptMgr.h"
#include "TemporarySummon.h"
#include "Transport.h"
#include "TransportMgr.h"
#include "WorldStatePackets.h"
#include <unordered_set>

enum PutricideValveState : uint8
{
    PUTRICIDE_VALVE_FESTERGUT = 1,
    PUTRICIDE_VALVE_ROTFACE   = 2
};

enum EventIds
{
    EVENT_PLAYERS_GUNSHIP_SPAWN     = 22663,
    EVENT_PLAYERS_GUNSHIP_COMBAT    = 22664,
    EVENT_PLAYERS_GUNSHIP_SAURFANG  = 22665,
    EVENT_ENEMY_GUNSHIP_COMBAT      = 22860,
    EVENT_ENEMY_GUNSHIP_DESPAWN     = 22861,
    EVENT_QUAKE                     = 23437,
    EVENT_FESTERGUT_VALVE_USED      = 23438,
    EVENT_ROTFACE_VALVE_USED        = 23426,
    EVENT_SECOND_REMORSELESS_WINTER = 23507,
    EVENT_TELEPORT_TO_FROSTMOURNE   = 23617
};

enum TimedEvents
{
    EVENT_UPDATE_EXECUTION_TIME = 1,
    EVENT_QUAKE_SHATTER         = 2,
    EVENT_REBUILD_PLATFORM      = 3,
    EVENT_RESPAWN_GUNSHIP       = 4,
    EVENT_SPAWN_SAURFANG_EVENT  = 6,
    EVENT_SAURFANG_ZEPPELIN_DOCK = 7,
    EVENT_SAURFANG_OUTRO_TIMEOUT = 8,
    EVENT_SAURFANG_ZEPPELIN_REMOVE = 9,
    // The camp is built on screen, stage by stage.
    EVENT_SAURFANG_CAMP_TELEPORTERS = 10,
    EVENT_SAURFANG_CAMP_WORKERS     = 11,
    EVENT_SAURFANG_CAMP_TENTS       = 12,
    EVENT_SAURFANG_CAMP_WORKERS_OUT = 13,
    EVENT_SAURFANG_CAMP_VENDORS     = 14,
    EVENT_SAURFANG_CAMP_SMITH_ARRIVE = 15,
    EVENT_SAURFANG_CAMP_WORKERS_FIRST_POS = 16,
    EVENT_SAURFANG_CAMP_WORKERS_RE_POS = 17,
    EVENT_SAURFANG_CAMP_WORKERS_BACK = 18,
    EVENT_SAURFANG_CAMP_VENDOR_ARRIVE = 19,
    EVENT_SAURFANG_NPCS_RESET = 20,
    EVENT_SAURFANG_CAMP_RESET = 21
};

enum SpawnGroups
{
    SPAWN_GROUP_ALLIANCE_ROS   = 57,
    SPAWN_GROUP_HORDE_ROS      = 58
};

BossBoundaryData const boundaries =
{
    { DATA_LORD_MARROWGAR,        new CircleBoundary(Position(-428.0f,2211.0f), 95.0) },
    { DATA_LORD_MARROWGAR,        new RectangleBoundary(-430.0f, -330.0f, 2110.0f, 2310.0f) },
    { DATA_LADY_DEATHWHISPER,     new RectangleBoundary(-670.0f, -520.0f, 2145.0f, 2280.0f) },
    { DATA_DEATHBRINGER_SAURFANG, new RectangleBoundary(-565.0f, -465.0f, 2160.0f, 2260.0f) },
    { DATA_ROTFACE,               new RectangleBoundary(4385.0f, 4505.0f, 3082.0f, 3195.0f) },
    { DATA_FESTERGUT,             new RectangleBoundary(4205.0f, 4325.0f, 3082.0f, 3195.0f) },
    { DATA_PROFESSOR_PUTRICIDE,   new ParallelogramBoundary(Position(4356.0f, 3290.0f), Position(4435.0f, 3194.0f), Position(4280.0f, 3194.0f)) },
    { DATA_PROFESSOR_PUTRICIDE,   new RectangleBoundary(4280.0f, 4435.0f, 3150.0f, 4360.0f) },
    { DATA_BLOOD_PRINCE_COUNCIL,  new EllipseBoundary(Position(4660.95f, 2769.194f), 85.0, 60.0) },
    { DATA_BLOOD_QUEEN_LANA_THEL, new CircleBoundary(Position(4595.93f, 2769.365f), 64.0) },
    { DATA_BLOOD_QUEEN_LANA_THEL, new ZRangeBoundary(391.78f, 473.43f) },
    { DATA_SISTER_SVALNA,         new RectangleBoundary(4291.0f, 4423.0f, 2438.0f, 2653.0f) },
    { DATA_VALITHRIA_DREAMWALKER, new RectangleBoundary(4112.5f, 4293.5f, 2385.0f, 2585.0f) },
    { DATA_SINDRAGOSA,            new EllipseBoundary(Position(4408.6f, 2484.0f), 100.0, 75.0) }
};

DoorData const doorData[] =
{
    { GO_LORD_MARROWGAR_S_ENTRANCE,           DATA_LORD_MARROWGAR,        DOOR_TYPE_ROOM },
    { GO_ICEWALL,                             DATA_LORD_MARROWGAR,        DOOR_TYPE_PASSAGE },
    { GO_DOODAD_ICECROWN_ICEWALL02,           DATA_LORD_MARROWGAR,        DOOR_TYPE_PASSAGE },
    { GO_ORATORY_OF_THE_DAMNED_ENTRANCE,      DATA_LADY_DEATHWHISPER,     DOOR_TYPE_ROOM  },
    { GO_SAURFANG_S_DOOR,                     DATA_DEATHBRINGER_SAURFANG, DOOR_TYPE_PASSAGE },
    { GO_ORANGE_PLAGUE_MONSTER_ENTRANCE,      DATA_FESTERGUT,             DOOR_TYPE_ROOM },
    { GO_GREEN_PLAGUE_MONSTER_ENTRANCE,       DATA_ROTFACE,               DOOR_TYPE_ROOM },
    { GO_SCIENTIST_ENTRANCE,                  DATA_PROFESSOR_PUTRICIDE,   DOOR_TYPE_ROOM },
    { GO_CRIMSON_HALL_DOOR,                   DATA_BLOOD_PRINCE_COUNCIL,  DOOR_TYPE_ROOM },
    { GO_BLOOD_ELF_COUNCIL_DOOR,              DATA_BLOOD_PRINCE_COUNCIL,  DOOR_TYPE_PASSAGE },
    { GO_BLOOD_ELF_COUNCIL_DOOR_RIGHT,        DATA_BLOOD_PRINCE_COUNCIL,  DOOR_TYPE_PASSAGE },
    { GO_DOODAD_ICECROWN_BLOODPRINCE_DOOR_01, DATA_BLOOD_QUEEN_LANA_THEL, DOOR_TYPE_ROOM },
    { GO_DOODAD_ICECROWN_GRATE_01,            DATA_BLOOD_QUEEN_LANA_THEL, DOOR_TYPE_PASSAGE },
    { GO_GREEN_DRAGON_BOSS_ENTRANCE,          DATA_SISTER_SVALNA,         DOOR_TYPE_PASSAGE },
    { GO_GREEN_DRAGON_BOSS_ENTRANCE,          DATA_VALITHRIA_DREAMWALKER, DOOR_TYPE_ROOM },
    { GO_GREEN_DRAGON_BOSS_EXIT,              DATA_VALITHRIA_DREAMWALKER, DOOR_TYPE_PASSAGE },
    { GO_DOODAD_ICECROWN_ROOSTPORTCULLIS_01,  DATA_VALITHRIA_DREAMWALKER, DOOR_TYPE_SPAWN_HOLE },
    { GO_DOODAD_ICECROWN_ROOSTPORTCULLIS_02,  DATA_VALITHRIA_DREAMWALKER, DOOR_TYPE_SPAWN_HOLE },
    { GO_DOODAD_ICECROWN_ROOSTPORTCULLIS_03,  DATA_VALITHRIA_DREAMWALKER, DOOR_TYPE_SPAWN_HOLE },
    { GO_DOODAD_ICECROWN_ROOSTPORTCULLIS_04,  DATA_VALITHRIA_DREAMWALKER, DOOR_TYPE_SPAWN_HOLE },
    { GO_SINDRAGOSA_SHORTCUT_ENTRANCE_DOOR,   DATA_SINDRAGOSA,            DOOR_TYPE_PASSAGE },
    { GO_SINDRAGOSA_SHORTCUT_EXIT_DOOR,       DATA_SINDRAGOSA,            DOOR_TYPE_PASSAGE },
    { GO_ICE_WALL,                            DATA_SINDRAGOSA,            DOOR_TYPE_ROOM },
    { GO_ICE_WALL,                            DATA_SINDRAGOSA,            DOOR_TYPE_ROOM },
    { 0,                                      0,                          DOOR_TYPE_ROOM }  // END
};

// this doesnt have to only store questgivers, also can be used for related quest spawns
struct WeeklyQuest
{
    uint32 creatureEntry;
    uint32 questId[2];  // 10 and 25 man versions
};

// when changing the content, remember to update SetData, DATA_BLOOD_QUICKENING_STATE case for NPC_ALRIN_THE_AGILE index
WeeklyQuest const WeeklyQuestData[WeeklyNPCs] =
{
    { NPC_INFILTRATOR_MINCHAR,         { QUEST_DEPROGRAMMING_10,                 QUEST_DEPROGRAMMING_25                 } }, // Deprogramming
    { NPC_KOR_KRON_LIEUTENANT,         { QUEST_SECURING_THE_RAMPARTS_10,         QUEST_SECURING_THE_RAMPARTS_25         } }, // Securing the Ramparts
    { NPC_ROTTING_FROST_GIANT_10,      { QUEST_SECURING_THE_RAMPARTS_10,         QUEST_SECURING_THE_RAMPARTS_25         } }, // Securing the Ramparts
    { NPC_ROTTING_FROST_GIANT_25,      { QUEST_SECURING_THE_RAMPARTS_10,         QUEST_SECURING_THE_RAMPARTS_25         } }, // Securing the Ramparts
    { NPC_ALCHEMIST_ADRIANNA,          { QUEST_RESIDUE_RENDEZVOUS_10,            QUEST_RESIDUE_RENDEZVOUS_25            } }, // Residue Rendezvous
    { NPC_ALRIN_THE_AGILE,             { QUEST_BLOOD_QUICKENING_10,              QUEST_BLOOD_QUICKENING_25              } }, // Blood Quickening
    { NPC_INFILTRATOR_MINCHAR_BQ,      { QUEST_BLOOD_QUICKENING_10,              QUEST_BLOOD_QUICKENING_25              } }, // Blood Quickening
    { NPC_MINCHAR_BEAM_STALKER,        { QUEST_BLOOD_QUICKENING_10,              QUEST_BLOOD_QUICKENING_25              } }, // Blood Quickening
    { NPC_VALITHRIA_DREAMWALKER_QUEST, { QUEST_RESPITE_FOR_A_TORNMENTED_SOUL_10, QUEST_RESPITE_FOR_A_TORNMENTED_SOUL_25 } }  // Respite for a Tormented Soul
};

// NPCs spawned at Light's Hammer on Lich King dead
Position const JainaSpawnPos    = { -48.65278f, 2211.026f, 27.98586f, 3.124139f };
Position const MuradinSpawnPos  = { -47.34549f, 2208.087f, 27.98586f, 3.106686f };
Position const UtherSpawnPos    = { -26.58507f, 2211.524f, 30.19898f, 3.124139f };
Position const SylvanasSpawnPos = { -41.45833f, 2222.891f, 27.98586f, 3.647738f };

// The camps differ: Horde tents from the retail sniff, Alliance ones measured in game and much
// closer together, with their own forge, anvil and banner by the second tent, and no bonfire.
Position const SaurfangCampTentPosH[2] =
{
    { -532.86456f, 2229.0088f, 539.2921f, 2.530723f },
    { -524.55730f, 2238.0920f, 539.2920f, 0.13962449f }
};

Position const SaurfangCampTentPosA[2] =
{
    { -531.84283f, 2230.6853f, 539.2918f, 5.625422f },
    { -528.76000f, 2234.7815f, 539.2918f, 5.609714f }
};

Position const SaurfangWorkerFirstPos = { -544.7736f, 2220.677f, 539.29114f, 0.156871f };

Position const SaurfangCampBannerPosA   = { -533.05540f, 2234.6326f, 539.2918f, 5.621495f };
Position const SaurfangCampBlacksmithPosA   = { -526.41502f, 2232.9104f, 539.2918f, 5.609714f };
Position const SaurfangCampGeneralGoodsPosA = { -529.46875f, 2228.8513f, 539.2918f, 5.625422f };
Position const SaurfangCampAnvilPosA    = { -525.71640f, 2236.1909f, 539.2918f, 5.609714f };
Position const SaurfangCampForgePosA    = { -528.06140f, 2238.0620f, 539.2918f, 5.609714f };
// Detour: both Alliance vendors round the south side, on separate corners.
Position const SaurfangCampSmithDetourPosA  = { -525.50000f, 2227.5000f, 539.2918f, 0.0f };
Position const SaurfangCampGoodsDetourPosA  = { -528.50000f, 2225.0000f, 539.2918f, 0.0f };

Position const SaurfangCampTeleporterPos[2] =
{
    { -560.41840f, 2202.7500f, 539.28534f, 0.0f },
    { -560.29517f, 2220.2153f, 539.28540f, 0.0f }
};

Position const SaurfangCampBlacksmithPos    = { -520.94100f, 2233.1077f, 539.3463f, 5.3756142f };
Position const SaurfangCampGeneralGoodsPos  = { -530.3813f, 2227.3657f, 539.2917f, 5.4628806f };
// Travel time for the ~41y between a teleporter pad and its tent site.
Seconds const SaurfangCampWorkerTravel = 7s;
// The Horde smith would otherwise walk straight through the bonfire.
Position const SaurfangCampSmithDetourPos   = { -529.00000f, 2233.0000f, 539.2920f, 0.0f };
Position const SaurfangOutroPortalPos       = { -523.55963f, 2238.8900f, 539.29070f, 6.1102815f };
// Where the zeppelin comes to rest; it is frozen on arrival.
Position const SaurfangOutroZeppelinPos     = { -527.66110f, 2254.6910f, 538.53300f, 0.6848107f };
float const SaurfangOutroZeppelinDockRange  = 12.0f;

class instance_icecrown_citadel : public InstanceMapScript
{
    public:
        instance_icecrown_citadel() : InstanceMapScript(ICCScriptName, 631) { }

        struct instance_icecrown_citadel_InstanceMapScript : public InstanceScript
        {
            instance_icecrown_citadel_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
            {
                SetHeaders(DataHeader);
                SetBossNumber(EncounterCount);
                LoadBossBoundaries(boundaries);
                LoadDoorData(doorData);
                TeamInInstance = map->GetTeamInInstance();
                HeroicAttempts = MaxHeroicAttempts;
                ColdflameJetsState = NOT_STARTED;
                UpperSpireTeleporterActiveState = NOT_STARTED;
                BloodQuickeningState = NOT_STARTED;
                BloodQuickeningMinutes = 0;
                BloodPrinceIntro = 1;
                SindragosaIntro = 1;
                SindragosaGauntletState = NOT_STARTED;
                _putricideTrapState = NOT_STARTED;
                _putricideValveState = 0;
                IsBonedEligible = true;
                IsOozeDanceEligible = true;
                IsNauseaEligible = true;
                IsOrbWhispererEligible = true;
                IsFactionBuffActive = true;
                _saurfangCampSpawned = false;
                _saurfangOutroRunning = false;
                _saurfangZeppelinDocked = false;
                _saurfangZeppelinLeaving = false;
            }

            // A function to help reduce the number of lines for teleporter management.
            void SetTeleporterState(GameObject* go, bool usable)
            {
                if (usable)
                {
                    go->RemoveFlag(GO_FLAG_NOT_SELECTABLE);
                    go->SetGoState(GO_STATE_ACTIVE);
                }
                else
                {
                    go->SetFlag(GO_FLAG_NOT_SELECTABLE);
                    go->SetGoState(GO_STATE_READY);
                }
            }

            void FillInitialWorldStates(WorldPackets::WorldState::InitWorldStates& packet) override
            {
                packet.Worldstates.emplace_back(WORLDSTATE_SHOW_TIMER, BloodQuickeningState == IN_PROGRESS ? 1 : 0);
                packet.Worldstates.emplace_back(WORLDSTATE_EXECUTION_TIME, BloodQuickeningMinutes);
                packet.Worldstates.emplace_back(WORLDSTATE_SHOW_ATTEMPTS, instance->IsHeroic() ? 1 : 0);
                packet.Worldstates.emplace_back(WORLDSTATE_ATTEMPTS_REMAINING, HeroicAttempts);
                packet.Worldstates.emplace_back(WORLDSTATE_ATTEMPTS_MAX, MaxHeroicAttempts);
            }

            void OnPlayerEnter(Player* player) override
            {
                if (SindragosaGauntletState == DONE)
                    if (GameObject* go = instance->GetGameObject(SindragosaEntranceDoorGUID))
                        go->SetGoState(GO_STATE_ACTIVE);

                uint8 spawnGroupId = TeamInInstance == ALLIANCE ? SPAWN_GROUP_ALLIANCE_ROS : SPAWN_GROUP_HORDE_ROS;
                if (!instance->IsSpawnGroupActive(spawnGroupId))
                    instance->SpawnGroupSpawn(spawnGroupId);

                if (GetBossState(DATA_LADY_DEATHWHISPER) == DONE && GetBossState(DATA_ICECROWN_GUNSHIP_BATTLE) != DONE)
                {
                    if (!GunshipGUID.IsEmpty())
                    {
                        if (GameObject* gunship = instance->GetGameObject(GunshipGUID))
                            gunship->AddObjectToRemoveList();

                        GunshipGUID.Clear();
                    }

                    ObjectGuid enemyGunshipGUID = GetGuidData(DATA_ENEMY_GUNSHIP);

                    if (!enemyGunshipGUID.IsEmpty())
                    {
                        if (GameObject* enemyGunship = instance->GetGameObject(enemyGunshipGUID))
                            enemyGunship->AddObjectToRemoveList();
                    }

                    SpawnGunship();
                }

                // Also covers an instance whose Gunship Battle state was set directly, which would
                // otherwise keep them hidden for good.
                if (GetBossState(DATA_ICECROWN_GUNSHIP_BATTLE) == DONE && !_saurfangOutroRunning)
                    Events.ScheduleEvent(EVENT_SAURFANG_NPCS_RESET, 3s);

                if (GetBossState(DATA_DEATHBRINGER_SAURFANG) == DONE && !_saurfangOutroRunning)
                    Events.ScheduleEvent(EVENT_SAURFANG_CAMP_RESET, 3s);

                if (IsFactionBuffActive)
                    DoCastSpellOnPlayer(player, TeamInInstance == ALLIANCE ? SPELL_STRENGHT_OF_WRYNN : SPELL_HELLSCREAMS_WARSONG);
            }

            void OnPlayerLeave(Player* player) override
            {
                DoRemoveAurasDueToSpellOnPlayer(player, TeamInInstance == ALLIANCE ? SPELL_STRENGHT_OF_WRYNN : SPELL_HELLSCREAMS_WARSONG, true, true);
            }

            void OnCreatureCreate(Creature* creature) override
            {
                if (creature->IsGuardian() && creature->GetOwnerGUID().IsPlayer())
                {
                    if (IsFactionBuffActive)
                        creature->CastSpell(creature, TeamInInstance == ALLIANCE ? SPELL_STRENGHT_OF_WRYNN : SPELL_HELLSCREAMS_WARSONG, true);
                }

                switch (creature->GetEntry())
                {
                    case NPC_NERUBAR_BROODKEEPER:
                    {
                        uint8 group = (creature->GetPositionX() > -230.0f) ? 0 : 1;
                        nerubarBroodkeepersGUIDs[group].emplace_back(creature->GetGUID());
                        break;
                    }
                    case NPC_LORD_MARROWGAR:
                        LordMarrowgarGUID = creature->GetGUID();
                        break;
                    case NPC_LADY_DEATHWHISPER:
                        LadyDeahtwhisperGUID = creature->GetGUID();
                        break;
                    case NPC_DEATHBRINGER_SAURFANG:
                        DeathbringerSaurfangGUID = creature->GetGUID();
                        break;
                    case NPC_ALLIANCE_GUNSHIP_CANNON:
                    case NPC_HORDE_GUNSHIP_CANNON:
                        creature->SetControlled(true, UNIT_STATE_ROOT);
                        break;
                    case NPC_SE_HIGH_OVERLORD_SAURFANG:
                    case NPC_SE_MURADIN_BRONZEBEARD:
                        // Only the static spawn is the event NPC - the Alliance outro summons another
                        // High Overlord Saurfang, which would otherwise clobber the guid.
                        if (creature->IsSummon())
                            break;

                        DeathbringerSaurfangEventGUID = creature->GetGUID();
                        creature->LastUsedScriptID = creature->GetScriptId();
                        HideSaurfangEventNpc(creature);
                        break;
                    case NPC_SE_KOR_KRON_REAVER:
                    case NPC_SE_SKYBREAKER_MARINE:
                        if (creature->IsSummon())
                            break;

                        SaurfangEventGuardGUIDs.push_back(creature->GetGUID());
                        HideSaurfangEventNpc(creature);
                        break;
                    case NPC_FESTERGUT:
                        FestergutGUID = creature->GetGUID();
                        break;
                    case NPC_ROTFACE:
                        RotfaceGUID = creature->GetGUID();
                        break;
                    case NPC_PROFESSOR_PUTRICIDE:
                        ProfessorPutricideGUID = creature->GetGUID();
                        break;
                    case NPC_PUTRICADES_TRAP:
                        PutricadesTrapGUID = creature->GetGUID();
                        break;
                    case NPC_VOLATILE_OOZE:
                    case NPC_GAS_CLOUD:
                        //! These creatures are summoned by something else than Professor Putricide
                        //! but need to be controlled/despawned by him - so they need to be
                        //! registered on his summon list
                        if (Creature* professorPutricide = instance->GetCreature(ProfessorPutricideGUID))
                            professorPutricide->AI()->JustSummoned(creature);
                        break;
                    case NPC_PRINCE_KELESETH:
                        BloodCouncilGUIDs[0] = creature->GetGUID();
                        break;
                    case NPC_PRINCE_TALDARAM:
                        BloodCouncilGUIDs[1] = creature->GetGUID();
                        break;
                    case NPC_PRINCE_VALANAR:
                        BloodCouncilGUIDs[2] = creature->GetGUID();
                        break;
                    case NPC_BLOOD_ORB_CONTROLLER:
                        BloodCouncilControllerGUID = creature->GetGUID();
                        break;
                    case NPC_BLOOD_QUEEN_LANA_THEL_COUNCIL:
                        BloodQueenLanaThelCouncilGUID = creature->GetGUID();
                        break;
                    case NPC_BLOOD_QUEEN_LANA_THEL:
                        BloodQueenLanaThelGUID = creature->GetGUID();
                        break;
                    case NPC_INFILTRATOR_MINCHAR_BQ:
                         // keep him in air
                         creature->SetEmoteState(EMOTE_ONESHOT_NONE);
                         creature->SetDisableGravity(true);
                         break;
                    case NPC_CROK_SCOURGEBANE:
                        CrokScourgebaneGUID = creature->GetGUID();
                        break;
                    // we can only do this because there are no gaps in their entries
                    case NPC_CAPTAIN_ARNATH:
                    case NPC_CAPTAIN_BRANDON:
                    case NPC_CAPTAIN_GRONDEL:
                    case NPC_CAPTAIN_RUPERT:
                        CrokCaptainGUIDs[creature->GetEntry()-NPC_CAPTAIN_ARNATH] = creature->GetGUID();
                        break;
                    case NPC_SISTER_SVALNA:
                        SisterSvalnaGUID = creature->GetGUID();
                        break;
                    case NPC_VALITHRIA_DREAMWALKER:
                        ValithriaDreamwalkerGUID = creature->GetGUID();
                        break;
                    case NPC_THE_LICH_KING_VALITHRIA:
                        ValithriaLichKingGUID = creature->GetGUID();
                        break;
                    case NPC_GREEN_DRAGON_COMBAT_TRIGGER:
                        ValithriaTriggerGUID = creature->GetGUID();
                        break;
                    case NPC_SINDRAGOSA_GAUNTLET:
                        SindragosaGauntletGUID = creature->GetGUID();
			break;
                    case NPC_SINDRAGOSA:
                        SindragosaGUID = creature->GetGUID();
                        break;
                    case NPC_SPINESTALKER:
                        SpinestalkerGUID = creature->GetGUID();
                        break;
                    case NPC_RIMEFANG:
                        RimefangGUID = creature->GetGUID();
                        break;
                    case NPC_INVISIBLE_STALKER:
                        // Teleporter visual at center
                        if (creature->GetExactDist2d(4357.052f, 2769.421f) < 10.0f)
                            creature->CastSpell(creature, SPELL_ARTHAS_TELEPORTER_CEREMONY, false);
                        break;
                    case NPC_THE_LICH_KING:
                        TheLichKingGUID = creature->GetGUID();
                        break;
                    case NPC_HIGHLORD_TIRION_FORDRING_LK:
                        HighlordTirionFordringGUID = creature->GetGUID();
                        break;
                    case NPC_TERENAS_MENETHIL_FROSTMOURNE:
                    case NPC_TERENAS_MENETHIL_FROSTMOURNE_H:
                        TerenasMenethilGUID = creature->GetGUID();
                        break;
                    case NPC_WICKED_SPIRIT:
                        // Remove corpse as soon as it dies (and respawn 10 seconds later)
                        creature->SetCorpseDelay(0);
                        creature->SetReactState(REACT_PASSIVE);
                        break;
                    default:
                        break;
                }
            }

            void OnCreatureRemove(Creature* creature) override
            {
                if (creature->GetEntry() == NPC_SINDRAGOSA)
                    SindragosaGUID.Clear();
            }

            // Weekly quest spawn prevention
            uint32 GetCreatureEntry(ObjectGuid::LowType /*guidLow*/, CreatureData const* data) override
            {
                uint32 entry = data->id;
                switch (entry)
                {
                    case NPC_INFILTRATOR_MINCHAR:
                    case NPC_KOR_KRON_LIEUTENANT:
                    case NPC_ALCHEMIST_ADRIANNA:
                    case NPC_ALRIN_THE_AGILE:
                    case NPC_INFILTRATOR_MINCHAR_BQ:
                    case NPC_MINCHAR_BEAM_STALKER:
                    case NPC_VALITHRIA_DREAMWALKER_QUEST:
                    {
                        for (uint8 questIndex = 0; questIndex < WeeklyNPCs; ++questIndex)
                        {
                            if (WeeklyQuestData[questIndex].creatureEntry == entry)
                            {
                                uint8 diffIndex = uint8(instance->GetSpawnMode() & 1);
                                if (!sQuestPoolMgr->IsQuestActive(WeeklyQuestData[questIndex].questId[diffIndex]))
                                    return 0;
                                break;
                            }
                        }

                        if (entry == NPC_KOR_KRON_LIEUTENANT && TeamInInstance == ALLIANCE)
                            return NPC_SKYBREAKER_LIEUTENANT;
                        break;
                    }
                    case NPC_HORDE_GUNSHIP_CANNON:
                    case NPC_ORGRIMS_HAMMER_CREW:
                    case NPC_SKY_REAVER_KORM_BLACKSCAR:
                        if (TeamInInstance == ALLIANCE)
                            return 0;
                        break;
                    case NPC_ALLIANCE_GUNSHIP_CANNON:
                    case NPC_SKYBREAKER_DECKHAND:
                    case NPC_HIGH_CAPTAIN_JUSTIN_BARTLETT:
                        if (TeamInInstance == HORDE)
                            return 0;
                        break;
                    case NPC_ZAFOD_BOOMBOX:
                        if (GameObjectTemplate const* go = sObjectMgr->GetGameObjectTemplate(GO_THE_SKYBREAKER_A))
                            if ((TeamInInstance == ALLIANCE && int32(data->mapId) == go->moTransport.mapID) ||
                                (TeamInInstance == HORDE && int32(data->mapId) != go->moTransport.mapID))
                                return entry;
                        return 0;
                    case NPC_IGB_MURADIN_BRONZEBEARD:
                        if ((TeamInInstance == ALLIANCE && data->spawnPoint.GetPositionX() > 10.0f) ||
                            (TeamInInstance == HORDE && data->spawnPoint.GetPositionX() < 10.0f))
                            return entry;
                        return 0;
                    case NPC_SE_HIGH_OVERLORD_SAURFANG:
                        return TeamInInstance == ALLIANCE ? NPC_SE_MURADIN_BRONZEBEARD : NPC_SE_HIGH_OVERLORD_SAURFANG;
                    case NPC_KOR_KRON_GENERAL:
                        return TeamInInstance == ALLIANCE ? NPC_ALLIANCE_COMMANDER : NPC_KOR_KRON_GENERAL;
                    case NPC_TORTUNOK:
                        return TeamInInstance == ALLIANCE ? NPC_ALANA_MOONSTRIKE : NPC_TORTUNOK;
                    case NPC_GERARDO_THE_SUAVE:
                        return TeamInInstance == ALLIANCE ? NPC_TALAN_MOONSTRIKE : NPC_GERARDO_THE_SUAVE;
                    case NPC_UVLUS_BANEFIRE:
                        return TeamInInstance == ALLIANCE ? NPC_MALFUS_GRIMFROST : NPC_UVLUS_BANEFIRE;
                    case NPC_IKFIRUS_THE_VILE:
                        return TeamInInstance == ALLIANCE ? NPC_YILI : NPC_IKFIRUS_THE_VILE;
                    case NPC_VOL_GUK:
                        return TeamInInstance == ALLIANCE ? NPC_JEDEBIA : NPC_VOL_GUK;
                    case NPC_HARAGG_THE_UNSEEN:
                        return TeamInInstance == ALLIANCE ? NPC_NIBY_THE_ALMIGHTY : NPC_HARAGG_THE_UNSEEN;
                    case NPC_GARROSH_HELLSCREAM:
                        return TeamInInstance == ALLIANCE ? NPC_KING_VARIAN_WRYNN : NPC_GARROSH_HELLSCREAM;
                    case NPC_SE_KOR_KRON_REAVER:
                        return TeamInInstance == ALLIANCE ? NPC_SE_SKYBREAKER_MARINE : NPC_SE_KOR_KRON_REAVER;
                    default:
                        break;
                }

                return entry;
            }

            uint32 GetGameObjectEntry(ObjectGuid::LowType /*guidLow*/, uint32 entry) override
            {
                switch (entry)
                {
                    case GO_GUNSHIP_ARMORY_H_10N:
                    case GO_GUNSHIP_ARMORY_H_25N:
                    case GO_GUNSHIP_ARMORY_H_10H:
                    case GO_GUNSHIP_ARMORY_H_25H:
                        if (TeamInInstance == ALLIANCE)
                            return 0;
                        break;
                    case GO_GUNSHIP_ARMORY_A_10N:
                    case GO_GUNSHIP_ARMORY_A_25N:
                    case GO_GUNSHIP_ARMORY_A_10H:
                    case GO_GUNSHIP_ARMORY_A_25H:
                        if (TeamInInstance == HORDE)
                            return 0;
                        break;
                    default:
                        break;
                }

                return entry;
            }

            void OnUnitDeath(Unit* unit) override
            {
                Creature* creature = unit->ToCreature();
                if (!creature)
                    return;

                switch (creature->GetEntry())
                {
                    case NPC_YMIRJAR_BATTLE_MAIDEN:
                    case NPC_YMIRJAR_DEATHBRINGER:
                    case NPC_YMIRJAR_FROSTBINDER:
                    case NPC_YMIRJAR_HUNTRESS:
                    case NPC_YMIRJAR_WARLORD:
                        if (Creature* crok = instance->GetCreature(CrokScourgebaneGUID))
                            crok->AI()->SetGUID(creature->GetGUID(), ACTION_VRYKUL_DEATH);
                        break;
                    case NPC_FROSTWING_WHELP:
                        if (FrostwyrmGUIDs.empty())
                            return;

                        if (creature->AI()->GetData(1/*DATA_FROSTWYRM_OWNER*/) == DATA_SPINESTALKER)
                        {
                            SpinestalkerTrash.erase(creature->GetSpawnId());
                            if (SpinestalkerTrash.empty())
                                if (Creature* spinestalk = instance->GetCreature(SpinestalkerGUID))
                                    spinestalk->AI()->DoAction(ACTION_START_FROSTWYRM);
                        }
                        else
                        {
                            RimefangTrash.erase(creature->GetSpawnId());
                            if (RimefangTrash.empty())
                                if (Creature* spinestalk = instance->GetCreature(RimefangGUID))
                                    spinestalk->AI()->DoAction(ACTION_START_FROSTWYRM);
                        }
                        break;
                    case NPC_RIMEFANG:
                    case NPC_SPINESTALKER:
                    {
                        if (instance->IsHeroic() && !HeroicAttempts)
                            return;

                        if (GetBossState(DATA_SINDRAGOSA) == DONE)
                            return;

                        FrostwyrmGUIDs.erase(creature->GetSpawnId());
                        if (FrostwyrmGUIDs.empty())
                        {
                            instance->LoadGrid(SindragosaSpawnPos.GetPositionX(), SindragosaSpawnPos.GetPositionY());
                            if (Creature* boss = instance->SummonCreature(NPC_SINDRAGOSA, SindragosaSpawnPos))
                                boss->AI()->DoAction(ACTION_START_FROSTWYRM);
                        }
                        break;
                    }
                    default:
                        break;
                }
            }

            void OnGameObjectCreate(GameObject* go) override
            {
                switch (go->GetEntry())
                {
                    case GO_DOODAD_ICECROWN_ICEWALL02:
                    case GO_ICEWALL:
                    case GO_LORD_MARROWGAR_S_ENTRANCE:
                    case GO_ORATORY_OF_THE_DAMNED_ENTRANCE:
                    case GO_ORANGE_PLAGUE_MONSTER_ENTRANCE:
                    case GO_GREEN_PLAGUE_MONSTER_ENTRANCE:
                        AddDoor(go, true);
                        break;
                    case GO_SCIENTIST_ENTRANCE:
                        AddDoor(go, true);
                        _putricideEntranceDoorGUID = go->GetGUID();
                        HandleGameObject(go->GetGUID(), _putricideTrapState == DONE, go);
                        break;
                    case GO_CRIMSON_HALL_DOOR:
                    case GO_BLOOD_ELF_COUNCIL_DOOR:
                    case GO_BLOOD_ELF_COUNCIL_DOOR_RIGHT:
                    case GO_DOODAD_ICECROWN_BLOODPRINCE_DOOR_01:
                    case GO_DOODAD_ICECROWN_GRATE_01:
                    case GO_GREEN_DRAGON_BOSS_ENTRANCE:
                    case GO_GREEN_DRAGON_BOSS_EXIT:
                    case GO_DOODAD_ICECROWN_ROOSTPORTCULLIS_02:
                    case GO_DOODAD_ICECROWN_ROOSTPORTCULLIS_03:
                    case GO_SINDRAGOSA_SHORTCUT_ENTRANCE_DOOR:
                    case GO_SINDRAGOSA_SHORTCUT_EXIT_DOOR:
                    case GO_ICE_WALL:
                        AddDoor(go, true);
                        break;
                    case GO_SINDRAGOSA_ENTRANCE_DOOR:
                        SindragosaEntranceDoorGUID = go->GetGUID();
                        go->SetGoState(SindragosaGauntletState == DONE ? GO_STATE_ACTIVE : GO_STATE_READY);
                        break;
                    // these 2 gates are functional only on 25man modes
                    case GO_DOODAD_ICECROWN_ROOSTPORTCULLIS_01:
                    case GO_DOODAD_ICECROWN_ROOSTPORTCULLIS_04:
                        if (instance->Is25ManRaid())
                            AddDoor(go, true);
                        break;
                    case GO_LADY_DEATHWHISPER_ELEVATOR:
                        LadyDeathwisperElevatorGUID = go->GetGUID();
                        if (GetBossState(DATA_LADY_DEATHWHISPER) == DONE)
                        {
                            go->SetLevel(0);
                            go->SetGoState(GO_STATE_READY);
                        }
                        break;
                    case GO_THE_SKYBREAKER_H:
                    case GO_ORGRIMS_HAMMER_A:
                        EnemyGunshipGUID = go->GetGUID();
                        break;
                    case GO_GUNSHIP_ARMORY_H_10N:
                    case GO_GUNSHIP_ARMORY_H_25N:
                    case GO_GUNSHIP_ARMORY_H_10H:
                    case GO_GUNSHIP_ARMORY_H_25H:
                    case GO_GUNSHIP_ARMORY_A_10N:
                    case GO_GUNSHIP_ARMORY_A_25N:
                    case GO_GUNSHIP_ARMORY_A_10H:
                    case GO_GUNSHIP_ARMORY_A_25H:
                        GunshipArmoryGUID = go->GetGUID();
                        break;
                    case GO_SAURFANG_S_DOOR:
                        DeathbringerSaurfangDoorGUID = go->GetGUID();
                        AddDoor(go, true);
                        break;
                    case GO_SAURFANG_CAMP_FORGE:
                    case GO_SAURFANG_CAMP_BONFIRE:
                    case GO_SAURFANG_CAMP_ANVIL:
                        // Camp props belong to the aftermath; their spawn rows are unconditional, hence the despawn.
                        SaurfangCampGUIDs.push_back(go->GetGUID());
                        if (GetBossState(DATA_DEATHBRINGER_SAURFANG) != DONE)
                            go->DespawnOrUnsummon(0ms, Seconds(WEEK));
                        // The despawn is persisted, so an instance restarted after the outro comes back
                        // with them hidden. Horde only - the Alliance camp summons its own props.
                        else if (TeamInInstance == HORDE)
                            go->Respawn();
                        break;
                    case GO_DEATHBRINGER_S_CACHE_10N:
                    case GO_DEATHBRINGER_S_CACHE_25N:
                    case GO_DEATHBRINGER_S_CACHE_10H:
                    case GO_DEATHBRINGER_S_CACHE_25H:
                        DeathbringersCacheGUID = go->GetGUID();
                        break;
                    case GO_SCOURGE_TRANSPORTER_LICHKING:
                        TeleporterLichKingGUID = go->GetGUID();
                        if (GetBossState(DATA_PROFESSOR_PUTRICIDE) == DONE && GetBossState(DATA_BLOOD_QUEEN_LANA_THEL) == DONE && GetBossState(DATA_SINDRAGOSA) == DONE)
                            go->SetGoState(GO_STATE_ACTIVE);
                        break;
                    case GO_SCOURGE_TRANSPORTER_UPPERSPIRE:
                        TeleporterUpperSpireGUID = go->GetGUID();
                        if (GetBossState(DATA_DEATHBRINGER_SAURFANG) != DONE || GetData(DATA_UPPERSPIRE_TELE_ACT) != DONE)
                            SetTeleporterState(go, false);
                        else
                            SetTeleporterState(go, true);
                        break;
                    case GO_SCOURGE_TRANSPORTER_LIGHTSHAMMER:
                        TeleporterLightsHammerGUID = go->GetGUID();
                        SetTeleporterState(go, GetBossState(DATA_LORD_MARROWGAR) == DONE);
                        break;
                    case GO_SCOURGE_TRANSPORTER_RAMPART:
                        TeleporterRampartsGUID = go->GetGUID();
                        SetTeleporterState(go, GetBossState(DATA_LADY_DEATHWHISPER) == DONE);
                        break;
                    case GO_SCOURGE_TRANSPORTER_DEATHBRINGER:
                        TeleporterDeathBringerGUID = go->GetGUID();
                        SetTeleporterState(go, GetBossState(DATA_ICECROWN_GUNSHIP_BATTLE) == DONE);
                        break;
                    case GO_SCOURGE_TRANSPORTER_ORATORY:
                        TeleporterOratoryGUID = go->GetGUID();
                        SetTeleporterState(go, GetBossState(DATA_LORD_MARROWGAR) == DONE);
                        break;
                    case GO_SCOURGE_TRANSPORTER_SINDRAGOSA:
                        TeleporterSindragosaGUID = go->GetGUID();
                        SetTeleporterState(go, GetBossState(DATA_VALITHRIA_DREAMWALKER) == DONE);
                        break;
                    case GO_PLAGUE_SIGIL:
                        PlagueSigilGUID = go->GetGUID();
                        if (GetBossState(DATA_PROFESSOR_PUTRICIDE) == DONE)
                            HandleGameObject(PlagueSigilGUID, false, go);
                        break;
                    case GO_BLOODWING_SIGIL:
                        BloodwingSigilGUID = go->GetGUID();
                        if (GetBossState(DATA_BLOOD_QUEEN_LANA_THEL) == DONE)
                            HandleGameObject(BloodwingSigilGUID, false, go);
                        break;
                    case GO_SIGIL_OF_THE_FROSTWING:
                        FrostwingSigilGUID = go->GetGUID();
                        if (GetBossState(DATA_SINDRAGOSA) == DONE)
                            HandleGameObject(FrostwingSigilGUID, false, go);
                        break;
                    case GO_SCIENTIST_AIRLOCK_DOOR_COLLISION:
                        PutricideCollisionGUID = go->GetGUID();
                        if (_putricideTrapState == IN_PROGRESS)
                            HandleGameObject(PutricideCollisionGUID, false, go);
                        else if (_putricideTrapState == DONE)
                            HandleGameObject(PutricideCollisionGUID, true, go);
                        else if ((_putricideValveState & (PUTRICIDE_VALVE_FESTERGUT | PUTRICIDE_VALVE_ROTFACE)) == (PUTRICIDE_VALVE_FESTERGUT | PUTRICIDE_VALVE_ROTFACE))
                            HandleGameObject(PutricideCollisionGUID, true, go);
                        break;
                    case GO_SCIENTIST_AIRLOCK_DOOR_ORANGE:
                        PutricideGateGUIDs[0] = go->GetGUID();
                        if (_putricideTrapState == IN_PROGRESS)
                            HandleGameObject(PutricideGateGUIDs[0], false, go);
                        else if (_putricideTrapState == DONE)
                            go->SetGoState(GO_STATE_DESTROYED);
                        else if ((_putricideValveState & (PUTRICIDE_VALVE_FESTERGUT | PUTRICIDE_VALVE_ROTFACE)) == (PUTRICIDE_VALVE_FESTERGUT | PUTRICIDE_VALVE_ROTFACE))
                            go->SetGoState(static_cast<GOState>(2));
                        else
                            HandleGameObject(PutricideGateGUIDs[0], !(_putricideValveState & PUTRICIDE_VALVE_FESTERGUT), go);
                        break;
                    case GO_SCIENTIST_AIRLOCK_DOOR_GREEN:
                        PutricideGateGUIDs[1] = go->GetGUID();
                        if (_putricideTrapState == IN_PROGRESS)
                            HandleGameObject(PutricideGateGUIDs[1], false, go);
                        else if (_putricideTrapState == DONE)
                            go->SetGoState(GO_STATE_DESTROYED);
                        else if ((_putricideValveState & (PUTRICIDE_VALVE_FESTERGUT | PUTRICIDE_VALVE_ROTFACE)) == (PUTRICIDE_VALVE_FESTERGUT | PUTRICIDE_VALVE_ROTFACE))
                            go->SetGoState(static_cast<GOState>(2));
                        else
                            HandleGameObject(PutricideGateGUIDs[1], !(_putricideValveState & PUTRICIDE_VALVE_ROTFACE), go);
                        break;
                    case GO_OOZE_RELEASE_VALVE:
                        OozeReleaseValveGUID = go->GetGUID();
                        if (GetBossState(DATA_ROTFACE) != DONE)
                            go->SetFlag(GO_FLAG_INTERACT_COND | GO_FLAG_NOT_SELECTABLE);
                        else
                            go->RemoveFlag(GO_FLAG_INTERACT_COND | GO_FLAG_NOT_SELECTABLE);
                        break;
                    case GO_GAS_RELEASE_VALVE:
                        GasReleaseValveGUID = go->GetGUID();
                        if (GetBossState(DATA_FESTERGUT) != DONE)
                            go->SetFlag(GO_FLAG_INTERACT_COND | GO_FLAG_NOT_SELECTABLE);
                        else
                            go->RemoveFlag(GO_FLAG_INTERACT_COND | GO_FLAG_NOT_SELECTABLE);
                        break;
                    case GO_DOODAD_ICECROWN_ORANGETUBES02:
                        PutricidePipeGUIDs[0] = go->GetGUID();
                        if (_putricideValveState & PUTRICIDE_VALVE_FESTERGUT)
                            HandleGameObject(PutricidePipeGUIDs[0], true, go);
                        break;
                    case GO_DOODAD_ICECROWN_GREENTUBES02:
                        PutricidePipeGUIDs[1] = go->GetGUID();
                        if (_putricideValveState & PUTRICIDE_VALVE_ROTFACE)
                            HandleGameObject(PutricidePipeGUIDs[1], true, go);
                        break;
                    case GO_DRINK_ME:
                        PutricideTableGUID = go->GetGUID();
                        break;
                    case GO_CACHE_OF_THE_DREAMWALKER_10N:
                    case GO_CACHE_OF_THE_DREAMWALKER_25N:
                    case GO_CACHE_OF_THE_DREAMWALKER_10H:
                    case GO_CACHE_OF_THE_DREAMWALKER_25H:
                        if (Creature* valithria = instance->GetCreature(ValithriaDreamwalkerGUID))
                            go->SetLootRecipient(valithria->GetLootRecipient(), valithria->GetLootRecipientGroup());
                        go->RemoveFlag(GO_FLAG_LOCKED | GO_FLAG_NOT_SELECTABLE | GO_FLAG_NODESPAWN);
                        break;
                    case GO_ARTHAS_PLATFORM:
                        // this enables movement at The Frozen Throne, when printed this value is 0.000000f
                        // however, when represented as integer client will accept only this value
                        go->SetUInt32Value(GAMEOBJECT_PARENTROTATION, 5535469);
                        ArthasPlatformGUID = go->GetGUID();
                        break;
                    case GO_ARTHAS_PRECIPICE:
                        go->SetUInt32Value(GAMEOBJECT_PARENTROTATION, 4178312);
                        ArthasPrecipiceGUID = go->GetGUID();
                        break;
                    case GO_DOODAD_ICECROWN_THRONEFROSTYEDGE01:
                        FrozenThroneEdgeGUID = go->GetGUID();
                        break;
                    case GO_DOODAD_ICECROWN_THRONEFROSTYWIND01:
                        FrozenThroneWindGUID = go->GetGUID();
                        break;
                    case GO_DOODAD_ICECROWN_SNOWEDGEWARNING01:
                        FrozenThroneWarningGUID = go->GetGUID();
                        break;
                    case GO_FROZEN_LAVAMAN:
                        FrozenBolvarGUID = go->GetGUID();
                        if (GetBossState(DATA_THE_LICH_KING) == DONE)
                            go->SetRespawnTime(7 * DAY);
                        break;
                    case GO_LAVAMAN_PILLARS_CHAINED:
                        PillarsChainedGUID = go->GetGUID();
                        if (GetBossState(DATA_THE_LICH_KING) == DONE)
                            go->SetRespawnTime(7 * DAY);
                        break;
                    case GO_LAVAMAN_PILLARS_UNCHAINED:
                        PillarsUnchainedGUID = go->GetGUID();
                        if (GetBossState(DATA_THE_LICH_KING) == DONE)
                            go->SetRespawnTime(7 * DAY);
                        break;
                    default:
                        break;
                }
            }

            void OnGameObjectRemove(GameObject* go) override
            {
                switch (go->GetEntry())
                {
                    case GO_DOODAD_ICECROWN_ICEWALL02:
                    case GO_ICEWALL:
                    case GO_LORD_MARROWGAR_S_ENTRANCE:
                    case GO_ORATORY_OF_THE_DAMNED_ENTRANCE:
                    case GO_SAURFANG_S_DOOR:
                    case GO_ORANGE_PLAGUE_MONSTER_ENTRANCE:
                    case GO_GREEN_PLAGUE_MONSTER_ENTRANCE:
                    case GO_SCIENTIST_ENTRANCE:
                    case GO_CRIMSON_HALL_DOOR:
                    case GO_BLOOD_ELF_COUNCIL_DOOR:
                    case GO_BLOOD_ELF_COUNCIL_DOOR_RIGHT:
                    case GO_DOODAD_ICECROWN_BLOODPRINCE_DOOR_01:
                    case GO_DOODAD_ICECROWN_GRATE_01:
                    case GO_GREEN_DRAGON_BOSS_ENTRANCE:
                    case GO_GREEN_DRAGON_BOSS_EXIT:
                    case GO_DOODAD_ICECROWN_ROOSTPORTCULLIS_01:
                    case GO_DOODAD_ICECROWN_ROOSTPORTCULLIS_02:
                    case GO_DOODAD_ICECROWN_ROOSTPORTCULLIS_03:
                    case GO_DOODAD_ICECROWN_ROOSTPORTCULLIS_04:
                    case GO_SINDRAGOSA_SHORTCUT_ENTRANCE_DOOR:
                    case GO_SINDRAGOSA_SHORTCUT_EXIT_DOOR:
                    case GO_ICE_WALL:
                        AddDoor(go, false);
                        break;
                    case GO_THE_SKYBREAKER_A:
                    case GO_ORGRIMS_HAMMER_H:
                        GunshipGUID.Clear();
                        break;
                    default:
                        break;
                }
            }

            uint32 GetData(uint32 type) const override
            {
                switch (type)
                {
                    case DATA_SAURFANG_OUTRO_ZEPPELIN:
                    // DONE once it has come to rest, so the outro can wait for the ship instead of guessing.
                    return _saurfangZeppelinDocked ? DONE : IN_PROGRESS;
                    case DATA_SINDRAGOSA_FROSTWYRMS:
                        return FrostwyrmGUIDs.size();
                    case DATA_SPINESTALKER:
                        return SpinestalkerTrash.size();
                    case DATA_RIMEFANG:
                        return RimefangTrash.size();
                    case DATA_COLDFLAME_JETS:
                        return ColdflameJetsState;
                    case DATA_UPPERSPIRE_TELE_ACT:
                        return UpperSpireTeleporterActiveState;
                    case DATA_TEAM_IN_INSTANCE:
                        return TeamInInstance;
                    case DATA_BLOOD_QUICKENING_STATE:
                        return BloodQuickeningState;
                    case DATA_HEROIC_ATTEMPTS:
                        return HeroicAttempts;
                    case DATA_BLOOD_PRINCE_COUNCIL_INTRO:
                        return BloodPrinceIntro;
                    case DATA_SINDRAGOSA_GAUNTLET:
                        return SindragosaGauntletState;
                    case DATA_PUTRICIDE_TRAP_STATE:
                        return _putricideTrapState;
                    case DATA_SINDRAGOSA_INTRO:
                        return SindragosaIntro;
                    case DATA_FACTION_BUFF:
                        return IsFactionBuffActive ? 1 : 0;
                    default:
                        break;
                }

                return 0;
            }

            ObjectGuid GetGuidData(uint32 type) const override
            {
                switch (type)
                {
                    case DATA_LORD_MARROWGAR:
                        return LordMarrowgarGUID;
                    case DATA_LADY_DEATHWHISPER:
                        return LadyDeahtwhisperGUID;
                    case DATA_ICECROWN_GUNSHIP_BATTLE:
                        return GunshipGUID;
                    case DATA_ENEMY_GUNSHIP:
                        return EnemyGunshipGUID;
                    case DATA_DEATHBRINGER_SAURFANG:
                        return DeathbringerSaurfangGUID;
                    case DATA_SAURFANG_EVENT_NPC:
                        return DeathbringerSaurfangEventGUID;
                    case GO_SAURFANG_S_DOOR:
                        return DeathbringerSaurfangDoorGUID;
                    case DATA_FESTERGUT:
                        return FestergutGUID;
                    case DATA_ROTFACE:
                        return RotfaceGUID;
                    case DATA_PROFESSOR_PUTRICIDE:
                        return ProfessorPutricideGUID;
                    case NPC_PUTRICADES_TRAP:
                        return PutricadesTrapGUID;
                    case DATA_PUTRICIDE_TABLE:
                        return PutricideTableGUID;
                    case DATA_PRINCE_KELESETH:
                        return BloodCouncilGUIDs[0];
                    case DATA_PRINCE_TALDARAM:
                        return BloodCouncilGUIDs[1];
                    case DATA_PRINCE_VALANAR:
                        return BloodCouncilGUIDs[2];
                    case DATA_BLOOD_PRINCES_CONTROL:
                        return BloodCouncilControllerGUID;
                    case DATA_BLOOD_QUEEN_LANA_THEL_COUNCIL:
                        return BloodQueenLanaThelCouncilGUID;
                    case DATA_BLOOD_QUEEN_LANA_THEL:
                        return BloodQueenLanaThelGUID;
                    case DATA_CROK_SCOURGEBANE:
                        return CrokScourgebaneGUID;
                    case DATA_CAPTAIN_ARNATH:
                    case DATA_CAPTAIN_BRANDON:
                    case DATA_CAPTAIN_GRONDEL:
                    case DATA_CAPTAIN_RUPERT:
                        return CrokCaptainGUIDs[type - DATA_CAPTAIN_ARNATH];
                    case DATA_SISTER_SVALNA:
                        return SisterSvalnaGUID;
                    case DATA_VALITHRIA_DREAMWALKER:
                        return ValithriaDreamwalkerGUID;
                    case DATA_VALITHRIA_LICH_KING:
                        return ValithriaLichKingGUID;
                    case DATA_VALITHRIA_TRIGGER:
                        return ValithriaTriggerGUID;
                    case DATA_SINDRAGOSA_GAUNTLET:
                        return SindragosaGauntletGUID;
                    case GO_SINDRAGOSA_ENTRANCE_DOOR:
                        return SindragosaEntranceDoorGUID;
                    case DATA_SINDRAGOSA:
                        return SindragosaGUID;
                    case DATA_SPINESTALKER:
                        return SpinestalkerGUID;
                    case DATA_RIMEFANG:
                        return RimefangGUID;
                    case DATA_THE_LICH_KING:
                        return TheLichKingGUID;
                    case DATA_HIGHLORD_TIRION_FORDRING:
                        return HighlordTirionFordringGUID;
                    case DATA_ARTHAS_PLATFORM:
                        return ArthasPlatformGUID;
                    case DATA_TERENAS_MENETHIL:
                        return TerenasMenethilGUID;
                    default:
                        break;
                }

                return ObjectGuid::Empty;
            }

            void HandleHeroicAttempts()
            {
                if (HeroicAttempts)
                {
                    --HeroicAttempts;
                    DoUpdateWorldState(WORLDSTATE_ATTEMPTS_REMAINING, HeroicAttempts);
                }

                if (!HeroicAttempts)
                {
                    for (ObjectGuid const& bossGuid : { ProfessorPutricideGUID, BloodQueenLanaThelGUID, SindragosaGUID, TheLichKingGUID })
                    {
                        if (Creature* boss = instance->GetCreature(bossGuid))
                            if (boss->IsAlive())
                                boss->DespawnOrUnsummon();
                    }
                }
            }

            bool SetBossState(uint32 type, EncounterState state) override
            {
                if (!InstanceScript::SetBossState(type, state))
                    return false;

                switch (type)
                {
                    case DATA_LORD_MARROWGAR:
                    {
                        if (state == DONE)
                        {
                            if (GameObject* teleporter = instance->GetGameObject(TeleporterLightsHammerGUID))
                                SetTeleporterState(teleporter, true);
                            if (GameObject* teleporter = instance->GetGameObject(TeleporterOratoryGUID))
                                SetTeleporterState(teleporter, true);
                        }
                        break;
                    }
                    case DATA_LADY_DEATHWHISPER:
                    {
                        if (state == DONE)
                        {
                            if (GameObject* teleporter = instance->GetGameObject(TeleporterRampartsGUID))
                                SetTeleporterState(teleporter, true);

                            if (GameObject* elevator = instance->GetGameObject(LadyDeathwisperElevatorGUID))
                            {
                                elevator->SetLevel(0);
                                elevator->SetGoState(GO_STATE_READY);
                            }

                            SpawnGunship();
                        }
                        break;
                    }
                    case DATA_ICECROWN_GUNSHIP_BATTLE:
                        if (state == DONE)
                        {
                            if (GameObject* teleporter = instance->GetGameObject(TeleporterDeathBringerGUID))
                                SetTeleporterState(teleporter, true);

                            if (GameObject* loot = instance->GetGameObject(GunshipArmoryGUID))
                                loot->RemoveFlag(GO_FLAG_LOCKED | GO_FLAG_NOT_SELECTABLE | GO_FLAG_NODESPAWN);
                        }
                        else if (state == FAIL)
                            Events.ScheduleEvent(EVENT_RESPAWN_GUNSHIP, 30s);
                        break;
                    case DATA_DEATHBRINGER_SAURFANG:
                        switch (state)
                        {
                            case DONE:
                            {
                                if (GameObject* loot = instance->GetGameObject(DeathbringersCacheGUID))
                                {
                                    if (Creature* deathbringer = instance->GetCreature(DeathbringerSaurfangGUID))
                                        loot->SetLootRecipient(deathbringer->GetLootRecipient(), deathbringer->GetLootRecipientGroup());
                                    loot->RemoveFlag(GO_FLAG_LOCKED | GO_FLAG_NOT_SELECTABLE | GO_FLAG_NODESPAWN);
                                }

                                if (GameObject* teleporter = instance->GetGameObject(TeleporterUpperSpireGUID))
                                    SetTeleporterState(teleporter, true);

                                if (GameObject* teleporter = instance->GetGameObject(TeleporterDeathBringerGUID))
                                    SetTeleporterState(teleporter, true);
                                break;
                            }
                            case NOT_STARTED:
                            {
                                if (GameObject* teleporter = instance->GetGameObject(TeleporterDeathBringerGUID))
                                    SetTeleporterState(teleporter, true);
                                break;
                            }
                            case IN_PROGRESS:
                            {
                                if (GameObject* teleporter = instance->GetGameObject(TeleporterDeathBringerGUID))
                                    SetTeleporterState(teleporter, false);
                                break;
                            }
                            default:
                                break;
                        }
                        break;
                    case DATA_FESTERGUT:
                        if (state == DONE)
                            if (GameObject* go = instance->GetGameObject(GasReleaseValveGUID))
                                go->RemoveFlag(GO_FLAG_INTERACT_COND | GO_FLAG_NOT_SELECTABLE);
                        break;
                    case DATA_ROTFACE:
                        if (state == DONE)
                            if (GameObject* go = instance->GetGameObject(OozeReleaseValveGUID))
                                go->RemoveFlag(GO_FLAG_INTERACT_COND | GO_FLAG_NOT_SELECTABLE);
                        break;
                    case DATA_PROFESSOR_PUTRICIDE:
                        HandleGameObject(PlagueSigilGUID, state != DONE);
                        if (instance->IsHeroic() && state == FAIL)
                            HandleHeroicAttempts();
                        else if (state == DONE)
                            CheckLichKingAvailability();
                        break;
                    case DATA_BLOOD_QUEEN_LANA_THEL:
                        HandleGameObject(BloodwingSigilGUID, state != DONE);
                        if (instance->IsHeroic() && state == FAIL)
                            HandleHeroicAttempts();
                        else if (state == DONE)
                            CheckLichKingAvailability();
                        break;
                    case DATA_VALITHRIA_DREAMWALKER:
                        if (state == DONE)
                        {
                            if (sQuestPoolMgr->IsQuestActive(WeeklyQuestData[8].questId[instance->GetSpawnMode() & 1]))
                                instance->SummonCreature(NPC_VALITHRIA_DREAMWALKER_QUEST, ValithriaSpawnPos);
                            if (GameObject* teleporter = instance->GetGameObject(TeleporterSindragosaGUID))
                                SetTeleporterState(teleporter, true);
                        }
                        break;
                    case DATA_SINDRAGOSA:
                        HandleGameObject(FrostwingSigilGUID, state != DONE);
                        if (instance->IsHeroic() && state == FAIL)
                            HandleHeroicAttempts();
                        else if (state == DONE)
                            CheckLichKingAvailability();
                        break;
                    case DATA_THE_LICH_KING:
                    {
                        // set the platform as active object to dramatically increase visibility range
                        // note: "active" gameobjects do not block grid unloading
                        if (GameObject* precipice = instance->GetGameObject(ArthasPrecipiceGUID))
                            precipice->SetFarVisible(state == IN_PROGRESS);

                        if (GameObject* platform = instance->GetGameObject(ArthasPlatformGUID))
                            platform->SetFarVisible(state == IN_PROGRESS);

                        if (instance->IsHeroic() && state == FAIL)
                            HandleHeroicAttempts();
                        else if (state == DONE)
                        {
                            if (GameObject* bolvar = instance->GetGameObject(FrozenBolvarGUID))
                                bolvar->SetRespawnTime(7 * DAY);
                            if (GameObject* pillars = instance->GetGameObject(PillarsChainedGUID))
                                pillars->SetRespawnTime(7 * DAY);
                            if (GameObject* pillars = instance->GetGameObject(PillarsUnchainedGUID))
                                pillars->SetRespawnTime(7 * DAY);

                            instance->SummonCreature(NPC_LADY_JAINA_PROUDMOORE_QUEST, JainaSpawnPos);
                            instance->SummonCreature(NPC_MURADIN_BRONZEBEARD_QUEST, MuradinSpawnPos);
                            instance->SummonCreature(NPC_UTHER_THE_LIGHTBRINGER_QUEST, UtherSpawnPos);
                            instance->SummonCreature(NPC_LADY_SYLVANAS_WINDRUNNER_QUEST, SylvanasSpawnPos);
                        }
                        break;
                    }
                    default:
                        break;
                 }

                 return true;
            }

            void SpawnGunship()
            {
                if (!GunshipGUID)
                {
                    SetBossState(DATA_ICECROWN_GUNSHIP_BATTLE, NOT_STARTED);
                    uint32 gunshipEntry = TeamInInstance == HORDE ? GO_ORGRIMS_HAMMER_H : GO_THE_SKYBREAKER_A;
                    if (Transport* gunship = sTransportMgr->CreateTransport(gunshipEntry, 0, instance))
                        GunshipGUID = gunship->GetGUID();
                }
            }

            GameObject* SummonGameObject(uint32 entry, Position const& pos, uint32 respawnTime)
            {
                QuaternionData rot = QuaternionData::fromEulerAnglesZYX(pos.GetOrientation(), 0.f, 0.f);

                GameObject* go = new GameObject();
                if (!go->Create(instance->GenerateLowGuid<HighGuid::GameObject>(), entry, instance, PHASEMASK_NORMAL, pos, rot, 255, GO_STATE_READY))
                {
                    delete go;
                    return nullptr;
                }

                // Xinef: if gameobject is temporary, set custom spellid
                if (respawnTime)
                    go->SetSpellId(1);

                go->SetRespawnTime(respawnTime);
                go->SetSpawnedByDefault(false);

                instance->AddToMap(go);
                return go;
            }

            // Hidden rather than despawned: a despawned creature leaves the map and its guid would no
            // longer resolve.
            void HideSaurfangEventNpc(Creature* creature)
            {
                if (GetBossState(DATA_ICECROWN_GUNSHIP_BATTLE) == DONE)
                    return;

                creature->SetVisible(false);
            }

            void RestoreSaurfangEventNpc(Creature* creature)
            {
                creature->NearTeleportTo(creature->GetHomePosition());
                creature->SetStandState(UNIT_STAND_STATE_STAND);
                creature->SetEmoteState(EMOTE_ONESHOT_NONE);
                creature->SetSheath(SHEATH_STATE_UNARMED);
                creature->SetVisible(true);
            }

            void SpawnSaurfangEventNpcs()
            {
                if (Creature* captain = instance->GetCreature(DeathbringerSaurfangEventGUID))
                    RestoreSaurfangEventNpc(captain);

                for (ObjectGuid const& guid : SaurfangEventGuardGUIDs)
                    if (Creature* guard = instance->GetCreature(guid))
                        RestoreSaurfangEventNpc(guard);
            }

            // Only the ones left hidden: a scene in progress has them visible and placed, and
            // teleporting those to their spawn points would break it mid-run.
            void RestoreHiddenSaurfangEventNpcs()
            {
                if (Creature* captain = instance->GetCreature(DeathbringerSaurfangEventGUID))
                    if (!captain->IsVisible())
                        RestoreSaurfangEventNpc(captain);

                for (ObjectGuid const& guid : SaurfangEventGuardGUIDs)
                    if (Creature* guard = instance->GetCreature(guid))
                        if (!guard->IsVisible())
                            RestoreSaurfangEventNpc(guard);
            }

            // staged runs the on-screen build: teleporters, then workers raising the tents, then the
            // vendors. Unstaged drops the finished camp at once, for an instance already DONE.
            void SpawnSaurfangCamp(bool staged)
            {
                if (_saurfangCampSpawned || !instance->HavePlayers())
                    return;

                _saurfangCampSpawned = true;
                if (staged)
                {
                    Events.ScheduleEvent(EVENT_SAURFANG_CAMP_TELEPORTERS, 3s);
                    return;
                }

                SummonSaurfangCampTeleporters();
                SpawnSaurfangEventNpcs();
                SpawnSaurfangCampTents();
                SummonSaurfangCampVendor(true, false);
                SummonSaurfangCampVendor(false, false);
            }

            void SummonSaurfangCampTeleporters()
            {
                uint32 const teleporter = TeamInInstance == HORDE ? GO_SAURFANG_CAMP_TELEPORTER_H : GO_SAURFANG_CAMP_TELEPORTER_A;
                for (uint8 i = 0; i < 2; ++i)
                {
                    if (GameObject* go = SummonGameObject(teleporter, SaurfangCampTeleporterPos[i], WEEK))
                    {
                        go->setActive(true);
                        go->SetGoState(GO_STATE_ACTIVE);
                    }
                }
            }

            void SpawnSaurfangCampTeleporters()
            {
                SummonSaurfangCampTeleporters();
                Events.ScheduleEvent(EVENT_SAURFANG_CAMP_WORKERS, 5s);
            }

            void SpawnSaurfangCampWorkers()
            {
                bool const horde = TeamInInstance == HORDE;
                uint32 const worker = horde ? NPC_CAMP_WARSONG_PEON : NPC_CAMP_ALLIANCE_MASON;
                for (uint8 i = 0; i < 2; ++i)
                {
                    if (Creature* builder = instance->SummonCreature(worker, SaurfangCampTeleporterPos[i]))
                    {
                        SaurfangCampWorkerGUIDs.push_back(builder->GetGUID());
                        builder->SetReactState(REACT_PASSIVE);
                        builder->CastSpell(builder, SPELL_OUTRO_TELEPORT_VISUAL, true);
                    }
                }

                Events.ScheduleEvent(EVENT_SAURFANG_CAMP_WORKERS_FIRST_POS, 0s);
                Events.ScheduleEvent(EVENT_SAURFANG_CAMP_WORKERS_RE_POS, 1500ms);
                const Seconds workTime = SaurfangCampWorkerTravel + (TeamInInstance == HORDE ? 3s : 11s);
                Events.ScheduleEvent(EVENT_SAURFANG_CAMP_WORKERS_FIRST_POS, workTime);
                // Timed from the summon, so the run out has to be paid for before the hammering starts.
                Events.ScheduleEvent(EVENT_SAURFANG_CAMP_WORKERS_BACK, workTime + 1500ms);
            }

            void SpawnSaurfangCampTents()
            {
                bool const horde = TeamInInstance == HORDE;
                uint32 const tents[2] =
                {
                    static_cast<uint32>(horde ? GO_SAURFANG_CAMP_TENT_H1 : GO_SAURFANG_CAMP_TENT_A),
                    static_cast<uint32>(horde ? GO_SAURFANG_CAMP_TENT_H2 : GO_SAURFANG_CAMP_TENT_A)
                };

                Position const* tentPos = horde ? SaurfangCampTentPosH : SaurfangCampTentPosA;
                for (uint8 i = 0; i < 2; ++i)
                    if (GameObject* go = SummonGameObject(tents[i], tentPos[i], WEEK))
                        go->setActive(true);

                if (horde)
                {
                    for (ObjectGuid const& guid : SaurfangCampGUIDs)
                        if (GameObject* camp = instance->GetGameObject(guid))
                        {
                            camp->Respawn();
                            camp->setActive(true);
                        }
                }
                else
                {
                    if (GameObject* go = SummonGameObject(GO_SAURFANG_CAMP_FORGE, SaurfangCampForgePosA, WEEK))
                        go->setActive(true);
                    if (GameObject* go = SummonGameObject(GO_SAURFANG_CAMP_ANVIL_A, SaurfangCampAnvilPosA, WEEK))
                        go->setActive(true);
                    if (GameObject* go = SummonGameObject(GO_SAURFANG_CAMP_BANNER_A, SaurfangCampBannerPosA, WEEK))
                        go->setActive(true);
                }
            }

            // They clear the site before the tent drops, or it lands on top of them.
            void SendSaurfangCampWorkersBack()
            {
                uint8 i = 0;
                for (ObjectGuid const& guid : SaurfangCampWorkerGUIDs)
                {
                    if (Creature* builder = instance->GetCreature(guid))
                    {
                        builder->SetEmoteState(EMOTE_ONESHOT_NONE);
                        builder->SetWalk(false);
                        builder->GetMotionMaster()->MovePoint(0, SaurfangCampTeleporterPos[1 - (i % 2)]);
                    }
                    ++i;
                }

                Events.ScheduleEvent(EVENT_SAURFANG_CAMP_TENTS, 500ms);
                // They only vanish once they are back standing on the pad.
                Events.ScheduleEvent(EVENT_SAURFANG_CAMP_WORKERS_OUT,
                    SaurfangCampWorkerTravel + (TeamInInstance == HORDE ? 2s : 5s));
            }

            void DespawnSaurfangCampWorkers()
            {
                for (ObjectGuid const& guid : SaurfangCampWorkerGUIDs)
                    if (Creature* builder = instance->GetCreature(guid))
                    {
                        builder->CastSpell(builder, SPELL_OUTRO_TELEPORT_VISUAL, true);
                        builder->DespawnOrUnsummon(1s);
                    }

                SaurfangCampWorkerGUIDs.clear();
                Events.ScheduleEvent(EVENT_SAURFANG_CAMP_VENDORS, 2s);
            }

            // walkIn false drops the vendor straight on his pitch, for a camp not built on screen.
            // The detours exist because the direct line clips the Horde bonfire / the first Alliance tent.
            void SummonSaurfangCampVendor(bool smith, bool walkIn)
            {
                bool const horde = TeamInInstance == HORDE;
                uint32 const entry = smith ? (horde ? NPC_CAMP_MORGAN_DAYBLAZE : NPC_CAMP_SHELY_STEELBOWELS)
                    : (horde ? NPC_CAMP_APOTHECARY_CANDITH_TOMAS : NPC_CAMP_BRAZIE_GETZ);
                Position const& pitch = smith ? (horde ? SaurfangCampBlacksmithPos : SaurfangCampBlacksmithPosA)
                    : (horde ? SaurfangCampGeneralGoodsPos : SaurfangCampGeneralGoodsPosA);

                if (!walkIn)
                {
                    instance->SummonCreature(entry, pitch);
                    return;
                }

                Creature* vendor = instance->SummonCreature(entry, SaurfangCampTeleporterPos[smith ? 0 : 1]);
                if (!vendor)
                    return;

                vendor->CastSpell(vendor, SPELL_OUTRO_TELEPORT_VISUAL, true);
                vendor->setActive(true);
                vendor->SetWalk(true);
                if (horde && !smith)
                {
                    vendor->GetMotionMaster()->MovePoint(0, pitch, false, pitch.GetOrientation());
                    return;
                }

                Position const& detour = horde ? SaurfangCampSmithDetourPos
                    : (smith ? SaurfangCampSmithDetourPosA : SaurfangCampGoodsDetourPosA);
                vendor->GetMotionMaster()->MovePoint(0, detour, false, detour.GetOrientation());

                if (smith)
                {
                    SaurfangCampSmithGUID = vendor->GetGUID();
                    Events.ScheduleEvent(EVENT_SAURFANG_CAMP_SMITH_ARRIVE, horde ? 19s : 17s);
                }
                else
                {
                    SaurfangCampGoodsGUID = vendor->GetGUID();
                    Events.ScheduleEvent(EVENT_SAURFANG_CAMP_VENDOR_ARRIVE, 15s);
                }
            }

            void SpawnSaurfangCampVendors()
            {
                SummonSaurfangCampVendor(true, true);
                SummonSaurfangCampVendor(false, true);
            }

            void SetData(uint32 type, uint32 data) override
            {
                switch (type)
                {
                        case DATA_SAURFANG_CAMP:
                        // The boss is DONE for the whole outro, so the encounter state alone cannot tell
                        // "outro playing" from "outro over".
                        if (data == IN_PROGRESS)
                        {
                            _saurfangOutroRunning = true;
                            // Safety net: an instance emptying mid-outro would leave the flag set and
                            // the camp could never be raised.
                            Events.ScheduleEvent(EVENT_SAURFANG_OUTRO_TIMEOUT, 8min);
                        }
                        else if (data == DONE)
                        {
                            Events.CancelEvent(EVENT_SAURFANG_OUTRO_TIMEOUT);
                            _saurfangOutroRunning = false;
                            SpawnSaurfangCamp(true);
                        }
                        break;
                    case DATA_SAURFANG_OUTRO_ZEPPELIN:
                        if (data == IN_PROGRESS)
                        {
                            if (!SaurfangZeppelinGUID)
                            {
                                _saurfangZeppelinDocked = false;
                                _saurfangZeppelinLeaving = false;
                                if (Transport* zeppelin = sTransportMgr->CreateTransport(GO_SAURFANG_OUTRO_ZEPPELIN, 0, instance))
                                {
                                    SaurfangZeppelinGUID = zeppelin->GetGUID();
                                    zeppelin->setActive(true);
                                    Events.ScheduleEvent(EVENT_SAURFANG_ZEPPELIN_DOCK, 1s);
                                }
                            }
                        }
                        // Asked for twice (departure, then cleanup); without the guard the second call
                        // would restart the removal timer.
                        else if (SaurfangZeppelinGUID != ObjectGuid::Empty && !_saurfangZeppelinLeaving)
                        {
                            Events.CancelEvent(EVENT_SAURFANG_ZEPPELIN_DOCK);
                            _saurfangZeppelinDocked = false;
                            _saurfangZeppelinLeaving = true;
                            if (GameObject* go = instance->GetGameObject(SaurfangZeppelinGUID))
                                if (Transport* zeppelin = go->ToTransport())
                                    zeppelin->EnableMovement(true);

                            // Releasing _pendingStop does not launch it: it leaves when the stop frame
                            // DepartureTime comes round, ~24s. Removal is well past that.
                            Events.ScheduleEvent(EVENT_SAURFANG_ZEPPELIN_REMOVE, 60s);
                        }
                        break;
                    case DATA_SAURFANG_OUTRO_PORTAL:
                        if (data == IN_PROGRESS)
                        {
                            if (GameObject* portal = SummonGameObject(GO_SAURFANG_OUTRO_PORTAL, SaurfangOutroPortalPos, HOUR))
                            {
                                // SPELLCASTER object carrying spell 59065; scenery here, so it must not be clickable.
                                portal->SetFlag(GO_FLAG_NOT_SELECTABLE);
                                SaurfangPortalGUID = portal->GetGUID();
                            }
                        }
                        else
                        {
                            if (GameObject* portal = instance->GetGameObject(SaurfangPortalGUID))
                                portal->DespawnOrUnsummon();
                            SaurfangPortalGUID.Clear();
                        }
                        break;
                    case DATA_BONED_ACHIEVEMENT:
                        IsBonedEligible = data ? true : false;
                        break;
                    case DATA_OOZE_DANCE_ACHIEVEMENT:
                        IsOozeDanceEligible = data ? true : false;
                        break;
                    case DATA_NAUSEA_ACHIEVEMENT:
                        IsNauseaEligible = data ? true : false;
                        break;
                    case DATA_ORB_WHISPERER_ACHIEVEMENT:
                        IsOrbWhispererEligible = data ? true : false;
                        break;
                    case DATA_COLDFLAME_JETS:
                        ColdflameJetsState = data;
                        if (ColdflameJetsState == DONE)
                            SaveToDB();
                        break;
                    case DATA_BLOOD_QUICKENING_STATE:
                    {
                        // skip if nothing changes
                        if (BloodQuickeningState == data)
                            break;

                        // 5 is the index of Blood Quickening
                        if (!sQuestPoolMgr->IsQuestActive(WeeklyQuestData[5].questId[instance->GetSpawnMode() & 1]))
                            break;

                        switch (data)
                        {
                            case IN_PROGRESS:
                                Events.ScheduleEvent(EVENT_UPDATE_EXECUTION_TIME, 1min);
                                BloodQuickeningMinutes = 30;
                                DoUpdateWorldState(WORLDSTATE_SHOW_TIMER, 1);
                                DoUpdateWorldState(WORLDSTATE_EXECUTION_TIME, BloodQuickeningMinutes);
                                break;
                            case DONE:
                                Events.CancelEvent(EVENT_UPDATE_EXECUTION_TIME);
                                BloodQuickeningMinutes = 0;
                                DoUpdateWorldState(WORLDSTATE_SHOW_TIMER, 0);
                                break;
                            default:
                                break;
                        }

                        BloodQuickeningState = data;
                        SaveToDB();
                        break;
                    }
                    case DATA_UPPERSPIRE_TELE_ACT:
                        UpperSpireTeleporterActiveState = data;
                        if (UpperSpireTeleporterActiveState == DONE)
                        {
                            if (GameObject* go = instance->GetGameObject(TeleporterUpperSpireGUID))
                                SetTeleporterState(go, true);
                            SaveToDB();
                        }
                        break;
                    case DATA_BLOOD_PRINCE_COUNCIL_INTRO:
                        BloodPrinceIntro = data;
                        break;
                    case DATA_SINDRAGOSA_GAUNTLET:
                        SindragosaGauntletState = data;

                        if (GameObject* go = instance->GetGameObject(SindragosaEntranceDoorGUID))
                            go->SetGoState(data == DONE ? GO_STATE_ACTIVE : GO_STATE_READY);

                        if (data == DONE)
                            SaveToDB();
                        break;
                    case DATA_PUTRICIDE_TRAP_STATE:
                    {
                        _putricideTrapState = data;

                        if (GameObject* go = instance->GetGameObject(_putricideEntranceDoorGUID))
                            HandleGameObject(go->GetGUID(), data == DONE, go);

                        if (data == IN_PROGRESS)
                        {
                            if (GameObject* go = instance->GetGameObject(PutricideCollisionGUID))
                                HandleGameObject(go->GetGUID(), false, go);

                            if (GameObject* go = instance->GetGameObject(PutricideGateGUIDs[0]))
                                HandleGameObject(go->GetGUID(), false, go);

                            if (GameObject* go = instance->GetGameObject(PutricideGateGUIDs[1]))
                                HandleGameObject(go->GetGUID(), false, go);
                        }
                        else if (data == NOT_STARTED)
                        {
                            // Nach komplettem Wipe: Airlock wieder vollständig öffnen
                            HandleGameObject(PutricideCollisionGUID, true);

                            if (GameObject* go = instance->GetGameObject(PutricideGateGUIDs[0]))
                                go->SetGoState(GO_STATE_DESTROYED);

                            if (GameObject* go = instance->GetGameObject(PutricideGateGUIDs[1]))
                                go->SetGoState(GO_STATE_DESTROYED);

                            // Putricide-Eingang bleibt geschlossen
                            if (GameObject* go = instance->GetGameObject(_putricideEntranceDoorGUID))
                                HandleGameObject(go->GetGUID(), false, go);
                        }
                        else if (data == DONE)
                        {
                            HandleGameObject(PutricideCollisionGUID, true);

                            if (GameObject* go = instance->GetGameObject(PutricideGateGUIDs[0]))
                                go->SetGoState(GO_STATE_DESTROYED);

                            if (GameObject* go = instance->GetGameObject(PutricideGateGUIDs[1]))
                                go->SetGoState(GO_STATE_DESTROYED);

                            SaveToDB();
                        }

                        break;
                    }
                    case DATA_SINDRAGOSA_INTRO:
                        SindragosaIntro = data;
                        break;
                    case DATA_FACTION_BUFF:
                        IsFactionBuffActive = data ? true : false;
                        if (!IsFactionBuffActive)
                            DoRemoveAurasDueToSpellOnPlayers(TeamInInstance == ALLIANCE ? SPELL_STRENGHT_OF_WRYNN : SPELL_HELLSCREAMS_WARSONG, true, true);
                        break;
                    case DATA_NERUBAR_BROODKEEPER_EVENT:
                    {
                        uint8 group = (data == AT_NERUBAR_BROODKEEPER) ? 0 : 1;
                        for (ObjectGuid guid : nerubarBroodkeepersGUIDs[group])
                            if (Creature* nerubar = instance->GetCreature(guid))
                                nerubar->AI()->DoAction(ACTION_NERUBAR_FALL);
                        break;
                    }
                    default:
                        break;
                }
            }

            void SetData64(uint32 type, uint64 data) override
            {
                switch (type)
                {
                    case DATA_SINDRAGOSA_FROSTWYRMS:
                        FrostwyrmGUIDs.insert(data);
                        break;
                    case DATA_SPINESTALKER:
                        SpinestalkerTrash.insert(data);
                        break;
                    case DATA_RIMEFANG:
                        RimefangTrash.insert(data);
                        break;
                    default:
                        break;
                }
            }

            bool CheckAchievementCriteriaMeet(uint32 criteria_id, Player const* /*source*/, Unit const* /*target*/, uint32 /*miscvalue1*/) override
            {
                switch (criteria_id)
                {
                    case CRITERIA_BONED_10N:
                    case CRITERIA_BONED_25N:
                    case CRITERIA_BONED_10H:
                    case CRITERIA_BONED_25H:
                        return IsBonedEligible;
                    case CRITERIA_DANCES_WITH_OOZES_10N:
                    case CRITERIA_DANCES_WITH_OOZES_25N:
                    case CRITERIA_DANCES_WITH_OOZES_10H:
                    case CRITERIA_DANCES_WITH_OOZES_25H:
                        return IsOozeDanceEligible;
                    case CRITERIA_NAUSEA_10N:
                    case CRITERIA_NAUSEA_25N:
                    case CRITERIA_NAUSEA_10H:
                    case CRITERIA_NAUSEA_25H:
                        return IsNauseaEligible;
                    case CRITERIA_ORB_WHISPERER_10N:
                    case CRITERIA_ORB_WHISPERER_25N:
                    case CRITERIA_ORB_WHISPERER_10H:
                    case CRITERIA_ORB_WHISPERER_25H:
                        return IsOrbWhispererEligible;
                    // Only one criteria for both modes, need to do it like this
                    case CRITERIA_KILL_LANA_THEL_10M:
                    case CRITERIA_ONCE_BITTEN_TWICE_SHY_10N:
                    case CRITERIA_ONCE_BITTEN_TWICE_SHY_10V:
                        return instance->ToInstanceMap()->GetMaxPlayers() == 10;
                    case CRITERIA_KILL_LANA_THEL_25M:
                    case CRITERIA_ONCE_BITTEN_TWICE_SHY_25N:
                    case CRITERIA_ONCE_BITTEN_TWICE_SHY_25V:
                        return instance->ToInstanceMap()->GetMaxPlayers() == 25;
                    default:
                        break;
                }

                return false;
            }

            bool CheckRequiredBosses(uint32 bossId, Player const* player = nullptr) const override
            {
                if (_SkipCheckRequiredBosses(player))
                    return true;

                switch (bossId)
                {
                    case DATA_THE_LICH_KING:
                        if (!CheckPlagueworks(bossId))
                            return false;
                        if (!CheckCrimsonHalls(bossId))
                            return false;
                        if (!CheckFrostwingHalls(bossId))
                            return false;
                        break;
                    case DATA_SINDRAGOSA:
                    case DATA_VALITHRIA_DREAMWALKER:
                        if (!CheckFrostwingHalls(bossId))
                            return false;
                        break;
                    case DATA_BLOOD_QUEEN_LANA_THEL:
                    case DATA_BLOOD_PRINCE_COUNCIL:
                        if (!CheckCrimsonHalls(bossId))
                            return false;
                        break;
                    case DATA_FESTERGUT:
                    case DATA_ROTFACE:
                    case DATA_PROFESSOR_PUTRICIDE:
                        if (!CheckPlagueworks(bossId))
                            return false;
                        break;
                    default:
                        break;
                }

                if (!CheckLowerSpire(bossId))
                    return false;

                return true;
            }

            bool CheckPlagueworks(uint32 bossId) const
            {
                switch (bossId)
                {
                    case DATA_THE_LICH_KING:
                        if (GetBossState(DATA_PROFESSOR_PUTRICIDE) != DONE)
                            return false;
                        [[fallthrough]];
                    case DATA_PROFESSOR_PUTRICIDE:
                        if (GetBossState(DATA_FESTERGUT) != DONE || GetBossState(DATA_ROTFACE) != DONE)
                            return false;
                        break;
                    default:
                        break;
                }

                return true;
            }

            bool CheckCrimsonHalls(uint32 bossId) const
            {
                switch (bossId)
                {
                    case DATA_THE_LICH_KING:
                        if (GetBossState(DATA_BLOOD_QUEEN_LANA_THEL) != DONE)
                            return false;
                        [[fallthrough]];
                    case DATA_BLOOD_QUEEN_LANA_THEL:
                        if (GetBossState(DATA_BLOOD_PRINCE_COUNCIL) != DONE)
                            return false;
                        break;
                    default:
                        break;
                }

                return true;
            }

            bool CheckFrostwingHalls(uint32 bossId) const
            {
                switch (bossId)
                {
                    case DATA_THE_LICH_KING:
                        if (GetBossState(DATA_SINDRAGOSA) != DONE)
                            return false;
                        [[fallthrough]];
                    case DATA_SINDRAGOSA:
                        if (GetBossState(DATA_VALITHRIA_DREAMWALKER) != DONE)
                            return false;
                        if (GetData(DATA_SINDRAGOSA_GAUNTLET) != DONE)
                            return false;
                        break;
                    default:
                        break;
                }

                return true;
            }

            bool CheckLowerSpire(uint32 bossId) const
            {
                switch (bossId)
                {
                    case DATA_THE_LICH_KING:
                    case DATA_SINDRAGOSA:
                    case DATA_BLOOD_QUEEN_LANA_THEL:
                    case DATA_PROFESSOR_PUTRICIDE:
                    case DATA_VALITHRIA_DREAMWALKER:
                    case DATA_BLOOD_PRINCE_COUNCIL:
                    case DATA_ROTFACE:
                    case DATA_FESTERGUT:
                        if (GetBossState(DATA_DEATHBRINGER_SAURFANG) != DONE)
                            return false;
                        [[fallthrough]];
                    case DATA_DEATHBRINGER_SAURFANG:
                        if (GetBossState(DATA_ICECROWN_GUNSHIP_BATTLE) != DONE)
                            return false;
                        [[fallthrough]];
                    case DATA_ICECROWN_GUNSHIP_BATTLE:
                        if (GetBossState(DATA_LADY_DEATHWHISPER) != DONE)
                            return false;
                        [[fallthrough]];
                    case DATA_LADY_DEATHWHISPER:
                        if (GetBossState(DATA_LORD_MARROWGAR) != DONE)
                            return false;
                        [[fallthrough]];
                    case DATA_LORD_MARROWGAR:
                    default:
                        break;
                }

                return true;
            }

            void CheckLichKingAvailability()
            {
                if (GetBossState(DATA_PROFESSOR_PUTRICIDE) == DONE && GetBossState(DATA_BLOOD_QUEEN_LANA_THEL) == DONE && GetBossState(DATA_SINDRAGOSA) == DONE)
                {
                    if (GameObject* teleporter = instance->GetGameObject(TheLichKingTeleportGUID))
                    {
                        teleporter->SetGoState(GO_STATE_ACTIVE);

                        std::list<Creature*> stalkers;
                        teleporter->GetCreatureListWithEntryInGrid(stalkers, NPC_INVISIBLE_STALKER, 100.0f);
                        if (stalkers.empty())
                            return;

                        stalkers.sort(Trinity::ObjectDistanceOrderPred(teleporter));
                        stalkers.front()->CastSpell(nullptr, SPELL_ARTHAS_TELEPORTER_CEREMONY, false);
                        stalkers.pop_front();
                        for (std::list<Creature*>::iterator itr = stalkers.begin(); itr != stalkers.end(); ++itr)
                            (*itr)->AI()->Reset();
                    }
                }
            }

            void WriteSaveDataMore(std::ostringstream& data) override
            {
                data << HeroicAttempts << ' '
                    << ColdflameJetsState << ' '
                    << SindragosaGauntletState << ' '
                    << BloodQuickeningState << ' '
                    << BloodQuickeningMinutes << ' '
                    << UpperSpireTeleporterActiveState << ' '
                    << _putricideTrapState << ' '
                    << uint32(_putricideValveState);
            }

            void ReadSaveDataMore(std::istringstream& data) override
            {
                uint32 temp = 0;

                data >> HeroicAttempts;

                data >> temp;
                ColdflameJetsState = temp == DONE ? DONE : NOT_STARTED;

                data >> temp;
                SindragosaGauntletState = temp == DONE ? DONE : NOT_STARTED;

                data >> temp;
                BloodQuickeningState = temp == DONE ? DONE : NOT_STARTED;

                data >> BloodQuickeningMinutes;

                data >> temp;
                UpperSpireTeleporterActiveState = temp == DONE ? DONE : NOT_STARTED;

                if (data >> temp)
                    _putricideTrapState = temp == DONE ? DONE : NOT_STARTED;
                else
                    _putricideTrapState = NOT_STARTED;

                if (data >> temp)
                    _putricideValveState = static_cast<uint8>(temp & (PUTRICIDE_VALVE_FESTERGUT | PUTRICIDE_VALVE_ROTFACE));
                else
                    _putricideValveState = 0;
            }

            void Update(uint32 diff) override
            {
                // Reset Putricide's trap event only after the complete group has died.
                if (_putricideTrapState == IN_PROGRESS)
                {
                    bool anyPlayerAlive = false;
                    for (Map::PlayerList::const_iterator itr = instance->GetPlayers().begin();
                         itr != instance->GetPlayers().end(); ++itr)
                    {
                        Player* player = itr->GetSource();
                        if (player && player->IsAlive())
                        {
                            anyPlayerAlive = true;
                            break;
                        }
                    }

                    if (!anyPlayerAlive)
                        SetData(DATA_PUTRICIDE_TRAP_STATE, NOT_STARTED);
                }

                if (Events.Empty())
                    return;

                Events.Update(diff);

                while (uint32 eventId = Events.ExecuteEvent())
                {
                    switch (eventId)
                    {
                        case EVENT_UPDATE_EXECUTION_TIME:
                        {
                            --BloodQuickeningMinutes;
                            if (BloodQuickeningMinutes)
                            {
                                Events.ScheduleEvent(EVENT_UPDATE_EXECUTION_TIME, 1min);
                                DoUpdateWorldState(WORLDSTATE_SHOW_TIMER, 1);
                                DoUpdateWorldState(WORLDSTATE_EXECUTION_TIME, BloodQuickeningMinutes);
                            }
                            else
                            {
                                BloodQuickeningState = DONE;
                                DoUpdateWorldState(WORLDSTATE_SHOW_TIMER, 0);
                                if (Creature* bq = instance->GetCreature(BloodQueenLanaThelGUID))
                                    bq->AI()->DoAction(ACTION_KILL_MINCHAR);
                            }
                            SaveToDB();
                            break;
                        }
                        case EVENT_QUAKE_SHATTER:
                        {
                            if (GameObject* platform = instance->GetGameObject(ArthasPlatformGUID))
                                platform->SetDestructibleState(GO_DESTRUCTIBLE_DAMAGED);
                            if (GameObject* edge = instance->GetGameObject(FrozenThroneEdgeGUID))
                                edge->SetGoState(GO_STATE_ACTIVE);
                            if (GameObject* wind = instance->GetGameObject(FrozenThroneWindGUID))
                                wind->SetGoState(GO_STATE_READY);
                            if (GameObject* warning = instance->GetGameObject(FrozenThroneWarningGUID))
                                warning->SetGoState(GO_STATE_READY);
                            if (Creature* theLichKing = instance->GetCreature(TheLichKingGUID))
                                theLichKing->AI()->DoAction(ACTION_RESTORE_LIGHT);
                            break;
                        }
                        case EVENT_REBUILD_PLATFORM:
                            if (GameObject* platform = instance->GetGameObject(ArthasPlatformGUID))
                                platform->SetDestructibleState(GO_DESTRUCTIBLE_REBUILDING);
                            if (GameObject* edge = instance->GetGameObject(FrozenThroneEdgeGUID))
                                edge->SetGoState(GO_STATE_READY);
                            if (GameObject* wind = instance->GetGameObject(FrozenThroneWindGUID))
                                wind->SetGoState(GO_STATE_ACTIVE);
                            break;
                        case EVENT_RESPAWN_GUNSHIP:
                            SpawnGunship();
                            break;
                        case EVENT_SPAWN_SAURFANG_EVENT:
                            SpawnSaurfangEventNpcs();
                            break;
                        case EVENT_SAURFANG_OUTRO_TIMEOUT:
                            _saurfangOutroRunning = false;
                            SetData(DATA_SAURFANG_OUTRO_PORTAL, DONE);
                            SetData(DATA_SAURFANG_OUTRO_ZEPPELIN, DONE);
                            if (GetBossState(DATA_DEATHBRINGER_SAURFANG) == DONE)
                            {
                                SpawnSaurfangCamp(false);
                                SpawnSaurfangEventNpcs();
                            }
                            break;
                        case EVENT_SAURFANG_CAMP_TELEPORTERS:
                            SpawnSaurfangCampTeleporters();
                            break;
                        case EVENT_SAURFANG_CAMP_WORKERS:
                            SpawnSaurfangCampWorkers();
                            break;
                        case EVENT_SAURFANG_CAMP_TENTS:
                            SpawnSaurfangCampTents();
                            break;
                        case EVENT_SAURFANG_CAMP_WORKERS_OUT:
                            DespawnSaurfangCampWorkers();
                            break;
                        case EVENT_SAURFANG_CAMP_WORKERS_FIRST_POS:
                        {
                            uint8 i = 0;
                            for (ObjectGuid guid : SaurfangCampWorkerGUIDs)
                                if (Creature* builder = instance->GetCreature(guid))
                                {
                                    builder->SetWalk(false);
                                    builder->GetMotionMaster()->MovePoint(0, SaurfangWorkerFirstPos, false, SaurfangWorkerFirstPos.GetOrientation());
                                    builder->SetEmoteState(EMOTE_ONESHOT_NONE);
                                    ++i;
                                }
                            break;
                        }
                        case EVENT_SAURFANG_CAMP_WORKERS_RE_POS:
                        {
                            bool const horde = TeamInInstance == HORDE;
                            uint8 i = 0;
                            for (ObjectGuid guid : SaurfangCampWorkerGUIDs)
                                if (Creature* builder = instance->GetCreature(guid))
                                {
                                    builder->SetWalk(false);
                                    Position pos = horde ? SaurfangCampTentPosH[i] : SaurfangCampTentPosA[i];
                                    builder->GetMotionMaster()->MovePoint(0, pos, false, pos.GetOrientation());
                                    builder->SetEmoteState(EMOTE_STATE_WORK_MINING);
                                    ++i;
                                }
                            break;
                        }
                        case EVENT_SAURFANG_CAMP_WORKERS_BACK:
                            SendSaurfangCampWorkersBack();
                            break;
                        case EVENT_SAURFANG_CAMP_SMITH_ARRIVE:
                            if (Creature* smith = instance->GetCreature(SaurfangCampSmithGUID))
                            {
                                Position pos = TeamInInstance == HORDE ? SaurfangCampBlacksmithPos : SaurfangCampBlacksmithPosA;
                                smith->GetMotionMaster()->MovePoint(0, pos, false, pos.GetOrientation());
                            }
                            break;
                        case EVENT_SAURFANG_CAMP_VENDOR_ARRIVE:
                            if (Creature* goods = instance->GetCreature(SaurfangCampGoodsGUID))
                                goods->GetMotionMaster()->MovePoint(0, SaurfangCampGeneralGoodsPosA, false, SaurfangCampGeneralGoodsPosA.GetOrientation());
                            break;
                        case EVENT_SAURFANG_CAMP_VENDORS:
                            SpawnSaurfangCampVendors();
                            break;
                        case EVENT_SAURFANG_ZEPPELIN_REMOVE:
                            if (GameObject* go = instance->GetGameObject(SaurfangZeppelinGUID))
                            {
                                // A MO_TRANSPORT is not despawned like an ordinary gameobject.
                                if (Transport* zeppelin = go->ToTransport())
                                {
                                    zeppelin->EnableMovement(false);
                                    zeppelin->UnloadStaticPassengers();
                                }

                                go->AddObjectToRemoveList();
                            }
                            _saurfangZeppelinLeaving = false;
                            SaurfangZeppelinGUID.Clear();
                            break;
                        case EVENT_SAURFANG_ZEPPELIN_DOCK:
                        {
                            // Reschedule unconditionally: giving up on a failed lookup would leave
                            // it looping its path forever.
                            GameObject* go = instance->GetGameObject(SaurfangZeppelinGUID);
                            Transport* zeppelin = go ? go->ToTransport() : nullptr;
                            if (zeppelin && zeppelin->GetExactDist2d(&SaurfangOutroZeppelinPos) <= SaurfangOutroZeppelinDockRange)
                            {
                                zeppelin->EnableMovement(false);
                                _saurfangZeppelinDocked = true;
                            }
                            else if (SaurfangZeppelinGUID != ObjectGuid::Empty)
                                Events.ScheduleEvent(EVENT_SAURFANG_ZEPPELIN_DOCK, 500ms);
                        }
                        break;
                        case EVENT_SAURFANG_NPCS_RESET:
                            RestoreHiddenSaurfangEventNpcs();
                            break;
                        case EVENT_SAURFANG_CAMP_RESET:
                            SpawnSaurfangCamp(false);
                            break;
                        default:
                            break;
                    }
                }
            }

            void ProcessEvent(WorldObject* source, uint32 eventId) override
            {
                switch (eventId)
                {
                    case EVENT_ENEMY_GUNSHIP_DESPAWN:
                        if (GetBossState(DATA_ICECROWN_GUNSHIP_BATTLE) == DONE)
                            source->AddObjectToRemoveList();
                        break;
                    case EVENT_ENEMY_GUNSHIP_COMBAT:
                        if (Creature* captain = source->FindNearestCreature(TeamInInstance == HORDE ? NPC_IGB_HIGH_OVERLORD_SAURFANG : NPC_IGB_MURADIN_BRONZEBEARD, 100.0f))
                            captain->AI()->DoAction(ACTION_ENEMY_GUNSHIP_TALK);
                        [[fallthrough]];
                    case EVENT_PLAYERS_GUNSHIP_SPAWN:
                    case EVENT_PLAYERS_GUNSHIP_COMBAT:
                        if (GameObject* go = source->ToGameObject())
                            if (Transport* transport = go->ToTransport())
                                transport->EnableMovement(false);
                        break;
                    case EVENT_PLAYERS_GUNSHIP_SAURFANG:
                        if (Creature* captain = source->FindNearestCreature(TeamInInstance == HORDE ? NPC_IGB_HIGH_OVERLORD_SAURFANG : NPC_IGB_MURADIN_BRONZEBEARD, 100.0f))
                            captain->AI()->DoAction(ACTION_EXIT_SHIP);
                        if (GameObject* go = source->ToGameObject())
                            if (Transport* transport = go->ToTransport())
                                transport->EnableMovement(false);
                        // The captain despawns 18s into his walk off the ship; the event party appears with him.
                        Events.ScheduleEvent(EVENT_SPAWN_SAURFANG_EVENT, 18s);
                        break;
                    case EVENT_QUAKE:
                        if (GameObject* warning = instance->GetGameObject(FrozenThroneWarningGUID))
                            warning->SetGoState(GO_STATE_ACTIVE);
                        Events.ScheduleEvent(EVENT_QUAKE_SHATTER, 5s);
                        break;
                    case EVENT_SECOND_REMORSELESS_WINTER:
                        if (GameObject* platform = instance->GetGameObject(ArthasPlatformGUID))
                        {
                            platform->SetDestructibleState(GO_DESTRUCTIBLE_DESTROYED);
                            Events.ScheduleEvent(EVENT_REBUILD_PLATFORM, 1500ms);
                        }
                        break;
                    case EVENT_FESTERGUT_VALVE_USED:
                        if (!(_putricideValveState & PUTRICIDE_VALVE_FESTERGUT))
                        {
                            _putricideValveState |= PUTRICIDE_VALVE_FESTERGUT;
                            if ((_putricideValveState & (PUTRICIDE_VALVE_FESTERGUT | PUTRICIDE_VALVE_ROTFACE)) == (PUTRICIDE_VALVE_FESTERGUT | PUTRICIDE_VALVE_ROTFACE))
                            {
                                HandleGameObject(PutricideCollisionGUID, true);
                                if (GameObject* go = instance->GetGameObject(PutricideGateGUIDs[0]))
                                    go->SetGoState(static_cast<GOState>(2));
                                if (GameObject* go = instance->GetGameObject(PutricideGateGUIDs[1]))
                                    go->SetGoState(static_cast<GOState>(2));
                            }
                            else
                                HandleGameObject(PutricideGateGUIDs[0], false);

                            HandleGameObject(PutricidePipeGUIDs[0], true);
                            SaveToDB();
                        }
                        break;
                    case EVENT_ROTFACE_VALVE_USED:
                        if (!(_putricideValveState & PUTRICIDE_VALVE_ROTFACE))
                        {
                            _putricideValveState |= PUTRICIDE_VALVE_ROTFACE;
                            if ((_putricideValveState & (PUTRICIDE_VALVE_FESTERGUT | PUTRICIDE_VALVE_ROTFACE)) == (PUTRICIDE_VALVE_FESTERGUT | PUTRICIDE_VALVE_ROTFACE))
                            {
                                HandleGameObject(PutricideCollisionGUID, true);
                                if (GameObject* go = instance->GetGameObject(PutricideGateGUIDs[0]))
                                    go->SetGoState(static_cast<GOState>(2));
                                if (GameObject* go = instance->GetGameObject(PutricideGateGUIDs[1]))
                                    go->SetGoState(static_cast<GOState>(2));
                            }
                            else
                                HandleGameObject(PutricideGateGUIDs[1], false);

                            HandleGameObject(PutricidePipeGUIDs[1], true);
                            SaveToDB();
                        }
                        break;
                    case EVENT_TELEPORT_TO_FROSTMOURNE: // Harvest Soul (normal mode)
                        if (Creature* terenas = instance->SummonCreature(NPC_TERENAS_MENETHIL_FROSTMOURNE, TerenasSpawn, nullptr, 63000))
                        {
                            terenas->AI()->DoAction(ACTION_FROSTMOURNE_INTRO);
                            std::list<Creature*> triggers;
                            terenas->GetCreatureListWithEntryInGrid(triggers, NPC_WORLD_TRIGGER_INFINITE_AOI, 100.0f);
                            if (!triggers.empty())
                            {
                                triggers.sort(Trinity::ObjectDistanceOrderPred(terenas, false));
                                Unit* visual = triggers.front();
                                visual->CastSpell(visual, SPELL_FROSTMOURNE_TELEPORT_VISUAL, true);
                            }

                            if (Creature* warden = instance->SummonCreature(NPC_SPIRIT_WARDEN, SpiritWardenSpawn, nullptr, 63000))
                            {
                                terenas->AI()->AttackStart(warden);
                                warden->GetThreatManager().AddThreat(terenas, 300000.0f, nullptr, true, true);
                            }
                        }
                        break;
                    default:
                        break;
                }
            }

        protected:
            EventMap Events;
            ObjectGuid LordMarrowgarGUID;
            ObjectGuid LadyDeahtwhisperGUID;
            ObjectGuid LadyDeathwisperElevatorGUID;
            ObjectGuid GunshipGUID;
            ObjectGuid EnemyGunshipGUID;
            ObjectGuid GunshipArmoryGUID;
            ObjectGuid DeathbringerSaurfangGUID;
            ObjectGuid DeathbringerSaurfangDoorGUID;
            ObjectGuid DeathbringerSaurfangEventGUID;   // Muradin Bronzebeard or High Overlord Saurfang
            ObjectGuid DeathbringersCacheGUID;
            GuidList SaurfangCampGUIDs;
            GuidList SaurfangEventGuardGUIDs;
            GuidList SaurfangCampWorkerGUIDs;
            ObjectGuid SaurfangCampSmithGUID;
            ObjectGuid SaurfangCampGoodsGUID;
            ObjectGuid SaurfangZeppelinGUID;
            ObjectGuid SaurfangPortalGUID;
            bool _saurfangCampSpawned;
            bool _saurfangOutroRunning;
            bool _saurfangZeppelinDocked;
            bool _saurfangZeppelinLeaving;
            ObjectGuid TeleporterLichKingGUID;
            ObjectGuid TeleporterUpperSpireGUID;
            ObjectGuid TeleporterLightsHammerGUID;
            ObjectGuid TeleporterRampartsGUID;
            ObjectGuid TeleporterDeathBringerGUID;
            ObjectGuid TeleporterOratoryGUID;
            ObjectGuid TeleporterSindragosaGUID;
            ObjectGuid PlagueSigilGUID;
            ObjectGuid BloodwingSigilGUID;
            ObjectGuid FrostwingSigilGUID;
            ObjectGuid PutricidePipeGUIDs[2];
            ObjectGuid PutricideGateGUIDs[2];
            ObjectGuid GasReleaseValveGUID;
            ObjectGuid OozeReleaseValveGUID;
            ObjectGuid PutricideCollisionGUID;
            ObjectGuid FestergutGUID;
            ObjectGuid RotfaceGUID;
            ObjectGuid ProfessorPutricideGUID;
            ObjectGuid PutricadesTrapGUID;
            ObjectGuid PutricideTableGUID;
            ObjectGuid BloodCouncilGUIDs[3];
            ObjectGuid BloodCouncilControllerGUID;
            ObjectGuid BloodQueenLanaThelCouncilGUID;
            ObjectGuid BloodQueenLanaThelGUID;
            ObjectGuid CrokScourgebaneGUID;
            ObjectGuid CrokCaptainGUIDs[4];
            ObjectGuid SisterSvalnaGUID;
            ObjectGuid ValithriaDreamwalkerGUID;
            ObjectGuid ValithriaLichKingGUID;
            ObjectGuid ValithriaTriggerGUID;
            ObjectGuid SindragosaGauntletGUID;
            ObjectGuid SindragosaEntranceDoorGUID;
            ObjectGuid SindragosaGUID;
            ObjectGuid SpinestalkerGUID;
            ObjectGuid RimefangGUID;
            ObjectGuid TheLichKingTeleportGUID;
            ObjectGuid TheLichKingGUID;
            ObjectGuid HighlordTirionFordringGUID;
            ObjectGuid TerenasMenethilGUID;
            ObjectGuid ArthasPlatformGUID;
            ObjectGuid ArthasPrecipiceGUID;
            ObjectGuid FrozenThroneEdgeGUID;
            ObjectGuid FrozenThroneWindGUID;
            ObjectGuid FrozenThroneWarningGUID;
            ObjectGuid FrozenBolvarGUID;
            ObjectGuid PillarsChainedGUID;
            ObjectGuid PillarsUnchainedGUID;
            Team TeamInInstance;
            uint32 ColdflameJetsState;
            uint32 UpperSpireTeleporterActiveState;
            std::unordered_set<ObjectGuid::LowType> FrostwyrmGUIDs;
            std::unordered_set<ObjectGuid::LowType> SpinestalkerTrash;
            std::unordered_set<ObjectGuid::LowType> RimefangTrash;
            uint32 BloodQuickeningState;
            uint32 HeroicAttempts;
            uint16 BloodQuickeningMinutes;
            uint8 BloodPrinceIntro;
            uint32 SindragosaGauntletState;
            uint8 SindragosaIntro;
            bool IsBonedEligible;
            bool IsOozeDanceEligible;
            bool IsNauseaEligible;
            bool IsOrbWhispererEligible;
            bool IsFactionBuffActive;
            std::array<GuidVector, 2> nerubarBroodkeepersGUIDs;

        private:
            uint32 _putricideTrapState;
            uint8 _putricideValveState;
            ObjectGuid _putricideEntranceDoorGUID;
        };

        InstanceScript* GetInstanceScript(InstanceMap* map) const override
        {
            return new instance_icecrown_citadel_InstanceMapScript(map);
        }
};

void AddSC_instance_icecrown_citadel()
{
    new instance_icecrown_citadel();
}
