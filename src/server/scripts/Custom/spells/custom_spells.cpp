#include "ScriptMgr.h"
#include "GridNotifiersImpl.h"
#include "Log.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "SpellAuras.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "SpellAuraEffects.h"
#include "Vehicle.h"
#include <random>
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "Containers.h"
#include "vector"


enum MyCustomSpells
{
    SPELL_ICY_GRIP_CUSTOM                     = 70117,
    SPELL_ICY_GRIP_JUMP_CUSTOM                = 70122,
    SPELL_FROST_BLAST_CUSTOM                  = 81116,
    SPELL_FROST_BLAST_DMG_CUSTOM              = 81117,
    SPELL_ICE_BURST_CUSTOM                    = 81260,
    SPELL_SHADOW_TRAP_AURA_CUSTOM             = 81268, //73525
    SPELL_SHADOW_TRAP_KNOCKBACK_CUSTOM        = 81269, //73529
    SPELL_WEB_WRAP_CUSTOM		              = 81360,
    SPELL_WEB_WRAP_WRAPPED_CUSTOM	          = 81361,
    SPELL_DEATH_PLAGUE_CUSTOM      	          = 81418, // 72879 // Triggert 81419
    SPELL_DEATH_PLAGUE_AURA_CUSTOM     	      = 81419, // 72865 // Triggert 81422
    SPELL_RECENTLY_INFECTED_CUSTOM     	      = 81420, // 72884
    SPELL_DEATH_PLAGUE_KILL_CUSTOM     	      = 81421, // 72867
    SPELL_STEAL_FLESH_2                       = 81426,
    SPELL_STEAL_FLESH_DEBUFF_2 		          = 81427,
    SPELL_ADD_MUG           	              = 42518,
};

enum Custom_spell_31695
{
    TRIGGER_NPC_ENTRY = 101003,
    TARGET_NPC_ENTRY  = 29919
};

using namespace std::chrono;

// 70117 - Icy Grip

class spell_custom_icy_grip : public SpellScript
{
    PrepareSpellScript(spell_custom_icy_grip);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_ICY_GRIP_JUMP_CUSTOM });
    }

    void HandleScript(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        GetHitUnit()->CastSpell(GetCaster(), SPELL_ICY_GRIP_JUMP_CUSTOM, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_custom_icy_grip::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// Durchbohren Custom SpellID 81091

class spell_gen_remove_on_health_pct_custom : public AuraScript
{
    PrepareAuraScript(spell_gen_remove_on_health_pct_custom);

    void PeriodicTick(AuraEffect const* /*aurEff*/)
    {
        // they apply damage so no need to check for ticks here

        if (GetTarget()->HealthAbovePct(GetEffectInfo(EFFECT_1).CalcValue()))
        {
            Remove(AURA_REMOVE_BY_ENEMY_SPELL);
            PreventDefaultAction();
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_gen_remove_on_health_pct_custom::PeriodicTick, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

// 81116 - Frostschlag
class spell_kaylas_frost_blast : public AuraScript
{
    PrepareAuraScript(spell_kaylas_frost_blast);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_FROST_BLAST_DMG_CUSTOM });
    }

    void PeriodicTick(AuraEffect const* aurEff)
    {
        PreventDefaultAction();

        // Stuns the target, dealing 26% of the target's maximum health in Frost damage every second for 4 sec.
        if (Unit* caster = GetCaster())
        {
            CastSpellExtraArgs args(aurEff);
            args.AddSpellBP0(GetTarget()->CountPctFromMaxHealth(26));
            caster->CastSpell(GetTarget(), SPELL_FROST_BLAST_DMG_CUSTOM, args);
        }
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_kaylas_frost_blast::PeriodicTick, EFFECT_1, SPELL_AURA_PERIODIC_TRIGGER_SPELL);
    }
};

// 81259 - Ice Burst Target Search
class spell_kerkermeister_ice_burst_target_search : public SpellScript
{
    PrepareSpellScript(spell_kerkermeister_ice_burst_target_search);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_ICE_BURST_CUSTOM });
    }

    void CheckTargetCount(std::list<WorldObject*>& unitList)
    {
        if (unitList.empty())
            return;

        // if there is at least one affected target cast the explosion
        GetCaster()->CastSpell(GetCaster(), SPELL_ICE_BURST_CUSTOM, true);
        if (GetCaster()->GetTypeId() == TYPEID_UNIT)
        {
            GetCaster()->ToCreature()->SetReactState(REACT_PASSIVE);
            GetCaster()->AttackStop();
            GetCaster()->ToCreature()->DespawnOrUnsummon(500ms);
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_kerkermeister_ice_burst_target_search::CheckTargetCount, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

// 73530 - Shadow Trap (Visual)
class spell_kerkermeister_shadow_trap_visual : public AuraScript
{
    PrepareAuraScript(spell_kerkermeister_shadow_trap_visual);

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() == AURA_REMOVE_BY_EXPIRE)
            GetTarget()->CastSpell(GetTarget(), SPELL_SHADOW_TRAP_AURA_CUSTOM, TRIGGERED_NONE);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectRemoveFn(spell_kerkermeister_shadow_trap_visual::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

// 74282 - Shadow Trap (Periodic)
class spell_kerkermeister_shadow_trap_periodic : public SpellScript
{
    PrepareSpellScript(spell_kerkermeister_shadow_trap_periodic);

    void CheckTargetCount(std::list<WorldObject*>& targets)
    {
        if (targets.empty())
            return;

        GetCaster()->CastSpell(nullptr, SPELL_SHADOW_TRAP_KNOCKBACK_CUSTOM, true);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_kerkermeister_shadow_trap_periodic::CheckTargetCount, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

// 81360 - Fangnetz
class spell_fangnetz : public AuraScript
{
    PrepareAuraScript(spell_fangnetz);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_WEB_WRAP_WRAPPED_CUSTOM });
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        if (Unit* target = GetTarget())
            target->CastSpell(target, SPELL_WEB_WRAP_WRAPPED_CUSTOM, true);
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_fangnetz::HandleEffectRemove, EFFECT_0, SPELL_AURA_MOD_ROOT, AURA_EFFECT_HANDLE_REAL);
    }
};

enum TheArtOfPersuasion_Custom
{
    WHISPER_TORTURE_CUSTOM_1                      = 0,
    WHISPER_TORTURE_CUSTOM_2                      = 1,
    WHISPER_TORTURE_CUSTOM_3                      = 2,
    WHISPER_TORTURE_CUSTOM_4                      = 3,
    WHISPER_TORTURE_CUSTOM_5                      = 4,
    WHISPER_TORTURE_CUSTOM_RANDOM_1               = 5,
    WHISPER_TORTURE_CUSTOM_RANDOM_2               = 6,
    WHISPER_TORTURE_CUSTOM_RANDOM_3               = 7,

    SPELL_NEURAL_NEEDLE_IMPACT             = 45702
};

// 45634 - Neural Needle
class spell_borean_tundra_neural_needle_custom : public SpellScript
{
    PrepareSpellScript(spell_borean_tundra_neural_needle_custom);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_NEURAL_NEEDLE_IMPACT });
    }

    void HandleWhisper()
    {
        Player* caster = GetCaster()->ToPlayer();
        Creature* target = GetHitCreature();
        if (!caster || !target)
            return;

        target->CastSpell(target, SPELL_NEURAL_NEEDLE_IMPACT);

        if (Aura* aura = caster->GetAura(GetSpellInfo()->Id))
        {
            switch (aura->GetStackAmount())
            {
                case 1:
                    target->AI()->Talk(WHISPER_TORTURE_CUSTOM_1, caster);
                    break;
                case 2:
                    target->AI()->Talk(WHISPER_TORTURE_CUSTOM_2, caster);
                    break;
                case 3:
                    target->AI()->Talk(WHISPER_TORTURE_CUSTOM_3, caster);
                    break;
                case 4:
                    target->AI()->Talk(WHISPER_TORTURE_CUSTOM_4, caster);
                    break;
                case 5:
                    target->AI()->Talk(WHISPER_TORTURE_CUSTOM_5, caster);
                    caster->KilledMonsterCredit(target->GetEntry());
                    break;
                case 6:
                    target->AI()->Talk(RAND(WHISPER_TORTURE_CUSTOM_RANDOM_1, WHISPER_TORTURE_CUSTOM_RANDOM_2, WHISPER_TORTURE_CUSTOM_RANDOM_3), caster);
                    break;
                default:
                    return;
            }
        }
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_borean_tundra_neural_needle_custom::HandleWhisper);
    }
};

