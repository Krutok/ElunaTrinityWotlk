
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



#include "CellImpl.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "Config.h"
#include "Containers.h"
#include "CreatureAIImpl.h"
#include "GameEventMgr.h"
#include "GameObjectAI.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "Group.h"
#include "Language.h"
#include "LFGMgr.h"
#include "PassiveAI.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "TaskScheduler.h"
#include "Util.h"
#include "World.h"
#include <sstream>
#include <iomanip>
#include "WorldSession.h"
#include "Item.h"

using namespace Trinity::ChatCommands;

static uint32 BrewfestStartTimer = 1;

enum RamBlaBla
{
    SPELL_GIDDYUP                           = 42924,
    SPELL_RENTAL_RACING_RAM                 = 43883,
    SPELL_SWIFT_WORK_RAM                    = 43880,
    SPELL_RENTAL_RACING_RAM_AURA            = 42146,
    SPELL_RAM_LEVEL_NEUTRAL                 = 43310,
    SPELL_RAM_TROT                          = 42992,
    SPELL_RAM_CANTER                        = 42993,
    SPELL_RAM_GALLOP                        = 42994,
    SPELL_RAM_FATIGUE                       = 43052,
    SPELL_EXHAUSTED_RAM                     = 43332,
    SPELL_RELAY_RACE_TURN_IN                = 44501,

    // Quest
    SPELL_BREWFEST_QUEST_SPEED_BUNNY_GREEN  = 43345,
    SPELL_BREWFEST_QUEST_SPEED_BUNNY_YELLOW = 43346,
    SPELL_BREWFEST_QUEST_SPEED_BUNNY_RED    = 43347
};

// 42924 - Giddyup!
class spell_brewfest_giddyup : public AuraScript
{
    PrepareAuraScript(spell_brewfest_giddyup);

    void OnChange(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (!target->HasAura(SPELL_RENTAL_RACING_RAM) && !target->HasAura(SPELL_SWIFT_WORK_RAM))
        {
            target->RemoveAura(GetId());
            return;
        }

        if (target->HasAura(SPELL_EXHAUSTED_RAM))
            return;

        switch (GetStackAmount())
        {
        case 1: // green
            target->RemoveAura(SPELL_RAM_LEVEL_NEUTRAL);
            target->RemoveAura(SPELL_RAM_CANTER);
            target->CastSpell(target, SPELL_RAM_TROT, true);
            break;
        case 6: // yellow
            target->RemoveAura(SPELL_RAM_TROT);
            target->RemoveAura(SPELL_RAM_GALLOP);
            target->CastSpell(target, SPELL_RAM_CANTER, true);
            break;
        case 11: // red
            target->RemoveAura(SPELL_RAM_CANTER);
            target->CastSpell(target, SPELL_RAM_GALLOP, true);
            break;
        default:
            break;
        }

        if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_DEFAULT)
        {
            target->RemoveAura(SPELL_RAM_TROT);
            target->CastSpell(target, SPELL_RAM_LEVEL_NEUTRAL, true);
        }
    }

    void OnPeriodic(AuraEffect const* /*aurEff*/)
    {
        GetTarget()->RemoveAuraFromStack(GetId());
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_brewfest_giddyup::OnChange, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK);
        OnEffectRemove += AuraEffectRemoveFn(spell_brewfest_giddyup::OnChange, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_CHANGE_AMOUNT_MASK);
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_brewfest_giddyup::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

// 43714 - Brewfest - Relay Race - Intro - Force - Player to throw- DND
class spell_brewfest_relay_race_intro_force_player_to_throw : public SpellScript
{
    PrepareSpellScript(spell_brewfest_relay_race_intro_force_player_to_throw);

    void HandleForceCast(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        // All this spells trigger a spell that requires reagents; if the
        // triggered spell is cast as "triggered", reagents are not consumed
        GetHitUnit()->CastSpell(nullptr, GetEffectInfo().TriggerSpell, TriggerCastFlags(TRIGGERED_FULL_MASK & ~TRIGGERED_IGNORE_POWER_AND_REAGENT_COST));
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_brewfest_relay_race_intro_force_player_to_throw::HandleForceCast, EFFECT_0, SPELL_EFFECT_FORCE_CAST);
    }
};

// 43755 - Brewfest - Daily - Relay Race - Player - Increase Mount Duration - DND
class spell_brewfest_relay_race_turn_in : public SpellScript
{
    PrepareSpellScript(spell_brewfest_relay_race_turn_in);

    void HandleDummy(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);

        if (Aura* aura = GetHitUnit()->GetAura(SPELL_SWIFT_WORK_RAM))
        {
            aura->SetDuration(aura->GetDuration() + 30 * IN_MILLISECONDS);
            GetCaster()->CastSpell(GetHitUnit(), SPELL_RELAY_RACE_TURN_IN, TRIGGERED_FULL_MASK);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_brewfest_relay_race_turn_in::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 43876 - Dismount Ram
class spell_brewfest_dismount_ram : public SpellScript
{
    PrepareSpellScript(spell_brewfest_dismount_ram);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->RemoveAura(SPELL_RENTAL_RACING_RAM);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_brewfest_dismount_ram::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

enum RamBlub
{
    // Horde
    QUEST_BARK_FOR_DROHNS_DISTILLERY        = 11407,
    QUEST_BARK_FOR_TCHALIS_VOODOO_BREWERY   = 11408,

    // Alliance
    QUEST_BARK_BARLEYBREW                   = 11293,
    QUEST_BARK_FOR_THUNDERBREWS             = 11294,

    // Bark for Drohn's Distillery!
    SAY_DROHN_DISTILLERY_1                  = 23520,
    SAY_DROHN_DISTILLERY_2                  = 23521,
    SAY_DROHN_DISTILLERY_3                  = 23522,
    SAY_DROHN_DISTILLERY_4                  = 23523,

    // Bark for T'chali's Voodoo Brewery!
    SAY_TCHALIS_VOODOO_1                    = 23524,
    SAY_TCHALIS_VOODOO_2                    = 23525,
    SAY_TCHALIS_VOODOO_3                    = 23526,
    SAY_TCHALIS_VOODOO_4                    = 23527,

    // Bark for the Barleybrews!
    SAY_BARLEYBREW_1                        = 23464,
    SAY_BARLEYBREW_2                        = 23465,
    SAY_BARLEYBREW_3                        = 23466,
    SAY_BARLEYBREW_4                        = 22941,

    // Bark for the Thunderbrews!
    SAY_THUNDERBREWS_1                      = 23467,
    SAY_THUNDERBREWS_2                      = 23468,
    SAY_THUNDERBREWS_3                      = 23469,
    SAY_THUNDERBREWS_4                      = 22942
};

// 43259 Brewfest  - Barker Bunny 1
// 43260 Brewfest  - Barker Bunny 2
// 43261 Brewfest  - Barker Bunny 3
// 43262 Brewfest  - Barker Bunny 4
class spell_brewfest_barker_bunny : public AuraScript
{
    PrepareAuraScript(spell_brewfest_barker_bunny);

    bool Load() override
    {
        return GetUnitOwner()->GetTypeId() == TYPEID_PLAYER;
    }

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Player* target = GetTarget()->ToPlayer();

        uint32 BroadcastTextId = 0;

        if (target->GetQuestStatus(QUEST_BARK_FOR_DROHNS_DISTILLERY) == QUEST_STATUS_INCOMPLETE ||
            target->GetQuestStatus(QUEST_BARK_FOR_DROHNS_DISTILLERY) == QUEST_STATUS_COMPLETE)
            BroadcastTextId = RAND(SAY_DROHN_DISTILLERY_1, SAY_DROHN_DISTILLERY_2, SAY_DROHN_DISTILLERY_3, SAY_DROHN_DISTILLERY_4);

        if (target->GetQuestStatus(QUEST_BARK_FOR_TCHALIS_VOODOO_BREWERY) == QUEST_STATUS_INCOMPLETE ||
            target->GetQuestStatus(QUEST_BARK_FOR_TCHALIS_VOODOO_BREWERY) == QUEST_STATUS_COMPLETE)
            BroadcastTextId = RAND(SAY_TCHALIS_VOODOO_1, SAY_TCHALIS_VOODOO_2, SAY_TCHALIS_VOODOO_3, SAY_TCHALIS_VOODOO_4);

        if (target->GetQuestStatus(QUEST_BARK_BARLEYBREW) == QUEST_STATUS_INCOMPLETE ||
            target->GetQuestStatus(QUEST_BARK_BARLEYBREW) == QUEST_STATUS_COMPLETE)
            BroadcastTextId = RAND(SAY_BARLEYBREW_1, SAY_BARLEYBREW_2, SAY_BARLEYBREW_3, SAY_BARLEYBREW_4);

        if (target->GetQuestStatus(QUEST_BARK_FOR_THUNDERBREWS) == QUEST_STATUS_INCOMPLETE ||
            target->GetQuestStatus(QUEST_BARK_FOR_THUNDERBREWS) == QUEST_STATUS_COMPLETE)
            BroadcastTextId = RAND(SAY_THUNDERBREWS_1, SAY_THUNDERBREWS_2, SAY_THUNDERBREWS_3, SAY_THUNDERBREWS_4);

        if (BroadcastTextId)
            target->Talk(BroadcastTextId, CHAT_MSG_SAY, sWorld->getFloatConfig(CONFIG_LISTEN_RANGE_SAY), target);
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_brewfest_barker_bunny::OnApply, EFFECT_1, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

enum BrewfestMountTransformation
{
    SPELL_MOUNT_RAM_100                         = 43900,
    SPELL_MOUNT_RAM_60                          = 43899,
    SPELL_MOUNT_KODO_100                        = 49379,
    SPELL_MOUNT_KODO_60                         = 49378,
    SPELL_BREWFEST_MOUNT_TRANSFORM              = 49357,
    SPELL_BREWFEST_MOUNT_TRANSFORM_REVERSE      = 52845,
};

// 49357 - Brewfest Mount Transformation
// 52845 - Brewfest Mount Transformation (Faction Swap)
class spell_brewfest_mount_transformation : public SpellScript
{
    PrepareSpellScript(spell_brewfest_mount_transformation);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo(
            {
                SPELL_MOUNT_RAM_100,
                SPELL_MOUNT_RAM_60,
                SPELL_MOUNT_KODO_100,
                SPELL_MOUNT_KODO_60
            });
    }

    void HandleDummy(SpellEffIndex /* effIndex */)
    {
        Player* caster = GetCaster()->ToPlayer();
        if (caster->HasAuraType(SPELL_AURA_MOUNTED))
        {
            caster->RemoveAurasByType(SPELL_AURA_MOUNTED);
            uint32 spell_id;

            switch (GetSpellInfo()->Id)
            {
            case SPELL_BREWFEST_MOUNT_TRANSFORM:
                if (caster->GetSpeedRate(MOVE_RUN) >= 2.0f)
                    spell_id = caster->GetTeam() == ALLIANCE ? SPELL_MOUNT_RAM_100 : SPELL_MOUNT_KODO_100;
                else
                    spell_id = caster->GetTeam() == ALLIANCE ? SPELL_MOUNT_RAM_60 : SPELL_MOUNT_KODO_60;
                break;
            case SPELL_BREWFEST_MOUNT_TRANSFORM_REVERSE:
                if (caster->GetSpeedRate(MOVE_RUN) >= 2.0f)
                    spell_id = caster->GetTeam() == HORDE ? SPELL_MOUNT_RAM_100 : SPELL_MOUNT_KODO_100;
                else
                    spell_id = caster->GetTeam() == HORDE ? SPELL_MOUNT_RAM_60 : SPELL_MOUNT_KODO_60;
                break;
            default:
                return;
            }
            caster->CastSpell(caster, spell_id, true);
        }
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_brewfest_mount_transformation::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

/*
    Brew of the Month
 January   [Wild Winter Pilsner]
    spell_brewfest_botm_the_beast_within
 February  [Izzard's Ever Flavor]
    spell_brewfest_botm_gassy
 March     [Aromatic Honey Brew]
    Nothing to script here
 April     [Metok's Bubble Bock]
    spell_brewfest_botm_bloated
    Incomplete (spells 49828, 49827, 49830, 49837)
 May       [Springtime Stout]
    Nothing to script here
 June      [Blackrock Lager]
    spell_brewfest_botm_internal_combustion
 July      [Stranglethorn Brew]
    spell_brewfest_botm_jungle_madness
 August    [Draenic Pale Ale]
    spell_brewfest_botm_pink_elekk
 September [Binary Brew]
    spell_brewfest_botm_teach_language
 October   [Autumnal Acorn Ale]
    Nothing to script here
 November  [Bartlett's Bitter Brew]
    spell_brewfest_botm_nauseous
    Incomplete
 December  [Lord of Frost's Private Label]
    Nothing to script here
*/

enum WildWinterPilsner
{
    SPELL_BOTM_UNLEASH_THE_BEAST    = 50099
};

// 50098 - The Beast Within
class spell_brewfest_botm_the_beast_within : public AuraScript
{
    PrepareAuraScript(spell_brewfest_botm_the_beast_within);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BOTM_UNLEASH_THE_BEAST });
    }

    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_BOTM_UNLEASH_THE_BEAST);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_brewfest_botm_the_beast_within::AfterRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

enum IzzardsEverFlavor
{
    SPELL_BOTM_BELCH_BREW_BELCH_VISUAL    = 49860
};

// 49864 - Gassy
class spell_brewfest_botm_gassy : public AuraScript
{
    PrepareAuraScript(spell_brewfest_botm_gassy);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BOTM_BELCH_BREW_BELCH_VISUAL });
    }

    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_BOTM_BELCH_BREW_BELCH_VISUAL, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_brewfest_botm_gassy::AfterRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

