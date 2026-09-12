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
#include "Containers.h"
#include "CreatureAIImpl.h"
#include "GameObjectAI.h"
#include "GossipDef.h"
#include "GridNotifiers.h"
#include "Group.h"
#include "LFGMgr.h"
#include "ObjectMgr.h"
#include "PassiveAI.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SharedDefines.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "TaskScheduler.h"
#include "WeatherMgr.h"
#include "Weather.h"

enum TrickSpells
{
    SPELL_PIRATE_COSTUME_MALE           = 24708,
    SPELL_PIRATE_COSTUME_FEMALE         = 24709,
    SPELL_NINJA_COSTUME_MALE            = 24710,
    SPELL_NINJA_COSTUME_FEMALE          = 24711,
    SPELL_LEPER_GNOME_COSTUME_MALE      = 24712,
    SPELL_LEPER_GNOME_COSTUME_FEMALE    = 24713,
    SPELL_SKELETON_COSTUME              = 24723,
    SPELL_GHOST_COSTUME_MALE            = 24735,
    SPELL_GHOST_COSTUME_FEMALE          = 24736,
    SPELL_TRICK_BUFF                    = 24753,
};

enum TrickOrTreatSpells
{
    SPELL_TRICK                 = 24714,
    SPELL_TREAT                 = 24715,
    SPELL_TRICKED_OR_TREATED    = 24755,
    SPELL_UPSET_TUMMY           = 42966
};

enum HallowendData
{
    SPELL_HALLOWED_WAND_PIRATE             = 24717,
    SPELL_HALLOWED_WAND_NINJA              = 24718,
    SPELL_HALLOWED_WAND_LEPER_GNOME        = 24719,
    SPELL_HALLOWED_WAND_RANDOM             = 24720,
    SPELL_HALLOWED_WAND_SKELETON           = 24724,
    SPELL_HALLOWED_WAND_WISP               = 24733,
    SPELL_HALLOWED_WAND_GHOST              = 24737,
    SPELL_HALLOWED_WAND_BAT                = 24741
};

// 24717, 24718, 24719, 24720, 24724, 24733, 24737, 24741
class spell_hallow_end_wand : public SpellScript
{
    PrepareSpellScript(spell_hallow_end_wand);