// 72864 - Death Plague
class spell_todesseuche : public SpellScript
{
    PrepareSpellScript(spell_todesseuche);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_RECENTLY_INFECTED_CUSTOM, SPELL_DEATH_PLAGUE_KILL_CUSTOM, SPELL_DEATH_PLAGUE_CUSTOM });
    }

    // Damage Effect count
    void CountTargets(std::list<WorldObject*>& targets)
    {
        _sharedList = targets;
        _failed = targets.empty();
    }

    // Filter targets to jump
    void FilterTargets(std::list<WorldObject*>& targets)
    {
        targets = _sharedList;
        targets.remove_if([](WorldObject* obj) -> bool
        {
            Unit* object = obj->ToUnit();

            if (!object || object->GetTypeId() != TYPEID_PLAYER)
                return true;

            if (object->HasAura(SPELL_RECENTLY_INFECTED_CUSTOM) || object->HasAura(SPELL_DEATH_PLAGUE_AURA_CUSTOM))
                return true;

            return false;
        });
    }

    void HandleScript(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        Unit* caster = GetCaster();
        caster->CastSpell(GetHitUnit(), SPELL_DEATH_PLAGUE_CUSTOM, true);
        caster->CastSpell(caster, SPELL_RECENTLY_INFECTED_CUSTOM, true);
    }

    void HandleKill()
    {
        if (_failed)
        {
            Unit* caster = GetCaster();
            caster->CastSpell(caster, SPELL_DEATH_PLAGUE_KILL_CUSTOM, true);
        }
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_todesseuche::CountTargets, EFFECT_0, TARGET_UNIT_SRC_AREA_ALLY);
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_todesseuche::FilterTargets, EFFECT_1, TARGET_UNIT_SRC_AREA_ALLY);
        OnEffectHitTarget += SpellEffectFn(spell_todesseuche::HandleScript, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
        AfterCast += SpellCastFn(spell_todesseuche::HandleKill);
    }

private:
    bool _failed = false;
    std::list<WorldObject*> _sharedList;
};

// 81426 - Steal Flesh
class spell_seuche_injekzieren : public AuraScript
{
    PrepareAuraScript(spell_seuche_injekzieren);

    void HandlePeriodic(AuraEffect const* /*eff*/)
    {

	GetCaster()->CastSpell(GetTarget(), SPELL_STEAL_FLESH_DEBUFF_2, true);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_seuche_injekzieren::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }
};

