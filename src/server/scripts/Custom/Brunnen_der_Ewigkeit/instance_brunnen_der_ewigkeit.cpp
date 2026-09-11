#include "InstanceScript.h"
#include "ScriptMgr.h"
#include "GameObject.h"
#include "brunnen_der_ewigkeit.h"

enum BrunnenDerEwigkeitDatas
{
    DATA_BOSS_1 = 0,   // Entry 100254
    DATA_BOSS_2 = 1    // Entry 100260
};

class instance_brunnen_der_ewigkeit : public InstanceMapScript
{
public:
    instance_brunnen_der_ewigkeit() : InstanceMapScript(BrunnenDerEwigkeitScriptName, 734) {}

    struct instance_brunnen_der_ewigkeit_InstanceScript : public InstanceScript
    {
        instance_brunnen_der_ewigkeit_InstanceScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
        }

        ObjectGuid normalDoorGUID;
        ObjectGuid heroicDoorGUID;

        ObjectGuid boss1GUID;
        ObjectGuid boss2GUID;

        void OnCreatureCreate(Creature* creature) override
        {
            switch (creature->GetEntry())
            {
            case 100254: // Boss 1
                boss1GUID = creature->GetGUID();
                break;

            case 100260: // Boss 2
                boss2GUID = creature->GetGUID();

                // Wenn Boss 1 noch nicht DONE -> Boss 2 immun
                if (GetBossState(DATA_BOSS_1) != DONE)
                {
                    creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                    creature->SetReactState(REACT_PASSIVE);
                }
                break;
            }

            InstanceScript::OnCreatureCreate(creature);
        }

        void OnGameObjectCreate(GameObject* go) override
        {
            if (go->GetEntry() == 600039)
                normalDoorGUID = go->GetGUID();
            else if (go->GetEntry() == 600040)
                heroicDoorGUID = go->GetGUID();

            InstanceScript::OnGameObjectCreate(go);
        }

        bool SetBossState(uint32 type, EncounterState state) override
        {
            if (!InstanceScript::SetBossState(type, state))
                return false;

            // Boss 1 DONE -> Türen despawnen + Boss 2 freischalten
            if (type == DATA_BOSS_1 && state == DONE)
            {
                // Türen despawnen
                if (GameObject* go = instance->GetGameObject(normalDoorGUID))
                    go->DespawnOrUnsummon();

                if (instance->IsHeroic())
                    if (GameObject* go = instance->GetGameObject(heroicDoorGUID))
                        go->DespawnOrUnsummon();

                // Boss 2 freischalten
                if (Creature* boss2 = instance->GetCreature(boss2GUID))
                {
                    boss2->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE);
                    boss2->SetReactState(REACT_AGGRESSIVE);
                }
            }

            return true;
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_brunnen_der_ewigkeit_InstanceScript(map);
    }
};

void AddSC_instance_brunnen_der_ewigkeit()
{
    new instance_brunnen_der_ewigkeit();
}
