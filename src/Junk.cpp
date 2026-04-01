#include "junk.h"
#include "JunkData.h"
#include "SendUIMessage.h"
#include <thread>
#include <chrono>
#include <set>

RE::MessageBoxData::~MessageBoxData() = default;

namespace JunkIt {

    std::atomic<bool> JunkHandler::operationInProgress{ false };
    std::atomic<bool> refreshInProgress{ false };

    bool JunkHandler::WarnLargeInventory(TESObjectREFR* a_container1, TESObjectREFR* a_container2) {
        // std::int32_t count1 = a_container1->GetInventoryCount();
        // std::int32_t count2 = a_container2->GetInventoryCount();
        // std::int32_t totalCount = count1 + count2;

        // SKSE::log::info("Large Inventory Check: Total Menu Form Count: {}", totalCount);

        // if (totalCount >= Settings::GetWarnInventorySizeThreshold()) {
        //     SKSE::log::info("Large Container Inventory Detected!");
        //     if (Settings::GetNotifyLargeInventoryLag()) {
        //         RE::DebugMessageBox("Large Inventory detected, transfer could lag. Please allow for a few additional seconds for the transfer to complete.");
        //     }
        //     return true;
        // }

        return false;
    }

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

    std::pair<std::vector<InventoryEntryData*>, GFxObjMap> JunkHandler::BuildTransferList() {
        SKSE::log::info(" ");
        SKSE::log::info("---- Finding Transferrable Junk ----");

        std::vector<InventoryEntryData*> transferList;
        GFxObjMap gfxObjMap;

        const auto ui = RE::UI::GetSingleton();
        GPtr<ContainerMenu> containerMenu = ui ? ui->GetMenu<ContainerMenu>() : nullptr;
        ItemList* itemListMenu = containerMenu ? containerMenu->GetRuntimeData().itemList : nullptr;
        if (!itemListMenu) {
            SKSE::log::error("No ItemListMenu found");
            return {transferList, gfxObjMap};
        }

        BSTArray<ItemList::Item*> listItems = itemListMenu->items;
        std::vector<InventoryEntryData*> sortFormData;
        std::unordered_map<InventoryEntryData*, RE::GFxValue> sortGfxMap;

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
            sortGfxMap[entryItem->data.objDesc] = entryItem->obj;
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
            gfxObjMap[entryData] = sortGfxMap[entryData];
            SKSE::log::info("     {} [{}]", entryObject->GetName(), FormUtil::Form::GetFormConfigString(entryData->object->As<TESForm>()));
        }

