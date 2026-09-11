#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "CombatAI.h"
#include "GameObject.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptedEscortAI.h"
#include "ScriptedGossip.h"
#include "SpellAuraEffects.h"
#include "SpellHistory.h"
#include "SpellScript.h"
#include "TemporarySummon.h"
#include "Vehicle.h"
#include "WorldSession.h"

/*#####
# Quest 13003 Thrusting Hodir's Spear
#####*/

enum WildWyrmCustom
{
    // Phase 1
    SPELL_PLAYER_MOUNT_WYRM_CUSTOM = 81529, // SPELL_PLAYER_MOUNT_WYRM             = 56672,
    SPELL_FIGHT_WYRM_CUSTOM = 81530, // SPELL_FIGHT_WYRM                    = 56673,
    SPELL_SPEAR_OF_WYRM_CUSTOM = 81528, // SPELL_SPEAR_OF_HODIR                = 56671,
    SPELL_GRIP = 56689,
    SPELL_GRAB_ON = 60533,
    SPELL_DODGE_CLAWS = 56704,
    SPELL_THRUST_SPEAR = 56690,
    SPELL_MIGHTY_SPEAR_THRUST = 60586,
    SPELL_CLAW_SWIPE_PERIODIC = 60689,
    SPELL_CLAW_SWIPE_DAMAGE = 60776,
    SPELL_FULL_HEAL_MANA = 32432,
    SPELL_LOW_HEALTH_TRIGGER = 60596,

    // Phase 2
    SPELL_EJECT_PASSENGER_1 = 60603,
    SPELL_PRY_JAWS_OPEN = 56706,
    SPELL_FATAL_STRIKE = 60587,
    SPELL_FATAL_STRIKE_DAMAGE = 60881,
    SPELL_JAWS_OF_DEATH_PERIODIC = 56692,
    SPELL_FLY_STATE_VISUAL = 60865,

    // Dead phase
    SPELL_WYRM_KILL_CREDIT_1 = 81526,
    SPELL_WYRM_KILL_CREDIT_2 = 81527,
    SPELL_FALLING_DRAGON_FEIGN_DEATH = 55795,
    SPELL_EJECT_ALL_PASSENGERS = 50630,

    SAY_SWIPE = 0,
    SAY_DODGED = 1,
    SAY_PHASE_2 = 2,
    SAY_GRIP_WARN = 3,
    SAY_STRIKE_MISS = 4,

    ACTION_CLAW_SWIPE_WARN = 1,
    ACTION_CLAW_SWIPE_DODGE = 2,
    ACTION_GRIP_FAILING = 3,
    ACTION_GRIP_LOST = 4,
    ACTION_FATAL_STRIKE_MISS = 5,

    POINT_START_FIGHT = 1,
    POINT_FALL = 2,
    POINT_RETURN_TO_SPAWN = 3,

    SEAT_INITIAL = 0,
    SEAT_MOUTH = 1,

    PHASE_INITIAL = 0,
    PHASE_MOUTH = 1,
    PHASE_DEAD = 2,
    PHASE_MAX = 3
};

uint8 const ControllableSpellsCount = 4;
uint32 const WyrmControlSpells[PHASE_MAX][ControllableSpellsCount] =
{
    { SPELL_GRAB_ON,       SPELL_DODGE_CLAWS, SPELL_THRUST_SPEAR, SPELL_MIGHTY_SPEAR_THRUST },
    { SPELL_PRY_JAWS_OPEN, 0,                 SPELL_FATAL_STRIKE, 0                         },
    { 0,                   0,                 0,                  0                         }
};

struct npc_wilder_protodrache : public VehicleAI
{
    npc_wilder_protodrache(Creature* creature) : VehicleAI(creature)
    {
        _spawnPosition = me->GetHomePosition();
        Initialize();
    }

    void Initialize()
    {
        _phase = PHASE_INITIAL;
        _playerCheckTimer = 1 * IN_MILLISECONDS;
    }

    void InitSpellsForPhase()
    {
        ASSERT(_phase < PHASE_MAX);
        for (uint8 i = 0; i < ControllableSpellsCount; ++i)
            me->m_spells[i] = WyrmControlSpells[_phase][i];
    }

