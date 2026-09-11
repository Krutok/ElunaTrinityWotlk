#include "ScriptedCreature.h"

enum Texts
{
    SAY_FIRST = 0,
    SAY_SECOND,
    SAY_THIRD,
    SAY_FOURTH,
    SAY_FIFTH,
    SAY_SIXTH,
};

enum BossEvents
{
    EVENT_ENTER_COMBAT = 1,
    EVENT_SPEAK,

    EVENT_SUMMON_FOUR,

    EVENT_INIT_PHASE_ONE,
    EVENT_INIT_PHASE_TWO,
    EVENT_INIT_PHASE_THREE,
    EVENT_INIT_PHASE_FOUR,

    EVENT_CAST_FIREBALL,
    EVENT_CAST_FIRE_BREATH,
    EVENT_CAST_RAIN_OF_FIRE,
    EVENT_CAST_FEAR,
    EVENT_CAST_RAGE,
    EVENT_BERSERK,
};

enum SummonEvents
{
    EVENT_SUMMON_CAST = 1,
};

enum Spells
{
    SPELL_FIREBOLT = 59468,
    SPELL_FIRE_BREATH = 74527,
    SPELL_FIRE_RAIN = 47820,
    SPELL_FEAR = 39415,

    SPELL_IMMUNITY = 40733,
    SPELL_RAGE = 48142,

    SPELL_VISUAL_BLOOD = 35847,
    SPELL_VISUAL_HELLFIRE = 32475,

    SPELL_BERSERK = 26662
};

enum Phases
{
    PHASE_ALL = 1,
    PHASE_ONE,
    PHASE_TWO,
    PHASE_THREE,
    PHASE_FOUR,
};

enum Creatures
{
    BOSS_DRAGONKIN = 600100,
    BOSS_GNOME     = 600102,

    FIRE_ELEMENTAL = 600101
};

struct boss_dragonkin : public ScriptedAI
{
public:
    boss_dragonkin(Creature* creature) : ScriptedAI(creature) {}

    void Initialize()
    {
        events.Reset();
        speechCount = 0;
        isImmune = false;
        me->UpdateEntry(BOSS_GNOME);
        me->RemoveAllAuras();
    }

    void Reset() override
    {
        for (const auto& summ : summonedGuids)
        {
            Creature* creature = me->GetMap()->GetCreature(summ);
            if (creature && creature->IsAlive())
            {
                creature->DespawnOrUnsummon();
            }
        }
        Initialize();
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        if (!me->IsAlive())
            return;

        me->CombatStop(true);
        EngagementOver();
        me->GetMotionMaster()->MoveTargetedHome();

        Reset();
    }

    void JustExitedCombat() override
    {
        EnterEvadeMode(EVADE_REASON_NO_HOSTILES);
    }

    void JustEngagedWith(Unit* who) override
    {
        if (!me->IsValidAttackTarget(who))
            return;

        AttackStart(who);
    }

    void AttackStart(Unit* target) override
    {
        if (!target)
            return;

        me->GetMotionMaster()->MoveCloserAndStop(1, target, 5.0f);
        me->AttackStop();

        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        me->SetFlag(UNIT_FIELD_FLAGS, GO_FLAG_NOT_SELECTABLE);

        events.ScheduleEvent(EVENT_ENTER_COMBAT, 1s);
        events.ScheduleEvent(EVENT_BERSERK, 360s, 360s); // Berserker in 6 Minuten
    }

