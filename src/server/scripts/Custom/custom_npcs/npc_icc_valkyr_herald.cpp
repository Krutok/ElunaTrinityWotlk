#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "Item.h"

class npc_icc_valkyr_herald : public CreatureScript
{
public:
    npc_icc_valkyr_herald() : CreatureScript("npc_icc_valkyr_herald") {}

    struct npc_icc_valkyr_heraldAI : public ScriptedAI
    {
        npc_icc_valkyr_heraldAI(Creature* creature) : ScriptedAI(creature), summons(me) {}
        EventMap events;
        SummonList summons;

        void Reset() override { events.Reset(); summons.DespawnAll(); }

        void JustEngagedWith(Unit* /*who*/) override
        {
            events.Reset();
            summons.DespawnAll();
            me->setActive(true);
            events.ScheduleEvent(1, 10s);
            DoZoneInCombat(me);
        }

        void JustReachedHome() override
        {
            me->setActive(false);
        }

        void JustSummoned(Creature* s) override
        {
            summons.Summon(s);
        }

        void MoveInLineOfSight(Unit* who) override
        {
            if (me->IsAlive() && !me->IsInCombat() && who->IsPlayer() && who->GetExactDist2d(me) < 35.0f)
                AttackStart(who);
        }

        void SummonedCreatureDespawn(Creature* s) override
        {
            summons.Despawn(s);
        }

        bool CanAIAttack(Unit const* target) const override
        {
            return target->GetExactDist(4357.0f, 2769.0f, 356.0f) < 170.0f;
        }

        void SpellHitTarget(WorldObject* target, SpellInfo const* spell) override
        {
            Unit* unitTarget = target->ToUnit();
            if (!unitTarget)
                return;

            if (spell->Id != 71906 && spell->Id != 71942)
                return;

            Player* playerTarget = unitTarget->ToPlayer();
            if (!playerTarget)
                return;

            if (Creature* c = me->SummonCreature(
                38410,
                playerTarget->GetPosition(),
                TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT,
                30s))
            {
                c->AI()->AttackStart(playerTarget);
                DoZoneInCombat(c);

                if (playerTarget->GetClass() != CLASS_DRUID)
                {
                    if (Item* i = playerTarget->GetWeaponForAttack(BASE_ATTACK))
                        me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 0, i->GetEntry());
                    if (Item* i = playerTarget->GetWeaponForAttack(OFF_ATTACK))
                        me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 1, i->GetEntry());
                    if (Item* i = playerTarget->GetWeaponForAttack(RANGED_ATTACK))
                        me->SetUInt32Value(UNIT_VIRTUAL_ITEM_SLOT_ID + 2, i->GetEntry());

                    playerTarget->CastSpell(c, 60352, true); // Mirror Image
                }

                c->AI()->DoAction(playerTarget->GetClass());
            }
        }
        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            switch (events.ExecuteEvent())
            {
            case 0:
                break;
            case 1:
            {
                uint8 count = me->GetMap()->Is25ManRaid() ? 4 : 2;
                bool casted = false;
                for (uint8 i = 0; i < count; ++i)
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 37.5f, true))
                    {
                        casted = true;
                        me->CastSpell(target, 71906, true); // Severed Essence
                    }
                events.Repeat(casted ? 25s : 5s);
            }
            break;
            }

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_icc_valkyr_heraldAI(creature);
    }
};


class SeveredEssenceSpellInfo
{
public:
    uint8 Class;
    uint32 id;
    Milliseconds cooldown_ms;
    uint8 targetType;
    float range;
};