    void Reset() override
    {
        Initialize();

        _playerGuid.Clear();
        _scheduler.CancelAll();

        InitSpellsForPhase();

        me->SetImmuneToPC(false);
    }

    void DoAction(int32 action) override
    {
        Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
        if (!player)
            return;

        switch (action)
        {
        case ACTION_CLAW_SWIPE_WARN:
            Talk(SAY_SWIPE, player);
            break;
        case ACTION_CLAW_SWIPE_DODGE:
            Talk(SAY_DODGED, player);
            break;
        case ACTION_GRIP_FAILING:
            Talk(SAY_GRIP_WARN, player);
            break;
        case ACTION_GRIP_LOST:
            DoCastAOE(SPELL_EJECT_PASSENGER_1, true);
            EnterEvadeMode();
            break;
        case ACTION_FATAL_STRIKE_MISS:
            Talk(SAY_STRIKE_MISS, player);
            break;
        default:
            break;
        }
    }

    void SpellHit(WorldObject* caster, SpellInfo const* spellInfo) override
    {
        if (!_playerGuid.IsEmpty() || spellInfo->Id != SPELL_SPEAR_OF_WYRM_CUSTOM)
            return;

        _playerGuid = caster->GetGUID();
        DoCastAOE(SPELL_FULL_HEAL_MANA, true);
        me->SetImmuneToPC(true);

        me->GetMotionMaster()->MovePoint(POINT_START_FIGHT, *caster);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE && type != EFFECT_MOTION_TYPE)
            return;

        switch (id)
        {
        case POINT_START_FIGHT:
        {
            Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
            if (!player)
                return;

            DoCast(player, SPELL_PLAYER_MOUNT_WYRM_CUSTOM);
            me->GetMotionMaster()->Clear();
            break;
        }
        case POINT_FALL:
            DoCastAOE(SPELL_EJECT_ALL_PASSENGERS);
            me->KillSelf();
            break;
        case POINT_RETURN_TO_SPAWN:
            me->GetMotionMaster()->MoveRandom(70.0f);
            break;
        default:
            break;
        }
    }

    void DamageTaken(Unit* /*attacker*/, uint32& damage, DamageEffectType /*damageType*/, SpellInfo const* /*spellInfo = nullptr*/) override
    {
        if (damage >= me->GetHealth())
        {
            damage = me->GetHealth() - 1;

            if (_phase == PHASE_DEAD)
                return;

            _phase = PHASE_DEAD;
            _scheduler.CancelAll()
                .Async([this]
                    {
                        InitSpellsForPhase();

                        if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid))
                        {
                            player->SetFullHealth();
                            player->VehicleSpellInitialize();
                        }

                        DoCastAOE(me->GetEntry() == 101178 ? SPELL_WYRM_KILL_CREDIT_1 : SPELL_WYRM_KILL_CREDIT_2);
                        DoCastAOE(SPELL_FALLING_DRAGON_FEIGN_DEATH);

                        me->RemoveAurasDueToSpell(SPELL_JAWS_OF_DEATH_PERIODIC);
                        me->RemoveAurasDueToSpell(SPELL_PRY_JAWS_OPEN);

                        me->ReplaceAllNpcFlags(UNIT_NPC_FLAG_NONE);

                        me->GetMotionMaster()->MoveFall(POINT_FALL);
                    });
        }
    }

    void PassengerBoarded(Unit* passenger, int8 seatId, bool apply) override
    {
        if (!apply)
        {
            if (passenger->GetGUID() != _playerGuid)
                return;

            if (_phase == PHASE_MOUTH)
                return;

            _manualExit = true;
            return;
        }

        if (passenger->GetGUID() != _playerGuid)
            return;

        if (seatId != SEAT_INITIAL)
            return;

        me->GetMotionMaster()->MovePoint(POINT_RETURN_TO_SPAWN, _spawnPosition);

        me->CastSpell(nullptr, SPELL_GRIP, CastSpellExtraArgs().AddSpellMod(SPELLVALUE_AURA_STACK, 50));
        DoCastAOE(SPELL_CLAW_SWIPE_PERIODIC);

        _scheduler.Schedule(500ms, [this](TaskContext context)
            {
                if (_phase == PHASE_MOUTH)
                    return;

                if (me->HealthBelowPct(25))
                {
                    _phase = PHASE_MOUTH;
                    context.Async([this]
                        {
                            InitSpellsForPhase();
                            DoCastAOE(SPELL_LOW_HEALTH_TRIGGER, true);
                            me->RemoveAurasDueToSpell(SPELL_CLAW_SWIPE_PERIODIC);
                            me->RemoveAurasDueToSpell(SPELL_GRIP);

                            if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid))
                                Talk(SAY_PHASE_2, player);

                            DoCastAOE(SPELL_EJECT_PASSENGER_1, true);
                            DoCastAOE(SPELL_JAWS_OF_DEATH_PERIODIC);
                            DoCastAOE(SPELL_FLY_STATE_VISUAL);
                        });
                    return;
                }

                context.Repeat();
            });
    }

    bool EvadeCheck() const
    {
        Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid);
        if (!player)
            return false;

        switch (_phase)
        {
        case PHASE_INITIAL:
        case PHASE_MOUTH:
            if (!player->IsAlive())
                return false;
            break;
        case PHASE_DEAD:
            break;
        default:
            ABORT();
            break;
        }

        return true;
    }

    void UpdateAI(uint32 diff) override
    {
        if (!_playerGuid)
        {
            if (UpdateVictim())
                DoMeleeAttackIfReady();

            return;
        }

        if (_playerCheckTimer <= diff)
        {
            if (!EvadeCheck())
                EnterEvadeMode(EVADE_REASON_NO_HOSTILES);

            _playerCheckTimer = 1 * IN_MILLISECONDS;
        }
        else
            _playerCheckTimer -= diff;

        if (_manualExit)
        {
            _manualExit = false;

            if (Player* player = ObjectAccessor::GetPlayer(*me, _playerGuid))
                player->SetFullHealth();

            EnterEvadeMode(EVADE_REASON_NO_HOSTILES);
            return;
        }

        _scheduler.Update(diff);
    }