        SKSE::log::info("---- Completed Junk Transfer List Generation ----");
        SKSE::log::info(" ");
        return {transferList, gfxObjMap};
    }

    std::pair<std::vector<std::pair<InventoryEntryData*, std::int32_t>>, GFxObjMap> JunkHandler::BuildSellList() {
        SKSE::log::info(" ");
        SKSE::log::info("---- Finding Sellable Junk ----");

        std::vector<std::pair<InventoryEntryData*, std::int32_t>> sellList;
        GFxObjMap gfxObjMap;

        auto& junkManager = JunkDataManager::GetSingleton();
        auto junkInventory = junkManager.GetPlayerJunkInventory();
        if (junkInventory.empty()) {
            SKSE::log::info("No junk items in player inventory");
            return {sellList, gfxObjMap};
        }

        const auto ui = RE::UI::GetSingleton();
        GPtr<BarterMenu> barterMenu = ui ? ui->GetMenu<BarterMenu>() : nullptr;
        ItemList* itemListMenu = barterMenu ? barterMenu->GetRuntimeData().itemList : nullptr;
        if (!itemListMenu) {
            SKSE::log::error("No ItemListMenu found");
            return {sellList, gfxObjMap};
        }

        std::unordered_map<uint64_t, InventoryEntryData*> barterIndex;
        std::unordered_map<InventoryEntryData*, RE::GFxValue> barterGfxMap;
        for (std::uint32_t i = 0, size = itemListMenu->items.size(); i < size; i++) {
            auto* item = itemListMenu->items[i];
            if (item && item->data.objDesc && item->data.objDesc->object) {
                RE::FormID baseFormID = item->data.objDesc->object->GetFormID();
                uint32_t extraHash = junkManager.ComputeExtraDataHash(item->data.objDesc);
                uint64_t packedKey = (static_cast<uint64_t>(baseFormID) << 32) | extraHash;
                barterIndex[packedKey] = item->data.objDesc;
                barterGfxMap[item->data.objDesc] = item->obj;
            }
        }

        std::vector<std::tuple<InventoryEntryData*, std::int32_t, float>> sortData;

        SKSE::log::info("Processing player junk inventory for sellable items");
        for (const auto& junkEntry : junkInventory) {
            uint64_t packedKey = (static_cast<uint64_t>(junkEntry.baseFormID) << 32) | junkEntry.extraDataHash;
            auto bIt = barterIndex.find(packedKey);
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
            gfxObjMap[objDesc] = barterGfxMap[objDesc];
            SKSE::log::info("     {} x{} [{}]", objDesc->object->GetName(), count,
                FormUtil::Form::GetFormConfigString(objDesc->object->As<TESForm>()));
        }

        SKSE::log::info("---- Generated Junk Sell FormList ----");
        SKSE::log::info(" ");
        return {sellList, gfxObjMap};
    }

    void JunkHandler::ToggleIsJunk() {
        ToggleSelectedItemJunk();
    }

    void JunkHandler::TransferJunk() {
        SKSE::log::info(" ");
        SKSE::log::info("==== Starting Junk Transfer Operation ====");

        if (refreshInProgress.load()) {
            SKSE::log::info("TransferJunk blocked: UI refresh verification is still running");
            RE::DebugMessageBox("Junk transfers are still being updated. Please wait a moment and try again.");
            return;
        }

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

        auto player = RE::PlayerCharacter::GetSingleton();
        float playerCarryWeight = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight);

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

        auto [transferList, gfxObjMap] = BuildTransferList();
        SKSE::log::info("Transfer list contains {} unique item types", transferList.size());

        RE::GFxMovieView* menuMovie = UIUtil::Menu::GetActiveMenuMovie();
        if (menuMovie) menuMovie->SetVisible(false);

        if (menuView == 0) {
            SKSE::log::info("Transfer Direction: Retrieve FROM container TO player");
            if (transferList.empty()) {
                SKSE::log::info("No Junk to retrieve!");
                RE::DebugMessageBox("No Junk to take!");
                if (menuMovie) menuMovie->SetVisible(true);
                operationInProgress.store(false);
                return;
            }

            if (Settings::ConfirmTransfer()) {
                SKSE::log::info("Showing confirmation dialog for retrieval");
                ShowConfirmationMessageBox("Retrieve all junk items from this container?", {"Yes", "No"},
                    [transferList, gfxObjMap, transferContainer, containerMode, menuView, playerCarryWeight, menuMovie](unsigned int choice) {
                        if (choice == 0) {
                            SKSE::log::info("User confirmed retrieval");
                            ExecuteTransfer(transferList, gfxObjMap, transferContainer, containerMode, menuView);
                            auto p = RE::PlayerCharacter::GetSingleton();
                            if (p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight) != playerCarryWeight) {
                                p->AsActorValueOwner()->SetActorValue(RE::ActorValue::kCarryWeight, playerCarryWeight);
                            }
                        } else {
                            SKSE::log::info("User cancelled retrieval");
                            if (menuMovie) menuMovie->SetVisible(true);
                        }
                        operationInProgress.store(false);
                    });
            } else {
                SKSE::log::info("Confirmation disabled, proceeding with retrieval");
                ExecuteTransfer(transferList, gfxObjMap, transferContainer, containerMode, menuView);
                if (player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight) != playerCarryWeight) {
                    player->AsActorValueOwner()->SetActorValue(RE::ActorValue::kCarryWeight, playerCarryWeight);
                }
                operationInProgress.store(false);
            }
        } else {
            SKSE::log::info("Transfer Direction: Transfer FROM player TO container");
            if (transferList.empty()) {
                SKSE::log::info("No Junk to transfer!");
                RE::DebugMessageBox("No Junk to transfer!");
                if (menuMovie) menuMovie->SetVisible(true);
                operationInProgress.store(false);
                return;
            }

            if (Settings::ConfirmTransfer()) {
                SKSE::log::info("Showing confirmation dialog for transfer");
                ShowConfirmationMessageBox("Transfer all junk items to this container?", {"Yes", "No"},
                    [transferList, gfxObjMap, transferContainer, containerMode, menuView, playerCarryWeight, menuMovie](unsigned int choice) {
                        if (choice == 0) {
                            SKSE::log::info("User confirmed transfer");
                            ExecuteTransfer(transferList, gfxObjMap, transferContainer, containerMode, menuView);
                            auto p = RE::PlayerCharacter::GetSingleton();
                            if (p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight) != playerCarryWeight) {
                                p->AsActorValueOwner()->SetActorValue(RE::ActorValue::kCarryWeight, playerCarryWeight);
                            }
                        } else {
                            SKSE::log::info("User cancelled transfer");
                            if (menuMovie) menuMovie->SetVisible(true);
                        }
                        operationInProgress.store(false);
                    });
            } else {
                SKSE::log::info("Confirmation disabled, proceeding with transfer");
                ExecuteTransfer(transferList, gfxObjMap, transferContainer, containerMode, menuView);
                if (player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight) != playerCarryWeight) {
                    player->AsActorValueOwner()->SetActorValue(RE::ActorValue::kCarryWeight, playerCarryWeight);
                }
                operationInProgress.store(false);
            }
        }
        SKSE::log::info("==== Junk Transfer Operation Complete ====");
        SKSE::log::info(" ");
    }

    void JunkHandler::ExecuteTransfer(std::vector<InventoryEntryData*> transferList, GFxObjMap gfxObjMap, TESObjectREFR* transferContainer, ContainerMenu::ContainerMode containerMode, int menuView) {
        SKSE::log::info("---- Executing Junk Transfer ----");
        auto player = RE::PlayerCharacter::GetSingleton();

        // WarnLargeInventory(player, transferContainer);

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
                
                auto invCounts = transferContainer->GetInventoryCounts();
                auto it = invCounts.find(entryData->object);
                Count itemCount = (it != invCounts.end()) ? it->second : 0;
                if (itemCount > 0) {
                    SKSE::log::info("Retrieving {} x{}", entryData->object->GetName(), itemCount);
                    TransferItem(entryData->object, transferContainer, player, reason, itemCount, entryData);
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
                    
                    auto playerInvCounts = player->GetInventoryCounts();
                    auto pIt = playerInvCounts.find(entryData->object);
                    Count iCount = (pIt != playerInvCounts.end()) ? pIt->second : 0;
                    Count iTotalCount = iCount;
                    totalPossibleTransferred += iCount;

                    if (iCount > 0) {
                        float itemWeight = entryData->object->GetWeight();
                        float currentWeightWithItems = (itemWeight * iCount) + currentWeight;

                        while (currentWeightWithItems > maxWeight && iCount > 0) {
                            iCount -= 1;
                            currentWeightWithItems = (itemWeight * iCount) + currentWeight;
                        }

                        if (iCount > 0) {
                            TransferItem(entryData->object, player, transferContainer, reason, iCount, entryData);
                            currentWeight += (itemWeight * iCount);
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
                    
                    Count itemCount = entryData->countDelta;
                    if (itemCount > 0) {
                        SKSE::log::info("Transferring {} x{}", entryData->object->GetName(), itemCount);
                        TransferItem(entryData->object, player, transferContainer, reason, itemCount, entryData);
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

        std::vector<std::pair<TESBoundObject*, std::int32_t>> expectedInSource;
        std::vector<std::pair<TESBoundObject*, std::int32_t>> expectedInDest;
        TESObjectREFR* verifySourceRef = nullptr;
        TESObjectREFR* verifyDestRef = nullptr;
        
        if (menuView == 0) {
            verifySourceRef = transferContainer;
            verifyDestRef = player;
            for (auto* entryData : transferList) {
                if (!entryData || !entryData->object) continue;
                expectedInSource.push_back({ entryData->object, 0 });
            }
            auto playerInvCounts = player->GetInventoryCounts();
            for (auto* entryData : transferList) {
                if (!entryData || !entryData->object) continue;
                auto invCounts = transferContainer->GetInventoryCounts();
                auto it = invCounts.find(entryData->object);
                Count itemCount = (it != invCounts.end()) ? it->second : 0;
                if (itemCount > 0) {
                    auto pIt = playerInvCounts.find(entryData->object);
                    Count currentInPlayer = (pIt != playerInvCounts.end()) ? pIt->second : 0;
                    expectedInDest.push_back({ entryData->object, currentInPlayer + itemCount });
                }
            }
        } else if (containerMode == ContainerMenu::ContainerMode::kNPCMode) {
            verifySourceRef = player;
            verifyDestRef = transferContainer;
            auto playerInvCounts = player->GetInventoryCounts();
            auto containerInvCounts = transferContainer->GetInventoryCounts();
            for (auto* entryData : transferList) {
                if (!entryData || !entryData->object) continue;
                auto pIt = playerInvCounts.find(entryData->object);
                Count playerCount = (pIt != playerInvCounts.end()) ? pIt->second : 0;
                Count iCount = playerCount;
                
                float itemWeight = entryData->object->GetWeight();
                Actor* transferActor = transferContainer->As<Actor>();
                float maxWeight = transferActor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight);
                float currentWeight = transferContainer->GetWeightInContainer();
                float currentWeightWithItems = (itemWeight * iCount) + currentWeight;
                
                while (currentWeightWithItems > maxWeight && iCount > 0) {
                    iCount -= 1;
                    currentWeightWithItems = (itemWeight * iCount) + currentWeight;
                }
                
                if (iCount > 0) {
                    expectedInSource.push_back({ entryData->object, playerCount - iCount });
                    auto cIt = containerInvCounts.find(entryData->object);
                    Count currentInContainer = (cIt != containerInvCounts.end()) ? cIt->second : 0;
                    expectedInDest.push_back({ entryData->object, currentInContainer + iCount });
                }
            }
        } else {
            verifySourceRef = player;
            verifyDestRef = transferContainer;
            auto containerInvCounts = transferContainer->GetInventoryCounts();
            for (auto* entryData : transferList) {
                if (!entryData || !entryData->object) continue;
                Count itemCount = entryData->countDelta;
                if (itemCount > 0) {
                    expectedInSource.push_back({ entryData->object, 0 });
                    auto cIt = containerInvCounts.find(entryData->object);
                    Count currentInContainer = (cIt != containerInvCounts.end()) ? cIt->second : 0;
                    expectedInDest.push_back({ entryData->object, currentInContainer + itemCount });
                }
            }
        }

        for (auto* entryData : transferList) {
            auto it = gfxObjMap.find(entryData);
            if (it == gfxObjMap.end()) continue;
            RE::GFxValue& obj = it->second;
            RE::GFxValue currentFlag;
            obj.GetMember("filterFlag", &currentFlag);
            if (currentFlag.IsNumber()) {
                auto flag = static_cast<std::uint32_t>(currentFlag.GetNumber());
                std::uint32_t newFlag = (menuView == 0)
                    ? (flag & 0xFFC00u) >> 10
                    : (flag & 0x003FFu) << 10;
                obj.SetMember("filterFlag", RE::GFxValue(static_cast<double>(newFlag)));
            }
        }
        if (auto* movie = UIUtil::Menu::GetActiveMenuMovie()) {
            RE::GFxValue itemListObj;
            movie->GetVariable(&itemListObj, "_root.Menu_mc.inventoryLists.panelContainer.itemList");
            if (itemListObj.IsObject())
                itemListObj.Invoke("requestUpdate", nullptr, nullptr, 0);
        }

        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            auto* ti = SKSE::GetTaskInterface();
            if (ti)
                ti->AddUITask([]() { 
                    UIUtil::ItemList::Refresh(); 
                    if (auto* movie = UIUtil::Menu::GetActiveMenuMovie())
                        movie->SetVisible(true);
                });
        }).detach();
        
        // if (verifySourceRef && !expectedInSource.empty())
        //     ScheduleVerifyAndDelayedRefresh(verifySourceRef, std::move(expectedInSource), verifyDestRef, std::move(expectedInDest));
    }

    namespace {
        bool VerifySWFEntryList(TESObjectREFR* sourceRef, const std::vector<std::pair<TESBoundObject*, std::int32_t>>& expectedCountsInSource, TESObjectREFR* destRef, const std::vector<std::pair<TESBoundObject*, std::int32_t>>& expectedCountsInDest) {
            const auto ui = RE::UI::GetSingleton();
            if (!ui) return true;

            RE::GFxMovieView* movieView = nullptr;
            bool isContainerMenu = ui->IsMenuOpen("ContainerMenu");
            bool isBarterMenu = ui->IsMenuOpen("BarterMenu");
            
            if (isContainerMenu) {
                auto menu = ui->GetMenu<ContainerMenu>();
                if (menu && menu->uiMovie)
                    movieView = menu->uiMovie.get();
            } else if (isBarterMenu) {
                auto menu = ui->GetMenu<BarterMenu>();
                if (menu && menu->uiMovie)
                    movieView = menu->uiMovie.get();
            }

            if (!movieView) return true;

            auto* itemList = UIUtil::ItemList::GetOpenList();
            if (!itemList || !itemList->items.empty() == false) return true;

            std::set<RE::FormID> expectedAbsentForms;
            for (const auto& [obj, expected] : expectedCountsInSource) {
                if (expected == 0 && obj) {
                    expectedAbsentForms.insert(obj->GetFormID());
                }
            }

            for (std::uint32_t i = 0; i < itemList->items.size(); i++) {
                auto* item = itemList->items[i];
                if (!item || !item->data.objDesc || !item->data.objDesc->object) continue;
                
                RE::FormID itemFormId = item->data.objDesc->object->GetFormID();
                if (expectedAbsentForms.count(itemFormId) > 0) {
                    SKSE::log::trace("SWF verification failed: found item that should be absent (FormID: 0x{:X})", itemFormId);
                    return false;
                }
            }

            SKSE::log::trace("SWF verification passed");
            return true;
        }

        void VerifyAndDelayedRefreshImpl(TESObjectREFR* sourceRef, std::vector<std::pair<TESBoundObject*, std::int32_t>> expectedCountsInSource, TESObjectREFR* destRef, std::vector<std::pair<TESBoundObject*, std::int32_t>> expectedCountsInDest, int attempt) {
            constexpr int maxAttempts = 2;
            
            const auto ui = RE::UI::GetSingleton();
            if (!ui || (!ui->IsMenuOpen("ContainerMenu") && !ui->IsMenuOpen("BarterMenu"))) {
                SKSE::log::info("Menu closed during verification, aborting refresh");
                refreshInProgress.store(false);
                return;
            }
            
            bool verified = true;
            
            if (sourceRef && !expectedCountsInSource.empty()) {
                auto counts = sourceRef->GetInventoryCounts();
                for (const auto& [obj, expected] : expectedCountsInSource) {
                    auto it = counts.find(obj);
                    std::int32_t actual = (it != counts.end()) ? it->second : 0;
                    if (actual != expected) {
                        verified = false;
                        break;
                    }
                }
            }
            
            if (verified && destRef && !expectedCountsInDest.empty()) {
                auto counts = destRef->GetInventoryCounts();
                for (const auto& [obj, expected] : expectedCountsInDest) {
                    auto it = counts.find(obj);
                    std::int32_t actual = (it != counts.end()) ? it->second : 0;
                    if (actual != expected) {
                        verified = false;
                        break;
                    }
                }
            }
            
            if (verified || attempt >= maxAttempts) {
                if (verified) {
                    UIUtil::ItemList::Refresh();
                    
                    bool swfVerified = VerifySWFEntryList(sourceRef, expectedCountsInSource, destRef, expectedCountsInDest);
                    
                    if (!swfVerified && attempt < maxAttempts) {
                        SKSE::log::info("Engine counts verified but SWF state mismatch, retrying (attempt {}/{})", attempt + 1, maxAttempts);
                        std::thread([=]() {
                            std::this_thread::sleep_for(std::chrono::milliseconds(150));
                            auto* ti = SKSE::GetTaskInterface();
                            if (ti)
                                ti->AddUITask([=]() {
                                    VerifyAndDelayedRefreshImpl(sourceRef, expectedCountsInSource, destRef, expectedCountsInDest, attempt + 1);
                                });
                        }).detach();
                        return;
                    }
                    
                    SKSE::log::info("Transfer verification complete, UI refreshed");
                } else {
                    SKSE::log::warn("Transfer verification timed out after {} attempts, forcing UI refresh", maxAttempts);
                    UIUtil::ItemList::Refresh();
                }
                
                refreshInProgress.store(false);
                return;
            }
            
            std::thread([=]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                auto* ti = SKSE::GetTaskInterface();
                if (ti)
                    ti->AddUITask([=]() {
                        VerifyAndDelayedRefreshImpl(sourceRef, expectedCountsInSource, destRef, expectedCountsInDest, attempt + 1);
                    });
            }).detach();
        }
    }

    void JunkHandler::ScheduleVerifyAndDelayedRefresh(TESObjectREFR* sourceRef, std::vector<std::pair<TESBoundObject*, std::int32_t>> expectedCountsInSource, TESObjectREFR* destRef, std::vector<std::pair<TESBoundObject*, std::int32_t>> expectedCountsInDest) {
        std::thread([=]() {
            float multiplier = Settings::GetHeavyLoadDelayMultiplier();
            int baseDelay = 100 + static_cast<int>(expectedCountsInSource.size()) * 10;
            int initialDelayMs = std::clamp(
                static_cast<int>(baseDelay * multiplier), 
                100, 
                static_cast<int>(2000 * multiplier)
            );

            std::this_thread::sleep_for(std::chrono::milliseconds(initialDelayMs));
            auto* ti = SKSE::GetTaskInterface();
            if (ti) {
                refreshInProgress.store(true);
                ti->AddUITask([=]() {
                    VerifyAndDelayedRefreshImpl(sourceRef, expectedCountsInSource, destRef, expectedCountsInDest, 0);
                });
            } else {
                refreshInProgress.store(false);
                UIUtil::ItemList::Refresh();
            }
        }).detach();
    }

    void JunkHandler::SellJunk() {
        SKSE::log::info(" ");
        SKSE::log::info("==== Starting Junk Sell Operation ====");

        if (refreshInProgress.load()) {
            SKSE::log::info("SellJunk blocked: UI refresh verification is still running");
            RE::DebugMessageBox("Junk sales are still being updated. Please wait a moment and try again.");
            return;
        }

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
        float playerCarryWeight = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight);

        auto [sellList, gfxObjMap] = BuildSellList();

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

        if (sellMult <= 0.0f) {
            SKSE::log::warn("Vendor sell multiplier is invalid ({}), barter prices may be incorrect!", sellMult);
        }

        Count totalToSell = 0;
        Count totalPossibleToSell = 0;
        float calculatedVendorGold = vendorGoldDisplay;
        Count totalSellValue = 0;

        std::vector<std::pair<InventoryEntryData*, Count>> itemsToSell;

        for (auto& [entryData, itemCount] : sellList) {
            if (!entryData || !entryData->object || itemCount <= 0) continue;

            totalPossibleToSell += itemCount;
            Count iCount = itemCount;

            SKSE::log::info("Calculating Sell Item: {} -- player has {} of this item", entryData->object->GetName(), iCount);

            Count adjustedValue = static_cast<Count>(std::floor(
                static_cast<float>(entryData->GetValue()) * sellMult + 0.5f));
            float goldDifferential = calculatedVendorGold - static_cast<float>(adjustedValue * iCount);

            while (goldDifferential < 0.0f && iCount > 0) {
                iCount -= 1;
                goldDifferential = calculatedVendorGold - static_cast<float>(adjustedValue * iCount);
            }

            if (iCount > 0) {
                Count itemTotal = adjustedValue * iCount;
                calculatedVendorGold -= static_cast<float>(itemTotal);
                totalSellValue += itemTotal;
                totalToSell += iCount;
                itemsToSell.push_back({entryData, iCount});
                SKSE::log::info("Sell {} {} for {} gold ({} gold per item)", iCount, entryData->object->GetName(), itemTotal, adjustedValue);
            }
        }

        if (totalToSell <= 0) {
            if (totalPossibleToSell == 0) {
                SKSE::log::info("No junk items to sell!");
                RE::DebugMessageBox("No Junk to sell!");
            } else {
                SKSE::log::info("Vendor cannot afford to buy any junk! Vendor Gold: {}, Required: {}", vendorGoldDisplay, totalSellValue);
                RE::DebugMessageBox("Vendor cannot afford to buy any junk!");
            }
            operationInProgress.store(false);
            return;
        }

        SKSE::log::info("Sale Summary: Selling {} items for {} gold (Vendor will have {} gold remaining)", totalToSell, totalSellValue, RoundNumber(calculatedVendorGold));

        RE::GFxMovieView* menuMovie = UIUtil::Menu::GetActiveMenuMovie();
        if (menuMovie) menuMovie->SetVisible(false);

        if (Settings::ConfirmSell()) {
            SKSE::log::info("Showing confirmation dialog for sale");
            std::string confirmText = fmt::format("Sell {} junk items for {} gold?", totalToSell, totalSellValue);
            ShowConfirmationMessageBox(confirmText.c_str(), {"Yes", "No"},
                [itemsToSell, gfxObjMap, vendorActorRef, vendorContainer, totalSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay, playerCarryWeight, menuMovie](unsigned int choice) {
                    if (choice == 0) {
                        SKSE::log::info("User confirmed sale");
                        ExecuteSell(itemsToSell, gfxObjMap, vendorActorRef, vendorContainer, totalSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay, playerCarryWeight);
                    } else {
                        SKSE::log::info("User cancelled sale");
                        if (menuMovie) menuMovie->SetVisible(true);
                    }
                    operationInProgress.store(false);
                });
        } else {
            SKSE::log::info("Confirmation disabled, proceeding with sale");
            ExecuteSell(itemsToSell, gfxObjMap, vendorActorRef, vendorContainer, totalSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay, playerCarryWeight);
            operationInProgress.store(false);
        }
        SKSE::log::info("==== Junk Sell Operation Complete ====");
        SKSE::log::info(" ");
    }

    void JunkHandler::ExecuteSell(std::vector<std::pair<InventoryEntryData*, Count>> itemsToSell, GFxObjMap gfxObjMap, TESObjectREFR* vendorActorRef, TESObjectREFR* vendorContainer, Count totalSellValue, Count totalToSell, Count totalPossibleToSell, float vendorGoldDisplay, float playerCarryWeight) {
        SKSE::log::info("---- Executing Junk Sale ----");
        auto player = RE::PlayerCharacter::GetSingleton();

        // WarnLargeInventory(player, vendorContainer);

        const auto ui = RE::UI::GetSingleton();

        if (Settings::GetNotifyOnJunkSell()) {
            DebugNotification("JunkIt - Processing Sale...");
        }

        TESObjectMISC* gold001 = Settings::GetGold001();
        Actor* vendorActor = vendorActorRef->As<Actor>();

        SKSE::log::info("Transferring {} gold from vendor to player...", totalSellValue);
        Count goldToGimme = totalSellValue;
        auto vendorInvCounts = vendorActorRef->GetInventoryCounts();
        auto vIt = vendorInvCounts.find(gold001);
        Count vendorActorGold = (vIt != vendorInvCounts.end()) ? vIt->second : 0;
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
            auto containerInvCounts = vendorContainer->GetInventoryCounts();
            auto cIt = containerInvCounts.find(gold001);
            Count containerGold = (cIt != containerInvCounts.end()) ? cIt->second : 0;
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

        Count totalVendorGoldLeft = static_cast<Count>(vendorGoldDisplay) - totalSellValue;
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
                player->RemoveItem(entryData->object, count, ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
                vendorContainer->AddObjectToContainer(entryData->object, nullptr, count, nullptr);
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

        if (player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight) != playerCarryWeight) {
            player->AsActorValueOwner()->SetActorValue(RE::ActorValue::kCarryWeight, playerCarryWeight);
        }

        std::vector<std::pair<TESBoundObject*, std::int32_t>> expectedInSource;
        std::vector<std::pair<TESBoundObject*, std::int32_t>> expectedInDest;
        
        auto playerInvCountsAfter = player->GetInventoryCounts();
        auto vendorInvCountsAfter = vendorContainer->GetInventoryCounts();
        
        for (const auto& [entryData, soldCount] : itemsToSell) {
            if (soldCount > 0 && entryData && entryData->object) {
                auto pIt = playerInvCountsAfter.find(entryData->object);
                Count expectedPlayerCount = (pIt != playerInvCountsAfter.end()) ? pIt->second : 0;
                expectedInSource.push_back({ entryData->object, expectedPlayerCount });
                
                auto vIt = vendorInvCountsAfter.find(entryData->object);
                Count expectedVendorCount = (vIt != vendorInvCountsAfter.end()) ? vIt->second : 0;
                expectedInDest.push_back({ entryData->object, expectedVendorCount });
            }
        }
        
        for (const auto& [entryData, soldCount] : itemsToSell) {
            auto it = gfxObjMap.find(entryData);
            if (it == gfxObjMap.end()) continue;
            RE::GFxValue& obj = it->second;
            RE::GFxValue currentFlag;
            obj.GetMember("filterFlag", &currentFlag);
            if (currentFlag.IsNumber()) {
                auto flag = static_cast<std::uint32_t>(currentFlag.GetNumber());
                obj.SetMember("filterFlag", RE::GFxValue(static_cast<double>((flag & 0x003FFu) << 10)));
            }
        }
        if (auto* movie = UIUtil::Menu::GetActiveMenuMovie()) {
            RE::GFxValue itemListObj;
            movie->GetVariable(&itemListObj, "_root.Menu_mc.inventoryLists.panelContainer.itemList");
            if (itemListObj.IsObject())
                itemListObj.Invoke("requestUpdate", nullptr, nullptr, 0);
        }

        std::thread([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
            auto* ti = SKSE::GetTaskInterface();
            if (ti)
                ti->AddUITask([]() { 
                    UIUtil::ItemList::Refresh(); 
                    if (auto* movie = UIUtil::Menu::GetActiveMenuMovie())
                        movie->SetVisible(true);
                });
        }).detach();
        
        // if (!expectedInSource.empty())
        //     ScheduleVerifyAndDelayedRefresh(player, std::move(expectedInSource), vendorContainer, std::move(expectedInDest));

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
                            junkManager.AddJunkItem(inventoryEntry);

                            ItemList* itemListMenu = UIUtil::ItemList::GetOpenList();
                            if (itemListMenu) {
                                itemListMenu->Update();
                            }

                            SKSE::log::info("Form: {} has been marked as junk", itemForm->GetName());
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
        
        if (isJunk) {
            SKSE::log::info("Removing junk status from {} [{}]", itemName, hexFormId);
            junkManager.RemoveJunkItem(inventoryEntry);
        } else {
            SKSE::log::info("Adding junk status to {} [{}]", itemName, hexFormId);
            junkManager.AddJunkItem(inventoryEntry);
        }

        bool isNowJunk = junkManager.IsJunk(inventoryEntry);

        itemListMenu->Update();

        if (isNowJunk) {
            SKSE::log::info("Form: {} has been marked as junk", itemForm->GetName());
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

    static bool HasTransferableExtraData(ExtraDataList* a_list)
    {
        bool hasTransferableData = false;
        for (const RE::BSExtraData& node : *a_list) {
            switch (node.GetType()) {
                case RE::ExtraDataType::kReferenceHandle:
                    return false;
                case RE::ExtraDataType::kHealth:
                case RE::ExtraDataType::kEnchantment:
                case RE::ExtraDataType::kCharge:
                case RE::ExtraDataType::kPoison:
                case RE::ExtraDataType::kOwnership:
                case RE::ExtraDataType::kWorn:
                case RE::ExtraDataType::kWornLeft:
                case RE::ExtraDataType::kUniqueID:
                case RE::ExtraDataType::kHotkey:
                case RE::ExtraDataType::kAliasInstanceArray:
                case RE::ExtraDataType::kSoul:
                case RE::ExtraDataType::kTextDisplayData:
                case RE::ExtraDataType::kFlags:
                case RE::ExtraDataType::kCount:
                    hasTransferableData = true;
                    break;
                default:
                    break;
            }
        }
        return hasTransferableData;
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
        Count remainingCount = a_count;

        const std::string itemName = a_item ? a_item->GetName() : "(null)";
        const std::string itemFormId = a_item ? FormUtil::Form::GetFormConfigString(a_item) : "(null)";
        const std::string fromName = a_fromContainer ? a_fromContainer->GetName() : "(null)";
        const std::string toName = a_toContainer ? a_toContainer->GetName() : "(null)";

        SKSE::log::info("  >> TransferItem: {} [{}] x{}", itemName, itemFormId, a_count);
        SKSE::log::info("       from: {} [{}]", fromName, a_fromContainer ? FormUtil::Form::GetFormConfigString(a_fromContainer) : "(null)");
        SKSE::log::info("       to:   {} [{}]", toName, a_toContainer ? FormUtil::Form::GetFormConfigString(a_toContainer) : "(null)");
        SKSE::log::info("       reason: {}", static_cast<int>(a_reason));

        if (a_invData) {
            SKSE::log::info("       invData: countDelta={} worn={} favorited={} enchanted={} poisoned={} quest={}",
                a_invData->countDelta,
                a_invData->IsWorn(),
                a_invData->IsFavorited(),
                a_invData->IsEnchanted(),
                a_invData->IsPoisoned(),
                a_invData->IsQuestObject()
            );
            if (a_invData->extraLists) {
                std::int32_t listIndex = 0;
                for (ExtraDataList* xList : *a_invData->extraLists) {
                    if (!xList) {
                        SKSE::log::info("       extraList[{}]: nullptr", listIndex);
                    } else {
                        bool hasData = xList->begin() != xList->end();
                        SKSE::log::info("       extraList[{}]: ptr=0x{:X} count={} hasData={}",
                            listIndex,
                            reinterpret_cast<uintptr_t>(xList),
                            xList->GetCount(),
                            hasData
                        );
                        if (hasData) {
                            auto extraTypeName = [](RE::ExtraDataType t) -> const char* {
                                switch (t) {
                                    case RE::ExtraDataType::kNone:                  return "kNone";
                                    case RE::ExtraDataType::kHavok:                 return "kHavok";
                                    case RE::ExtraDataType::kCell3D:                return "kCell3D";
                                    case RE::ExtraDataType::kCellWaterType:         return "kCellWaterType";
                                    case RE::ExtraDataType::kRegionList:            return "kRegionList";
                                    case RE::ExtraDataType::kSeenData:              return "kSeenData";
                                    case RE::ExtraDataType::kEditorID:              return "kEditorID";
                                    case RE::ExtraDataType::kCellMusicType:         return "kCellMusicType";
                                    case RE::ExtraDataType::kCellSkyRegion:         return "kCellSkyRegion";
                                    case RE::ExtraDataType::kProcessMiddleLow:      return "kProcessMiddleLow";
                                    case RE::ExtraDataType::kDetachTime:            return "kDetachTime";
                                    case RE::ExtraDataType::kPersistentCell:        return "kPersistentCell";
                                    case RE::ExtraDataType::kUnk0C:                 return "kUnk0C";
                                    case RE::ExtraDataType::kAction:                return "kAction";
                                    case RE::ExtraDataType::kStartingPosition:      return "kStartingPosition";
                                    case RE::ExtraDataType::kUnk0F:                 return "kUnk0F";
                                    case RE::ExtraDataType::kAnimGraphManager:      return "kAnimGraphManager";
                                    case RE::ExtraDataType::kBiped:                 return "kBiped";
                                    case RE::ExtraDataType::kUsedMarkers:           return "kUsedMarkers";
                                    case RE::ExtraDataType::kDistantData:           return "kDistantData";
                                    case RE::ExtraDataType::kRagDollData:           return "kRagDollData";
                                    case RE::ExtraDataType::kContainerChanges:      return "kContainerChanges";
                                    case RE::ExtraDataType::kWorn:                  return "kWorn";
                                    case RE::ExtraDataType::kWornLeft:              return "kWornLeft";
                                    case RE::ExtraDataType::kPackageStartLocation:  return "kPackageStartLocation";
                                    case RE::ExtraDataType::kPackage:               return "kPackage";
                                    case RE::ExtraDataType::kTresPassPackage:       return "kTresPassPackage";
                                    case RE::ExtraDataType::kRunOncePacks:          return "kRunOncePacks";
                                    case RE::ExtraDataType::kReferenceHandle:       return "kReferenceHandle";
                                    case RE::ExtraDataType::kFollower:              return "kFollower";
                                    case RE::ExtraDataType::kLevCreaModifier:       return "kLevCreaModifier";
                                    case RE::ExtraDataType::kGhost:                 return "kGhost";
                                    case RE::ExtraDataType::kOriginalReference:     return "kOriginalReference";
                                    case RE::ExtraDataType::kOwnership:             return "kOwnership";
                                    case RE::ExtraDataType::kGlobal:                return "kGlobal";
                                    case RE::ExtraDataType::kRank:                  return "kRank";
                                    case RE::ExtraDataType::kCount:                 return "kCount";
                                    case RE::ExtraDataType::kHealth:                return "kHealth";
                                    case RE::ExtraDataType::kUnk26:                 return "kUnk26";
                                    case RE::ExtraDataType::kTimeLeft:              return "kTimeLeft";
                                    case RE::ExtraDataType::kCharge:                return "kCharge";
                                    case RE::ExtraDataType::kLight:                 return "kLight";
                                    case RE::ExtraDataType::kLock:                  return "kLock";
                                    case RE::ExtraDataType::kTeleport:              return "kTeleport";
                                    case RE::ExtraDataType::kMapMarker:             return "kMapMarker";
                                    case RE::ExtraDataType::kLeveledCreature:       return "kLeveledCreature";
                                    case RE::ExtraDataType::kLeveledItem:           return "kLeveledItem";
                                    case RE::ExtraDataType::kScale:                 return "kScale";
                                    case RE::ExtraDataType::kMissingLinkedRefIDs:   return "kMissingLinkedRefIDs";
                                    case RE::ExtraDataType::kMagicCaster:           return "kMagicCaster";
                                    case RE::ExtraDataType::kNonActorMagicTarget:   return "kNonActorMagicTarget";
                                    case RE::ExtraDataType::kUnk33:                 return "kUnk33";
                                    case RE::ExtraDataType::kPlayerCrimeList:       return "kPlayerCrimeList";
                                    case RE::ExtraDataType::kUnk35:                 return "kUnk35";
                                    case RE::ExtraDataType::kEnableStateParent:     return "kEnableStateParent";
                                    case RE::ExtraDataType::kEnableStateChildren:   return "kEnableStateChildren";
                                    case RE::ExtraDataType::kItemDropper:           return "kItemDropper";
                                    case RE::ExtraDataType::kDroppedItemList:       return "kDroppedItemList";
                                    case RE::ExtraDataType::kRandomTeleportMarker:  return "kRandomTeleportMarker";
                                    case RE::ExtraDataType::kUnk3B:                 return "kUnk3B";
                                    case RE::ExtraDataType::kSavedHavokData:        return "kSavedHavokData";
                                    case RE::ExtraDataType::kCannotWear:            return "kCannotWear";
                                    case RE::ExtraDataType::kPoison:                return "kPoison";
                                    case RE::ExtraDataType::kMagicLight:            return "kMagicLight";
                                    case RE::ExtraDataType::kLastFinishedSequence:  return "kLastFinishedSequence";
                                    case RE::ExtraDataType::kSavedAnimation:        return "kSavedAnimation";
                                    case RE::ExtraDataType::kNorthRotation:         return "kNorthRotation";
                                    case RE::ExtraDataType::kSpawnContainer:        return "kSpawnContainer";
                                    case RE::ExtraDataType::kFriendHits:            return "kFriendHits";
                                    case RE::ExtraDataType::kHeadingTarget:         return "kHeadingTarget";
                                    case RE::ExtraDataType::kUnk46:                 return "kUnk46";
                                    case RE::ExtraDataType::kRefractionProperty:    return "kRefractionProperty";
                                    case RE::ExtraDataType::kStartingWorldOrCell:   return "kStartingWorldOrCell";
                                    case RE::ExtraDataType::kHotkey:                return "kHotkey";
                                    case RE::ExtraDataType::kEditorRef3DData:       return "kEditorRef3DData";
                                    case RE::ExtraDataType::kEditorRefMoveData:     return "kEditorRefMoveData";
                                    case RE::ExtraDataType::kInfoGeneralTopic:      return "kInfoGeneralTopic";
                                    case RE::ExtraDataType::kHasNoRumors:           return "kHasNoRumors";
                                    case RE::ExtraDataType::kSound:                 return "kSound";
                                    case RE::ExtraDataType::kTerminalState:         return "kTerminalState";
                                    case RE::ExtraDataType::kLinkedRef:             return "kLinkedRef";
                                    case RE::ExtraDataType::kLinkedRefChildren:     return "kLinkedRefChildren";
                                    case RE::ExtraDataType::kActivateRef:           return "kActivateRef";
                                    case RE::ExtraDataType::kActivateRefChildren:   return "kActivateRefChildren";
                                    case RE::ExtraDataType::kCanTalkToPlayer:       return "kCanTalkToPlayer";
                                    case RE::ExtraDataType::kObjectHealth:          return "kObjectHealth";
                                    case RE::ExtraDataType::kCellImageSpace:        return "kCellImageSpace";
                                    case RE::ExtraDataType::kNavMeshPortal:         return "kNavMeshPortal";
                                    case RE::ExtraDataType::kModelSwap:             return "kModelSwap";
                                    case RE::ExtraDataType::kRadius:                return "kRadius";
                                    case RE::ExtraDataType::kUnk5A:                 return "kUnk5A";
                                    case RE::ExtraDataType::kFactionChanges:        return "kFactionChanges";
                                    case RE::ExtraDataType::kDismemberedLimbs:      return "kDismemberedLimbs";
                                    case RE::ExtraDataType::kActorCause:            return "kActorCause";
                                    case RE::ExtraDataType::kMultiBound:            return "kMultiBound";
                                    case RE::ExtraDataType::kMultiBoundMarkerData:  return "kMultiBoundMarkerData";
                                    case RE::ExtraDataType::kMultiBoundRef:         return "kMultiBoundRef";
                                    case RE::ExtraDataType::kReflectedRefs:         return "kReflectedRefs";
                                    case RE::ExtraDataType::kReflectorRefs:         return "kReflectorRefs";
                                    case RE::ExtraDataType::kEmittanceSource:       return "kEmittanceSource";
                                    case RE::ExtraDataType::kUnk64:                 return "kUnk64";
                                    case RE::ExtraDataType::kCombatStyle:           return "kCombatStyle";
                                    case RE::ExtraDataType::kUnk66:                 return "kUnk66";
                                    case RE::ExtraDataType::kPrimitive:             return "kPrimitive";
                                    case RE::ExtraDataType::kOpenCloseActivateRef:  return "kOpenCloseActivateRef";
                                    case RE::ExtraDataType::kAnimNoteReceiver:      return "kAnimNoteReceiver";
                                    case RE::ExtraDataType::kAmmo:                  return "kAmmo";
                                    case RE::ExtraDataType::kPatrolRefData:         return "kPatrolRefData";
                                    case RE::ExtraDataType::kPackageData:           return "kPackageData";
                                    case RE::ExtraDataType::kOcclusionShape:        return "kOcclusionShape";
                                    case RE::ExtraDataType::kCollisionData:         return "kCollisionData";
                                    case RE::ExtraDataType::kSayTopicInfoOnceADay:  return "kSayTopicInfoOnceADay";
                                    case RE::ExtraDataType::kEncounterZone:         return "kEncounterZone";
                                    case RE::ExtraDataType::kSayTopicInfo:          return "kSayTopicInfo";
                                    case RE::ExtraDataType::kOcclusionPlaneRefData: return "kOcclusionPlaneRefData";
                                    case RE::ExtraDataType::kPortalRefData:         return "kPortalRefData";
                                    case RE::ExtraDataType::kPortal:                return "kPortal";
                                    case RE::ExtraDataType::kRoom:                  return "kRoom";
                                    case RE::ExtraDataType::kHealthPerc:            return "kHealthPerc";
                                    case RE::ExtraDataType::kRoomRefData:           return "kRoomRefData";
                                    case RE::ExtraDataType::kGuardedRefData:        return "kGuardedRefData";
                                    case RE::ExtraDataType::kCreatureAwakeSound:    return "kCreatureAwakeSound";
                                    case RE::ExtraDataType::kUnk7A:                 return "kUnk7A";
                                    case RE::ExtraDataType::kHorse:                 return "kHorse";
                                    case RE::ExtraDataType::kIgnoredBySandbox:      return "kIgnoredBySandbox";
                                    case RE::ExtraDataType::kCellAcousticSpace:     return "kCellAcousticSpace";
                                    case RE::ExtraDataType::kReservedMarkers:       return "kReservedMarkers";
                                    case RE::ExtraDataType::kWeaponIdleSound:       return "kWeaponIdleSound";
                                    case RE::ExtraDataType::kWaterLightRefs:        return "kWaterLightRefs";
                                    case RE::ExtraDataType::kLitWaterRefs:          return "kLitWaterRefs";
                                    case RE::ExtraDataType::kWeaponAttackSound:     return "kWeaponAttackSound";
                                    case RE::ExtraDataType::kActivateLoopSound:     return "kActivateLoopSound";
                                    case RE::ExtraDataType::kPatrolRefInUseData:    return "kPatrolRefInUseData";
                                    case RE::ExtraDataType::kAshPileRef:            return "kAshPileRef";
                                    case RE::ExtraDataType::kCreatureMovementSound: return "kCreatureMovementSound";
                                    case RE::ExtraDataType::kFollowerSwimBreadcrumbs: return "kFollowerSwimBreadcrumbs";
                                    case RE::ExtraDataType::kAliasInstanceArray:    return "kAliasInstanceArray";
                                    case RE::ExtraDataType::kLocation:              return "kLocation";
                                    case RE::ExtraDataType::kUnk8A:                 return "kUnk8A";
                                    case RE::ExtraDataType::kLocationRefType:       return "kLocationRefType";
                                    case RE::ExtraDataType::kPromotedRef:           return "kPromotedRef";
                                    case RE::ExtraDataType::kAnimationSequencer:    return "kAnimationSequencer";
                                    case RE::ExtraDataType::kOutfitItem:            return "kOutfitItem";
                                    case RE::ExtraDataType::kUnk8F:                 return "kUnk8F";
                                    case RE::ExtraDataType::kLeveledItemBase:       return "kLeveledItemBase";
                                    case RE::ExtraDataType::kLightData:             return "kLightData";
                                    case RE::ExtraDataType::kSceneData:             return "kSceneData";
                                    case RE::ExtraDataType::kBadPosition:           return "kBadPosition";
                                    case RE::ExtraDataType::kHeadTrackingWeight:    return "kHeadTrackingWeight";
                                    case RE::ExtraDataType::kFromAlias:             return "kFromAlias";
                                    case RE::ExtraDataType::kShouldWear:            return "kShouldWear";
                                    case RE::ExtraDataType::kFavorCost:             return "kFavorCost";
                                    case RE::ExtraDataType::kAttachedArrows3D:      return "kAttachedArrows3D";
                                    case RE::ExtraDataType::kTextDisplayData:       return "kTextDisplayData";
                                    case RE::ExtraDataType::kAlphaCutoff:           return "kAlphaCutoff";
                                    case RE::ExtraDataType::kEnchantment:           return "kEnchantment";
                                    case RE::ExtraDataType::kSoul:                  return "kSoul";
                                    case RE::ExtraDataType::kForcedTarget:          return "kForcedTarget";
                                    case RE::ExtraDataType::kUnk9E:                 return "kUnk9E";
                                    case RE::ExtraDataType::kUniqueID:              return "kUniqueID";
                                    case RE::ExtraDataType::kFlags:                 return "kFlags";
                                    case RE::ExtraDataType::kRefrPath:              return "kRefrPath";
                                    case RE::ExtraDataType::kDecalGroup:            return "kDecalGroup";
                                    case RE::ExtraDataType::kLockList:              return "kLockList";
                                    case RE::ExtraDataType::kForcedLandingMarker:   return "kForcedLandingMarker";
                                    case RE::ExtraDataType::kLargeRefOwnerCells:    return "kLargeRefOwnerCells";
                                    case RE::ExtraDataType::kCellWaterEnvMap:       return "kCellWaterEnvMap";
                                    case RE::ExtraDataType::kCellGrassData:         return "kCellGrassData";
                                    case RE::ExtraDataType::kTeleportName:          return "kTeleportName";
                                    case RE::ExtraDataType::kInteraction:           return "kInteraction";
                                    case RE::ExtraDataType::kWaterData:             return "kWaterData";
                                    case RE::ExtraDataType::kWaterCurrentZoneData:  return "kWaterCurrentZoneData";
                                    case RE::ExtraDataType::kAttachRef:             return "kAttachRef";
                                    case RE::ExtraDataType::kAttachRefChildren:     return "kAttachRefChildren";
                                    case RE::ExtraDataType::kGroupConstraint:       return "kGroupConstraint";
                                    case RE::ExtraDataType::kScriptedAnimDependence: return "kScriptedAnimDependence";
                                    case RE::ExtraDataType::kCachedScale:           return "kCachedScale";
                                    case RE::ExtraDataType::kRaceData:              return "kRaceData";
                                    case RE::ExtraDataType::kGIDBuffer:             return "kGIDBuffer";
                                    case RE::ExtraDataType::kMissingRefIDs:         return "kMissingRefIDs";
                                    case RE::ExtraDataType::kUnkB4:                 return "kUnkB4";
                                    case RE::ExtraDataType::kResourcesPreload:      return "kResourcesPreload";
                                    case RE::ExtraDataType::kUnkB6:                 return "kUnkB6";
                                    case RE::ExtraDataType::kUnkB7:                 return "kUnkB7";
                                    case RE::ExtraDataType::kUnkB8:                 return "kUnkB8";
                                    case RE::ExtraDataType::kUnkB9:                 return "kUnkB9";
                                    case RE::ExtraDataType::kUnkBA:                 return "kUnkBA";
                                    case RE::ExtraDataType::kUnkBB:                 return "kUnkBB";
                                    case RE::ExtraDataType::kUnkBC:                 return "kUnkBC";
                                    case RE::ExtraDataType::kUnkBD:                 return "kUnkBD";
                                    case RE::ExtraDataType::kUnkBE:                 return "kUnkBE";
                                    case RE::ExtraDataType::kUnkBF:                 return "kUnkBF";
                                    default:                                         return "kUnknown";
                                }
                            };

                            std::int32_t nodeIndex = 0;
                            for (const RE::BSExtraData& node : *xList) {
                                auto nodeType = node.GetType();
                                if (nodeType == RE::ExtraDataType::kReferenceHandle) {
                                    const auto* refHandle = static_cast<const RE::ExtraReferenceHandle*>(&node);
                                    RE::NiPointer<RE::TESObjectREFR> origRef = const_cast<RE::ExtraReferenceHandle*>(refHandle)->GetOriginalReference();
                                    SKSE::log::info("         node[{}]: {} (0x{:02X}) handle=0x{:X} origRef={}",
                                        nodeIndex,
                                        extraTypeName(nodeType),
                                        static_cast<uint32_t>(nodeType),
                                        refHandle->containerRef.native_handle(),
                                        origRef ? origRef->GetName() : "(null)"
                                    );
                                } else if (nodeType == RE::ExtraDataType::kCount) {
                                    const auto* extraCount = static_cast<const RE::ExtraCount*>(&node);
                                    SKSE::log::info("         node[{}]: {} (0x{:02X}) count={}",
                                        nodeIndex,
                                        extraTypeName(nodeType),
                                        static_cast<uint32_t>(nodeType),
                                        extraCount->count
                                    );
                                } else {
                                    SKSE::log::info("         node[{}]: {} (0x{:02X})",
                                        nodeIndex,
                                        extraTypeName(nodeType),
                                        static_cast<uint32_t>(nodeType)
                                    );
                                }
                                ++nodeIndex;
                            }
                        }
                    }
                    ++listIndex;
                }
            } else {
                SKSE::log::info("       extraLists: nullptr");
            }
        } else {
            SKSE::log::info("       invData: nullptr");
        }

        // Check if the invData is valid or not, if it's not just do a generic item move
        if (!a_invData || !a_invData->extraLists || a_invData->extraLists->empty()) {
            SKSE::log::info("     Moving {} {} [{}] without an ExtraDataList", 
                a_count, 
                itemName,
                itemFormId
            );
            a_fromContainer->RemoveItem(a_item, a_count, a_reason, nullptr, a_toContainer);
            return;
        }

        // Iterate through the ExtraDataLists and add each one to the receiving container
        std::for_each(a_invData->extraLists->begin(), a_invData->extraLists->end(), [&](ExtraDataList* dataList) {
            if (dataList == nullptr) {
                SKSE::log::error("     Ignoring null or invalid ExtraDataList on {} [{}]", 
                    itemName,
                    itemFormId
                );
                return -1;
            }

            if (!HasTransferableExtraData(dataList)) {
                SKSE::log::warn("     Skipping non-transferable ExtraDataList on {} [{}], deferring to fallback",
                    itemName,
                    itemFormId
                );
                return -1;
            }

            Count itemCount = dataList->GetCount();
            SKSE::log::info("     Moving {} {} [{}] with valid ExtraDataList (ptr=0x{:X})", 
                itemCount, 
                itemName,
                itemFormId,
                reinterpret_cast<uintptr_t>(dataList)
            );
            a_fromContainer->RemoveItem(a_item, itemCount, a_reason, dataList, a_toContainer);
            remainingCount -= itemCount;
            return 1;
        });

        // Get any items missed by the ExtraDataLists
        if (remainingCount > 0) {
            SKSE::log::info("     Moving remaining {} {} [{}] with no ExtraDataList", 
                remainingCount, 
                itemName,
                itemFormId
            );
            a_fromContainer->RemoveItem(a_item, remainingCount, a_reason, nullptr, a_toContainer);
        }
    }
}
