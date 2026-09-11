#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-source-encoding"

#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "ScriptedCreature.h"
#include "Log.h"

namespace
{
    // DK
    enum eDKSpells
    {
        SPELL_DEATH_GRIP = 49576,
        SPELL_BLOOD_PRESENCE = 48266,
        SPELL_DESECRATION = 55667,

        SPELL_SUMMON_GHOUL = 70617,
        SPELL_HOW = 57623,
        SPELL_LK_GRASP = 60231,
        SPELL_SCOURGE_BANNER = 60023,
        SPELL_ICY_TOUCH = 49909,
        SPELL_MIND_FREEZE = 47528,
        SPELL_PLAGUE_STRIKE = 49921,
        SPELL_AMS = 48707,
        SPELL_DEATH_COIL = 49895,
        SPELL_GNAW = 47481,
        SPELL_ARMYOFDEAD = 42650,
        SPELL_DEATH_AND_DECAY = 49938,
        SPELL_DEATH_CHARGER_MOUNT = 73313,
        SPELL_BERSERK = 26662
    };
    enum eDKEvents
    {
        EVENT_CAST_LK_GRASP = 1,
        EVENT_CAST_SCOURGE_BANNER,
        EVENT_CAST_ICY_TOUCH,
        EVENT_CAST_MIND_FREEZE,
        EVENT_CAST_DEATH_GRIP,
        EVENT_CAST_PLAGUE_STRIKE,
        EVENT_CAST_AMS,
        EVENT_CAST_DEATH_COIL,
        EVENT_CAST_GNAW,
        EVENT_CAST_ARMY_OF_DEAD,
        EVENT_CAST_DEATH_N_DECAY,
        EVENT_CAST_GHOUL,

        EVENT_TALK_1,
        EVENT_TALK_2,
        EVENT_TALK_3,

        EVENT_INITAL_RP_EVENT,
        EVENT_BERSERK,

        // if you're going to add more events make sure they're above the EVENT_LAST
        EVENT_LAST
    };

    // Lich King
    enum eLKSpells
    {
        SPELL_DEFILE = 72762,
        SPELL_DEFILE_AURA = 72743,
        SPELL_DEFILE_GROW = 72756,
        SPELL_SUMMON_SHAMBLING_HORROR = 70372,
        SPELL_RISEN_WITCH_DOCTOR_SPAWN = 69639,
        SPELL_SUMMON_DRUDGE_GHOULS = 70358,
        SPELL_INFEST = 70541,
        SPELL_NECROTIC_PLAGUE = 70337,
    };
    enum eLKEvents
    {
        EVENT_LK_TALK_1 = EVENT_LAST + 1,
        EVENT_LK_TALK_2,
        EVENT_LK_TALK_3,
        EVENT_LK_DEFILE,
        EVENT_LK_SHAMBLING,
        EVENT_LK_WITCH_DOCTOR,
        EVENT_LK_DRUDGE,
        EVENT_LK_INFEST,
        EVENT_LK_NECROTIC,
    };

    enum Phases
    {
        PHASE_1 = 1,
        PHASE_2 = 2,
        PHASE_3 = 3
    };

    const uint32 LK_MODEL = 24191;
    const uint32 NPC_DEFILE = 38757;
    const uint32 SELF_STUN = 47466;
    const uint32 GHOUL_ENTRY = 37881;
}

class StartCombat : public BasicEvent
{
public:
    StartCombat(Unit* cre, uint32 m_OrigFaction, Unit* target, const std::string& text = "") : BasicEvent(),
        creature(cre), faction(m_OrigFaction), m_Target(target), m_text(text) { }

    bool Execute(uint64 /*time_t*/, uint32 /*time_e*/) override
    {
        if (!creature)
            return true;

        if (m_text.empty())
        {
            creature->RemoveAura(SPELL_DEATH_CHARGER_MOUNT);
            creature->SetFaction(faction);
            if (m_Target)
            creature->GetAI()->AttackStart(m_Target);
        }
        else
        {
            creature->Say(m_text.c_str(), LANG_UNIVERSAL);
        }

        return true;
    }

protected:
    Unit* creature;
    uint32 faction;
    Unit* m_Target;
    std::string m_text;
};