enum MetoksBubbleBock
{
    SPELL_BOTM_BUBBLE_BREW_TRIGGER_MISSILE    = 50072
};

// 49822 - Bloated
class spell_brewfest_botm_bloated : public AuraScript
{
    PrepareAuraScript(spell_brewfest_botm_bloated);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BOTM_BUBBLE_BREW_TRIGGER_MISSILE });
    }

    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_BOTM_BUBBLE_BREW_TRIGGER_MISSILE, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_brewfest_botm_bloated::AfterRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

enum BlackrockLager
{
    SPELL_BOTM_BELCH_FIRE_VISUAL    = 49737
};

// 49738 - Internal Combustion
class spell_brewfest_botm_internal_combustion : public AuraScript
{
    PrepareAuraScript(spell_brewfest_botm_internal_combustion);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BOTM_BELCH_FIRE_VISUAL });
    }

    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_BOTM_BELCH_FIRE_VISUAL, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_brewfest_botm_internal_combustion::AfterRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

enum StranglethornBrew
{
    SPELL_BOTM_JUNGLE_BREW_VISION_EFFECT    = 50010
};

// 49962 - Jungle Madness!
class spell_brewfest_botm_jungle_madness : public SpellScript
{
    PrepareSpellScript(spell_brewfest_botm_jungle_madness);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BOTM_JUNGLE_BREW_VISION_EFFECT });
    }

    void HandleAfterCast()
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_BOTM_JUNGLE_BREW_VISION_EFFECT, true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_brewfest_botm_jungle_madness::HandleAfterCast);
    }
};

enum BinaryBrew
{
    SPELL_LEARN_GNOMISH_BINARY      = 50242,
    SPELL_LEARN_GOBLIN_BINARY       = 50246
};

// 50243 - Teach Language
class spell_brewfest_botm_teach_language : public SpellScript
{
    PrepareSpellScript(spell_brewfest_botm_teach_language);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_LEARN_GNOMISH_BINARY, SPELL_LEARN_GOBLIN_BINARY });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Player* caster = GetCaster()->ToPlayer())
            caster->CastSpell(caster, caster->GetTeam() == ALLIANCE ? SPELL_LEARN_GNOMISH_BINARY : SPELL_LEARN_GOBLIN_BINARY, true);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_brewfest_botm_teach_language::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

enum CreateEmptyBrewBottle
{
    SPELL_BOTM_CREATE_EMPTY_BREW_BOTTLE    = 51655
};

// 42254, 42255, 42256, 42257, 42258, 42259, 42260, 42261, 42263, 42264, 43959, 43961 - Weak Alcohol
class spell_brewfest_botm_weak_alcohol : public SpellScript
{
    PrepareSpellScript(spell_brewfest_botm_weak_alcohol);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BOTM_CREATE_EMPTY_BREW_BOTTLE });
    }

    void HandleAfterCast()
    {
        GetCaster()->CastSpell(GetCaster(), SPELL_BOTM_CREATE_EMPTY_BREW_BOTTLE, true);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_brewfest_botm_weak_alcohol::HandleAfterCast);
    }
};

enum EmptyBottleThrow
{
    SPELL_BOTM_EMPTY_BOTTLE_THROW_IMPACT_CREATURE    = 51695,   // Just unit, not creature
    SPELL_BOTM_EMPTY_BOTTLE_THROW_IMPACT_GROUND      = 51697
};

// 51694 - BOTM - Empty Bottle Throw - Resolve
class spell_brewfest_botm_empty_bottle_throw_resolve : public SpellScript
{
    PrepareSpellScript(spell_brewfest_botm_empty_bottle_throw_resolve);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
            {
                SPELL_BOTM_EMPTY_BOTTLE_THROW_IMPACT_CREATURE,
                SPELL_BOTM_EMPTY_BOTTLE_THROW_IMPACT_GROUND
            });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();

        if (Unit* target = GetHitUnit())
            caster->CastSpell(target, SPELL_BOTM_EMPTY_BOTTLE_THROW_IMPACT_CREATURE, true);
        else
            caster->CastSpell(GetHitDest()->GetPosition(), SPELL_BOTM_EMPTY_BOTTLE_THROW_IMPACT_GROUND, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_brewfest_botm_empty_bottle_throw_resolve::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

enum MoleMachine
{
    SPELL_PORT_TO_GRIM_GUZZLER     = 47523
};

// 49466 - Mole Machine Portal Schedule
class spell_brewfest_mole_machine_portal_schedule : public SpellScript
{
    PrepareSpellScript(spell_brewfest_mole_machine_portal_schedule);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_PORT_TO_GRIM_GUZZLER });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        GetHitUnit()->CastSpell(GetHitUnit(), SPELL_PORT_TO_GRIM_GUZZLER);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_brewfest_mole_machine_portal_schedule::HandleScript, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

///////////////////////////////////////
////// GOS
///////////////////////////////////////

///////////////////////////////////////
////// NPCS
///////////////////////////////////////
enum kegThrowers
{
    QUEST_THERE_AND_BACK_AGAIN_A                = 11122,
    QUEST_THERE_AND_BACK_AGAIN_H                = 11412,
    RAM_DISPLAY_ID                              = 22630,
    NPC_FLYNN_FIREBREW                          = 24364,
    NPC_BOK_DROPCERTAIN                         = 24527,
    ITEM_PORTABLE_BREWFEST_KEG                  = 33797,
    SPELL_THROW_KEG                             = 43660,
    SPELL_THROW_KEG_PLAYER                      = 43663,
    SPELL_RAM_AURA                              = 43883,
    SPELL_DETECT_MARKER                         = 44069,
    SPELL_ADD_TOKENS                            = 44501,
    SPELL_RAM_RACING_CROP                       = 44262,
    SPELL_COOLDOWN_CHECKER                      = 44689,
    NPC_RAM_MASTER_RAY                          = 24497,
    NPC_NEILL_RAMSTEIN                          = 23558,
    KEG_KILL_CREDIT                             = 24337,
    GOSSIP_NEIL                                 = 8953,
    GOSSIP_RAY                                  = 8973
};

class spell_ram_aura_handler : public SpellScriptLoader
{
public:
    spell_ram_aura_handler() : SpellScriptLoader("spell_ram_aura_handler") {}

    class spell_ram_aura_handler_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_ram_aura_handler_AuraScript);

        void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            if (GetId() != SPELL_RAM_AURA)
                return;

            if (Unit* target = GetTarget())
                target->CastSpell(target, SPELL_DETECT_MARKER, true);
        }

        void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
        {
            if (GetId() != SPELL_RAM_AURA)
                return;

            if (Unit* target = GetTarget())
                target->RemoveAura(SPELL_DETECT_MARKER);
        }

        void Register() override
        {
            OnEffectApply += AuraEffectApplyFn(spell_ram_aura_handler_AuraScript::OnApply, EFFECT_0, SPELL_AURA_MOUNTED, AURA_EFFECT_HANDLE_REAL);
            OnEffectRemove += AuraEffectRemoveFn(spell_ram_aura_handler_AuraScript::OnRemove, EFFECT_0, SPELL_AURA_MOUNTED, AURA_EFFECT_HANDLE_REAL);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_ram_aura_handler_AuraScript();
    }
};

struct npc_brewfest_keg_thrower : public ScriptedAI
{
    npc_brewfest_keg_thrower(Creature* creature) : ScriptedAI(creature)
    {
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (me->GetDistance(who) < 10.0f && who->IsPlayer() && who->GetMountDisplayId() == RAM_DISPLAY_ID)
        {
            if (!who->ToPlayer()->HasItemCount(ITEM_PORTABLE_BREWFEST_KEG)) // portable brewfest keg
                me->CastSpell(who, SPELL_THROW_KEG, true);          // throw keg
        }
    }

    bool CanAlwaysSee(WorldObject const* obj) const
    {
        if (obj->IsPlayer() && obj->ToPlayer()->GetMountDisplayId() == RAM_DISPLAY_ID)
            return true;

        return false;
    }
};

struct npc_brewfest_keg_reciver : public ScriptedAI
{
    npc_brewfest_keg_reciver(Creature* creature) : ScriptedAI(creature)
    {
        spellInfo = sSpellMgr->GetSpellInfo(SPELL_COOLDOWN_CHECKER);
    }

    void MoveInLineOfSight(Unit* who) override
{
    if (!spellInfo)
        return;

    if (!who->IsPlayer())
        return;

    Player* player = who->ToPlayer();

    if (me->GetDistance(player) >= 10.0f)
        return;

    // Queststatus pr fen
    bool hasQuest =
        player->GetQuestStatus(QUEST_THERE_AND_BACK_AGAIN_A) == QUEST_STATUS_INCOMPLETE ||
        player->GetQuestStatus(QUEST_THERE_AND_BACK_AGAIN_H) == QUEST_STATUS_INCOMPLETE;

    if (!hasQuest)
        return;

    // Aura pr fen
    if (!player->HasAura(SPELL_RAM_AURA))
        return;

    // Item pr fen
    if (!player->HasItemCount(ITEM_PORTABLE_BREWFEST_KEG, 1))
        return;

    // Cooldown pr fen
    if (player->GetSpellHistory()->HasCooldown(SPELL_THROW_KEG_PLAYER))
        return;

	// Spell casten (triggered=false)
    player->CastSpell(player, SPELL_THROW_KEG_PLAYER, false);

    // Cooldown setzen
    player->GetSpellHistory()->AddCooldown(SPELL_THROW_KEG_PLAYER, 0, std::chrono::milliseconds(10000));

}

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        const uint32 sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);
        const uint32 action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);

        if (sender == GOSSIP_NEIL || sender == GOSSIP_RAY)
        {
            player->CastSpell(player, SPELL_COOLDOWN_CHECKER, true);
            player->GetSpellHistory()->AddCooldown(SPELL_COOLDOWN_CHECKER, 0, std::chrono::milliseconds(18 * HOUR * IN_MILLISECONDS));
        }
        else if (action != 4)
        {
            CloseGossipMenuFor(player);
            player->CastSpell(player, SPELL_RAM_AURA, true);
            player->CastSpell(player, SPELL_RAM_RACING_CROP, true);
        }

        return true;
    }

private:
    const SpellInfo* spellInfo;
};

class spell_throw_keg_player : public SpellScriptLoader
{
public:
    spell_throw_keg_player() : SpellScriptLoader("spell_throw_keg_player") { }

    class spell_throw_keg_player_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_throw_keg_player_SpellScript);

        SpellCastResult CheckCast()
        {
            Player* player = GetCaster()->ToPlayer();
            if (!player)
                return SPELL_FAILED_BAD_TARGETS;

            // Cooldown
            if (player->GetSpellHistory()->HasCooldown(SPELL_THROW_KEG_PLAYER))
                return SPELL_FAILED_NOT_READY;

            // Queststatus pr fen
            bool hasQuest =
                player->GetQuestStatus(QUEST_THERE_AND_BACK_AGAIN_A) == QUEST_STATUS_INCOMPLETE ||
                player->GetQuestStatus(QUEST_THERE_AND_BACK_AGAIN_H) == QUEST_STATUS_INCOMPLETE;

            if (!hasQuest)
                return SPELL_FAILED_BAD_TARGETS; // Neutraler R ckgabecode

            // Aura pr fen
            if (!player->HasAura(SPELL_RAM_AURA))
                return SPELL_FAILED_BAD_TARGETS;

            // Item pr fen
            if (!player->HasItemCount(ITEM_PORTABLE_BREWFEST_KEG, 1))
                return SPELL_FAILED_ITEM_GONE;

            return SPELL_CAST_OK;
        }

        void HandleDummy(SpellEffIndex /*effIndex*/)
        {
            Player* player = GetCaster()->ToPlayer();
            if (!player)
                return;

            // Item entfernen
            if (!player->DestroyItemCount(ITEM_PORTABLE_BREWFEST_KEG, 1, true))
                return;

            // Cooldown setzen (10 Sekunden)
            player->GetSpellHistory()->AddCooldown(SPELL_THROW_KEG_PLAYER, 0, std::chrono::milliseconds(10000));

            // Kill Credit und Tokens
            player->KilledMonsterCredit(KEG_KILL_CREDIT);
            player->CastSpell(player, SPELL_ADD_TOKENS, true);
        }

        void Register() override
        {
            OnCheckCast += SpellCheckCastFn(spell_throw_keg_player_SpellScript::CheckCast);
            OnEffectHitTarget += SpellEffectFn(spell_throw_keg_player_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_throw_keg_player_SpellScript();
    }
};
enum darkIronAttack
{
    // Gos
    GO_MOLE_MACHINE                     = 195305,
    GO_PLANS_A                          = 189989,
    GO_PLANS_H                          = 189990,

