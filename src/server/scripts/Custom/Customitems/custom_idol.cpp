#include "ScriptMgr.h"
#include "Containers.h"
#include "GameTime.h"
#include "Optional.h"
#include "Player.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellMgr.h"
#include "SpellScript.h"

enum CustomSpells
{
    SPELL_DRUID_IDOL_FERAL_2       = 81034,
    SPELL_DRUID_IDOL_FERAL_1       = 81036
};

class spell_dru_feral_custom_idol : public AuraScript
{
    PrepareAuraScript(spell_dru_feral_custom_idol);

    bool Load() override
    {
        Unit* caster = GetCaster();
        return caster && GetCaster()->GetTypeId() == TYPEID_PLAYER;
    }

    void CalculateAmount(AuraEffect const* /*aurEff*/, int32& amount, bool& canBeRecalculated)
    {
        canBeRecalculated = false;

        if (Unit* caster = GetCaster())
        {
            // 0.01 * $AP * cp
            uint8 cp = caster->ToPlayer()->GetComboPoints();

            // Idol of Feral Shadows. Can't be handled as SpellMod due its dependency from CPs
            if (AuraEffect const* auraEffIdolOfFeralShadows = caster->GetAuraEffect(SPELL_DRUID_IDOL_FERAL_1, EFFECT_0))
                amount += cp * auraEffIdolOfFeralShadows->GetAmount();
                // Idol of Worship. Can't be handled as SpellMod due its dependency from CPs
            else if (AuraEffect const* auraEffIdolOfWorship = caster->GetAuraEffect(SPELL_DRUID_IDOL_FERAL_2, EFFECT_0))
                amount += cp * auraEffIdolOfWorship->GetAmount();

            amount += int32(CalculatePct(caster->GetTotalAttackPowerValue(BASE_ATTACK), cp));
        }
    }

    void Register() override
    {
        DoEffectCalcAmount += AuraEffectCalcAmountFn(spell_dru_feral_custom_idol::CalculateAmount, EFFECT_0, SPELL_AURA_PERIODIC_DAMAGE);
    }
};

void AddSC_custom_idol_spell_scripts()
{
    
    RegisterSpellScript(spell_dru_feral_custom_idol);
}