enum q13007IronColossus
{
    SPELL_JORMUNGAR_SUBMERGE         = 56504,
    SPELL_JORMUNGAR_EMERGE           = 56508,
    SPELL_JORMUNGAR_SUBMERGE_VISUAL  = 56512,
    SPELL_COLOSSUS_GROUND_SLAM       = 61673,
    SPELL_RIDE_VEHICLE_HARDCODED     = 46598,
    SPELL_JORMUNGAR_SUBMERGE_AURA    = 56512
};

class spell_q13007_iron_colossus : public SpellScript
{
    PrepareSpellScript(spell_q13007_iron_colossus);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_JORMUNGAR_SUBMERGE_VISUAL, SPELL_COLOSSUS_GROUND_SLAM });
    }

    void HandleDummy(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        Creature* caster = GetCaster()->ToCreature();
        if (!caster)
            return;

        if (GetSpellInfo()->Id == SPELL_JORMUNGAR_SUBMERGE)
        {
            caster->CastSpell(caster, SPELL_JORMUNGAR_SUBMERGE_VISUAL, true);
            caster->ApplySpellImmune(SPELL_COLOSSUS_GROUND_SLAM, IMMUNITY_ID, SPELL_COLOSSUS_GROUND_SLAM, true);

            // Immunit t gegen jeglichen Schaden setzen
            caster->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_ALL, true);

            // Einmalig 5% Leben heilen
            caster->SetHealth(std::min(caster->GetHealth() + caster->GetMaxHealth() / 20, caster->GetMaxHealth()));

            // Bewegung erlauben: Root entfernen
            caster->SetControlled(false, UNIT_STATE_ROOT);

            for (uint8 i = 0; i < MAX_CREATURE_SPELLS; ++i)
                caster->m_spells[i] = 0;

            caster->m_spells[0] = SPELL_JORMUNGAR_EMERGE;
        }
        else
        {
            caster->RemoveAurasDueToSpell(SPELL_JORMUNGAR_SUBMERGE_VISUAL);
            caster->ApplySpellImmune(SPELL_COLOSSUS_GROUND_SLAM, IMMUNITY_ID, SPELL_COLOSSUS_GROUND_SLAM, false);

            // Immunit t gegen Schaden entfernen
            caster->ApplySpellImmune(0, IMMUNITY_DAMAGE, SPELL_SCHOOL_MASK_ALL, false);

            // Bewegung sperren: Root setzen
            caster->SetControlled(true, UNIT_STATE_ROOT);

            if (CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(caster->GetEntry()))
                for (uint8 i = 0; i < MAX_CREATURE_SPELLS; ++i)
                    caster->m_spells[i] = ct->spells[i];
        }

        if (Player* player = caster->GetCharmerOrOwnerPlayerOrPlayerItself())
            player->VehicleSpellInitialize();
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_q13007_iron_colossus::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};


enum MountModSpells
{
    SPELL_CARROT_ON_A_STICK_EFFECT = 48402,
    SPELL_RIDING_CROP_EFFECT = 48383,
    SPELL_MITHRIL_SPURS_EFFECT = 59916,
    SPELL_MITHRIL_SPURS = 7215,
    SPELL_MOUNT_SPEED_CARROT = 48777,
    SPELL_MOUNT_SPEED_RIDING = 48776
};

class spell_item_with_mount_speed : public AuraScript
{
    PrepareAuraScript(spell_item_with_mount_speed);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        if (!sSpellMgr->GetSpellInfo(SPELL_MOUNT_SPEED_CARROT)
            || !sSpellMgr->GetSpellInfo(SPELL_MITHRIL_SPURS)
            || !sSpellMgr->GetSpellInfo(SPELL_MOUNT_SPEED_RIDING))
        {
            return false;
        }
        return true;
    }

    uint32 getMountSpellId()
    {
        switch (m_scriptSpellId)
        {
            case SPELL_MOUNT_SPEED_CARROT:
                return SPELL_CARROT_ON_A_STICK_EFFECT;
            case SPELL_MITHRIL_SPURS:
                return SPELL_MITHRIL_SPURS_EFFECT;
            case SPELL_MOUNT_SPEED_RIDING:
                return SPELL_RIDING_CROP_EFFECT;
            default:
                return 0;
        }
    }

    void OnApply(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (target->GetLevel() <= 70)
        {
            if (auto spellId = getMountSpellId())
            {
                target->CastSpell(target, spellId, aurEff);
            }
        }
    }

    void OnRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* target = GetTarget();
        if (auto spellId = getMountSpellId())
        {
            target->RemoveAurasDueToSpell(spellId);
        }
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_item_with_mount_speed::OnApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_item_with_mount_speed::OnRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};


enum TheChainGunAndYou
{
    TEXT_CALL_OUT_1    = 27083,
    TEXT_CALL_OUT_2    = 27084
};

class spell_dragonblight_call_out_injured_soldier_custom : public SpellScript
{
    PrepareSpellScript(spell_dragonblight_call_out_injured_soldier_custom);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return sObjectMgr->GetBroadcastText(TEXT_CALL_OUT_1) && sObjectMgr->GetBroadcastText(TEXT_CALL_OUT_2);
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Vehicle* vehicle = GetCaster()->GetVehicleKit())
            if (Unit* passenger = vehicle->GetPassenger(0))
            {
                passenger->Unit::Say(RAND(TEXT_CALL_OUT_1, TEXT_CALL_OUT_2), passenger);
                //GetCaster()->CastSpell(GetCaster(), 49554, true);
            }
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_dragonblight_call_out_injured_soldier_custom::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