    // Npcs
    NPC_BARLEYBREW_KEG                  = 23700,
    NPC_THUNDERBREW_KEG                 = 23702,
    NPC_GORDOK_KEG                      = 23706,
    NPC_VOODOO_KEG                      = 24373,
    NPC_DROHN_KEG                       = 24372,
    NPC_MOLE_MACHINE_TRIGGER            = 23894,
    NPC_DARK_IRON_GUZZLER               = 23709,
    NPC_NORMAL_DROHN                    = 24492,
    NPC_NORMAL_VOODOO                   = 24493,
    NPC_NORMAL_BARLEYBREW               = 23683,
    NPC_NORMAL_THUNDERBREW              = 23684,
    NPC_NORMAL_GORDOK                   = 23685,
    NPC_EVENT_GENERATOR                 = 23703,
    NPC_SUPER_BREW_TRIGGER              = 23808,
    NPC_DARK_IRON_HERALD                = 24536,
    NPC_BREWFEST_REVELER                = 24484,

    // Events
    EVENT_CHECK_HOUR                    = 1,
    EVENT_SPAWN_MOLE_MACHINE            = 2,
    EVENT_PRE_FINISH_ATTACK             = 3,
    EVENT_FINISH_ATTACK                 = 4,
    EVENT_BARTENDER_SAY                 = 5,
    EVENT_TIME_LIMIT                    = 6,
    EVENT_CHECK_NEXT_TIME               = 7,
    EVENT_CHECK_PLAYERS_UI              = 8,

    // Spells
    SPELL_THROW_MUG_TO_PLAYER           = 42300,
    SPELL_ADD_MUG                       = 42518,
    SPELL_SPAWN_MOLE_MACHINE            = 43563,
    SPELL_KEG_MARKER                    = 42761,
    SPELL_PLAYER_MUG                    = 42436,
    SPELL_REPORT_DEATH                  = 42655,
    SPELL_CREATE_SUPER_BREW             = 42715,
    SPELL_DRUNKEN_MASTER                = 42696,
    SPELL_SUMMON_PLANS_A                = 48145,
    SPELL_SUMMON_PLANS_H                = 49318,
    SPELL_WEAK_ALCOHOL                  = 42523,

    // Dark Irons
    SPELL_ATTACK_KEG                    = 42393,
    SPELL_KNOCKBACK_AURA                = 42676,
    SPELL_MUG_BOUNCE_BACK               = 42522,
    DARK_IRON_SAY_1                     = 22316, // "Drink it all boys!"
    DARK_IRON_SAY_2                     = 22833, // "DRINK! BRAWL! DRINK! BRAWL!"
    DARK_IRON_SAY_3                     = 22318, // "Did someone say, "Free Brew"?"
    DARK_IRON_SAY_4                     = 22163, // "No one expects the Dark Iron dwarves!"
    DARK_IRON_SAY_5                     = 22317, // "It's not a party without some crashers!"

    DARK_IRON_HERALD_RF_SAY_1           = 23490, // "We did it boys!  Now back to the Grim Guzzler and we'll drink to the $3151W that were injuredl!"
    DARK_IRON_HERALD_DM_SAY_1           = 22601, // "We did it boys!  Now back to the Grim Guzzler and we'll drink to the $3096W that were injuredl!"
    DARK_IRON_HERALD_RF_SAY_2           = 23489, // "RETREAT!!  We've taken a beating and had $3151W casualties!  We can't keep taking these losses!  FALL BACK!!"
    DARK_IRON_HERALD_DM_SAY_2           = 22602, // "RETREAT!!  We've already lost $3096W and we can't afford to lose any more!!"

    BARTENDER_SAY_1                     = 22547, // "SOMEONE TRY THIS SUPER BREW!!"
    BARTENDER_SAY_2_RF                  = 26023, // "Chug and chuck!  Chug and chuck!"
    BARTENDER_SAY_2_DM                  = 23621, // "Chug and chuck!  Chug and chuck!"
    BARTENDER_SAY_3_RF                  = 22385, // "Down the free brew and pelt the Guzzlers with your mug!"
    BARTENDER_SAY_3_DM                  = 23620, // "Down the free brew and pelt the Guzzlers with your mug!"

    // Areas
    AREA_ROCKTUSK_FARM_ID   = 1296,
    AREA_DUN_MOROGH_ID      = 1,

    // World States
    WORLDSTATE_BREWFEST_RF_FINISH_COUNT                 = 3151,
    WORLDSTATE_BREWFEST_DM_FINISH_COUNT                 = 3096,
    WORLDSTATE_BREWFEST_RF_KEGS_EMPTIED_UI              = 5102,
    WORLDSTATE_BREWFEST_DM_KEGS_EMPTIED_UI              = 5106,
    WORLDSTATE_BREWFEST_RF_KEGS_EMPTIED                 = 5103,
    WORLDSTATE_BREWFEST_DM_KEGS_EMPTIED                 = 5107,
    WORLDSTATE_BREWFEST_RF_DWARVES_KILLED_UI            = 5100,
    WORLDSTATE_BREWFEST_DM_DWARVES_KILLED_UI            = 5104,
    WORLDSTATE_BREWFEST_RF_DWARVES_KILLED               = 5101,
    WORLDSTATE_BREWFEST_DM_DWARVES_KILLED               = 5105,
    WORLDSTATE_BREWFEST_RF_DWARVES_TO_KILL              = 5120,
    WORLDSTATE_BREWFEST_DM_DWARVES_TO_KILL              = 5121,
    WORLDSTATE_BREWFEST_RF_NEXT_EVENT_UI                = 5122,
    WORLDSTATE_BREWFEST_DM_NEXT_EVENT_UI                = 5123,
    WORLDSTATE_BREWFEST_RF_NEXT_EVENT_MIN               = 5124,
    WORLDSTATE_BREWFEST_DM_NEXT_EVENT_MIN               = 5125,
    WORLDSTATE_BREWFEST_RF_NEXT_EVENT_SEC               = 5126,
    WORLDSTATE_BREWFEST_DM_NEXT_EVENT_SEC               = 5127,
    WORLDSTATE_BREWFEST_RF_ANNOUNCER_NEXT_EVENT_UI      = 5128,
    WORLDSTATE_BREWFEST_RF_ANNOUNCER_NEXT_EVENT_MIN     = 5129,
    WORLDSTATE_BREWFEST_RF_ANNOUNCER_NEXT_EVENT_SEC     = 5130,
    WORLDSTATE_BREWFEST_DM_ANNOUNCER_NEXT_EVENT_UI      = 5131,
    WORLDSTATE_BREWFEST_DM_ANNOUNCER_NEXT_EVENT_MIN     = 5132,
    WORLDSTATE_BREWFEST_DM_ANNOUNCER_NEXT_EVENT_SEC     = 5133,
};

struct npc_brewfest_bark_trigger : public ScriptedAI
{
    npc_brewfest_bark_trigger(Creature* creature) : ScriptedAI(creature) { }

    void MoveInLineOfSight(Unit* who) override
    {
        if (!who || !me || !who->IsPlayer())
            return;

        Player* player = who->ToPlayer();

        // HARTE Abfrage: Ohne Allianz-Ram keine weitere Logik
        if (!player->HasAura(43883))
            return;

        if (me->GetDistance(who) < 10.0f)
        {
            // Bunny Trigger Spell-IDs nach Entry
            uint32 spellId = 0;
            switch (me->GetEntry())
            {
                case 24202: spellId = 43259; break; // Bunny 1
                case 24203: spellId = 43260; break; // Bunny 2
                case 24204: spellId = 43261; break; // Bunny 3
                case 24205: spellId = 43262; break; // Bunny 4
                default: break;
            }

            if (spellId)
            {
                me->CastSpell(player, spellId, true);
                return; // Spell gewirkt, keine Quest-Logik n tig
            }

            // Quest-Logik
            bool allow = false;
            uint32 quest = 0;

            if (me->GetAreaId() == AREA_ROCKTUSK_FARM_ID)
            {
                if (player->GetQuestStatus(QUEST_BARK_FOR_DROHNS_DISTILLERY) == QUEST_STATUS_INCOMPLETE)
                {
                    allow = true;
                    quest = QUEST_BARK_FOR_DROHNS_DISTILLERY;
                }
                else if (player->GetQuestStatus(QUEST_BARK_FOR_TCHALIS_VOODOO_BREWERY) == QUEST_STATUS_INCOMPLETE)
                {
                    allow = true;
                    quest = QUEST_BARK_FOR_TCHALIS_VOODOO_BREWERY;
                }
            }
            else if (me->GetAreaId() == AREA_DUN_MOROGH_ID)
            {
                if (player->GetQuestStatus(QUEST_BARK_BARLEYBREW) == QUEST_STATUS_INCOMPLETE)
                {
                    allow = true;
                    quest = QUEST_BARK_BARLEYBREW;
                }
                else if (player->GetQuestStatus(QUEST_BARK_FOR_THUNDERBREWS) == QUEST_STATUS_INCOMPLETE)
                {
                    allow = true;
                    quest = QUEST_BARK_FOR_THUNDERBREWS;
                }
            }

            if (allow)
            {
                auto itr = player->getQuestStatusMap().find(quest);
                if (itr == player->getQuestStatusMap().end())
                    return;

                QuestStatusData& q_status = itr->second;

                if (q_status.CreatureOrGOCount[me->GetEntry() - 24202] == 0)
                {
                    player->KilledMonsterCredit(me->GetEntry());

                    if (const uint32 textId = GetTextIdForQuest(quest))
                        me->Talk(textId, CHAT_MSG_MONSTER_SAY, 0, player);
                }
            }
        }
    }

    uint32 GetTextIdForQuest(uint32 questId)
    {
        switch (questId)
        {
            case QUEST_BARK_FOR_DROHNS_DISTILLERY:
                switch (urand(0, 3))
                {
                    case 0: return SAY_DROHN_DISTILLERY_1;
                    case 1: return SAY_DROHN_DISTILLERY_2;
                    case 2: return SAY_DROHN_DISTILLERY_3;
                    case 3: return SAY_DROHN_DISTILLERY_4;
                }
                break;
            case QUEST_BARK_FOR_TCHALIS_VOODOO_BREWERY:
                switch (urand(0, 3))
                {
                    case 0: return SAY_TCHALIS_VOODOO_1;
                    case 1: return SAY_TCHALIS_VOODOO_2;
                    case 2: return SAY_TCHALIS_VOODOO_3;
                    case 3: return SAY_TCHALIS_VOODOO_4;
                }
                break;
            case QUEST_BARK_BARLEYBREW:
                switch (urand(0, 3))
                {
                    case 0: return SAY_BARLEYBREW_1;
                    case 1: return SAY_BARLEYBREW_2;
                    case 2: return SAY_BARLEYBREW_3;
                    case 3: return SAY_BARLEYBREW_4;
                }
                break;
            case QUEST_BARK_FOR_THUNDERBREWS:
                switch (urand(0, 3))
                {
                    case 0: return SAY_THUNDERBREWS_1;
                    case 1: return SAY_THUNDERBREWS_2;
                    case 2: return SAY_THUNDERBREWS_3;
                    case 3: return SAY_THUNDERBREWS_4;
                }
                break;
            default:
                break;
        }

        return 0;
    }
};

struct npc_dark_iron_attack_generator : public ScriptedAI
{
    npc_dark_iron_attack_generator(Creature* creature) : ScriptedAI(creature), summons(me) { }

    EventMap events;
    SummonList summons;
    uint32 kegCounter, guzzlerCounter;
    uint8 thrown;
    GuidVector revelerGUIDs;

