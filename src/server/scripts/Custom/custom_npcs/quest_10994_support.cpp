#include "ScriptMgr.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellInfo.h"
#include "GameObject.h"
#include "GameObjectAI.h"

/*####
# npc_rizzle_sprysprocket_custom
####*/

enum RizzleSprysprocketData
{
    QUEST_CHASING_THE_MOONSTONE     = 10994,

    NPC_DEPTH_CHARGE_ENTRY          = 23025,

    SPELL_RIZZLE_BLACKJACK          = 39865,
    SPELL_RIZZLE_ESCAPE             = 39871,
    SPELL_RIZZLE_FROST_GRENADE      = 40525,
    SPELL_DEPTH_CHARGE_TRAP         = 38576,
    SPELL_PERIODIC_DEPTH_CHARGE     = 39912,
    SPELL_GIVE_SOUTHFURY_MOONSTONE  = 39886,

    SAY_RIZZLE_START                = 0,
    SAY_RIZZLE_GRENADE              = 1,
    SAY_RIZZLE_FINAL                = 2,
    MSG_ESCAPE_NOTICE               = 3,
    GOSSIP_MENU_GET_MOONSTONE       = 57025,
    GOSSIP_OPTION_GET_MOONSTONE     = 0
};

Position const WPs[58] =
{
    {3691.97f, -3962.41f, 35.9118f, 3.67f},
    {3675.02f, -3960.49f, 35.9118f, 3.67f},
    {3653.19f, -3958.33f, 33.9118f, 3.59f},
    {3621.12f, -3958.51f, 29.9118f, 3.48f},
    {3604.86f, -3963,     29.9118f, 3.48f},
    {3569.94f, -3970.25f, 29.9118f, 3.44f},
    {3541.03f, -3975.64f, 29.9118f, 3.41f},
    {3510.84f, -3978.71f, 29.9118f, 3.41f},
    {3472.7f,  -3997.07f, 29.9118f, 3.35f},
    {3439.15f, -4014.55f, 29.9118f, 3.29f},
    {3412.8f,  -4025.87f, 29.9118f, 3.25f},
    {3384.95f, -4038.04f, 29.9118f, 3.24f},
    {3346.77f, -4052.93f, 29.9118f, 3.22f},
    {3299.56f, -4071.59f, 29.9118f, 3.20f},
    {3261.22f, -4080.38f, 30.9118f, 3.19f},
    {3220.68f, -4083.09f, 31.9118f, 3.18f},
    {3187.11f, -4070.45f, 33.9118f, 3.16f},
    {3162.78f, -4062.75f, 33.9118f, 3.15f},
    {3136.09f, -4050.32f, 33.9118f, 3.07f},
    {3119.47f, -4044.51f, 36.0363f, 3.07f},
    {3098.95f, -4019.8f,  33.9118f, 3.07f},
    {3073.07f, -4011.42f, 33.9118f, 3.07f},
    {3051.71f, -3993.37f, 33.9118f, 3.02f},
    {3027.52f, -3978.6f,  33.9118f, 3.00f},
    {3003.78f, -3960.14f, 33.9118f, 2.98f},
    {2977.99f, -3941.98f, 31.9118f, 2.96f},
    {2964.57f, -3932.07f, 30.9118f, 2.96f},
    {2947.9f,  -3921.31f, 29.9118f, 2.96f},
    {2924.91f, -3910.8f,  29.9118f, 2.94f},
    {2903.04f, -3896.42f, 29.9118f, 2.93f},
    {2884.75f, -3874.03f, 29.9118f, 2.90f},
    {2868.19f, -3851.48f, 29.9118f, 2.82f},
    {2854.62f, -3819.72f, 29.9118f, 2.80f},
    {2825.53f, -3790.4f,  29.9118f, 2.744f},
    {2804.31f, -3773.05f, 29.9118f, 2.71f},
    {2769.78f, -3763.57f, 29.9118f, 2.70f},
    {2727.23f, -3745.92f, 30.9118f, 2.69f},
    {2680.12f, -3737.49f, 30.9118f, 2.67f},
    {2647.62f, -3739.94f, 30.9118f, 2.66f},
    {2616.6f,  -3745.75f, 30.9118f, 2.64f},
    {2589.38f, -3731.97f, 30.9118f, 2.61f},
    {2562.94f, -3722.35f, 31.9118f, 2.56f},
    {2521.05f, -3716.6f,  31.9118f, 2.55f},
    {2485.26f, -3706.67f, 31.9118f, 2.51f},
    {2458.93f, -3696.67f, 31.9118f, 2.51f},
    {2432,     -3692.03f, 31.9118f, 2.46f},
    {2399.59f, -3681.97f, 31.9118f, 2.45f},
    {2357.75f, -3666.6f,  31.9118f, 2.44f},
    {2311.99f, -3656.88f, 31.9118f, 2.94f},
    {2263.41f, -3649.55f, 31.9118f, 3.02f},
    {2209.05f, -3641.76f, 31.9118f, 2.99f},
    {2164.83f, -3637.64f, 31.9118f, 3.15f},
    {2122.42f,  -3639,    31.9118f, 3.21f},
    {2075.73f, -3643.59f, 31.9118f, 3.22f},
    {2033.59f, -3649.52f, 31.9118f, 3.42f},
    {1985.22f, -3662.99f, 31.9118f, 3.42f},
    {1927.09f, -3679.56f, 33.9118f, 3.42f},
    {1873.57f, -3695.32f, 33.9118f, 3.44f}
};

