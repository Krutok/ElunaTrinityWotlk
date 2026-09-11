#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-source-encoding"
#include "ScriptMgr.h"
#include "ObjectAccessor.h"
#include "ScriptedCreature.h"

enum CreatureIds
{
    GHOUL_ID = 30851,
    VOID_ID  = 16697
};

// Spell ids that the boss will cast
enum Spells
{
    // Generic spells
    SPELL_GHOUL_SELF_STUN = 47466,
    SPELL_VOID_ZONE_SELF_STUN  = 38505,
    SPELL_FROST_AURA = 28531,
    SPELL_BERSERK = 26662,

    // Phase 1 spells
    SPELL_FROSTBOLT = 46987,
    SPELL_CONE_OF_COLD = 38384,
    SPELL_SUMMON_WATER_ELEMENTAL = 45067,
    SPELL_ICEBLOCK = 46604,
    SPELL_FROST_BLAST = 27808,

    // Phase 2 spells
    SPELL_INCREASE_SPELL_DAMAGE = 54192,
    SPELL_ENRAGE = 28131,
    SPELL_CURSE_OF_WEAKNESS = 71204,
    SPELL_VOID_ZONE = 62269,
    SPELL_VOID_BOLT = 71254,
    SPELL_TOD_VERFALL = 71001,
    SPELL_SUMMON_GHOUL = 57913
};

enum Events
{
    EVENT_FROSTBOLT = 1,
    EVENT_CONE_OF_COLD,
    EVENT_SUMMON_WATER_ELEMENTAL,
    EVENT_CURSE_OF_WEAKNESS,
    EVENT_SUMMON_VOID_ZONE,
    EVENT_VOID_BOLT,
    EVENT_SUMMON_GHOUL,
    EVENT_CHECK_RESET,
    EVENT_TOD_VERFALL,
    EVENT_FROST_BLAST,
    EVENT_BERSERK
};

enum Phases
{
    PHASE_1,
    PHASE_2
};

struct boss_myra : public ScriptedAI
{
    boss_myra(Creature* creature) : ScriptedAI(creature), summons(me)
    {
        Initialize();
    }

    // When the boss has spawned in world or reset
    void Initialize()
    {
        me->SetDisplayId(me->GetNativeDisplayId()); // reset our modelId
        currentPhase = PHASE_1;
        WaterElementalGUID.Clear();
        HasCastIceblock = false;
        hasEnraged = false;
    }

    // When the boss resets (leaves combat)
    void Reset() override
    {
        Initialize();
        events.Reset();
        summons.DespawnAll();
    }

    // When you just aggro'd the boss
    void JustEngagedWith(Unit* /*who*/) override
    {
        me->Say("Du willst dich wirklich mit Myra der Legende anlegen? Das werdet ihr bereuen", LANG_UNIVERSAL);
        me->CastSpell(me, SPELL_FROST_AURA);
        events.ScheduleEvent(EVENT_FROSTBOLT, 4s); // 4 seconds after aggroing the boss, the boss will cast frostbolt
        events.ScheduleEvent(EVENT_CONE_OF_COLD, 8s); // 8 seconds after aggroing the boss, the boss will cast cone of cold
        events.ScheduleEvent(EVENT_SUMMON_WATER_ELEMENTAL, 3s); // 3 seconds after aggroing the boss, the boss will summon a water elemental
        events.ScheduleEvent(EVENT_CHECK_RESET, 5s); // 5 seconds after aggroing the boss, the boss will check if he's 50 yards away from his spawn position if so then will reset
        events.ScheduleEvent(EVENT_FROST_BLAST, 15s);
        events.ScheduleEvent(EVENT_BERSERK, 360s, 360s); // Berserker nach 6 Minuten
    }

    // When boss has summoned ads
    void JustSummoned(Creature* summoned) override
    {
        summoned->AI()->AttackStart(SelectTarget(SelectTargetMethod::Random, 0, 50, true)); // ads will search for a random raget in 0 to 50 yards
        summoned->SetFaction(me->GetFaction()); // set faction the same as boss
        WaterElementalGUID = summoned->GetGUID(); // set water elemental guid
        summons.Summon(summoned); // add water elemental to summons
    }

