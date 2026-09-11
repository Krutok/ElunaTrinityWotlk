#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedEscortAI.h"
#include "Player.h"
#include "SpellMgr.h"
#include "SpellScript.h"
#include "ObjectAccessor.h"
#include "MotionMaster.h"
#include "Creature.h"
#include "Unit.h"
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "Log.h"
#include "WorldSession.h"


// === Globale Variablen / Maps ===

std::unordered_map<ObjectGuid, uint32> PlayerToDisplayIdMap;
std::unordered_map<ObjectGuid, std::vector<ObjectGuid>> PlayerSummonedNpcsMap;

constexpr uint32 SPELL_MOTIVATE_39623 = 73943; // Summon Spell 1
constexpr uint32 SPELL_MOTIVATE_39253 = 74080; // Summon Spell 2

constexpr uint32 groundMountDisplayId = 14376;  // Bodenmount
// === AI für Motivated Follower ===

class npc_motivated_follower : public CreatureScript
{
public:
    npc_motivated_follower() : CreatureScript("npc_motivated_follower") { }

    struct npc_motivated_followerAI : public ScriptedAI
    {
        npc_motivated_followerAI(Creature* creature) : ScriptedAI(creature), isSummoned(false), angleOffset(0.0f) {}

        ObjectGuid ownerGUID;
        bool isSummoned;
        float angleOffset;

        void IsSummonedBy(WorldObject* summoner) override
        {
            if (Player* player = summoner->ToPlayer())
            {
                isSummoned = true;
                ownerGUID = player->GetGUID();

                auto it = PlayerToDisplayIdMap.find(ownerGUID);
                if (it != PlayerToDisplayIdMap.end())
                {
                    me->SetDisplayId(it->second);
                    me->SetVisible(true);
                    PlayerToDisplayIdMap.erase(it);
                }
                else
                {
                    me->SetDisplayId(me->GetCreatureTemplate()->Modelid1);
                    me->SetVisible(true);
                }

                angleOffset = static_cast<float>(urand(0, 359)) * M_PI / 180.f;
                float dist = 2.5f;
                me->GetMotionMaster()->MoveFollow(player, dist, angleOffset);
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_motivated_followerAI(creature);
    }
};

// === AI für Motivated NPC ===

class npc_motivated_npc : public CreatureScript
{
public:
    npc_motivated_npc() : CreatureScript("npc_motivated_npc") { }

    struct npc_motivated_npcAI : public ScriptedAI
    {
        npc_motivated_npcAI(Creature* creature) : ScriptedAI(creature), isSummoned(false), isMounted(false), angleOffset(0.0f) {}

        ObjectGuid ownerGUID;
        bool isSummoned;
        bool isMounted;
        float angleOffset;

        void IsSummonedBy(WorldObject* summoner) override
        {
            if (!summoner || !summoner->IsPlayer())
                return;

            Player* player = summoner->ToPlayer();
            ownerGUID = player->GetGUID();
            isSummoned = true;

            PlayerSummonedNpcsMap[ownerGUID].push_back(me->GetGUID());

            auto itr = PlayerToDisplayIdMap.find(ownerGUID);
            if (itr != PlayerToDisplayIdMap.end())
            {
                me->SetDisplayId(itr->second);
                me->SetVisible(true);
                TC_LOG_INFO("custom", "Setze DisplayId %u fuer beschworenen NPC", itr->second);
            }

            angleOffset = static_cast<float>(urand(0, 359)) * M_PI / 180.f;
            me->CastSpell(me, 74034, true); // Buff mit 15 Min Dauer
        }

        void JustDied(Unit* /*killer*/) override
        {
            auto& list = PlayerSummonedNpcsMap[ownerGUID];
            list.erase(std::remove(list.begin(), list.end(), me->GetGUID()), list.end());
        }

        void OnRemove()
        {
            auto& list = PlayerSummonedNpcsMap[ownerGUID];
            list.erase(std::remove(list.begin(), list.end(), me->GetGUID()), list.end());
        }

        void UpdateAI(uint32 diff) override
        {
            (void)diff;

            if (!isSummoned)
                return;

            if (!me->HasAura(74034))
            {
                me->DespawnOrUnsummon();
                return;
            }

            Player* owner = ObjectAccessor::FindPlayer(ownerGUID);
            if (!owner)
            {
                me->DespawnOrUnsummon();
                return;
            }

            // Mount-Zustand synchronisieren
            bool playerMounted = owner->IsMounted();
            if (playerMounted && !isMounted)
            {
                me->SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, groundMountDisplayId);
                isMounted = true;
            }
            else if (!playerMounted && isMounted)
            {
                me->SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 0);
                isMounted = false;
            }

            // Bewegung zum Zielpunkt
            const float radius = 6.0f;
            float x = owner->GetPositionX() + radius * cos(angleOffset);
            float y = owner->GetPositionY() + radius * sin(angleOffset);
            float z = owner->GetPositionZ();

            if (me->GetDistance(x, y, z) > 0.5f)
                me->GetMotionMaster()->MovePoint(0, x, y, z);
            else
                me->GetMotionMaster()->Clear();

            // NPC 39675 in LOS & 10 Yards => KillCredit 39466 + Despawn
            std::list<Creature*> targetList;
            me->GetCreatureListWithEntryInGrid(targetList, 39675, 10.0f);

            for (Creature* npc : targetList)
            {
                if (me->IsWithinLOSInMap(npc))
                {
                    owner->KilledMonsterCredit(39466);
                    me->DespawnOrUnsummon();
                    return;
                }
            }
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_motivated_npcAI(creature);
    }
};

// SpellScript: Motivates NPCs and summons copies with limit
class spell_motivate_npc : public SpellScript
{
    PrepareSpellScript(spell_motivate_npc);

    void HandleScriptEffect(SpellEffIndex /*effIndex*/)
    {
        Creature* target = GetHitCreature();
        Unit* caster = GetCaster();
        Player* player = caster->ToPlayer();

        if (!target || !player)
            return;

        // Alte/despawnte NPCs aus Liste entfernen
        auto& summonedList = PlayerSummonedNpcsMap[player->GetGUID()];
        summonedList.erase(
            std::remove_if(summonedList.begin(), summonedList.end(),
                [&](ObjectGuid guid)
                {
                    Creature* c = ObjectAccessor::GetCreature(*player, guid);
                    return !c || !c->IsInWorld();
                }),
            summonedList.end());

        if (summonedList.size() >= 5)
        {
            player->GetSession()->SendNotification("Maximal 5 motivierte NPCs erlaubt.");
            return;
        }

        uint32 targetEntry = target->GetEntry();
        uint32 summonSpell = 0;

        switch (targetEntry)
        {
        case 39623:
            summonSpell = SPELL_MOTIVATE_39623;
            break;
        case 39253:
            summonSpell = SPELL_MOTIVATE_39253;
            break;
        default:
            player->GetSession()->SendNotification("Dieser NPC kann nicht motiviert werden.");
            return;
        }

        // DisplayID speichern
        PlayerToDisplayIdMap[player->GetGUID()] = target->GetDisplayId();

        // Summon-Spell casten
        player->CastSpell(player, summonSpell, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_motivate_npc::HandleScriptEffect, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};


// === Registrierung ===

void AddSC_motivated_summons()
{
    RegisterSpellScript(spell_motivate_npc);
    new npc_motivated_follower();
    new npc_motivated_npc();
}
