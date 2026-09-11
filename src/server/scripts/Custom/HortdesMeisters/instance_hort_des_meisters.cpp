#include "InstanceScript.h"
#include "hort_des_meisters.h"

class instance_hort_des_meisters : public InstanceMapScript
{
public:
    instance_hort_des_meisters() : InstanceMapScript(HortdesMeistersScriptName, 730) { }

    struct instance_hort_des_meisters_InstanceScript : public InstanceScript
    {
        instance_hort_des_meisters_InstanceScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_hort_des_meisters_InstanceScript(map);
    }
};

void AddSC_instance_hort_des_meisters()
{
    new instance_hort_des_meisters();
}
