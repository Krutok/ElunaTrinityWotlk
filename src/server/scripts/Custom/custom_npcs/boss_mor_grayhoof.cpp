#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Containers.h"

enum Texts
{
    SAY_AGGRO = 0,
    SAY_DEATH,
};

enum Spells
{
    // Druid spell
    SPELL_HURRICANE = 27530,
    SPELL_MOONFIRE = 27737,
    SPELL_SHOCK = 15605,

    // Healing spell
    SPELL_HEALING_TOUCH = 27527,
    SPELL_REJUVENATION = 27532,

    // Bear spell
    SPELL_BEAR_FORM = 27543,
    SPELL_DEMORALIZING_ROAR = 27551,
    SPELL_MAUL = 27553,
    SPELL_SWIPE = 27554,

    // Cat spell
    SPELL_CAT_FORM = 27545,
    SPELL_SHRED = 27555,
    SPELL_RAKE = 27556,
    SPELL_FEROCIOUS_BITE = 27557,

    // Dragon form
    SPELL_FAERIE_DRAGON_FORM = 27546,
    SPELL_ARCANE_EXPLOSION = 22271,
    SPELL_REFLECTION = 27564,
    SPELL_CHAIN_LIGHTNING = 27567,
    SPELL_SLEEP = 20663 // guessed
};

enum Phases
{
    PHASE_HUMAN = 1,
    PHASE_CAT,
    PHASE_BEAR,
    PHASE_FAERIE
};

enum Events
{
    EVENT_CAST_HUMAN = 1,
    EVENT_CAST_BEAR,
    EVENT_CAST_CAT,
    EVENT_CAST_FAERIE,
    EVENT_RESTORE_THREAT
};

enum DataTypes
{
    DATA_MOR_GRAYHOOF = 24,

};

static std::vector<uint32> catSpells = { SPELL_SHRED, SPELL_RAKE, SPELL_FEROCIOUS_BITE };
static std::vector<uint32> humanSpells = { SPELL_HURRICANE, SPELL_MOONFIRE, SPELL_SHOCK, SPELL_HEALING_TOUCH, SPELL_REJUVENATION };
static std::vector<uint32> bearSpells = { SPELL_DEMORALIZING_ROAR, SPELL_MAUL, SPELL_SWIPE };
static std::vector<uint32> faerieSpells = { SPELL_ARCANE_EXPLOSION, SPELL_REFLECTION, SPELL_CHAIN_LIGHTNING, SPELL_SLEEP };

struct boss_mor_grayhoof : public BossAI
{
    boss_mor_grayhoof(Creature* creature) : BossAI(creature, DATA_MOR_GRAYHOOF) {}

    void Reset() override
    {
        events.Reset();
        _phase = PHASE_HUMAN;
        _sleepTargetThreat = 0.f;
        _sleepTargetGUID.Clear();
    }

