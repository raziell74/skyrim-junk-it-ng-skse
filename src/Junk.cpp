#include "junk.h"

namespace JunkIt {

    std::atomic<bool> JunkHandler::operationInProgress{ false };

    void JunkHandler::UpdateItemKeywords() {
        BGSKeyword* isJunkKYWD = Settings::GetIsJunkKYWD();

        BGSListForm* junkList = Settings::GetJunkList();
        junkList->ForEachForm([&](TESForm& form) {
            BGSKeywordForm* keywordForm = nullptr;

            if (form.GetFormType() == FormType::Ammo) {
                TESAmmo* ammo = form.As<TESAmmo>();
                keywordForm = ammo->AsKeywordForm();
            } else {
                keywordForm = form.As<BGSKeywordForm>();
            }

            if (keywordForm && !keywordForm->HasKeyword(isJunkKYWD)) {
                keywordForm->AddKeyword(isJunkKYWD);
            }

            return BSContainer::ForEachResult::kContinue;
        });

        BGSListForm* unjunkedList = Settings::GetUnjunkedList();
        unjunkedList->ForEachForm([&](TESForm& form) {
            BGSKeywordForm* keywordForm = nullptr;

            if (form.GetFormType() == FormType::Ammo) {
                TESAmmo* ammo = form.As<TESAmmo>();
                keywordForm = ammo->AsKeywordForm();
            } else {
                keywordForm = form.As<BGSKeywordForm>();
            }

            if (keywordForm && keywordForm->HasKeyword(isJunkKYWD)) {
                keywordForm->RemoveKeyword(isJunkKYWD);
            }

            return BSContainer::ForEachResult::kContinue;
        });
    }

    bool JunkHandler::WarnLargeInventory(TESObjectREFR* a_container1, TESObjectREFR* a_container2) {
        std::int32_t count1 = static_cast<std::int32_t>(a_container1->GetContainerForms().size());
        std::int32_t count2 = static_cast<std::int32_t>(a_container2->GetContainerForms().size());
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
        auto messageBoxData = new RE::MessageBoxData();
        messageBoxData->bodyText = bodyText;
        for (const auto& button : buttons) {
            messageBoxData->buttonText.push_back(button.c_str());
        }
        messageBoxData->callback = RE::BSTSmartPointer<RE::IMessageBoxCallback>(new JunkItMessageBoxCallback(std::move(callback)));
        messageBoxData->unk4C = 4;   // Controls message box behavior flags; 4 = standard interactive dialog
        messageBoxData->unk38 = 10;  // Message box priority/queue ordering; 10 = foreground priority
        messageBoxData->QueueMessage();
    }

