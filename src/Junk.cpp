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
    
}