    void Reset() override
    {
        events.Reset();

        _requiredDwarves = sConfigMgr->GetIntDefault("EventBrewfest.DwarvesRequired", 50);
        _timeLimit = sConfigMgr->GetIntDefault("EventBrewfest.TimeLimit", 10) * MINUTE * IN_MILLISECONDS;
        _eventDelay = sConfigMgr->GetIntDefault("EventBrewfest.EventDelay", 30) * MINUTE * IN_MILLISECONDS;

        for (ObjectGuid const& guid : revelerGUIDs)
        {
            if (Creature* reveler = ObjectAccessor::GetCreature(*me, guid))
            {
                reveler->SetRespawnDelay(5 * MINUTE);
                reveler->Respawn();

                // It's here because SmartAI::JustRespawned restores original faction
                // So we need to delay a little bit reloading auras from creature_template_addon
                reveler->m_Events.AddEventAtOffset([reveler]()
                {
                    reveler->RemoveAllAuras();
                    reveler->LoadCreaturesAddon();
                }, 100ms);
            }
        }
        revelerGUIDs.clear();

        std::list<Player*> players;
        me->GetPlayerListInGrid(players, 150.f);
        for (Player* player : players)
        {
            player->SendUpdateWorldState(player->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_DWARVES_KILLED_UI : WORLDSTATE_BREWFEST_DM_DWARVES_KILLED_UI, 0);
            player->SendUpdateWorldState(player->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_KEGS_EMPTIED_UI : WORLDSTATE_BREWFEST_DM_KEGS_EMPTIED_UI, 0);
            player->SendUpdateWorldState(player->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_NEXT_EVENT_UI : WORLDSTATE_BREWFEST_DM_NEXT_EVENT_UI, 1);
        }

        sWorld->setWorldState(me->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_NEXT_EVENT_UI : WORLDSTATE_BREWFEST_DM_NEXT_EVENT_UI, 1);

        events.ScheduleEvent(EVENT_CHECK_NEXT_TIME, 1s);

        summons.DespawnAll();

        auto DespawnAllLeft = [this](uint32 entry, bool isCreature)
        {
            if (isCreature)
            {
                std::list<Creature*> remaining;
                me->GetCreatureListWithEntryInGrid(remaining, entry, 150.f);

                for (Creature* creature : remaining)
                    creature->DespawnOrUnsummon();
            }
            else
            {
                std::list<GameObject*> remaining;
                me->GetGameObjectListWithEntryInGrid(remaining, entry, 150.f);

                for (GameObject* go : remaining)
                    go->DespawnOrUnsummon();
            }
        };

        DespawnAllLeft(NPC_DARK_IRON_GUZZLER, true);
        DespawnAllLeft(NPC_SUPER_BREW_TRIGGER, true);
        DespawnAllLeft(NPC_MOLE_MACHINE_TRIGGER, true);
        DespawnAllLeft(GO_MOLE_MACHINE, false);

        summons.clear();

        events.ScheduleEvent(EVENT_CHECK_HOUR, 2s);
        kegCounter = 0;
        guzzlerCounter = 0;
        thrown = 0;

        _msOldTime = BrewfestStartTimer == 1 ? getMSTime() : BrewfestStartTimer;
        BrewfestStartTimer = _msOldTime;
    }

    void DoAction(int32 action) override
    {
        if (action == 98)
            FinishEventDueToLoss();
        else if (action == 99 && AllowStart(true))
            PrepareEvent();
    }

    // DARK IRON ATTACK EVENT
    void MoveInLineOfSight(Unit*  /*who*/) override {}
    void JustEngagedWith(Unit*) override {}

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id != SPELL_REPORT_DEATH)
            return;

        const uint32 entry = caster->GetEntry();
        if (entry == NPC_DARK_IRON_GUZZLER)
        {
            if (++guzzlerCounter == _requiredDwarves)
                FinishAttackDueToWin();
            else
            {
                std::list<Player*> players;
                me->GetPlayerListInGrid(players, 150.f);
                for (Player* player : players)
                    player->SendUpdateWorldState(player->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_DWARVES_KILLED : WORLDSTATE_BREWFEST_DM_DWARVES_KILLED, guzzlerCounter);
            }
        }
        else if (entry == NPC_DROHN_KEG || entry == NPC_VOODOO_KEG || entry == NPC_GORDOK_KEG || entry == NPC_THUNDERBREW_KEG || entry == NPC_BARLEYBREW_KEG || entry == NPC_GORDOK_KEG)
        {
            if (++kegCounter == 3)
                FinishEventDueToLoss();
            else
            {
                std::list<Player*> players;
                me->GetPlayerListInGrid(players, 150.f);
                for (Player* player : players)
                    player->SendUpdateWorldState(player->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_KEGS_EMPTIED : WORLDSTATE_BREWFEST_DM_KEGS_EMPTIED, kegCounter);
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);

        while (uint32 eventId = events.ExecuteEvent())
            switch (eventId)
            {
            case EVENT_CHECK_HOUR:
            {
                // determine hour
                if (AllowStart())
                {
                    PrepareEvent();
                    events.Repeat(300000ms);
                    return;
                }
                events.Repeat(2000ms);
                break;
            }
            case EVENT_SPAWN_MOLE_MACHINE:
            {
                if (me->GetAreaId() == AREA_ROCKTUSK_FARM_ID)
                {
                    float rand = 8 + rand_norm() * 12;
                    float angle = rand_norm() * 2 * M_PI;
                    float x = 1201.8f + rand * cos(angle);
                    float y = -4299.6f + rand * std::sin(angle);
                    if (Creature* cr = me->SummonCreature(NPC_MOLE_MACHINE_TRIGGER, x, y, 21.3f, 0.0f))
                        cr->CastSpell(cr, SPELL_SPAWN_MOLE_MACHINE, true);
                }
                else if (me->GetAreaId() == AREA_DUN_MOROGH_ID)
                {
                    float rand = rand_norm() * 20;
                    float angle = rand_norm() * 2 * M_PI;
                    float x = -5157.1f + rand * cos(angle);
                    float y = -598.98f + rand * std::sin(angle);
                    if (Creature* cr = me->SummonCreature(NPC_MOLE_MACHINE_TRIGGER, x, y, 398.11f, 0.0f))
                        cr->CastSpell(cr, SPELL_SPAWN_MOLE_MACHINE, true);
                }
                events.Repeat(3000ms);
                break;
            }
            case EVENT_PRE_FINISH_ATTACK:
            {
                events.CancelEvent(EVENT_SPAWN_MOLE_MACHINE);
                events.ScheduleEvent(EVENT_FINISH_ATTACK, 20s);
                break;
            }
            case EVENT_FINISH_ATTACK:
            {
                FinishAttackDueToWin();
                events.RescheduleEvent(EVENT_CHECK_HOUR, 1min);
                break;
            }
            case EVENT_BARTENDER_SAY:
            {
                events.Repeat(12000ms);
                Creature* sayer = GetRandomBartender();
                if (!sayer)
                    return;

                thrown++;
                 if (thrown == 3)
{
    thrown = 0;
    sayer->Say(BARTENDER_SAY_1);
    if (TempSummon* summon = sayer->SummonCreature(NPC_SUPER_BREW_TRIGGER, sayer->GetPositionX() + 15.f * cos(sayer->GetOrientation()), sayer->GetPositionY() + 15.f * sin(sayer->GetOrientation()),
        sayer->GetPositionZ(), 0.f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 30s))
        summons.Summon(summon);

    std::list<Player*> players;
    sayer->GetPlayerListInGrid(players, 100.0f);
    if (!players.empty())
        sayer->CastSpell(players.front(), SPELL_CREATE_SUPER_BREW, true);
}
                else
                {
                    if (urand(0, 1))
                        sayer->Say(me->GetAreaId() == 1296 ? BARTENDER_SAY_2_RF : BARTENDER_SAY_2_DM);
                    else
                        sayer->Say(me->GetAreaId() == 1296 ? BARTENDER_SAY_3_RF : BARTENDER_SAY_3_DM);
                }

                break;
            }
            case EVENT_TIME_LIMIT: FinishEventDueToLoss(); break;
            case EVENT_CHECK_NEXT_TIME: _EventNextTimeCheck(); events.Repeat(1s); break;
            case EVENT_CHECK_PLAYERS_UI: _EventCheckPlayersUI(); events.Repeat(3s); break;
            }
    }

    void FinishEventDueToLoss()
    {
        if (Creature* herald = me->FindNearestCreature(NPC_DARK_IRON_HERALD, 100.0f))
        {
            std::list<Player*> players;
            me->GetPlayerListInGrid(players, 150.f);
            for (Player* player : players)
                player->SendUpdateWorldState(player->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_FINISH_COUNT : WORLDSTATE_BREWFEST_DM_FINISH_COUNT, guzzlerCounter);

            herald->Yell(me->GetAreaId() == 1296 ? DARK_IRON_HERALD_RF_SAY_1 : DARK_IRON_HERALD_DM_SAY_1);
        }

        BrewfestStartTimer = 1;
        Reset();
        events.RescheduleEvent(EVENT_CHECK_HOUR, 1min);
    }

    void FinishAttackDueToWin()
    {
        if (Creature* herald = me->FindNearestCreature(NPC_DARK_IRON_HERALD, 100.0f))
        {
            std::list<Player*> players;
            me->GetPlayerListInGrid(players, 150.f);
            for (Player* player : players)
                player->SendUpdateWorldState(player->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_FINISH_COUNT : WORLDSTATE_BREWFEST_DM_FINISH_COUNT, guzzlerCounter);

            herald->Yell(me->GetAreaId() == 1296 ? DARK_IRON_HERALD_RF_SAY_2 : DARK_IRON_HERALD_DM_SAY_2);
        }

        BrewfestStartTimer = 1;
        me->CastSpell(me, (me->GetAreaId() == 1296 ? SPELL_SUMMON_PLANS_H : SPELL_SUMMON_PLANS_A), true);
        Reset();
    }

    void PrepareEvent()
    {
        events.CancelEvent(EVENT_CHECK_NEXT_TIME);
        events.ScheduleEvent(EVENT_CHECK_PLAYERS_UI, 3s);
        BrewfestStartTimer = 0;

        std::list<Creature*> revelers;
        GetCreatureListWithEntryInGrid(revelers, me, NPC_BREWFEST_REVELER, 100.f);
        for (Creature* reveler : revelers)
        {
            revelerGUIDs.push_back(reveler->GetGUID());
            reveler->SetRespawnDelay(MONTH);
            reveler->AI()->SetData(0, me->GetMapId());
        }

        Creature* cr;
        auto EnsureKeg = [&](uint32 entry, float x, float y, float z, float o)
        {
            if (Creature* keg = me->FindNearestCreature(entry, 150.f))
                cr = keg;
            else
                cr = me->SummonCreature(entry, x, y, z, o);

            if (cr)
            {
                cr->SetReactState(REACT_PASSIVE);
                summons.Summon(cr);
                revelerGUIDs.push_back(cr->GetGUID());
                cr->CastSpell(cr, SPELL_KEG_MARKER, true);
            }
        };

        auto SpawnSuperBrewTriggers = [&](float x, float y, float z, float o)
        {
            if (Creature* sbTrigger = me->FindNearestCreature(NPC_SUPER_BREW_TRIGGER, 150.f))
            {
                cr = sbTrigger;
                cr->Respawn(true);
            }
            else
                cr = me->SummonCreature(NPC_SUPER_BREW_TRIGGER, x, y, z, o);

            if (cr)
                summons.Summon(cr);
        };

        if (me->GetAreaId() == AREA_ROCKTUSK_FARM_ID)
        {
            EnsureKeg(NPC_DROHN_KEG, 1183.69f, -4315.15f, 21.1875f, 0.750492f);
            EnsureKeg(NPC_VOODOO_KEG, 1182.42f, -4272.45f, 21.1182f, -1.02974f);
            EnsureKeg(NPC_GORDOK_KEG, 1223.78f, -4296.48f, 21.1707f, -2.86234f);

            SpawnSuperBrewTriggers(1187.31f, -4314.69f, 21.3166f, 1.48962f);
            SpawnSuperBrewTriggers(1220.11f, -4298.07f, 21.3166f, 1.37265f);
            SpawnSuperBrewTriggers(1184.7f, -4275.28f, 21.2707f, 3.13497f);
        }
        else if (me->GetAreaId() == AREA_DUN_MOROGH_ID)
        {
            EnsureKeg(NPC_BARLEYBREW_KEG, -5187.23f, -599.779f, 397.176f, 0.017453f);
            EnsureKeg(NPC_THUNDERBREW_KEG, -5160.05f, -632.632f, 397.178f, 1.39626f);
            EnsureKeg(NPC_GORDOK_KEG, -5145.75f, -575.667f, 397.176f, -2.28638f);

            SpawnSuperBrewTriggers(-5183.95f, -601.58f, 397.301f, 0.90622f);
            SpawnSuperBrewTriggers(-5159.31f, -629.611f, 397.224f, 2.80666f);
            SpawnSuperBrewTriggers(-5146.07f, -579.877f, 397.301f, 2.30973f);
        }

        if ((cr = me->SummonCreature(NPC_DARK_IRON_HERALD, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0.0f, TEMPSUMMON_TIMED_DESPAWN, 300000ms)))
            summons.Summon(cr);

        kegCounter = 0;
        guzzlerCounter = 0;
        thrown = 0;

        sWorld->SendWorldText(LANG_BREWFEST_DARK_IRON_EVENT_ANNOUNCE);

        std::list<Player*> players;
        me->GetPlayerListInGrid(players, 150.f);
        for (Player* player : players)
    {
        uint32 areaId = player->GetAreaId();
        bool isHorde = (areaId == AREA_ROCKTUSK_FARM_ID);

        // Reset WorldStates zu Beginn des Events
        player->SendUpdateWorldState(isHorde ? WORLDSTATE_BREWFEST_RF_DWARVES_KILLED : WORLDSTATE_BREWFEST_DM_DWARVES_KILLED,0);
        player->SendUpdateWorldState(isHorde ? WORLDSTATE_BREWFEST_RF_DWARVES_KILLED_UI : WORLDSTATE_BREWFEST_DM_DWARVES_KILLED_UI,0);
        player->SendUpdateWorldState(isHorde ? WORLDSTATE_BREWFEST_RF_KEGS_EMPTIED : WORLDSTATE_BREWFEST_DM_KEGS_EMPTIED,0);
        player->SendUpdateWorldState(isHorde ? WORLDSTATE_BREWFEST_RF_KEGS_EMPTIED_UI : WORLDSTATE_BREWFEST_DM_KEGS_EMPTIED_UI,0);

        // Bestehende WorldStates setzen
        player->SendUpdateWorldState(isHorde ? WORLDSTATE_BREWFEST_RF_DWARVES_KILLED_UI : WORLDSTATE_BREWFEST_DM_DWARVES_KILLED_UI,1);
        player->SendUpdateWorldState(isHorde ? WORLDSTATE_BREWFEST_RF_KEGS_EMPTIED_UI : WORLDSTATE_BREWFEST_DM_KEGS_EMPTIED_UI,1);
        player->SendUpdateWorldState(isHorde ? WORLDSTATE_BREWFEST_RF_DWARVES_TO_KILL : WORLDSTATE_BREWFEST_DM_DWARVES_TO_KILL,_requiredDwarves);
        player->SendUpdateWorldState(isHorde ? WORLDSTATE_BREWFEST_RF_NEXT_EVENT_UI : WORLDSTATE_BREWFEST_DM_NEXT_EVENT_UI,0);
    }

    sWorld->setWorldState(me->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_NEXT_EVENT_UI : WORLDSTATE_BREWFEST_DM_NEXT_EVENT_UI,0);

    events.ScheduleEvent(EVENT_SPAWN_MOLE_MACHINE, 1500ms);
    events.ScheduleEvent(EVENT_PRE_FINISH_ATTACK, 280s);
    events.ScheduleEvent(EVENT_BARTENDER_SAY, 5s);
}

    bool AllowStart(bool ignoreTime = false)
    {
        if (!_requiredDwarves)
            return false;

        if (me->FindNearestGameObject(me->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? GO_PLANS_H : GO_PLANS_A, 150.f))
            return false;

        if (!ignoreTime)
        {
            const uint32 msTime = GetMSTimeDiffToNow(_msOldTime);
            if (msTime < _eventDelay)
                return false;
        }

        return true;
    }

    Creature* GetRandomBartender()
    {
        uint32 entry = 0;
        switch (urand(0, 2))
        {
        case 0:
            entry = (me->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? NPC_NORMAL_DROHN : NPC_NORMAL_THUNDERBREW);
            break;
        case 1:
            entry = (me->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? NPC_NORMAL_VOODOO : NPC_NORMAL_BARLEYBREW);
            break;
        case 2:
            entry = NPC_NORMAL_GORDOK;
            break;
        }

        return me->FindNearestCreature(entry, 100.0f);
    }