class npc_dk_boss : public CreatureScript
{
public:
    npc_dk_boss() : CreatureScript("npc_dk_boss") { }

    struct npc_dk_bossAI : public ScriptedAI
    {
        npc_dk_bossAI(Creature* creature) : ScriptedAI(creature)
        {
            Initialize();
        }

        // When the boss has spawned in world or reset
        void Initialize()
        {
            if (!me->HasAura(SPELL_DEATH_CHARGER_MOUNT))
                me->AddAura(SPELL_DEATH_CHARGER_MOUNT, me);

            if (!m_InitalRP)
            {
                m_Target = nullptr;

                me->RestoreDisplayId();

                hasCastedArmy = false;

                DoCastSelf(SPELL_DESECRATION);

                m_Phase = PHASE_1;

                DespawnPet();

                m_Events.Reset();
            }
        }

void DespawnPet()
{
    // despawn any previous summoned pets
    if (auto pet = me->GetGuardianPet())
        pet->DisappearAndDie();

    if (Creature* cre = me->FindNearestCreature(GHOUL_ENTRY, 50.f))
        cre->DisappearAndDie();
    while (Creature* cre = me->FindNearestCreature(24207, 50.f))
        cre->DisappearAndDie();
    while (Creature* cre = me->FindNearestCreature(37698, 50.f))
        cre->DisappearAndDie();
    while (Creature* cre = me->FindNearestCreature(37695, 50.f))
        cre->DisappearAndDie();
    while (Creature* cre = me->FindNearestCreature(38757, 50.f)) // defile despawn after boss dead or reset?
        cre->DisappearAndDie();
}

        // When the boss resets (leaves combat)
        void Reset() override
        {
            m_Target = nullptr;
            Initialize();
        }

        void JustSummoned(Creature* summoned) override
        {
            switch (summoned->GetEntry())
            {
            case NPC_DEFILE:
                summoned->SetReactState(REACT_PASSIVE);
                summoned->CastSpell(summoned, SPELL_DEFILE_AURA, false);
                break;
            default:
                summoned->CastSpell(summoned, SELF_STUN);
                if (me->GetVictim())
                    summoned->AI()->AttackStart(me->GetVictim());
                break;
            }

            summoned->SetFaction(me->GetFaction());
        }

        // When the boss has just died
        void JustDied(Unit* /*killer*/) override
        {
            me->Say("Unmöglich....ich wurde von einfachen sterblichen besiegt.", LANG_UNIVERSAL);
        }

        // When you just aggro'd the boss
        void JustEngagedWith(Unit* who) override
        {
            if (!m_InitalRP)
            {
                m_Target = who;
                m_InitalRP = true;
                m_Events.ScheduleEvent(EVENT_INITAL_RP_EVENT, 0s);
                return;
            }

            m_InitalRP = false;

            DoCastSelf(SPELL_BLOOD_PRESENCE);
            DoCastSelf(SPELL_HOW);

            m_Events.ScheduleEvent(EVENT_CAST_GHOUL, 1s, 1s);
            m_Events.ScheduleEvent(EVENT_CAST_MIND_FREEZE, 2s, 2s);
            m_Events.ScheduleEvent(EVENT_CAST_DEATH_GRIP, 1s, 1s);
            m_Events.ScheduleEvent(EVENT_CAST_AMS, 10s, 10s);
            m_Events.ScheduleEvent(EVENT_CAST_DEATH_N_DECAY, 3s, 3s);
            m_Events.ScheduleEvent(EVENT_CAST_GNAW, 7s, 7s);
            m_Events.ScheduleEvent(EVENT_CAST_ICY_TOUCH, 3s, 4s);
            m_Events.ScheduleEvent(EVENT_CAST_PLAGUE_STRIKE, 5s, 6s);
            m_Events.ScheduleEvent(EVENT_CAST_DEATH_COIL, 6s, 6s);
            m_Events.ScheduleEvent(EVENT_BERSERK, 360s, 360s);
        }

