#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "MotionMaster.h"
#include "TemporarySummon.h"
#include "Player.h"
#include "Define.h" // für 1s Unterstützung

enum chainGun
{
    NPC_INJURED_7TH_LEGION_SOLDIER = 27788,
    SPELL_FEAR_AURA_WITH_COWER = 49774
};

struct npc_injured_7th_legion_soldier : public ScriptedAI
{
    npc_injured_7th_legion_soldier(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        ApplyFearAura();

        me->SetWalk(false);

        uint32 pathId = me->GetEntry() * 10 + urand(0, 4);
        if (me->GetPositionY() > -1150.0f)
            pathId += 5;

        pathId <<= 3;

        me->GetMotionMaster()->MovePath(pathId, false);
    }

    void MovementInform(uint32 type, uint32 pointId) override
    {
        if (type != WAYPOINT_MOTION_TYPE)
            return;

        me->SetHomePosition(me->GetPositionX(), me->GetPositionY(), me->GetPositionZ(), me->GetOrientation());

        if (pointId == 8)
        {
            Talk(0);
            me->RemoveAllAuras();
            me->DespawnOrUnsummon(1s);

            if (TempSummon* summon = me->ToTempSummon())
                if (WorldObject* summonerObj = summon->GetSummoner())
                    if (Unit* summoner = summonerObj->ToUnit())
                        if (Player* player = summoner->ToPlayer())
                            player->KilledMonsterCredit(me->GetEntry());
        }
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        ScriptedAI::EnterEvadeMode();
        ApplyFearAura();
    }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!me->HasAura(SPELL_FEAR_AURA_WITH_COWER))
            ApplyFearAura();
    }

    void DamageTaken(Unit* attacker, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo*/) override
    {
        if (attacker && attacker->GetEntry() == 27712)
            damage = damage / 10;
    }
    void ApplyFearAura()
    {
        if (!me->HasAura(SPELL_FEAR_AURA_WITH_COWER))
            me->CastSpell(me, SPELL_FEAR_AURA_WITH_COWER, true);
    }
};

// Registrieren
void AddSC_npc_injured_7th_legion_soldier()
{
    RegisterCreatureAI(npc_injured_7th_legion_soldier);
}
