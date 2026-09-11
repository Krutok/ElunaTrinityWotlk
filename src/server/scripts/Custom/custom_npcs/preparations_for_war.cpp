#include "PassiveAI.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "Transport.h"
#include "Vehicle.h"
#include "MoveSplineInit.h"
#include "MoveSpline.h"
#include <G3D/Vector3.h>
#include "Transport.h"
#include "Unit.h"
#include "WaypointManager.h"

enum ePreparationsForWar
{
    NPC_HAMMERHEAD                  = 30585,
    NPC_CLOUDBUSTER                 = 30470,

    NPC_ORGRIMS_HAMMER_DUMMY        = 30342,
    NPC_THE_SKYBREAKER_DUMMY        = 30343,

    TRANSPORT_ORGRIMS_HAMMER        = 192241,
    TRANSPORT_THE_SKYBREAKER        = 192242,

    SPELL_PARACHUTE                 = 79404
};

struct npc_preparations_for_war_vehicle : public NullCreatureAI
{
    npc_preparations_for_war_vehicle(Creature* creature)
        : NullCreatureAI(creature),
          _events(),
          targetT(nullptr),
          _hasReachedShip(false),
          _distance()
    { }

    void InitializeAI() override
    {
        _events.Reset();

        uint32 pathId = (me->GetEntry() << 3) | 1; // falls Bit-Shift angewendet wurde
        const WaypointPath* m_Path = sWaypointMgr->GetPath(pathId);
        if (!m_Path || m_Path->nodes.empty())
        {
            me->DespawnOrUnsummon();
            return;
        }

        targetT = me->GetMap()->GetTransport(me->GetEntry() == NPC_HAMMERHEAD ? TRANSPORT_ORGRIMS_HAMMER : TRANSPORT_THE_SKYBREAKER);
        if (!targetT)
        {
            me->DespawnOrUnsummon();
            return;
        }

        std::vector<Position> _positions;
        for (const WaypointNode& node : m_Path->nodes)
            _positions.push_back(Position(node.x, node.y, node.z));
        _FlyPlayerTo(_positions);

        NullCreatureAI::InitializeAI();
        _hasReachedShip = false;
        _distance = .35f * VISIBILITY_DISTANCE_NORMAL;
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (!targetT || _events.HasEventScheduled(EVENT_SEARCH_FOR_SHIP))
            return;

        if (type != SPLINE_CHAIN_MOTION_TYPE)
            return;

        if (!_hasReachedShip)
        {
            _events.ScheduleEvent(EVENT_SEARCH_FOR_SHIP, 0s);
            return;
        }

        if (id == 99)
            _events.ScheduleEvent(EVENT_SHIP_REACHED, 10s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!targetT)
            return;

        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
            switch (eventId)
            {
                case EVENT_SEARCH_FOR_SHIP: SearchForShip(); break;
                case EVENT_SHIP_REACHED: ShipReached(); break;
                default: break;
            }
    }

private:
    EventMap _events;
    Transport* targetT;
    bool _hasReachedShip;
    float _distance;

    enum Events
    {
        EVENT_SEARCH_FOR_SHIP = 1,
        EVENT_SHIP_REACHED = 2,
    };

    Player* _GetPassengerPlayer()
    {
        if (Vehicle* vehicle = me->GetVehicleKit())
            if (Unit* passenger = vehicle->GetPassenger(0); passenger->IsPlayer())
                return passenger->ToPlayer();

        return nullptr;
    }

    void _FlyPlayerTo(const Position pos)
    {
        std::function<void(Movement::MoveSplineInit&)> initializer = [pos](Movement::MoveSplineInit& init)
        {
            init.MoveTo(G3D::Vector3(pos.m_positionX, pos.m_positionY, pos.m_positionZ), true);
            init.SetFly();
        };

        me->GetMotionMaster()->LaunchMoveSpline(std::move(initializer), !_hasReachedShip ? 0 : 99, MOTION_PRIORITY_NORMAL, SPLINE_CHAIN_MOTION_TYPE);
    }

    void _FlyPlayerTo(std::vector<Position> positions)
    {
        std::function<void(Movement::MoveSplineInit&)> initializer = [positions](Movement::MoveSplineInit& init)
        {
            Movement::PointsArray path;
            path.reserve(positions.size());
            std::transform(std::begin(positions), std::end(positions), std::back_inserter(path), [](Position const& pos)
            {
                return G3D::Vector3(pos.m_positionX, pos.m_positionY, pos.m_positionZ);
            });

            init.MovebyPath(path);
            init.SetFly();
        };

        me->GetMotionMaster()->LaunchMoveSpline(std::move(initializer), 0, MOTION_PRIORITY_NORMAL, SPLINE_CHAIN_MOTION_TYPE);
    }

    void SearchForShip()
    {
        me->GetMotionMaster()->Clear();

        float x, y, z;

        if (Creature* dummy = targetT->FindNearestCreature(
                targetT->GetEntry() == TRANSPORT_ORGRIMS_HAMMER ?
                NPC_ORGRIMS_HAMMER_DUMMY : NPC_THE_SKYBREAKER_DUMMY,
                VISIBILITY_DISTANCE_NORMAL))
        {
            if (!me->GetTransport())
            {
                targetT->AddPassenger(me);
                _hasReachedShip = true;
            }

            dummy->GetPosition(x, y, z);
        }
        else
        {
            targetT->m_movementInfo.transport.pos.GetPosition(x, y, z);
            targetT->CalculatePassengerPosition(x, y, z);
            _events.ScheduleEvent(EVENT_SEARCH_FOR_SHIP, 3s);
        }

        _FlyPlayerTo(Position(x, y, z + _distance));
    }

	void ShipReached()
    {
        Player* player = _GetPassengerPlayer();
        if (!player)
            return;

        if (Vehicle* vehicle = me->GetVehicleKit())
        {
            vehicle->RemovePassenger(player);
            player->ExitVehicle();
            targetT->RemovePassenger(me);
        }

        targetT->AddPassenger(player);

        if (Creature* dummy = targetT->FindNearestCreature(targetT->GetEntry() == TRANSPORT_ORGRIMS_HAMMER ? NPC_ORGRIMS_HAMMER_DUMMY : NPC_THE_SKYBREAKER_DUMMY, VISIBILITY_DISTANCE_NORMAL))
        {
            float x, y, z, o;
            dummy->m_movementInfo.transport.pos.GetPosition(x, y, z, o);
            targetT->CalculatePassengerPosition(x, y, z, &o);

            player->ForceRemoveRoot();
            player->TeleportTo(dummy->GetMapId(), x, y, z + _distance, o, TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT | TELE_TO_NOT_UNSUMMON_PET | TELE_TO_TRANSPORT_TELEPORT);
        }        
        
        player->CastSpell(player, SPELL_PARACHUTE, true);
        me->DespawnOrUnsummon();
    }


};


void Script_PreparationsForWar()
{
    RegisterCreatureAI(npc_preparations_for_war_vehicle);
}