SeveredEssenceSpellInfo sesi_spells[] =
{
    { CLASS_SHAMAN, 71938, 5s, 1, 0.0f },
    { CLASS_PALADIN, 57767, 8s, 2, 30.0f },
    { CLASS_WARLOCK, 71937, 10s, 1, 0.0f },
    { CLASS_DEATH_KNIGHT, 49576, 15s, 1, 30.0f },
    { CLASS_ROGUE, 71933, 8s, 1, 0.0f },
    { CLASS_MAGE, 71928, 4s, 1, 40.0f },
    { CLASS_PALADIN, 71930, 5s, 2, 40.0f },
    { CLASS_ROGUE, 71955, 40s, 1, 30.0f },
    { CLASS_PRIEST, 71931, 5s, 2, 40.0f },
    { CLASS_SHAMAN, 71934, 7s, 1, 0.0f },
    { CLASS_DRUID, 71925, 5s, 1, 0.0f },
    { CLASS_DEATH_KNIGHT, 71951, 8s, 1, 0.0f },
    { CLASS_DEATH_KNIGHT, 71924, 8s, 1, 0.0f },
    { CLASS_WARLOCK, 71965, 20s, 0, 0.0f },
    { CLASS_PRIEST, 71932, 8s, 2, 40.0f },
    { CLASS_DRUID, 71926, 10s, 1, 0.0f },
    { CLASS_WARLOCK, 71936, 9s, 1, 0.0f },
    { CLASS_ROGUE, 57640, 3s, 1, 0.0f },
    { CLASS_WARRIOR, 71961, 5s, 1, 0.0f },
    { CLASS_MAGE, 71929, 10s, 1, 0.0f },
    { CLASS_WARRIOR, 53395, 5s, 1, 0.0f },
    { CLASS_WARRIOR, 71552, 5s, 1, 0.0f },
    { CLASS_HUNTER, 36984, 7s, 1, 0.0f },
    { CLASS_HUNTER, 29576, 5s, 1, 0.0f },
    { 0, 0, 0ms, 0, 0.0f }
};

class npc_icc_severed_essence : public CreatureScript
{
public:
    npc_icc_severed_essence() : CreatureScript("npc_icc_severed_essence") {}

    struct npc_icc_severed_essenceAI : public ScriptedAI
    {
        npc_icc_severed_essenceAI(Creature* creature) : ScriptedAI(creature) {}
        EventMap events;
        uint8 Class;

        void DoAction(int32 a) override
        {
            switch (a)
            {
            case CLASS_PALADIN:
                me->CastSpell(me, 71953, true);
                break;
            case CLASS_DRUID:
                //me->CastSpell(me, 57655, true);
                me->SetNativeDisplayId(1933);
                me->SetDisplayId(1933);
                break;
            }

            Class = a;

            for (uint8 i = 0; ; ++i)
            {
                if (sesi_spells[i].id)
                {
                    if (Class == sesi_spells[i].Class)
                        events.ScheduleEvent(i + 1, Milliseconds(sesi_spells[i].cooldown_ms / 4));
                }
                else
                    break;
            }
        }

        bool CanAIAttack(Unit const* target) const override
        {
            return target->GetExactDist(4357.0f, 2769.0f, 356.0f) < 170.0f;
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            events.Update(diff);

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            if (uint32 e = events.ExecuteEvent())
            {
                Unit* target = nullptr;
                if (sesi_spells[e - 1].targetType == 1)
                    target = me->GetVictim();
                else
                    target = DoSelectLowestHpFriendly(sesi_spells[e - 1].range - 3.0f);

                if (target)
                    me->CastSpell(target, sesi_spells[e - 1].id, TRIGGERED_IGNORE_SHAPESHIFT);

                events.Repeat(sesi_spells[e - 1].cooldown_ms);
            }

            if (Class == CLASS_HUNTER)
            {
                if (me->isAttackReady() && !me->HasUnitState(UNIT_STATE_CASTING))
                {
                    me->CastSpell(me->GetVictim(), 71927, true);
                    me->resetAttackTimer();
                }
            }
            else
                DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_icc_severed_essenceAI(creature);
    }
};

void AddSC_icc_valkyr()
{
    new npc_icc_valkyr_herald();
    new npc_icc_severed_essence();
}
