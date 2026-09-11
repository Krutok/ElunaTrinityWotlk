#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "GameObject.h"
#include "SpellMgr.h"
#include "EventMap.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"

/*
SUPPORT QUEST 12673 It Rolls Downhill
*/

enum IT_ROLLS_DOWNHILL
{
    // Spells
    SPELL_HARVEST_BLIGHT_CRYSTAL            = 52245,
    SPELL_EVIL_TELEPORT_VISUAL_ONLY         = 61456,
    SPELL_CHARM_CHANNEL                     = 52252,
    SPELL_ORANGE_RADIATION                  = 52243,

    // GameObjects
    GO_CRYSTALLIZED_BLIGHT_1                = 190940,
    GO_CRYSTALLIZED_BLIGHT_2                = 190716,
    GO_CRYSTALLIZED_BLIGHT_3                = 190939,

    // Events
    EVENT_HANDLE_EMOTE                      = 1,
    EVENT_START_MOVE                        = 2,
    EVENT_MOVE_TO_FINAL                     = 3,

    // MovePoint IDs
    POINT_GO                                = 1,
    POINT_FINAL                             = 2,

    // Talk IDs
    TALK_BLIGHT_GEIST                       = 0,

    // KillCredit
    KILLCREDIT_NPC                          = 28740
};

struct npc_blight_geist : public ScriptedAI
{
    npc_blight_geist(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        _targetGO.Clear();
        _events.Reset();
        me->SetReactState(REACT_AGGRESSIVE);
        me->GetMotionMaster()->MoveTargetedHome();
        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
    }

    void SpellHit(WorldObject* /*caster*/, SpellInfo const* spell) override
    {
        if (spell->Id != SPELL_HARVEST_BLIGHT_CRYSTAL)
            return;

        Talk(TALK_BLIGHT_GEIST);

        std::list<GameObject*> goList;
        me->GetGameObjectListWithEntryInGrid(goList, GO_CRYSTALLIZED_BLIGHT_1, 20.0f);
        me->GetGameObjectListWithEntryInGrid(goList, GO_CRYSTALLIZED_BLIGHT_2, 20.0f);
        me->GetGameObjectListWithEntryInGrid(goList, GO_CRYSTALLIZED_BLIGHT_3, 20.0f);

        if (!goList.empty())
        {
            GameObject* closestGO = nullptr;
            float minDist = 100.0f;
            for (auto go : goList)
            {
                float dist = me->GetDistance(go);
                if (dist < minDist)
                {
                    minDist = dist;
                    closestGO = go;
                }
            }

            if (closestGO)
            {
                _targetGO = closestGO->GetGUID();
                _events.ScheduleEvent(EVENT_HANDLE_EMOTE, 2s);
                _events.ScheduleEvent(EVENT_START_MOVE, 3s);
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        // Blizzlike gets the aura while channeling 
        if (me->HasAura(SPELL_CHARM_CHANNEL))
        {
            if (!me->HasAura(SPELL_ORANGE_RADIATION))
                DoCastSelf(SPELL_ORANGE_RADIATION, false);
        }
        else
        {
            if (me->HasAura(SPELL_ORANGE_RADIATION))
                me->RemoveAura(SPELL_ORANGE_RADIATION);
        }

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_HANDLE_EMOTE:
            {
                if (GameObject* go = ObjectAccessor::GetGameObject(*me, _targetGO))
                {
                    me->SetFacingToObject(go);
                    me->HandleEmoteCommand(EMOTE_ONESHOT_POINT);
                }
                break;
            }
            case EVENT_START_MOVE:
            {
                if (GameObject* go = ObjectAccessor::GetGameObject(*me, _targetGO))
                {
                    me->GetMotionMaster()->MovePoint(POINT_GO, go->GetPositionX(), go->GetPositionY(), go->GetPositionZ());
                }
                break;
            }
            case EVENT_MOVE_TO_FINAL:
            {
                me->GetMotionMaster()->MovePoint(POINT_FINAL, 6174.28f, -2017.25f, 245.116f);
                break;
            }
            }
        }

        if (!me->HasUnitState(UNIT_STATE_CHARMED))
            if (!UpdateVictim())
                return;

        DoMeleeAttackIfReady();
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
        {
            ScriptedAI::MovementInform(type, id);
            return;
        }

        switch (id)
        {
        case POINT_GO:
        {
            if (GameObject* go = ObjectAccessor::GetGameObject(*me, _targetGO))
            {
                me->HandleEmoteCommand(EMOTE_ONESHOT_ATTACK1H);
                go->DespawnOrUnsummon(1s, 120s);
            }

            _events.ScheduleEvent(EVENT_MOVE_TO_FINAL, 3s);
            break;
        }
        case POINT_FINAL:
        {
            if (Unit* charmer = me->GetCharmer())
                if (charmer->GetTypeId() == TYPEID_PLAYER)
                    if (Player* owner = charmer->ToPlayer())
                        owner->KilledMonsterCredit(KILLCREDIT_NPC);

            DoCastSelf(SPELL_EVIL_TELEPORT_VISUAL_ONLY, false);
            me->DespawnOrUnsummon(3s, 10s);
            break;
        }
        default:
            ScriptedAI::MovementInform(type, id);
            break;
        }
    }

    void OnCharmed(bool /*apply*/) override
    {
        me->SetReactState(REACT_PASSIVE);
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        me->GetMotionMaster()->MoveTargetedHome();
    }

private:
    EventMap _events;
    ObjectGuid _targetGO;
};

CreatureAI* GetAI_npc_blight_geist(Creature* creature)
{
    return new npc_blight_geist(creature);
}

void AddSC_npc_blight_geist()
{
    RegisterCreatureAI(npc_blight_geist);
}
