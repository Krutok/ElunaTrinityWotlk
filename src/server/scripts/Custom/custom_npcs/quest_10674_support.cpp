#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "Creature.h"
#include "GameObject.h"
#include <list>

enum Spells
{
    SPELL_TRIGGER = 28337,
    SPELL_SELF = 35426,
    SPELL_SUMMONER = 37903,
    GO_CUSTOM = 185011,

    NPC_ENTRY_1 = 20635,
    NPC_ENTRY_2 = 20771,
    NPC_TARGET = 21926,
    NPC_TARGET_HORDE = 22333,

};

struct quest_10674_support : public ScriptedAI
{
    quest_10674_support(Creature* creature) : ScriptedAI(creature), _summonerGUID(ObjectGuid::Empty), _isHorde(false)  { }

    void SetSummoner(Player* player)
    {
        if (player)
        {
            _summonerGUID = player->GetGUID();
            _isHorde = (player->GetTeam() == HORDE); // Fraktion vom Spieler übernehmen
        }
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spell) override
    {
        if (!spell || spell->Id != SPELL_TRIGGER)
            return;

        if (caster && (caster->GetEntry() == NPC_TARGET || caster->GetEntry() == 22333))
        {
            me->GetMotionMaster()->MoveIdle();
            me->GetMotionMaster()->MovePoint(1, caster->GetPosition());
        }
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE || id != 1)
            return;

        me->CastSpell(me, SPELL_SELF, false);

        if (!_summonerGUID.IsEmpty())
        {
            if (Player* player = ObjectAccessor::GetPlayer(*me, _summonerGUID))
                player->CastSpell(player, SPELL_SUMMONER, false);
        }
        if (!_isHorde)
        {
            if (GameObject* go = me->FindNearestGameObject(GO_CUSTOM, 2.0f))
                go->SetGoState(GO_STATE_READY);
        }
        if (!_isHorde)
        {
            if (Creature* npcTarget = me->FindNearestCreature(NPC_TARGET, 2.0f))
                npcTarget->DespawnOrUnsummon(5s);
        }
        else
        {
            if (Creature* npcTarget_horde = me->FindNearestCreature(NPC_TARGET_HORDE, 2.0f))
                npcTarget_horde->DespawnOrUnsummon(5s);
        }

        me->DespawnOrUnsummon(1s);
    }

private:
    ObjectGuid _summonerGUID;
    bool _isHorde;
};

struct npc_summon_bunny : public ScriptedAI
{
    npc_summon_bunny(Creature* creature) : ScriptedAI(creature), _hasCasted(false), _isHorde(false) {}

    void IsSummonedBy(WorldObject* summoner) override
    {
        Player* player = summoner ? summoner->ToPlayer() : nullptr;

        if (player)
            _isHorde = (player->GetTeam() == HORDE);
        else
            _isHorde = false;

        if (_isHorde)
        {
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        }

        if (!_isHorde)
        {
            if (GameObject* go = me->SummonGameObject(GO_CUSTOM, me->GetPosition(), QuaternionData(), 0s))
                go->SetGoState(GO_STATE_ACTIVE);
        }

        std::list<Creature*> targets;
        GetCreatureListWithEntryInGrid(targets, me, NPC_ENTRY_1, 40.0f);
        GetCreatureListWithEntryInGrid(targets, me, NPC_ENTRY_2, 40.0f);

        if (!_hasCasted && !targets.empty())
        {
            auto it = targets.begin();
            std::advance(it, rand() % targets.size());
            if (Creature* target = *it)
            {
                me->CastSpell(target, SPELL_TRIGGER, false);

                if (target->AI())
                {
                    if (auto ai = dynamic_cast<quest_10674_support*>(target->AI()))
                        ai->SetSummoner(player);
                }

                _hasCasted = true;
            }
        }
    }
private:
    bool _hasCasted;
    bool _isHorde;
};

void AddSC_quest_10674_support()
{
    RegisterCreatureAI(quest_10674_support);
    RegisterCreatureAI(npc_summon_bunny);
}