    bool Validate(SpellInfo const* /*spellEntry*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_PIRATE_COSTUME_MALE,
            SPELL_PIRATE_COSTUME_FEMALE,
            SPELL_NINJA_COSTUME_MALE,
            SPELL_NINJA_COSTUME_FEMALE,
            SPELL_LEPER_GNOME_COSTUME_MALE,
            SPELL_LEPER_GNOME_COSTUME_FEMALE,
            SPELL_GHOST_COSTUME_MALE,
            SPELL_GHOST_COSTUME_FEMALE
        });
    }

    void HandleScriptEffect()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();

        uint32 spellId = 0;
        uint8 gender = target->GetNativeGender();

        switch (GetSpellInfo()->Id)
        {
            case SPELL_HALLOWED_WAND_LEPER_GNOME:
                spellId = gender ? SPELL_LEPER_GNOME_COSTUME_FEMALE : SPELL_LEPER_GNOME_COSTUME_MALE;
                break;
            case SPELL_HALLOWED_WAND_PIRATE:
                spellId = gender ? SPELL_PIRATE_COSTUME_FEMALE : SPELL_PIRATE_COSTUME_MALE;
                break;
            case SPELL_HALLOWED_WAND_GHOST:
                spellId = gender ? SPELL_GHOST_COSTUME_FEMALE : SPELL_GHOST_COSTUME_MALE;
                break;
            case SPELL_HALLOWED_WAND_NINJA:
                spellId = gender ? SPELL_NINJA_COSTUME_FEMALE : SPELL_NINJA_COSTUME_MALE;
                break;
            case SPELL_HALLOWED_WAND_RANDOM:
                spellId = RAND(SPELL_HALLOWED_WAND_PIRATE, SPELL_HALLOWED_WAND_NINJA, SPELL_HALLOWED_WAND_LEPER_GNOME, SPELL_HALLOWED_WAND_SKELETON, SPELL_HALLOWED_WAND_WISP, SPELL_HALLOWED_WAND_GHOST, SPELL_HALLOWED_WAND_BAT);
                break;
            default:
                return;
        }
        caster->CastSpell(target, spellId, true);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_hallow_end_wand::HandleScriptEffect);
    }
};

///////////////////////////////////////
////// ITEMS FIXES, BASIC STUFF
///////////////////////////////////////

enum eTrickSpells
{
    SPELL_BAT_COSTUME                   = 24732,
    SPELL_WHISP_COSTUME                 = 24740,
};

class spell_hallows_end_trick : public SpellScript
{
    PrepareSpellScript(spell_hallows_end_trick);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Player* target = GetHitPlayer())
        {
            uint8 gender = target->GetGender();
            uint32 spellId = SPELL_TRICK_BUFF;
            switch (urand(0, 7))
            {
            case 1:
                spellId = gender ? SPELL_LEPER_GNOME_COSTUME_FEMALE : SPELL_LEPER_GNOME_COSTUME_MALE;
                break;
            case 2:
                spellId = gender ? SPELL_PIRATE_COSTUME_FEMALE : SPELL_PIRATE_COSTUME_MALE;
                break;
            case 3:
                spellId = gender ? SPELL_GHOST_COSTUME_FEMALE : SPELL_GHOST_COSTUME_MALE;
                break;
            case 4:
                spellId = gender ? SPELL_NINJA_COSTUME_FEMALE : SPELL_NINJA_COSTUME_MALE;
                break;
            case 5:
                spellId = SPELL_SKELETON_COSTUME;
                break;
            case 6:
                spellId = SPELL_BAT_COSTUME;
                break;
            case 7:
                spellId = SPELL_WHISP_COSTUME;
                break;
            default:
                break;
            }

            GetCaster()->CastSpell(target, spellId, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hallows_end_trick::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class spell_hallows_end_put_costume : public SpellScript
{
public:
    spell_hallows_end_put_costume(uint32 maleSpell, uint32 femaleSpell) : _maleSpell(maleSpell), _femaleSpell(femaleSpell) { }

    PrepareSpellScript(spell_hallows_end_put_costume);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Player* target = GetHitPlayer())
            GetCaster()->CastSpell(target, target->GetGender() ? _femaleSpell : _maleSpell, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hallows_end_put_costume::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }

private:
    uint32 _maleSpell;
    uint32 _femaleSpell;
};

class spell_hallows_end_trick_or_treat : public SpellScript
{
    PrepareSpellScript(spell_hallows_end_trick_or_treat);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Player* target = GetHitPlayer())
        {
            GetCaster()->CastSpell(target, roll_chance_i(50) ? SPELL_TRICK : SPELL_TREAT, true);
            GetCaster()->CastSpell(target, SPELL_TRICKED_OR_TREATED, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hallows_end_trick_or_treat::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

enum eHallowsEndCandy
{
    SPELL_HALLOWS_END_CANDY_1               = 24924,
    SPELL_HALLOWS_END_CANDY_2               = 24925,
    SPELL_HALLOWS_END_CANDY_3               = 24926,
    SPELL_HALLOWS_END_CANDY_3_FEMALE        = 44742,
    SPELL_HALLOWS_END_CANDY_3_MALE          = 44743,
    SPELL_HALLOWS_END_CANDY_4               = 24927
};

class spell_hallows_end_candy : public SpellScript
{
    PrepareSpellScript(spell_hallows_end_candy);

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (Player* target = GetHitPlayer())
        {
            uint32 spellId = SPELL_HALLOWS_END_CANDY_1 + urand(0, 3);
            GetCaster()->CastSpell(target, spellId, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hallows_end_candy::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_hallows_end_candy_pirate_costume : public AuraScript
{
    PrepareAuraScript(spell_hallows_end_candy_pirate_costume);

    void HandleEffectApply(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Unit* target = GetTarget())
        {
            target->CastSpell(target, target->GetGender() == GENDER_MALE ? SPELL_HALLOWS_END_CANDY_3_MALE : SPELL_HALLOWS_END_CANDY_3_FEMALE, true);
        }
    }

    void HandleEffectRemove(AuraEffect const*  /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (Unit* target = GetTarget())
        {
            target->RemoveAurasDueToSpell(SPELL_HALLOWS_END_CANDY_3_MALE);
            target->RemoveAurasDueToSpell(SPELL_HALLOWS_END_CANDY_3_FEMALE);
        }
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_hallows_end_candy_pirate_costume::HandleEffectApply, EFFECT_0, SPELL_AURA_MOD_INCREASE_SWIM_SPEED, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_hallows_end_candy_pirate_costume::HandleEffectRemove, EFFECT_0, SPELL_AURA_MOD_INCREASE_SWIM_SPEED, AURA_EFFECT_HANDLE_REAL);
    }
};

class spell_hallows_end_tricky_treat : public SpellScript
{
    PrepareSpellScript(spell_hallows_end_tricky_treat);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Player* target = GetHitPlayer())
        {
            if (roll_chance_i(20))
                target->CastSpell(target, SPELL_UPSET_TUMMY, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hallows_end_tricky_treat::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

///////////////////////////////////////
////// SHADE OF THE HORSEMAN EVENT
///////////////////////////////////////

enum costumedOrphan
{
    // Quests
    QUEST_LET_THE_FIRES_COME_A              = 12135,
    QUEST_LET_THE_FIRES_COME_H              = 12139,
    QUEST_STOP_THE_FIRES_A                  = 11131,
    QUEST_STOP_THE_FIRES_H                  = 11219,

    // Spells
    SPELL_HORSEMAN_MOUNT                    = 48025,
    SPELL_FIRE_AURA_BASE                    = 42075,
    SPELL_START_FIRE                        = 42132,
    SPELL_SPREAD_FIRE                       = 42079,
    SPELL_CREATE_BUCKET                     = 42349,
    SPELL_WATER_SPLASH                      = 42348,
    SPELL_SUMMON_LANTERN                    = 44231,
    SPELL_HORSEMAN_CONFLAGRATION            = 42380,
    SPELL_HORSEMAN_CONFLAGRATION_SOUND      = 48149,
    SPELL_HORSEMAN_CLEAVE                   = 42587,
    SPELL_FIRE_BUNNY_BASE            	    = 43184, // Cosmetic Base
    SPELL_FIRE_VISUAL_BIG		    = 43148, // gro er visueller Effekt

    // NPCs
    NPC_SHADE_OF_HORSEMAN                   = 23543,
    NPC_FIRE_TRIGGER                        = 101000,
    NPC_ALLIANCE_MATRON                     = 24519,
    NPC_HORDE_MATRON                        = 23973,

    // Actions
    ACTION_START_EVENT                      = 1,
    ACTION_RESET_EVENT                      = 2,

    // Talks
    TALK_SHADE_CONFLAGRATION                = 0,
    TALK_SHADE_PREPARE                      = 1,
    TALK_SHADE_START_EVENT                  = 2,
    TALK_SHADE_MORE_FIRES                   = 3,
    TALK_SHADE_FAILED                       = 4,
    TALK_SHADE_DEFEATED                     = 5,
    TALK_SHADE_DEATH                        = 6,

    // Areas
    AREA_GOLDSHIRE          = 87,
    AREA_KHARANOS           = 131,
    AREA_AZURE_WATCH        = 3576,
    AREA_RAZOR_HILL         = 362,
    AREA_BRILL              = 159,
    AREA_FALCONWING_SQUARE  = 3665,

    // World States
    WORLDSTATE_HALLOWS_END_GS_ACTIVE_FIRES_UI       = 5108,
    WORLDSTATE_HALLOWS_END_KH_ACTIVE_FIRES_UI       = 5110,
    WORLDSTATE_HALLOWS_END_AW_ACTIVE_FIRES_UI       = 5112,
    WORLDSTATE_HALLOWS_END_RH_ACTIVE_FIRES_UI       = 5114,
    WORLDSTATE_HALLOWS_END_BR_ACTIVE_FIRES_UI       = 5116,
    WORLDSTATE_HALLOWS_END_FS_ACTIVE_FIRES_UI       = 5118,

    WORLDSTATE_HALLOWS_END_GS_ACTIVE_FIRES          = 5109,
    WORLDSTATE_HALLOWS_END_KH_ACTIVE_FIRES          = 5111,
    WORLDSTATE_HALLOWS_END_AW_ACTIVE_FIRES          = 5113,
    WORLDSTATE_HALLOWS_END_RH_ACTIVE_FIRES          = 5115,
    WORLDSTATE_HALLOWS_END_BR_ACTIVE_FIRES          = 5117,
    WORLDSTATE_HALLOWS_END_FS_ACTIVE_FIRES          = 5119,

    /*

    AREA_GOLDSHIRE = 87,
    AREA_KHARANOS = 131,
    AREA_AZURE_WATCH = 3576,
    AREA_RAZOR_HILL = 362,
    AREA_BRILL = 159,
    AREA_FALCONWING_SQUARE = 3665,

    */

    GAMEOBJECT_JACK_O_LANTERN = 186887,
};

class spell_hallows_end_bucket_lands : public SpellScript
{
    PrepareSpellScript(spell_hallows_end_bucket_lands);