        // On damage taken
        void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
        {
            if (me->HealthBelowPctDamaged(66, damage) && m_Phase == PHASE_1)
            {
                m_Phase = PHASE_2;
                m_Events.ScheduleEvent(EVENT_TALK_1, 1s, 1s);
                m_Events.ScheduleEvent(EVENT_CAST_LK_GRASP, 1s, 1s);
                m_Events.ScheduleEvent(EVENT_CAST_SCOURGE_BANNER, 1s, 1s);

            }
            else if (me->HealthBelowPctDamaged(33, damage) && m_Phase == PHASE_2)
            {
                DespawnPet();

                me->RemoveAura(SPELL_BLOOD_PRESENCE);
                me->RemoveAura(SPELL_HOW);
                me->RemoveAura(SPELL_LK_GRASP);
                me->RemoveAura(SPELL_SCOURGE_BANNER);

                m_Phase = PHASE_3;
                DoCastSelf(4335);
                me->SetDisplayId(LK_MODEL);
                m_Events.ScheduleEvent(EVENT_LK_TALK_1, 1s, 1s);

                m_Events.ScheduleEvent(EVENT_LK_DEFILE, 1s, 1s);
                m_Events.ScheduleEvent(EVENT_LK_SHAMBLING, 2s, 2s);
                m_Events.ScheduleEvent(EVENT_LK_DRUDGE, 15s, 15s);
                m_Events.ScheduleEvent(EVENT_LK_INFEST, 5s, 5s);
                m_Events.ScheduleEvent(EVENT_LK_NECROTIC, 10s, 10s);
            }
        }

        // Update bossAI
        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim() && !m_InitalRP)
            {
                Reset();
                return;
            }

            m_Events.Update(diff);

            // if already casting don't execute any events
            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            const auto allowedEvents = [&](uint32 eventId)
            {
                std::unordered_map<Phases, std::vector<uint32>> phases =
                {
                    {PHASE_1, {EVENT_INITAL_RP_EVENT, EVENT_CAST_ICY_TOUCH, EVENT_CAST_MIND_FREEZE, EVENT_CAST_DEATH_GRIP, EVENT_CAST_PLAGUE_STRIKE,
                               EVENT_CAST_AMS, EVENT_CAST_DEATH_COIL, EVENT_CAST_GNAW, EVENT_CAST_ARMY_OF_DEAD, EVENT_CAST_DEATH_N_DECAY, EVENT_CAST_GHOUL, EVENT_BERSERK}},

                    {PHASE_2, {EVENT_CAST_LK_GRASP, EVENT_CAST_SCOURGE_BANNER, EVENT_CAST_ICY_TOUCH, EVENT_CAST_MIND_FREEZE, EVENT_CAST_DEATH_GRIP,
                               EVENT_CAST_AMS, EVENT_CAST_DEATH_COIL, EVENT_CAST_ARMY_OF_DEAD, EVENT_CAST_GNAW, EVENT_CAST_DEATH_N_DECAY,
                               EVENT_TALK_1, EVENT_TALK_2, EVENT_TALK_3, EVENT_CAST_GHOUL, EVENT_CAST_PLAGUE_STRIKE, EVENT_BERSERK}},

                    {PHASE_3, {EVENT_LK_TALK_1, EVENT_LK_TALK_2, EVENT_LK_TALK_3, EVENT_LK_DEFILE, EVENT_LK_SHAMBLING, EVENT_LK_WITCH_DOCTOR,
                               EVENT_LK_DRUDGE, EVENT_LK_INFEST, EVENT_LK_NECROTIC, EVENT_BERSERK}}
                };

                auto it = std::find(std::begin(phases[m_Phase]), std::end(phases[m_Phase]), eventId);
                if (it == std::end(phases[m_Phase]))
                    return false;

                return true;
            };

            const auto isCaster = [](Unit* vic)
            {
                if (vic->ToPlayer())
                {
                    switch (vic->GetClass())
                    {
                    case CLASS_PRIEST:
                    case CLASS_WARLOCK:
                    case CLASS_DRUID:
                    case CLASS_MAGE:
                    case CLASS_SHAMAN:
                        return true;
                    }
                }
                return false;
            };

