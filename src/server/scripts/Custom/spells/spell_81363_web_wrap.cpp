#include "SpellAuras.h"
#include "SpellScript.h"
#include "PassiveAI.h"
#include "Containers.h"
#include "CreatureTextMgr.h"

enum Spells
{
    SPELL_WEB_WRAP_TRIGGER = 81364,
    SPELL_WEB_TRAP_DOT = 81365,
};

class npc_web_wrap : public NullCreatureAI
{
public:
    npc_web_wrap(Creature* creature) : NullCreatureAI(creature) { }

    void JustDied(Unit* /*killer*/) override
    {
        if (TempSummon* meSummon = me->ToTempSummon())
            if (Unit* summoner = meSummon->GetSummonerUnit())
            {
                summoner->RemoveAurasDueToSpell(SPELL_WEB_WRAP_TRIGGER);
                summoner->RemoveAurasDueToSpell(SPELL_WEB_TRAP_DOT);
            }
    }
};

class spell_81363_web_wrap : public AuraScript
{
    PrepareAuraScript(spell_81363_web_wrap);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_WEB_WRAP_TRIGGER, SPELL_WEB_TRAP_DOT });
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        if (GetTargetApplication()->GetRemoveMode() != AURA_REMOVE_BY_EXPIRE)
            return;

        if (Unit* target = GetTarget())
        {
            target->CastSpell(target, SPELL_WEB_WRAP_TRIGGER, true);
            caster->CastSpell(target, SPELL_WEB_TRAP_DOT, true);

            CreatureTextMap textMap = sCreatureTextMgr->GetTextMap();
            CreatureTextEntry itr = Trinity::Containers::SelectRandomContainerElement(textMap[NPC_WEB_WRAP][0]);
            target->Say(itr.text, LANG_UNIVERSAL);
        }
    }

    void Register() override
    {
        OnEffectRemove += AuraEffectRemoveFn(spell_81363_web_wrap::HandleEffectRemove, EFFECT_0, SPELL_AURA_MOD_ROOT, AURA_EFFECT_HANDLE_REAL);
    }

private:
    const uint32 NPC_WEB_WRAP = 100801;
};

void spell_81363_web_wrap_Script()
{
    RegisterCreatureAI(npc_web_wrap);
    RegisterSpellScript(spell_81363_web_wrap);
}
