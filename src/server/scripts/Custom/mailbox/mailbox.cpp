#include "ScriptPCH.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "WorldSession.h"

namespace
{
    enum NpcMailBoxGossip
    {
        MAIL_BOX   = 0,
        MENU_ID    = 60015,
        HELLO_TEXT = 100019
    };
}

class Npc_MailBox : public CreatureScript
{
public:
    Npc_MailBox()
        : CreatureScript("Npc_MailBox")
    {}

    struct Npc_MailBoxAI : public ScriptedAI
    {
        Npc_MailBoxAI(Creature* creature)
            : ScriptedAI(creature)
        {}

        bool OnGossipHello(Player* player) override
        {
            return OnGossipHello(player, me);
        }

        static bool OnGossipHello(Player* player, Creature* creature)
        {
            AddGossipItemFor(player, MENU_ID, MAIL_BOX, GOSSIP_SENDER_MAIN, MAIL_BOX);
            SendGossipMenuFor(player, HELLO_TEXT, creature->GetGUID());
            return true;
        }

        bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            uint32 const sender = player->PlayerTalkClass->GetGossipOptionSender(gossipListId);
            uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
            return OnGossipSelect(player, me, sender, action);
        }

        bool OnGossipSelect(Player* player, Creature* /*creature*/, uint32 /*sender*/, uint32 uiAction)
        {
            ClearGossipMenuFor(player);

            switch (uiAction)
            {
            case MAIL_BOX:
                me->SetNpcFlag(UNIT_NPC_FLAG_MAILBOX);
                player->GetSession()->SendShowMailBox(me->GetGUID());
                break;
            default:
                break;
            }

            CloseGossipMenuFor(player);
            return true;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new Npc_MailBoxAI(creature);
    }
};

void AddSC_Npc_MailBox()
{
    new Npc_MailBox();
}