/*
 * The Dreamland
 * Author: r4dish (Discord: r4dish#0692)
 */

#ifndef CU_THE_DREAMLAND_H
#define CU_THE_DREAMLAND_H

#include "CreatureAIImpl.h"

#define TheDreamlandScriptName "instance_the_dreamland"
#define DataHeader             "TDL"

inline constexpr uint32 EncounterCount = 4;

template <class AI, class T>
inline AI* GetTheDreamlandAI(T* obj)
{
    return GetInstanceAI<AI>(obj, TheDreamlandScriptName);
}

#define RegisterTheDreamlandCreatureAI(ai_name) RegisterCreatureAIWithFactory(ai_name, GetTheDreamlandAI)

#endif // CU_THE_DREAMLAND_H
