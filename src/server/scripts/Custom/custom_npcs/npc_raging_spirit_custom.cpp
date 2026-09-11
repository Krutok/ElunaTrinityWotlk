enum eRagingSpirit
{
    SPELL_PLAGUE_AVOIDANCE = 72846, // Dummy Aura für Seuchenvermeidung
    SPELL_RAGING_SPIRIT = 69200, // Beschwörungs-Spell (SpellScript extern)
    SPELL_RAGING_SPIRIT_VISUAL = 69197, // Grafik für den Spirit
    SPELL_RAGING_SPIRIT_VISUAL_CLONE = 69198, // optional Kopie des Summoner-Visuals
    SPELL_BOSS_HITTIN_YA = 69208, // Nahkampf-Buff/Marker
    SPELL_SOUL_SHRIEK = 69242, // AoE-Schrei

    EVENT_SOUL_SHRIEK = 1
};

struct npc_raging_spirit_custom : public ScriptedAI
{
    npc_raging_spirit_custom(Creature* creature) : ScriptedAI(creature) {}

    void Reset() override
    {
        // Sofort aggressiv, damit NPC Spieler direkt verfolgt
        me->SetReactState(REACT_AGGRESSIVE);

        _events.Reset();
        _events.ScheduleEvent(EVENT_SOUL_SHRIEK, 12s, 15s);

        // Buffs / Visuals
        DoCastSelf(SPELL_PLAGUE_AVOIDANCE, true);
        DoCastSelf(SPELL_RAGING_SPIRIT_VISUAL, true);
        DoCastSelf(SPELL_BOSS_HITTIN_YA, true);
    }

    void IsSummonedBy(WorldObject* /*summoner*/) override
    {
        if (TempSummon* summon = me->ToTempSummon())
            if (Unit* summonerUnit = summon->GetSummonerUnit())
            {
                // Visual-Kopie
                summonerUnit->CastSpell(me, SPELL_RAGING_SPIRIT_VISUAL_CLONE, true);

                // Sofort Aggro auf Spieler starten
                me->SetReactState(REACT_AGGRESSIVE);
                //me->Attack(summonerUnit, true);
                me->AI()->AttackStart(summonerUnit);
            }
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (TempSummon* summon = me->ToTempSummon())
            summon->SetTempSummonType(TEMPSUMMON_CORPSE_DESPAWN);
    }

    void UpdateAI(uint32 diff) override
    {
        _events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = _events.ExecuteEvent())
        {
            switch (eventId)
            {
            case EVENT_SOUL_SHRIEK:
                DoCastAOE(SPELL_SOUL_SHRIEK);
                _events.ScheduleEvent(EVENT_SOUL_SHRIEK, 12s, 15s);
                break;
            default:
                break;
            }
        }

        // Nahkampfangriff ? NPC folgt dem Spieler automatisch
        if (Unit* victim = me->GetVictim())
            DoMeleeAttackIfReady();

        // Despawn, wenn kein Kampf
        if (!me->IsInCombat())
            me->DespawnOrUnsummon(1s);
    }

private:
    EventMap _events;
};

// Factory-Funktion
CreatureAI* GetAI_npc_raging_spirit_custom(Creature* creature)
{
    return new npc_raging_spirit_custom(creature);
}

// Script-Register
void AddSC_npc_raging_spirit_custom()
{
    RegisterCreatureAI(npc_raging_spirit_custom);
}