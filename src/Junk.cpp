#include "junk.h"
#include "JunkData.h"
#include "SendUIMessage.h"
#include "SkyPromptIntegration.h"
#include "Translation.h"
#include <SKSE/API.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <unordered_set>
#include <vector>

RE::MessageBoxData::~MessageBoxData() = default;

namespace JunkIt {

    std::atomic<bool> JunkHandler::operationInProgress{ false };

    namespace {
        RE::GFxMovieView* GetOpenInventoryMovie() {
            const auto ui = RE::UI::GetSingleton();
            if (!ui) {
                return nullptr;
            }
            if (auto containerMenu = ui->GetMenu<ContainerMenu>()) {
                if (containerMenu->uiMovie) {
                    return containerMenu->uiMovie.get();
                }
            }
            if (auto barterMenu = ui->GetMenu<BarterMenu>()) {
                if (barterMenu->uiMovie) {
                    return barterMenu->uiMovie.get();
                }
            }
            return nullptr;
        }

        TESObjectREFR* LookupRefr(FormID formId) {
            if (formId == 0) {
                return nullptr;
            }
            auto* form = RE::TESForm::LookupByID(formId);
            return form ? form->As<TESObjectREFR>() : nullptr;
        }

        void SendInventoryUpdate(TESObjectREFR* ref) {
            if (ref) {
                RE::SendUIMessage::SendInventoryUpdateMessage(ref, nullptr);
            }
        }

        void UpdateItemListOwner(ItemList* itemList, TESObjectREFR* owner) {
            if (itemList && owner) {
                itemList->Update(owner);
            }
        }

        void InvalidateInventoryLists(RE::GFxMovieView* movie) {
            if (!movie) {
                return;
            }
            movie->Invoke("_root.Menu_mc.inventoryLists.InvalidateListData", nullptr, nullptr, 0);
        }

        void NudgeActiveSegment(RE::GFxMovieView* movie) {
            if (!movie) {
                return;
            }

            RE::GFxValue activeSegment;
            if (!movie->GetVariable(&activeSegment, "_root.Menu_mc.inventoryLists.categoryList.activeSegment")) {
                return;
            }

            const int original = static_cast<int>(activeSegment.GetNumber());
            const int flipped = original == 0 ? 1 : 0;

            RE::GFxValue flippedVal(static_cast<double>(flipped));
            movie->SetVariable("_root.Menu_mc.inventoryLists.categoryList.activeSegment", flippedVal);

            RE::GFxValue restoredVal(static_cast<double>(original));
            movie->SetVariable("_root.Menu_mc.inventoryLists.categoryList.activeSegment", restoredVal);
            movie->Invoke("_root.Menu_mc.inventoryLists.showItemsList", nullptr, nullptr, 0);
        }

        struct PreviewStack {
            InventoryEntryData* entry = nullptr;
            std::int32_t count = 0;
        };

        void SortPreviewStacks(std::vector<PreviewStack>& stacks, Settings::SortPriority priority) {
            const auto valueWeight = [](InventoryEntryData* entry) {
                const float weight = entry->GetWeight();
                return weight != 0.0f ? entry->GetValue() / weight : 0.0f;
            };

            switch (priority) {
                case Settings::SortPriority::kWeightHighLow:
                    std::sort(stacks.begin(), stacks.end(), [](const PreviewStack& a, const PreviewStack& b) {
                        return a.entry->GetWeight() > b.entry->GetWeight();
                    });
                    break;
                case Settings::SortPriority::kWeightLowHigh:
                    std::sort(stacks.begin(), stacks.end(), [](const PreviewStack& a, const PreviewStack& b) {
                        return a.entry->GetWeight() < b.entry->GetWeight();
                    });
                    break;
                case Settings::SortPriority::kValueHighLow:
                    std::sort(stacks.begin(), stacks.end(), [](const PreviewStack& a, const PreviewStack& b) {
                        return a.entry->GetValue() > b.entry->GetValue();
                    });
                    break;
                case Settings::SortPriority::kValueLowHigh:
                    std::sort(stacks.begin(), stacks.end(), [](const PreviewStack& a, const PreviewStack& b) {
                        return a.entry->GetValue() < b.entry->GetValue();
                    });
                    break;
                case Settings::SortPriority::kValueWeightHighLow:
                    std::sort(stacks.begin(), stacks.end(), [&](const PreviewStack& a, const PreviewStack& b) {
                        return valueWeight(a.entry) > valueWeight(b.entry);
                    });
                    break;
                case Settings::SortPriority::kValueWeightLowHigh:
                    std::sort(stacks.begin(), stacks.end(), [&](const PreviewStack& a, const PreviewStack& b) {
                        return valueWeight(a.entry) < valueWeight(b.entry);
                    });
                    break;
                case Settings::SortPriority::kChaos:
                    break;
            }
        }

        std::int32_t CountIdentityUnits(InventoryEntryData* entry, const std::unordered_set<std::string>& identities) {
            if (!entry || identities.empty()) {
                return 0;
            }

            if (!entry->extraLists || entry->extraLists->empty()) {
                const auto identity = JunkDataManager::BuildIdentityForEntry(entry, nullptr);
                if (identities.contains(identity)) {
                    return entry->countDelta > 0 ? entry->countDelta : 0;
                }
                return 0;
            }

            std::int32_t extrasTotal = 0;
            std::int32_t matched = 0;
            for (auto* extraList : *entry->extraLists) {
                if (!extraList) {
                    continue;
                }
                const auto extraCount = extraList->GetCount();
                extrasTotal += extraCount;
                if (identities.contains(JunkDataManager::BuildIdentityForEntry(entry, extraList))) {
                    matched += extraCount;
                }
            }

            const auto plain = entry->countDelta - extrasTotal;
            if (plain > 0 && identities.contains(JunkDataManager::BuildIdentityForEntry(entry, nullptr))) {
                matched += plain;
            }
            return matched > 0 ? matched : 0;
        }

