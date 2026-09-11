#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-source-encoding"
#include "ScriptedGossip.h"
#include "WorldSession.h"
#include "Guild.h"
#include "GameObject.h"
#include "ObjectMgr.h"

static const std::vector<uint32> buffSpells = { 48470, 42995, 25898, 48938, 27683, 48073, 48161 };

constexpr uint32 guildBankObject = 132834;
constexpr uint32 utilityCreature = 100008;

constexpr uint64 guildBankDespawnTime = 3 * MINUTE * IN_MILLISECONDS;
constexpr uint64 utilityCreatureDespawn = 3 * MINUTE * IN_MILLISECONDS;


enum class GossipOptions : uint32
{
    Bank,
    Mailbox,
    ActionHouse,
    GuildBank,
    Repair,
    Buffs,
    CloseMenu
};

template<typename T>
class DespawnObject : public BasicEvent
{
public:
    DespawnObject(T* obj, bool& spawned) : BasicEvent(), object(obj), spawned(spawned) {}

    bool Execute(uint64 /*time_t*/, uint32 /*time_e*/) override
    {
        object->DespawnOrUnsummon();
        spawned = false;
        return true;
    }

private:
    T* object;
    bool& spawned;
};


struct npc_utility_creature : public ScriptedAI
{
public:
    npc_utility_creature(Creature* creature) : ScriptedAI(creature)
    {
        guildBankSpawned = false;
    }

    bool OnGossipHello(Player* player)
    {
        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GossipOptionIcon(0), "|TInterface/ICONS/inv_misc_bag_10:30:30:-21:0|tDeine Bank", GOSSIP_SENDER_MAIN, static_cast<uint32>(GossipOptions::Bank));
        AddGossipItemFor(player, GossipOptionIcon(0), "|TInterface/ICONS/inv_letter_18:30:30:-21:0|tBriefkasten", GOSSIP_SENDER_MAIN, static_cast<uint32>(GossipOptions::Mailbox));
        AddGossipItemFor(player, GossipOptionIcon(0), "|TInterface/ICONS/inv_misc_coin_04:30:30:-21:0|tGilden Bank", GOSSIP_SENDER_MAIN, static_cast<uint32>(GossipOptions::GuildBank));
        AddGossipItemFor(player, GossipOptionIcon(0), "|TInterface/ICONS/inv_misc_coin_02:30:30:-21:0|tAuktionshaus", GOSSIP_SENDER_MAIN, static_cast<uint32>(GossipOptions::ActionHouse));
        AddGossipItemFor(player, GossipOptionIcon(0), "|TInterface/ICONS/ability_repair:30:30:-21:0|tReparieren und Verkaufen", GOSSIP_SENDER_MAIN, static_cast<uint32>(GossipOptions::Repair));
        AddGossipItemFor(player, GossipOptionIcon(0), "|TInterface/ICONS/spell_holy_auramastery:30:30:-21:0|tBuffen", GOSSIP_SENDER_MAIN, static_cast<uint32>(GossipOptions::Buffs));
        AddGossipItemFor(player, GossipOptionIcon(0), "|TInterface/ICONS/ability_spy:30:30:-21:0|tSchließe Menü", GOSSIP_SENDER_MAIN, static_cast<uint32>(GossipOptions::CloseMenu));
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me);
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*send*/, uint32 gossipInfoDef)
    {
        auto action = player->PlayerTalkClass->GetGossipOptionAction(gossipInfoDef);

        const auto sendNotif = [&player](std::string notif) -> void
        {
            player->GetSession()->SendNotification("%s", notif.c_str());
        };

        auto sourcedEnum = static_cast<GossipOptions>(action);
        switch (sourcedEnum)
        {
        case GossipOptions::Bank:
            player->GetSession()->SendShowBank(player->GetGUID());
            break;

        case GossipOptions::Mailbox:
            player->GetSession()->SendShowMailBox(player->GetGUID());
            break;

        case GossipOptions::GuildBank:
        {
            if (guildBankSpawned)
            {
                sendNotif("Du hast eine gespawnte Gildenbank!");
                OnGossipHello(player);
                return false;
            }
            Guild* guild = player->GetGuild();
            if (!guild)
            {
                sendNotif("Du bist in keiner Gilde!");
                OnGossipHello(player);
                return false;
            }
            else
            {
                Position pos = me->GetPosition();
                pos.m_positionX -= 5.0f;

                GameObject* go = new GameObject();
                auto rot = QuaternionData::fromEulerAnglesZYX(0.f, 0.f, 0.f);
                auto map = me->GetMap();
                if (!go->Create(map->GenerateLowGuid<HighGuid::GameObject>(), guildBankObject, map, PHASEMASK_NORMAL, pos, rot, 255, GO_STATE_READY))
                {
                    delete go;
                    return false;
                }
                map->AddToMap(go);
                auto guid = go->GetSpawnId();
                GameObjectData& data = sObjectMgr->NewOrExistGameObjectData(guid);
                data.spawnId = guid;
                data.id = guildBankObject;
                data.dbData = false;
                data.spawnGroupData = sObjectMgr->GetLegacySpawnGroup();

                sObjectMgr->AddGameobjectToGrid(guid, &data);
                std::string notif{ "Du hast eine temporäre Gildenbank beschworen, sie verschwindet in 3 Minuten." };
                sendNotif(notif);
                go->m_Events.AddEvent(new DespawnObject(go, guildBankSpawned), go->m_Events.CalculateTime(Milliseconds(guildBankDespawnTime)));
                guildBankSpawned = true;
            }
        }
        break;

        case GossipOptions::ActionHouse:
            player->GetSession()->SendAuctionHello(me->GetGUID(), me->ToCreature());
            break;

        case GossipOptions::Repair:
            player->GetSession()->SendListInventory(me->GetGUID());
            break;

        case GossipOptions::Buffs:
        {
            for (const auto& spellId : buffSpells)
                player->AddAura(spellId, player);

            sendNotif("Du wurdest gebufft");
            OnGossipHello(player);
        }
        break;

        case GossipOptions::CloseMenu: CloseGossipMenuFor(player); break;
        }

        return true;
    }

protected:
    bool guildBankSpawned;
};

class is_utility_spawner : public ItemScript
{
public:
    is_utility_spawner() : ItemScript("is_utility_spawner")
    {
        creatureSpawned = false;
    }

    bool OnUse(Player* player, Item* /*item*/, SpellCastTargets const& /*targets*/) override
    {
        if (creatureSpawned)
        {
            player->GetSession()->SendNotification("Du hast bereits eine gespawnten Helfer!");
            return false;
        }
        float px, py, pz;
        player->GetClosePoint(px, py, pz, player->GetCombatReach());

        Position pos{ px, py, pz, player->GetOrientation() };
        auto tempSum = player->GetMap()->SummonCreature(utilityCreature, pos);
        tempSum->SetFacingToObject(player);
        std::string hello = "Hey there " + player->GetName() + "!\nWie kann ich helfen?";
        tempSum->Say(hello.c_str(), Language::LANG_UNIVERSAL);

        std::string notif{ "Du hast einen temporären Hilfs-NPC beschworen, er verschwindet in 3 Minuten." };
        player->GetSession()->SendNotification("%s", notif.c_str());

        tempSum->m_Events.AddEvent(new DespawnObject(tempSum, creatureSpawned), tempSum->m_Events.CalculateTime(Milliseconds(utilityCreatureDespawn)));
        creatureSpawned = true;

        return true;
    }

private:
    bool creatureSpawned;
};

void AddSC_utility_creature()
{
    RegisterCreatureAI(npc_utility_creature);
    new is_utility_spawner();
}