#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-source-encoding"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "ScriptedCreature.h"
#include "Log.h"

// A global bool that tracks if there's a battle in progress. so we don't have all bosses fighting at the same time in the arena.
bool g_BattleInProgress = false;

namespace
{
    // Druid Boss
    enum eDruidSpells
    {
        BEAR_FORM = 9634,
        MAUL = 6807,
        DEMO_ROAR = 48560,
        MANGLE = 48564,
        AUFSCHLITZEN = 48568,
        PRANKENHIEB = 48562
    };
    enum eDruidEvents
    {
        EVENT_MAUL = 4,
        EVENT_DEMOROAR = 5,
        EVENT_MANGLE = 6,
        EVENT_AUFSCHLITZEN = 7,
        EVENT_PRANKENHIEB = 8
    };

    // Warlock Boss
    enum eWarlpells
    {
        DEMON_ARMOR = 58453,
        CORRUPTION = 65810,
        SHADOW_FLAME = 22539,
        SHADOW_BOLT = 75384,
        WICHTEL_PET = 688,
        MANA_REG_WARLOCK = 57669
    };
    enum eWarlEvents
    {
        EVENT_CORRUPTION = 4,
        EVENT_SHADOWFLAME = 5,
        EVENT_SHADOWBOLT = 6,
        EVENT_PET = 7,
        EVENT_MANA_REG_WARLOCK = 8
    };

    // Paladin Boss
    enum ePalaSpells
    {
        RETRIBUTION_AURA = 7294,
        HOJ = 853,
        CRUSADER_STRIKE = 35395,
        DIVINE_STROM = 53385,
        MANA_REG_PALADIN = 57669
    };
    enum ePalaEvents
    {
        EVENT_HOJ = 4,
        EVENT_CRUSADER_STRIKE = 5,
        EVENT_DIVINE_STORM = 6,
        EVENT_MANA_REG_PALADIN = 7
    };

    // Boss Ids
    enum eBosses
    {
        BOSS_DRUID = 100005,
        BOSS_WARLOCK = 100006,
        BOSS_PALADIN = 100007
    };

    enum eGossip
    {
        CHALLENGE = 1,
        EXIT = 2
    };

    enum eEvents
    {
        EVENT_MOVE_TO_CENTER = 1,
        EVENT_TALK = 2,
        EVENT_ENGAGE_IN_BATTLE = 3,
        EVENT_CHECK_IF_IN_CENTER = 4
    };

    enum ePosition
    {
        CENTER_OF_RING = 1
    };
    std::unordered_map<ePosition, Position> m_Pos =
    {
        {CENTER_OF_RING, {-7139.436f, -3784.763f, 8.923f}},
    };

    // max allowed distance from center of the ring if npc pos exceedes that distance then it will reset
    constexpr float maxAllowedDist = 23.f;

    const uint32 gossip_action_def = GOSSIP_ACTION_INFO_DEF;
    const uint32 gossip_exit = EXIT;
    const uint32 gossip_challege = CHALLENGE;
}

class npc_gadgetzan_arena_boss : public CreatureScript
{
public:
    npc_gadgetzan_arena_boss() : CreatureScript("npc_gadgetzan_arena_boss") { }

    struct npc_gadgetzan_arena_bossAI : public ScriptedAI
    {
        npc_gadgetzan_arena_bossAI(Creature* creature) : ScriptedAI(creature)
        {
            Initialize();
        }

        // When the boss has spawned in world or reset
        void Initialize()
        {
            me->SetFaction(FACTION_FRIENDLY);
            m_Target.Clear();
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
            m_MoveToCenter = false;
            m_Battle = false;
            g_BattleInProgress = false;

            // despawn any previous summoned pets
            if (auto pet = me->GetGuardianPet())
                pet->DisappearAndDie();
        }

        // When the boss resets (leaves combat)
        void Reset() override
        {
            Initialize();
            m_Events.Reset();
        }