    bool handled;
    bool Load() override { handled = false; return true; }
    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        if (handled || !GetCaster())
            return;

        handled = true;
        if (Player* target = GetHitPlayer())
            GetCaster()->CastSpell(target, SPELL_CREATE_BUCKET, true);
        else if (Unit* tgt = GetHitUnit())
            GetCaster()->CastSpell(tgt, SPELL_WATER_SPLASH, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hallows_end_bucket_lands::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_hallows_end_base_fire : public AuraScript
{
    PrepareAuraScript(spell_hallows_end_base_fire);

    void CalcPeriodic(AuraEffect const* /*aurEff*/, bool& isPeriodic, int32& amplitude)
    {
        // Blockiere periodisches Verhalten f r Trigger
        if (GetUnitOwner()->GetEntry() == NPC_FIRE_TRIGGER)
        {
            isPeriodic = false;
            amplitude = 0;
        }
        else
        {
            if (Creature* creature = GetCaster()->ToCreature())
            {
                if (!(creature->AI()->GetData(0) % 3))
                    amplitude = static_cast<int32>(amplitude * 1.5f);
            }
        }
    }

    void HandleEffectPeriodicUpdate(AuraEffect* aurEff)
    {
        Unit* owner = GetUnitOwner();
        if (!owner)
            return;

        if (owner->GetEntry() == NPC_FIRE_TRIGGER)
            return; // Trigger: keine Skalierung, kein Spread

        int32 amount = aurEff->GetAmount();

        if (amount < 3)
            ++amount;
        else if (aurEff->GetTickNumber() % 3 != 2)
            return;

        aurEff->SetAmount(amount);

        if (amount <= 3)
            owner->SetObjectScale(amount / 2.0f);

        if (amount >= 3)
            owner->CastSpell(owner, SPELL_SPREAD_FIRE, true);
    }

    void HandleEffectApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();

        if (target->GetEntry() == NPC_FIRE_TRIGGER)
        {
            target->SetObjectScale(6.0f); // Einmalige feste Gr  e
            return;
        }

        if (AuraEffect* aEff = GetEffect(EFFECT_0))
            aEff->SetAmount(1);
    }

    void Register() override
    {
        DoEffectCalcPeriodic += AuraEffectCalcPeriodicFn(spell_hallows_end_base_fire::CalcPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        OnEffectUpdatePeriodic += AuraEffectUpdatePeriodicFn(spell_hallows_end_base_fire::HandleEffectPeriodicUpdate, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
        OnEffectApply += AuraEffectApplyFn(spell_hallows_end_base_fire::HandleEffectApply, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

struct npc_costumed_orphan_matron : public ScriptedAI
{
    npc_costumed_orphan_matron(Creature* c) : ScriptedAI(c) {}

    void Reset() override
    {
        allowStart = true;
        events.Reset();
        horseGUID.Clear();
    }

    void GetInitXYZ(float& x, float& y, float& z, float& o, uint32& path)
    {
        switch (me->GetAreaId())
        {
        case AREA_GOLDSHIRE:
            x = -9494.4f;
            y = 48.53f;
            z = 70.5f;
            o = 0.5f;
            path = 1883448;
            break;
        case AREA_KHARANOS:
            x = -5558.34f;
            y = -499.46f;
            z = 414.12f;
            o = 2.08f;
            path = 1883456;
            break;
        case AREA_AZURE_WATCH:
            x = -4163.58f;
            y = -12460.30f;
            z = 63.02f;
            o = 4.31f;
            path = 1883464;
            break;
        case AREA_RAZOR_HILL:
            x = 373.2f;
            y = -4723.4f;
            z = 31.2f;
            o = 3.2f;
            path = 1883472;
            break;
        case AREA_BRILL:
            x = 2195.2f;
            y = 264.0f;
            z = 55.62f;
            o = 0.15f;
            path = 1883480;
            break;
        case AREA_FALCONWING_SQUARE:
            x = 9547.91f;
            y = -6809.9f;
            z = 27.96f;
            o = 3.4f;
            path = 1883488;
            break;
        }
    }

    void DoAction(int32 action) override
    {
        if (action == ACTION_START_EVENT)
        {
            allowStart = false;
            float x = 0, y = 0, z = 0, o = 0;
            uint32 path = 0;
            GetInitXYZ(x, y, z, o, path);
            if (Creature* cr = me->SummonCreature(NPC_SHADE_OF_HORSEMAN, x, y, z, o, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 10000ms))
            {
                cr->GetMotionMaster()->MovePath(path, true);
                cr->AI()->DoAction(path);
                horseGUID = cr->GetGUID();
            }
        }
        else if (action == ACTION_RESET_EVENT)
        {
            allowStart = true;
            events.ScheduleEvent(EVENT_RESET_WEATHER, 3s);
        }
    }

    bool HasAnyRequiredQuest(Player* player) const
    {
        static const uint32 requiredQuests[] = {
            QUEST_LET_THE_FIRES_COME_A,
            QUEST_LET_THE_FIRES_COME_H,
            QUEST_STOP_THE_FIRES_A,
            QUEST_STOP_THE_FIRES_H
        };

        for (uint32 questId : requiredQuests)
        {
            QuestStatus qs = player->GetQuestStatus(questId);
            if (qs == QUEST_STATUS_INCOMPLETE || qs == QUEST_STATUS_COMPLETE)
                return true;
        }
        return false;
    }

    bool OnGossipHello(Player* player) override
{
    bool isGM = player->IsGameMaster();
    bool isEventAllowed = allowStart; // true = Event kann gestartet werden
    bool isPumpkinNearby = me->FindNearestGameObject(GAMEOBJECT_JACK_O_LANTERN, 150.f) != nullptr;
    bool hasQuest = HasAnyRequiredQuest(player);

    if (isGM || (hasQuest && isEventAllowed && !isPumpkinNearby))
    {
        BroadcastTextEntry const* bText = sObjectMgr->GetBroadcastText(BROADCAST_TEXT_START_EVENT);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT,
            bText ? bText->GetText(sObjectMgr->GetDBCLocaleIndex(), player->GetGender()) : "Start Event",
            GOSSIP_SENDER_MAIN, GOSSIP_ACTION_BATTLE);
    }

    player->PrepareQuestMenu(me->GetGUID());
    SendGossipMenuFor(player, player->GetGossipTextId(me), me);
    return true;
}


    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = GetGossipActionFor(player, gossipListId);
        ClearGossipMenuFor(player);

        if (action == GOSSIP_ACTION_BATTLE)
            DoAction(ACTION_START_EVENT);

        CloseGossipMenuFor(player);
        return true;
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);

        if (uint32 eventId = events.ExecuteEvent())
            if (eventId == EVENT_RESET_WEATHER)
              //me->GetMap()->GetOrGenerateZoneDefaultWeather(me->GetZoneId());
	        me->GetMap()->SetZoneWeather(me->GetZoneId(), WEATHER_STATE_FINE, 100.f);
    }

private:
    EventMap events;
    ObjectGuid horseGUID;
    bool allowStart;

    enum Assets
    {
        BROADCAST_TEXT_START_EVENT = 200004,

        EVENT_RESET_WEATHER = 1,
    };
};

class npc_soh_fire_trigger : public CreatureScript
{
public:
    npc_soh_fire_trigger() : CreatureScript("npc_soh_fire_trigger") { }

    struct npc_soh_fire_triggerAI : public ScriptedAI
    {
        npc_soh_fire_triggerAI(Creature* creature) : ScriptedAI(creature) { }

        void Reset() override
        {
            me->SetDisableGravity(true);
            me->SetObjectScale(1.3f); // feste gro e Flamme beim Spawn
        }

        void SpellHit(WorldObject* /*caster*/, SpellInfo const* spellInfo) override
        {
            switch (spellInfo->Id)
            {
                case SPELL_START_FIRE:
                    me->RemoveAllAuras();
                    me->CastSpell(me, SPELL_FIRE_AURA_BASE, true);
                    me->SetObjectScale(1.3f); // direkt gro 
                    break;

                case SPELL_WATER_SPLASH:
                    me->RemoveAllAuras();
                    me->SetObjectScale(1.0f); // bei L schung zur cksetzen
                    break;
            }
        }

        void UpdateAI(uint32 /*diff*/) override
        {
            if (me->HasAura(SPELL_FIRE_AURA_BASE))
                me->SetObjectScale(1.3f); // gegen DBC-Resets absichern
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_soh_fire_triggerAI(creature);
    }
};


struct npc_hallows_end_soh : public ScriptedAI
{
    npc_hallows_end_soh(Creature* creature) : ScriptedAI(creature)
    {
        m_WorldState = 0;
        pos = 0;
        counter = 0;
        _toFireTargets.clear();
        unitList.clear();
        me->CastSpell(me, SPELL_HORSEMAN_MOUNT, true);
        me->SetSpeed(MOVE_WALK, 3.0f);
    }

    EventMap events;
    uint32 playerCount;
    uint32 counter;
    GuidList unitList;
    int32 pos;
    TaskScheduler scheduler;

    void JustEngagedWith(Unit*) override
    {
        scheduler.Schedule(6s, [this](TaskContext context)
        {
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 30.f, true))
            {
                me->CastSpell(target, SPELL_HORSEMAN_CONFLAGRATION, false);
                target->CastSpell(target, SPELL_HORSEMAN_CONFLAGRATION_SOUND, true);
                Talk(TALK_SHADE_CONFLAGRATION);
            }

            context.Repeat(12s);
        })
            .Schedule(7s, [this](TaskContext context)
        {
            DoCastVictim(SPELL_HORSEMAN_CLEAVE, true);

            context.Repeat(8s);
        });
    }

    void MoveInLineOfSight(Unit*  /*who*/) override {}

    void DoAction(int32 param) override
    {
        pos = param;
    }

    void GetPosToLand(float& x, float& y, float& z)
    {
        switch (pos)
        {
        case 1883448:
            x = -9445.1f;
            y = 63.27f;
            z = 58.16f;
            break;
        case 1883456:
            x = -5616.30f;
            y = -481.89f;
            z = 398.99f;
            break;
        case 1883464:
            x = -4198.1f;
            y = -12509.13f;
            z = 46.6f;
            break;
        case 1883472:
            x = 360.9f;
            y = -4735.5f;
            z = 11.773f;
            break;
        case 1883480:
            x = 2229.4f;
            y = 263.1f;
            z = 36.13f;
            break;
        case 1883488:
            x = 9532.9f;
            y = -6833.8f;
            z = 18.5f;
            break;
        default:
            x = 0;
            y = 0;
            z = 0;
            break;
        }
    }

    void Reset() override
    {
        playerCount = 0;
        _toFireTargets.clear();
        unitList.clear();
        std::list<Creature*> temp;
        me->GetCreatureListWithEntryInGrid(temp, NPC_FIRE_TRIGGER, 150.f);
        for (std::list<Creature*>::const_iterator itr = temp.begin(); itr != temp.end(); ++itr)
            unitList.push_back((*itr)->GetGUID());

        m_WorldState = GetWorldState(me->GetAreaId());
        me->GetMap()->SetZoneWeather(me->GetZoneId(), WEATHER_STATE_HEAVY_RAIN, 100.f);

        events.Reset();
        events.ScheduleEvent(1, 3s);
        events.ScheduleEvent(2, 20s);
        events.ScheduleEvent(2, 38s);
        events.ScheduleEvent(3, 58s);
        events.ScheduleEvent(5, 19s);
        events.ScheduleEvent(6, 22s);

        me->SetReactState(REACT_PASSIVE);
        me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);

        me->SetCanFly(true);
        me->SetDisableGravity(true);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        me->DespawnOrUnsummon(1s);
    }

    uint32 GetData(uint32 /*type*/) const override
    {
        return playerCount;
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        switch (events.ExecuteEvent())
        {
        case 1:
            Talk(TALK_SHADE_PREPARE);
            break;
        case 2:
        {
            CastFires(true);
            break;
        }
        case 3:
        {
            bool checkBurningTriggers = false;
            for (ObjectGuid const& guid : unitList)
                if (Unit* c = ObjectAccessor::GetUnit(*me, guid))
                    if (c->HasAuraType(SPELL_AURA_PERIODIC_DUMMY))
                    {
                        checkBurningTriggers = true;
                        break;
                    }

            if (!checkBurningTriggers)
            {
                FinishEvent(false);
                return;
            }

            counter++;
            if (counter > 21)
            {
                bool failed = false;
                for (ObjectGuid const& guid : unitList)
                    if (Unit* c = ObjectAccessor::GetUnit(*me, guid))
                        if (c->HasAuraType(SPELL_AURA_PERIODIC_DUMMY))
                        {
                            failed = true;
                            break;
                        }
                FinishEvent(failed);
                return;
            }
            if (counter == 5)
            {
                Talk(TALK_SHADE_START_EVENT);
            }
            else if (counter == 15)
            {
                Talk(TALK_SHADE_MORE_FIRES);
            }

            CastFires(false);
            events.Repeat(15000ms);
            break;
        }
        case 4:
        {
            me->SetCanFly(false);
            me->m_movementInfo.RemoveMovementFlag(MOVEMENTFLAG_HOVER | MOVEMENTFLAG_CAN_FLY);
            me->SetDisableGravity(false);
            me->ReplaceAllUnitFlags(UnitFlags(0x00000000));
            me->SetReactState(REACT_AGGRESSIVE);
            if (Unit* target = me->SelectNearestPlayer(30.0f))
                AttackStart(target);
            break;
        }
        case 5:
        {
            std::list<Player*> players;
            me->GetPlayerListInGrid(players, 100.f);
            for (Player* player : players)
                player->SendUpdateWorldState(m_WorldState, 1);
            break;
        }
        case 6:
        {
            GetListOfAvailableFireTargets();
            const uint32 fireCount = GetFiresCount();

            std::list<Player*> players;
            me->GetPlayerListInGrid(players, 100.f);
            for (Player* player : players)
                player->SendUpdateWorldState(m_WorldState + 1, fireCount);

            events.Repeat(2s);
            break;
        }
        }

        if (!UpdateVictim())
            return;

        scheduler.Update(diff, [this]
        {
            DoMeleeAttackIfReady();
        });
    }

    void CastFires(bool initial)
    {
        std::list<Player*> players;
        Trinity::AnyPlayerInObjectRangeCheck checker(me, 60.f);
        Trinity::PlayerListSearcher<Trinity::AnyPlayerInObjectRangeCheck> searcher(me, players, checker);
        Cell::VisitWorldObjects(me, searcher, 60.f);
        if (players.empty())
            return;

        GetListOfAvailableFireTargets();
        std::list<Unit*> tmpList;
        tmpList.insert(tmpList.begin(), _toFireTargets.begin(), _toFireTargets.end());

        if (tmpList.empty())
            return;

        playerCount = static_cast<uint32>(players.size()) - 1;

        if (!initial)
        {
            float playerRate = std::max(0.f, 0.5f - playerCount * 0.25f);

            // If there are more burning triggers than players, do not cast next fire
            if (tmpList.size() < unitList.size() * playerRate)
                return;
        }
        else
            playerCount += 1;

        uint32 sizeCount = (playerCount / 3) + 1;
        if (initial && playerCount > 0)
            sizeCount += playerCount % 2;

        Trinity::Containers::RandomResize(tmpList, sizeCount);
        for (Unit* trigger : tmpList)
            me->CastSpell(trigger, SPELL_START_FIRE, true);
    }

    void FinishEvent(bool failed)
    {
        events.CancelEvent(6);

        std::list<Player*> players;
        me->GetPlayerListInGrid(players, 100.f);
        for (Player* player : players)
            player->SendUpdateWorldState(m_WorldState, 0);

        me->GetMap()->GetOrGenerateZoneDefaultWeather(me->GetZoneId());

        if (failed)
        {
            Talk(TALK_SHADE_FAILED);
            for (ObjectGuid const& guid : unitList)
                if (Unit* c = ObjectAccessor::GetUnit(*me, guid))
                    c->RemoveAllAuras();

            me->DespawnOrUnsummon(1s);
        }
        else
        {
            Talk(TALK_SHADE_DEFEATED);
            float x, y, z;
            GetPosToLand(x, y, z);
            me->GetMotionMaster()->Clear();
            me->GetMotionMaster()->MoveIdle();
            me->GetMotionMaster()->MovePoint(8, x, y, z);
        }
    }

    void MovementInform(uint32 type, uint32 point) override
    {
        if (type == POINT_MOTION_TYPE && point == 8)
        {
            me->RemoveAllAuras();
            me->SetCanFly(false);
            me->m_movementInfo.RemoveMovementFlag(MOVEMENTFLAG_HOVER | MOVEMENTFLAG_CAN_FLY);
            me->SetDisableGravity(false);
            events.ScheduleEvent(4, 2000ms);
        }
    }

    void JustDied(Unit*  /*killer*/) override
    {
        me->GetMap()->GetOrGenerateZoneDefaultWeather(me->GetZoneId());
        Talk(TALK_SHADE_DEATH);
        float x, y, z;
        GetPosToLand(x, y, z);
        Position pos(x, y, z, me->GetOrientation());
        CastSpellTargetArg targets(pos);
        me->CastSpell(targets, SPELL_SUMMON_LANTERN, true);
        CompleteQuest();

        if (Creature* matron = GetMatronFor(me->GetAreaId()))
            if (matron->IsAIEnabled())
                matron->AI()->DoAction(ACTION_RESET_EVENT);
    }

    void CompleteQuest()
    {
        float radius = 100.0f;
        std::list<Player*> players;
        Trinity::AnyPlayerInObjectRangeCheck checker(me, radius);
        Trinity::PlayerListSearcher<Trinity::AnyPlayerInObjectRangeCheck> searcher(me, players, checker);
        Cell::VisitWorldObjects(me, searcher, radius);

        for (Player* player : players)
        {
            player->AreaExploredOrEventHappens(QUEST_STOP_THE_FIRES_H);
            player->AreaExploredOrEventHappens(QUEST_STOP_THE_FIRES_A);
            player->AreaExploredOrEventHappens(QUEST_LET_THE_FIRES_COME_H);
            player->AreaExploredOrEventHappens(QUEST_LET_THE_FIRES_COME_A);
        }
    }

private:
    uint32 m_WorldState;
    std::list<Unit*> _toFireTargets;

    uint32 GetFiresCount() { return uint32(unitList.size()) - uint32(_toFireTargets.size()); }

    void GetListOfAvailableFireTargets()
    {
        _toFireTargets.clear();

        for (ObjectGuid const& guid : unitList)
            if (Unit* c = ObjectAccessor::GetUnit(*me, guid))
                if (!c->HasAuraType(SPELL_AURA_PERIODIC_DUMMY))
                    _toFireTargets.push_back(c);
    }

    uint32 GetWorldState(uint32 areaId)
    {
        switch (areaId)
        {
        case AREA_GOLDSHIRE: return WORLDSTATE_HALLOWS_END_GS_ACTIVE_FIRES_UI;
        case AREA_KHARANOS: return WORLDSTATE_HALLOWS_END_KH_ACTIVE_FIRES_UI;
        case AREA_AZURE_WATCH: return WORLDSTATE_HALLOWS_END_AW_ACTIVE_FIRES_UI;
        case AREA_RAZOR_HILL: return WORLDSTATE_HALLOWS_END_RH_ACTIVE_FIRES_UI;
        case AREA_BRILL: return WORLDSTATE_HALLOWS_END_BR_ACTIVE_FIRES_UI;
        case AREA_FALCONWING_SQUARE: return WORLDSTATE_HALLOWS_END_FS_ACTIVE_FIRES_UI;
        }

        return 0;
    }

    Creature* GetMatronFor(const uint32 areaId)
    {
        switch (areaId)
        {
        case AREA_GOLDSHIRE: 
        case AREA_KHARANOS:
        case AREA_AZURE_WATCH: return me->FindNearestCreature(NPC_ALLIANCE_MATRON, 150.f);
        case AREA_BRILL:
        case AREA_FALCONWING_SQUARE:
        case AREA_RAZOR_HILL: return me->FindNearestCreature(NPC_HORDE_MATRON, 150.f);
        }

        return nullptr;
    }
};

struct npc_hallows_end_train_fire : public NullCreatureAI
{
    npc_hallows_end_train_fire(Creature* creature) : NullCreatureAI(creature) { }