struct npc_rizzle_sprysprocket_custom : public ScriptedAI
{
    npc_rizzle_sprysprocket_custom(Creature* creature) : ScriptedAI(creature)  { }

    void Reset() override
    {
        _playerGUID.Clear();
        CurrWP = 0;
        Escape = false;
        Reached = false;
        ReachedPlayer = false;
        DespawnAfterWP = false;
        GrenadeTimer = 20000;
        SpellEscapeTimer = 0;
        TeleportTimer = 0;
        CheckTimer = 100;
        MustDieTimer = 0;
        MustDie = false;
        ContinueWP = false;
        _events.Reset();
        me->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
    }


    void IsSummonedBy(WorldObject* summoner) override
    {
        if (Player* player = summoner->ToPlayer())
            _playerGUID = player->GetGUID();

        _events.ScheduleEvent(1, 5s);
        _events.ScheduleEvent(2, 10s);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        if (!Reached)
        {
            if ((CurrWP + 1) % 5 == 0)
            {
                Position pos = me->GetPosition();
                me->SummonGameObject(185907, pos, QuaternionData(), Seconds(60));
            }

            ++CurrWP;
            ContinueWP = true;
        }

        if (id == 57)
        {
            if (!Reached)
                me->DespawnOrUnsummon();
            return;
        }
    }


    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
            case 1:
                if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                    Talk(SAY_RIZZLE_START, player);
                break;

