#include "ScriptedCreature.h"

// 100001 Myra my first boss, 100013 DK Boss, 600100 Dragon Boss
enum Creatures
{
    BOSS_MYRA =     100001, //100001,
    BOSS_DK =       100013, //100013,
    BOSS_DRAGON =   600100,

    NPC_GUARDIAN =  16441,
    NPC_PORTAL =    36737
};

enum Texts
{
    SAY_FIRST = 0,
    SAY_SECOND,
    SAY_THIRD,
    SAY_FOURTH,
    SAY_FIFTH,
    SAY_SIXTH,
    SAY_SEVENTH,
};

enum BossEvents
{
    EVENT_ENTER_COMBAT = 1,

    EVENT_INIT_PHASE_ONE,
    EVENT_INIT_PHASE_TWO,
    EVENT_INIT_PHASE_THREE,
    EVENT_INIT_PHASE_FOUR,
    EVENT_INIT_PHASE_FIVE,
    EVENT_INIT_PHASE_SIX,
    EVENT_INIT_PHASE_SEVEN,

    EVENT_CAST_FROSTBOLT,
    EVENT_CAST_FROSTBLAST,
    EVENT_CAST_VOID_FISSURE,

    EVENT_BERSERK_20M,
    EVENT_ENRAGE_7PHASE,

    EVENT_SUMMON_MYRA,
    EVENT_SUMMON_DK,
    EVENT_SUMMON_DRAGON,

    EVENT_SUMMON_GUARDIANS,
};

enum Spells
{
    SPELL_FROSTBOLT = 46987,
    SPELL_FROSTBLAST = 27808,
    SPELL_VOID_FISSURE = 27810,
    SPELL_BERSERK = 26662,
    SPELL_ENRAGE = 28131,

    SPELL_VISUAL_PORTAL = 64446,
    SPELL_VISUAL_SUMMON_PORTAL = 51807
};

enum Phases
{
    PHASE_ALL = 1,
    PHASE_ONE,      // First phase, boss casts spells       - 100% - 80%
    PHASE_TWO,      // Second phase boss summons Myra       - 80% HP
    PHASE_THREE,    // Third phase boss continues casting   - 80% - 60%
    PHASE_FOUR,     // Fourth phase boss summons DK         - 60%
    PHASE_FIVE,     // Fifth phase boss casts spells        - 60% - 40%
    PHASE_SIX,      // Sixth phase boss summons Dragon      - 40%
    PHASE_SEVEN     // Boss continues casting until death   - 40% - 0%
};

struct boss_master_summoner : public ScriptedAI
{
public:
    boss_master_summoner(Creature* creature) : ScriptedAI(creature) {}

