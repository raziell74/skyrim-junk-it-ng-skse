#include "junk.h"
#include "JunkData.h"
#include "SendUIMessage.h"

RE::MessageBoxData::~MessageBoxData() = default;

namespace JunkIt {

    std::atomic<bool> JunkHandler::operationInProgress{ false };

    bool JunkHandler::WarnLargeInventory(TESObjectREFR* a_container1, TESObjectREFR* a_container2) {
        std::int32_t count1 = a_container1->GetInventoryCount();
        std::int32_t count2 = a_container2->GetInventoryCount();
        std::int32_t totalCount = count1 + count2;

        SKSE::log::info("Large Inventory Check: Total Menu Form Count: {}", totalCount);

        if (totalCount >= Settings::GetWarnInventorySizeThreshold()) {
            SKSE::log::info("Large Container Inventory Detected!");
            if (Settings::GetNotifyLargeInventoryLag()) {
                RE::DebugMessageBox("Large Inventory detected, transfer could lag. Please allow for a few additional seconds for the transfer to complete.");
            }
            return true;
        }

        return false;
    }

    void JunkHandler::ShowConfirmationMessageBox(const char* bodyText, std::vector<std::string> buttons, std::function<void(unsigned int)> callback) {
        RE::GFxMovieView* underlyingMovie = nullptr;
        const auto ui = RE::UI::GetSingleton();
        if (ui) {
            auto containerMenu = ui->GetMenu<ContainerMenu>();
            if (containerMenu && containerMenu->uiMovie) {
                underlyingMovie = containerMenu->uiMovie.get();
            } else {
                auto barterMenu = ui->GetMenu<BarterMenu>();
                if (barterMenu && barterMenu->uiMovie)
                    underlyingMovie = barterMenu->uiMovie.get();
            }
        }

        if (underlyingMovie)
            underlyingMovie->SetVisible(false);

        auto wrappedCallback = [originalCallback = std::move(callback), underlyingMovie](unsigned int choice) {
            if (originalCallback)
                originalCallback(choice);
            if (underlyingMovie)
                underlyingMovie->SetVisible(true);
        };

        auto messageBoxData = new RE::MessageBoxData();
        messageBoxData->bodyText = bodyText;
        for (const auto& button : buttons) {
            messageBoxData->buttonText.push_back(button.c_str());
        }
        messageBoxData->optionIndexOffset = 4;
        messageBoxData->type = 10;
        messageBoxData->menuDepth = 10;
        messageBoxData->callback = RE::BSTSmartPointer<RE::IMessageBoxCallback>(new JunkItMessageBoxCallback(std::move(wrappedCallback), messageBoxData->optionIndexOffset));
        messageBoxData->QueueMessage();
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

    std::vector<InventoryEntryData*> JunkHandler::BuildSellList() {
        SKSE::log::info(" ");
        SKSE::log::info("---- Finding Sellable Junk ----");

        std::vector<InventoryEntryData*> sellList;

        const auto ui = RE::UI::GetSingleton();
        GPtr<BarterMenu> barterMenu = ui ? ui->GetMenu<BarterMenu>() : nullptr;
        ItemList* itemListMenu = barterMenu ? barterMenu->GetRuntimeData().itemList : nullptr;
        if (!itemListMenu) {
            SKSE::log::error("No ItemListMenu found");
            return sellList;
        }

        BSTArray<ItemList::Item*> listItems = itemListMenu->items;
        std::vector<InventoryEntryData*> sortFormData;

        SKSE::log::info("Processing Entry List for sellable junk items");
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
            if (Settings::ProtectEnchanted() && entryItem->data.objDesc->IsEnchanted()) {
                SKSE::log::info("Junk Item Enchanted - Skipping {}", entryItem->data.objDesc->object->GetName());
                continue;
            }

            sortFormData.push_back(entryItem->data.objDesc);
        }

        auto priority = Settings::GetSellPriority();
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

        SKSE::log::info("Finalized SellList:");
        for (InventoryEntryData* entryData : sortFormData) {
            const TESBoundObject* entryObject = entryData->object;
            if (!entryObject) continue;
            sellList.push_back(entryData);
            SKSE::log::info("     {} [{}]", entryObject->GetName(), FormUtil::Form::GetFormConfigString(entryData->object->As<TESForm>()));
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
                    [transferList, transferContainer, containerMode, menuView, playerCarryWeight](unsigned int choice) {
                        if (choice == 0) {
                            SKSE::log::info("User confirmed retrieval");
                            ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                            auto p = RE::PlayerCharacter::GetSingleton();
                            if (p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight) != playerCarryWeight) {
                                p->AsActorValueOwner()->SetActorValue(RE::ActorValue::kCarryWeight, playerCarryWeight);
                            }
                        } else {
                            SKSE::log::info("User cancelled retrieval");
                        }
                        operationInProgress.store(false);
                    });
            } else {
                SKSE::log::info("Confirmation disabled, proceeding with retrieval");
                ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
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
                operationInProgress.store(false);
                return;
            }

            if (Settings::ConfirmTransfer()) {
                SKSE::log::info("Showing confirmation dialog for transfer");
                ShowConfirmationMessageBox("Transfer all junk items to this container?", {"Yes", "No"},
                    [transferList, transferContainer, containerMode, menuView, playerCarryWeight](unsigned int choice) {
                        if (choice == 0) {
                            SKSE::log::info("User confirmed transfer");
                            ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                            auto p = RE::PlayerCharacter::GetSingleton();
                            if (p->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight) != playerCarryWeight) {
                                p->AsActorValueOwner()->SetActorValue(RE::ActorValue::kCarryWeight, playerCarryWeight);
                            }
                        } else {
                            SKSE::log::info("User cancelled transfer");
                        }
                        operationInProgress.store(false);
                    });
            } else {
                SKSE::log::info("Confirmation disabled, proceeding with transfer");
                ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                if (player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight) != playerCarryWeight) {
                    player->AsActorValueOwner()->SetActorValue(RE::ActorValue::kCarryWeight, playerCarryWeight);
                }
                operationInProgress.store(false);
            }
        }
        SKSE::log::info("==== Junk Transfer Operation Complete ====");
        SKSE::log::info(" ");
    }

    void JunkHandler::ExecuteTransfer(std::vector<InventoryEntryData*> transferList, TESObjectREFR* transferContainer, ContainerMenu::ContainerMode containerMode, int menuView) {
        SKSE::log::info("---- Executing Junk Transfer ----");
        auto player = RE::PlayerCharacter::GetSingleton();

        WarnLargeInventory(player, transferContainer);

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
                auto playerInvCounts = player->GetInventoryCounts();
                for (auto* entryData : transferList) {
                    if (!entryData || !entryData->object) continue;
                    
                    auto pIt = playerInvCounts.find(entryData->object);
                    Count itemCount = (pIt != playerInvCounts.end()) ? pIt->second : 0;
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

        RE::SendUIMessage::SendInventoryUpdateMessage(player, nullptr);

        UIUtil::ItemList::Refresh();

        std::vector<std::pair<TESBoundObject*, std::int32_t>> expectedInSource;
        TESObjectREFR* verifySourceRef = nullptr;
        if (menuView == 0) {
            verifySourceRef = transferContainer;
            for (auto* entryData : transferList) {
                if (!entryData || !entryData->object) continue;
                expectedInSource.push_back({ entryData->object, 0 });
            }
        } else if (containerMode == ContainerMenu::ContainerMode::kNPCMode) {
            verifySourceRef = player;
            auto playerInvCounts = player->GetInventoryCounts();
            for (auto* entryData : transferList) {
                if (!entryData || !entryData->object) continue;
                auto it = playerInvCounts.find(entryData->object);
                Count remaining = (it != playerInvCounts.end()) ? it->second : 0;
                expectedInSource.push_back({ entryData->object, remaining });
            }
        } else {
            verifySourceRef = player;
            for (auto* entryData : transferList) {
                if (!entryData || !entryData->object) continue;
                expectedInSource.push_back({ entryData->object, 0 });
            }
        }
        if (verifySourceRef && !expectedInSource.empty())
            ScheduleVerifyAndDelayedRefresh(verifySourceRef, std::move(expectedInSource));
    }

    namespace {
        void VerifyAndDelayedRefreshImpl(TESObjectREFR* sourceRef, std::vector<std::pair<TESBoundObject*, std::int32_t>> expectedCountsInSource, int attempt) {
            constexpr int maxAttempts = 30;
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
            if (verified || attempt >= maxAttempts) {
                UIUtil::ItemList::Refresh();
                return;
            }
            auto* ti = SKSE::GetTaskInterface();
            if (ti)
                ti->AddUITask([sourceRef, expectedCountsInSource, attempt]() { VerifyAndDelayedRefreshImpl(sourceRef, expectedCountsInSource, attempt + 1); });
            else
                UIUtil::ItemList::Refresh();
        }
    }

    void JunkHandler::ScheduleVerifyAndDelayedRefresh(TESObjectREFR* sourceRef, std::vector<std::pair<TESBoundObject*, std::int32_t>> expectedCountsInSource) {
        auto* ti = SKSE::GetTaskInterface();
        if (ti)
            ti->AddUITask([sourceRef, expectedCountsInSource]() { VerifyAndDelayedRefreshImpl(sourceRef, expectedCountsInSource, 0); });
        else
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
        float playerCarryWeight = player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight);

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

        SKSE::log::info("Vendor Gold: {}", vendorGoldDisplay);
        SKSE::log::info("Vendor Sell Mult: {}", sellMult);

        Count totalToSell = 0;
        Count totalPossibleToSell = 0;
        float calculatedVendorGold = vendorGoldDisplay;
        float totalSellValue = 0;

        std::vector<std::pair<InventoryEntryData*, Count>> itemsToSell;

        auto playerInvCounts = player->GetInventoryCounts();
        for (auto* entryData : sellList) {
            if (!entryData || !entryData->object) continue;

            auto pIt = playerInvCounts.find(entryData->object);
            Count iCount = (pIt != playerInvCounts.end()) ? pIt->second : 0;
            totalPossibleToSell += iCount;

            SKSE::log::info("Calculating Sell Item: {} -- player has {} of this item", entryData->object->GetName(), iCount);

            if (iCount > 0) {
                float itemGoldValue = static_cast<float>(entryData->GetValue());
                float sellValue = itemGoldValue * sellMult;
                float goldDifferential = calculatedVendorGold - (sellValue * iCount);

                while (RoundNumber(goldDifferential) <= 0 && iCount > 0) {
                    iCount -= 1;
                    goldDifferential = calculatedVendorGold - (sellValue * iCount);
                }

                if (iCount > 0) {
                    calculatedVendorGold -= sellValue * iCount;
                    totalSellValue += sellValue * iCount;
                    totalToSell += iCount;
                    itemsToSell.push_back({entryData, iCount});
                    SKSE::log::info("Sell {} {} for {} gold", iCount, entryData->object->GetName(), sellValue * iCount);
                }
            }
        }

        if (totalToSell <= 0) {
            SKSE::log::info("Vendor cannot afford to buy any junk! Vendor Gold: {}, Required: {}", vendorGoldDisplay, totalSellValue);
            RE::DebugMessageBox("Vendor cannot afford to buy any junk!");
            operationInProgress.store(false);
            return;
        }

        SKSE::log::info("Sale Summary: Selling {} items for {} gold (Vendor will have {} gold remaining)", totalToSell, RoundNumber(totalSellValue), RoundNumber(calculatedVendorGold));

        if (Settings::ConfirmSell()) {
            SKSE::log::info("Showing confirmation dialog for sale");
            std::string confirmText = fmt::format("Sell {} junk items for {} gold?", totalToSell, RoundNumber(totalSellValue));
            ShowConfirmationMessageBox(confirmText.c_str(), {"Yes", "No"},
                [itemsToSell, vendorActorRef, vendorContainer, totalSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay, playerCarryWeight](unsigned int choice) {
                    if (choice == 0) {
                        SKSE::log::info("User confirmed sale");
                        ExecuteSell(itemsToSell, vendorActorRef, vendorContainer, totalSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay, playerCarryWeight);
                    } else {
                        SKSE::log::info("User cancelled sale");
                    }
                    operationInProgress.store(false);
                });
        } else {
            SKSE::log::info("Confirmation disabled, proceeding with sale");
            ExecuteSell(itemsToSell, vendorActorRef, vendorContainer, totalSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay, playerCarryWeight);
            operationInProgress.store(false);
        }
        SKSE::log::info("==== Junk Sell Operation Complete ====");
        SKSE::log::info(" ");
    }

    void JunkHandler::ExecuteSell(std::vector<std::pair<InventoryEntryData*, Count>> itemsToSell, TESObjectREFR* vendorActorRef, TESObjectREFR* vendorContainer, float totalSellValue, Count totalToSell, Count totalPossibleToSell, float vendorGoldDisplay, float playerCarryWeight) {
        SKSE::log::info("---- Executing Junk Sale ----");
        auto player = RE::PlayerCharacter::GetSingleton();

        WarnLargeInventory(player, vendorContainer);

        if (Settings::GetNotifyOnJunkSell()) {
            DebugNotification("JunkIt - Processing Sale...");
        }

        TESObjectMISC* gold001 = Settings::GetGold001();
        Actor* vendorActor = vendorActorRef->As<Actor>();

        SKSE::log::info("Transferring {} gold from vendor to player...", RoundNumber(totalSellValue));
        Count goldToGimme = RoundNumber(totalSellValue);
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

        Count totalVendorGoldLeft = RoundNumber(vendorGoldDisplay - totalSellValue);
        if (totalVendorGoldLeft < 0) totalVendorGoldLeft = 0;

        const auto ui = RE::UI::GetSingleton();
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
                TransferItem(entryData->object, player, vendorContainer, ITEM_REMOVE_REASON::kSelling, count, entryData);
                SKSE::log::info("Transaction for {} {} complete", count, entryData->object->GetName());
            }
        }

        SKSE::log::info("Adding {} Speech experience", totalSellValue);
        player->AddSkillExperience(RE::ActorValue::kSpeech, totalSellValue);

        if (totalToSell >= totalPossibleToSell) {
            SKSE::log::info("Sold ALL {} Junk Items for {} Gold", totalToSell, RoundNumber(totalSellValue));
            if (Settings::GetNotifyOnJunkSell()) {
                DebugNotification("JunkIt - Sold All Junk Items!");
            }
        } else {
            SKSE::log::info("Sold {} of {} Junk Items for {} Gold (vendor gold limit reached)", totalToSell, totalPossibleToSell, RoundNumber(totalSellValue));
            if (Settings::GetNotifyOnJunkSell()) {
                std::string msg = fmt::format("JunkIt - Sold {} Junk Items!", totalToSell);
                DebugNotification(msg.c_str());
            }
        }

        if (player->AsActorValueOwner()->GetActorValue(RE::ActorValue::kCarryWeight) != playerCarryWeight) {
            player->AsActorValueOwner()->SetActorValue(RE::ActorValue::kCarryWeight, playerCarryWeight);
        }

        RE::SendUIMessage::SendInventoryUpdateMessage(player, nullptr);

        UIUtil::ItemList::Refresh();

        std::vector<std::pair<TESBoundObject*, std::int32_t>> expectedInPlayer;
        for (const auto& [entryData, count] : itemsToSell) {
            if (entryData && entryData->object && count > 0)
                expectedInPlayer.push_back({ entryData->object, 0 });
        }
        if (!expectedInPlayer.empty())
            ScheduleVerifyAndDelayedRefresh(player, std::move(expectedInPlayer));

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

        if (itemForm->GetFormType() == FormType::Light) {
            DebugNotification("JunkIt - Lights cannot be marked as Junk");
            return nullptr;
        }

        if (inventoryEntry->IsQuestObject()) {
            SKSE::log::info("Cannot mark quest item {} [{}] as junk", itemName, hexFormId);
            auto& junkManager = JunkDataManager::GetSingleton();
            if (!junkManager.IsJunk(inventoryEntry)) {
                DebugNotification("JunkIt - Quest Items cannot be marked as Junk");
                return nullptr;
            }
        }

        if (Settings::ProtectEquipped() && inventoryEntry->IsWorn()) {
            SKSE::log::info("Cannot mark equipped item {} [{}] as junk", itemName, hexFormId);
            auto& junkManager = JunkDataManager::GetSingleton();
            if (!junkManager.IsJunk(inventoryEntry)) {
                DebugNotification("JunkIt - Equipped Items are protected and cannot be marked as Junk");
                return nullptr;
            }
        }

        if (Settings::ProtectFavorites() && inventoryEntry->IsFavorited()) {
            SKSE::log::info("Cannot mark favorited item {} [{}] as junk", itemName, hexFormId);
            auto& junkManager = JunkDataManager::GetSingleton();
            if (!junkManager.IsJunk(inventoryEntry)) {
                DebugNotification("JunkIt - Favorited Items are protected and cannot be marked as Junk");
                return nullptr;
            }
        }

        auto& junkManager = JunkDataManager::GetSingleton();
        bool isJunk = junkManager.IsJunk(inventoryEntry);
        
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

        // Check if the invData is valid or not, if it's not just do a generic item move
        if (!a_invData || !a_invData->extraLists || a_invData->extraLists->empty()) {
            SKSE::log::info("     Moving {} {} [{}] without an ExtraDataList", 
                a_count, 
                a_item->GetName(),
                FormUtil::Form::GetFormConfigString(a_item)
            );
            a_fromContainer->RemoveItem(a_item, a_count, a_reason, nullptr, a_toContainer);
            return;
        }

        // Iterate through the ExtraDataLists and add each one to the receiving container
        std::for_each(a_invData->extraLists->begin(), a_invData->extraLists->end(), [&](ExtraDataList* dataList) {
            if (dataList == nullptr) {
                SKSE::log::error("     Ignoring null or invalid ExtraDataList on {} [{}]", 
                    a_item->GetName(),
                    FormUtil::Form::GetFormConfigString(a_item)
                );
                return -1;
            }
    
            Count itemCount = dataList->GetCount();
            SKSE::log::info("     Moving {} {} [{}] with valid ExtraDataList", 
                itemCount, 
                a_item->GetName(),
                FormUtil::Form::GetFormConfigString(a_item)
            );
            a_fromContainer->RemoveItem(a_item, itemCount, a_reason, dataList, a_toContainer);
            remainingCount -= itemCount;
            return 1;
        });

        // Get any items missed by the ExtraDataLists
        if (remainingCount > 0) {
            SKSE::log::info("     Moving remaining {} {} [{}] with no ExtraDataList", 
                remainingCount, 
                a_item->GetName(),
                FormUtil::Form::GetFormConfigString(a_item)
            );
            a_fromContainer->RemoveItem(a_item, remainingCount, a_reason, nullptr, a_toContainer);
        }
    }
}