        // When the boss has just died
        void JustDied(Unit* /*killer*/) override
        {
            me->Say("Ich wurde besiegt..", LANG_UNIVERSAL);
        }

        bool HasReached(ePosition pos)
        {
            return (me->GetPosition().m_positionX == m_Pos[pos].m_positionX &&
                me->GetPosition().m_positionY == m_Pos[pos].m_positionY);
        }

        void MoveInLineOfSight(Unit* who) override
        {
            if (me->IsInCombat() || g_BattleInProgress)
                return;

            if (lastTimeGreeted < time(nullptr))
            {
                const float dist = 2.0f; // distance between player and npc if player is within distance the npc will say something
                const uint8 sayCooldown = 20; // in seconds
                if (who->GetTypeId() == TYPEID_PLAYER)
                {
                    if (who->GetExactDist2d(me) <= dist)
                    {
                        me->Say("Willkommen in der Gadgetzan Arena. Wir sind die neuen Gladiatoren.", LANG_UNIVERSAL);
                        lastTimeGreeted = time(nullptr) + sayCooldown;
                    }
                }
            }
        }

        void CastStance()
        {
            switch (me->GetEntry())
            {
            case BOSS_DRUID:
                me->Say("Bei Elune, ich werde nicht scheitern.", LANG_UNIVERSAL);
                me->CastSpell(me, BEAR_FORM);
                break;
            case BOSS_WARLOCK:
                me->Say("Schatten umarmen dich.", LANG_UNIVERSAL);
                me->CastSpell(me, DEMON_ARMOR);
                break;
            case BOSS_PALADIN:
                me->Say("Wie Licht mein Inneres ist, werde ich dich töten.", LANG_UNIVERSAL);
                me->CastSpell(me, RETRIBUTION_AURA);
                break;
            }

            // Schedule spells to be casted
            ScheduleSpells();
        }

        void ScheduleSpells()
        {
            switch (me->GetEntry())
            {
            case BOSS_DRUID:
                m_Events.ScheduleEvent(EVENT_MAUL, 4s, 6s);
                m_Events.ScheduleEvent(EVENT_DEMOROAR, 7s, 8s);
                m_Events.ScheduleEvent(EVENT_MANGLE, 3s, 4s);
                m_Events.ScheduleEvent(EVENT_AUFSCHLITZEN, 4s, 5s);
                m_Events.ScheduleEvent(EVENT_PRANKENHIEB, 6s, 7s);
                break;
            case BOSS_WARLOCK:
                m_Events.ScheduleEvent(EVENT_CORRUPTION, 18s, 19s);
                m_Events.ScheduleEvent(EVENT_SHADOWFLAME, 7s, 8s);
                m_Events.ScheduleEvent(EVENT_SHADOWBOLT, 3s, 4s);
                m_Events.ScheduleEvent(EVENT_PET, 1s, 1s);
                m_Events.ScheduleEvent(EVENT_MANA_REG_WARLOCK, 4s, 6s);
                break;
            case BOSS_PALADIN:
                m_Events.ScheduleEvent(EVENT_HOJ, 7s, 8s);
                m_Events.ScheduleEvent(EVENT_CRUSADER_STRIKE, 3s, 4s);
                m_Events.ScheduleEvent(EVENT_DIVINE_STORM, 4s, 5s);
                m_Events.ScheduleEvent(EVENT_MANA_REG_PALADIN, 4s, 6s);
                break;
            }
        }

