#ifndef CU_HORT_DES_MEISTERS_H
#define CU_HORT_DES_MEISTERS_H

#include "CreatureAIImpl.h"

#define HortdesMeistersScriptName "instance_hort_des_meisters"
#define DataHeader             "HDM"

inline constexpr uint32 EncounterCount = 4;

template <class AI, class T>
inline AI* GetHortdesMeistersAI(T* obj)
{
    return GetInstanceAI<AI>(obj, HortdesMeistersScriptName);
}

#define RegisterHortdesMeistersCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetHortdesMeistersAI)

#endif // CU_HORT_DES_MEISTERS_H
