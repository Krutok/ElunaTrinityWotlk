#ifndef CU_BRUNNEN_DER_EWIGKEIT_H
#define CU_BRUNNEN_DER_EWIGKEIT_H

#include "CreatureAIImpl.h"

#define BrunnenDerEwigkeitScriptName "instance_brunnen_der_ewigkeit"
#define DataHeader             "BDE"

inline constexpr uint32 EncounterCount = 3;

template <class AI, class T>
inline AI* GetBrunnenDerEwigkeitAI(T* obj)
{
    return GetInstanceAI<AI>(obj, BrunnenDerEwigkeitScriptName);
}

#define RegisterBrunnenDerEwigkeitCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetBrunnenDerEwigkeitAI)

#endif // CU_BRUNNEN_DER_EWIGKEIT_H
