#include "InstanceScript.h"
#include "die_zwischenwelt.h"

class instance_die_zwischenwelt : public InstanceMapScript
{
public:
    instance_die_zwischenwelt() : InstanceMapScript(ZwischenweltScriptName, 734) { }

    struct instance_die_zwischenwelt_InstanceScript : public InstanceScript
    {
        instance_die_zwischenwelt_InstanceScript(InstanceMap* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(EncounterCount);
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_die_zwischenwelt_InstanceScript(map);
    }
};

void AddSC_instance_die_zwischenwelt()
{
    new instance_die_zwischenwelt();
}