        bool ExtraListMatchesUniqueID(const ExtraDataList* extraList, std::uint16_t uniqueID) {
            if (!extraList) {
                return false;
            }
            const auto* extraId = extraList->GetByType<RE::ExtraUniqueID>();
            return extraId && extraId->uniqueID == uniqueID;
        }

        bool EntryMatchesMovedUniqueID(InventoryEntryData* entry, std::uint16_t uniqueID) {
            if (!entry) {
                return false;
            }

            auto& junkManager = JunkDataManager::GetSingleton();
            if (uniqueID == 0) {
                return junkManager.IsJunk(entry);
            }

            if (!entry->extraLists) {
                return false;
            }

            for (auto* extraList : *entry->extraLists) {
                if (!ExtraListMatchesUniqueID(extraList, uniqueID)) {
                    continue;
                }
                return junkManager.IsJunk(JunkDataManager::BuildIdentityForEntry(entry, extraList));
            }
            return false;
        }

        TESBoundObject* LookupBoundObject(FormID baseObj) {
            auto* form = TESForm::LookupByID(baseObj);
            return form ? form->As<TESBoundObject>() : nullptr;
        }
    }

    void JunkHandler::ApplyInventoryUIRefresh(TESObjectREFR* primary, TESObjectREFR* secondary, bool nudgeSegment) {
        ItemList* itemList = UIUtil::ItemList::GetOpenList();
        if (!itemList) {
            return;
        }

        auto* movie = GetOpenInventoryMovie();

        SendInventoryUpdate(primary);
        if (secondary && secondary != primary) {
            SendInventoryUpdate(secondary);
        }

        UpdateItemListOwner(itemList, primary);
        if (secondary && secondary != primary) {
            UpdateItemListOwner(itemList, secondary);
        }

        InvalidateInventoryLists(movie);

        if (nudgeSegment) {
            NudgeActiveSegment(movie);
        }
    }

    void JunkHandler::ScheduleInventoryUIRefresh(FormID primaryId, FormID secondaryId, bool largeOp, int framesRemaining) {
        auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            return;
        }