protected:
    uint32 _msOldTime;

private:
    uint32 _requiredDwarves;
    uint32 _timeLimit;
    uint32 _eventDelay;

    void _EventNextTimeCheck()
    {
        std::list<Player*> players;
        me->GetPlayerListInGrid(players, 150.f);

        if (players.empty())
            return;

        const uint32 secLeft = (_eventDelay - GetMSTimeDiffToNow(BrewfestStartTimer)) / IN_MILLISECONDS;
        const uint32 min = secLeft / MINUTE;
        const uint32 sec = secLeft % 60;

        const uint32 _targetPlayer = me->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? HORDE : ALLIANCE;
        for (Player* player : players)
        {
            if (player->GetTeam() != _targetPlayer)
                continue;

            player->SendUpdateWorldState(player->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_NEXT_EVENT_MIN : WORLDSTATE_BREWFEST_DM_NEXT_EVENT_MIN, min);
            player->SendUpdateWorldState(player->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_NEXT_EVENT_SEC : WORLDSTATE_BREWFEST_DM_NEXT_EVENT_SEC, sec);

            player->SendUpdateWorldState(player->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_ANNOUNCER_NEXT_EVENT_MIN : WORLDSTATE_BREWFEST_DM_ANNOUNCER_NEXT_EVENT_MIN, min);
            player->SendUpdateWorldState(player->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_ANNOUNCER_NEXT_EVENT_SEC : WORLDSTATE_BREWFEST_DM_ANNOUNCER_NEXT_EVENT_SEC, sec);
        }
    }

    void _EventCheckPlayersUI()
    {
        std::list<Player*> players;
        me->GetPlayerListInGrid(players, 150.f);

        if (players.empty())
            return;

        for (Player* player : players)
        {
            const uint32 areaId = player->GetAreaId();
            bool isHorde = (areaId == AREA_ROCKTUSK_FARM_ID);

            player->SendUpdateWorldState(isHorde ? WORLDSTATE_BREWFEST_RF_DWARVES_KILLED_UI : WORLDSTATE_BREWFEST_DM_DWARVES_KILLED_UI,1);
            player->SendUpdateWorldState(isHorde ? WORLDSTATE_BREWFEST_RF_KEGS_EMPTIED_UI : WORLDSTATE_BREWFEST_DM_KEGS_EMPTIED_UI,1);
            player->SendUpdateWorldState(isHorde ? WORLDSTATE_BREWFEST_RF_DWARVES_TO_KILL : WORLDSTATE_BREWFEST_DM_DWARVES_TO_KILL,_requiredDwarves);
            player->SendUpdateWorldState(isHorde ? WORLDSTATE_BREWFEST_RF_NEXT_EVENT_UI : WORLDSTATE_BREWFEST_DM_NEXT_EVENT_UI,0);
        }
    }
};

struct npc_dark_iron_attack_mole_machine : public ScriptedAI
{
    npc_dark_iron_attack_mole_machine(Creature* creature) : ScriptedAI(creature) { }

    void JustEngagedWith(Unit*) override {}
    void MoveInLineOfSight(Unit*) override {}
    void AttackStart(Unit*) override {}

    uint32 goTimer, summonTimer;
    void Reset() override
    {
        goTimer = 1;
        summonTimer = 0;
    }

    void UpdateAI(uint32 diff) override
    {
        if (goTimer)
        {
            goTimer += diff;
            if (goTimer >= 3000)
            {
                goTimer = 0;
                summonTimer++;
                if (GameObject* drill = me->SummonGameObject(GO_MOLE_MACHINE, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), float(M_PI) / 4.f,
                    QuaternionData(0.f, 0.f, 0.f, 0.f), 8s))
                {
                    //drill->SetGoAnimProgress(0);
                    drill->SetLootState(GO_READY);
                    drill->UseDoorOrButton(8);
                }
            }
        }
        if (summonTimer)
        {
            summonTimer += diff;
            if (summonTimer >= 2000 && summonTimer < 10000)
            {
                me->SummonCreature(NPC_DARK_IRON_GUZZLER, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0.f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 6000ms);
                summonTimer = 10000;
            }
            if (summonTimer >= 13000 && summonTimer < 20000)
            {
                me->SummonCreature(NPC_DARK_IRON_GUZZLER, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0.f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 6000ms);
                summonTimer = 0;
                me->DespawnOrUnsummon(3000ms);
            }
        }
    }
};

struct npc_dark_iron_guzzler : public ScriptedAI
{
    npc_dark_iron_guzzler(Creature* creature) : ScriptedAI(creature)
    {
        me->SetReactState(REACT_PASSIVE);
        attacking = false;
        _kegs.clear();
    }

    void JustEngagedWith(Unit*) override {}
    void MoveInLineOfSight(Unit*) override {}
    void AttackStart(Unit*) override {}

    void MovementInform(uint32 type, uint32 /*id*/) override
    {
        if (type != FOLLOW_MOTION_TYPE)
            return;

        if (Unit* target = GetTarget())
        {
            timer = 0;
            attacking = true;
            me->CastSpell(target, SPELL_ATTACK_KEG);
        }
    }

    void FindNextKeg()
    {
        attacking = false;

        Trinity::Containers::RandomShuffle(_kegs);

        std::vector<ObjectGuid>::iterator itr = _kegs.begin();
        for (uint8 i = 0; i < uint8(_kegs.size()) && itr != _kegs.end(); ++i)
        {
            if (Creature* cr = ObjectAccessor::GetCreature(*me, *itr))
            {
                cr->SetWalk(true);
                me->GetMotionMaster()->MoveFollow(cr, 1.0f, me->GetFollowAngle());
                targetGUID = *itr;
                return;
            }

            ++itr;
        }
    }

    Unit* GetTarget() { return ObjectAccessor::GetUnit(*me, targetGUID); }

    void Reset() override
    {
        if (!CheckKegs())
            return;

        timer = 0;
        targetGUID.Clear();
        me->SetWalk(true);
        FindNextKeg();
        me->ApplySpellImmune(SPELL_ATTACK_KEG, IMMUNITY_ID, SPELL_ATTACK_KEG, true);
        SayText();
        me->CastSpell(me, SPELL_KNOCKBACK_AURA, true);
    }

    void SayText()
    {
        if (!urand(0, 20))
        {
            switch (urand(0, 4))
            {
            case 0: me->Say(DARK_IRON_SAY_1); break;
            case 1: me->Say(DARK_IRON_SAY_2); break;
            case 2: me->Say(DARK_IRON_SAY_3); break;
            case 3: me->Say(DARK_IRON_SAY_4); break;
            case 4: me->Say(DARK_IRON_SAY_5); break;
            }
        }
    }