            while (uint32 eventId = m_Events.ExecuteEvent())
            {
                if (!allowedEvents(eventId))
                    continue;

                // a little hack
                me->SetPower(POWER_RUNIC_POWER, 1000);

                switch (eventId)
                {
                case EVENT_INITAL_RP_EVENT:
                    me->SetFaction(FACTION_FRIENDLY); // make boss friendly for awhile
                    me->Say("Stop Stop wo bin ich hier?", LANG_UNIVERSAL);
                    me->m_Events.AddEvent(new StartCombat(me, m_OrigFaction, m_Target, "Gerade war ich noch im Strom der Zeit unterwegs"), me->m_Events.CalculateTime(3s));
                    me->m_Events.AddEvent(new StartCombat(me, m_OrigFaction, m_Target, "Und auf einmal spürte ich ein stechenden Schmerz und wurde durch dieses Portal gerissen."), me->m_Events.CalculateTime(6s));
                    me->m_Events.AddEvent(new StartCombat(me, m_OrigFaction, m_Target), me->m_Events.CalculateTime(9s));
                    break;
                case EVENT_CAST_GHOUL:
                    DoCastSelf(SPELL_SUMMON_GHOUL);
                    break;
                case EVENT_CAST_LK_GRASP:
                    DoCastSelf(SPELL_LK_GRASP);
                    break;
                case EVENT_CAST_SCOURGE_BANNER:
                    DoCastSelf(SPELL_SCOURGE_BANNER);
                    break;
                case EVENT_CAST_ICY_TOUCH:
                    DoCastVictim(SPELL_ICY_TOUCH);
                    m_Events.ScheduleEvent(EVENT_CAST_ICY_TOUCH, 4s, 5s);
                    break;
                case EVENT_CAST_PLAGUE_STRIKE:
                    DoCastVictim(SPELL_PLAGUE_STRIKE);
                    m_Events.ScheduleEvent(EVENT_CAST_PLAGUE_STRIKE, 6s, 7s);
                    break;
                case EVENT_CAST_MIND_FREEZE:
                    if (Unit* victim = me->GetVictim())
                    {
                        if (victim->HasUnitState(UNIT_STATE_CASTING))
                        {
                            if (DoCastVictim(SPELL_MIND_FREEZE) == SPELL_FAILED_SUCCESS)
                                m_Events.ScheduleEvent(EVENT_CAST_MIND_FREEZE, 10s, 11s);
                        }
                    }
                    m_Events.ScheduleEvent(EVENT_CAST_MIND_FREEZE, 1s, 1s);
                    break;
                case EVENT_CAST_DEATH_GRIP:
                    if (Unit* victim = me->GetVictim())
                    {
                        if (me->GetDistance(victim) >= 7.f)
                        {
                            DoCastVictim(SPELL_DEATH_GRIP);
                            m_Events.ScheduleEvent(EVENT_CAST_DEATH_GRIP, 35s, 35s);
                        }
                    }
                    m_Events.ScheduleEvent(EVENT_CAST_DEATH_GRIP, 3s, 3s);
                    break;
                case EVENT_CAST_AMS:
                    if (Unit* victim = me->GetVictim())
                    {
                        if (isCaster(victim))
                        {
                            if (victim->HasUnitState(UNIT_STATE_CASTING))
                            {
                                DoCastSelf(SPELL_AMS);
                                m_Events.ScheduleEvent(EVENT_CAST_AMS, 45s, 45s);
                            }
                        }
                    }
                    m_Events.ScheduleEvent(EVENT_CAST_AMS, 1s, 1s);
                    break;
                case EVENT_CAST_DEATH_COIL:
                    DoCastVictim(SPELL_DEATH_COIL);
                    m_Events.ScheduleEvent(EVENT_CAST_DEATH_COIL, 4s, 4s);
                    break;
                case EVENT_CAST_GNAW:
                    if (Unit* vic = me->GetVictim())
                        vic->AddAura(SPELL_GNAW, vic);
                    m_Events.ScheduleEvent(EVENT_CAST_GNAW, 60s, 60s);
                    if (!hasCastedArmy)
                        m_Events.ScheduleEvent(EVENT_CAST_ARMY_OF_DEAD, 1s, 1s);
                    break;
                case EVENT_CAST_ARMY_OF_DEAD:
                    DoCastSelf(SPELL_ARMYOFDEAD);
                    hasCastedArmy = true;
                    m_Events.ScheduleEvent(EVENT_CAST_ARMY_OF_DEAD, 360s, 360s);
                    break;
                case EVENT_CAST_DEATH_N_DECAY:
                    DoCastVictim(SPELL_DEATH_AND_DECAY);
                    m_Events.ScheduleEvent(EVENT_CAST_DEATH_N_DECAY, 30s, 30s);
                    break;
                case EVENT_TALK_1:
                    me->Say("Ich spüre eine seltsame Präsenz als würde mich jemand rufen", LANG_UNIVERSAL);
                    m_Events.ScheduleEvent(EVENT_TALK_2, 3s, 3s);
                    break;
                case EVENT_TALK_2:
                    me->Say("Ich muss dem wiederstehen...Arrrrrgghhhhhh", LANG_UNIVERSAL);
                    m_Events.ScheduleEvent(EVENT_TALK_3, 3s, 3s);
                    break;
                case EVENT_TALK_3:
                    me->Say("Es tut so weh...es soll aufhören bitte", LANG_UNIVERSAL);
                    break;
                // Lich King
                case EVENT_LK_TALK_1:
                    me->Yell("Hahaha...ich wusste es eines Tages kommt jemand mächtiges und kann meine Macht verstärken", LANG_UNIVERSAL);
                    m_Events.ScheduleEvent(EVENT_LK_TALK_2, 3s, 3s);
                    break;
                case EVENT_LK_TALK_2:
                    me->Yell("Die Zeit ist gekommen jetzt spürt ihr die Macht des Lichkönigs", LANG_UNIVERSAL);
                    m_Events.ScheduleEvent(EVENT_LK_TALK_3, 3s, 3s);
                    break;
                case EVENT_LK_TALK_3:
                    me->Yell("Wenn ich mit euch fertig bin, dann wird ganz Azeroth mir als Untote Geißel dienen oder untergehen.....Hahahahaha", LANG_UNIVERSAL);
                    break;
                case EVENT_LK_DEFILE:
                    DoCastVictim(SPELL_DEFILE);
                    m_Events.ScheduleEvent(EVENT_LK_DEFILE, 25s, 25s);
                    break;
                case EVENT_LK_SHAMBLING:
                    DoCastVictim(SPELL_SUMMON_SHAMBLING_HORROR);
                    m_Events.ScheduleEvent(EVENT_LK_SHAMBLING, 30s, 30s);
                    break;
                case EVENT_LK_WITCH_DOCTOR:
                    break;
                case EVENT_LK_DRUDGE:
                    DoCastVictim(SPELL_SUMMON_DRUDGE_GHOULS);
                    m_Events.ScheduleEvent(EVENT_LK_DRUDGE, 45s, 45s);
                    break;
                case EVENT_LK_INFEST:
                    DoCastVictim(SPELL_INFEST);
                    m_Events.ScheduleEvent(EVENT_LK_INFEST, 15s, 15s);
                    break;
                case EVENT_LK_NECROTIC:
                    DoCastVictim(SPELL_NECROTIC_PLAGUE);
                    m_Events.ScheduleEvent(EVENT_LK_NECROTIC, 20s, 20s);
                    break;
                case EVENT_BERSERK:
                    //DoCast(me, SPELL_BERSERK);
                    DoCastSelf(SPELL_BERSERK);
                    break;    
                }
            }

            // if we're not casting let's melee attack
            if (!me->HasUnitState(UNIT_STATE_CASTING))
                DoMeleeAttackIfReady();
        }

    private:
        Unit* m_Target{nullptr};
        uint32 m_OrigFaction { 974 };
        bool hasCastedArmy, m_InitalRP{ false };
        Phases m_Phase;
        EventMap m_Events;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_dk_bossAI(creature);
    }
};

void AddSC_DeathKnight_Boss()
{
    new npc_dk_boss();
}