int32 CalculateRandomDamage()
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dist(1500, 2500);
    return dist(gen);
}

class spell_scaled_random_damage : public SpellScript
{
    PrepareSpellScript(spell_scaled_random_damage);

    void HandleEffect(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Player* player = nullptr;

        if (caster)
        {
            if (caster->IsPlayer())
                player = caster->ToPlayer();
            else if (Unit* owner = caster->GetCharmerOrOwner())
                player = owner->ToPlayer();
        }

        if (!player)
            return;

        SetHitDamage(CalculateRandomDamage());
    }

    void Register()
    {
        OnEffectHitTarget += SpellEffectFn(spell_scaled_random_damage::HandleEffect, EFFECT_0, SPELL_EFFECT_SCHOOL_DAMAGE);
    }
};

enum SongOfWindAndWater
{
    NPC_SOWAW_WATER_ELEMENTAL = 28999,
    NPC_SOWAW_WIND_ELEMENTAL  = 28985,
    NPC_SOWAW_WIND_MODEL      = 14516,
    NPC_SOWAW_WATER_MODEL     = 20076,
};

class spell_q12726_song_of_wind_and_water : public SpellScript
{
    PrepareSpellScript(spell_q12726_song_of_wind_and_water);

    void HandleEffect(SpellEffIndex /*effIndex*/)
    {
        if (Creature* cr = GetHitCreature())
        {
            Player* owner = cr->GetCharmerOrOwnerPlayerOrPlayerItself();
            if (!owner)
                return;

            // Display wechseln
            uint32 newDisplayId = (cr->GetDisplayId() == NPC_SOWAW_WATER_MODEL) ? NPC_SOWAW_WIND_MODEL : NPC_SOWAW_WATER_MODEL;
            cr->SetDisplayId(newDisplayId);

            // Spellset des NPC updaten
            uint32 newEntry = (newDisplayId == NPC_SOWAW_WIND_MODEL) ? NPC_SOWAW_WIND_ELEMENTAL : NPC_SOWAW_WATER_ELEMENTAL;
            if (CreatureTemplate const* ct = sObjectMgr->GetCreatureTemplate(newEntry))
            {
                for (uint8 i = 0; i < MAX_CREATURE_SPELLS; ++i)
                    cr->m_spells[i] = ct->spells[i];
            }

            // Vehicle-Besitzer heilen (35% HP)
            Unit* vehicle = owner->GetVehicleBase();
            if (vehicle)
            {
                int32 healAmount = int32(vehicle->CountPctFromMaxHealth(35));
                HealInfo healInfo(vehicle, vehicle, healAmount, GetSpellInfo(), SPELL_SCHOOL_MASK_NORMAL);
                vehicle->HealBySpell(healInfo);
            }

            owner->VehicleSpellInitialize();
        }
    }

    void Register() override
    {
        // EFFECT_3 = HEAL_PCT (der einzige Effekt, der Scripts triggert)
        OnEffectHitTarget += SpellEffectFn(spell_q12726_song_of_wind_and_water::HandleEffect, EFFECT_2, SPELL_EFFECT_HEAL_PCT);;
    }
};

class spell_colossal_slam : public SpellScript
{
    PrepareSpellScript(spell_colossal_slam);

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        if (!target->IsCreature())
            return;

        // Knockback nur f r NPC mit Entry 28103
        if (target->ToCreature()->GetEntry() != 28103)
            return;

        const SpellInfo* spellInfo = GetSpellInfo();
        if (!spellInfo)
            return;

        const SpellEffectInfo& kbEffect = spellInfo->_effects[EFFECT_1];

        float knockbackDistance = float(kbEffect.BasePoints);
        float speedZ = 10.0f;

        target->KnockbackFrom(caster->GetPositionX(), caster->GetPositionY(), knockbackDistance, speedZ);
    }

    void Register() override
    {
        // Dummy Effekt (EFFECT_0) l st Knockback aus
        OnEffectHitTarget += SpellEffectFn(spell_colossal_slam::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_suppress_58913_damage : public SpellScript
{
    PrepareSpellScript(spell_suppress_58913_damage);

    void HandleDamageEffect(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target)
            return;

        Unit* vehicleBase = nullptr;
        if (Vehicle* vehicle = caster->GetVehicle())
            vehicleBase = vehicle->GetBase();
        else
            vehicleBase = caster;

        if (!vehicleBase)
            return;

        if (vehicleBase->GetGUID() == target->GetGUID() || target->GetEntry() == 31276)
            SetHitDamage(0);
    }

    void FilterTargets(std::list<Unit*>& targets)
    {
        targets.remove_if([](Unit* unit)
            {
                return unit->GetEntry() == 31276; // Eigener Ghoul
            });
    }

    void HandleKnockbackEffect(SpellEffIndex effIndex)
    {
        Unit* target = GetHitUnit();
        if (!target)
            return;

        if (target->GetEntry() == 31276)
            PreventHitEffect(effIndex);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_suppress_58913_damage::HandleDamageEffect, EFFECT_2, SPELL_EFFECT_WEAPON_DAMAGE);
        OnEffectHitTarget += SpellEffectFn(spell_suppress_58913_damage::HandleKnockbackEffect, EFFECT_1, SPELL_EFFECT_KNOCK_BACK);
    }
};


class spell_31696 : public SpellScript
{
    PrepareSpellScript(spell_31696);

