/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "ScriptMgr.h"
#include "GameTime.h"
#include "InstanceScript.h"
#include "vault_of_archavon.h"
#include "BattlefieldMgr.h"
#include "CreatureTextMgr.h"
#include "World.h"
#include <set>

 /* Vault of Archavon encounters:
 1 - Archavon the Stone Watcher event
 2 - Emalon the Storm Watcher event
 3 - Koralon the Flame Watcher event
 4 - Toravon the Ice Watcher event
 */

enum VAInstanceData
{
    DATA_VOA_CLOSING_WARNING = 5
};

ObjectData const creatureData[] =
{
    { NPC_ARCHAVON, DATA_ARCHAVON },
    { NPC_EMALON,   DATA_EMALON   },
    { NPC_KORALON,  DATA_KORALON  },
    { NPC_TORAVON,  DATA_TORAVON  },
    { 32780,        DATA_VOA_TRIGGER },
    { 0,            0,            }
};

std::set<ObjectGuid> pendingVaultGhostPlayers;

class instance_vault_of_archavon : public InstanceMapScript
{
public:
    instance_vault_of_archavon() : InstanceMapScript(VoAScriptName, 624) {}

    struct instance_vault_of_archavon_InstanceMapScript : public InstanceScript
    {
        instance_vault_of_archavon_InstanceMapScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
            LoadObjectData(creatureData, nullptr);

            ArchavonDeath = 0;
            EmalonDeath = 0;
            KoralonDeath = 0;

            _vaultClosing = false;
            _vaultClosingWarning = false;
            _vaultPlayersTeleported = false;
            _vaultGhostPlayers.clear();
        }

        bool SetBossState(uint32 type, EncounterState state) override
        {
            if (!InstanceScript::SetBossState(type, state))
                return false;

            if (state != DONE)
                return true;

            switch (type)
            {
            case DATA_ARCHAVON:
                ArchavonDeath = GameTime::GetGameTime();
                break;
            case DATA_EMALON:
                EmalonDeath = GameTime::GetGameTime();
                break;
            case DATA_KORALON:
                KoralonDeath = GameTime::GetGameTime();
                break;
            default:
                return true;
            }

            // on every death of Archavon, Emalon and Koralon check our achievement
            DoCastSpellOnPlayers(SPELL_EARTH_WIND_FIRE_ACHIEVEMENT_CHECK);

            return true;
        }

        void OnPlayerEnter(Player* player) override
        {
            InstanceScript::OnPlayerEnter(player);

            Battlefield* wintergrasp = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG);

            if (!wintergrasp)
                return;

            uint32 noBattleTimer = sWorld->getIntConfig(CONFIG_WINTERGRASP_NOBATTLETIME) * MINUTE * 1000;
            uint32 closingTime = std::min(uint32(5 * MINUTE * 1000), noBattleTimer);

            if (pendingVaultGhostPlayers.erase(player->GetGUID()) > 0)
                _vaultGhostPlayers.insert(player->GetGUID());

            if (wintergrasp->IsWarTime() || wintergrasp->GetTimer() > closingTime)
                return;

            player->CastSpell(player, 65124, true);

            Creature* archavon = GetCreature(DATA_ARCHAVON);
            Creature* emalon = GetCreature(DATA_EMALON);
            Creature* koralon = GetCreature(DATA_KORALON);
            Creature* toravon = GetCreature(DATA_TORAVON);

            Creature* bosses[] = { archavon, emalon, koralon, toravon };

