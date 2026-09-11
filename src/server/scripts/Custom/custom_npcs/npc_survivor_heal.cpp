#include "ScriptMgr.h"
#include "ScriptedCreature.h"

enum SurvivorEvents
{
    EVENT_CHECK_HEALTH = 1
};

enum SurvivorTexts
{
    SAY_HEALING = 0
};

class npc_survivor_alliance : public CreatureScript
{
public:
    npc_survivor_alliance() : CreatureScript("npc_survivor_alliance") { }

    struct npc_survivor_allianceAI : public ScriptedAI
    {
        npc_survivor_allianceAI(Creature* creature) : ScriptedAI(creature) { }

        void Reset() override
        {
            events.Reset();

            me->SetRegenerateHealth(false);
            me->SetHealth(me->CountPctFromMaxHealth(5));

            events.ScheduleEvent(EVENT_CHECK_HEALTH, 1s);
        }

        void UpdateAI(uint32 diff) override
        {
            events.Update(diff);

            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case EVENT_CHECK_HEALTH:
                        if (me->GetHealthPct() >= 7.0f)
                        {
                            me->AI()->Talk(SAY_HEALING);
                            me->SetHealth(me->CountPctFromMaxHealth(5));
                        }

                        events.Repeat(1s);
                        break;

                    default:
                        break;
                }
            }
        }

    private:
        EventMap events;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_survivor_allianceAI(creature);
    }
};

void AddSC_npc_survivor_alliance()
{
    new npc_survivor_alliance();
}