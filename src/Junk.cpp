#include "junk.h"
#include "JunkData.h"
#include "SendUIMessage.h"
#include <algorithm>
#include <unordered_map>

RE::MessageBoxData::~MessageBoxData() = default;

namespace JunkIt {

    std::atomic<bool> JunkHandler::operationInProgress{ false };

    void JunkHandler::ShowConfirmationMessageBox(const char* bodyText, std::vector<std::string> buttons, std::function<void(unsigned int)> callback) {
        auto messageBoxData = new RE::MessageBoxData();
        messageBoxData->bodyText = bodyText;
        for (const auto& button : buttons) {
            messageBoxData->buttonText.push_back(button.c_str());
        }
        messageBoxData->optionIndexOffset = 4;
        messageBoxData->type = 10;
        messageBoxData->menuDepth = 10;
        messageBoxData->callback = RE::BSTSmartPointer<RE::IMessageBoxCallback>(new JunkItMessageBoxCallback(std::move(callback), messageBoxData->optionIndexOffset));
        messageBoxData->QueueMessage();
    }

    JunkHandler::Count JunkHandler::GetItemCount(TESObjectREFR* a_container, TESBoundObject* a_item) {
        if (!a_container || !a_item) {
            return 0;
        }
        const auto invCounts = a_container->GetInventoryCounts();
        const auto it = invCounts.find(a_item);
        return it != invCounts.end() ? it->second : 0;
    }

    void JunkHandler::MoveItems(TESBoundObject* a_item, TESObjectREFR* a_from, TESObjectREFR* a_to, ITEM_REMOVE_REASON a_reason, Count a_count) {
        if (!a_item || !a_from || !a_to || a_count <= 0) {
            return;
        }
        a_from->RemoveItem(a_item, a_count, a_reason, nullptr, a_to);
    }