        void CastSpells(uint32 eventId)
        {
            if (!eventId) return;

            // Spells for bosses
            switch (me->GetEntry())
            {
            case BOSS_DRUID:
                switch (eventId)
                {
                case EVENT_MAUL:
                    DoCastVictim(MAUL);
                    m_Events.ScheduleEvent(EVENT_MAUL, 4s, 6s);
                    break;
                case EVENT_DEMOROAR:
                    DoCastVictim(DEMO_ROAR);
                    m_Events.ScheduleEvent(EVENT_DEMOROAR, 7s, 8s);
                    break;
                case EVENT_MANGLE:
                    DoCastVictim(MANGLE);
                    m_Events.ScheduleEvent(EVENT_MANGLE, 3s, 3s);
                    break;
                case EVENT_AUFSCHLITZEN:
                    DoCastVictim(AUFSCHLITZEN);
                    m_Events.ScheduleEvent(EVENT_AUFSCHLITZEN, 4s, 5s);
                    break;
                case EVENT_PRANKENHIEB:
                    DoCastVictim(PRANKENHIEB);
                    m_Events.ScheduleEvent(EVENT_PRANKENHIEB, 6s, 7s);
                    break;
                }
                break;
            case BOSS_WARLOCK:
                switch (eventId)
                {
                case EVENT_CORRUPTION:
                    DoCastVictim(CORRUPTION);
                    m_Events.ScheduleEvent(EVENT_CORRUPTION, 4s, 6s);
                    break;
                case EVENT_SHADOWFLAME:
                    DoCastVictim(SHADOW_FLAME);
                    m_Events.ScheduleEvent(EVENT_SHADOWFLAME, 7s, 8s);
                    break;
                case EVENT_SHADOWBOLT:
                    DoCastVictim(SHADOW_BOLT);
                    m_Events.ScheduleEvent(EVENT_SHADOWBOLT, 2s, 3s);
                    break;
                case EVENT_MANA_REG_WARLOCK:
                    me->CastSpell(me, MANA_REG_WARLOCK);
                    m_Events.ScheduleEvent(MANA_REG_WARLOCK, 4s, 6s);
                    break;
                case EVENT_PET:
                    if (!me->GetGuardianPet())
                    {
                        me->CastSpell(me, WICHTEL_PET, true);
                        m_Events.ScheduleEvent(EVENT_PET, 5s, 9s);
                    }
                    break;
                }
                break;
            case BOSS_PALADIN:
                switch (eventId)
                {
                case EVENT_HOJ:
                    DoCastVictim(HOJ);
                    m_Events.ScheduleEvent(EVENT_HOJ, 7s, 8s);
                    break;
                case EVENT_CRUSADER_STRIKE:
                    DoCastVictim(CRUSADER_STRIKE);
                    m_Events.ScheduleEvent(EVENT_CRUSADER_STRIKE, 3s, 4s);
                    break;
                case EVENT_DIVINE_STORM:
                    DoCastVictim(DIVINE_STROM);
                    m_Events.ScheduleEvent(EVENT_DIVINE_STORM, 4s, 5s);
                    break;
                case EVENT_MANA_REG_PALADIN:
                    me->CastSpell(me, MANA_REG_PALADIN);
                    m_Events.ScheduleEvent(MANA_REG_WARLOCK, 4s, 6s);
                    break;    
                }
                break;
            }
        }

        std::vector<Player*> GetAttackersInRing()
        {
            std::vector<Player*> playersNearby, attackersInRing;
            GetPlayerListInGrid(playersNearby, me, 30.0f);

            for (const auto& players : playersNearby)
            {
                // attacker must be valid and a player
                if (!players)
                    continue;

                // attacker must be inside the ring
                if (players->GetDistance(m_Pos[CENTER_OF_RING]) < maxAllowedDist)
                    attackersInRing.push_back(players->ToPlayer());
            }

            return attackersInRing;
        }

        // Update bossAI
        void UpdateAI(uint32 diff) override
        {
            if (me->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP))
                return;

            if (!m_Battle && !UpdateVictim())
            {
                Reset();
                return;
            }

            m_Events.Update(diff);

