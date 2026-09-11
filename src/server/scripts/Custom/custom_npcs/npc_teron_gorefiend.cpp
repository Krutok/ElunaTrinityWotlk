#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "Log.h"

enum MyConstants : uint32
{
    MY_QUEST_ID_ALLIANCE = 10645,
    MY_QUEST_ID_HORDE = 10639,
    SPELL_DISMEMBODIED_SPIRIT = 37782,
    SPELL_TERON_GOREFIEND = 37769,
    SPELL_HAST = 37728,
    SPELL_TERON_BLUTSCHATTEN = 37748,
    ENTRY_CHAIN_OF_SHADOWS = 21876,
    ENTRY_TERON_GOREFIEND = 21867,
    ENTRY_KARSIUS = 21877,
    ENTRY_VOICE_OF_GOREFIEND = 21872
};

struct npc_teron_gorefiendAI : public ScriptedAI
{
    npc_teron_gorefiendAI(Creature* creature) : ScriptedAI(creature) { Reset(); }

    uint32 phaseTimer = 0;
    uint8 phase = 0;
    ObjectGuid playerGuid;

    void Reset() override
    {
        phaseTimer = 0;
        phase = 0;
        playerGuid.Clear();
    }

    void OnQuestAccept(Player* player, Quest const* quest) override
    {
        if (quest->GetQuestId() == MY_QUEST_ID_ALLIANCE || quest->GetQuestId() == MY_QUEST_ID_HORDE)
        {
            playerGuid = player->GetGUID();
            Talk(0, player);
            phase = 1;
            phaseTimer = 1000;
        }
    }

    void OnCharmed(bool /*isNew*/) override
    {
        if (me->IsCharmed())
            return;

        Player* player = ObjectAccessor::FindPlayer(playerGuid);
        if (player)
        {
            player->RemoveAura(SPELL_TERON_BLUTSCHATTEN);
            player->RemoveAura(SPELL_DISMEMBODIED_SPIRIT);
        }

        DespawnSummons();

        // Karsius nur despawnen, wenn er lebt (vorzeitiges Verlassen)
        std::list<Creature*> karsiusList;
        me->GetCreatureListWithEntryInGrid(karsiusList, ENTRY_KARSIUS, 100.0f);
        for (Creature* karsius : karsiusList)
        {
            if (karsius->IsAlive())
                karsius->DespawnOrUnsummon();
        }

        me->DespawnOrUnsummon();
    }

    void DespawnSummons()
    {
        std::list<Creature*> creatures;

        // Chain of Shadows despawnen
        me->GetCreatureListWithEntryInGrid(creatures, ENTRY_CHAIN_OF_SHADOWS, 100.0f);
        for (Creature* c : creatures)
            c->DespawnOrUnsummon();

        creatures.clear();
    }

    void UpdateAI(uint32 diff) override
    {
        DoMeleeAttackIfReady();

        if (phase == 0)
            return;

        if (phaseTimer <= diff)
        {
            switch (phase)
            {
            case 1:
                me->SummonCreature(ENTRY_CHAIN_OF_SHADOWS, -4524.73f, 1009.76f, 21.3249f, 2.02458f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 300s);
                me->SummonCreature(ENTRY_CHAIN_OF_SHADOWS, -4515.91f, 1020.08f, 23.6738f, 2.72271f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 300s);
                me->SummonCreature(ENTRY_CHAIN_OF_SHADOWS, -4515.17f, 1033.11f, 20.7127f, 3.1765f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 300s);
                me->SummonCreature(ENTRY_CHAIN_OF_SHADOWS, -4525.0f, 1045.42f, 19.8945f, 4.15388f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 300s);
                me->SummonCreature(ENTRY_CHAIN_OF_SHADOWS, -4537.54f, 1049.36f, 18.7409f, 4.41568f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 300s);
                me->SummonCreature(ENTRY_CHAIN_OF_SHADOWS, -4551.17f, 1044.11f, 16.521f, 5.21853f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 300s);
                me->SummonCreature(ENTRY_CHAIN_OF_SHADOWS, -4523.75f, 1062.35f, 24.3041f, 4.43314f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 300s);
                me->SummonCreature(ENTRY_CHAIN_OF_SHADOWS, -4509.67f, 1047.06f, 26.4582f, 3.78736f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 300s);
                me->SummonCreature(ENTRY_CHAIN_OF_SHADOWS, -4504.67f, 1020.56f, 33.0728f, 2.93215f, TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, 300s);
                phaseTimer = 1000;
                phase = 2;
                break;

            case 2:
                if (auto karsius = me->SummonCreature(ENTRY_KARSIUS, -4535.79f, 1029.28f, 8.83636f, 3.78736f, TEMPSUMMON_MANUAL_DESPAWN))
                {
                    if (Player* player = ObjectAccessor::FindPlayer(playerGuid))
                        karsius->AI()->Talk(0, player);
                }
                phaseTimer = 1000;
                phase = 3;
                break;

            case 3:
                me->UpdateEntry(ENTRY_TERON_GOREFIEND);
                me->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                me->CastSpell(me, 37495, true);
                if (Player* player = ObjectAccessor::FindPlayer(playerGuid))
                {
                    player->CastSpell(player, SPELL_DISMEMBODIED_SPIRIT, false);
                    player->CastSpell(player, SPELL_TERON_GOREFIEND, true);
                }
                me->CastSpell(me, SPELL_HAST, true);
                phaseTimer = 1000;
                phase = 4;
                break;

            case 4:
                if (Creature* voice = me->FindNearestCreature(ENTRY_VOICE_OF_GOREFIEND, 100.0f))
                {
                    if (Player* player = ObjectAccessor::FindPlayer(playerGuid))
                        voice->AI()->Talk(0, player);
                }
                phase = 0;
                phaseTimer = 0;
                break;
            }
        }
        else
            phaseTimer -= diff;


    }
};