    uint32 timer;
    void Reset() override
    {
        timer = 0;
    }

    void UpdateAI(uint32 diff) override
    {
        timer += diff;
        if (timer >= 5000)
            if (!me->GetAuraEffect(SPELL_FIRE_AURA_BASE, EFFECT_0))
                me->CastSpell(me, SPELL_FIRE_AURA_BASE, true);
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        if (spellInfo->Id == SPELL_WATER_SPLASH && caster->ToPlayer())
        {
            if (AuraEffect* aurEff = me->GetAuraEffect(SPELL_FIRE_AURA_BASE, EFFECT_0))
            {
                int32 amt = aurEff->GetAmount();
                if (amt > 1)
                    aurEff->SetAmount(amt - 1);
                else
                    me->RemoveAllAuras();

                caster->ToPlayer()->KilledMonsterCredit(me->GetEntry());
            }
        }
    }
};

enum TrickInitial
{
    SPELL_TRICK_INITIAL = 24750
};

// 24714 - Trick
class spell_hallow_end_trick_initial : public SpellScript
{
    PrepareSpellScript(spell_hallow_end_trick_initial);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_TRICK_INITIAL });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        GetHitUnit()->CastSpell(GetHitUnit(), SPELL_TRICK_INITIAL);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hallow_end_trick_initial::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

enum CreateWaterBucket
{
    SPELL_CREATE_WATER_BUCKET_BARREL_SPLASH = 43244,
    SPELL_JUST_LOOTED_WATER_BARREL = 44410
};

// 42144 - Headless Horseman - Create Water Bucket
class spell_hallow_end_create_water_bucket : public SpellScript
{
    PrepareSpellScript(spell_hallow_end_create_water_bucket);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CREATE_WATER_BUCKET_BARREL_SPLASH, SPELL_JUST_LOOTED_WATER_BARREL });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* target = GetHitUnit();
        target->CastSpell(target, SPELL_CREATE_WATER_BUCKET_BARREL_SPLASH);
        target->CastSpell(target, SPELL_JUST_LOOTED_WATER_BARREL);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hallow_end_create_water_bucket::HandleScript, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