        taskInterface->AddUITask([primaryId, secondaryId, largeOp, framesRemaining]() {
            if (framesRemaining > 0) {
                ScheduleInventoryUIRefresh(primaryId, secondaryId, largeOp, framesRemaining - 1);
                return;
            }

            auto* primary = LookupRefr(primaryId);
            auto* secondary = LookupRefr(secondaryId);
            if (!primary && !secondary) {
                return;
            }

            ApplyInventoryUIRefresh(primary, secondary, largeOp);
            SkyPromptIntegration::GetSingleton().RefreshPrompts();
        });
    }

    namespace {
        void ScheduleAggressiveRefresh(std::chrono::steady_clock::time_point deadline, int framesRemaining) {
            auto* taskInterface = SKSE::GetTaskInterface();
            if (!taskInterface) {
                return;
            }

            taskInterface->AddUITask([deadline, framesRemaining]() {
                if (std::chrono::steady_clock::now() >= deadline) {
                    return;
                }
                if (framesRemaining > 0) {
                    ScheduleAggressiveRefresh(deadline, framesRemaining - 1);
                    return;
                }
                UIUtil::ItemList::Refresh();
                ScheduleAggressiveRefresh(deadline, 300);
            });
        }
    }

    void JunkHandler::StartAggressiveRefresh() {
        if (!Settings::GetAggressiveRefresh()) {
            return;
        }
        const auto deadline = std::chrono::steady_clock::now()
            + std::chrono::seconds(Settings::GetAggressiveRefreshMaxInterval());
        ScheduleAggressiveRefresh(deadline, 0);
    }

    void JunkHandler::RefreshMenusAfterBulk(TESObjectREFR* primary, TESObjectREFR* secondary, std::size_t uniqueTypes, Count totalItems) {
        const bool largeOp = uniqueTypes >= Settings::GetLargeUniqueTypes() || totalItems >= Settings::GetLargeTotalItems();
        int deferredFrames = 1;
        if (largeOp) {
            deferredFrames = static_cast<int>(std::lround(2.0f * Settings::GetHeavyLoadDelayMultiplier()));
            deferredFrames = std::clamp(deferredFrames, 1, 10);
        }

        SKSE::log::info(
            "Bulk UI refresh | uniqueTypes={} totalItems={} large={} deferFrames={}",
            uniqueTypes,
            totalItems,
            largeOp,
            deferredFrames);

        ApplyInventoryUIRefresh(primary, secondary, largeOp);

        const FormID primaryId = primary ? primary->GetFormID() : 0;
        const FormID secondaryId = secondary ? secondary->GetFormID() : 0;

        ScheduleInventoryUIRefresh(primaryId, secondaryId, largeOp, 0);

        if (largeOp && deferredFrames > 1) {
            ScheduleInventoryUIRefresh(primaryId, secondaryId, largeOp, deferredFrames - 1);
        }

        StartAggressiveRefresh();
        SkyPromptIntegration::GetSingleton().RecapturePreviews();
    }

    void JunkHandler::ShowConfirmationMessageBox(const char* bodyText, std::vector<std::string> buttons, std::function<void(unsigned int)> callback) {
        auto messageBoxData = new RE::MessageBoxData();
        messageBoxData->bodyText = bodyText;
        for (const auto& button : buttons) {
            messageBoxData->buttonText.push_back(button.c_str());
        }
        messageBoxData->buttonPressOffset = 4;
        messageBoxData->warningType = 10;
        messageBoxData->menuDepth = 10;
        messageBoxData->callback = RE::BSTSmartPointer<RE::IMessageBoxCallback>(new JunkItMessageBoxCallback(std::move(callback), messageBoxData->buttonPressOffset));
        RE::MessageBoxMenu::QueueMessage(messageBoxData);
    }

    JunkHandler::Count JunkHandler::GetItemCount(TESObjectREFR* a_container, TESBoundObject* a_item) {
        if (!a_container || !a_item) {
            return 0;
        }

        Count count = 0;
        if (auto* tesContainer = a_container->GetContainer()) {
            count += tesContainer->GetObjectCount(a_item);
        }

        if (auto* changes = a_container->GetInventoryChanges(); changes && changes->entryList) {
            for (const auto& entry : *changes->entryList) {
                if (entry && entry->object == a_item) {
                    count += entry->countDelta;
                }
            }
        }

        return count > 0 ? count : 0;
    }

    bool JunkHandler::TryReadBarterPrices(float& vendorGold, float& sellMult) {
        const auto ui = RE::UI::GetSingleton();
        auto menu = ui ? ui->GetMenu<BarterMenu>() : nullptr;
        if (!menu || !menu->uiMovie) {
            return false;
        }

        RE::GFxValue gfxVendorGold, gfxSellMult;
        menu->uiMovie->GetVariable(&gfxVendorGold, "_root.Menu_mc._vendorGold");
        menu->uiMovie->GetVariable(&gfxSellMult, "_root.Menu_mc._sellMult");

        vendorGold = static_cast<float>(gfxVendorGold.GetNumber());
        sellMult = static_cast<float>(gfxSellMult.GetNumber());

        if (sellMult <= 0.0f) {
            menu->uiMovie->GetVariable(&gfxSellMult, "_root.Menu_mc.fSellMult");
            sellMult = static_cast<float>(gfxSellMult.GetNumber());
            if (sellMult <= 0.0f) {
                sellMult = 0.5f;
            }
        }

        return true;
    }

    JunkHandler::SellTotals JunkHandler::ComputeSellTotals(
        const std::vector<std::pair<InventoryEntryData*, Count>>& sellList,
        float vendorGold,
        float sellMult) {
        SellTotals totals;
        float calculatedVendorGold = vendorGold;
        float totalSellValue = 0.0f;

        for (auto& [entryData, itemCount] : sellList) {
            if (!entryData || !entryData->object || itemCount <= 0) {
                continue;
            }

            Count iCount = itemCount;
            totals.totalPossibleToSell += iCount;

            Count menuValue = GetMenuItemValue(entryData);
            float itemGoldValue = menuValue >= 0 ? static_cast<float>(menuValue) : static_cast<float>(entryData->GetValue());
            float sellValue = itemGoldValue * sellMult;

            if (sellValue <= 0.0f) {
                totals.totalToSell += iCount;
                totals.itemsToSell.push_back({entryData, iCount});
                continue;
            }

            while (iCount > 0 && RoundNumber(sellValue * static_cast<float>(iCount)) > RoundNumber(calculatedVendorGold)) {
                iCount -= 1;
            }

            if (iCount > 0) {
                calculatedVendorGold -= sellValue * static_cast<float>(iCount);
                totalSellValue += sellValue * static_cast<float>(iCount);
                totals.totalToSell += iCount;
                totals.itemsToSell.push_back({entryData, iCount});
            }
        }

        totals.roundedSellValue = RoundNumber(totalSellValue);
        return totals;
    }

    bool JunkHandler::EntryPassesPreviewFilters(InventoryEntryData* a_entry, bool sellFilters) {
        if (!a_entry || !a_entry->object || a_entry->IsQuestObject()) {
            return false;
        }
        if (Settings::ProtectEquipped() && a_entry->IsWorn()) {
            return false;
        }
        if (Settings::ProtectFavorites() && a_entry->IsFavorited()) {
            return false;
        }
        if (sellFilters && Settings::ProtectEnchanted() && a_entry->IsEnchanted()) {
            return false;
        }
        return true;
    }

    namespace {
        template <class InventoryMap, class Passes, class Sellable>
        void FillPreviewStacks(
            InventoryMap& inventory,
            bool sellFilters,
            std::vector<PreviewStack>& out,
            Passes passesFilters,
            Sellable sellableCount) {
            for (auto& [obj, data] : inventory) {
                if (!obj || data.first <= 0) {
                    continue;
                }

                auto* entry = data.second.get();
                if (!entry || !passesFilters(entry, sellFilters)) {
                    continue;
                }

                const auto count = sellableCount(entry);
                if (count > 0) {
                    out.push_back({ entry, count });
                }
            }
        }
    }

    std::optional<JunkHandler::ContainerPreviewCounts> JunkHandler::CaptureContainerPreview() {
        auto* player = PlayerCharacter::GetSingleton();
        TESObjectREFR* container = GetContainerMenuContainer();
        if (!player || !container) {
            return std::nullopt;
        }

        auto passes = [](InventoryEntryData* entry, bool sellFilters) {
            return JunkHandler::EntryPassesPreviewFilters(entry, sellFilters);
        };
        auto sellable = [](InventoryEntryData* entry) {
            return JunkHandler::GetSellableJunkCount(entry);
        };

        auto containerInventory = container->GetInventory();
        std::vector<PreviewStack> retrieveStacks;
        FillPreviewStacks(containerInventory, false, retrieveStacks, passes, sellable);

        ContainerPreviewCounts preview;
        for (const auto& stack : retrieveStacks) {
            preview.retrieveCount += stack.count;
        }

        auto playerInventory = player->GetInventory();
        std::vector<PreviewStack> storeStacks;
        FillPreviewStacks(playerInventory, false, storeStacks, passes, sellable);

        if (GetContainerMode() == ContainerMenu::ContainerMode::kNPCMode) {
            Actor* transferActor = container->As<Actor>();
            if (!transferActor) {
                preview.storeCount = 0;
                return preview;
            }

            SortPreviewStacks(storeStacks, Settings::GetTransferPriority());
            float maxWeight = transferActor->AsActorValueOwner()->GetActorValue(ActorValue::kCarryWeight);
            float currentWeight = container->GetWeightInContainer();
            Count total = 0;
            for (auto& stack : storeStacks) {
                if (!stack.entry || !stack.entry->object || stack.count <= 0) {
                    continue;
                }

                Count iCount = stack.count;
                const float itemWeight = stack.entry->object->GetWeight();
                float currentWeightWithItems = (itemWeight * static_cast<float>(iCount)) + currentWeight;
                while (currentWeightWithItems > maxWeight && iCount > 0) {
                    iCount -= 1;
                    currentWeightWithItems = (itemWeight * static_cast<float>(iCount)) + currentWeight;
                }

                if (iCount > 0) {
                    currentWeight += itemWeight * static_cast<float>(iCount);
                    total += iCount;
                }
            }
            preview.storeCount = total;
            return preview;
        }

        for (const auto& stack : storeStacks) {
            preview.storeCount += stack.count;
        }
        return preview;
    }

    JunkHandler::SellPreviewCapture JunkHandler::CaptureSellPreview() {
        SellPreviewCapture capture;
        auto* player = PlayerCharacter::GetSingleton();
        if (!player) {
            capture.pricesReady = true;
            return capture;
        }

        auto playerInventory = player->GetInventory();
        std::vector<PreviewStack> sellStacks;
        FillPreviewStacks(
            playerInventory,
            true,
            sellStacks,
            [](InventoryEntryData* entry, bool sellFilters) {
                return JunkHandler::EntryPassesPreviewFilters(entry, sellFilters);
            },
            [](InventoryEntryData* entry) {
                return JunkHandler::GetSellableJunkCount(entry);
            });

        if (sellStacks.empty()) {
            capture.pricesReady = true;
            return capture;
        }

        SortPreviewStacks(sellStacks, Settings::GetSellPriority());

        std::vector<std::pair<InventoryEntryData*, Count>> sellList;
        sellList.reserve(sellStacks.size());
        for (const auto& stack : sellStacks) {
            sellList.emplace_back(stack.entry, stack.count);
        }

        float vendorGold = 0.0f;
        float sellMult = 0.0f;
        if (!TryReadBarterPrices(vendorGold, sellMult)) {
            return capture;
        }

        capture.sellMult = sellMult;
        capture.pricesReady = true;
        capture.gold = ComputeSellTotals(sellList, vendorGold, sellMult).roundedSellValue;
        return capture;
    }

    void JunkHandler::CollectEntryIdentities(InventoryEntryData* entry, std::vector<std::string>& out) {
        out.clear();
        if (!entry || !entry->object) {
            return;
        }

        if (!entry->extraLists || entry->extraLists->empty()) {
            out.push_back(JunkDataManager::BuildIdentityForEntry(entry, nullptr));
            return;
        }

        for (auto* extraList : *entry->extraLists) {
            out.push_back(JunkDataManager::BuildIdentityForEntry(entry, extraList));
        }
    }

    std::int32_t JunkHandler::CountPreviewIdentities(
        TESObjectREFR* container,
        const std::vector<std::string>& identities,
        bool sellFilters) {
        if (!container || identities.empty()) {
            return 0;
        }

        std::unordered_set<std::string> identitySet(identities.begin(), identities.end());
        identitySet.erase("");
        if (identitySet.empty()) {
            return 0;
        }

        auto inventory = container->GetInventory();
        Count total = 0;
        for (auto& [obj, data] : inventory) {
            if (!obj || data.first <= 0) {
                continue;
            }

            auto* entry = data.second.get();
            if (!entry || !EntryPassesPreviewFilters(entry, sellFilters)) {
                continue;
            }
            total += CountIdentityUnits(entry, identitySet);
        }
        return total;
    }

    bool JunkHandler::MovedItemIsPreviewableJunk(TESObjectREFR* dest, FormID baseObj, std::uint16_t uniqueID, bool sellFilters) {
        if (!dest) {
            return false;
        }

        auto* form = TESForm::LookupByID(baseObj);
        if (!form || !JunkDataManager::GetSingleton().IsAnyJunkForForm(form)) {
            return false;
        }

        auto* bound = LookupBoundObject(baseObj);
        if (!bound) {
            return false;
        }

        auto inventory = dest->GetInventory();
        const auto it = inventory.find(bound);
        if (it == inventory.end() || !it->second.second) {
            return false;
        }

        auto* entry = it->second.second.get();
        if (!EntryPassesPreviewFilters(entry, sellFilters)) {
            return false;
        }
        return EntryMatchesMovedUniqueID(entry, uniqueID);
    }

    std::int32_t JunkHandler::CountJunkUnits(InventoryEntryData* entry) {
        return GetSellableJunkCount(entry);
    }

    std::optional<JunkHandler::JunkPreviewUnit> JunkHandler::LookupJunkPreviewUnit(
        TESObjectREFR* dest,
        FormID baseObj,
        std::uint16_t uniqueID,
        float sellMult) {
        if (!dest) {
            return std::nullopt;
        }

        auto* form = TESForm::LookupByID(baseObj);
        if (!form || !JunkDataManager::GetSingleton().IsAnyJunkForForm(form)) {
            return std::nullopt;
        }

        auto* bound = LookupBoundObject(baseObj);
        if (!bound) {
            return std::nullopt;
        }

        auto inventory = dest->GetInventory();
        const auto it = inventory.find(bound);
        if (it == inventory.end() || !it->second.second) {
            return std::nullopt;
        }

        auto* entry = it->second.second.get();
        if (!entry || entry->IsQuestObject() || !EntryMatchesMovedUniqueID(entry, uniqueID)) {
            return std::nullopt;
        }

        const Count count = GetSellableJunkCount(entry);
        if (count <= 0) {
            return std::nullopt;
        }

        JunkPreviewUnit unit;
        unit.count = count;
        unit.favorited = entry->IsFavorited();
        unit.enchanted = entry->IsEnchanted();
        unit.worn = entry->IsWorn();
        if (sellMult > 0.0f) {
            unit.gold = ComputeSellGoldDelta(entry, count, sellMult);
        }
        return unit;
    }

    std::int32_t JunkHandler::ComputeSellGoldDelta(InventoryEntryData* entry, std::int32_t count, float sellMult) {
        if (!entry || count <= 0) {
            return 0;
        }

        const Count menuValue = GetMenuItemValue(entry);
        const float itemGoldValue = menuValue >= 0 ? static_cast<float>(menuValue) : static_cast<float>(entry->GetValue());
        const float sellValue = itemGoldValue * sellMult;
        if (sellValue <= 0.0f) {
            return 0;
        }
        return RoundNumber(sellValue * static_cast<float>(count));
    }

    std::optional<std::int32_t> JunkHandler::ComputeMovedItemSellGold(
        TESObjectREFR* dest,
        FormID baseObj,
        std::uint16_t uniqueID,
        std::int32_t count,
        float sellMult) {
        if (!dest || count <= 0) {
            return std::nullopt;
        }

        auto* form = TESForm::LookupByID(baseObj);
        if (!form || !JunkDataManager::GetSingleton().IsAnyJunkForForm(form)) {
            return std::nullopt;
        }

        auto* bound = LookupBoundObject(baseObj);
        if (!bound) {
            return std::nullopt;
        }

        auto inventory = dest->GetInventory();
        const auto it = inventory.find(bound);
        if (it == inventory.end() || !it->second.second) {
            return std::nullopt;
        }

        auto* entry = it->second.second.get();
        if (!EntryPassesPreviewFilters(entry, true) || !EntryMatchesMovedUniqueID(entry, uniqueID)) {
            return std::nullopt;
        }
        return ComputeSellGoldDelta(entry, count, sellMult);
    }

    void JunkHandler::MoveItems(TESBoundObject* a_item, TESObjectREFR* a_from, TESObjectREFR* a_to, ITEM_REMOVE_REASON a_reason, Count a_count, ExtraDataList* a_extraList) {
        if (!a_item || !a_from || !a_to || a_count <= 0) {
            return;
        }
        a_from->RemoveItem(a_item, a_count, a_reason, a_extraList, a_to);
    }

    JunkHandler::Count JunkHandler::GetSellableJunkCount(InventoryEntryData* a_entry) {
        if (!a_entry || !a_entry->object) {
            return 0;
        }

        auto& junkManager = JunkDataManager::GetSingleton();
        if (!a_entry->extraLists || a_entry->extraLists->empty()) {
            if (!junkManager.IsJunk(JunkDataManager::BuildIdentityForEntry(a_entry, nullptr))) {
                return 0;
            }
            return a_entry->countDelta > 0 ? a_entry->countDelta : 0;
        }

        Count extrasTotal = 0;
        Count junkExtras = 0;
        for (auto* extraList : *a_entry->extraLists) {
            if (!extraList) {
                continue;
            }
            const Count extraCount = extraList->GetCount();
            extrasTotal += extraCount;
            if (junkManager.IsJunk(JunkDataManager::BuildIdentityForEntry(a_entry, extraList))) {
                junkExtras += extraCount;
            }
        }

        Count junkPlain = 0;
        const Count plain = a_entry->countDelta - extrasTotal;
        if (plain > 0 && junkManager.IsJunk(JunkDataManager::BuildIdentityForEntry(a_entry, nullptr))) {
            junkPlain = plain;
        }

        const Count total = junkExtras + junkPlain;
        return total > 0 ? total : 0;
    }

    bool JunkHandler::EntryIsFullyJunk(InventoryEntryData* a_entry) {
        if (!a_entry || !a_entry->object) {
            return false;
        }

        auto& junkManager = JunkDataManager::GetSingleton();
        if (!a_entry->extraLists || a_entry->extraLists->empty()) {
            return junkManager.IsJunk(JunkDataManager::BuildIdentityForEntry(a_entry, nullptr));
        }

        Count extrasTotal = 0;
        for (auto* extraList : *a_entry->extraLists) {
            if (!extraList) {
                continue;
            }
            extrasTotal += extraList->GetCount();
            if (!junkManager.IsJunk(JunkDataManager::BuildIdentityForEntry(a_entry, extraList))) {
                return false;
            }
        }

        const Count plain = a_entry->countDelta - extrasTotal;
        if (plain > 0) {
            return junkManager.IsJunk(JunkDataManager::BuildIdentityForEntry(a_entry, nullptr));
        }
        return extrasTotal > 0;
    }

    void JunkHandler::SellEntryUnits(InventoryEntryData* a_entry, TESObjectREFR* a_from, TESObjectREFR* a_to, Count a_count) {
        if (!a_entry || !a_entry->object || !a_from || !a_to || a_count <= 0) {
            return;
        }

        if (EntryIsFullyJunk(a_entry)) {
            MoveItems(a_entry->object, a_from, a_to, ITEM_REMOVE_REASON::kSelling, a_count, nullptr);
            return;
        }

        auto& junkManager = JunkDataManager::GetSingleton();
        Count remaining = a_count;

        std::vector<std::pair<ExtraDataList*, Count>> junkStacks;
        if (a_entry->extraLists) {
            for (auto* extraList : *a_entry->extraLists) {
                if (!extraList) {
                    continue;
                }
                if (!junkManager.IsJunk(JunkDataManager::BuildIdentityForEntry(a_entry, extraList))) {
                    continue;
                }
                const Count extraCount = extraList->GetCount();
                if (extraCount > 0) {
                    junkStacks.emplace_back(extraList, extraCount);
                }
            }
        }

        for (const auto& [extraList, extraCount] : junkStacks) {
            if (remaining <= 0) {
                break;
            }
            const Count toSell = std::min(remaining, extraCount);
            MoveItems(a_entry->object, a_from, a_to, ITEM_REMOVE_REASON::kSelling, toSell, extraList);
            remaining -= toSell;
        }

        if (remaining > 0 && junkManager.IsJunk(JunkDataManager::BuildIdentityForEntry(a_entry, nullptr))) {
            MoveItems(a_entry->object, a_from, a_to, ITEM_REMOVE_REASON::kSelling, remaining, nullptr);
        }
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

            if (entryItem->data.objDesc->IsQuestObject()) {
                SKSE::log::info("Junk Item is Quest Item - Skipping {}", entryItem->data.objDesc->object->GetName());
                continue;
            }
            
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

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            SKSE::log::error("No player found");
            return sellList;
        }

        const auto ui = RE::UI::GetSingleton();
        GPtr<BarterMenu> barterMenu = ui ? ui->GetMenu<BarterMenu>() : nullptr;
        ItemList* itemListMenu = barterMenu ? barterMenu->GetRuntimeData().itemList : nullptr;
        if (!itemListMenu) {
            SKSE::log::error("No ItemListMenu found");
            return sellList;
        }

        // ItemList holds both inventories; StandardItemData::owner distinguishes player vs vendor.
        const auto playerHandle = player->GetHandle().native_handle();
        BSTArray<ItemList::Item*> listItems = itemListMenu->items;
        std::vector<std::pair<InventoryEntryData*, Count>> sortData;

        SKSE::log::info("Processing BarterMenu ItemList for player-owned sellable junk");

        for (std::uint32_t i = 0, size = listItems.size(); i < size; i++) {
            ItemList::Item* entryItem = listItems[i];
            if (!entryItem || !entryItem->data.objDesc) {
                continue;
            }

            if (entryItem->data.owner != playerHandle) {
                continue;
            }

            InventoryEntryData* objDesc = entryItem->data.objDesc;
            if (!objDesc->object) {
                continue;
            }

            if (objDesc->IsQuestObject()) {
                SKSE::log::info("Junk Item is Quest Item - Skipping {}", objDesc->object->GetName());
                continue;
            }

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

            Count count = GetSellableJunkCount(objDesc);
            if (EntryIsFullyJunk(objDesc)) {
                const Count uiCount = static_cast<Count>(entryItem->data.GetCount());
                if (uiCount > count) {
                    count = uiCount;
                }
            }
            if (count <= 0) {
                continue;
            }

            sortData.emplace_back(objDesc, count);
        }

        auto priority = Settings::GetSellPriority();
        if (priority == Settings::SortPriority::kWeightHighLow) {
            std::sort(sortData.begin(), sortData.end(), [](const auto& a, const auto& b) {
                return a.first->GetWeight() > b.first->GetWeight();
            });
        } else if (priority == Settings::SortPriority::kWeightLowHigh) {
            std::sort(sortData.begin(), sortData.end(), [](const auto& a, const auto& b) {
                return a.first->GetWeight() < b.first->GetWeight();
            });
        } else if (priority == Settings::SortPriority::kValueHighLow) {
            std::sort(sortData.begin(), sortData.end(), [](const auto& a, const auto& b) {
                return a.first->GetValue() > b.first->GetValue();
            });
        } else if (priority == Settings::SortPriority::kValueLowHigh) {
            std::sort(sortData.begin(), sortData.end(), [](const auto& a, const auto& b) {
                return a.first->GetValue() < b.first->GetValue();
            });
        } else if (priority == Settings::SortPriority::kValueWeightHighLow) {
            std::sort(sortData.begin(), sortData.end(), [](const auto& a, const auto& b) {
                float aVW = a.first->GetWeight() != 0 ? a.first->GetValue() / a.first->GetWeight() : 0;
                float bVW = b.first->GetWeight() != 0 ? b.first->GetValue() / b.first->GetWeight() : 0;
                return aVW > bVW;
            });
        } else if (priority == Settings::SortPriority::kValueWeightLowHigh) {
            std::sort(sortData.begin(), sortData.end(), [](const auto& a, const auto& b) {
                float aVW = a.first->GetWeight() != 0 ? a.first->GetValue() / a.first->GetWeight() : 0;
                float bVW = b.first->GetWeight() != 0 ? b.first->GetValue() / b.first->GetWeight() : 0;
                return aVW < bVW;
            });
        }

        SKSE::log::info("Finalized SellList:");
        for (auto& [objDesc, count] : sortData) {
            if (!objDesc->object) {
                continue;
            }
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

        Count totalCount = 0;
        if (const auto preview = CaptureContainerPreview()) {
            totalCount = menuView == 0 ? preview->retrieveCount : preview->storeCount;
        }

        if (menuView == 0) {
            SKSE::log::info("Transfer Direction: Retrieve FROM container TO player");
            if (transferList.empty()) {
                SKSE::log::info("No Junk to retrieve!");
                RE::DebugMessageBox("No Junk to take!");
                operationInProgress.store(false);
                return;
            }

            if (Settings::ConfirmTransfer()) {
                SKSE::log::info("Retrieve {} Junk Items?", totalCount);
                std::string confirmText = Translation::Format("$JunkIt_RetrievalConfirmation", totalCount);
                ShowConfirmationMessageBox(
                    confirmText.c_str(),
                    { Translation::Get("$JunkIt_RetrieveConfirmYes"), Translation::Get("$JunkIt_ConfirmNo") },
                    [transferList, transferContainer, containerMode, menuView](unsigned int choice) {
                        if (choice == 0) {
                            SKSE::log::info("User confirmed retrieval");
                            ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                        } else {
                            SKSE::log::info("User cancelled retrieval");
                            SkyPromptIntegration::GetSingleton().RefreshPrompts();
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
                SKSE::log::info("Store {} Junk Items?", totalCount);
                std::string confirmText = Translation::Format("$JunkIt_TransferConfirmation", totalCount);
                ShowConfirmationMessageBox(
                    confirmText.c_str(),
                    { Translation::Get("$JunkIt_TransferConfirmYes"), Translation::Get("$JunkIt_ConfirmNo") },
                    [transferList, transferContainer, containerMode, menuView](unsigned int choice) {
                        if (choice == 0) {
                            SKSE::log::info("User confirmed transfer");
                            ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                        } else {
                            SKSE::log::info("User cancelled transfer");
                            SkyPromptIntegration::GetSingleton().RefreshPrompts();
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
                SendHUDMessage::ShowHUDMessage("JunkIt - Processing Retrieval...");
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
                SendHUDMessage::ShowHUDMessage(msg.c_str());
            }
        } else {
            SKSE::log::info("Transferring items to container...");
            if (Settings::GetNotifyOnJunkTransfer()) {
                SendHUDMessage::ShowHUDMessage("JunkIt - Processing Transfer...");
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
                        SendHUDMessage::ShowHUDMessage(msg.c_str());
                    } else {
                        SKSE::log::info("[NPC Mode] Transferred {} of {} possible junk items (NPC weight limit reached)", totalTransferred, totalPossibleTransferred);
                        std::string msg = fmt::format("JunkIt - Transferred {} Junk Items!", totalTransferred);
                        SendHUDMessage::ShowHUDMessage(msg.c_str());
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
                    SendHUDMessage::ShowHUDMessage(msg.c_str());
                }
            }
        }

        SKSE::log::info("---- Transfer Execution Complete ----");
        RefreshMenusAfterBulk(player, transferContainer, transferList.size(), totalTransferred);
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

        float vendorGoldDisplay = 0.0f;
        float sellMult = 0.0f;
        if (!TryReadBarterPrices(vendorGoldDisplay, sellMult)) {
            operationInProgress.store(false);
            return;
        }

        SKSE::log::info("Vendor Gold: {}", vendorGoldDisplay);
        SKSE::log::info("Vendor Sell Mult: {}", sellMult);

        auto totals = ComputeSellTotals(sellList, vendorGoldDisplay, sellMult);
        auto itemsToSell = std::move(totals.itemsToSell);
        Count totalToSell = totals.totalToSell;
        Count totalPossibleToSell = totals.totalPossibleToSell;
        Count roundedSellValue = totals.roundedSellValue;

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

        SKSE::log::info("Sale Summary: Selling {} items for {} gold", totalToSell, roundedSellValue);

        if (Settings::ConfirmSell()) {
            SKSE::log::info("Showing confirmation dialog for sale");
            std::string confirmText = Translation::Format("$JunkIt_SellConfirmationCount", totalToSell, roundedSellValue);
            ShowConfirmationMessageBox(confirmText.c_str(),
                { Translation::Get("$JunkIt_SellConfirmYes"), Translation::Get("$JunkIt_ConfirmNo") },
                [itemsToSell, vendorActorRef, vendorContainer, roundedSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay](unsigned int choice) {
                    if (choice == 0) {
                        SKSE::log::info("User confirmed sale");
                        ExecuteSell(itemsToSell, vendorActorRef, vendorContainer, roundedSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay);
                    } else {
                        SKSE::log::info("User cancelled sale");
                        SkyPromptIntegration::GetSingleton().RefreshPrompts();
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
            SendHUDMessage::ShowHUDMessage("JunkIt - Processing Sale...");
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
                SellEntryUnits(entryData, player, vendorContainer, count);
                SKSE::log::info("Transaction for {} {} complete", count, entryData->object->GetName());
            }
        }

        SKSE::log::info("Adding {} Speech experience", totalSellValue);
        player->AddSkillExperience(RE::ActorValue::kSpeech, static_cast<float>(totalSellValue));

        if (totalToSell >= totalPossibleToSell) {
            SKSE::log::info("Sold ALL {} Junk Items for {} Gold", totalToSell, totalSellValue);
            if (Settings::GetNotifyOnJunkSell()) {
                SendHUDMessage::ShowHUDMessage("JunkIt - Sold All Junk Items!");
            }
        } else {
            SKSE::log::info("Sold {} of {} Junk Items for {} Gold (vendor gold limit reached)", totalToSell, totalPossibleToSell, totalSellValue);
            if (Settings::GetNotifyOnJunkSell()) {
                std::string msg = fmt::format("JunkIt - Sold {} Junk Items!", totalToSell);
                SendHUDMessage::ShowHUDMessage(msg.c_str());
            }
        }

        RefreshMenusAfterBulk(player, vendorActorRef, itemsToSell.size(), totalToSell);
        if (vendorContainer && vendorContainer != vendorActorRef) {
            RE::SendUIMessage::SendInventoryUpdateMessage(vendorContainer, nullptr);
            if (auto* itemList = UIUtil::ItemList::GetOpenList()) {
                itemList->Update(vendorContainer);
            }
        }
        SKSE::log::info("---- Sale Execution Complete ----");
    }

    TESForm* JunkHandler::ToggleSelectedItemJunk() {
        ItemList* itemListMenu = UIUtil::ItemList::GetOpenList();
        if (!itemListMenu) {
            SKSE::log::error("No ItemListMenu found");
            SendHUDMessage::ShowHUDMessage("JunkIt - No item selected!");
            return nullptr;
        }

        ItemList::Item* selectedItem = itemListMenu->GetSelectedItem();
        if (!selectedItem) {
            SKSE::log::info("No item selected in ItemListMenu. Updating UI and trying again");
            itemListMenu->Update();

            selectedItem = itemListMenu->GetSelectedItem();
            if (!selectedItem) {
                SKSE::log::error("No item selected in ItemListMenu");
                SendHUDMessage::ShowHUDMessage("JunkIt - No item selected!");
                return nullptr;
            }
        }

        InventoryEntryData* inventoryEntry = selectedItem->data.objDesc;
        if (!inventoryEntry) {
            SKSE::log::error("Error getting InventoryEntryData for {}", selectedItem->data.objDesc->GetDisplayName());
            SendHUDMessage::ShowHUDMessage("JunkIt - Failed to mark item as junk!");
            return nullptr;
        }

        TESBoundObject* itemObject = inventoryEntry->object;
        if (!itemObject) {
            SKSE::log::error("Error getting item as object for {}", inventoryEntry->GetDisplayName());
            SendHUDMessage::ShowHUDMessage("JunkIt - Failed to mark item as junk!");
            return nullptr;
        }

        TESForm* itemForm = itemObject->As<TESForm>();
        if (!itemForm) {
            SKSE::log::error("Error getting item as form for {}", inventoryEntry->GetDisplayName());
            SendHUDMessage::ShowHUDMessage("JunkIt - Failed to mark item as junk!");
            return nullptr;
        }

        std::string itemName = itemForm->GetName();
        std::string hexFormId = FormUtil::Form::GetFormConfigString(itemForm);

        bool playerOwned = false;
        if (auto* player = PlayerCharacter::GetSingleton()) {
            playerOwned = selectedItem->data.owner == player->GetHandle().native_handle();
        }

        if (inventoryEntry->IsQuestObject()) {
            SKSE::log::info("Cannot mark quest item {} [{}] as junk", itemName, hexFormId);
            auto& junkManager = JunkDataManager::GetSingleton();
            if (!junkManager.IsJunk(inventoryEntry)) {
                SendHUDMessage::ShowHUDMessage("JunkIt - Quest Items cannot be marked as Junk");
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
                std::string confirmText = Translation::Format("$JunkIt_MarkProtectedConfirm", protectionReason);
                ShowConfirmationMessageBox(confirmText.c_str(),
                    { Translation::Get("$JunkIt_Yes"), Translation::Get("$JunkIt_ConfirmNo") },
                    [inventoryEntry, itemForm, itemName, hexFormId, playerOwned](unsigned int choice) {
                        if (choice == 0) {
                            SKSE::log::info("User confirmed marking protected item as junk");
                            SKSE::log::info("Adding junk status to {} [{}]", itemName, hexFormId);
                            auto& junkManager = JunkDataManager::GetSingleton();
                            const auto addedIdentity = junkManager.AddJunkItem(inventoryEntry);

                            if (junkManager.IsJunk(inventoryEntry)) {
                                SkyPromptIntegration::GetSingleton().OnJunkToggled(inventoryEntry, true, playerOwned);
                            }

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
                                SendHUDMessage::ShowHUDMessage(msg.c_str());
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

        if (isJunk != isNowJunk) {
            SkyPromptIntegration::GetSingleton().OnJunkToggled(inventoryEntry, isNowJunk, playerOwned);
        }

        itemListMenu->Update();

        if (isNowJunk) {
            if (junkIdentity) {
                SKSE::log::info("Form marked as junk: {}", *junkIdentity);
            } else {
                SKSE::log::warn("Form marked as junk but no identity was returned for {}", itemForm->GetName());
            }
            if (Settings::GetNotifyOnMarkUnmark()) {
                std::string msg = fmt::format("JunkIt - {} has been marked as junk", itemForm->GetName());
                SendHUDMessage::ShowHUDMessage(msg.c_str());
            }
        } else {
            SKSE::log::info("Form: {} is no longer marked as junk", itemForm->GetName());
            if (Settings::GetNotifyOnMarkUnmark()) {
                std::string msg = fmt::format("JunkIt - {} is no longer marked as junk", itemForm->GetName());
                SendHUDMessage::ShowHUDMessage(msg.c_str());
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

    std::int32_t JunkHandler::GetMenuItemValue(InventoryEntryData* a_entry) {
        if (!a_entry || !a_entry->object) {
            return -1;
        }

        ItemList* itemListMenu = UIUtil::ItemList::GetOpenList();
        if (!itemListMenu) {
            SKSE::log::error("No ItemListMenu found");
            return -1;
        }

        BSTArray<ItemList::Item*> listItems = itemListMenu->items;
        for (std::uint32_t i = 0, size = listItems.size(); i < size; i++) {
            ItemList::Item* entryItem = listItems[i];
            if (!entryItem || !entryItem->data.objDesc) {
                continue;
            }
            if (entryItem->data.objDesc == a_entry) {
                const std::int32_t goldValue = a_entry->GetValue();
                SKSE::log::info("          Value Per Item = {} gold", goldValue);
                return goldValue;
            }
        }

        return GetMenuItemValue(a_entry->object->As<TESForm>());
    }

    std::int32_t JunkHandler::GetMenuItemValue(TESForm* a_form) {
        std::int32_t goldValue = -1;
        if (!a_form) {
            return goldValue;
        }

        ItemList* itemListMenu = UIUtil::ItemList::GetOpenList();
        if (!itemListMenu) {
            SKSE::log::error("No ItemListMenu found");
            return goldValue;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        const auto playerHandle = player ? player->GetHandle().native_handle() : 0;

        BSTArray<ItemList::Item*> listItems = itemListMenu->items;
        InventoryEntryData* formFallback = nullptr;

        for (std::uint32_t i = 0, size = listItems.size(); i < size; i++) {
            ItemList::Item* entryItem = listItems[i];
            if (!entryItem) {
                continue;
            }

            InventoryEntryData* entryData = entryItem->data.objDesc;
            if (!entryData || !entryData->object) {
                continue;
            }

            if (entryData->object->GetFormID() != a_form->GetFormID()) {
                continue;
            }

            if (player && entryItem->data.owner == playerHandle) {
                goldValue = entryData->GetValue();
                SKSE::log::info("          Value Per Item = {} gold", goldValue);
                return goldValue;
            }

            if (!formFallback) {
                formFallback = entryData;
            }
        }

        if (formFallback) {
            goldValue = formFallback->GetValue();
            SKSE::log::info("          Value Per Item = {} gold", goldValue);
        }

        return goldValue;
    }
}
