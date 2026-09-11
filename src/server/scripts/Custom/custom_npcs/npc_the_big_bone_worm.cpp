#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "Player.h"
#include "EventMap.h"

enum The_Big_Bone_Worm_Spells
{
    SPELL_SUBMERGED = 37751,
    SPELL_TUNNEL_BORE_BONE_PASSIVE = 37989,
    SPELL_CAST_POISON = 31747,
    SPELL_CAST_BORE = 32738,
    SPELL_KNOCKBACK = 37317
};

enum The_Big_Bone_Worm_Events
{
    EVENT_REMOVE_AURAS = 1,
    EVENT_CAST_POISON = 2,
    EVENT_CAST_BORE = 3,
    EVENT_TELEPORT = 4,
    EVENT_NEW_POSITION = 5
};

struct npc_the_big_bone_worm : public ScriptedAI
{
    npc_the_big_bone_worm(Creature* creature) : ScriptedAI(creature) {}

    void IsSummonedBy(WorldObject* summoner) override
    {
        if (!summoner)
            return;

        summonerGuid = summoner->GetGUID();

        me->CastSpell(me, SPELL_SUBMERGED, true);
        me->CastSpell(me, SPELL_TUNNEL_BORE_BONE_PASSIVE, true);
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE);

        events.ScheduleEvent(EVENT_REMOVE_AURAS, 2s);
    }

    void Reset() override
    {
        me->SetReactState(REACT_AGGRESSIVE);
        me->SetControlled(true, UNIT_STATE_ROOT);
        SetCombatMovement(false);
        events.Reset();
        isActive = false;
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);

        if (isActive && !me->IsInCombat() && me->IsAlive())
        {
            me->DespawnOrUnsummon(1s);
            return;
        }

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_REMOVE_AURAS:
                me->RemoveAurasDueToSpell(SPELL_SUBMERGED);
                me->RemoveAurasDueToSpell(SPELL_TUNNEL_BORE_BONE_PASSIVE);
                me->SetStandState(UNIT_STAND_STATE_STAND);
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE);
                me->SetControlled(false, UNIT_STATE_ROOT);

                if (Unit* summoner = ObjectAccessor::GetUnit(*me, summonerGuid))
                {
                    if (summoner->GetTypeId() == TYPEID_PLAYER)
                    {
                        me->CastSpell(summoner, SPELL_KNOCKBACK, false);
                        AttackStart(summoner);
                        isActive = true;

                        events.ScheduleEvent(EVENT_CAST_POISON, 3s);
                        events.ScheduleEvent(EVENT_CAST_BORE, 8s);
                        events.ScheduleEvent(EVENT_TELEPORT, 15s);
                    }
                }
                break;

            case EVENT_TELEPORT:
                if (Unit* summoner = ObjectAccessor::GetUnit(*me, summonerGuid))
                {
                    isActive = false;
                    me->CastSpell(me, SPELL_SUBMERGED, true);
                    me->CastSpell(me, SPELL_TUNNEL_BORE_BONE_PASSIVE, true);
                    me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE);

                    events.ScheduleEvent(EVENT_NEW_POSITION, 5s);
                }
                break;

            case EVENT_NEW_POSITION:
                if (Unit* summoner = ObjectAccessor::GetUnit(*me, summonerGuid))
                {
                    Position newPos = me->GetPosition();
                    float angle = frand(0.0f, 2 * M_PI);
                    float radius = frand(5.0f, 40.0f); // min. 5, max. 40 yards

                    me->MovePositionToFirstCollision(newPos, radius, angle);
                    me->NearTeleportTo(newPos.GetPositionX(), newPos.GetPositionY(), newPos.GetPositionZ(), me->GetOrientation());

                    events.ScheduleEvent(EVENT_REMOVE_AURAS, 5s);
                }
                break;

            case EVENT_CAST_POISON:
                if (!isActive) break;
                if (Unit* target = me->GetVictim())
                {
                    me->CastSpell(target, SPELL_CAST_POISON, false);
                    events.Repeat(3s, 7s);
                }
                break;

            case EVENT_CAST_BORE:
                if (!isActive) break;
                if (Unit* target = me->GetVictim())
                {
                    me->CastSpell(target, SPELL_CAST_BORE, true);
                    events.Repeat(8s, 11s);
                }
                break;
            }
        }

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

private:
    EventMap events;
    ObjectGuid summonerGuid;
    bool isActive = false;
};

CreatureAI* GetAI_npc_the_big_bone_worm(Creature* creature)
{
    return new npc_the_big_bone_worm(creature);
}

void AddSC_npc_the_big_bone_worm()
{
    RegisterCreatureAI(npc_the_big_bone_worm);
}