    std::vector<InventoryEntryData*> JunkHandler::BuildTransferList() {
        SKSE::log::info(" ");
        SKSE::log::info("---- Finding Transferrable Junk ----");

        std::vector<InventoryEntryData*> transferList;

        const auto ui = RE::UI::GetSingleton();
        GPtr<ContainerMenu> containerMenu = ui ? ui->GetMenu<ContainerMenu>() : nullptr;
        ItemList* itemListMenu = containerMenu ? containerMenu->GetRuntimeData().itemList : nullptr;
        if (!itemListMenu) {
            SKSE::log::error("No ItemListMenu found");
            return transferList;
        }

        BSTArray<ItemList::Item*> listItems = itemListMenu->items;
        std::vector<InventoryEntryData*> sortFormData;

        SKSE::log::info("Processing Entry List for transferable junk items");
        auto& junkManager = JunkDataManager::GetSingleton();

        for (std::uint32_t i = 0, size = listItems.size(); i < size; i++) {
            ItemList::Item* entryItem = listItems[i];
            if (!entryItem) continue;

            if (!junkManager.IsJunk(entryItem->data.objDesc)) continue;

            if (Settings::ProtectEquipped() && entryItem->data.objDesc->IsWorn()) {
                SKSE::log::info("Junk Item Equipped - Skipping {}", entryItem->data.objDesc->object->GetName());
                continue;
            }
            if (Settings::ProtectFavorites() && entryItem->data.objDesc->IsFavorited()) {
                SKSE::log::info("Junk Item Favorited - Skipping {}", entryItem->data.objDesc->object->GetName());
                continue;
            }

            sortFormData.push_back(entryItem->data.objDesc);
        }

        auto priority = Settings::GetTransferPriority();
        if (priority == Settings::SortPriority::kWeightHighLow) {
            std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) { return a->GetWeight() > b->GetWeight(); });
        } else if (priority == Settings::SortPriority::kWeightLowHigh) {
            std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) { return a->GetWeight() < b->GetWeight(); });
        } else if (priority == Settings::SortPriority::kValueHighLow) {
            std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) { return a->GetValue() > b->GetValue(); });
        } else if (priority == Settings::SortPriority::kValueLowHigh) {
            std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) { return a->GetValue() < b->GetValue(); });
        } else if (priority == Settings::SortPriority::kValueWeightHighLow) {
            std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
                float aVW = a->GetWeight() != 0 ? a->GetValue() / a->GetWeight() : 0;
                float bVW = b->GetWeight() != 0 ? b->GetValue() / b->GetWeight() : 0;
                return aVW > bVW;
            });
        } else if (priority == Settings::SortPriority::kValueWeightLowHigh) {
            std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
                float aVW = a->GetWeight() != 0 ? a->GetValue() / a->GetWeight() : 0;
                float bVW = b->GetWeight() != 0 ? b->GetValue() / b->GetWeight() : 0;
                return aVW < bVW;
            });
        }

        SKSE::log::info("Finalized TransferList:");
        for (InventoryEntryData* entryData : sortFormData) {
            const TESBoundObject* entryObject = entryData->object;
            if (!entryObject) continue;
            transferList.push_back(entryData);
            SKSE::log::info("     {} [{}]", entryObject->GetName(), FormUtil::Form::GetFormConfigString(entryData->object->As<TESForm>()));
        }

        SKSE::log::info("---- Completed Junk Transfer List Generation ----");
        SKSE::log::info(" ");
        return transferList;
    }

    std::vector<std::pair<InventoryEntryData*, std::int32_t>> JunkHandler::BuildSellList() {
        SKSE::log::info(" ");
        SKSE::log::info("---- Finding Sellable Junk ----");

        std::vector<std::pair<InventoryEntryData*, std::int32_t>> sellList;

        auto& junkManager = JunkDataManager::GetSingleton();
        auto junkInventory = junkManager.GetPlayerJunkInventory();
        if (junkInventory.empty()) {
            SKSE::log::info("No junk items in player inventory");
            return sellList;
        }

        const auto ui = RE::UI::GetSingleton();
        GPtr<BarterMenu> barterMenu = ui ? ui->GetMenu<BarterMenu>() : nullptr;
        ItemList* itemListMenu = barterMenu ? barterMenu->GetRuntimeData().itemList : nullptr;
        if (!itemListMenu) {
            SKSE::log::error("No ItemListMenu found");
            return sellList;
        }

        std::unordered_map<std::string, InventoryEntryData*> barterIndex;
        for (std::uint32_t i = 0, size = itemListMenu->items.size(); i < size; i++) {
            auto* item = itemListMenu->items[i];
            if (item && item->data.objDesc && item->data.objDesc->object) {
                InventoryEntryData* objDesc = item->data.objDesc;
                if (objDesc->extraLists && !objDesc->extraLists->empty() && objDesc->object) {
                    for (auto* extraList : *objDesc->extraLists) {
                        if (!extraList) {
                            continue;
                        }
                        const auto identity = JunkDataManager::BuildIdentityForEntry(objDesc, extraList);
                        if (!identity.empty()) {
                            barterIndex[identity] = objDesc;
                        }
                    }
                }
            }
        }

        std::vector<std::tuple<InventoryEntryData*, std::int32_t, float>> sortData;

        SKSE::log::info("Processing player junk inventory for sellable items");
        for (const auto& junkEntry : junkInventory) {
            auto bIt = barterIndex.find(junkEntry.identity);
            if (bIt == barterIndex.end()) continue;

            InventoryEntryData* objDesc = bIt->second;
            std::int32_t count = junkEntry.count;

            if (Settings::ProtectEquipped() && objDesc->IsWorn()) {
                SKSE::log::info("Junk Item Equipped - Skipping {}", objDesc->object->GetName());
                continue;
            }
            if (Settings::ProtectFavorites() && objDesc->IsFavorited()) {
                SKSE::log::info("Junk Item Favorited - Skipping {}", objDesc->object->GetName());
                continue;
            }
            if (Settings::ProtectEnchanted() && objDesc->IsEnchanted()) {
                SKSE::log::info("Junk Item Enchanted - Skipping {}", objDesc->object->GetName());
                continue;
            }

            sortData.emplace_back(objDesc, count, 0.0f);
        }

        auto priority = Settings::GetSellPriority();
        if (priority == Settings::SortPriority::kWeightHighLow) {
            std::sort(sortData.begin(), sortData.end(), [](const auto& a, const auto& b) {
                return std::get<0>(a)->GetWeight() > std::get<0>(b)->GetWeight();
            });
        } else if (priority == Settings::SortPriority::kWeightLowHigh) {
            std::sort(sortData.begin(), sortData.end(), [](const auto& a, const auto& b) {
                return std::get<0>(a)->GetWeight() < std::get<0>(b)->GetWeight();
            });
        } else if (priority == Settings::SortPriority::kValueHighLow) {
            std::sort(sortData.begin(), sortData.end(), [](const auto& a, const auto& b) {
                return std::get<0>(a)->GetValue() > std::get<0>(b)->GetValue();
            });
        } else if (priority == Settings::SortPriority::kValueLowHigh) {
            std::sort(sortData.begin(), sortData.end(), [](const auto& a, const auto& b) {
                return std::get<0>(a)->GetValue() < std::get<0>(b)->GetValue();
            });
        } else if (priority == Settings::SortPriority::kValueWeightHighLow) {
            std::sort(sortData.begin(), sortData.end(), [](const auto& a, const auto& b) {
                auto* entryA = std::get<0>(a);
                auto* entryB = std::get<0>(b);
                float aVW = entryA->GetWeight() != 0 ? entryA->GetValue() / entryA->GetWeight() : 0;
                float bVW = entryB->GetWeight() != 0 ? entryB->GetValue() / entryB->GetWeight() : 0;
                return aVW > bVW;
            });
        } else if (priority == Settings::SortPriority::kValueWeightLowHigh) {
            std::sort(sortData.begin(), sortData.end(), [](const auto& a, const auto& b) {
                auto* entryA = std::get<0>(a);
                auto* entryB = std::get<0>(b);
                float aVW = entryA->GetWeight() != 0 ? entryA->GetValue() / entryA->GetWeight() : 0;
                float bVW = entryB->GetWeight() != 0 ? entryB->GetValue() / entryB->GetWeight() : 0;
                return aVW < bVW;
            });
        }

        SKSE::log::info("Finalized SellList:");
        for (auto& [objDesc, count, _] : sortData) {
            if (!objDesc->object) continue;
            sellList.push_back({objDesc, count});
            SKSE::log::info("     {} x{} [{}]", objDesc->object->GetName(), count,
                FormUtil::Form::GetFormConfigString(objDesc->object->As<TESForm>()));
        }

        SKSE::log::info("---- Generated Junk Sell FormList ----");
        SKSE::log::info(" ");
        return sellList;
    }

    void JunkHandler::ToggleIsJunk() {
        ToggleSelectedItemJunk();
    }

    void JunkHandler::TransferJunk() {
        SKSE::log::info(" ");
        SKSE::log::info("==== Starting Junk Transfer Operation ====");

        bool expected = false;
        if (!operationInProgress.compare_exchange_strong(expected, true)) {
            SKSE::log::info("TransferJunk blocked: another operation is already in progress");
            return;
        }

        auto& junkManager = JunkDataManager::GetSingleton();
        SKSE::log::info("Current Junk List Size: {}", junkManager.Size());

        if (junkManager.Size() == 0) {
            SKSE::log::info("No items in junk list, aborting transfer");
            operationInProgress.store(false);
            return;
        }

        TESObjectREFR* transferContainer = GetContainerMenuContainer();
        if (!transferContainer) {
            SKSE::log::error("Failed to get container reference");
            operationInProgress.store(false);
            return;
        }

        auto containerMode = GetContainerMode();
        SKSE::log::info("Container Mode: {}", static_cast<int>(containerMode));

        if (containerMode == ContainerMenu::ContainerMode::kPickpocket) {
            SKSE::log::info("Junk Transfer disabled while pickpocketing");
            RE::DebugMessageBox("Junk Transfer is disabled while pickpocketing");
            operationInProgress.store(false);
            return;
        }

        const auto ui = RE::UI::GetSingleton();
        auto menu = ui ? ui->GetMenu<ContainerMenu>() : nullptr;
        if (!menu || !menu->uiMovie) {
            operationInProgress.store(false);
            return;
        }

        RE::GFxValue result;
        menu->uiMovie->GetVariable(&result, "_root.Menu_mc.inventoryLists.categoryList.activeSegment");
        int menuView = static_cast<int>(result.GetNumber());

        auto transferList = BuildTransferList();
        SKSE::log::info("Transfer list contains {} unique item types", transferList.size());

        if (menuView == 0) {
            SKSE::log::info("Transfer Direction: Retrieve FROM container TO player");
            if (transferList.empty()) {
                SKSE::log::info("No Junk to retrieve!");
                RE::DebugMessageBox("No Junk to take!");
                operationInProgress.store(false);
                return;
            }

            if (Settings::ConfirmTransfer()) {
                SKSE::log::info("Showing confirmation dialog for retrieval");
                ShowConfirmationMessageBox("Retrieve all junk items from this container?", {"Yes", "No"},
                    [transferList, transferContainer, containerMode, menuView](unsigned int choice) {
                        if (choice == 0) {
                            SKSE::log::info("User confirmed retrieval");
                            ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                        } else {
                            SKSE::log::info("User cancelled retrieval");
                        }
                        operationInProgress.store(false);
                    });
            } else {
                SKSE::log::info("Confirmation disabled, proceeding with retrieval");
                ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                operationInProgress.store(false);
            }
        } else {
            SKSE::log::info("Transfer Direction: Transfer FROM player TO container");
            if (transferList.empty()) {
                SKSE::log::info("No Junk to transfer!");
                RE::DebugMessageBox("No Junk to transfer!");
                operationInProgress.store(false);
                return;
            }

            if (Settings::ConfirmTransfer()) {
                SKSE::log::info("Showing confirmation dialog for transfer");
                ShowConfirmationMessageBox("Transfer all junk items to this container?", {"Yes", "No"},
                    [transferList, transferContainer, containerMode, menuView](unsigned int choice) {
                        if (choice == 0) {
                            SKSE::log::info("User confirmed transfer");
                            ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                        } else {
                            SKSE::log::info("User cancelled transfer");
                        }
                        operationInProgress.store(false);
                    });
            } else {
                SKSE::log::info("Confirmation disabled, proceeding with transfer");
                ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                operationInProgress.store(false);
            }
        }
        SKSE::log::info("==== Junk Transfer Operation Complete ====");
        SKSE::log::info(" ");
    }

    void JunkHandler::ExecuteTransfer(std::vector<InventoryEntryData*> transferList, TESObjectREFR* transferContainer, ContainerMenu::ContainerMode containerMode, int menuView) {
        SKSE::log::info("---- Executing Junk Transfer ----");
        auto player = RE::PlayerCharacter::GetSingleton();

        ITEM_REMOVE_REASON reason = ITEM_REMOVE_REASON::kStoreInContainer;
        if (containerMode == ContainerMenu::ContainerMode::kNPCMode) {
            reason = ITEM_REMOVE_REASON::kStoreInTeammate;
            SKSE::log::info("Transfer Reason: Store in Teammate");
        } else {
            SKSE::log::info("Transfer Reason: Store in Container");
        }

        Count totalTransferred = 0;

        if (menuView == 0) {
            SKSE::log::info("Retrieving items from container...");
            if (Settings::GetNotifyOnJunkTransfer()) {
                DebugNotification("JunkIt - Processing Retrieval...");
            }

            for (auto* entryData : transferList) {
                if (!entryData || !entryData->object) continue;

                Count itemCount = GetItemCount(transferContainer, entryData->object);
                if (itemCount > 0) {
                    SKSE::log::info("Retrieving {} x{}", entryData->object->GetName(), itemCount);
                    MoveItems(entryData->object, transferContainer, player, reason, itemCount);
                    totalTransferred += itemCount;
                }
            }

            SKSE::log::info("Junk Retrieved! Total items: {}", totalTransferred);
            if (Settings::GetNotifyOnJunkTransfer()) {
                std::string msg = fmt::format("JunkIt - {} Junk Items Retrieved!", totalTransferred);
                DebugNotification(msg.c_str());
            }
        } else {
            SKSE::log::info("Transferring items to container...");
            if (Settings::GetNotifyOnJunkTransfer()) {
                DebugNotification("JunkIt - Processing Transfer...");
            }

            if (containerMode == ContainerMenu::ContainerMode::kNPCMode) {
                Actor* transferActor = transferContainer->As<Actor>();
                float maxWeight = transferActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight);
                float currentWeight = transferContainer->GetWeightInContainer();
                SKSE::log::info("[NPC Mode] CarryWeight {}/{}", currentWeight, maxWeight);

                Count totalPossibleTransferred = 0;

                for (auto* entryData : transferList) {
                    if (!entryData || !entryData->object) continue;

                    Count iCount = GetItemCount(player, entryData->object);
                    Count iTotalCount = iCount;
                    totalPossibleTransferred += iTotalCount;

                    if (iCount > 0) {
                        float itemWeight = entryData->object->GetWeight();
                        float currentWeightWithItems = (itemWeight * static_cast<float>(iCount)) + currentWeight;

                        while (currentWeightWithItems > maxWeight && iCount > 0) {
                            iCount -= 1;
                            currentWeightWithItems = (itemWeight * static_cast<float>(iCount)) + currentWeight;
                        }

                        if (iCount > 0) {
                            MoveItems(entryData->object, player, transferContainer, reason, iCount);
                            currentWeight += (itemWeight * static_cast<float>(iCount));
                            totalTransferred += iCount;
                            SKSE::log::info("Transferred {} {} [{}/{}]", iCount, entryData->object->GetName(), RoundNumber(currentWeight), RoundNumber(maxWeight));
                        }
                    }
                }

                if (totalTransferred == 0) {
                    SKSE::log::info("[NPC Mode] NPC cannot carry any more junk - transfer aborted");
                    RE::DebugMessageBox("This person cannot carry any more");
                } else if (Settings::GetNotifyOnJunkTransfer()) {
                    if (totalTransferred >= totalPossibleTransferred) {
                        SKSE::log::info("[NPC Mode] Transferred all {} junk items successfully", totalTransferred);
                        std::string msg = fmt::format("JunkIt - Transferred All {} Junk Items!", totalTransferred);
                        DebugNotification(msg.c_str());
                    } else {
                        SKSE::log::info("[NPC Mode] Transferred {} of {} possible junk items (NPC weight limit reached)", totalTransferred, totalPossibleTransferred);
                        std::string msg = fmt::format("JunkIt - Transferred {} Junk Items!", totalTransferred);
                        DebugNotification(msg.c_str());
                    }
                }
            } else {
                SKSE::log::info("[Container Mode] Transferring all items to container...");
                for (auto* entryData : transferList) {
                    if (!entryData || !entryData->object) continue;

                    Count itemCount = GetItemCount(player, entryData->object);
                    if (itemCount > 0) {
                        SKSE::log::info("Transferring {} x{}", entryData->object->GetName(), itemCount);
                        MoveItems(entryData->object, player, transferContainer, reason, itemCount);
                        totalTransferred += itemCount;
                    }
                }

                SKSE::log::info("[Container Mode] Transferred {} junk items successfully", totalTransferred);
                if (Settings::GetNotifyOnJunkTransfer()) {
                    std::string msg = fmt::format("JunkIt - Transferred {} Junk Items!", totalTransferred);
                    DebugNotification(msg.c_str());
                }
            }
        }

        SKSE::log::info("---- Transfer Execution Complete ----");
        UIUtil::ItemList::Refresh();
    }

    void JunkHandler::SellJunk() {
        SKSE::log::info(" ");
        SKSE::log::info("==== Starting Junk Sell Operation ====");

        bool expected = false;
        if (!operationInProgress.compare_exchange_strong(expected, true)) {
            SKSE::log::info("SellJunk blocked: another operation is already in progress");
            return;
        }

        auto& junkManager = JunkDataManager::GetSingleton();
        SKSE::log::info("Current Junk List Size: {}", junkManager.Size());

        if (junkManager.Size() == 0) {
            SKSE::log::info("No items in junk list, aborting sell");
            RE::DebugMessageBox("No Junk to sell!");
            operationInProgress.store(false);
            return;
        }

        auto player = RE::PlayerCharacter::GetSingleton();
        auto sellList = BuildSellList();

        SKSE::log::info("SellList generated. Entry Count: {}", sellList.size());

        if (sellList.empty()) {
            SKSE::log::info("No sellable junk in inventory!");
            RE::DebugMessageBox("No Junk to sell!");
            operationInProgress.store(false);
            return;
        }

        TESObjectREFR* vendorActorRef = GetBarterMenuContainer();
        TESObjectREFR* vendorContainer = GetBarterMenuMerchantContainer();

        if (!vendorActorRef || vendorActorRef == player->As<TESObjectREFR>()) {
            SKSE::log::error("Failed to get a valid vendor actor. Exiting Bulk Sale process.");
            RE::DebugMessageBox("JunkIt encountered an error attempting to sell items. Please report this on the JunkIt mod page.");
            operationInProgress.store(false);
            return;
        }

        SKSE::log::info("Vendor Actor: {}", vendorActorRef->GetName());

        if (!vendorContainer) {
            SKSE::log::info("Vendor Container not found, using Vendor Actor as Container.");
            vendorContainer = vendorActorRef;
        } else {
            SKSE::log::info("Vendor Container: {}", vendorContainer->GetName());
        }

        const auto ui = RE::UI::GetSingleton();
        auto menu = ui ? ui->GetMenu<BarterMenu>() : nullptr;
        if (!menu || !menu->uiMovie) {
            operationInProgress.store(false);
            return;
        }

        RE::GFxValue gfxVendorGold, gfxSellMult;
        menu->uiMovie->GetVariable(&gfxVendorGold, "_root.Menu_mc._vendorGold");
        menu->uiMovie->GetVariable(&gfxSellMult, "_root.Menu_mc._sellMult");

        float vendorGoldDisplay = static_cast<float>(gfxVendorGold.GetNumber());
        float sellMult = static_cast<float>(gfxSellMult.GetNumber());

        if (sellMult <= 0.0f) {
            SKSE::log::warn("Vendor sell multiplier from _sellMult is invalid ({}), trying fSellMult...", sellMult);
            menu->uiMovie->GetVariable(&gfxSellMult, "_root.Menu_mc.fSellMult");
            sellMult = static_cast<float>(gfxSellMult.GetNumber());
            if (sellMult > 0.0f) {
                SKSE::log::info("Successfully read fSellMult: {}", sellMult);
            } else {
                SKSE::log::error("Both _sellMult and fSellMult failed, using fallback 0.5");
                sellMult = 0.5f;
            }
        }

        SKSE::log::info("Vendor Gold: {}", vendorGoldDisplay);
        SKSE::log::info("Vendor Sell Mult: {}", sellMult);

        Count totalToSell = 0;
        Count totalPossibleToSell = 0;
        float calculatedVendorGold = vendorGoldDisplay;
        float totalSellValue = 0.0f;

        std::vector<std::pair<InventoryEntryData*, Count>> itemsToSell;

        for (auto& [entryData, itemCount] : sellList) {
            if (!entryData || !entryData->object || itemCount <= 0) continue;

            Count iCount = GetItemCount(player, entryData->object);
            if (iCount <= 0) continue;
            if (iCount > itemCount) {
                iCount = itemCount;
            }

            Count iTotalCount = iCount;
            totalPossibleToSell += iTotalCount;

            SKSE::log::info("Calculating Sell Item: {} -- player has {} of this item", entryData->object->GetName(), iCount);

            Count menuValue = GetMenuItemValue(entryData->object->As<TESForm>());
            float itemGoldValue = menuValue >= 0 ? static_cast<float>(menuValue) : static_cast<float>(entryData->GetValue());
            float sellValue = itemGoldValue * sellMult;
            float goldDifferential = calculatedVendorGold - (sellValue * static_cast<float>(iCount));

            while (RoundNumber(goldDifferential) <= 0 && iCount > 0) {
                iCount -= 1;
                goldDifferential = calculatedVendorGold - (sellValue * static_cast<float>(iCount));
            }

            if (iCount > 0) {
                calculatedVendorGold -= sellValue * static_cast<float>(iCount);
                totalSellValue += sellValue * static_cast<float>(iCount);
                totalToSell += iCount;
                itemsToSell.push_back({entryData, iCount});
                SKSE::log::info("Sell {} {} for {} gold ({} gold per item)", iCount, entryData->object->GetName(), RoundNumber(sellValue * static_cast<float>(iCount)), RoundNumber(sellValue));
            }
        }

        if (totalToSell <= 0) {
            if (totalPossibleToSell == 0) {
                SKSE::log::info("No junk items to sell!");
                RE::DebugMessageBox("No Junk to sell!");
            } else {
                SKSE::log::info("Vendor cannot afford to buy any junk! Vendor Gold: {}", vendorGoldDisplay);
                RE::DebugMessageBox("Vendor cannot afford to buy any junk!");
            }
            operationInProgress.store(false);
            return;
        }

        Count roundedSellValue = RoundNumber(totalSellValue);
        SKSE::log::info("Sale Summary: Selling {} items for {} gold (Vendor will have {} gold remaining)", totalToSell, roundedSellValue, RoundNumber(calculatedVendorGold));

        if (Settings::ConfirmSell()) {
            SKSE::log::info("Showing confirmation dialog for sale");
            std::string confirmText = fmt::format("Sell {} junk items for {} gold?", totalToSell, roundedSellValue);
            ShowConfirmationMessageBox(confirmText.c_str(), {"Yes", "No"},
                [itemsToSell, vendorActorRef, vendorContainer, roundedSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay](unsigned int choice) {
                    if (choice == 0) {
                        SKSE::log::info("User confirmed sale");
                        ExecuteSell(itemsToSell, vendorActorRef, vendorContainer, roundedSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay);
                    } else {
                        SKSE::log::info("User cancelled sale");
                    }
                    operationInProgress.store(false);
                });
        } else {
            SKSE::log::info("Confirmation disabled, proceeding with sale");
            ExecuteSell(itemsToSell, vendorActorRef, vendorContainer, roundedSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay);
            operationInProgress.store(false);
        }
        SKSE::log::info("==== Junk Sell Operation Complete ====");
        SKSE::log::info(" ");
    }

    void JunkHandler::ExecuteSell(std::vector<std::pair<InventoryEntryData*, Count>> itemsToSell, TESObjectREFR* vendorActorRef, TESObjectREFR* vendorContainer, Count totalSellValue, Count totalToSell, Count totalPossibleToSell, float vendorGoldDisplay) {
        SKSE::log::info("---- Executing Junk Sale ----");
        auto player = RE::PlayerCharacter::GetSingleton();
        const auto ui = RE::UI::GetSingleton();

        if (Settings::GetNotifyOnJunkSell()) {
            DebugNotification("JunkIt - Processing Sale...");
        }

        TESObjectMISC* gold001 = Settings::GetGold001();
        Actor* vendorActor = vendorActorRef->As<Actor>();

        SKSE::log::info("Transferring {} gold from vendor to player...", totalSellValue);
        Count goldToGimme = totalSellValue;
        Count vendorActorGold = GetItemCount(vendorActorRef, gold001);
        if (vendorActorGold > 0) {
            Count onHandGoldToGimme = goldToGimme;
            if (vendorActorGold < goldToGimme) {
                onHandGoldToGimme = vendorActorGold;
            }
            SKSE::log::info("Vendor has {} gold on hand. Taking {} gold from vendor...", vendorActorGold, onHandGoldToGimme);
            vendorActor->RemoveItem(gold001, onHandGoldToGimme, ITEM_REMOVE_REASON::kRemove, nullptr, player);
            goldToGimme -= onHandGoldToGimme;
        }

        if (goldToGimme > 0) {
            Count containerGold = GetItemCount(vendorContainer, gold001);
            if (containerGold > 0) {
                Count containerGoldToGimme = goldToGimme;
                if (containerGold < goldToGimme) {
                    containerGoldToGimme = containerGold;
                }
                SKSE::log::info("Vendor Container has {} gold. Taking {} gold...", containerGold, containerGoldToGimme);
                vendorContainer->RemoveItem(gold001, containerGoldToGimme, ITEM_REMOVE_REASON::kRemove, nullptr, player);
                goldToGimme -= containerGoldToGimme;
            }

            if (goldToGimme > 0) {
                SKSE::log::info("Vendor ran out of money! Gold owed to player {}", goldToGimme);
                player->AddObjectToContainer(gold001, nullptr, goldToGimme, nullptr);
            }
        }

        Count totalVendorGoldLeft = RoundNumber(vendorGoldDisplay) - totalSellValue;
        if (totalVendorGoldLeft < 0) totalVendorGoldLeft = 0;

        auto menu = ui ? ui->GetMenu<BarterMenu>() : nullptr;
        if (menu && menu->uiMovie) {
            RE::GFxValue goldVal(static_cast<double>(totalVendorGoldLeft));
            menu->uiMovie->SetVariable("_root.Menu_mc._vendorGold", goldVal);
        }

        SKSE::log::info("SellList Size: {}", itemsToSell.size());
        SKSE::log::info("Transferring junk items to vendor...");
        for (const auto& [entryData, count] : itemsToSell) {
            if (count > 0 && entryData && entryData->object) {
                SKSE::log::info("Selling {} x{}", entryData->object->GetName(), count);
                MoveItems(entryData->object, player, vendorContainer, ITEM_REMOVE_REASON::kRemove, count);
                SKSE::log::info("Transaction for {} {} complete", count, entryData->object->GetName());
            }
        }

        SKSE::log::info("Adding {} Speech experience", totalSellValue);
        player->AddSkillExperience(RE::ActorValue::kSpeech, static_cast<float>(totalSellValue));

        if (totalToSell >= totalPossibleToSell) {
            SKSE::log::info("Sold ALL {} Junk Items for {} Gold", totalToSell, totalSellValue);
            if (Settings::GetNotifyOnJunkSell()) {
                DebugNotification("JunkIt - Sold All Junk Items!");
            }
        } else {
            SKSE::log::info("Sold {} of {} Junk Items for {} Gold (vendor gold limit reached)", totalToSell, totalPossibleToSell, totalSellValue);
            if (Settings::GetNotifyOnJunkSell()) {
                std::string msg = fmt::format("JunkIt - Sold {} Junk Items!", totalToSell);
                DebugNotification(msg.c_str());
            }
        }

        UIUtil::ItemList::Refresh();
        SKSE::log::info("---- Sale Execution Complete ----");
    }

    TESForm* JunkHandler::ToggleSelectedItemJunk() {
        ItemList* itemListMenu = UIUtil::ItemList::GetOpenList();
        if (!itemListMenu) {
            SKSE::log::error("No ItemListMenu found");
            DebugNotification("JunkIt - No item selected!");
            return nullptr;
        }

        ItemList::Item* selectedItem = itemListMenu->GetSelectedItem();
        if (!selectedItem) {
            SKSE::log::info("No item selected in ItemListMenu. Updating UI and trying again");
            itemListMenu->Update();

            selectedItem = itemListMenu->GetSelectedItem();
            if (!selectedItem) {
                SKSE::log::error("No item selected in ItemListMenu");
                DebugNotification("JunkIt - No item selected!");
                return nullptr;
            }
        }

        InventoryEntryData* inventoryEntry = selectedItem->data.objDesc;
        if (!inventoryEntry) {
            SKSE::log::error("Error getting InventoryEntryData for {}", selectedItem->data.objDesc->GetDisplayName());
            DebugNotification("JunkIt - Failed to mark item as junk!");
            return nullptr;
        }

        TESBoundObject* itemObject = inventoryEntry->object;
        if (!itemObject) {
            SKSE::log::error("Error getting item as object for {}", inventoryEntry->GetDisplayName());
            DebugNotification("JunkIt - Failed to mark item as junk!");
            return nullptr;
        }

        TESForm* itemForm = itemObject->As<TESForm>();
        if (!itemForm) {
            SKSE::log::error("Error getting item as form for {}", inventoryEntry->GetDisplayName());
            DebugNotification("JunkIt - Failed to mark item as junk!");
            return nullptr;
        }

        std::string itemName = itemForm->GetName();
        std::string hexFormId = FormUtil::Form::GetFormConfigString(itemForm);

        if (inventoryEntry->IsQuestObject()) {
            SKSE::log::info("Cannot mark quest item {} [{}] as junk", itemName, hexFormId);
            auto& junkManager = JunkDataManager::GetSingleton();
            if (!junkManager.IsJunk(inventoryEntry)) {
                DebugNotification("JunkIt - Quest Items cannot be marked as Junk");
                return nullptr;
            }
        }

        auto& junkManager = JunkDataManager::GetSingleton();
        bool isJunk = junkManager.IsJunk(inventoryEntry);

        if (!isJunk) {
            bool needsConfirmation = false;
            std::string protectionReason;

            if (Settings::ProtectEquipped() && inventoryEntry->IsWorn()) {
                SKSE::log::info("Item is equipped and protected: {} [{}]", itemName, hexFormId);
                needsConfirmation = true;
                protectionReason = "equipped";
            } else if (Settings::ProtectFavorites() && inventoryEntry->IsFavorited()) {
                SKSE::log::info("Item is favorited and protected: {} [{}]", itemName, hexFormId);
                needsConfirmation = true;
                protectionReason = "favorited";
            }

            if (needsConfirmation) {
                SKSE::log::info("Showing confirmation dialog for protected item");
                std::string confirmText = fmt::format("Mark this {} item as junk?", protectionReason);
                ShowConfirmationMessageBox(confirmText.c_str(), {"Yes", "No"},
                    [inventoryEntry, itemForm, itemName, hexFormId](unsigned int choice) {
                        if (choice == 0) {
                            SKSE::log::info("User confirmed marking protected item as junk");
                            SKSE::log::info("Adding junk status to {} [{}]", itemName, hexFormId);
                            auto& junkManager = JunkDataManager::GetSingleton();
                            const auto addedIdentity = junkManager.AddJunkItem(inventoryEntry);

                            ItemList* itemListMenu = UIUtil::ItemList::GetOpenList();
                            if (itemListMenu) {
                                itemListMenu->Update();
                            }

                            if (addedIdentity) {
                                SKSE::log::info("Form marked as junk: {}", *addedIdentity);
                            } else {
                                SKSE::log::warn("Failed to mark form as junk: {}", itemForm->GetName());
                            }
                            if (Settings::GetNotifyOnMarkUnmark()) {
                                std::string msg = fmt::format("JunkIt - {} has been marked as junk", itemForm->GetName());
                                DebugNotification(msg.c_str());
                            }
                        } else {
                            SKSE::log::info("User cancelled marking protected item as junk");
                        }
                    });
                return itemForm;
            }
        }

        std::optional<std::string> junkIdentity;
        if (isJunk) {
            SKSE::log::info("Removing junk status from {} [{}]", itemName, hexFormId);
            junkIdentity = junkManager.RemoveJunkItem(inventoryEntry);
        } else {
            SKSE::log::info("Adding junk status to {} [{}]", itemName, hexFormId);
            junkIdentity = junkManager.AddJunkItem(inventoryEntry);
        }

        bool isNowJunk = junkManager.IsJunk(inventoryEntry);

        itemListMenu->Update();

        if (isNowJunk) {
            if (junkIdentity) {
                SKSE::log::info("Form marked as junk: {}", *junkIdentity);
            } else {
                SKSE::log::warn("Form marked as junk but no identity was returned for {}", itemForm->GetName());
            }
            if (Settings::GetNotifyOnMarkUnmark()) {
                std::string msg = fmt::format("JunkIt - {} has been marked as junk", itemForm->GetName());
                DebugNotification(msg.c_str());
            }
        } else {
            SKSE::log::info("Form: {} is no longer marked as junk", itemForm->GetName());
            if (Settings::GetNotifyOnMarkUnmark()) {
                std::string msg = fmt::format("JunkIt - {} is no longer marked as junk", itemForm->GetName());
                DebugNotification(msg.c_str());
            }
        }

        return itemForm;
    }

    TESObjectREFR* JunkHandler::GetContainerMenuContainer() {
        SKSE::log::info(" ");
        SKSE::log::info("Getting Container target data ----");
        TESObjectREFR* container = nullptr;

        const auto ui = RE::UI::GetSingleton();
        const auto menu = ui ? ui->GetMenu<ContainerMenu>() : nullptr;
        if (menu) {
            const auto refHandle = menu->GetTargetRefHandle();
            TESObjectREFRPtr refr;
            LookupReferenceByHandle(refHandle, refr);
            container = refr.get();
        }

        if (!container) {
            SKSE::log::info("     No container target found");
            return nullptr;
        }

        SKSE::log::info("     Container target {} [{}]", container->GetName(), FormUtil::Form::GetFormConfigString(container));
        return container;
    }

    TESObjectREFR* JunkHandler::GetBarterMenuContainer() {
        SKSE::log::info(" ");
        SKSE::log::info("Getting Vendor data ----");
        TESObjectREFR* container = UIUtil::Menu::GetBarterMenuTargetRef();

        if (!container) {
            SKSE::log::info("     No merchant actor container found");
            return nullptr;
        }

        SKSE::log::info("     Vendor actor {} [{}]", container->GetName(), FormUtil::Form::GetFormConfigString(container));
        return container;
    }

    TESObjectREFR* JunkHandler::GetBarterMenuMerchantContainer() {
        TESObjectREFR* merchantRef = UIUtil::Menu::GetBarterMenuTargetRef();
        if (!merchantRef) {
            SKSE::log::error("     Vendor Ref is required to get the merchant container. Exiting with error.");
            return nullptr;
        }

        TESFaction* merchantFaction = merchantRef->As<Actor>()->GetVendorFaction();
        if (!merchantFaction) {
            SKSE::log::error("     No merchant faction found - using vendor actor as container");
            return merchantRef;
        }

        SKSE::log::info("     Merchant faction found with id {} - looking up faction->merchantContainer", FormUtil::Form::GetFormConfigString(merchantFaction));
        TESObjectREFR* container = merchantFaction->vendorData.merchantContainer;
        if (!container) {
            SKSE::log::info("     Merchant container not found for faction - using vendor actor as merchantContainer");
            return merchantRef;
        }

        SKSE::log::info("     Merchant Container identified with Reference FormID {}", FormUtil::Form::GetFormConfigString(container));
        return container;
    }

    ContainerMenu::ContainerMode JunkHandler::GetContainerMode() {
        const auto ui = RE::UI::GetSingleton();
        const auto containerMenu = ui ? ui->GetMenu<ContainerMenu>() : nullptr;
        if (!containerMenu) {
            SKSE::log::info("No open menu found");
            return ContainerMenu::ContainerMode::kLoot;
        }

        ContainerMenu::ContainerMode mode = containerMenu->GetContainerMode();
        SKSE::log::info("Container Mode: {}", static_cast<std::uint32_t>(mode));
        return mode;
    }

    std::int32_t JunkHandler::GetMenuItemValue(TESForm* a_form) {
        std::int32_t goldValue = -1;

        ItemList* itemListMenu = UIUtil::ItemList::GetOpenList();
        if (!itemListMenu) {
            SKSE::log::error("No ItemListMenu found");
            return goldValue;
        }

        BSTArray<ItemList::Item*> listItems = itemListMenu->items;

        for (std::uint32_t i = 0, size = listItems.size(); i < size; i++) {
            ItemList::Item* entryItem = listItems[i];
            if (!entryItem) continue;

            InventoryEntryData* entryData = entryItem->data.objDesc;
            if (!entryData) continue;

            TESBoundObject* entryObject = entryData->object;
            if (!entryObject) continue;

            if (entryObject->GetFormID() == a_form->GetFormID()) {
                goldValue = entryData->GetValue();
                SKSE::log::info("          Value Per Item = {} gold", goldValue);
                break;
            }
        }

        return goldValue;
    }
}
