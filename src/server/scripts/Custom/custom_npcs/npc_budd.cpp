#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "EventMap.h"
#include <list>
#include <vector>

enum Filling_the_Cages
{
    SPELL_TRIGGER = 47031,
    SPELL_CAST_AFTER = 47035,
    SPELL_NPC_SELF = 47025,
    SPELL_PLAYER_AURA = 47014,
    SPELL_STEALTH_SLOW = 47032,

    TALK_INTRO = 0,
    TALK_AFTER_CAST = 1,
    EVENT_TALK0 = 1,

    PHASE_IDLE = 0,
    PHASE_MOVE_TO_TARGET = 1,
    PHASE_CAST = 2,
    PHASE_RETURN = 3,

    QUEST_ID_ALLIANCE = 11984
};

struct npc_budd : public ScriptedAI
{
    npc_budd(Creature* creature) : ScriptedAI(creature), _phase(PHASE_IDLE) {}

    void Reset() override
    {
        _targetGUID.Clear();
        _targets.clear();
        _phase = PHASE_IDLE;
        _events.Reset();
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);

        _events.ScheduleEvent(EVENT_TALK0, 5s);
    }

    void SpellHit(WorldObject* caster, const SpellInfo* spell) override
    {
        if (!spell || spell->Id != SPELL_TRIGGER)
            return;

        if (Player* player = caster->ToPlayer())
            _playerGUID = player->GetGUID();

        // Ziel-NPCs sammeln, nur gültige ohne IMMUNE_TO_NPC und ohne bestehende Aura
        uint32 const targetEntries[] = { 26425, 26447 };
        _targets.clear();

        for (uint32 entry : targetEntries)
        {
            std::list<Creature*> tempList;
            me->GetCreatureListWithEntryInGrid(tempList, entry, 40.0f);
            for (Creature* cr : tempList)
            {
                if (!cr || !cr->IsAlive())
                    continue;

                // Ignorieren, wenn NPC IMMUNE_TO_NPC hat oder bereits die Aura vom Spell hat
                if (cr->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_NPC))
                    continue;

                if (cr->HasAura(SPELL_CAST_AFTER))
                    continue;

                _targets.push_back(cr);
            }
        }

        // Erstes gültiges Ziel auswählen
        _targetGUID.Clear();
        for (Creature* cr : _targets)
        {
            if (cr && cr->IsAlive())
            {
                _targetGUID = cr->GetGUID();
                break;
            }
        }

        if (!_targetGUID.IsEmpty())
            _phase = PHASE_MOVE_TO_TARGET;
    }


    void IsSummonedBy(WorldObject* summoner) override
    {
        if (Player* player = summoner->ToPlayer())
        {
            _playerGUID = player->GetGUID();

            DoCastSelf(SPELL_NPC_SELF, true);
            player->CastSpell(player, SPELL_PLAYER_AURA, true);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        // Talk Event
        while (uint32 eventId = _events.ExecuteEvent())
        {
            if (eventId == EVENT_TALK0)
            {
                Talk(TALK_INTRO);
                _events.ScheduleEvent(EVENT_TALK0, 15s);
            }
        }

        // Phase logic
        Creature* target = ObjectAccessor::GetCreature(*me, _targetGUID);

        switch (_phase)
        {
        case PHASE_MOVE_TO_TARGET:
            if (!target || !target->IsAlive())
                return;

            DoCastSelf(SPELL_STEALTH_SLOW, true);

            if (me->GetDistance(target) > 3.0f)
            {
                me->GetMotionMaster()->MovePoint(1, target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());
                return;
            }

            me->GetMotionMaster()->MoveFollow(target, 1.5f, 0.0f);

            if (!target->HasAura(SPELL_CAST_AFTER))
                DoCast(target, SPELL_CAST_AFTER);

            _phase = PHASE_CAST;
            break;

        case PHASE_CAST:
            if (!target || !target->IsAlive())
            {
                _phase = PHASE_RETURN;
                return;
            }

            if (me->HasUnitState(UNIT_STATE_CASTING) && me->GetDistance(target) > 3.0f)
                me->GetMotionMaster()->MoveFollow(target, 1.5f, 0.0f);

            if (!me->HasUnitState(UNIT_STATE_CASTING))
            {
                Talk(TALK_AFTER_CAST);
                _phase = PHASE_RETURN;
            }
            break;

        case PHASE_RETURN:
            me->AI()->EnterEvadeMode();
            break;

        default:
            break;
        }

        // Quest-Check: Spieler hat Quest abgegeben
        if (!_playerGUID.IsEmpty())
        {
            if (Player* player = ObjectAccessor::FindPlayer(_playerGUID))
            {
                if (player->GetQuestStatus(QUEST_ID_ALLIANCE) == QUEST_STATUS_REWARDED)
                {
                    if (player->HasAura(SPELL_PLAYER_AURA))
                        player->RemoveAurasDueToSpell(SPELL_PLAYER_AURA);

                    me->DespawnOrUnsummon();
                    return;
                }
            }
        }

        // Despawn / Spieler-Aura entfernen, wenn NPC-Aura weg
        if (!me->HasAura(SPELL_NPC_SELF))
        {
            if (!_playerGUID.IsEmpty())
            {
                if (Player* player = ObjectAccessor::FindPlayer(_playerGUID))
                {
                    if (player->HasAura(SPELL_PLAYER_AURA))
                        player->RemoveAurasDueToSpell(SPELL_PLAYER_AURA);
                }
            }

            me->DespawnOrUnsummon();
        }
    }

private:
    ObjectGuid _targetGUID;
    ObjectGuid _playerGUID;
    std::vector<Creature*> _targets;
    Filling_the_Cages _phase;
    EventMap _events;
};

CreatureAI* GetAI_npc_budd(Creature* creature)
{
    return new npc_budd(creature);
}

void AddSC_npc_budd()
{
    RegisterCreatureAI(npc_budd);
}