    bool Validate(SpellInfo const* spellInfo) override
    {
        return sObjectMgr->GetBroadcastText(uint32(spellInfo->GetEffect(EFFECT_0).CalcValue()));
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Unit* target = GetHitUnit())
        {
            Player* player = target->ToPlayer();
            if (!player || player->IsInCombat())
                return;

            std::list<Creature*> npcs;
            player->GetCreatureListWithEntryInGrid(npcs, 29919, 20.0f);

            Creature* than = nullptr;
            for (Creature* npc : npcs)
            {
                if (npc->IsAlive() && !npc->IsInCombat())
                {
                    than = npc;
                    break;
                }
            }

            if (!than)
                return;

            target->Unit::Say(uint32(GetEffectValue()), target);
            player->SummonCreature(101003, player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation(), TEMPSUMMON_MANUAL_DESPAWN);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_31696::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};


enum TheFelAndTheFurious
{
    SPELL_ROCKET_LAUNCHER = 38083
};

class spell_q10612_10613_the_fel_and_the_furious : public SpellScript
{
    PrepareSpellScript(spell_q10612_10613_the_fel_and_the_furious);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_ROCKET_LAUNCHER });
    }

    void HandleScriptEffect(SpellEffIndex  /*effIndex*/)
    {
        Player* charmer = GetCaster()->GetCharmerOrOwnerPlayerOrPlayerItself();
        if (!charmer)
            return;

        std::list<GameObject*> gList;
        GetCaster()->GetGameObjectListWithEntryInGrid(gList, 184979, 30.0f);
        uint8 counter = 0;
        for (std::list<GameObject*>::const_iterator itr = gList.begin(); itr != gList.end(); ++itr, ++counter)
        {
            if (counter >= 10)
                break;
            GameObject* go = *itr;
            if (!go->isSpawned())
                continue;
            Creature* cr2 = go->SummonTrigger(go->GetPositionX(), go->GetPositionY(), go->GetPositionZ() + 2.0f, 0.0f, milliseconds(100));
            if (cr2)
            {
                cr2->SetFaction(FACTION_MONSTER);
                cr2->ReplaceAllUnitFlags(UnitFlags(0));
                GetCaster()->CastSpell(cr2, SPELL_ROCKET_LAUNCHER, true);
            }

            go->SetLootState(GO_JUST_DEACTIVATED);
            charmer->KilledMonsterCredit(21959);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_q10612_10613_the_fel_and_the_furious::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 45396, 45398 - Weapon Coating Enchant
class spell_gen_weapon_coating_enchant : public AuraScript
{
    PrepareAuraScript(spell_gen_weapon_coating_enchant);

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        Unit* caster = eventInfo.GetActor();
        if (!caster)
            return false;

        return (caster->GetZoneId() == 4080 || caster->GetZoneId() == 4075 || caster->GetZoneId() == 4131);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_gen_weapon_coating_enchant::CheckProc);
    }
};


enum eQ10923EvilDrawsNear
{
    SPELL_DUSTIN_UNDEAD_DRAGON_VISUAL1      = 39256,
    SPELL_DUSTIN_UNDEAD_DRAGON_VISUAL2      = 39257,
    SPELL_DUSTIN_UNDEAD_DRAGON_VISUAL_AURA  = 39259,

    NPC_AUCHENAI_DEATH_SPIRIT               = 21967
};

class spell_q10923_evil_draws_near_summon : public SpellScript
{
    PrepareSpellScript(spell_q10923_evil_draws_near_summon);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DUSTIN_UNDEAD_DRAGON_VISUAL_AURA });
    }

    void HandleSendEvent(SpellEffIndex  /*effIndex*/)
    {
        if (Creature* auchenai = GetCaster()->FindNearestCreature(NPC_AUCHENAI_DEATH_SPIRIT, 10.0f, true))
            auchenai->CastSpell(auchenai, SPELL_DUSTIN_UNDEAD_DRAGON_VISUAL_AURA, true);
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_q10923_evil_draws_near_summon::HandleSendEvent, EFFECT_0, SPELL_EFFECT_SEND_EVENT);
    }
};

class spell_q10923_evil_draws_near_periodic_aura : public AuraScript
{
    PrepareAuraScript(spell_q10923_evil_draws_near_periodic_aura);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_DUSTIN_UNDEAD_DRAGON_VISUAL1, SPELL_DUSTIN_UNDEAD_DRAGON_VISUAL2 });
    }

    void HandlePeriodic(AuraEffect const* /*aurEff*/)
    {
        PreventDefaultAction();

        GetUnitOwner()->CastSpell(
            GetUnitOwner(),
            RAND(SPELL_DUSTIN_UNDEAD_DRAGON_VISUAL1, SPELL_DUSTIN_UNDEAD_DRAGON_VISUAL2),false);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(
            spell_q10923_evil_draws_near_periodic_aura::HandlePeriodic, EFFECT_0, SPELL_AURA_PERIODIC_TRIGGER_SPELL );
    }
};

class spell_q10923_evil_draws_near_visual : public SpellScript
{
    PrepareSpellScript(spell_q10923_evil_draws_near_visual);

    void SetDest(SpellDestination& dest)
    {
        // Adjust effect summon position
        Position const offset = { 0.0f, 0.0f, 20.0f, 0.0f };
        dest.RelocateOffset(offset);
    }

    void Register() override
    {
        OnDestinationTargetSelect += SpellDestinationTargetSelectFn(spell_q10923_evil_draws_near_visual::SetDest, EFFECT_0, TARGET_DEST_CASTER_RADIUS);
    }
};

class spell_kerkermeister_spirits : public AuraScript
{
    PrepareAuraScript(spell_kerkermeister_spirits);

public:
    spell_kerkermeister_spirits()
    {
        _is25Man = false;
    }

private:
    bool Load() override
    {
        _is25Man = GetUnitOwner()->GetMap()->Is25ManRaid();
        return true;
    }