    BGSListForm* JunkHandler::BuildTransferFormList() {
        SKSE::log::info(" ");
        SKSE::log::info("---- Finding Transferrable Junk ----");

        BGSListForm* junkList = Settings::GetJunkList();
        BGSListForm* transferList = Settings::GetTransferList();
        transferList->ClearData();

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
        for (std::uint32_t i = 0, size = listItems.size(); i < size; i++) {
            ItemList::Item* entryItem = listItems[i];
            if (!entryItem) continue;

            if (!junkList->HasForm(entryItem->data.objDesc->GetObject())) continue;

            if (Settings::ProtectEquipped() && entryItem->data.objDesc->IsWorn()) {
                SKSE::log::info("Junk Item Equipped - Skipping {}", entryItem->data.objDesc->GetObject()->GetName());
                continue;
            }
            if (Settings::ProtectFavorites() && entryItem->data.objDesc->IsFavorited()) {
                SKSE::log::info("Junk Item Favorited - Skipping {}", entryItem->data.objDesc->GetObject()->GetName());
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
        for (const InventoryEntryData* entryData : sortFormData) {
            const TESBoundObject* entryObject = entryData->GetObject();
            if (!entryObject) continue;
            if (!transferList->HasForm(entryObject->GetFormID())) {
                TESForm* itemForm = entryData->object->As<TESForm>();
                transferList->AddForm(itemForm);
                SKSE::log::info("     {} [{}]", entryObject->GetName(), FormUtil::Form::GetFormConfigString(itemForm));
            }
        }

        SKSE::log::info("---- Completed Junk Transfer List Generation ----");
        SKSE::log::info(" ");
        return transferList;
    }

    BGSListForm* JunkHandler::BuildSellFormList() {
        SKSE::log::info(" ");
        SKSE::log::info("---- Finding Sellable Junk ----");

        BGSListForm* junkList = Settings::GetJunkList();
        BGSListForm* sellList = Settings::GetSellList();
        sellList->ClearData();

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
        for (std::uint32_t i = 0, size = listItems.size(); i < size; i++) {
            ItemList::Item* entryItem = listItems[i];
            if (!entryItem) continue;

            if (!junkList->HasForm(entryItem->data.objDesc->GetObject())) continue;

            if (Settings::ProtectEquipped() && entryItem->data.objDesc->IsWorn()) {
                SKSE::log::info("Junk Item Equipped - Skipping {}", entryItem->data.objDesc->GetObject()->GetName());
                continue;
            }
            if (Settings::ProtectFavorites() && entryItem->data.objDesc->IsFavorited()) {
                SKSE::log::info("Junk Item Favorited - Skipping {}", entryItem->data.objDesc->GetObject()->GetName());
                continue;
            }
            if (Settings::ProtectEnchanted() && entryItem->data.objDesc->IsEnchanted()) {
                SKSE::log::info("Junk Item Enchanted - Skipping {}", entryItem->data.objDesc->GetObject()->GetName());
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
        for (const InventoryEntryData* entryData : sortFormData) {
            const TESBoundObject* entryObject = entryData->GetObject();
            if (!entryObject) continue;
            if (!sellList->HasForm(entryObject->GetFormID())) {
                TESForm* itemForm = entryData->object->As<TESForm>();
                sellList->AddForm(itemForm);
                SKSE::log::info("     {} [{}]", entryObject->GetName(), FormUtil::Form::GetFormConfigString(itemForm));
            }
        }

        SKSE::log::info("---- Generated Junk Sell FormList ----");
        SKSE::log::info(" ");
        return sellList;
    }

    void JunkHandler::ToggleIsJunk() {
        TESForm* itemForm = ToggleSelectedItemKeyword();
        if (!itemForm) return;

        BGSKeyword* isJunkKYWD = Settings::GetIsJunkKYWD();
        BGSListForm* junkList = Settings::GetJunkList();
        BGSListForm* junkHistory = Settings::GetJunkHistory();
        BGSListForm* unjunkedList = Settings::GetUnjunkedList();

        BGSKeywordForm* keywordForm = nullptr;
        if (itemForm->GetFormType() == FormType::Ammo) {
            keywordForm = itemForm->As<TESAmmo>()->AsKeywordForm();
        } else {
            keywordForm = itemForm->As<BGSKeywordForm>();
        }

        if (!keywordForm) return;

        if (keywordForm->HasKeyword(isJunkKYWD)) {
            SKSE::log::info("Form: {} has been marked as junk", itemForm->GetName());
            if (Settings::GetNotifyOnMarkUnmark()) {
                std::string msg = fmt::format("JunkIt - {} has been marked as junk", itemForm->GetName());
                DebugNotification(msg.c_str());
            }

            if (!junkList->HasForm(itemForm)) {
                junkList->AddForm(itemForm);
                if (junkHistory) {
                    junkHistory->AddForm(itemForm);
                }
            }

            if (unjunkedList->HasForm(itemForm)) {
                unjunkedList->RemoveAddedForm(itemForm);
            }
        } else {
            SKSE::log::info("Form: {} is no longer marked as junk", itemForm->GetName());
            if (Settings::GetNotifyOnMarkUnmark()) {
                std::string msg = fmt::format("JunkIt - {} is no longer marked as junk", itemForm->GetName());
                DebugNotification(msg.c_str());
            }

            if (junkList->HasForm(itemForm)) {
                junkList->RemoveAddedForm(itemForm);
            }

            if (!unjunkedList->HasForm(itemForm)) {
                unjunkedList->AddForm(itemForm);
            }
        }
    }

    void JunkHandler::TransferJunk() {
        bool expected = false;
        if (!operationInProgress.compare_exchange_strong(expected, true)) {
            SKSE::log::info("TransferJunk blocked: another operation is already in progress");
            return;
        }

        BGSListForm* junkList = Settings::GetJunkList();
        if (!junkList || junkList->forms.size() <= 0) {
            operationInProgress.store(false);
            return;
        }

        auto player = RE::PlayerCharacter::GetSingleton();
        float playerCarryWeight = player->GetActorValue(RE::ActorValue::kCarryWeight);

        TESObjectREFR* transferContainer = GetContainerMenuContainer();
        if (!transferContainer) {
            operationInProgress.store(false);
            return;
        }

        auto containerMode = GetContainerMode();

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

        BGSListForm* transferList = BuildTransferFormList();

        if (menuView == 0) {
            if (GetContainerItemListCount(transferContainer, transferList) <= 0) {
                SKSE::log::info("No Junk to retrieve!");
                RE::DebugMessageBox("No Junk to take!");
                operationInProgress.store(false);
                return;
            }

            if (Settings::ConfirmTransfer()) {
                ShowConfirmationMessageBox("Retrieve all junk items from this container?", {"Yes", "No"},
                    [transferList, transferContainer, containerMode, menuView, playerCarryWeight](unsigned int choice) {
                        if (choice == 0) {
                            ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                            auto p = RE::PlayerCharacter::GetSingleton();
                            if (p->GetActorValue(RE::ActorValue::kCarryWeight) != playerCarryWeight) {
                                p->SetActorValue(RE::ActorValue::kCarryWeight, playerCarryWeight);
                            }
                        }
                        operationInProgress.store(false);
                    });
            } else {
                ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                if (player->GetActorValue(RE::ActorValue::kCarryWeight) != playerCarryWeight) {
                    player->SetActorValue(RE::ActorValue::kCarryWeight, playerCarryWeight);
                }
                operationInProgress.store(false);
            }
        } else {
            if (GetContainerItemListCount(player, transferList) <= 0) {
                SKSE::log::info("No Junk to transfer!");
                RE::DebugMessageBox("No Junk to transfer!");
                operationInProgress.store(false);
                return;
            }

            if (Settings::ConfirmTransfer()) {
                ShowConfirmationMessageBox("Transfer all junk items to this container?", {"Yes", "No"},
                    [transferList, transferContainer, containerMode, menuView, playerCarryWeight](unsigned int choice) {
                        if (choice == 0) {
                            ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                            auto p = RE::PlayerCharacter::GetSingleton();
                            if (p->GetActorValue(RE::ActorValue::kCarryWeight) != playerCarryWeight) {
                                p->SetActorValue(RE::ActorValue::kCarryWeight, playerCarryWeight);
                            }
                        }
                        operationInProgress.store(false);
                    });
            } else {
                ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                if (player->GetActorValue(RE::ActorValue::kCarryWeight) != playerCarryWeight) {
                    player->SetActorValue(RE::ActorValue::kCarryWeight, playerCarryWeight);
                }
                operationInProgress.store(false);
            }
        }
    }

    void JunkHandler::ExecuteTransfer(BGSListForm* transferList, TESObjectREFR* transferContainer, ContainerMenu::ContainerMode containerMode, int menuView) {
        auto player = RE::PlayerCharacter::GetSingleton();

        WarnLargeInventory(player, transferContainer);

        if (menuView == 0) {
            if (Settings::GetNotifyOnJunkTransfer()) {
                DebugNotification("JunkIt - Processing Retrieval...");
            }

            Count iRetrievedCount = ProcessItemListTransfer(transferList, transferContainer, player, 0);

            SKSE::log::info("Junk Retrieved!");
            if (Settings::GetNotifyOnJunkTransfer()) {
                std::string msg = fmt::format("JunkIt - {} Junk Items Retrieved!", iRetrievedCount);
                DebugNotification(msg.c_str());
            }

            if (Settings::GetAggressiveRefresh()) {
                UIUtil::ItemList::Refresh();
            }
            return;
        }

        if (Settings::GetNotifyOnJunkTransfer()) {
            DebugNotification("JunkIt - Processing Transfer...");
        }

        if (containerMode == ContainerMenu::ContainerMode::kNPCMode) {
            Actor* transferActor = transferContainer->As<Actor>();
            float maxWeight = transferActor->GetActorValue(RE::ActorValue::kCarryWeight);
            float currentWeight = transferContainer->GetTotalItemWeight();
            SKSE::log::info("[NPC Mode] CarryWeight {}/{}", currentWeight, maxWeight);

            BSTArray<TESForm*> transferForms = transferList->forms;
            Count iTotal = transferForms.size();
            Count totalTransferred = 0;
            Count totalPossibleTransferred = 0;

            BGSListForm* transferAllList = transferList;

            for (Count iCurrent = 0; iCurrent < iTotal; iCurrent++) {
                TESForm* item = transferForms[iCurrent];
                if (!item) continue;

                Count iCount = GetContainerSingleItemCount(player, item);
                Count iTotalCount = iCount;
                totalPossibleTransferred += iCount;

                if (iCount > 0) {
                    float itemWeight = item->GetWeight();
                    float currentWeightWithItems = (itemWeight * iCount) + currentWeight;

                    while (currentWeightWithItems > maxWeight && iCount > 0) {
                        iCount -= 1;
                        currentWeightWithItems = (itemWeight * iCount) + currentWeight;
                    }

                    if (iCount > 0 && iCount < iTotalCount) {
                        player->RemoveItem(item->As<TESBoundObject>(), iCount, ITEM_REMOVE_REASON::kStoreInTeammate, nullptr, transferContainer);
                        currentWeight += (itemWeight * iCount);
                        totalTransferred += iCount;
                        transferAllList->RemoveAddedForm(item);
                        SKSE::log::info("Transferred limited quantity {} {} [{}/{}]", iCount, item->GetName(), RoundNumber(currentWeight), RoundNumber(maxWeight));
                    } else if (iCount <= 0) {
                        transferAllList->RemoveAddedForm(item);
                    } else {
                        totalTransferred += iCount;
                        currentWeight += (itemWeight * iCount);
                        SKSE::log::info("Listing {} {} for full quantity transfer [{}/{}]", iCount, item->GetName(), RoundNumber(currentWeight), RoundNumber(maxWeight));
                    }
                }
            }

            ProcessItemListTransfer(transferAllList, player, transferContainer, 0);

            if (totalTransferred == 0) {
                SKSE::log::info("[NPC Mode] NPC cannot carry any more junk");
                RE::DebugMessageBox("This person cannot carry any more");
            } else if (Settings::GetNotifyOnJunkTransfer()) {
                if (totalTransferred >= totalPossibleTransferred) {
                    std::string msg = fmt::format("JunkIt - Transferred All {} Junk Items!", totalTransferred);
                    DebugNotification(msg.c_str());
                } else {
                    std::string msg = fmt::format("JunkIt - Transferred {} Junk Items!", totalTransferred);
                    DebugNotification(msg.c_str());
                }
            }
        } else {
            Count iTransferredCount = ProcessItemListTransfer(transferList, player, transferContainer, 0);

            if (Settings::GetNotifyOnJunkTransfer()) {
                std::string msg = fmt::format("JunkIt - Transferred {} Junk Items!", iTransferredCount);
                DebugNotification(msg.c_str());
            }
        }

        if (Settings::GetAggressiveRefresh()) {
            UIUtil::ItemList::Refresh();
        }
    }

    void JunkHandler::SellJunk() {
        bool expected = false;
        if (!operationInProgress.compare_exchange_strong(expected, true)) {
            SKSE::log::info("SellJunk blocked: another operation is already in progress");
            return;
        }

        BGSListForm* junkList = Settings::GetJunkList();
        if (!junkList || junkList->forms.size() <= 0) {
            SKSE::log::info("No Junk to sell!");
            RE::DebugMessageBox("No Junk to sell!");
            operationInProgress.store(false);
            return;
        }

        auto player = RE::PlayerCharacter::GetSingleton();
        float playerCarryWeight = player->GetActorValue(RE::ActorValue::kCarryWeight);

        BGSListForm* sellList = BuildSellFormList();
        Count playerItemListCount = GetContainerItemListCount(player, sellList);

        SKSE::log::info("SellList generated. Form Count {}", sellList->forms.size());
        SKSE::log::info("Player has {} junk items in the SellList to sell!", playerItemListCount);

        if (playerItemListCount <= 0) {
            SKSE::log::info("No Junk to sell!");
            RE::DebugMessageBox("No Junk to sell!");
            operationInProgress.store(false);
            return;
        }

        TESObjectREFR* vendorActorRef = GetBarterMenuContainer();
        TESObjectREFR* vendorContainer = GetBarterMenuMerchantContainer();

        if (!vendorActorRef || vendorActorRef == player->As<TESObjectREFR>()) {
            SKSE::log::error("SKSE Failed to get a valid vendor actor. Exiting Bulk Sale process.");
            RE::DebugMessageBox("JunkIt encountered an error attempting to sell items. Please report this on the JunkIt mod page.");
            operationInProgress.store(false);
            return;
        }

        if (!vendorContainer) {
            SKSE::log::info("Vendor Container not found, using Vendor Actor as Container.");
            vendorContainer = vendorActorRef;
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

        BSTArray<TESForm*> sellForms = sellList->forms;
        Count iTotal = sellForms.size();
        Count totalToSell = 0;
        Count totalPossibleToSell = 0;
        float calculatedVendorGold = vendorGoldDisplay;
        float totalSellValue = 0;

        BGSListForm* sellAllList = sellList;
        std::vector<std::pair<TESForm*, Count>> partialSellItems;

        for (Count iCurrent = 0; iCurrent < iTotal; iCurrent++) {
            TESForm* item = sellForms[iCurrent];
            if (!item) continue;

            Count iCount = GetContainerSingleItemCount(player, item);
            Count iTotalCount = iCount;
            totalPossibleToSell += iCount;

            SKSE::log::info("Calculating Sell Item: {} -- player has {} of this item", item->GetName(), iCount);

            if (iCount > 0) {
                float itemGoldValue = static_cast<float>(GetMenuItemValue(item));
                float sellValue = itemGoldValue * sellMult;
                float goldDifferential = calculatedVendorGold - (sellValue * iCount);

                while (RoundNumber(goldDifferential) <= 0 && iCount > 0) {
                    iCount -= 1;
                    goldDifferential = calculatedVendorGold - (sellValue * iCount);
                }

                if (iCount > 0 && iCount < iTotalCount) {
                    sellAllList->RemoveAddedForm(item);
                    partialSellItems.push_back({item, iCount});

                    calculatedVendorGold -= sellValue * iCount;
                    totalSellValue += sellValue * iCount;
                    totalToSell += iCount;
                    SKSE::log::info("Creating partial listing for {} {} for {} gold", iCount, item->GetName(), sellValue * iCount);
                } else if (iCount <= 0) {
                    SKSE::log::info("Cannot sell any of this item, removing from bulk sale list");
                    sellAllList->RemoveAddedForm(item);
                } else {
                    calculatedVendorGold -= sellValue * iCount;
                    totalSellValue += sellValue * iCount;
                    totalToSell += iCount;
                    SKSE::log::info("Full Quantity Sell {} {} for {} gold", iCount, item->GetName(), sellValue * iCount);
                }
            }
        }

        if (totalToSell <= 0) {
            SKSE::log::info("Vendor cannot afford to buy any junk!");
            RE::DebugMessageBox("Vendor cannot afford to buy any junk!");
            operationInProgress.store(false);
            return;
        }

        if (Settings::ConfirmSell()) {
            std::string confirmText = fmt::format("Sell {} junk items for {} gold?", totalToSell, RoundNumber(totalSellValue));
            ShowConfirmationMessageBox(confirmText.c_str(), {"Yes", "No"},
                [sellAllList, partialSellItems, vendorActorRef, vendorContainer, totalSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay, playerCarryWeight](unsigned int choice) {
                    if (choice == 0) {
                        ExecuteSell(sellAllList, partialSellItems, vendorActorRef, vendorContainer, totalSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay, playerCarryWeight);
                    }
                    operationInProgress.store(false);
                });
        } else {
            ExecuteSell(sellAllList, partialSellItems, vendorActorRef, vendorContainer, totalSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay, playerCarryWeight);
            operationInProgress.store(false);
        }
    }

    void JunkHandler::ExecuteSell(BGSListForm* sellAllList, std::vector<std::pair<TESForm*, Count>> partialSellItems, TESObjectREFR* vendorActorRef, TESObjectREFR* vendorContainer, float totalSellValue, Count totalToSell, Count totalPossibleToSell, float vendorGoldDisplay, float playerCarryWeight) {
        auto player = RE::PlayerCharacter::GetSingleton();

        WarnLargeInventory(player, vendorContainer);

        if (Settings::GetNotifyOnJunkSell()) {
            DebugNotification("JunkIt - Processing Sale...");
        }

        TESObjectMISC* gold001 = Settings::GetGold001();
        Actor* vendorActor = vendorActorRef->As<Actor>();

        Count goldToGimme = RoundNumber(totalSellValue);
        Count vendorActorGold = vendorActor->GetItemCount(gold001);
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
            Count containerGold = vendorContainer->GetItemCount(gold001);
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

        SKSE::log::info("SellPartialList Size: {}", partialSellItems.size());
        for (const auto& [item, count] : partialSellItems) {
            if (count > 0) {
                player->RemoveItem(item->As<TESBoundObject>(), count, ITEM_REMOVE_REASON::kSelling, nullptr, vendorContainer);
                SKSE::log::info("Transaction for partial quantity listing {} {} complete", count, item->GetName());
            }
        }

        SKSE::log::info("ProcessItemListTransfer(SellAllList) - SellAllList.Size: {}", sellAllList->forms.size());
        Count iTotalFullQuantityItems = ProcessItemListTransfer(sellAllList, player, vendorContainer, 1);
        SKSE::log::info("Transaction {} full quantity item sales complete", iTotalFullQuantityItems);

        player->AddSkillExperience(RE::ActorValue::kSpeech, totalSellValue);

        if (totalToSell >= totalPossibleToSell) {
            SKSE::log::info("Sold All Junk Items for {} Gold", totalSellValue);
            if (Settings::GetNotifyOnJunkSell()) {
                DebugNotification("JunkIt - Sold All Junk Items!");
            }
        } else {
            SKSE::log::info("Sold {} Junk Items for {} Gold", totalToSell, totalSellValue);
            if (Settings::GetNotifyOnJunkSell()) {
                std::string msg = fmt::format("JunkIt - Sold {} Junk Items!", totalToSell);
                DebugNotification(msg.c_str());
            }
        }

        if (player->GetActorValue(RE::ActorValue::kCarryWeight) != playerCarryWeight) {
            player->SetActorValue(RE::ActorValue::kCarryWeight, playerCarryWeight);
        }

        if (Settings::GetAggressiveRefresh()) {
            UIUtil::ItemList::Refresh();
        }
    }

    TESForm* JunkHandler::ToggleSelectedItemKeyword() {
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

        TESBoundObject* itemObject = inventoryEntry->GetObject();
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
        BGSKeyword* isJunkKYWD = Settings::GetIsJunkKYWD();
        BGSKeywordForm* keywordForm = nullptr;

        if (itemForm->GetFormType() == FormType::Light) {
            DebugNotification("JunkIt - Lights cannot be marked as Junk");
            return nullptr;
        }

        if (itemForm->GetFormType() == FormType::Ammo) {
            TESAmmo* ammo = itemForm->As<TESAmmo>();
            keywordForm = ammo->AsKeywordForm();
        } else {
            keywordForm = itemForm->As<BGSKeywordForm>();
        }

        if (!keywordForm) {
            SKSE::log::error("Error attempting to add IsJunk keyword to {} [{}]. Failed to typecast to BGSKeywordForm", itemName, hexFormId);
            DebugNotification("JunkIt - Failed to mark item as junk!");
            return nullptr;
        }

        if (inventoryEntry->IsQuestObject()) {
            SKSE::log::info("Cannot mark quest item {} [{}] as junk", itemName, hexFormId);
            if (!keywordForm->HasKeyword(isJunkKYWD)) {
                DebugNotification("JunkIt - Quest Items cannot be marked as Junk");
                return nullptr;
            }
        }

        if (Settings::ProtectEquipped() && inventoryEntry->IsWorn()) {
            SKSE::log::info("Cannot mark equipped item {} [{}] as junk", itemName, hexFormId);
            if (!keywordForm->HasKeyword(isJunkKYWD)) {
                DebugNotification("JunkIt - Equipped Items are protected and cannot be marked as Junk");
                return nullptr;
            }
        }

        if (Settings::ProtectFavorites() && inventoryEntry->IsFavorited()) {
            SKSE::log::info("Cannot mark favorited item {} [{}] as junk", itemName, hexFormId);
            if (!keywordForm->HasKeyword(isJunkKYWD)) {
                DebugNotification("JunkIt - Favorited Items are protected and cannot be marked as Junk");
                return nullptr;
            }
        }

        bool isJunk = keywordForm->HasKeyword(isJunkKYWD);
        if (isJunk) {
            SKSE::log::info("Removing IsJunk keyword from {} [{}]", itemName, hexFormId);
            keywordForm->RemoveKeyword(isJunkKYWD);
        } else {
            SKSE::log::info("Adding IsJunk keyword to {} [{}]", itemName, hexFormId);
            keywordForm->AddKeyword(isJunkKYWD);
        }

        itemListMenu->Update();
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

            TESBoundObject* entryObject = entryData->GetObject();
            if (!entryObject) continue;

            if (entryObject->GetFormID() == a_form->GetFormID()) {
                goldValue = entryData->GetValue();
                SKSE::log::info("          Value Per Item = {} gold", goldValue);
                break;
            }
        }

        return goldValue;
    }

    std::int32_t JunkHandler::ProcessItemListTransfer(BGSListForm* a_itemList, TESObjectREFR* a_fromContainer, TESObjectREFR* a_toContainer, std::int32_t a_isBarter) {
        SKSE::log::info(" ");
        SKSE::log::info("---- Initiating Item Transfer using FormList ----");

        ITEM_REMOVE_REASON reason = ITEM_REMOVE_REASON::kStoreInContainer;
        if (a_isBarter == 1) {
            reason = ITEM_REMOVE_REASON::kSelling;
            SKSE::log::info("Item Removal Reason set to ITEM_REMOVE_REASON::kSelling");
        } else if (a_toContainer->GetFormType() == FormType::ActorCharacter) {
            reason = ITEM_REMOVE_REASON::kStoreInTeammate;
            SKSE::log::info("Item Removal Reason set to ITEM_REMOVE_REASON::kStoreInTeammate");
        } else {
            SKSE::log::info("Item Removal Reason set to ITEM_REMOVE_REASON::kStoreInContainer");
        }

        InventoryItemMap filteredInventoryMap = a_fromContainer->GetInventory([&](TESBoundObject& obj) {
            return a_itemList->HasForm(obj.GetFormID());
        });

        Count totalTransferred = 0;
        for (auto const& [item, inventoryData] : filteredInventoryMap) {
            Count itemCount = inventoryData.first;
            if (itemCount == 0) continue;

            InventoryEntryData* invData = inventoryData.second.get();
            TransferItem(item, a_fromContainer, a_toContainer, reason, itemCount, invData);
            totalTransferred += itemCount;
        }

        UIUtil::ItemList::Refresh();

        SKSE::log::info("---- ItemList Transfer Completed ----");
        SKSE::log::info(" ");

        return totalTransferred;
    }

    std::int32_t JunkHandler::GetContainerItemListCount(TESObjectREFR* a_container, BGSListForm* a_itemList) {
        Count totalCount = 0;

        std::string containerName = a_container->GetName();
        if (containerName.empty()) {
            containerName = FormUtil::Form::GetFormConfigString(a_container);
        }

        InventoryCountMap filteredInventoryMap = a_container->GetInventoryCounts([&](TESBoundObject& obj) {
            return a_itemList->HasForm(obj.GetFormID());
        });

        SetContainerInventoryCountMap(filteredInventoryMap, a_container);

        for (auto const& [item, count] : filteredInventoryMap) {
            totalCount += count;
        }

        SKSE::log::info("     {} Inventory FormList Count {}", containerName, totalCount);
        return totalCount;
    }

    std::int32_t JunkHandler::GetContainerSingleItemCount(TESObjectREFR* a_container, TESForm* a_item) {
        Count totalCount = 0;

        if (!a_item) {
            SKSE::log::error("     Item form is not valid. Item Count {}", totalCount);
            return totalCount;
        }

        InventoryCountMap* invMap = GetContainerInventoryCountMap(a_container);

        auto itemInvData = invMap->find(a_item->As<TESBoundObject>());
        if (itemInvData != invMap->end()) {
            totalCount = itemInvData->second;
        }

        if (totalCount) {
            SKSE::log::info("     {} {} [{}]", totalCount, a_item->GetName(), FormUtil::Form::GetFormConfigString(a_item));
        }
        return totalCount;
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
