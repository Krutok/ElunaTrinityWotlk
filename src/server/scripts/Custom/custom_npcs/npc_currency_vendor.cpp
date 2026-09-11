#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "CreatureData.h"
#include "DBCStores.h"
#include "ObjectMgr.h"
#include "WorldSession.h"
#include "SmartEnum.h"
#include "Log.h"

namespace
{
	enum ClassMask : uint32
	{
		Warrior 	= (1 << (CLASS_WARRIOR 		- 1)),
		Paladin 	= (1 << (CLASS_PALADIN 		- 1)),
		Hunter 		= (1 << (CLASS_HUNTER 		- 1)),
		Rogue 		= (1 << (CLASS_ROGUE 		- 1)),
		Priest 		= (1 << (CLASS_PRIEST 		- 1)),
		DeathKnight = (1 << (CLASS_DEATH_KNIGHT - 1)),
		Shaman      = (1 << (CLASS_SHAMAN 		- 1)),
		Mage 		= (1 << (CLASS_MAGE 		- 1)),
		Warlock		= (1 << (CLASS_WARLOCK 		- 1)),
		Druid 		= (1 << (CLASS_DRUID 		- 1)),
	};
	
	struct ItemsData
	{
		uint32 ItemId;
		uint32 ItemCount;
		uint32 ItemAllowedClasses = CLASSMASK_ALL_PLAYABLE;
	};
	
    struct ItemData
    {
        std::string_view GossipName;
        std::vector<ItemsData> Items;
        uint32 ExtendedCost;
        uint32 AllowedClasses     = CLASSMASK_ALL_PLAYABLE;
    };

    std::array<ItemData, 3> g_ItemData =
    {{
        { "1x Shadowmourne",       { { 49623, 1 } }, 1994,    ClassMask::Warrior }, 					          // 2x Shadowmourne for 10x Glowcap - only allowed for warriors
        { "2x Long Sword",         { { 923,   2 } }, 1979,    ClassMask::Hunter  | ClassMask::Rogue }, 		      // 2x long sword for 160x apexis shard - only allowed for hunters and rogues
        { "1x Weapon + 1x Shield", { { 19019, 1, ClassMask::Warrior }, { 1168, 1, ClassMask::Warrior } }, 1979, ClassMask::Warrior }
    }};


    EnumText ClassToString(Classes value)
    {
        switch (value)
        {
            case CLASS_NONE:   	     return { "", 					"" , 		    "" };
            case CLASS_WARRIOR: 	 return { "CLASS_WARRIOR", 		"Warrior", 		"" };
            case CLASS_PALADIN:      return { "CLASS_PALADIN", 		"Paladin", 		"" };
            case CLASS_HUNTER: 		 return { "CLASS_HUNTER",  		"Hunter", 		"" };
            case CLASS_ROGUE: 	     return { "CLASS_ROGUE",   		"Rogue", 		"" };
            case CLASS_PRIEST:       return { "CLASS_PRIEST",  		"Priest", 		"" };
            case CLASS_DEATH_KNIGHT: return { "CLASS_DEATH_KNIGHT", "Death Knight", "" };
            case CLASS_SHAMAN: 		 return { "CLASS_SHAMAN", 		"Shaman", 		"" };
            case CLASS_MAGE: 		 return { "CLASS_MAGE", 		"Mage", 		"" };
            case CLASS_WARLOCK: 	 return { "CLASS_WARLOCK", 		"Warlock", 		"" };
            case CLASS_DRUID: 	     return { "CLASS_DRUID", 		"Druid", 		"" };
        }
        return { "", "", "" };
    }
}

class CS_CurrencyVendor : public CreatureScript
{
public:
    CS_CurrencyVendor()
        : CreatureScript("CS_CurrencyVendor")
    { }

    class CurrencyVendorAI : public ScriptedAI
    {
    public:
        CurrencyVendorAI(Creature* creature)
            : ScriptedAI(creature)
        { }

        std::string GetCurrencyNameAndCount(uint8 index, uint8 currIndex)
        {
            std::string message;
            uint32 itemExId = 0;
            if (ItemExtendedCostEntry const* iece = sItemExtendedCostStore.LookupEntry(g_ItemData[index].ExtendedCost))
                if (iece->ItemID[currIndex])
                {
                    message += std::to_string(iece->ItemCount[currIndex]);
                    if (itemExId == 0)
                        itemExId = iece->ItemID[currIndex];

                    std::string itemName;
                    if (ItemLocale const* il = sObjectMgr->GetItemLocale(itemExId))
                        ObjectMgr::GetLocaleString(il->Name, LOCALE_deDE, itemName);

                    message += "x ";
                    message += itemName.c_str();
                }

            return message;
        }