    void CastRandomSpell(uint8 phase)
    {
        uint32 spell = 0;

        switch (phase)
        {
        case PHASE_HUMAN:
            spell = Trinity::Containers::SelectRandomContainerElement(humanSpells);
            if (spell == SPELL_REJUVENATION || spell == SPELL_HEALING_TOUCH)
                DoCastSelf(spell);
            else
                DoCastAOE(spell);
            break;
        case PHASE_BEAR:
            spell = Trinity::Containers::SelectRandomContainerElement(bearSpells);
            DoCastVictim(spell);
            break;
        case PHASE_CAT:
            spell = Trinity::Containers::SelectRandomContainerElement(catSpells);
            DoCastVictim(spell);
            break;
        case PHASE_FAERIE:
        {
            spell = Trinity::Containers::SelectRandomContainerElement(faerieSpells);

            if (spell == SPELL_SLEEP)
            {
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 50.f, true))
                {
                    me->CastSpell(target, SPELL_SLEEP, false);

                    // Temporär Threat auf 0 setzen
                    _sleepTargetGUID = target->GetGUID();

                    // Threat über die moderne ThreatList skalieren
                    for (ThreatReference* ref : me->GetThreatManager().GetModifiableThreatList())
                    {
                        if (ref->GetVictim() == target)
                        {
                            ref->ScaleThreat(0.0f);
                            _sleepTargetThreat = ref->GetThreat(); // speichere alten Threat
                            break;
                        }
                    }

                    // Optional: +10s Event, um Threat zurückzusetzen
                    events.ScheduleEvent(EVENT_RESTORE_THREAT, 10s);
                }
            }
            else
            {
                DoCastVictim(spell);
            }
            break;
            }
        }
    }

    void DamageTaken(Unit* /*attacker*/, uint32& /*damage*/, DamageEffectType /*type*/, SpellInfo const* /*spellInfo*/) override
    {
        if (_phase == PHASE_HUMAN && me->HealthBelowPct(75))
        {
            _phase = PHASE_BEAR;
            me->CastStop();
            DoCastSelf(SPELL_BEAR_FORM);
            events.CancelEventGroup(PHASE_HUMAN);
            events.ScheduleEvent(EVENT_CAST_BEAR, 3s, 0, PHASE_BEAR);
        }
        else if (_phase == PHASE_BEAR && me->HealthBelowPct(50))
        {
            _phase = PHASE_CAT;
            me->CastStop();
            DoCastSelf(SPELL_CAT_FORM);
            events.CancelEventGroup(PHASE_BEAR);
            events.ScheduleEvent(EVENT_CAST_CAT, 3s, 0, PHASE_CAT);
        }
        else if (_phase == PHASE_CAT && me->HealthBelowPct(25))
        {
            _phase = PHASE_FAERIE;
            me->CastStop();
            DoCastSelf(SPELL_FAERIE_DRAGON_FORM);
            events.CancelEventGroup(PHASE_CAT);
            events.ScheduleEvent(EVENT_CAST_FAERIE, 5s, 0, PHASE_FAERIE);
        }
    }

    void JustEngagedWith(Unit* who) override
    {
        _JustEngagedWith(who);  // <-- hier den Parameter mitgeben
        Talk(SAY_AGGRO);

        events.ScheduleEvent(EVENT_CAST_HUMAN, 5s, 0, PHASE_HUMAN);
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
            case EVENT_CAST_HUMAN:
                CastRandomSpell(PHASE_HUMAN);
                events.Repeat(5s, 10s);
                break;
            case EVENT_CAST_BEAR:
                CastRandomSpell(PHASE_BEAR);
                events.Repeat(3s, 3s);
                break;
            case EVENT_CAST_CAT:
                CastRandomSpell(PHASE_CAT);
                events.Repeat(3s, 3s);
                break;
            case EVENT_CAST_FAERIE:
                CastRandomSpell(PHASE_FAERIE);
                events.Repeat(5s, 10s);
                break;
            case EVENT_RESTORE_THREAT:
                if (Unit* sleepTarget = ObjectAccessor::GetUnit(*me, _sleepTargetGUID))
                {
                    for (ThreatReference* ref : me->GetThreatManager().GetModifiableThreatList())
                    {
                        if (ref->GetVictim() == sleepTarget)
                        {
                            ref->AddThreat(_sleepTargetThreat); // alten Threat wiederherstellen
                            break;
                        }
                    }
                }
                break;
            }
        }

        DoMeleeAttackIfReady();
    }

    void JustDied(Unit* /*killer*/) override
    {
        _JustDied();
        Talk(SAY_DEATH);
    }

private:
    EventMap events;
    uint8 _phase = PHASE_HUMAN;
    ObjectGuid _sleepTargetGUID;
    float _sleepTargetThreat = 0.f;
};

void AddSC_boss_mor_grayhoof()
{
    RegisterCreatureAI(boss_mor_grayhoof);
}