    void OnPeriodic(AuraEffect const* aurEff)
    {
        if (_is25Man || ((aurEff->GetTickNumber() - 1) % 5))
            GetTarget()->CastSpell(nullptr, aurEff->GetSpellEffectInfo().TriggerSpell, { aurEff, GetCasterGUID() });
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_kerkermeister_spirits::OnPeriodic, EFFECT_0, SPELL_AURA_PERIODIC_DUMMY);
    }

    bool _is25Man;
};

class spell_kerkermeister_spirits_visual : public SpellScript
{
    PrepareSpellScript(spell_kerkermeister_spirits_visual);

    void ModDestHeight(SpellEffIndex /*effIndex*/)
    {
        Position offset = { 0.0f, 0.0f, 15.0f, 0.0f };
        const_cast<WorldLocation*>(GetExplTargetDest())->RelocateOffset(offset);
    }

    void Register() override
    {
        OnEffectLaunch += SpellEffectFn(spell_kerkermeister_spirits_visual::ModDestHeight, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

class spell_kerkermeister_spirit_move_target_search : public SpellScript
{
    PrepareSpellScript(spell_kerkermeister_spirit_move_target_search);

public:
    spell_kerkermeister_spirit_move_target_search()
    {
        _target = nullptr;
    }

private:
    bool Load() override
    {
        return GetCaster()->GetTypeId() == TYPEID_UNIT;
    }

    void SelectTarget(std::list<WorldObject*>& targets)
    {
        if (targets.empty())
            return;

        _target = Trinity::Containers::SelectRandomContainerElement(targets);
    }

    void HandleScript(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        // for this spell, all units are in target map, however it should select one to attack
        if (GetHitUnit() != _target)
            return;

        GetCaster()->ToCreature()->AI()->AttackStart(GetHitUnit());
        GetCaster()->GetThreatManager().AddThreat(GetHitUnit(), 100000.0f);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_kerkermeister_spirit_move_target_search::SelectTarget, EFFECT_0, TARGET_UNIT_SRC_AREA_ENEMY);
        OnEffectHitTarget += SpellEffectFn(spell_kerkermeister_spirit_move_target_search::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }

    WorldObject* _target;
};

class spell_kerkermeister_spirit_damage_target_search : public SpellScript
{
    PrepareSpellScript(spell_kerkermeister_spirit_damage_target_search);

    bool Load() override
    {
        return GetCaster()->GetTypeId() == TYPEID_UNIT;
    }

    void CheckTargetCount(std::list<WorldObject*>& targets)
    {
        if (targets.empty())
            return;

        GetCaster()->CastSpell(nullptr, 70503, true);
        GetCaster()->ToCreature()->DespawnOrUnsummon(1s);
        GetCaster()->SetUnitFlag(UNIT_FLAG_UNINTERACTIBLE);
    }

    void Register() override
    {
        OnObjectAreaTargetSelect += SpellObjectAreaTargetSelectFn(spell_kerkermeister_spirit_damage_target_search::CheckTargetCount,EFFECT_0,TARGET_UNIT_SRC_AREA_ENEMY);
    }
};

class spell_kerkermeister_raging_spirit : public SpellScript
{
    PrepareSpellScript(spell_kerkermeister_raging_spirit);

    void HandleScript(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        GetHitUnit()->CastSpell(GetHitUnit(), uint32(GetEffectValue()), true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_kerkermeister_raging_spirit::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};



class spell_kerkermeister_summon_into_air : public SpellScript
{
    PrepareSpellScript(spell_kerkermeister_summon_into_air);

    void ModDestHeight(SpellEffIndex /*effIndex*/)
    {
        static Position const offset = {0.0f, 0.0f, 15.0f, 0.0f};
        WorldLocation* dest = const_cast<WorldLocation*>(GetExplTargetDest());
        dest->RelocateOffset(offset);
        GetHitDest()->RelocateOffset(offset);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_kerkermeister_summon_into_air::ModDestHeight, EFFECT_0, SPELL_EFFECT_SUMMON);
    }
};

#include "ScriptMgr.h"
#include "SpellScript.h"
#include "SpellAuraEffects.h"
#include "GameObject.h"

enum CovertOpsAlphaBetaMisc
{
    // Spells
    SPELL_SET_NG_5_CHARGE_RED = 6630,
    SPELL_SET_NG_5_CHARGE_BLUE = 6626,
    SPELL_REMOTE_DETONATOR_RED = 6627,
    SPELL_REMOTE_DETONATOR_BLUE = 6656,

    // Gameobjects
    GO_NG_5_EXPLOSIVES_RED = 19592,
    GO_NG_5_EXPLOSIVES_BLUE = 19601,
    GO_SPELLFOCUS_RED = 19600,
    GO_SPELLFOCUS_BLUE = 19591,
    VENTURE_WAGON_1 = 20899,
    VENTURE_WAGON_2 = 19547,
};

class spell_set_ng_5_charge : public SpellScriptLoader
{
public:
    spell_set_ng_5_charge() : SpellScriptLoader("spell_set_ng_5_charge") {}

    class spell_set_ng_5_charge_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_set_ng_5_charge_SpellScript);

        void HandleAfterCast()
        {
            if (Unit* caster = GetCaster())
            {
                Position pos = caster->GetPosition();

                if (caster->FindNearestGameObject(GO_SPELLFOCUS_RED, 100.0f))
                    caster->SummonGameObject(GO_NG_5_EXPLOSIVES_RED, pos, QuaternionData(), Seconds(300));
                else if (caster->FindNearestGameObject(GO_SPELLFOCUS_BLUE, 100.0f))
                    caster->SummonGameObject(GO_NG_5_EXPLOSIVES_BLUE, pos, QuaternionData(), Seconds(300));
            }
        }

        void Register() override
        {
            AfterCast += SpellCastFn(spell_set_ng_5_charge_SpellScript::HandleAfterCast);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_set_ng_5_charge_SpellScript();
    }
};

class spell_remote_detonator : public SpellScriptLoader
{
public:
    spell_remote_detonator() : SpellScriptLoader("spell_remote_detonator") {}

    class spell_remote_detonator_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_remote_detonator_SpellScript);

        void HandleAfterCast()
        {
            if (Unit* caster = GetCaster())
            {
                if (caster->FindNearestGameObject(GO_SPELLFOCUS_RED, 100.0f))
                {
                    if (GameObject* trap = caster->FindNearestGameObject(VENTURE_WAGON_1, 100.0f))
                        trap->UseDoorOrButton();
                }
                else if (caster->FindNearestGameObject(GO_SPELLFOCUS_BLUE, 100.0f))
                {
                    if (GameObject* trap = caster->FindNearestGameObject(VENTURE_WAGON_2, 100.0f))
                        trap->UseDoorOrButton();
                }
            }
        }

        void Register() override
        {
            AfterCast += SpellCastFn(spell_remote_detonator_SpellScript::HandleAfterCast);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_remote_detonator_SpellScript();
    }
};

enum spell_ritual_prayer_beads
{
    SPELL_HEAL_BARADA           = 39322,

    NPC_ANCHORITE_BARADA        = 22431,
    NPC_DARKNESS_RELEASED       = 22507,
    NPC_FOUL_PURGE              = 22506
};

class spell_39322_prayer_beads : public SpellScript
{
    PrepareSpellScript(spell_39322_prayer_beads);

    void HandleAfterHit()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();
        if (!caster || !target || !target->IsCreature())
            return;

        uint32 entry = target->GetEntry();
        if (entry == NPC_ANCHORITE_BARADA || entry == NPC_DARKNESS_RELEASED || entry == NPC_FOUL_PURGE)
        {
            if (Creature* npc22431 = target->FindNearestCreature(NPC_ANCHORITE_BARADA, 100.0f, true))
            {
                caster->CastSpell(npc22431, SPELL_HEAL_BARADA, true);
            }
        }
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_39322_prayer_beads::HandleAfterHit);
    }
};