        bool OnGossipHello(Player* player) override
        {
            ClearGossipMenuFor(player);

            for (uint32 i = 0; i != g_ItemData.size(); ++i)
            {
                std::string message = "Are you sure you wish to purchase:";

                for (uint32 j = 0; j != g_ItemData[i].Items.size(); ++j)
                {
                    message += '\n';
                    message += std::to_string(g_ItemData[i].Items[j].ItemCount);
                    message += "x ";

                    std::string itemName;
                    if (ItemLocale const* il = sObjectMgr->GetItemLocale(g_ItemData[i].Items[j].ItemId))
                        ObjectMgr::GetLocaleString(il->Name, LOCALE_deDE, itemName);

                    message += itemName.c_str();
                }

                message += " for\n";
                for (uint8 j = 0; j < MAX_ITEM_EXTENDED_COST_REQUIREMENTS; ++j)
                    message += GetCurrencyNameAndCount(i, j).c_str();
                message += "?";

                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, g_ItemData[i].GossipName.data(), GOSSIP_SENDER_MAIN, i, message.c_str(), 0, false);
            }

            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, me->GetGUID());
            return true;
        }

        bool OnGossipSelect(Player* player, uint32 /*menu_id*/, uint32 gossipListId) override
        {
            const uint32 action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
            ClearGossipMenuFor(player);

            std::vector<ItemsData> items;

            try
            {
                items = g_ItemData.at(action).Items;
            }
            catch (...)
            {
                CloseGossipMenuFor(player);
                return false;
            }

            bool successfullyBought = false;
            const auto* crItem = &g_ItemData[action];
            ItemExtendedCostEntry const* iece = sItemExtendedCostStore.LookupEntry(crItem->ExtendedCost);

            for (const auto& itemData : items)
            {
                if (crItem->ExtendedCost)
                {
                    if (!iece)
                    {
                        TC_LOG_ERROR("entities.player", "CS_CurrencyVendor Item {} has wrong ExtendedCost field value {}", itemData.ItemId, crItem->ExtendedCost);
                        CloseGossipMenuFor(player);
                        return false;
                    }

                    // honor points price
                    if (player->GetHonorPoints() < iece->HonorPoints)
                    {
                        player->SendEquipError(EQUIP_ERR_NOT_ENOUGH_HONOR_POINTS, nullptr, nullptr);
                        CloseGossipMenuFor(player);
                        return false;
                    }

                    // arena points price
                    if (player->GetArenaPoints() < iece->ArenaPoints)
                    {
                        player->SendEquipError(EQUIP_ERR_NOT_ENOUGH_ARENA_POINTS, nullptr, nullptr);
                        CloseGossipMenuFor(player);
                        return false;
                    }

                    // item base price
                    for (uint8 i = 0; i < MAX_ITEM_EXTENDED_COST_REQUIREMENTS; ++i)
                    {
                        if (iece->ItemID[i] && !player->HasItemCount(iece->ItemID[i], iece->ItemCount[i]))
                        {
                            std::string err = "|cffff0000You don't have enough:\n";
                            err += GetCurrencyNameAndCount(action, i).c_str();
                            player->GetSession()->SendAreaTriggerMessage("%s", err.c_str());
                            CloseGossipMenuFor(player);
                            return false;
                        }
                    }

                    // check for personal arena rating requirement
                    if (player->GetMaxPersonalArenaRatingRequirement(iece->ArenaBracket) < iece->RequiredArenaRating)
                    {
                        // probably not the proper equip err
                        player->SendEquipError(EQUIP_ERR_CANT_EQUIP_RANK, nullptr, nullptr);
                        CloseGossipMenuFor(player);
                        return false;
                    }

                    if ((player->GetClassMask() & crItem->AllowedClasses) == 0)
                    {
                        std::string err = "|cffff0000This item is only available to:\n";

                        for (uint32 c = CLASS_WARRIOR; c <= CLASS_DRUID; ++c)
                        {
                            if (c == 10)
                                continue;

                            if (crItem->AllowedClasses & (1 << (c - 1)))
                            {
                                err += ClassToString(Classes(c)).Title;
                                err += ",";
                            }
                        }

                        if (err[err.size() - 1] == ',')
                            err.pop_back();

                        player->GetSession()->SendAreaTriggerMessage("%s", err.c_str());
                        CloseGossipMenuFor(player);
                        return false;
                    }

                    // add item if class req are met
                    if (player->GetClassMask() & itemData.ItemAllowedClasses)
                    {
                        successfullyBought = true;
                        player->AddItem(itemData.ItemId, itemData.ItemCount);
                    }
                }
            }

            if (successfullyBought && iece)
            {
                // destory currency
                if (iece->HonorPoints)
                    player->ModifyHonorPoints(-int32(iece->HonorPoints));
                if (iece->ArenaPoints)
                    player->ModifyArenaPoints(-int32(iece->ArenaPoints));

                for (uint8 i = 0; i < MAX_ITEM_EXTENDED_COST_REQUIREMENTS; ++i)
                    if (iece->ItemID[i])
                        player->DestroyItemCount(iece->ItemID[i], (iece->ItemCount[i]), true);
            }

            CloseGossipMenuFor(player);
            return true;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new CurrencyVendorAI(creature);
    }
};

void AddSC_CurrencyVendor()
{
    new CS_CurrencyVendor();
}