    void JustDied(Unit* /*killer*/) override
    {
        me->CombatStop(true);
        EngagementOver();
        Reset();
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/ , SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (me->HealthBelowPctDamaged(66, damage) && events.IsInPhase(PHASE_ONE))
        {
            events.RemovePhase(PHASE_ONE);
            events.SetPhase(PHASE_TWO);
            events.ScheduleEvent(EVENT_INIT_PHASE_TWO, 1s);
            if (auto aura = me->GetAura(SPELL_IMMUNITY))
            {
                me->RemoveAura(aura);
                isImmune = false;
            }
        }
        else if (me->HealthBelowPctDamaged(33, damage) && events.IsInPhase(PHASE_TWO))
        {
            events.RemovePhase(PHASE_TWO);
            events.SetPhase(PHASE_THREE);
            events.ScheduleEvent(EVENT_INIT_PHASE_THREE, 1s);
        }
        else if (me->HealthBelowPctDamaged(10, damage) && events.IsInPhase(PHASE_THREE))
        {
            me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
            me->SetFlag(UNIT_FIELD_FLAGS, GO_FLAG_NOT_SELECTABLE);

            events.RemovePhase(PHASE_THREE);
            events.SetPhase(PHASE_FOUR);
            events.ScheduleEvent(EVENT_INIT_PHASE_FOUR, 1s);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);

        if (events.IsInPhase(PHASE_ONE))
        {
            if (isImmune)
            {
                m_timer += diff;
                if (m_timer >= 3000)
                {
                    uint8 spawnsCount = 0;
                    for (const auto& guid : summonedGuids)
                    {
                        if (auto creature = me->GetMap()->GetCreature(guid))
                        {
                            if (creature->IsAlive())
                                spawnsCount++;
                        }
                    }
                    if (spawnsCount == 0)
                    {
                        isImmune = false;
                        if (auto aura = me->GetAura(SPELL_IMMUNITY))
                            me->RemoveAura(aura, AURA_REMOVE_BY_DEFAULT);
                    }
                    m_timer = 0;
                }
            }
        }

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_ENTER_COMBAT:
            {
                me->CastSpell(me, SPELL_VISUAL_BLOOD);
                if (speechCount < 3)
                    events.ScheduleEvent(EVENT_SPEAK, 0s);

                events.ScheduleEvent(EVENT_INIT_PHASE_ONE, 7s);
                events.SetPhase(PHASE_ONE);
            }
            break;

            case EVENT_SPEAK:
                if (events.IsInPhase(PHASE_ONE) && speechCount < 3)
                {
                    Talk(static_cast<Texts>(speechCount++));
                    events.ScheduleEvent(EVENT_SPEAK, 2s);
                }
                else if (events.IsInPhase(PHASE_FOUR) && speechCount < 6)
                {
                    Talk(static_cast<Texts>(speechCount++));
                    events.ScheduleEvent(EVENT_SPEAK, 2s);
                }

            break;

            case EVENT_INIT_PHASE_ONE:
            {
                //me->CastSpell(me, SPELL_VISUAL_HELLFIRE);
                auto nearest = me->SelectNearestPlayer(100.0f);
                me->GetMotionMaster()->MoveChase(nearest);
                ScriptedAI::AttackStart(nearest);
                me->Attack(nearest, true);
                events.ScheduleEvent(EVENT_SUMMON_FOUR, 1s);
                events.ScheduleEvent(EVENT_CAST_FIRE_BREATH, 4s);
                events.ScheduleEvent(EVENT_CAST_FIREBALL, 2s);
                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                me->RemoveFlag(UNIT_FIELD_FLAGS, GO_FLAG_NOT_SELECTABLE);
                me->UpdateEntry(BOSS_DRAGONKIN);
                me->SetHealth(me->GetMaxHealth());
            }
            break;

            case EVENT_INIT_PHASE_TWO:
            {
                events.CancelEvent(EVENT_SUMMON_FOUR);
                events.ScheduleEvent(EVENT_SUMMON_FOUR, 1s);
                events.ScheduleEvent(EVENT_CAST_RAIN_OF_FIRE, 10s);
            }
            break;

            case EVENT_INIT_PHASE_THREE:
            {
                events.CancelEvent(EVENT_SUMMON_FOUR);
                events.ScheduleEvent(EVENT_SUMMON_FOUR, 1s);
                events.ScheduleEvent(EVENT_CAST_FEAR, 5s);
            }
            break;

            case EVENT_INIT_PHASE_FOUR:
            {
                events.ScheduleEvent(EVENT_CAST_RAGE, 1s);
                events.ScheduleEvent(EVENT_SPEAK, 1s);
            }
            break;
            
            case EVENT_BERSERK:
                    DoCastSelf(SPELL_BERSERK);
                    break;

            case EVENT_SUMMON_FOUR:
            {
                for (int i = 0; i < 4; i++)
                {
                    auto randomPlayer = SelectTarget(SelectTargetMethod::Random);
                    auto summon = me->SummonCreature(FIRE_ELEMENTAL, randomPlayer->GetRandomNearPosition(20.0f), TEMPSUMMON_TIMED_OR_CORPSE_DESPAWN, Milliseconds(60000));
                    summon->GetAI()->AttackStart(randomPlayer);

                    summonedGuids.push_back(summon->GetGUID());
                    m_timer = 0;
                }
                if (events.IsInPhase(PHASE_ONE) && !me->HasAura(SPELL_IMMUNITY))
                {
                    me->CastSpell(me, SPELL_IMMUNITY);
                    isImmune = true;
                }
                events.ScheduleEvent(EVENT_SUMMON_FOUR, 60s);
            }
            break;

            case EVENT_CAST_FIREBALL:
            {
                auto randomPlayer = SelectTarget(SelectTargetMethod::Random);
                me->CastSpell(randomPlayer, SPELL_FIREBOLT);
                events.ScheduleEvent(EVENT_CAST_FIREBALL, 7s);
            }
            break;

            case EVENT_CAST_RAIN_OF_FIRE:
            {
                auto randomPlayer = SelectTarget(SelectTargetMethod::Random);
                me->CastSpell(randomPlayer, SPELL_FIRE_RAIN);
                events.ScheduleEvent(EVENT_CAST_RAIN_OF_FIRE, 10s);
            }
            break;

            case EVENT_CAST_FEAR:
            {
                auto randomPlayer = SelectTarget(SelectTargetMethod::Random);
                me->CastSpell(randomPlayer, SPELL_FEAR, TRIGGERED_FULL_MASK);
                events.ScheduleEvent(EVENT_CAST_FEAR, 10s);
            }
            break;

            case EVENT_CAST_RAGE:
            {
                me->CastSpell(me, SPELL_RAGE);

                me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                me->RemoveFlag(UNIT_FIELD_FLAGS, GO_FLAG_NOT_SELECTABLE);
            }
            break;

            case EVENT_CAST_FIRE_BREATH:
            {
                auto randomPlayer = SelectTarget(SelectTargetMethod::Random);
                me->CastSpell(randomPlayer, SPELL_FIRE_BREATH, TRIGGERED_FULL_MASK);
                events.ScheduleEvent(EVENT_CAST_FIRE_BREATH, 15s);
            }
            break;
            }
        }
        DoMeleeAttackIfReady();
    }

protected:
    EventMap events;
    uint8 speechCount;
    bool isImmune;
    std::vector<ObjectGuid> summonedGuids;
    uint8 currentPhase;
    uint32 m_timer;
};

struct fire_elemental_ais : public ScriptedAI
{
public:
    fire_elemental_ais(Creature* creature) : ScriptedAI(creature) { }

    void AttackStart(Unit* target) override
    {
        ScriptedAI::AttackStart(target);
        me->GetMotionMaster()->MoveChase(target);
        me->Attack(target, false);
        events.ScheduleEvent(EVENT_SUMMON_CAST, 1s);
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_SUMMON_CAST:
            {
                auto randomPlayer = SelectTarget(SelectTargetMethod::Random);
                me->CastSpell(randomPlayer, SPELL_FIREBOLT);
                events.ScheduleEvent(SPELL_FIREBOLT, 8s);
            }
            break;
            }
        }
        DoMeleeAttackIfReady();
    }
protected:
    EventMap events;
};

void AddSC_boss_dragonkin()
{
    RegisterCreatureAI(boss_dragonkin);
    RegisterCreatureAI(fire_elemental_ais);
}