void AddSC_event_hallows_end()
{
    RegisterSpellScript(spell_hallow_end_wand);

    // Spells
    RegisterSpellScript(spell_hallows_end_trick);
    RegisterSpellScript(spell_hallows_end_trick_or_treat);
    RegisterSpellScript(spell_hallows_end_candy);
    RegisterSpellScript(spell_hallows_end_candy_pirate_costume);
    RegisterSpellScript(spell_hallows_end_tricky_treat);
    RegisterSpellScriptWithArgs(spell_hallows_end_put_costume, "spell_hallows_end_pirate_costume", SPELL_PIRATE_COSTUME_MALE, SPELL_PIRATE_COSTUME_FEMALE);
    RegisterSpellScriptWithArgs(spell_hallows_end_put_costume, "spell_hallows_end_leper_costume", SPELL_LEPER_GNOME_COSTUME_MALE, SPELL_LEPER_GNOME_COSTUME_FEMALE);
    RegisterSpellScriptWithArgs(spell_hallows_end_put_costume, "spell_hallows_end_ghost_costume", SPELL_GHOST_COSTUME_MALE, SPELL_GHOST_COSTUME_FEMALE);
    RegisterSpellScriptWithArgs(spell_hallows_end_put_costume, "spell_hallows_end_ninja_costume", SPELL_NINJA_COSTUME_MALE, SPELL_NINJA_COSTUME_FEMALE);
    RegisterSpellScript(spell_hallows_end_base_fire);
    RegisterSpellScript(spell_hallows_end_bucket_lands);
    RegisterSpellScript(spell_hallow_end_create_water_bucket);
    RegisterSpellScript(spell_hallow_end_trick_initial);

    // Quests
    RegisterCreatureAI(npc_hallows_end_train_fire);

    // creatures
    RegisterCreatureAI(npc_costumed_orphan_matron);
    new npc_soh_fire_trigger();
    RegisterCreatureAI(npc_hallows_end_soh);
}