class npc_teron_gorefiend : public CreatureScript
{
public:
    npc_teron_gorefiend() : CreatureScript("npc_teron_gorefiend") {}

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_teron_gorefiendAI(creature);
    }
};

struct npc_karsiusAI : public ScriptedAI
{
    npc_karsiusAI(Creature* creature) : ScriptedAI(creature)
    {
        me->SetEmoteState(EMOTE_STATE_READY1H); // Emote beim Spawn
        immunityTimer = 3000;
    }

    uint32 immunityTimer;

    void Reset() override
    {
        immunityTimer = 3000;
        me->SetEmoteState(EMOTE_STATE_READY1H);
    }

    void UpdateAI(uint32 diff) override
    {
        if (immunityTimer <= diff)
        {
            me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC);

            Creature* teron = me->FindNearestCreature(MyConstants::ENTRY_TERON_GOREFIEND, 100.0f);
            if (teron && teron->IsAlive())
                teron->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC | UNIT_FLAG_IMMUNE_TO_NPC);

            immunityTimer = 3000;
        }
        else
            immunityTimer -= diff;

        if (!UpdateVictim())
            return;

        DoMeleeAttackIfReady();
    }

    void JustEngagedWith(Unit* who) override
    {
        AttackStart(who);
    }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(1); // Talk 1

        std::list<Creature*> shadows;
        me->GetCreatureListWithEntryInGrid(shadows, MyConstants::ENTRY_CHAIN_OF_SHADOWS, 200.0f);
        for (Creature* c : shadows)
            c->DespawnOrUnsummon();

        std::list<Player*> players;
        GetPlayerListInGrid(players, me, 100.0f);
        if (!players.empty())
        {
            Player* player = players.front();
            player->RemoveAura(MyConstants::SPELL_DISMEMBODIED_SPIRIT);
            player->RemoveAura(MyConstants::SPELL_TERON_BLUTSCHATTEN);
        }

        Creature* teron = me->FindNearestCreature(MyConstants::ENTRY_TERON_GOREFIEND, 50.0f);
        if (teron)
            teron->RemoveAura(MyConstants::SPELL_TERON_BLUTSCHATTEN);

        // Direkt nach Tod NPC 21867 an der Leiche spawnen
        me->SummonCreature(
            21867,
            me->GetPositionX(),
            me->GetPositionY(),
            me->GetPositionZ(),
            me->GetOrientation(),
            TEMPSUMMON_MANUAL_DESPAWN
        );
    }
};

class npc_karsius : public CreatureScript
{
public:
    npc_karsius() : CreatureScript("npc_karsius") {}

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_karsiusAI(creature);
    }
};

void AddSC_npc_teron_gorefiend()
{
    RegisterCreatureAI(npc_teron_gorefiendAI);
    RegisterCreatureAI(npc_karsiusAI);
}

