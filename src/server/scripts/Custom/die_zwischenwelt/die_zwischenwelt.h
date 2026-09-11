#ifndef CU_DIE_ZWISCHENWELT_H
#define CU_DIE_ZWISCHENWELT_H

#include "CreatureAIImpl.h"

#define ZwischenweltScriptName "instance_die_zwischenwelt"
#define DataHeader             "ZW"

inline constexpr uint32 EncounterCount = 2;

template <class AI, class T>
inline AI* GetZwischenweltAI(T* obj)
{
    return GetInstanceAI<AI>(obj, ZwischenweltScriptName);
}

#define RegisterZwischenweltCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetZwischenweltAI)

#endif // CU_DIE_ZWISCHENWELT_H