    void KilledUnit(Unit* who) override
    {
        if (who == me)
            return;

        who->CastSpell(who, SPELL_REPORT_DEATH, true);
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
    {
        if (me->IsAlive() && spellInfo->Id == SPELL_PLAYER_MUG)
        {
            me->CastSpell(me, SPELL_MUG_BOUNCE_BACK, true);
            me->KillSelf();
            me->CastSpell(me, SPELL_REPORT_DEATH, true);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        timer += diff;
        if (timer < 2000)
            return;

        timer = 0;

        // Pr fe, ob NPC_EVENT_GENERATOR in 100 Yards Reichweite existiert
        Creature* generator = me->FindNearestCreature(NPC_EVENT_GENERATOR, 100.0f);
        if (!generator)
        {
            me->DespawnOrUnsummon();
            return;  // sofort despawnen wenn nicht da
        }

        if (!targetGUID)
            return;

        Unit* target = GetTarget();
        if (target && target->IsAlive())
        {
            if (attacking)
                me->CastSpell(target, SPELL_ATTACK_KEG);
        }
        else
            FindNextKeg();
    }

protected:
    std::vector<ObjectGuid> _kegs;

private:
    uint32 timer;
    ObjectGuid targetGUID;
    bool attacking;

    bool CheckKegs()
    {
        _kegs.clear();

        for (uint8 kegID = 0; kegID < 3; ++kegID)
        {
            uint32 entry = 0;
            switch (kegID)
            {
            case 0: entry = me->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? NPC_DROHN_KEG : NPC_THUNDERBREW_KEG; break;
            case 1: entry = me->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? NPC_VOODOO_KEG : NPC_BARLEYBREW_KEG; break;
            case 2: entry = NPC_GORDOK_KEG; break;
            }

            if (!entry)
                continue;

            Creature* keg = me->FindNearestCreature(entry, 100.0f);
            if (!keg)
                continue;

            _kegs.push_back(keg->GetGUID());
        }

        if (_kegs.empty())
        {
            if (Creature* generator = me->FindNearestCreature(NPC_EVENT_GENERATOR, 150.f))
                if (generator->IsAIEnabled())
                    generator->AI()->DoAction(99);

            return false;
        }

        return true;
    }
};

struct npc_brewfest_super_brew_trigger : public ScriptedAI
{
    npc_brewfest_super_brew_trigger(Creature* creature) : ScriptedAI(creature) { }

    uint32 timer;
    void JustEngagedWith(Unit*) override {}
    void MoveInLineOfSight(Unit*  /*who*/) override
    {
    }

    void AttackStart(Unit*) override {}

    void Reset() override
    {
        if (!sWorld->getWorldState(me->GetAreaId() == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_NEXT_EVENT_UI : WORLDSTATE_BREWFEST_DM_NEXT_EVENT_UI))
        {
            me->DespawnOrUnsummon();
            return;
        }

        timer = 0;
        me->SummonGameObject(186478, me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), 0.f, QuaternionData(0.f, 0.f, 0.f, 0.f), 30s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (timer >= 500)
        {
            timer = 0;
            Player* player = nullptr;
            Trinity::AnyPlayerInObjectRangeCheck checker(me, 2.0f);
            Trinity::PlayerSearcher<Trinity::AnyPlayerInObjectRangeCheck> searcher(me, player, checker);
            Cell::VisitWorldObjects(me, searcher, 2.0f);
            if (!player)
                return;

            me->CastSpell(player, SPELL_DRUNKEN_MASTER, true);
            me->RemoveAllGameObjects();
            me->KillSelf();
            me->DespawnOrUnsummon();
        }
        else
            timer += diff;
    }
};


struct npc_brewfest_announcer : public ScriptedAI
{
    npc_brewfest_announcer(Creature* creature) : ScriptedAI(creature)
    {
        _teleports[TEAM_ALLIANCE] = WorldLocation(0, -5134.032f, -619.30054f, 398.65698f, 2.454368f);
        _teleports[TEAM_HORDE] = WorldLocation(1, 1209.0376f, -4269.708f, 22.32866f, 4.215077f);

        _modelActiveAlliance = 21847;
        _modelInactiveAlliance = 3047;
        _modelActiveHorde = 21855;
        _modelInactiveHorde = 7137;

        _hordeZones = { {1, 1637}, {1, 1638}, {0, 1497} };
        _allianceZones = { {0, 1519}, {0, 1537}, {0, 1657} };
    }

    void Reset() override
    {
        _eventDelay = sConfigMgr->GetIntDefault("EventBrewfest.EventDelay", 30) * MINUTE * IN_MILLISECONDS;
        _eventTeleport = sConfigMgr->GetIntDefault("EventBrewfest.Teleport", 5) * MINUTE * IN_MILLISECONDS;
        UpdateModelByLocation();
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        UpdateModelByLocation();
    }

    bool OnGossipHello(Player* player) override
    {
        ClearGossipMenuFor(player);

        if (sGameEventMgr->IsActiveEvent(24))
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Brauereifest-Informationen anzeigen", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO);

        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, "Zeig mir deine Waren.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_VENDOR);

        SendGossipMenuFor(player, player->GetGossipTextId(me), me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
        ClearGossipMenuFor(player);

        if (action == GOSSIP_ACTION_INFO)
        {
            ShowEventInfo(player);
            return true;
        }

        if (action == GOSSIP_ACTION_VENDOR)
        {
            CloseGossipMenuFor(player);
            player->GetSession()->SendListInventory(me->GetGUID());
            return true;
        }

        if (action == GOSSIP_ACTION_BATTLE)
        {
            CloseGossipMenuFor(player);
            player->TeleportTo(player->GetTeam() == HORDE ? _teleports[TEAM_HORDE] : _teleports[TEAM_ALLIANCE]);
            return true;
        }

        return false;
    }

private:
    uint32 _eventDelay;
    uint32 _eventTeleport;

    WorldLocation _teleports[2];

    uint32 _modelActiveAlliance;
    uint32 _modelInactiveAlliance;
    uint32 _modelActiveHorde;
    uint32 _modelInactiveHorde;

    std::set<std::pair<uint32, uint32>> _hordeZones;
    std::set<std::pair<uint32, uint32>> _allianceZones;

    void ShowEventInfo(Player* player)
    {
        bool eventActive = sGameEventMgr->IsActiveEvent(24);
        uint32 npcTextId = 0;
        bool addTp = false;

        if (eventActive)
        {
            if (BrewfestStartTimer)
            {
                if (BrewfestStartTimer == 1)
                    BrewfestStartTimer = getMSTime();

                const uint32 timeLeft = _eventDelay - GetMSTimeDiffToNow(BrewfestStartTimer);
                const uint32 secLeft = timeLeft / IN_MILLISECONDS;
                const uint32 min = secLeft / MINUTE;
                const uint32 sec = secLeft % 60;

                player->SendUpdateWorldState(player->GetTeam() == HORDE ? WORLDSTATE_BREWFEST_RF_ANNOUNCER_NEXT_EVENT_MIN : WORLDSTATE_BREWFEST_DM_ANNOUNCER_NEXT_EVENT_MIN, min);
                player->SendUpdateWorldState(player->GetTeam() == HORDE ? WORLDSTATE_BREWFEST_RF_ANNOUNCER_NEXT_EVENT_SEC : WORLDSTATE_BREWFEST_DM_ANNOUNCER_NEXT_EVENT_SEC, sec);

                if (!_eventTeleport || timeLeft <= _eventTeleport)
                    addTp = true;

                npcTextId = player->GetTeam() == HORDE ? NPC_TEXT_H : NPC_TEXT_A;
            }
            else
            {
                addTp = true;
                npcTextId = player->GetTeam() == HORDE ? NPC_TEXT_BEGUN_H : NPC_TEXT_BEGUN_A;
            }
        }
        else
            npcTextId = player->GetTeam() == HORDE ? NPC_TEXT_INACTIVE_H : NPC_TEXT_INACTIVE_A;

        if (addTp)
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Teleport me to battle!", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_BATTLE);

        SendGossipMenuFor(player, npcTextId, me->GetGUID());
    }

    void UpdateModelByLocation()
    {
        std::pair<uint32, uint32> loc = { me->GetMapId(), me->GetAreaId() };
        bool eventActive = sGameEventMgr->IsActiveEvent(24);

        uint32 modelId = _modelInactiveAlliance;

        if (_hordeZones.count(loc))
            modelId = eventActive ? _modelActiveHorde : _modelInactiveHorde;
        else if (_allianceZones.count(loc))
            modelId = eventActive ? _modelActiveAlliance : _modelInactiveAlliance;

        me->SetDisplayId(modelId);
    }

    enum Gossip
    {
        GOSSIP_ACTION_INFO = GOSSIP_ACTION_INFO_DEF,
        GOSSIP_ACTION_VENDOR = GOSSIP_ACTION_INFO_DEF + 1,
        GOSSIP_ACTION_BATTLE = GOSSIP_ACTION_INFO_DEF + 2,

        NPC_TEXT_A = 200000,
        NPC_TEXT_H = 200001,
        NPC_TEXT_BEGUN_A = 200002,
        NPC_TEXT_BEGUN_H = 200003,
        NPC_TEXT_INACTIVE_A = 200004,
        NPC_TEXT_INACTIVE_H = 200005,
    };
};



///////////////////////////////////////
////// SPELLS
///////////////////////////////////////

enum ramRacing
{
    SPELL_TROT              = 42992,
    SPELL_CANTER            = 42993,
    SPELL_GALLOP            = 42994,
    SPELL_RAM_EXHAUSTED     = 43332,

    CREDIT_TROT             = 24263,
    CREDIT_CANTER           = 24264,
    CREDIT_GALLOP           = 24265,

    RACING_RAM_MODEL        = 22630,
};

class spell_brewfest_main_ram_buff : public AuraScript
{
    PrepareAuraScript(spell_brewfest_main_ram_buff);

    bool Load() override
    {
        questTick = 0;
        privateLevel = 0;
        return true;
    }

    void HandleEffectPeriodic(AuraEffect const* aurEff)
    {
        Unit* caster = GetCaster();
        if (!caster || !caster->IsMounted() || !caster->ToPlayer())
            return;

        if (caster->GetMountDisplayId() != RACING_RAM_MODEL)
            return;

        Aura* aur = caster->GetAura(42924);
        if (!aur)
        {
            caster->CastSpell(caster, 42924, true);
            return;
        }

        // Check if exhausted
        if (caster->GetAura(SPELL_RAM_EXHAUSTED))
        {
            if (privateLevel)
            {
                caster->RemoveAurasDueToSpell(SPELL_CANTER);
                caster->RemoveAurasDueToSpell(SPELL_GALLOP);
            }

            aur->SetStackAmount(1);
            return;
        }

        uint32 stack = aur->GetStackAmount();
        uint8 mode = 0;
        switch (privateLevel)
        {
        case 0:
            if (stack > 1)
            {
                questTick = 0;
                caster->CastSpell(caster, SPELL_TROT, true);
                privateLevel++;
                mode = 1; // unapply
                break;
            }
            // just walking, fatiuge handling
            if (Aura* fatigueAura = caster->GetAura(SPELL_RAM_FATIGUE))
            {
                fatigueAura->ModStackAmount(-4);
            }
            break;
        case 1:
            // One click to maintain speed, more to increase
            if (stack < 2)
            {
                caster->RemoveAurasDueToSpell(SPELL_TROT);
                questTick = 0;
                privateLevel--;
                mode = 2; // apply
            }
            else if (stack > 2)
            {
                questTick = 0;
                caster->CastSpell(caster, SPELL_CANTER, true);
                privateLevel++;
            }
            else if (questTick++ > 3)
                caster->ToPlayer()->KilledMonsterCredit(CREDIT_TROT);
            break;
        case 2:
            // Two - three clicks to maintains speed, less to decrease, more to increase
            if (stack < 3)
            {
                caster->CastSpell(caster, SPELL_TROT, true);
                privateLevel--;
                questTick = 0;
            }
            else if (stack > 4)
            {
                caster->CastSpell(caster, SPELL_GALLOP, true);
                privateLevel++;
                questTick = 0;
            }
            else if (questTick++ > 3)
                caster->ToPlayer()->KilledMonsterCredit(CREDIT_CANTER);
            break;
        case 3:
            // Four or more clicks to maintains speed, less to decrease
            if (stack < 5)
            {
                caster->CastSpell(caster, SPELL_CANTER, true);
                privateLevel--;
                questTick = 0;
            }
            else if (questTick++ > 3)
                caster->ToPlayer()->KilledMonsterCredit(CREDIT_GALLOP);
            break;
        }

        // Set to base amount
        aur->SetStackAmount(1);

        // apply/unapply effect 1
        if (mode)
            if (Aura* base = aurEff->GetBase())
                if (AuraEffect* aEff = base->GetEffect(EFFECT_0))
                {
                    aEff->SetAmount(mode == 1 ? 0 : -50);
                    caster->UpdateSpeed(MOVE_RUN);
                }
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Unit* target = GetTarget())
            target->RemoveAurasDueToSpell(SPELL_RAM_FATIGUE);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_brewfest_main_ram_buff::HandleEffectPeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
        OnEffectRemove += AuraEffectRemoveFn(spell_brewfest_main_ram_buff::HandleEffectRemove, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }

private:
    uint8 privateLevel;
    uint32 questTick;
};

class spell_brewfest_ram_fatigue : public AuraScript
{
    PrepareAuraScript(spell_brewfest_ram_fatigue);

    void HandleEffectPeriodic(AuraEffect const* aurEff)
    {
        Unit* target = GetTarget();
        if (!target)
            return;

        int8 fatigue = 0;
        uint32 npcCredit = 0;

        switch (aurEff->GetId())
        {
            case SPELL_TROT:   fatigue = -2; npcCredit = 24263; break;
            case SPELL_CANTER: fatigue = 1;  npcCredit = 24264; break;
            case SPELL_GALLOP: fatigue = 5;  npcCredit = 24265; break;
            default: return;
        }

        // Fatigue-Handling
        if (Aura* fatigueAura = target->GetAura(SPELL_RAM_FATIGUE))
        {
            fatigueAura->ModStackAmount(fatigue);
            if (fatigueAura->GetStackAmount() >= 100)
                target->CastSpell(target, SPELL_RAM_EXHAUSTED, true);
        }
        else
        {
            target->CastSpell(target, SPELL_RAM_FATIGUE, true);
        }

        // Killcredit-Handling nach 8 Sekunden
        if (Player* player = target->ToPlayer())
        {
            if (Aura* aura = player->GetAura(aurEff->GetId()))
            {
                if (aura->GetDuration() <= aura->GetMaxDuration() - 8000)
                {
                    player->KilledMonsterCredit(npcCredit);
                }
            }
        }
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Unit* target = GetTarget())
        {
            if (Aura* fatigueAura = target->GetAura(SPELL_RAM_FATIGUE))
                fatigueAura->ModStackAmount(-15);
        }
    }

    void Register() override
    {
        if (m_scriptSpellId != 43332)
        {
            OnEffectPeriodic += AuraEffectPeriodicFn(spell_brewfest_ram_fatigue::HandleEffectPeriodic, EFFECT_1, SPELL_AURA_PERIODIC_DUMMY);
        }
        else
        {
            OnEffectRemove += AuraEffectRemoveFn(spell_brewfest_ram_fatigue::HandleEffectRemove, EFFECT_0, SPELL_AURA_MOD_DECREASE_SPEED, AURA_EFFECT_HANDLE_REAL);
        }
    }
};


class spell_brewfest_apple_trap : public SpellScript
{
    PrepareSpellScript(spell_brewfest_apple_trap);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if(Trinity::UnitAuraCheck(false, SPELL_RAM_FATIGUE));

        if (targets.empty())
            FinishCast(SPELL_FAILED_CASTER_AURASTATE);
    }

    void HandleDummyEffect(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            if (Aura* aur = target->GetAura(SPELL_RAM_FATIGUE))
                aur->Remove();
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_brewfest_apple_trap::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_brewfest_apple_trap::HandleDummyEffect, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

enum Catch
{
    NPC_WILD_WOLPERTINGER = 23487,

    ITEM_STUNNED_WOLPERTINGER = 32906
};

class spell_catch_the_wild_wolpertinger : public AuraScript
{
    PrepareAuraScript(spell_catch_the_wild_wolpertinger);

    void HandleEffectApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Creature* wild = GetTarget()->ToCreature())
        {
            if (wild->GetEntry() == NPC_WILD_WOLPERTINGER)
            {
                wild->ToCreature()->DespawnOrUnsummon(1s, 0s);
                GetCaster()->ToPlayer()->AddItem(ITEM_STUNNED_WOLPERTINGER, 1);
            }
        }
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_catch_the_wild_wolpertinger::HandleEffectApply, EFFECT_0, SPELL_AURA_MOD_PACIFY_SILENCE, AURA_EFFECT_HANDLE_REAL);
    }
};