    // When boss has despawned ads
    void SummonedCreatureDespawn(Creature* summoned) override
    {
        summons.Despawn(summoned); // despawn all summons
    }

    // When the boss has just died
    void JustDied(Unit* /*killer*/) override
    {
        me->Say("Das kann nicht sein ich bin doch eine Legende.......Ahhhhhhhhhhhhhhh", LANG_UNIVERSAL);
        summons.DespawnAll();
    }

    // When the boss has taken damage
    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        // If the boss is below 70% hp and has not yet casted ice block
        if (me->HealthBelowPctDamaged(70, damage) && !HasCastIceblock)
        {
            // cast ice block
            me->Say("Hahahahaha.....", LANG_UNIVERSAL);
            DoCast(SPELL_ICEBLOCK);
            HasCastIceblock = true;
        }
        // after taking damage and is at 50% hp enter phase 2
        else if (me->HealthBelowPctDamaged(20, damage) && !hasEnraged)
        {
            // change our modelId
            me->SetDisplayId(27538); // this id is of a skeleton necromancer

            // store that we're in phase 2
            currentPhase = PHASE_2;

            // add buff auras
            me->AddAura(SPELL_INCREASE_SPELL_DAMAGE, me); // add increase spell damage to boss
            me->AddAura(SPELL_ENRAGE, me); // add enrage aura to boss
            hasEnraged = true;

             // increase boss size
            me->SetObjectScale(1.5f);

            // yell
            me->Yell("Jetzt spürst du die ware macht meiner Magie", LANG_UNIVERSAL);

            //Remove Frost Aura
            me->RemoveOwnedAura(SPELL_FROST_AURA);

            // schedule the new phase 2 events
            events.ScheduleEvent(EVENT_CURSE_OF_WEAKNESS, 1s);
            events.ScheduleEvent(EVENT_VOID_BOLT, 2s);
            events.ScheduleEvent(EVENT_TOD_VERFALL, 10s);
            events.ScheduleEvent(EVENT_SUMMON_VOID_ZONE, 4s);
            events.ScheduleEvent(EVENT_SUMMON_GHOUL, 5s);
        }
    }

    // Update bossAI
    void UpdateAI(uint32 diff) override
    {
        // if no victim is found return
        if (!UpdateVictim())
            return;

        events.Update(diff);

        // if already casting don't execute any events
        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            // every 5 seconds check if we or ours summons have gone too far from spawn point
            if (eventId == EVENT_CHECK_RESET)
            {
                // if our current position is greater than 50 yards reset
                if (me->GetDistance2d(me->GetHomePosition().GetPositionX(), me->GetHomePosition().GetPositionY()) > 80)
                    EnterEvadeMode();

                // if our water elemental position is greater than 50 yards the water elemental should reset
                if (Creature* elemental = ObjectAccessor::GetCreature(*me, WaterElementalGUID))
                    if (elemental->GetDistance2d(me->GetHomePosition().GetPositionX(), me->GetHomePosition().GetPositionY()) > 80)
                        elemental->AI()->EnterEvadeMode();

                events.ScheduleEvent(EVENT_CHECK_RESET, 5s); // check again in 5 seconds
            }

            // Phase 1 events
            if (currentPhase == PHASE_1)
            {
                switch (eventId)
                {
                case EVENT_FROSTBOLT:
                    DoCastVictim(SPELL_FROSTBOLT);
                    events.ScheduleEvent(EVENT_FROSTBOLT, 4s, 12s); // after casting frostbolt repeat frostbolt after 4 to 12 seconds
                    break;
                case EVENT_CONE_OF_COLD:
                    DoCastVictim(SPELL_CONE_OF_COLD);
                    events.ScheduleEvent(EVENT_CONE_OF_COLD, 10s, 20s); // after casting cone of cold repeat cone of cold after 10 to 20 seconds
                    break;
                case EVENT_FROST_BLAST:
                    me->Yell("Na ist dir zu warm? Lasse mich dich ein wenig abkühlen...HAHA!", LANG_UNIVERSAL);
                    DoCast(SelectTarget(SelectTargetMethod::Random, 0, 50, true), SPELL_FROST_BLAST);
                    events.ScheduleEvent(EVENT_FROST_BLAST, 20s, 30s);
                    break;    
                case EVENT_SUMMON_WATER_ELEMENTAL:
                    if (summons.empty())
                        DoCast(SPELL_SUMMON_WATER_ELEMENTAL);
                    events.ScheduleEvent(EVENT_SUMMON_WATER_ELEMENTAL, 50s); // after summoing water elemental repeat summon after 50 seconds
                    break;
                case EVENT_BERSERK:
                    DoCastSelf(SPELL_BERSERK);
                    break;    
                }
            }
            // phase 2 events
            else if (currentPhase == PHASE_2)
            {
                switch (eventId)
                {
                case EVENT_CURSE_OF_WEAKNESS:
                    DoCast(SelectTarget(SelectTargetMethod::MaxThreat, 0, 50, true), SPELL_CURSE_OF_WEAKNESS); // cast curse of weakness on the person with most thread (usually the Tank)
                    events.ScheduleEvent(EVENT_CURSE_OF_WEAKNESS, 20s, 25s); // after casting curse of weakness set event to repeat in 6 to 12 seconds
                    break;
                case EVENT_SUMMON_VOID_ZONE:

                    // find previous void zone and despawn it
                    if (Creature* voidZone = me->FindNearestCreature(VOID_ID, 10.f))
                        voidZone->DespawnOrUnsummon();

                    DoCast(SelectTarget(SelectTargetMethod::Random, 0, 50, true), SPELL_VOID_ZONE); // cast void zone on a random player

                    // after casting voidzone stun it so it doesn't chase the player
                    if (Creature* voidZone = me->FindNearestCreature(VOID_ID, 10.f))
                        voidZone->AddAura(SPELL_VOID_ZONE_SELF_STUN, voidZone);

                    events.ScheduleEvent(EVENT_SUMMON_VOID_ZONE, 30s, 40s); // cast again between 4 to 6 seconds
                    break;
                case EVENT_VOID_BOLT:
                    DoCastVictim(SPELL_VOID_BOLT);
                    events.ScheduleEvent(EVENT_VOID_BOLT, 4s, 6s); // cast again between 2 to 3 seconds
                    break;


                case EVENT_TOD_VERFALL:

                    // find previous void zone and despawn it
                    if (Creature* voidZone = me->FindNearestCreature(VOID_ID, 10.f))
                        voidZone->DespawnOrUnsummon();

                    DoCast(SelectTarget(SelectTargetMethod::Random, 0, 50, true), SPELL_TOD_VERFALL); // cast void zone on a random player

                    // after casting voidzone stun it so it doesn't chase the player
                    if (Creature* voidZone = me->FindNearestCreature(VOID_ID, 10.f))
                        voidZone->AddAura(SPELL_VOID_ZONE_SELF_STUN, voidZone);

                    events.ScheduleEvent(EVENT_TOD_VERFALL, 50s, 60s); // cast again between 4 to 6 seconds
                    break;



                case EVENT_SUMMON_GHOUL:
                    me->Yell("Sag hallo zu meinen kleinen Freund hier..HAHA!", LANG_UNIVERSAL);
                    DoCastVictim(SPELL_SUMMON_GHOUL); // cast summon ghoul

                    // after ghoul appears cast stun so he doesn't run to player while playing his animation
                    if (Creature* ghoul = me->FindNearestCreature(GHOUL_ID, 10.f))
                        ghoul->AddAura(SPELL_GHOUL_SELF_STUN, ghoul);

                    events.ScheduleEvent(EVENT_SUMMON_GHOUL, 21s, 30s); // summon ghoul again in 20 to 30 seconds
                    break;
                case EVENT_BERSERK:
                    DoCastSelf(SPELL_BERSERK);
                    break;    
                }
            }

            // don't go any further if we're casting
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;
        }

        // we're not casting so let's melee attack
        DoMeleeAttackIfReady();
    }

private:
    Phases currentPhase;
    EventMap events;
    SummonList summons;
    ObjectGuid WaterElementalGUID;
    bool HasCastIceblock;
    bool hasEnraged;
};

void AddSC_boss_myra()
{
    RegisterCreatureAI(boss_myra);
}