            for (Creature* boss : bosses)
            {
                if (!boss || !boss->IsAlive())
                    continue;

                if (VaultOfArchavonBossAI* ai = dynamic_cast<VaultOfArchavonBossAI*>(boss->AI()))
                    ai->CloseEncounter();
            }

        }

        uint32 GetData(uint32 type) const override
        {
            if (type == DATA_VOA_CLOSING_WARNING)
                return _vaultClosingWarning ? 1 : 0;

            return 0;
        }

        void SetData(uint32 type, uint32 data) override
        {
            if (type == DATA_VOA_CLOSING_WARNING)
                _vaultClosingWarning = data != 0;
        }

        void CloseVault()
        {
            DoCastSpellOnPlayers(65124);

            Creature* archavon = GetCreature(DATA_ARCHAVON);
            Creature* emalon = GetCreature(DATA_EMALON);
            Creature* koralon = GetCreature(DATA_KORALON);
            Creature* toravon = GetCreature(DATA_TORAVON);

            Creature* bosses[] = { archavon, emalon, koralon, toravon };

            for (Creature* boss : bosses)
            {
                if (!boss || !boss->IsAlive())
                    continue;

                if (VaultOfArchavonBossAI* ai = dynamic_cast<VaultOfArchavonBossAI*>(boss->AI()))
                    ai->CloseEncounter();
            }

            _vaultClosing = true;
        }

        void Update(uint32 /*diff*/) override
        {
            Battlefield* wintergrasp = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG);

            if (!wintergrasp)
                return;

            if (wintergrasp->IsWarTime())
            {
                _vaultClosing = false;
                _vaultClosingWarning = false;
                _vaultPlayersTeleported = false;
                _vaultGhostPlayers.clear();
                return;
            }

            uint32 timer = wintergrasp->GetTimer();

            uint32 noBattleTimer = sWorld->getIntConfig(CONFIG_WINTERGRASP_NOBATTLETIME) * MINUTE * 1000;
            uint32 closingTime = std::min(uint32(5 * MINUTE * 1000), noBattleTimer);
            uint32 teleportTime = closingTime / 2;

            if (_vaultClosing && !_vaultGhostPlayers.empty())
            {
                for (auto const& playerRef : instance->GetPlayers())
                {
                    if (Player* player = playerRef.GetSource())
                    {
                        auto itr = _vaultGhostPlayers.find(player->GetGUID());

                        if (itr == _vaultGhostPlayers.end())
                            continue;

                        if (!player->IsAlive())
                            continue;

                        player->CastSpell(player, 51347, true);
                        player->TeleportTo(571, 5364.852051f, 2841.636475f, 409.239624f, 3.119730f);

                        _vaultGhostPlayers.erase(itr);
                        break;
                    }
                }
            }

            if (!_vaultPlayersTeleported && timer <= teleportTime)
            {
                for (auto const& playerRef : instance->GetPlayers())
                {
                    if (Player* player = playerRef.GetSource())
                    {
                        player->CastSpell(player, 51347, true);
                        player->TeleportTo(571, 5364.852051f, 2841.636475f, 409.239624f, 3.119730f);
                    }
                }

                _vaultPlayersTeleported = true;
            }

            if (_vaultClosing || timer > closingTime)
                return;

            CloseVault();
        }

        bool CheckAchievementCriteriaMeet(uint32 criteria_id, Player const* /*source*/, Unit const* /*target*/, uint32 /*miscvalue1*/) override
        {
            switch (criteria_id)
            {
            case CRITERIA_EARTH_WIND_FIRE_10:
            case CRITERIA_EARTH_WIND_FIRE_25:
                if (ArchavonDeath && EmalonDeath && KoralonDeath)
                {
                    // instance difficulty check is already done in db (achievement_criteria_data)
                    // int() for Visual Studio, compile errors with abs(time_t)
                    return (abs(int(ArchavonDeath - EmalonDeath)) < MINUTE && \
                        abs(int(EmalonDeath - KoralonDeath)) < MINUTE && \
                        abs(int(KoralonDeath - ArchavonDeath)) < MINUTE);
                }
                break;
            default:
                break;
            }

            return false;
        }

    private:
        time_t ArchavonDeath;
        time_t EmalonDeath;
        time_t KoralonDeath;
        bool _vaultClosing;
        bool _vaultClosingWarning;
        bool _vaultPlayersTeleported;
        std::set<ObjectGuid> _vaultGhostPlayers;
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_vault_of_archavon_InstanceMapScript(map);
    }
};

class spell_vault_of_archavon_closing_warning_65124 : public SpellScript
{
    PrepareSpellScript(spell_vault_of_archavon_closing_warning_65124);

    void HandleSendEvent(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);

        if (Unit* caster = GetCaster())
        {
            if (InstanceMap* instanceMap = caster->GetMap()->ToInstanceMap())
            {
                if (InstanceScript* instance = instanceMap->GetInstanceScript())
                {
                    if (instance->GetData(DATA_VOA_CLOSING_WARNING))
                        return;

                    instance->SetData(DATA_VOA_CLOSING_WARNING, 1);

                    if (Creature* trigger = instance->instance->GetCreature(instance->GetGuidData(DATA_VOA_TRIGGER)))
                        trigger->AI()->Talk(0);
                }
            }
        }
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_vault_of_archavon_closing_warning_65124::HandleSendEvent, EFFECT_0, SPELL_EFFECT_SEND_EVENT);
    }
};

class at_vault_of_archavon : public AreaTriggerScript
{
public:
    at_vault_of_archavon() : AreaTriggerScript("at_vault_of_archavon") {}

    bool OnTrigger(Player* player, AreaTriggerEntry const* /*trigger*/) override
    {
        if (!player)
            return false;

        Battlefield* wintergrasp = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG);

        if (!wintergrasp)
            return false;

        // Wintergrasp battle is currently in progress.
        if (wintergrasp->IsWarTime())
            return false;

        uint32 noBattleTimer = sWorld->getIntConfig(CONFIG_WINTERGRASP_NOBATTLETIME) * MINUTE * 1000;
        uint32 closingTime = std::min(uint32(5 * MINUTE * 1000), noBattleTimer);

        bool isGhost = player->HasAura(8326);

        // Vault of Archavon is closing.
        // Dead players may still enter so they can resurrect inside.
        if (wintergrasp->GetTimer() <= closingTime && !isGhost)
            return false;

        // Only the faction currently controlling Wintergrasp
        // may enter the Vault of Archavon.
        if (wintergrasp->GetDefenderTeam() != player->GetTeamId())
            return false;

        if (isGhost)
            pendingVaultGhostPlayers.insert(player->GetGUID());

        player->TeleportTo(624, -507.946f, -103.067f, 157.0f, 0.0f);

        return true;
    }
};


void AddSC_instance_vault_of_archavon()
{
    new instance_vault_of_archavon();
    new at_vault_of_archavon();
    RegisterSpellScript(spell_vault_of_archavon_closing_warning_65124);
}
