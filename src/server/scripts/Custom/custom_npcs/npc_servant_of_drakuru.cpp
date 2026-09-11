#include "ScriptMgr.h"
#include "Creature.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "Player.h"
#include "ScriptedCreature.h"

enum
{
    SPELL_TRIGGER_ID                = 52390,
    NPC_ENTRY_CONTROLLED            = 28805,
    NPC_ENTRY_NORMAL                = 28802,
    QUEST_ID                        = 12686,
    SPELL_CAST                      = 50361,

    EVENT_CAST_SPELL                = 1,
    EVENT_CONTROL_CHECK             = 2,
    TALK_DRAKURU                    = 0
};

struct npc_servant_of_drakuru : public ScriptedAI
{
    npc_servant_of_drakuru(Creature* creature) : ScriptedAI(creature), _controller(nullptr), _controlled(false) { }

    void JustEngagedWith(Unit* /*who*/) override
    {
        _events.Reset();
        _events.ScheduleEvent(EVENT_CAST_SPELL, 4s);
        _events.ScheduleEvent(EVENT_CONTROL_CHECK, 1s);
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spell) override
    {
        if (spell->Id == SPELL_TRIGGER_ID)
        {
            if (Player* player = caster->ToPlayer())
            {
                if (player->GetQuestStatus(QUEST_ID) == QUEST_STATUS_INCOMPLETE)
                {
                    _controller = player;
                    Talk(TALK_DRAKURU);
                    me->UpdateEntry(NPC_ENTRY_CONTROLLED);
                    me->SetHealth(me->GetMaxHealth());
                    _controlled = true;
                }
            }
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_CAST_SPELL:
                if (!_controlled)
                {
                    if (Unit* target = me->GetVictim())
                        DoCastVictim(SPELL_CAST, true);
                }
                _events.Repeat(12s, 14s);
                break;

            case EVENT_CONTROL_CHECK:
                if (_controlled)
                {
                    if (me->GetCharmerOrOwner() != _controller)
                    {
                        if (_controller && _controller->GetQuestStatus(QUEST_ID) == QUEST_STATUS_INCOMPLETE)
                            me->DespawnOrUnsummon(1s, 3s);

                        _controlled = false;
                        _controller = nullptr;
                        _events.Reset();
                        return;
                    }
                }
                _events.Repeat(1s);
                break;
            }
        }

        if (!_controlled)
            ScriptedAI::UpdateAI(diff);
    }

private:
    EventMap _events;
    Player* _controller;
    bool _controlled;
};

void AddSC_npc_servant_of_drakuru()
{
    RegisterCreatureAI(npc_servant_of_drakuru);
}