    void makeImmune(Creature* me)
    {
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        me->SetFlag(UNIT_FIELD_FLAGS, GO_FLAG_NOT_SELECTABLE);
        me->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC);
        me->AttackStop();
        me->GetMotionMaster()->Clear();
    }

    void removeImmune(Creature* me)
    {
        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
        me->RemoveFlag(UNIT_FIELD_FLAGS, GO_FLAG_NOT_SELECTABLE);
        me->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IMMUNE_TO_PC);
    }

    void Initialize()
    {
        events.Reset();
        speechCount = 0;

        me->RemoveAllAuras();
        for (int i = 0; i < 3; i++)
            spawned[i] = false;
        removeImmune(me);
    }

    void Reset() override
    {
        me->NearTeleportTo(spawnPos);
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

        me->GetMotionMaster()->MoveChase(target);
        events.ScheduleEvent(EVENT_ENTER_COMBAT, 1s);
    }

    void JustDied(Unit* /*killer*/) override
    {
        me->CombatStop(true);
        EngagementOver();
        Reset();
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (me->HealthBelowPctDamaged(80, damage) && events.IsInPhase(PHASE_ONE) && spawned[0] == false)
        {
            Talk(SAY_SECOND);
            spawned[0] = true;
            makeImmune(me);
            events.ScheduleEvent(EVENT_INIT_PHASE_TWO, 1s);
        }
        else if (me->HealthBelowPctDamaged(60, damage) && events.IsInPhase(PHASE_THREE) && spawned[1] == false)
        {
            Talk(SAY_FOURTH);
            spawned[1] = true;
            makeImmune(me);
            events.ScheduleEvent(EVENT_INIT_PHASE_FOUR, 1s);
        }
        else if (me->HealthBelowPctDamaged(40, damage) && events.IsInPhase(PHASE_FIVE) && spawned[2] == false)
        {
            Talk(SAY_SIXTH);
            spawned[2] = true;
            makeImmune(me);
            events.ScheduleEvent(EVENT_INIT_PHASE_SIX, 1s);
        }
    }

    void SummonedCreatureDespawn(Creature* /*summon*/) override
    {
        if (events.IsInPhase(PHASE_TWO)) // Myra died, start phase 3
        {
            removeImmune(me);

            events.ScheduleEvent(EVENT_INIT_PHASE_THREE, 2s);
        }
        if (events.IsInPhase(PHASE_FOUR)) // DK died, start phase 5
        {
            removeImmune(me);

            events.ScheduleEvent(EVENT_INIT_PHASE_FIVE, 2s);
        }
        if (events.IsInPhase(PHASE_SIX)) // Dragon died, start phase 7
        {
            removeImmune(me);

            events.ScheduleEvent(EVENT_INIT_PHASE_SEVEN, 2s);
            me->CastSpell(me, SPELL_ENRAGE);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        events.Update(diff);

        const auto attackTarg = [](Creature* me, Player* target, bool summon = false) -> void
        {
            me->GetMotionMaster()->MoveChase(target);
            me->Attack(target, true);

            if (summon)
                me->GetAI()->AttackStart(target);
        };

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_ENTER_COMBAT:
            {
                Talk(SAY_FIRST);
                events.ScheduleEvent(EVENT_INIT_PHASE_ONE, 1s);
                events.ScheduleEvent(EVENT_BERSERK_20M, 20min);
            }
            break;
            case EVENT_INIT_PHASE_ONE:
            {
                events.SetPhase(PHASE_ONE);
                events.ScheduleEvent(EVENT_CAST_FROSTBOLT, frostBoltTimer);
                events.ScheduleEvent(EVENT_CAST_FROSTBLAST, frostBlastTimer);
                events.ScheduleEvent(EVENT_CAST_VOID_FISSURE, voidFissureTimer);
                attackTarg(me, me->SelectNearestPlayer(50.0f));
            }
            break;
            case EVENT_INIT_PHASE_TWO:
            {
                events.Reset();
                events.SetPhase(PHASE_TWO);

                auto visualPortal = me->GetMap()->SummonCreature(NPC_PORTAL, spawnPos, nullptr, portalDuration);
                visualPortal->SetObjectScale(0.3f);
                visualPortal->CastSpell(visualPortal, SPELL_VISUAL_SUMMON_PORTAL);

                me->CastSpell(me, SPELL_VISUAL_PORTAL);
                me->NearTeleportTo(immunePos);
                events.ScheduleEvent(EVENT_SUMMON_MYRA, 5s);
            }
            break;
            case EVENT_INIT_PHASE_THREE:
            {
                events.Reset();
                events.RemovePhase(PHASE_TWO);
                events.SetPhase(PHASE_THREE);

                me->CastSpell(me, SPELL_VISUAL_PORTAL);
                me->NearTeleportTo(spawnPos);

                events.ScheduleEvent(EVENT_CAST_FROSTBOLT, frostBoltTimer);
                events.ScheduleEvent(EVENT_CAST_FROSTBLAST, frostBlastTimer);
                events.ScheduleEvent(EVENT_CAST_VOID_FISSURE, voidFissureTimer);
                attackTarg(me, me->SelectNearestPlayer(50.0f));

                Talk(SAY_THIRD);

            }
            break;
            case EVENT_INIT_PHASE_FOUR:
            {
                events.Reset();
                events.RemovePhase(PHASE_THREE);
                events.SetPhase(PHASE_FOUR);

                auto visualPortal = me->GetMap()->SummonCreature(NPC_PORTAL, spawnPos, nullptr, portalDuration);
                visualPortal->SetObjectScale(0.3f);
                visualPortal->CastSpell(visualPortal, SPELL_VISUAL_SUMMON_PORTAL);

                me->CastSpell(me, SPELL_VISUAL_PORTAL);
                me->NearTeleportTo(immunePos);

                events.ScheduleEvent(EVENT_SUMMON_DK, 5s);
            }
            break;
            case EVENT_INIT_PHASE_FIVE:
            {
                events.Reset();
                events.RemovePhase(PHASE_FOUR);
                events.SetPhase(PHASE_FIVE);

                me->CastSpell(me, SPELL_VISUAL_PORTAL);
                me->NearTeleportTo(spawnPos);

                events.ScheduleEvent(EVENT_CAST_FROSTBOLT, frostBoltTimer);
                events.ScheduleEvent(EVENT_CAST_FROSTBLAST, frostBlastTimer);
                events.ScheduleEvent(EVENT_CAST_VOID_FISSURE, voidFissureTimer);
                attackTarg(me, me->SelectNearestPlayer(50.0f));

                Talk(SAY_FIFTH);

            }
            break;
            case EVENT_INIT_PHASE_SIX:
            {
                events.Reset();
                events.RemovePhase(PHASE_FIVE);
                events.SetPhase(PHASE_SIX);

                auto visualPortal = me->GetMap()->SummonCreature(NPC_PORTAL, spawnPos, nullptr, portalDuration);
                visualPortal->SetObjectScale(0.3f);
                visualPortal->CastSpell(visualPortal, SPELL_VISUAL_SUMMON_PORTAL);

                me->CastSpell(me, SPELL_VISUAL_PORTAL);
                me->NearTeleportTo(immunePos);

                events.ScheduleEvent(EVENT_SUMMON_DRAGON, 5s);
            }
            break;
            case EVENT_INIT_PHASE_SEVEN:
            {
                events.Reset();
                events.RemovePhase(PHASE_SIX);
                events.SetPhase(PHASE_SEVEN);

                me->CastSpell(me, SPELL_VISUAL_PORTAL);
                me->NearTeleportTo(spawnPos);

                events.ScheduleEvent(EVENT_CAST_FROSTBOLT, frostBoltTimer);
                events.ScheduleEvent(EVENT_CAST_FROSTBLAST, frostBlastTimer);
                events.ScheduleEvent(EVENT_CAST_VOID_FISSURE, voidFissureTimer);

                {auto visualPortal = me->GetMap()->SummonCreature(NPC_PORTAL, bugLeft, nullptr, portalDuration);
                visualPortal->SetObjectScale(0.2f);
                visualPortal->CastSpell(visualPortal, SPELL_VISUAL_SUMMON_PORTAL); }

                {auto visualPortal = me->GetMap()->SummonCreature(NPC_PORTAL, bugRight, nullptr, portalDuration);
                visualPortal->SetObjectScale(0.2f);
                visualPortal->CastSpell(visualPortal, SPELL_VISUAL_SUMMON_PORTAL); }

                events.ScheduleEvent(EVENT_SUMMON_GUARDIANS, 5s);
                attackTarg(me, me->SelectNearestPlayer(50.0f));

                Talk(SAY_SEVENTH);

            }
            break;
            case EVENT_CAST_FROSTBOLT:
            {
                auto randomPlayer = SelectTarget(SelectTargetMethod::Random);
                me->CastSpell(randomPlayer, SPELL_FROSTBOLT, TRIGGERED_FULL_MASK);
                events.ScheduleEvent(EVENT_CAST_FROSTBOLT, frostBoltTimer);
            }
            break;
            case EVENT_CAST_FROSTBLAST:
            {
                auto randomPlayer = SelectTarget(SelectTargetMethod::Random);
                me->CastSpell(randomPlayer, SPELL_FROSTBLAST);
                events.ScheduleEvent(EVENT_CAST_FROSTBLAST, frostBlastTimer);
            }
            break;
            case EVENT_CAST_VOID_FISSURE:
            {
                auto randomPlayer = SelectTarget(SelectTargetMethod::Random);
                me->CastSpell(randomPlayer, SPELL_VOID_FISSURE, TRIGGERED_FULL_MASK);
                events.ScheduleEvent(EVENT_CAST_VOID_FISSURE, voidFissureTimer);
            }
            break;
            case EVENT_SUMMON_MYRA:
            {
                auto summ = me->SummonCreature(BOSS_MYRA, spawnPos, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, despawnOutOfCombat);
                attackTarg(summ, summ->SelectNearestPlayer(100.0f), true);
            }
            break;
            case EVENT_SUMMON_DK:
            {
                auto summ = me->SummonCreature(BOSS_DK, spawnPos, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, despawnOutOfCombat);
                attackTarg(summ, summ->SelectNearestPlayer(100.0f), true);
            }
            break;
            case EVENT_SUMMON_DRAGON:
            {
                auto summ = me->SummonCreature(BOSS_DRAGON, spawnPos, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, despawnOutOfCombat);
                attackTarg(summ, summ->SelectNearestPlayer(100.0f), true);
            }
            break;
            case EVENT_SUMMON_GUARDIANS:
            {
                auto left = me->SummonCreature(NPC_GUARDIAN, bugLeft, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, despawnOutOfCombat);
                auto right = me->SummonCreature(NPC_GUARDIAN, bugRight, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, despawnOutOfCombat);

                attackTarg(left, left->SelectNearestPlayer(100.0f), true);
                attackTarg(right, right->SelectNearestPlayer(100.0f), true);
            }
            break;

            case EVENT_BERSERK_20M:
            {
                me->CastSpell(me, SPELL_BERSERK);
            }
            break;

            }
        }
        DoMeleeAttackIfReady();
    }

protected:
    EventMap events;
    uint8 speechCount;
    uint8 currentPhase;
    uint32 m_timer;
    std::array<bool, 3> spawned;

    const Position spawnPos     { 5763.822266f, -2957.943359f, 273.913300f, 4.792159f };    // Where the 3 bosses are summoned
    const Position immunePos    { 5757.332031f, -2945.115723f, 286.276917f, 5.228056f}; // Where master teleports and becomes immune
    const Position bugLeft      { 5741.269531f, -2966.229980f, 273.334351f, 6.080219f};  // Left Defender in 7th phase
    const Position bugRight     { 5783.863770f, -2945.387451f, 274.559540f, 4.360205f};  // Right defender in 7th phase

    const Milliseconds frostBoltTimer   = 6s;       // Time between frost bolt casts
    const Milliseconds frostBlastTimer  = 10s;      // Time between frost blast casts
    const Milliseconds voidFissureTimer = 15s;      // Time between void fissure cast

    uint32 portalDuration = 8000; // 8 seconds
    Milliseconds despawnOutOfCombat = Milliseconds(10000); // 2.5 seconds

};

void AddSC_boss_master_summoner()
{
    RegisterCreatureAI(boss_master_summoner);
}