// F r die Spell ID 36414

class spell_fokussierte_explosion : public SpellScript
{
    PrepareSpellScript(spell_fokussierte_explosion);

    void HandleDummy(SpellEffIndex effIndex)
{
    PreventHitDefaultEffect(effIndex);

    if (effIndex != EFFECT_0)
        return;

    if (Unit* target = GetHitUnit())
    {
        // Zugriff auf BasePoints korrekt  ber '.'
        uint32 spellId = uint32(GetSpellInfo()->GetEffect(EFFECT_0).BasePoints) + urand(1, 3);
        GetCaster()->CastSpell(target, spellId, true);
    }
}

    void Register() override
    {
        // Ein Handler f r alle Dummy-Effekte
        OnEffectHitTarget += SpellEffectFn(spell_fokussierte_explosion::HandleDummy, EFFECT_ALL, SPELL_EFFECT_DUMMY);
    }
};

// Allianz ? Map 729
static const Position AlliancePos =
{
    2966.185791f,
    3260.218750f,
    50.340939f,
    3.357370f
};

// Horde ? Map 729
static const Position HordePos =
{
    3182.010010f,
    3169.659912f,
    53.835201f,
    1.378620f
};

class spell_faction_teleport : public SpellScriptLoader
{
public:
    spell_faction_teleport() : SpellScriptLoader("spell_faction_teleport") {}

    class spell_faction_teleport_SpellScript : public SpellScript
    {
        PrepareSpellScript(spell_faction_teleport_SpellScript);

        void HandleDummy(SpellEffIndex /*effIndex*/)
        {
            Player* player = GetCaster()->ToPlayer();
            if (!player)
                return;

            // Allianz
            if (player->GetTeamId() == TEAM_ALLIANCE)
            {
                player->TeleportTo(
                    729,
                    AlliancePos.GetPositionX(),
                    AlliancePos.GetPositionY(),
                    AlliancePos.GetPositionZ(),
                    AlliancePos.GetOrientation()
                );
                return;
            }

            // Horde
            if (player->GetTeamId() == TEAM_HORDE)
            {
                player->TeleportTo(
                    729,
                    HordePos.GetPositionX(),
                    HordePos.GetPositionY(),
                    HordePos.GetPositionZ(),
                    HordePos.GetOrientation()
                );
            }
        }

        void Register() override
        {
            OnEffectHitTarget += SpellEffectFn(spell_faction_teleport_SpellScript::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
        }
    };

    SpellScript* GetSpellScript() const override
    {
        return new spell_faction_teleport_SpellScript();
    }
};

class spell_81464 : public SpellScriptLoader
{
public:
    spell_81464() : SpellScriptLoader("spell_81464") { }

    class spell_81464_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_81464_AuraScript);

        void OnPeriodic(AuraEffect const*)
{
    WorldObject* ownerObj = GetOwner();
    if (!ownerObj)
        return;

    Unit* owner = ownerObj->ToUnit();
    if (!owner)
        return;

    // Haupttr ger darf KEINEN Schaden vom Trigger bekommen
    if (owner->HasAura(81463))
        PreventDefaultAction();
}