            // if already casting don't execute any events
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            while (uint32 eventId = m_Events.ExecuteEvent())
            {
                switch (eventId)
                {
                case EVENT_MOVE_TO_CENTER:
                    if (HasReached(CENTER_OF_RING))
                    {
                        me->SetWalk(false);
                        m_Events.ScheduleEvent(EVENT_TALK, 1s, 1s);
                        me->SetFacingTo(5.962f); // face towards the gate
                    }
                    else
                    {
                        if (!m_MoveToCenter)
                        {
                            me->GetMotionMaster()->MovePoint(1, m_Pos[CENTER_OF_RING]);
                            m_MoveToCenter = true;
                        }
                        m_Events.ScheduleEvent(EVENT_MOVE_TO_CENTER, 1s, 1s);
                    }
                    break;
                case EVENT_TALK:
                    CastStance();
                    me->SetFaction(FACTION_CREATURE);
                    m_Events.ScheduleEvent(EVENT_ENGAGE_IN_BATTLE, 1s, 1s);
                    break;
                case EVENT_ENGAGE_IN_BATTLE:
                    if (Player* target = ObjectAccessor::FindPlayer(m_Target))
                    {
                        me->AI()->AttackStart(target);
                        m_Battle = false;
                        m_Events.ScheduleEvent(EVENT_CHECK_IF_IN_CENTER, 3s, 3s);
                    }
                    break;
                case EVENT_CHECK_IF_IN_CENTER:
                    if (me->IsInCombat())
                    {
                        // get a vector of all the attackers inside the ring
                        auto attackersInRing = GetAttackersInRing();

                        // if there are no attackers inside the ring Reset.
                        if (attackersInRing.empty())
                        {
                            me->AttackStop();
                            me->AI()->EnterEvadeMode();
                            Reset();
                            return;
                        }
                        else
                        {
                            if (auto target = me->GetVictim())
                            {
                                // my current target is outside the ring attack one of my attackers that is inside the ring
                                if (target->GetDistance(m_Pos[CENTER_OF_RING]) >= maxAllowedDist)
                                {
                                    for (const auto& newTarget : attackersInRing)
                                    {
                                        AddThreat(newTarget, 1000000.0f);
                                        me->AI()->AttackStart(newTarget);
                                        ResetThreat(target);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                    m_Events.ScheduleEvent(EVENT_CHECK_IF_IN_CENTER, 1s, 1s);
                    break;
                default:
                    CastSpells(eventId);
                    break;
                }
            }

            // if we're not casting let's melee attack
            if (!me->HasUnitState(UNIT_STATE_CASTING))
                DoMeleeAttackIfReady();
        }

        bool OnGossipHello(Player* player) override
        {
            if (me->isMoving())
                return true;

            if (g_BattleInProgress)
            {
                me->Whisper("Es scheint, dass bereits ein Kampf im Gange ist. Warten wir, bis sie fertig sind.", LANG_UNIVERSAL, player);
                return true;
            }

            if (player->GetLevel() < 80)
                AddGossipItemFor(player, 60000, 1, GOSSIP_SENDER_MAIN, gossip_action_def + gossip_exit);
            else
                AddGossipItemFor(player, 60000, 0, GOSSIP_SENDER_MAIN, gossip_action_def + gossip_challege);

            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
            return true;
        }

        bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            const uint32 action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
            ClearGossipMenuFor(player);



            switch (action)
            {
                case gossip_action_def + gossip_challege:
                g_BattleInProgress = true;
                me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP); // remove gossip flag
                CloseGossipMenuFor(player);
                m_Target = player->GetGUID();
                m_Battle = true;
                me->Say("Sehr gut, ich nehme Ihre Herausforderung an.", LANG_UNIVERSAL);
                me->SetWalk(true);
                m_Events.ScheduleEvent(EVENT_MOVE_TO_CENTER, 2s, 2s);
                break;
                case gossip_action_def + gossip_exit:
                CloseGossipMenuFor(player);
                break;
            }
            return true;
        }

    private:
        time_t lastTimeGreeted;
        EventMap m_Events;
        ObjectGuid m_Target;
        bool m_MoveToCenter = false, m_Battle = false;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_gadgetzan_arena_bossAI(creature);
    }
};

void AddSC_Gadgetzan_ArenaBoss()
{
    new npc_gadgetzan_arena_boss();
}