private:
    uint8 _phase;
    uint32 _playerCheckTimer;
    ObjectGuid _playerGuid;
    TaskScheduler _scheduler;
    Position _spawnPosition;
    bool _manualExit = false;
};

// 81529 - Player Mount Wyrm
class spell_player_mount_wyrm_custom : public AuraScript
{
    PrepareAuraScript(spell_player_mount_wyrm_custom);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_FIGHT_WYRM_CUSTOM });
    }

    void HandleDummy(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        GetTarget()->CastSpell(nullptr, SPELL_FIGHT_WYRM_CUSTOM, true);
    }

    void Register() override
    {
        AfterEffectRemove += AuraEffectApplyFn(spell_player_mount_wyrm_custom::HandleDummy, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};


enum HexOfMendingSpells
{
    SPELL_HEX_OF_MENDING      = 67534,
    SPELL_HEX_OF_MENDING_HEAL = 67535
};

class spell_hex_of_mending : public AuraScript
{
    PrepareAuraScript(spell_hex_of_mending);

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        return eventInfo.GetHealInfo() != nullptr;
    }

    void HandleProc(const AuraEffect* aurEff, ProcEventInfo& eventInfo)
    {
        HealInfo* healInfo = eventInfo.GetHealInfo();

        if (!healInfo)
            return;

        uint32 healAmount = healInfo->GetHeal();

        if (!healAmount)
            return;

        healInfo->SetEffectiveHeal(0);

        GetTarget()->CastSpell(GetTarget(), SPELL_HEX_OF_MENDING_HEAL, aurEff);
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_hex_of_mending::CheckProc);

        OnEffectProc += AuraEffectProcFn(
            spell_hex_of_mending::HandleProc,
            EFFECT_0,
            SPELL_AURA_DUMMY);
    }
};

void AddSC_npc_wilder_protodrache_custom()
{
    RegisterCreatureAI(npc_wilder_protodrache);
    RegisterSpellScript(spell_player_mount_wyrm_custom);
    RegisterSpellScript(spell_hex_of_mending);
}