        void Register() override
        {
            OnEffectPeriodic += AuraEffectPeriodicFn(
                spell_81464_AuraScript::OnPeriodic,
                EFFECT_1,
                SPELL_AURA_PERIODIC_DAMAGE);
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_81464_AuraScript();
    }
};

enum Npcs
{
    NPC_ADD = 100795
};

class spell_spawn_add_periodic : public SpellScriptLoader
{
public:
    spell_spawn_add_periodic() : SpellScriptLoader("spell_spawn_add_periodic") {}

    class spell_spawn_add_periodic_AuraScript : public AuraScript
    {
        PrepareAuraScript(spell_spawn_add_periodic_AuraScript);

        void OnPeriodic(AuraEffect const* /*aurEff*/)
        {
            Unit* owner = GetOwner()->ToUnit();
            if (!owner)
                return;

            Map* map = owner->GetMap();
            if (!map)
                return;

            for (Map::PlayerList::const_iterator itr = map->GetPlayers().begin(); itr != map->GetPlayers().end(); ++itr)
            {
                if (!itr->GetSource())
                    continue;

                Player* player = itr->GetSource()->ToPlayer();
                if (!player || !player->IsInWorld())
                    continue;

                // Pr fe die Aura 81468
                if (!player->HasAura(81468))
                    continue;

                // Spawn direkt auf Spielerposition
                player->SummonCreature(
                    NPC_ADD,
                    player->GetPositionX(),
                    player->GetPositionY(),
                    player->GetPositionZ(),
                    player->GetOrientation()
                );
            }
        }

        void Register() override
        {
            OnEffectPeriodic += AuraEffectPeriodicFn(
                spell_spawn_add_periodic_AuraScript::OnPeriodic,
                EFFECT_0,
                SPELL_AURA_PERIODIC_TRIGGER_SPELL
            );
        }
    };

    AuraScript* GetAuraScript() const override
    {
        return new spell_spawn_add_periodic_AuraScript();
    }
};

/*######
## Quest 12828: Ample Inspiration
######*/

enum SpellJade
{
    SPELL_JADE_SUMMON_OBJECT_1 = 81490,
    SPELL_JADE_SUMMON_OBJECT_2 = 81491,
    SPELL_JADE_SUMMON_OBJECT_3 = 81492
};

// 81493 - Jade Explosion Spell Spawner
class spell_summon_jade_master : public SpellScript
{
    PrepareSpellScript(spell_summon_jade_master);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
            {
                SPELL_JADE_SUMMON_OBJECT_1,
                SPELL_JADE_SUMMON_OBJECT_2,
                SPELL_JADE_SUMMON_OBJECT_3
            });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        caster->CastSpell(caster, SPELL_JADE_SUMMON_OBJECT_1);
        caster->CastSpell(caster, SPELL_JADE_SUMMON_OBJECT_2);
        caster->CastSpell(caster, SPELL_JADE_SUMMON_OBJECT_3);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_summon_jade_master::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

void AddSC_custom_spells()
{
    RegisterSpellScript(spell_item_with_mount_speed);
    RegisterSpellScript(spell_39322_prayer_beads);
    RegisterSpellScript(spell_custom_icy_grip);
    RegisterSpellScript(spell_gen_remove_on_health_pct_custom);
    RegisterSpellScript(spell_kaylas_frost_blast);
    RegisterSpellScript(spell_kerkermeister_ice_burst_target_search);
    RegisterSpellScript(spell_kerkermeister_shadow_trap_visual);
    RegisterSpellScript(spell_kerkermeister_shadow_trap_periodic);
    RegisterSpellScript(spell_fangnetz);
    RegisterSpellScript(spell_borean_tundra_neural_needle_custom);
    RegisterSpellScript(spell_seuche_injekzieren);
    RegisterSpellScript(spell_todesseuche);
    RegisterSpellScript(spell_q13007_iron_colossus);
    RegisterSpellScript(spell_dragonblight_call_out_injured_soldier_custom);
    RegisterSpellScript(spell_q12726_song_of_wind_and_water);
    RegisterSpellScript(spell_colossal_slam);
    RegisterSpellScript(spell_scaled_random_damage);
    RegisterSpellScript(spell_suppress_58913_damage);
    RegisterSpellScript(spell_31696);
    RegisterSpellScript(spell_q10612_10613_the_fel_and_the_furious);
    RegisterSpellScript(spell_gen_weapon_coating_enchant);
    RegisterSpellScript(spell_q10923_evil_draws_near_summon);
    RegisterSpellScript(spell_q10923_evil_draws_near_visual);
    RegisterSpellScript(spell_q10923_evil_draws_near_periodic_aura);
    RegisterSpellScript(spell_kerkermeister_spirits);
    RegisterSpellScript(spell_kerkermeister_spirits_visual);
    RegisterSpellScript(spell_kerkermeister_spirit_move_target_search);
    RegisterSpellScript(spell_kerkermeister_spirit_damage_target_search);
    RegisterSpellScript(spell_kerkermeister_raging_spirit);
    RegisterSpellScript(spell_kerkermeister_summon_into_air);
    RegisterSpellScript(spell_fokussierte_explosion);
    RegisterSpellScript(spell_summon_jade_master);
    new spell_spawn_add_periodic();
    new spell_set_ng_5_charge();
    new spell_remote_detonator();
    new spell_faction_teleport();
	new spell_81464();
}

