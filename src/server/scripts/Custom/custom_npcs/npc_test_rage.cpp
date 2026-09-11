#include "ScriptMgr.h"
#include "ScriptedCreature.h"

enum TestNpcSpells
{
    SPELL_TEST = 71515 // Testspell
};

enum TestNpcEvents
{
    EVENT_GAIN_RAGE = 1
};

struct npc_test_rage : public ScriptedAI
{
    npc_test_rage(Creature* creature) : ScriptedAI(creature) { }

    void Reset() override
    {
        events.Reset();

        me->SetPowerType(POWER_RAGE);
        me->SetMaxPower(POWER_RAGE, 1000);
        me->SetPower(POWER_RAGE, 0);

        events.ScheduleEvent(EVENT_GAIN_RAGE, 5s);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_GAIN_RAGE:
                {
                    me->ModifyPower(POWER_RAGE, 50); // +5 Rage

                    if (me->GetPower(POWER_RAGE) >= 1000)
                    {
                        DoCast(me, SPELL_TEST, true);
                        me->SetPower(POWER_RAGE, 0);
                    }

                    events.Repeat(5s);
                    break;
                }
            }
        }

        DoMeleeAttackIfReady();
    }

private:
    EventMap events;
};

void AddSC_test_rage()
{
    RegisterCreatureAI(npc_test_rage);
}