            case 2:
            {
                Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
                if (!player)
                    break;

                Talk(MSG_ESCAPE_NOTICE, player);
                DoCast(player, SPELL_RIZZLE_BLACKJACK, true);

                me->SetHover(true);
                me->SetSwim(false);
                me->SetSpeedRate(MOVE_RUN, 0.85f);

                DoTeleportTo(3706.39f, -3969.15f, 35.9118f);
                DoCast(me, SPELL_PERIODIC_DEPTH_CHARGE, true);

                std::list<GameObject*> goList;
                me->GetGameObjectListWithEntryInGrid(goList, 185566, 100.0f);
                if (!goList.empty())
                    (*goList.begin())->DespawnOrUnsummon(1s, 60s);

                CurrWP = 0;
                me->GetMotionMaster()->MovePoint(CurrWP, WPs[CurrWP]);
                Escape = true;
                break;
                }
            }
        }

        if (!Escape)
            return;

        if (!Reached)
        {
            if (GrenadeTimer <= diff)
            {
                if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID))
                {
                    Talk(SAY_RIZZLE_GRENADE, player);
                    DoCast(player, SPELL_RIZZLE_FROST_GRENADE, true);
                }
                GrenadeTimer = 30000;
            }
            else
                GrenadeTimer -= diff;
        }
        if (ContinueWP)
        {
            me->GetMotionMaster()->MovePoint(CurrWP, WPs[CurrWP]);
            ContinueWP = false;
        }
        if (CheckTimer <= diff)
        {
            Player* player = ObjectAccessor::GetPlayer(*me, _playerGUID);
            if (!player)
            {
                me->DespawnOrUnsummon(2s);
                return;
            }

            if (me->IsWithinDist(player, 10.0f) && !Reached)
            {
                Talk(SAY_RIZZLE_FINAL);
                me->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
                me->GetMotionMaster()->MoveIdle();
                me->RemoveAurasDueToSpell(SPELL_PERIODIC_DEPTH_CHARGE);
                Reached = true;
            }

            CheckTimer = 100;
        }
        else
            CheckTimer -= diff;

        if (MustDie)
        {
            if (MustDieTimer <= diff)
            {
                me->DespawnOrUnsummon();
                return;
            }
            else
                MustDieTimer -= diff;
        }
    }

    bool OnGossipHello(Player* player) override
    {
        InitGossipMenuFor(player, GOSSIP_MENU_GET_MOONSTONE);
        if (player->GetQuestStatus(QUEST_CHASING_THE_MOONSTONE) != QUEST_STATUS_INCOMPLETE)
            return true;

        AddGossipItemFor(player, GOSSIP_MENU_GET_MOONSTONE, GOSSIP_OPTION_GET_MOONSTONE, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
        SendGossipMenuFor(player, 10811, me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 /*gossipListId*/) override
    {
        CloseGossipMenuFor(player);
        me->CastSpell(player, SPELL_GIVE_SOUTHFURY_MOONSTONE, true);
        MustDieTimer = 3000;
        MustDie = true;
        return false;
    }

private:
    ObjectGuid _playerGUID;
    uint32 SpellEscapeTimer;
    uint32 TeleportTimer;
    uint32 CheckTimer;
    uint32 GrenadeTimer;
    uint32 MustDieTimer;
    uint32 CurrWP;
    bool MustDie;
    bool Escape;
    bool ContinueWP;
    bool Reached;
    bool DespawnAfterWP;
    bool ReachedPlayer;
    EventMap _events;
};

CreatureAI* GetAI_npc_rizzle_sprysprocket_custom(Creature* creature)
{
    return new npc_rizzle_sprysprocket_custom(creature);
}

/*####
# npc_depth_charge_custom
####*/

struct npc_depth_charge_custom : public ScriptedAI
{
    npc_depth_charge_custom(Creature * creature) : ScriptedAI(creature)
    {
        Initialize();
    }

    void Initialize()
    {
        _weMustDie = false;
        _weMustDieTimer = 1000;
    }

    void Reset() override
    {
        me->SetHover(true);
        me->SetSwim(true);
        Initialize();
    }

    void JustEngagedWith(Unit* /*who*/) override {}
    void AttackStart(Unit* /*who*/) override {}

    void MoveInLineOfSight(Unit * who) override
    {
        if (!who)
            return;

        if (who->GetTypeId() == TYPEID_PLAYER && me->IsWithinDistInMap(who, 3))
        {
            DoCast(who, SPELL_DEPTH_CHARGE_TRAP);
            _weMustDie = true;
        }
    }

    void UpdateAI(uint32 diff) override
    {
        if (_weMustDie)
        {
            if (_weMustDieTimer <= diff)
                me->DespawnOrUnsummon();
            else
                _weMustDieTimer -= diff;
        }
    }

private:
    bool _weMustDie;
    uint32 _weMustDieTimer;
};

CreatureAI* GetAI_npc_depth_charge_custom(Creature* creature)
{
    return new npc_depth_charge_custom(creature);
}

void AddSC_quest_10994_support()
{
    RegisterCreatureAI(npc_rizzle_sprysprocket_custom);
    RegisterCreatureAI(npc_depth_charge_custom);
}
