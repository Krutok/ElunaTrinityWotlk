#include "ScriptedCreature.h"
#include "ScriptMgr.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "EventMap.h"

// Trigger-Koordinaten
const Position GROUP1_TRIGGERS[4] =
{
    {3222.320068f, 3525.120117f, 121.931f, 0.0f},
    {3221.620117f, 3530.449951f, 122.556f, 0.0f},
    {3221.500000f, 3532.709961f, 122.931f, 0.0f},
    {3221.510010f, 3534.090088f, 123.041f, 0.0f}
};

const Position GROUP2_TRIGGERS[3] =
{
    {3222.780029f, 3538.600098f, 123.541f, 0.0f},
    {3223.550049f, 3540.429932f, 123.666f, 0.0f},
    {3224.979980f, 3542.899902f, 123.916f, 0.0f}
};

const Position GROUP3_TRIGGERS[2] =
{
    {3227.560059f, 3546.129883f, 124.041f, 0.0f},
    {3228.540039f, 3546.989990f, 124.041f, 0.0f}
};

const Position GROUP4_TRIGGERS[2] =
{
    {3233.580078f, 3549.959961f, 123.964996f, 0.0f},
    {3235.750000f, 3550.560059f, 123.839996f, 0.0f}
};

// Spells und Gruppen in einem Enum
enum DR_BOOM_BOMB
{
    SPELL_BOMB_HIT_PLAYER   = 35132,
    SPELL_BOMB_REACH_TARGET = 35341,

    GROUP_1 = 0,
    GROUP_2 = 1,
    GROUP_3 = 2,
    GROUP_4 = 3
};

struct npc_drboom_bombe : public ScriptedAI
{
    npc_drboom_bombe(Creature* creature) : ScriptedAI(creature)
    {
        _startGroup = DR_BOOM_BOMB(rand() % 4);
    }

public:
    void Reset() override
    {
        me->SetWalk(false);
        me->SetReactState(REACT_PASSIVE);
    }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        _events.ScheduleEvent(1, 5s);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (uint32 eventId = _events.ExecuteEvent())
        {
            if (eventId == 1)
            {
                Position targetPos = me->GetHomePosition();
                int targetIndex = -1;
                uint32 start = 0, end = 0;

                switch (_startGroup)
                {
                    case GROUP_1: start = 0; end = 4; break;
                    case GROUP_2: start = 4; end = 7; break;
                    case GROUP_3: start = 7; end = 9; break;
                    case GROUP_4: start = 9; end = 11; break;
                    default: break;
                }

                for (uint32 i = start; i < end; ++i)
                {
                    if (TargetCounter[i] < 2)
                    {
                        targetIndex = i;
                        TargetCounter[i]++;
                        break;
                    }
                }

                if (targetIndex == -1)
                {
                    targetIndex = start;
                    TargetCounter[targetIndex]++;
                }

                if (targetIndex < 4)
                    targetPos = GROUP1_TRIGGERS[targetIndex];
                else if (targetIndex < 7)
                    targetPos = GROUP2_TRIGGERS[targetIndex - 4];
                else if (targetIndex < 9)
                    targetPos = GROUP3_TRIGGERS[targetIndex - 7];
                else
                    targetPos = GROUP4_TRIGGERS[targetIndex - 9];

                _targetIndex = targetIndex;
                me->GetMotionMaster()->MovePoint(0, targetPos);
            }
        }
    }

    void MovementInform(uint32 type, uint32 /*id*/) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        DoCastSelf(SPELL_BOMB_REACH_TARGET, true);
        if (_targetIndex != -1)
            TargetCounter[_targetIndex]--;

        me->DespawnOrUnsummon(1s);
    }

    void MoveInLineOfSight(Unit* who) override
    {
        if (!who || !me)
            return;

        if (who->IsPlayer() && me->IsWithinDistInMap(who, 1.0f))
        {
            DoCastSelf(SPELL_BOMB_HIT_PLAYER, true);
            if (_targetIndex != -1)
                TargetCounter[_targetIndex]--;

            me->DespawnOrUnsummon(1s);
        }
    }

private:
    inline static uint32 TargetCounter[11] = { 0 };
    DR_BOOM_BOMB _startGroup;
    int _targetIndex = -1;
    EventMap _events;
};

CreatureAI* GetAI_npc_drboom_bombe(Creature* creature)
{
    return new npc_drboom_bombe(creature);
}

void AddSC_npc_drboom_bombe()
{
    RegisterCreatureAI(npc_drboom_bombe);
}