enum fillKeg
{
    GREEN_EMPTY_KEG             = 37892,
    BLUE_EMPTY_KEG              = 33016,
    YELLOW_EMPTY_KEG            = 32912,
};

class spell_brewfest_fill_keg : public SpellScript
{
    PrepareSpellScript(spell_brewfest_fill_keg);

    void HandleAfterHit()
    {
        if (GetCaster() && GetCaster()->ToPlayer())
        {
            if (Item* itemCaster = GetCastItem())
            {
                Player* player = GetCaster()->ToPlayer();
                uint32 item = 0;
                switch (itemCaster->GetEntry())
                {
                case GREEN_EMPTY_KEG:
                case BLUE_EMPTY_KEG:
                    item = itemCaster->GetEntry() + urand(1, 5); // 5 items, id in range empty+1-5
                    break;
                case YELLOW_EMPTY_KEG:
                    if (uint8 num = urand(0, 4))
                        item = 32916 + num;
                    else
                        item = 32915;
                    break;
                }

                if (item && player->AddItem(item, 1)) // ensure filled keg is stored
                {
                    player->DestroyItemCount(itemCaster->GetEntry(), 1, true);
                    GetSpell()->m_CastItem = nullptr;
                    GetSpell()->m_castItemGUID.Clear();
                }
            }
        }
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_brewfest_fill_keg::HandleAfterHit);
    }
};

class spell_brewfest_unfill_keg : public SpellScript
{
    PrepareSpellScript(spell_brewfest_unfill_keg);

    uint32 GetEmptyEntry(uint32 baseEntry)
    {
        switch (baseEntry)
        {
        case 37893:
        case 37894:
        case 37895:
        case 37896:
        case 37897:
            return GREEN_EMPTY_KEG;
        case 33017:
        case 33018:
        case 33019:
        case 33020:
        case 33021:
            return BLUE_EMPTY_KEG;
        case 32915:
        case 32917:
        case 32918:
        case 32919:
        case 32920:
            return YELLOW_EMPTY_KEG;
        }

        return 0;
    }

    void HandleAfterHit()
    {
        if (GetCaster() && GetCaster()->ToPlayer())
        {
            if (Item* itemCaster = GetCastItem())
            {
                uint32 item = GetEmptyEntry(itemCaster->GetEntry());
                Player* player = GetCaster()->ToPlayer();

                if (item && player->AddItem(item, 1)) // ensure filled keg is stored
                {
                    player->DestroyItemCount(itemCaster->GetEntry(), 1, true);
                    GetSpell()->m_CastItem = nullptr;
                    GetSpell()->m_castItemGUID.Clear();
                }
            }
        }
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_brewfest_unfill_keg::HandleAfterHit);
    }
};

class spell_brewfest_toss_mug : public SpellScript
{
    PrepareSpellScript(spell_brewfest_toss_mug);

    SpellCastResult CheckCast()
    {
        if (Unit* caster = GetCaster())
        {
            WorldLocation pPosition = WorldLocation(*caster);
            caster->MovePositionToFirstCollision(pPosition, 14.f, 0.f);
            SetExplTargetDest(pPosition);
        }

        return SPELL_CAST_OK;
    }

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        WorldObject* target = nullptr;
        for (std::list<WorldObject*>::iterator itr = targets.begin(); itr != targets.end(); ++itr)
        {
            const float objectSize = (*itr)->GetCombatReach();
            if (caster->HasInLine((*itr), objectSize, 2.f))
            {
                target = (*itr);
                break;
            }
        }

        targets.clear();
        if (target)
            targets.push_back(target);

