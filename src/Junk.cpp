#include "junk.h"

namespace JunkIt {

    RE::ItemList* JunkHandler::GetItemListMenu() {
        const auto UI = RE::UI::GetSingleton();
        RE::ItemList* itemListMenu = nullptr;
        if (UI && UI->IsMenuOpen("InventoryMenu")) {
            itemListMenu = UI->GetMenu<RE::InventoryMenu>()->GetRuntimeData().itemList;
            JunkHandler::MenuName = "InventoryMenu";
        } else if (UI && UI->IsMenuOpen("ContainerMenu")) {
            itemListMenu = UI->GetMenu<RE::ContainerMenu>()->GetRuntimeData().itemList;
            JunkHandler::MenuName = "ContainerMenu";
        } else if (UI && UI->IsMenuOpen("BarterMenu")) {
            itemListMenu = UI->GetMenu<RE::BarterMenu>()->GetRuntimeData().itemList;
            JunkHandler::MenuName = "BarterMenu";
        } else	{
            SKSE::log::info("No open menu found");
            return nullptr;
        }

        if (!itemListMenu) {
            SKSE::log::info("No ItemListMenu found");
            return nullptr;
        }

        JunkHandler::ItemListMenu = itemListMenu;

        return itemListMenu;
    }

    
	void JunkHandler::TransferItem(
        TESBoundObject* a_item, 
        TESObjectREFR* a_fromContainer, 
        TESObjectREFR* a_toContainer, 
        ITEM_REMOVE_REASON a_reason, 
        std::int32_t a_count, 
        InventoryEntryData* a_invData) 
    {
        using Count = std::int32_t;

        // Check if the invData is valid or not, if it's not just do a generic item move
        if (!a_invData || !a_invData->extraLists || a_invData->extraLists->empty()) {
            a_fromContainer->RemoveItem(a_item, a_count, a_reason, nullptr, a_toContainer);
            return;
       }

       Count remainingCount = a_count;

        // Iterate through the ExtraDataLists and add each one to the receiving container
        std::for_each(a_invData->extraLists->begin(), a_invData->extraLists->end(), [&](ExtraDataList* dataList) {
            if (dataList == nullptr) {
                SKSE::log::error(":: [0x{:X}] ExtraDataList in inventory is null or invalid", a_item->GetFormID());
                return -1;
            }
    
            Count itemCount = dataList->GetCount();
            a_fromContainer->RemoveItem(a_item, itemCount, a_reason, dataList, a_toContainer);
            remainingCount -= itemCount;
            return 1;
        });

        // Get any items missed by the ExtraDataLists
        if (remainingCount > 0) {
            a_fromContainer->RemoveItem(a_item, remainingCount, a_reason, nullptr, a_toContainer);
        }
    }
}