        targets.push_back(caster);
    }

    void HandleBeforeHit(SpellMissInfo missInfo)
    {
        if (missInfo != SPELL_MISS_NONE)
        {
            return;
        }

        if (Unit* target = GetHitUnit())
        {
            if (!GetCaster() || target->GetGUID() == GetCaster()->GetGUID())
                return;

            WorldLocation pPosition = WorldLocation(target->GetMapId(), target->GetPositionX(), target->GetPositionY(), target->GetPositionZ() + 4.0f, target->GetOrientation());
            SetExplTargetDest(pPosition);
        }
    }

    void HandleScriptEffect(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (!GetHitUnit() || GetHitUnit()->GetGUID() != caster->GetGUID())
            return;

        std::vector<Creature*> bakers;
        if (caster->GetAreaId() == AREA_ROCKTUSK_FARM_ID)
        {
            if (Creature* creature = caster->FindNearestCreature(NPC_NORMAL_VOODOO, 40.0f))
            {
                bakers.push_back(creature);
            }

            if (Creature* creature = caster->FindNearestCreature(NPC_NORMAL_DROHN, 40.0f))
            {
                bakers.push_back(creature);
            }

            if (Creature* creature = caster->FindNearestCreature(NPC_NORMAL_GORDOK, 40.0f))
            {
                bakers.push_back(creature);
            }
        }
        else
        {
            if (Creature* creature = caster->FindNearestCreature(NPC_NORMAL_THUNDERBREW, 40.0f))
            {
                bakers.push_back(creature);
            }

            if (Creature* creature = caster->FindNearestCreature(NPC_NORMAL_BARLEYBREW, 40.0f))
            {
                bakers.push_back(creature);
            }

            if (Creature* creature = caster->FindNearestCreature(NPC_NORMAL_GORDOK, 40.0f))
            {
                bakers.push_back(creature);
            }
        }

        if (!bakers.empty())
        {
            std::sort(bakers.begin(), bakers.end(), Trinity::ObjectDistanceOrderPred(caster));
            if (Creature* creature = *bakers.begin())
            {
                creature->CastSpell(caster, SPELL_THROW_MUG_TO_PLAYER, true);
            }
        }

        caster->CastSpell(caster, SPELL_WEAK_ALCOHOL, true);
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_brewfest_toss_mug::CheckCast);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_brewfest_toss_mug::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
        BeforeHit += BeforeSpellHitFn(spell_brewfest_toss_mug::HandleBeforeHit);
        OnEffectHitTarget += SpellEffectFn(spell_brewfest_toss_mug::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_brewfest_add_mug : public SpellScript
{
    PrepareSpellScript(spell_brewfest_add_mug);

    void HandleDummyEffect(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
            target->CastSpell(target, SPELL_ADD_MUG, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_brewfest_add_mug::HandleDummyEffect, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

enum brewBubble
{
    SPELL_BUBBLE_BUILD_UP           = 49828,
};

struct npc_brew_bubble : public NullCreatureAI
{
    npc_brew_bubble(Creature* creature) : NullCreatureAI(creature) { }

    uint32 timer;

    void Reset() override
    {
        me->SetReactState(REACT_AGGRESSIVE);
        me->GetMotionMaster()->MoveRandom(15.0f);
        timer = 0;
    }

    void DoAction(int32) override
    {
        timer = 0;
    }

    void MoveInLineOfSight(Unit* target) override
    {
        if (target->GetEntry() == me->GetEntry())
            if (me->IsWithinDist(target, 1.0f))
            {
                uint8 stacksMe = me->GetAuraCount(SPELL_BUBBLE_BUILD_UP);
                uint8 stacksTarget = target->GetAuraCount(SPELL_BUBBLE_BUILD_UP);
                if (stacksMe >= stacksTarget)
                {
                    if (Aura* aura = me->GetAura(SPELL_BUBBLE_BUILD_UP))
                        aura->ModStackAmount(stacksTarget + 1);
                    else
                        me->AddAura(SPELL_BUBBLE_BUILD_UP, me);

                    target->ToCreature()->DespawnOrUnsummon();
                    DoAction(0);
                }
                else if (Aura* aura = target->GetAura(SPELL_BUBBLE_BUILD_UP))
                {
                    aura->ModStackAmount(stacksMe);

                    target->ToCreature()->AI()->DoAction(0);
                    me->DespawnOrUnsummon();
                }
            }
    }

    void UpdateAI(uint32 diff) override
    {
        timer += diff;
        if (timer >= 25000)
        {
            timer = 0;
            me->DespawnOrUnsummon();
        }
    }
};

enum BrewfestRevelerEnum
{
    FACTION_ALLIANCE    = 1934,
    FACTION_HORDE       = 1935,

    SPELL_BREWFEST_REVELER_TRANSFORM_GOBLIN_MALE          = 44003,
    SPELL_BREWFEST_REVELER_TRANSFORM_GOBLIN_FEMALE        = 44004,
    SPELL_BREWFEST_REVELER_TRANSFORM_BE                   = 43907,
    SPELL_BREWFEST_REVELER_TRANSFORM_ORC                  = 43914,
    SPELL_BREWFEST_REVELER_TRANSFORM_TAUREN               = 43915,
    SPELL_BREWFEST_REVELER_TRANSFORM_TROLL                = 43916,
    SPELL_BREWFEST_REVELER_TRANSFORM_UNDEAD               = 43917,

    SPELL_DRUNKEN_BREWFEST_REVELER_TRANSFORM_GOBLIN_MALE  = 44096
};

class spell_brewfest_reveler_transform : public AuraScript
{
    PrepareAuraScript(spell_brewfest_reveler_transform);

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        uint32 factionId = FACTION_ALLIANCE;
        switch (m_scriptSpellId)
        {
        case SPELL_BREWFEST_REVELER_TRANSFORM_BE:
        case SPELL_BREWFEST_REVELER_TRANSFORM_ORC:
        case SPELL_BREWFEST_REVELER_TRANSFORM_TAUREN:
        case SPELL_BREWFEST_REVELER_TRANSFORM_TROLL:
        case SPELL_BREWFEST_REVELER_TRANSFORM_UNDEAD:
            factionId = FACTION_HORDE;
            break;
        case SPELL_BREWFEST_REVELER_TRANSFORM_GOBLIN_MALE:
        case SPELL_BREWFEST_REVELER_TRANSFORM_GOBLIN_FEMALE:
        case SPELL_DRUNKEN_BREWFEST_REVELER_TRANSFORM_GOBLIN_MALE:
            factionId = FACTION_FRIENDLY;
            break;
        default:
            break;
        }

        GetTarget()->SetFaction(factionId);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_brewfest_reveler_transform::OnApply, EFFECT_0, SPELL_AURA_TRANSFORM, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_brewfest_relay_race_force_cast : public SpellScript
{
    PrepareSpellScript(spell_brewfest_relay_race_force_cast);

    SpellCastResult CheckItem()
    {
        if (Unit* target = GetExplTargetUnit())
        {
            if (SpellInfo const* triggeredSpellInfo = sSpellMgr->GetSpellInfo(GetSpellInfo()->GetEffect(EFFECT_0).TriggerSpell))
            {
                if (Player* player = target->ToPlayer())
                {
                    if (player->HasItemCount(triggeredSpellInfo->Reagent[0]))
                    {
                        return SPELL_CAST_OK;
                    }
                }
            }
        }

        return SPELL_FAILED_DONT_REPORT;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_brewfest_relay_race_force_cast::CheckItem);
    }
};

class brewfest_commandscript : public CommandScript
{
public:
    brewfest_commandscript() : CommandScript("brewfest_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable brewfestChatCommandTable =
        {
            { "sm",          HandleBrewfestStartMoleEventCommand,               rbac::RBAC_PERM_COMMAND_EVENT_START,                Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "brewfest", brewfestChatCommandTable },
        };
        return commandTable;
    }

    static bool HandleBrewfestStartMoleEventCommand(ChatHandler* handler)
    {
        if (Creature* generator = handler->GetPlayer()->FindNearestCreature(NPC_EVENT_GENERATOR, 150.f))
            if (generator->IsAIEnabled())
            {
                generator->AI()->DoAction(99);
                return true;
            }

        return false;
    }
};

class spell_custom_kill_npcs_42655 : public SpellScript
{
    PrepareSpellScript(spell_custom_kill_npcs_42655);

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Player* caster = GetCaster()->ToPlayer();
        if (!caster)
            return;

        float radius = 15.0f;
        std::list<Creature*> creatureList;

        CellCoord pair(Trinity::ComputeCellCoord(caster->GetPositionX(), caster->GetPositionY()));
        Cell cell(pair);
        cell.SetNoCreate();

        Trinity::AnyUnitInObjectRangeCheck check(caster, radius);
        Trinity::CreatureListSearcher<Trinity::AnyUnitInObjectRangeCheck> searcher(caster, creatureList, check);
        TypeContainerVisitor<decltype(searcher), GridTypeMapContainer> visitor(searcher);
        cell.Visit(pair, visitor, *caster->GetMap(), *caster, radius);

        for (Creature* creature : creatureList)
        {
            if (!creature->IsAlive() || creature->GetEntry() != NPC_DARK_IRON_GUZZLER)
                continue;

            // Spieler killt NPC
            Unit::Kill(caster, creature);

            // Report-Spell
            creature->CastSpell(creature, SPELL_REPORT_DEATH, true);
        }
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_custom_kill_npcs_42655::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_custom_knockback_dummy_42674 : public SpellScript
{
    PrepareSpellScript(spell_custom_knockback_dummy_42674);

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        float radius = 1.0f;               // Radius 1 Meter
        float knockback_distance = 10.0f;  // Knockback-Distanz 10 Meter
        float knockback_height = 10.0f;    // Knockback-H he 20 Meter

        Map* map = caster->GetMap();
        if (!map)
            return;

        auto const& playerList = map->GetPlayers();

        for (auto itr = playerList.begin(); itr != playerList.end(); ++itr)
        {
            Player* player = itr->GetSource();
            if (!player || !player->IsAlive())
                continue;

            if (player->IsWithinDistInMap(caster, radius))
                player->KnockbackFrom(caster->GetPositionX(), caster->GetPositionY(), knockback_distance, knockback_height);
        }
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_custom_knockback_dummy_42674::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_42695_only_hit_23709 : public SpellScript
{
    PrepareSpellScript(spell_42695_only_hit_23709);

    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets.remove_if([](WorldObject* obj)
        {
            if (Creature* creature = obj->ToCreature())
                return creature->GetEntry() != NPC_DARK_IRON_GUZZLER; // Nur NPC 23709 behalten

            return true; // Spieler und andere Objekte entfernen
        });
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_42695_only_hit_23709::FilterTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ENTRY);
    }
};

class spell_brewfest_apple_trap_clean_aura : public AuraScript
{
    PrepareAuraScript(spell_brewfest_apple_trap_clean_aura);

    void OnApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Player* player = GetTarget()->ToPlayer())
        {
            if (player->HasAura(SPELL_RAM_FATIGUE))
            {
                player->RemoveAura(SPELL_RAM_FATIGUE);
                // TC_LOG_INFO("general", "AuraScript: Aura 43052 entfernt bei %s", player->GetName().c_str());
            }
        }
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_brewfest_apple_trap_clean_aura::OnApply, EFFECT_0, SPELL_AURA_FORCE_REACTION, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_brewfest_apple_trap_clean_loader : public SpellScriptLoader
{
public:
    spell_brewfest_apple_trap_clean_loader() : SpellScriptLoader("spell_brewfest_apple_trap_clean") { }

    AuraScript* GetAuraScript() const override
    {
        return new spell_brewfest_apple_trap_clean_aura();
    }
};


class brewfest_playerscript : public PlayerScript
{
public:
    brewfest_playerscript() : PlayerScript("brewfest_playerscript") { }

    void OnUpdateArea(Player* player, uint32 oldArea, uint32 newArea) override
    {
        if (oldArea == newArea)
            return;

        const uint32 targetPlayerArea = player->GetTeam() == HORDE ? AREA_ROCKTUSK_FARM_ID : AREA_DUN_MOROGH_ID;
        const uint32 state = targetPlayerArea == AREA_ROCKTUSK_FARM_ID
            ? WORLDSTATE_BREWFEST_RF_NEXT_EVENT_UI
            : WORLDSTATE_BREWFEST_DM_NEXT_EVENT_UI;

        // Spieler verl sst Brewfest-Gebiet
        if (oldArea == targetPlayerArea && newArea != targetPlayerArea)
        {
            player->SendInitWorldStates(player->GetZoneId(), newArea);
            player->SendUpdateWorldState(state, 0);
            player->SendUpdateWorldState(targetPlayerArea == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_DWARVES_KILLED_UI : WORLDSTATE_BREWFEST_DM_DWARVES_KILLED_UI, 0);
            player->SendUpdateWorldState(targetPlayerArea == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_KEGS_EMPTIED_UI : WORLDSTATE_BREWFEST_DM_KEGS_EMPTIED_UI, 0);
            return;
        }

        // Spieler betritt Brewfest-Gebiet
        if (newArea == targetPlayerArea)
        {
            player->SendInitWorldStates(player->GetZoneId(), newArea);

            if (sGameEventMgr->IsActiveEvent(24) && BrewfestStartTimer && sWorld->getWorldState(state))
                player->SendUpdateWorldState(state, 1);
            else
            {
                player->SendUpdateWorldState(state, 0); // Deaktivieren, wenn Event aus
                player->SendUpdateWorldState(targetPlayerArea == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_DWARVES_KILLED_UI : WORLDSTATE_BREWFEST_DM_DWARVES_KILLED_UI, 0);
                player->SendUpdateWorldState(targetPlayerArea == AREA_ROCKTUSK_FARM_ID ? WORLDSTATE_BREWFEST_RF_KEGS_EMPTIED_UI : WORLDSTATE_BREWFEST_DM_KEGS_EMPTIED_UI, 0);
            }
        }
    }
};

// --- WorldScript ---

class brewfest_worldscript : public WorldScript
{
public:
    brewfest_worldscript() : WorldScript("brewfest_worldscript") { }

    void OnUpdate(uint32 diff) override
    {
        static uint32 checkTimer = 0;
        checkTimer += diff;

        if (checkTimer < 3000) // alle 3 Sekunden pr fen
            return;

        checkTimer = 0;

        for (auto const& sessionPair : sWorld->GetAllSessions())
        {
            WorldSession* session = sessionPair.second;
            if (!session)
                continue;

            Player* player = session->GetPlayer();
            if (!player || !player->IsInWorld())
                continue;

            uint32 area = player->GetAreaId();
            uint32 team = player->GetTeam();

            uint32 targetArea = team == HORDE ? AREA_ROCKTUSK_FARM_ID : AREA_DUN_MOROGH_ID;
            uint32 stateNextEvent = (targetArea == AREA_ROCKTUSK_FARM_ID)
                ? WORLDSTATE_BREWFEST_RF_NEXT_EVENT_UI
                : WORLDSTATE_BREWFEST_DM_NEXT_EVENT_UI;

            uint32 stateKills = (targetArea == AREA_ROCKTUSK_FARM_ID)
                ? WORLDSTATE_BREWFEST_RF_DWARVES_KILLED_UI
                : WORLDSTATE_BREWFEST_DM_DWARVES_KILLED_UI;

            uint32 stateKegs = (targetArea == AREA_ROCKTUSK_FARM_ID)
                ? WORLDSTATE_BREWFEST_RF_KEGS_EMPTIED_UI
                : WORLDSTATE_BREWFEST_DM_KEGS_EMPTIED_UI;

            // Spieler steht im Brewfest-Gebiet, aber Event wurde gestoppt
            if (area == targetArea && !sGameEventMgr->IsActiveEvent(24))
            {
                player->SendUpdateWorldState(stateNextEvent, 0);
                player->SendUpdateWorldState(stateKills, 0);
                player->SendUpdateWorldState(stateKegs, 0);
            }
        }
    }
};

enum BartlettsBitterBrew
{
    SPELL_BOTM_VOMIT_BREW_VOMIT_VISUAL    = 49867
};


// 49869 - Nauseous
class spell_brewfest_botm_nauseous : public AuraScript
{
    PrepareAuraScript(spell_brewfest_botm_nauseous);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BOTM_VOMIT_BREW_VOMIT_VISUAL });
    }

    void AfterRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->CastSpell(GetTarget(), SPELL_BOTM_VOMIT_BREW_VOMIT_VISUAL, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_brewfest_botm_nauseous::AfterRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

enum DraenicPaleAle
{
    SPELL_BOTM_PINK_ELEKK    = 49908
};


// 42264 - Weak Alcohol
class spell_brewfest_botm_pink_elekk : public SpellScript
{
    PrepareSpellScript(spell_brewfest_botm_pink_elekk);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BOTM_PINK_ELEKK });
    }

    void HandleAfterCast()
    {
        // TODO: Needs additional research, this spell is most likely used if drunk state is high enough.
        if (roll_chance_i(50))
            GetCaster()->CastSpell(GetCaster(), SPELL_BOTM_PINK_ELEKK);
    }

    void Register() override
    {
        AfterCast += SpellCastFn(spell_brewfest_botm_pink_elekk::HandleAfterCast);
    }
};

class quest_fassabgabe : public PlayerScript
{
public:
    quest_fassabgabe() : PlayerScript("quest_fassabgabe") { }

    void OnQuestObjectiveProgress(Player* player, Quest const* quest, uint32 objectiveId, uint16 amount) override
    {
        if (!player || !quest)
            return;

        // Nur wenn Objective 0 fertig ist (z.?B. 3/3 Fässer)
        if (objectiveId != 0 || amount != 3)
            return;

        // Allianz
        if (player->GetTeam() == ALLIANCE && quest->GetQuestId() == QUEST_THERE_AND_BACK_AGAIN_A)
        {
            player->CastSpell(player, 43876, true);
        }
        // Horde
        else if (player->GetTeam() == HORDE && quest->GetQuestId() == QUEST_THERE_AND_BACK_AGAIN_H)
        {
            player->CastSpell(player, 43876, true);
        }
    }
};

void AddSC_event_brewfest()
{
    RegisterSpellScript(spell_brewfest_giddyup);
    RegisterSpellScript(spell_brewfest_relay_race_turn_in);
    RegisterSpellScript(spell_brewfest_dismount_ram);
    RegisterSpellScript(spell_brewfest_barker_bunny);
    RegisterSpellScript(spell_brewfest_mount_transformation);
    RegisterSpellScript(spell_brewfest_botm_the_beast_within);
    RegisterSpellScript(spell_brewfest_botm_gassy);
    RegisterSpellScript(spell_brewfest_botm_bloated);
    RegisterSpellScript(spell_brewfest_botm_internal_combustion);
    RegisterSpellScript(spell_brewfest_botm_jungle_madness);
    RegisterSpellScript(spell_brewfest_botm_teach_language);
    RegisterSpellScript(spell_brewfest_botm_weak_alcohol);
    RegisterSpellScript(spell_brewfest_botm_empty_bottle_throw_resolve);
    RegisterSpellScript(spell_brewfest_mole_machine_portal_schedule);

    // Npcs
    RegisterCreatureAI(npc_brewfest_keg_thrower);
    RegisterCreatureAI(npc_brewfest_keg_reciver);
    RegisterCreatureAI(npc_brewfest_bark_trigger);
    RegisterCreatureAI(npc_dark_iron_attack_generator);
    RegisterCreatureAI(npc_dark_iron_attack_mole_machine);
    RegisterCreatureAI(npc_dark_iron_guzzler);
    RegisterCreatureAI(npc_brewfest_super_brew_trigger);
    RegisterCreatureAI(npc_brewfest_announcer);

    // Spells

    RegisterSpellScript(spell_brewfest_botm_pink_elekk);
    RegisterSpellScript(spell_brewfest_botm_nauseous);
    new spell_ram_aura_handler();

    // ram
    RegisterSpellScript(spell_brewfest_main_ram_buff);
    RegisterSpellScript(spell_brewfest_ram_fatigue);
    RegisterSpellScript(spell_brewfest_apple_trap);

    // other
    RegisterSpellScript(spell_catch_the_wild_wolpertinger);
    RegisterSpellScript(spell_brewfest_fill_keg);
    RegisterSpellScript(spell_brewfest_unfill_keg);
    RegisterSpellScript(spell_brewfest_toss_mug);
    RegisterSpellScript(spell_brewfest_add_mug);
    RegisterSpellScript(spell_brewfest_reveler_transform);
    RegisterSpellScript(spell_brewfest_relay_race_force_cast);

    RegisterSpellScript(spell_custom_kill_npcs_42655);
    RegisterSpellScript(spell_custom_knockback_dummy_42674);
    RegisterSpellScript(spell_42695_only_hit_23709);

    // beer effect
    RegisterCreatureAI(npc_brew_bubble);

    new brewfest_commandscript();
    new spell_brewfest_apple_trap_clean_loader();
    new brewfest_playerscript();
    new brewfest_worldscript();
    new spell_throw_keg_player();
    new quest_fassabgabe();
}
