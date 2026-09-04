#include "junk.h"
#include "I4Integration.h"
#include "InventoryWalk.h"
#include "JunkData.h"
#include "OperationOverlay.h"
#include "SendUIMessage.h"
#include "SkyPromptIntegration.h"
#include "Translation.h"
#include "UI.h"
#include <SKSE/API.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <unordered_map>
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
            if (auto inventoryMenu = ui->GetMenu<InventoryMenu>()) {
                if (inventoryMenu->uiMovie) {
                    return inventoryMenu->uiMovie.get();
                }
            }
            return nullptr;
        }

        TESObjectREFR* GetOpenMenuListSecondary() {
            const auto ui = RE::UI::GetSingleton();
            if (!ui) {
                return nullptr;
            }
            if (ui->IsMenuOpen("ContainerMenu")) {
                const auto menu = ui->GetMenu<ContainerMenu>();
                if (!menu) {
                    return nullptr;
                }
                TESObjectREFRPtr refr;
                LookupReferenceByHandle(menu->GetTargetRefHandle(), refr);
                return refr.get();
            }
            if (ui->IsMenuOpen("BarterMenu")) {
                return UIUtil::Menu::GetBarterMenuTargetRef();
            }
            return nullptr;
        }

        TESObjectREFR* GetVisibleListOwner(TESObjectREFR* primary, TESObjectREFR* secondary) {
            const auto ui = RE::UI::GetSingleton();
            if (ui && ui->IsMenuOpen("InventoryMenu")) {
                return primary;
            }

            if (ui && (ui->IsMenuOpen("ContainerMenu") || ui->IsMenuOpen("BarterMenu"))) {
                auto* movie = GetOpenInventoryMovie();
                RE::GFxValue result;
                if (movie &&
                    movie->GetVariable(&result, "_root.Menu_mc.inventoryLists.categoryList.activeSegment") &&
                    result.IsNumber() &&
                    static_cast<int>(result.GetNumber()) != 0) {
                    return primary;
                }
                return secondary ? secondary : primary;
            }

            return secondary ? secondary : primary;
        }

        bool InventoryLikeMenuOpen() {
            auto* ui = RE::UI::GetSingleton();
            if (!ui) {
                return false;
            }
            return ui->IsMenuOpen("InventoryMenu") || ui->IsMenuOpen("ContainerMenu") || ui->IsMenuOpen("BarterMenu");
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

        void DrainInventoryUIUpdates(TESObjectREFR* primary, TESObjectREFR* secondary) {
            SendInventoryUpdate(primary);
            if (secondary && secondary != primary) {
                SendInventoryUpdate(secondary);
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

        void RefreshJunkListIcons(ItemList* itemList, TESBoundObject* object, RefHandle owner, bool isNowJunk) {
            if (itemList && object) {
                for (std::uint32_t i = 0, size = itemList->items.size(); i < size; i++) {
                    auto* item = itemList->items[i];
                    if (!item || !item->data.objDesc || item->data.owner != owner) {
                        continue;
                    }
                    if (item->data.objDesc->object != object) {
                        continue;
                    }
                    I4Integration::SetJunkFlags(item->obj, isNowJunk);
                }
            }
            InvalidateInventoryLists(GetOpenInventoryMovie());
        }

        struct PreviewStack {
            InventoryEntryData* entry = nullptr;
            std::int32_t count = 0;
            float weight = 0.0f;
            std::int32_t value = 0;
        };

        std::int32_t FitCountToCarryWeight(std::int32_t count, float itemWeight, float currentWeight, float maxWeight) {
            if (count <= 0) {
                return 0;
            }
            if (!(itemWeight > 0.0f)) {
                return currentWeight > maxWeight ? 0 : count;
            }

            const float remaining = maxWeight - currentWeight;
            const float maxFitF = remaining > 0.0f ? std::floor(remaining / itemWeight) : 0.0f;

            std::int32_t n = 0;
            if (maxFitF >= static_cast<float>(count)) {
                n = count;
            } else if (maxFitF > 0.0f) {
                n = static_cast<std::int32_t>(maxFitF);
            }

            // Keep floor() aligned with the old (weight * n + current) > max comparison.
            if (n > 0 && (itemWeight * static_cast<float>(n)) + currentWeight > maxWeight) {
                n -= 1;
            } else if (n < count && (itemWeight * static_cast<float>(n + 1)) + currentWeight <= maxWeight) {
                n += 1;
            }
            return n;
        }

        void SortPreviewStacks(std::vector<PreviewStack>& stacks, Settings::SortPriority priority) {
            if (priority == Settings::SortPriority::kChaos) {
                return;
            }

            const bool needWeight =
                priority == Settings::SortPriority::kWeightHighLow ||
                priority == Settings::SortPriority::kWeightLowHigh ||
                priority == Settings::SortPriority::kValueWeightHighLow ||
                priority == Settings::SortPriority::kValueWeightLowHigh;
            const bool needValue =
                priority == Settings::SortPriority::kValueHighLow ||
                priority == Settings::SortPriority::kValueLowHigh ||
                priority == Settings::SortPriority::kValueWeightHighLow ||
                priority == Settings::SortPriority::kValueWeightLowHigh;

            for (auto& stack : stacks) {
                if (!stack.entry) {
                    continue;
                }
                if (needWeight) {
                    stack.weight = stack.entry->GetWeight();
                }
                if (needValue) {
                    stack.value = stack.entry->GetValue();
                }
            }

            const auto valueWeight = [](const PreviewStack& stack) {
                return stack.weight != 0.0f ? static_cast<float>(stack.value) / stack.weight : 0.0f;
            };

            switch (priority) {
                case Settings::SortPriority::kWeightHighLow:
                    std::sort(stacks.begin(), stacks.end(), [](const PreviewStack& a, const PreviewStack& b) {
                        return a.weight > b.weight;
                    });
                    break;
                case Settings::SortPriority::kWeightLowHigh:
                    std::sort(stacks.begin(), stacks.end(), [](const PreviewStack& a, const PreviewStack& b) {
                        return a.weight < b.weight;
                    });
                    break;
                case Settings::SortPriority::kValueHighLow:
                    std::sort(stacks.begin(), stacks.end(), [](const PreviewStack& a, const PreviewStack& b) {
                        return a.value > b.value;
                    });
                    break;
                case Settings::SortPriority::kValueLowHigh:
                    std::sort(stacks.begin(), stacks.end(), [](const PreviewStack& a, const PreviewStack& b) {
                        return a.value < b.value;
                    });
                    break;
                case Settings::SortPriority::kValueWeightHighLow:
                    std::sort(stacks.begin(), stacks.end(), [&](const PreviewStack& a, const PreviewStack& b) {
                        return valueWeight(a) > valueWeight(b);
                    });
                    break;
                case Settings::SortPriority::kValueWeightLowHigh:
                    std::sort(stacks.begin(), stacks.end(), [&](const PreviewStack& a, const PreviewStack& b) {
                        return valueWeight(a) < valueWeight(b);
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

        std::uint16_t ExtraUniqueIDValue(const ExtraDataList* extraList) {
            if (!extraList) {
                return 0;
            }
            const auto* extraId = extraList->GetByType<RE::ExtraUniqueID>();
            return extraId ? extraId->uniqueID : 0;
        }

        std::uint16_t CaptureSelectedUniqueID(InventoryEntryData* entry, std::int32_t rowCount) {
            if (!entry || !entry->extraLists) {
                return 0;
            }

            ExtraDataList* first = nullptr;
            ExtraDataList* countMatch = nullptr;
            int extraCount = 0;
            int countMatches = 0;
            for (auto* extra : *entry->extraLists) {
                if (!extra) {
                    continue;
                }
                if (extraCount == 0) {
                    first = extra;
                }
                ++extraCount;
                if (static_cast<std::int32_t>(extra->GetCount()) == rowCount) {
                    ++countMatches;
                    countMatch = extra;
                }
            }

            ExtraDataList* chosen = nullptr;
            if (extraCount == 1) {
                chosen = first;
            } else if (countMatches == 1) {
                chosen = countMatch;
            }
            return ExtraUniqueIDValue(chosen);
        }

        ExtraDataList* FindExtraByUniqueID(TESObjectREFR* from, TESBoundObject* item, std::uint16_t uniqueID) {
            if (!from || !item) {
                return nullptr;
            }

            ExtraDataList* matched = nullptr;
            ExtraDataList* only = nullptr;
            int extraCount = 0;

            ForEachInventoryEntry(from, [&](InventoryEntryData* entry) {
                if (!entry || entry->object != item || !entry->extraLists) {
                    return;
                }
                for (auto* extra : *entry->extraLists) {
                    if (!extra) {
                        continue;
                    }
                    if (uniqueID != 0 && !matched && ExtraListMatchesUniqueID(extra, uniqueID)) {
                        matched = extra;
                    }
                    if (extraCount == 0) {
                        only = extra;
                    }
                    ++extraCount;
                }
            });

            if (uniqueID != 0) {
                return matched;
            }
            return extraCount == 1 ? only : nullptr;
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

        int HeavyLoadDeferredFrames() {
            return std::clamp(static_cast<int>(std::lround(2.0f * Settings::GetHeavyLoadDelayMultiplier())), 1, 10);
        }

        constexpr int kUiMessageQueueSize = 64;

        int BulkRefreshDrainFrames(std::size_t uniqueTypes, bool largeOp) {
            if (!largeOp) {
                return 1;
            }

            int drainFrames = HeavyLoadDeferredFrames();
            if (uniqueTypes > kUiMessageQueueSize) {
                const int extra = static_cast<int>(
                    (uniqueTypes - kUiMessageQueueSize + kUiMessageQueueSize - 1) / kUiMessageQueueSize);
                drainFrames = std::clamp(drainFrames + extra, 1, 20);
            }
            return drainFrames;
        }

        struct EntryJunkScan {
            std::int32_t junkCount = 0;
            bool fullyJunk = false;
            bool plainIsJunk = false;
            std::vector<std::pair<ExtraDataList*, std::int32_t>> junkExtras;
        };

        EntryJunkScan ScanEntryJunk(InventoryEntryData* entry, bool collectExtras) {
            EntryJunkScan result;
            if (!entry || !entry->object) {
                return result;
            }

            auto& junkManager = JunkDataManager::GetSingleton();
            if (!junkManager.IsAnyJunkForForm(entry->object)) {
                return result;
            }

            const auto base = JunkDataManager::CaptureIdentityBase(entry->object, entry->GetDisplayName());
            if (!base) {
                return result;
            }

            const auto extraIsJunk = [&](const ExtraDataList* extraList) {
                return junkManager.IsJunk(JunkDataManager::BuildIdentity(*base, extraList));
            };

            if (!entry->extraLists || entry->extraLists->empty()) {
                result.plainIsJunk = extraIsJunk(nullptr);
                result.fullyJunk = result.plainIsJunk;
                if (result.plainIsJunk && entry->countDelta > 0) {
                    result.junkCount = entry->countDelta;
                }
                return result;
            }

            std::int32_t extrasTotal = 0;
            std::int32_t junkExtraCount = 0;
            bool extrasAllJunk = true;
            for (auto* extraList : *entry->extraLists) {
                if (!extraList) {
                    continue;
                }
                const std::int32_t extraCount = extraList->GetCount();
                extrasTotal += extraCount;
                if (!extraIsJunk(extraList)) {
                    extrasAllJunk = false;
                    continue;
                }
                junkExtraCount += extraCount;
                if (collectExtras && extraCount > 0) {
                    result.junkExtras.emplace_back(extraList, extraCount);
                }
            }

            const std::int32_t plain = entry->countDelta - extrasTotal;
            result.plainIsJunk = extraIsJunk(nullptr);
            if (plain > 0 && result.plainIsJunk) {
                junkExtraCount += plain;
            }
            result.junkCount = junkExtraCount > 0 ? junkExtraCount : 0;
            if (plain > 0) {
                result.fullyJunk = extrasAllJunk && result.plainIsJunk;
            } else {
                result.fullyJunk = extrasAllJunk && extrasTotal > 0;
            }
            return result;
        }

        std::int32_t MeasureBarterSellCount(ItemList::Item* entryItem) {
            if (!entryItem || !entryItem->data.objDesc) {
                return 0;
            }

            const auto scan = ScanEntryJunk(entryItem->data.objDesc, false);
            std::int32_t count = scan.junkCount;
            if (scan.fullyJunk) {
                const auto uiCount = static_cast<std::int32_t>(entryItem->data.GetCount());
                if (uiCount > count) {
                    count = uiCount;
                }
            }
            return count > 0 ? count : 0;
        }
    }

    void JunkHandler::ApplyInventoryUIRefresh(TESObjectREFR* primary, TESObjectREFR* secondary) {
        ItemList* itemList = UIUtil::ItemList::GetOpenList();
        if (!itemList) {
            return;
        }

        auto* movie = GetOpenInventoryMovie();

        SendInventoryUpdate(primary);
        if (secondary && secondary != primary) {
            SendInventoryUpdate(secondary);
        }

        UpdateItemListOwner(itemList, GetVisibleListOwner(primary, secondary));
        InvalidateInventoryLists(movie);
    }

    void JunkHandler::ScheduleInventoryUIRefresh(FormID primaryId, FormID secondaryId, int framesRemaining, std::function<void()> onComplete, bool rebuildList) {
        auto* taskInterface = SKSE::GetTaskInterface();
        if (!taskInterface) {
            if (onComplete) {
                onComplete();
            }
            return;
        }

        taskInterface->AddUITask([primaryId, secondaryId, framesRemaining, onComplete = std::move(onComplete), rebuildList]() mutable {
            if (framesRemaining > 0) {
                ScheduleInventoryUIRefresh(primaryId, secondaryId, framesRemaining - 1, std::move(onComplete), rebuildList);
                return;
            }

            auto* primary = LookupRefr(primaryId);
            auto* secondary = LookupRefr(secondaryId);
            if (primary || secondary) {
                if (rebuildList) {
                    ApplyInventoryUIRefresh(primary, secondary);
                } else {
                    DrainInventoryUIUpdates(primary, secondary);
                }
            }

            if (!onComplete) {
                return;
            }

            if (auto* tasks = SKSE::GetTaskInterface()) {
                tasks->AddUITask(std::move(onComplete));
            } else {
                onComplete();
            }
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
                auto* player = PlayerCharacter::GetSingleton();
                UpdateItemListOwner(
                    UIUtil::ItemList::GetOpenList(),
                    GetVisibleListOwner(player, GetOpenMenuListSecondary()));
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
        const bool largeOp = uniqueTypes >= static_cast<std::size_t>(kUiMessageQueueSize)
            || uniqueTypes >= Settings::GetLargeUniqueTypes();
        const int drainFrames = BulkRefreshDrainFrames(uniqueTypes, largeOp);

        SKSE::log::info(
            "Bulk UI refresh | uniqueTypes={} totalItems={} large={} drainFrames={}",
            uniqueTypes,
            totalItems,
            largeOp,
            drainFrames);

        const FormID primaryId = primary ? primary->GetFormID() : 0;
        const FormID secondaryId = secondary ? secondary->GetFormID() : 0;
        ScheduleInventoryUIRefresh(primaryId, secondaryId, drainFrames, CompleteOperation);

        StartAggressiveRefresh();
    }

    void JunkHandler::CompleteOperation() {
        OperationOverlay::NotifyWorkComplete([] {
            operationInProgress.store(false);
            SkyPromptIntegration::GetSingleton().RecapturePreviews();
        });
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

    JunkHandler::Count JunkHandler::ComputeUnitSellPrice(InventoryEntryData* entry, float sellMult) {
        if (!entry) {
            return 0;
        }

        const Count unitPrice = RoundNumber(static_cast<float>(entry->GetValue()) * sellMult);
        return unitPrice > 0 ? unitPrice : 0;
    }

    JunkHandler::Count JunkHandler::ComputePricedStackGold(
        const std::vector<SellPreviewStack>& stacks,
        float vendorGold) {
        Count remainingVendorGold = RoundNumber(vendorGold);
        if (remainingVendorGold < 0) {
            remainingVendorGold = 0;
        }

        Count totalSellValue = 0;
        for (const auto& stack : stacks) {
            if (stack.count <= 0 || stack.unitPrice <= 0) {
                continue;
            }

            Count take = stack.count;
            const Count maxAfford = remainingVendorGold / stack.unitPrice;
            if (take > maxAfford) {
                take = maxAfford;
            }
            if (take <= 0) {
                continue;
            }

            remainingVendorGold -= stack.unitPrice * take;
            totalSellValue += stack.unitPrice * take;
        }
        return totalSellValue;
    }

    std::optional<std::int32_t> JunkHandler::ComputeSellPreviewGold(const std::vector<SellPreviewStack>& stacks) {
        float vendorGold = 0.0f;
        float sellMult = 0.0f;
        if (!TryReadBarterPrices(vendorGold, sellMult)) {
            return std::nullopt;
        }
        (void)sellMult;
        return ComputePricedStackGold(stacks, vendorGold);
    }

    JunkHandler::SellTotals JunkHandler::ComputeSellTotals(
        const std::vector<std::pair<InventoryEntryData*, Count>>& sellList,
        float vendorGold,
        float sellMult) {
        SellTotals totals;
        Count remainingVendorGold = RoundNumber(vendorGold);
        if (remainingVendorGold < 0) {
            remainingVendorGold = 0;
        }
        Count totalSellValue = 0;

        for (auto& [entryData, itemCount] : sellList) {
            if (!entryData || !entryData->object || itemCount <= 0) {
                continue;
            }

            Count iCount = itemCount;
            totals.totalPossibleToSell += iCount;

            const Count unitPrice = ComputeUnitSellPrice(entryData, sellMult);
            if (unitPrice <= 0) {
                totals.totalToSell += iCount;
                totals.itemsToSell.push_back({entryData, iCount});
                continue;
            }

            const Count maxAfford = remainingVendorGold / unitPrice;
            if (iCount > maxAfford) {
                iCount = maxAfford;
            }

            if (iCount > 0) {
                remainingVendorGold -= unitPrice * iCount;
                totalSellValue += unitPrice * iCount;
                totals.totalToSell += iCount;
                totals.itemsToSell.push_back({entryData, iCount});
            }
        }

        totals.roundedSellValue = totalSellValue;
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
        template <class Passes, class Sellable>
        void FillPreviewStacks(
            TESObjectREFR* container,
            bool sellFilters,
            std::vector<PreviewStack>& out,
            Passes passesFilters,
            Sellable sellableCount) {
            if (!container) {
                return;
            }
            ForEachInventoryEntry(container, [&](InventoryEntryData* entry) {
                if (!entry || !entry->object || !passesFilters(entry, sellFilters)) {
                    return;
                }
                const auto count = sellableCount(entry);
                if (count > 0) {
                    out.push_back({ entry, count });
                }
            });
        }
    }

    std::optional<JunkHandler::ContainerPreviewCounts> JunkHandler::CaptureContainerPreview(ContainerPreviewSide side) {
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

        ContainerPreviewCounts preview;

        if (side != ContainerPreviewSide::Store) {
            std::vector<PreviewStack> retrieveStacks;
            FillPreviewStacks(container, false, retrieveStacks, passes, sellable);
            for (const auto& stack : retrieveStacks) {
                preview.retrieveCount += stack.count;
            }
        }

        if (side == ContainerPreviewSide::Retrieve) {
            return preview;
        }

        std::vector<PreviewStack> storeStacks;
        FillPreviewStacks(player, false, storeStacks, passes, sellable);

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
                iCount = FitCountToCarryWeight(iCount, itemWeight, currentWeight, maxWeight);

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

        float vendorGold = 0.0f;
        float sellMult = 0.0f;
        if (!TryReadBarterPrices(vendorGold, sellMult)) {
            return capture;
        }
        capture.sellMult = sellMult;

        auto* player = PlayerCharacter::GetSingleton();
        const auto ui = RE::UI::GetSingleton();
        auto barterMenu = ui ? ui->GetMenu<BarterMenu>() : nullptr;
        ItemList* itemListMenu = barterMenu ? barterMenu->GetRuntimeData().itemList : nullptr;
        if (!player || !itemListMenu || itemListMenu->items.empty()) {
            return capture;
        }

        capture.pricesReady = true;
        const auto playerHandle = player->GetHandle().native_handle();
        std::vector<PreviewStack> sortStacks;
        const auto& items = itemListMenu->items;
        for (std::uint32_t i = 0, size = items.size(); i < size; i++) {
            ItemList::Item* entryItem = items[i];
            if (!entryItem || !entryItem->data.objDesc) {
                continue;
            }
            if (entryItem->data.owner != playerHandle) {
                continue;
            }

            InventoryEntryData* objDesc = entryItem->data.objDesc;
            if (!objDesc->object || !EntryPassesPreviewFilters(objDesc, true)) {
                continue;
            }

            const Count count = MeasureBarterSellCount(entryItem);
            if (count > 0) {
                sortStacks.push_back({ objDesc, count });
            }
        }

        SortPreviewStacks(sortStacks, Settings::GetSellPriority());
        capture.stacks.reserve(sortStacks.size());
        for (const auto& stack : sortStacks) {
            capture.stacks.push_back({ stack.count, ComputeUnitSellPrice(stack.entry, sellMult), stack.entry });
        }

        const Count roundedSellValue = ComputePricedStackGold(capture.stacks, vendorGold);
        if (roundedSellValue > 0) {
            capture.gold = roundedSellValue;
        }
        return capture;
    }

    bool JunkHandler::TryPatchSellPreviewStacks(
        std::vector<SellPreviewStack>& stacks,
        InventoryEntryData* entry) {
        if (!entry) {
            return false;
        }

        std::unordered_map<InventoryEntryData*, SellPreviewStack> byEntry;
        byEntry.reserve(stacks.size() + 1);
        for (const auto& stack : stacks) {
            if (!stack.entry) {
                return false;
            }
            if (!byEntry.emplace(stack.entry, stack).second) {
                return false;
            }
        }

        float vendorGold = 0.0f;
        float sellMult = 0.0f;
        if (!TryReadBarterPrices(vendorGold, sellMult)) {
            return false;
        }
        (void)vendorGold;

        auto* player = PlayerCharacter::GetSingleton();
        const auto ui = RE::UI::GetSingleton();
        auto barterMenu = ui ? ui->GetMenu<BarterMenu>() : nullptr;
        ItemList* itemListMenu = barterMenu ? barterMenu->GetRuntimeData().itemList : nullptr;
        if (!player || !itemListMenu) {
            return false;
        }

        const auto playerHandle = player->GetHandle().native_handle();
        ItemList::Item* matchedItem = nullptr;
        const auto& items = itemListMenu->items;
        for (std::uint32_t i = 0, size = items.size(); i < size; i++) {
            ItemList::Item* entryItem = items[i];
            if (!entryItem || entryItem->data.objDesc != entry) {
                continue;
            }
            if (entryItem->data.owner != playerHandle) {
                continue;
            }
            matchedItem = entryItem;
            break;
        }
        if (!matchedItem) {
            return false;
        }

        SellPreviewStack measured;
        measured.entry = entry;
        if (entry->object && EntryPassesPreviewFilters(entry, true)) {
            measured.count = MeasureBarterSellCount(matchedItem);
            if (measured.count > 0) {
                measured.unitPrice = ComputeUnitSellPrice(entry, sellMult);
            }
        }
        if (measured.count > 0) {
            byEntry[entry] = measured;
        } else {
            byEntry.erase(entry);
        }

        std::vector<PreviewStack> ordered;
        ordered.reserve(byEntry.size());
        std::size_t seen = 0;
        for (std::uint32_t i = 0, size = items.size(); i < size; i++) {
            ItemList::Item* entryItem = items[i];
            if (!entryItem || !entryItem->data.objDesc) {
                continue;
            }
            if (entryItem->data.owner != playerHandle) {
                continue;
            }
            auto it = byEntry.find(entryItem->data.objDesc);
            if (it == byEntry.end()) {
                continue;
            }
            ordered.push_back({ it->first, it->second.count });
            ++seen;
        }
        if (seen != byEntry.size()) {
            return false;
        }

        SortPreviewStacks(ordered, Settings::GetSellPriority());

        stacks.clear();
        stacks.reserve(ordered.size());
        for (const auto& stack : ordered) {
            const auto it = byEntry.find(stack.entry);
            if (it == byEntry.end()) {
                return false;
            }
            stacks.push_back({ stack.count, it->second.unitPrice, stack.entry });
        }
        return true;
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
        bool sellFilters,
        TESBoundObject* objectFilter) {
        if (!container || identities.empty()) {
            return 0;
        }

        std::unordered_set<std::string> identitySet(identities.begin(), identities.end());
        identitySet.erase("");
        if (identitySet.empty()) {
            return 0;
        }

        Count total = 0;
        ForEachInventoryEntry(container, [&](InventoryEntryData* entry) {
            if (!entry || !entry->object) {
                return;
            }
            if (objectFilter && entry->object != objectFilter) {
                return;
            }
            if (!EntryPassesPreviewFilters(entry, sellFilters)) {
                return;
            }
            total += CountIdentityUnits(entry, identitySet);
        });
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

        bool found = false;
        ForEachInventoryEntry(dest, [&](InventoryEntryData* entry) {
            if (found || !entry || entry->object != bound) {
                return;
            }
            if (EntryPassesPreviewFilters(entry, sellFilters) && EntryMatchesMovedUniqueID(entry, uniqueID)) {
                found = true;
            }
        });
        return found;
    }

    std::int32_t JunkHandler::CountJunkUnits(InventoryEntryData* entry) {
        return GetSellableJunkCount(entry);
    }

    std::optional<JunkHandler::JunkPreviewUnit> JunkHandler::LookupJunkPreviewUnit(
        TESObjectREFR* dest,
        FormID baseObj,
        std::uint16_t uniqueID) {
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

        InventoryEntryData* entry = nullptr;
        ForEachInventoryEntry(dest, [&](InventoryEntryData* candidate) {
            if (entry || !candidate || candidate->object != bound) {
                return;
            }
            if (!candidate->IsQuestObject() && EntryMatchesMovedUniqueID(candidate, uniqueID)) {
                entry = candidate;
            }
        });
        if (!entry) {
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
        return unit;
    }

    void JunkHandler::MoveItems(TESBoundObject* a_item, TESObjectREFR* a_from, TESObjectREFR* a_to, ITEM_REMOVE_REASON a_reason, Count a_count, ExtraDataList* a_extraList) {
        if (!a_item || !a_from || !a_to || a_count <= 0) {
            return;
        }
        a_from->RemoveItem(a_item, a_count, a_reason, a_extraList, a_to);
    }

    JunkHandler::Count JunkHandler::GetSellableJunkCount(InventoryEntryData* a_entry) {
        return ScanEntryJunk(a_entry, false).junkCount;
    }

    bool JunkHandler::EntryIsFullyJunk(InventoryEntryData* a_entry) {
        return ScanEntryJunk(a_entry, false).fullyJunk;
    }

    void JunkHandler::SellEntryUnits(InventoryEntryData* a_entry, TESObjectREFR* a_from, TESObjectREFR* a_to, Count a_count) {
        if (!a_entry || !a_entry->object || !a_from || !a_to || a_count <= 0) {
            return;
        }

        const auto scan = ScanEntryJunk(a_entry, true);
        if (scan.fullyJunk) {
            MoveItems(a_entry->object, a_from, a_to, ITEM_REMOVE_REASON::kSelling, a_count, nullptr);
            return;
        }

        Count remaining = a_count;
        for (const auto& [extraList, extraCount] : scan.junkExtras) {
            if (remaining <= 0) {
                break;
            }
            const Count toSell = std::min(remaining, extraCount);
            MoveItems(a_entry->object, a_from, a_to, ITEM_REMOVE_REASON::kSelling, toSell, extraList);
            remaining -= toSell;
        }

        if (remaining > 0 && scan.plainIsJunk) {
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

        const auto& listItems = itemListMenu->items;
        std::vector<PreviewStack> sortFormData;

        SKSE::log::info("Processing Entry List for transferable junk items");
        auto& junkManager = JunkDataManager::GetSingleton();

        for (std::uint32_t i = 0, size = listItems.size(); i < size; i++) {
            ItemList::Item* entryItem = listItems[i];
            if (!entryItem) continue;

            if (!junkManager.IsJunk(entryItem->data.objDesc)) continue;

            if (entryItem->data.objDesc->IsQuestObject()) {
                if (spdlog::should_log(spdlog::level::debug)) {
                    SKSE::log::debug("Junk Item is Quest Item - Skipping {}", entryItem->data.objDesc->object->GetName());
                }
                continue;
            }
            
            if (Settings::ProtectEquipped() && entryItem->data.objDesc->IsWorn()) {
                if (spdlog::should_log(spdlog::level::debug)) {
                    SKSE::log::debug("Junk Item Equipped - Skipping {}", entryItem->data.objDesc->object->GetName());
                }
                continue;
            }
            if (Settings::ProtectFavorites() && entryItem->data.objDesc->IsFavorited()) {
                if (spdlog::should_log(spdlog::level::debug)) {
                    SKSE::log::debug("Junk Item Favorited - Skipping {}", entryItem->data.objDesc->object->GetName());
                }
                continue;
            }

            sortFormData.push_back({ entryItem->data.objDesc, 0 });
        }

        SortPreviewStacks(sortFormData, Settings::GetTransferPriority());

        for (const auto& stack : sortFormData) {
            if (!stack.entry || !stack.entry->object) continue;
            transferList.push_back(stack.entry);
        }
        SKSE::log::info("Finalized TransferList: {} items", transferList.size());
        if (spdlog::should_log(spdlog::level::debug)) {
            for (InventoryEntryData* entryData : transferList) {
                SKSE::log::debug(
                    "     {} [{}]",
                    entryData->object->GetName(),
                    FormUtil::Form::GetFormConfigString(entryData->object->As<TESForm>()));
            }
        }

        SKSE::log::info("---- Completed Junk Transfer List Generation ----");
        SKSE::log::info(" ");
        return transferList;
    }

    std::vector<std::pair<InventoryEntryData*, std::int32_t>> JunkHandler::BuildSellList(bool allowUiCountBoost) {
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
        const auto& listItems = itemListMenu->items;
        std::vector<PreviewStack> sortData;

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
                if (spdlog::should_log(spdlog::level::debug)) {
                    SKSE::log::debug("Junk Item is Quest Item - Skipping {}", objDesc->object->GetName());
                }
                continue;
            }

            if (Settings::ProtectEquipped() && objDesc->IsWorn()) {
                if (spdlog::should_log(spdlog::level::debug)) {
                    SKSE::log::debug("Junk Item Equipped - Skipping {}", objDesc->object->GetName());
                }
                continue;
            }
            if (Settings::ProtectFavorites() && objDesc->IsFavorited()) {
                if (spdlog::should_log(spdlog::level::debug)) {
                    SKSE::log::debug("Junk Item Favorited - Skipping {}", objDesc->object->GetName());
                }
                continue;
            }
            if (Settings::ProtectEnchanted() && objDesc->IsEnchanted()) {
                if (spdlog::should_log(spdlog::level::debug)) {
                    SKSE::log::debug("Junk Item Enchanted - Skipping {}", objDesc->object->GetName());
                }
                continue;
            }

            const auto scan = ScanEntryJunk(objDesc, false);
            Count count = scan.junkCount;
            if (allowUiCountBoost && scan.fullyJunk) {
                const Count uiCount = static_cast<Count>(entryItem->data.GetCount());
                if (uiCount > count) {
                    count = uiCount;
                }
            }
            if (count <= 0) {
                continue;
            }

            sortData.push_back({ objDesc, count });
        }

        SortPreviewStacks(sortData, Settings::GetSellPriority());

        for (const auto& stack : sortData) {
            if (!stack.entry || !stack.entry->object) {
                continue;
            }
            sellList.push_back({ stack.entry, stack.count });
        }
        SKSE::log::info("Finalized SellList: {} items", sellList.size());
        if (spdlog::should_log(spdlog::level::debug)) {
            for (auto& [objDesc, count] : sellList) {
                SKSE::log::debug(
                    "     {} x{} [{}]",
                    objDesc->object->GetName(),
                    count,
                    FormUtil::Form::GetFormConfigString(objDesc->object->As<TESForm>()));
            }
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
        int menuView = 0;
        if (menu->uiMovie->GetVariable(&result, "_root.Menu_mc.inventoryLists.categoryList.activeSegment") && result.IsNumber()) {
            menuView = static_cast<int>(result.GetNumber());
        }

        auto transferList = BuildTransferList();
        SKSE::log::info("Transfer list contains {} unique item types", transferList.size());

        Count totalCount = 0;
        if (Settings::ConfirmTransfer() && !transferList.empty()) {
            const auto side = menuView == 0
                ? ContainerPreviewSide::Retrieve
                : ContainerPreviewSide::Store;
            if (const auto preview = CaptureContainerPreview(side)) {
                totalCount = menuView == 0 ? preview->retrieveCount : preview->storeCount;
            }
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
                            OperationOverlay::RunWithOverlay(OperationOverlay::Action::Retrieve, [=] {
                                ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                            });
                        } else {
                            SKSE::log::info("User cancelled retrieval");
                            SkyPromptIntegration::GetSingleton().ScheduleLabelSync();
                            operationInProgress.store(false);
                        }
                    });
            } else {
                SKSE::log::info("Confirmation disabled, proceeding with retrieval");
                OperationOverlay::RunWithOverlay(OperationOverlay::Action::Retrieve, [=] {
                    ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                });
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
                            OperationOverlay::RunWithOverlay(OperationOverlay::Action::Store, [=] {
                                ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                            });
                        } else {
                            SKSE::log::info("User cancelled transfer");
                            SkyPromptIntegration::GetSingleton().ScheduleLabelSync();
                            operationInProgress.store(false);
                        }
                    });
            } else {
                SKSE::log::info("Confirmation disabled, proceeding with transfer");
                OperationOverlay::RunWithOverlay(OperationOverlay::Action::Store, [=] {
                    ExecuteTransfer(transferList, transferContainer, containerMode, menuView);
                });
            }
        }
        SKSE::log::info("==== Junk Transfer Operation Complete ====");
        SKSE::log::info(" ");
    }

    void JunkHandler::ExecuteTransfer(std::vector<InventoryEntryData*> transferList, TESObjectREFR* transferContainer, ContainerMenu::ContainerMode containerMode, int menuView) {
        SKSE::log::info("---- Executing Junk Transfer ----");
        auto player = RE::PlayerCharacter::GetSingleton();
        TESObjectREFR* source = menuView == 0 ? transferContainer : player;

        cInventoryContainerId = 0;
        const auto* sourceCounts = GetContainerInventoryCountMap(source);
        auto countOf = [&](TESBoundObject* obj) -> Count {
            const auto it = sourceCounts->find(obj);
            if (it == sourceCounts->end() || it->second <= 0) {
                return 0;
            }
            return it->second;
        };

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

                Count itemCount = countOf(entryData->object);
                if (itemCount > 0) {
                    if (spdlog::should_log(spdlog::level::debug)) {
                        SKSE::log::debug("Retrieving {} x{}", entryData->object->GetName(), itemCount);
                    }
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

                    Count iCount = countOf(entryData->object);
                    Count iTotalCount = iCount;
                    totalPossibleTransferred += iTotalCount;

                    if (iCount > 0) {
                        float itemWeight = entryData->object->GetWeight();
                        iCount = FitCountToCarryWeight(iCount, itemWeight, currentWeight, maxWeight);

                        if (iCount > 0) {
                            MoveItems(entryData->object, player, transferContainer, reason, iCount);
                            currentWeight += (itemWeight * static_cast<float>(iCount));
                            totalTransferred += iCount;
                            if (spdlog::should_log(spdlog::level::debug)) {
                                SKSE::log::debug(
                                    "Transferred {} {} [{}/{}]",
                                    iCount,
                                    entryData->object->GetName(),
                                    RoundNumber(currentWeight),
                                    RoundNumber(maxWeight));
                            }
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

                    Count itemCount = countOf(entryData->object);
                    if (itemCount > 0) {
                        if (spdlog::should_log(spdlog::level::debug)) {
                            SKSE::log::debug("Transferring {} x{}", entryData->object->GetName(), itemCount);
                        }
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

        SkyPromptIntegration::GetSingleton().InvalidateSellPreview();

        auto& junkManager = JunkDataManager::GetSingleton();
        SKSE::log::info("Current Junk List Size: {}", junkManager.Size());

        if (junkManager.Size() == 0) {
            SKSE::log::info("No items in junk list, aborting sell");
            RE::DebugMessageBox("No Junk to sell!");
            operationInProgress.store(false);
            SkyPromptIntegration::GetSingleton().RecapturePreviews();
            return;
        }

        auto player = RE::PlayerCharacter::GetSingleton();
        auto sellList = BuildSellList();

        SKSE::log::info("SellList generated. Entry Count: {}", sellList.size());

        if (sellList.empty()) {
            SKSE::log::info("No sellable junk in inventory!");
            RE::DebugMessageBox("No Junk to sell!");
            operationInProgress.store(false);
            SkyPromptIntegration::GetSingleton().RecapturePreviews();
            return;
        }

        TESObjectREFR* vendorActorRef = GetBarterMenuContainer();
        TESObjectREFR* vendorContainer = GetBarterMenuMerchantContainer();

        if (!vendorActorRef || vendorActorRef == player->As<TESObjectREFR>()) {
            SKSE::log::error("Failed to get a valid vendor actor. Exiting Bulk Sale process.");
            RE::DebugMessageBox("JunkIt encountered an error attempting to sell items. Please report this on the JunkIt mod page.");
            operationInProgress.store(false);
            SkyPromptIntegration::GetSingleton().RecapturePreviews();
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
            SkyPromptIntegration::GetSingleton().RecapturePreviews();
            return;
        }

        float vendorGoldDisplay = 0.0f;
        float sellMult = 0.0f;
        if (!TryReadBarterPrices(vendorGoldDisplay, sellMult)) {
            operationInProgress.store(false);
            SkyPromptIntegration::GetSingleton().RecapturePreviews();
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
            SkyPromptIntegration::GetSingleton().RecapturePreviews();
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
                        OperationOverlay::RunWithOverlay(OperationOverlay::Action::Sell, [=] {
                            ExecuteSell(itemsToSell, vendorActorRef, vendorContainer, roundedSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay);
                        });
                    } else {
                        SKSE::log::info("User cancelled sale");
                        operationInProgress.store(false);
                        SkyPromptIntegration::GetSingleton().RecapturePreviews();
                    }
                });
        } else {
            SKSE::log::info("Confirmation disabled, proceeding with sale");
            OperationOverlay::RunWithOverlay(OperationOverlay::Action::Sell, [=] {
                ExecuteSell(itemsToSell, vendorActorRef, vendorContainer, roundedSellValue, totalToSell, totalPossibleToSell, vendorGoldDisplay);
            });
        }
        SKSE::log::info("==== Junk Sell Operation Complete ====");
        SKSE::log::info(" ");
    }

    std::vector<JunkHandler::SellWorkItem> JunkHandler::BuildSellWorkList(
        const std::vector<std::pair<InventoryEntryData*, Count>>& itemsToSell) {
        std::vector<SellWorkItem> work;
        std::unordered_map<FormID, std::size_t> index;
        for (const auto& [entry, count] : itemsToSell) {
            if (!entry || !entry->object || count <= 0) {
                continue;
            }
            const FormID formId = entry->object->GetFormID();
            if (auto it = index.find(formId); it != index.end()) {
                work[it->second].count += count;
            } else {
                index[formId] = work.size();
                work.push_back({ formId, count });
            }
        }
        return work;
    }

    void JunkHandler::SellWorkUnits(std::vector<SellWorkItem>& remaining, TESObjectREFR* from, TESObjectREFR* to, Count maxUnits) {
        if (!from || !to || maxUnits <= 0) {
            return;
        }

        std::unordered_set<FormID> wanted;
        wanted.reserve(remaining.size());
        for (const auto& item : remaining) {
            wanted.insert(item.formId);
        }

        std::unordered_map<FormID, InventoryEntryData*> entriesByForm;
        if (auto* changes = from->GetInventoryChanges(); changes && changes->entryList) {
            for (auto& entry : *changes->entryList) {
                if (!entry || !entry->object) {
                    continue;
                }
                const FormID formId = entry->object->GetFormID();
                if (!wanted.contains(formId) || entriesByForm.contains(formId)) {
                    continue;
                }
                if (GetSellableJunkCount(entry) > 0) {
                    entriesByForm[formId] = entry;
                }
            }
        }

        auto resolveSellable = [&](TESBoundObject* bound) {
            InventoryEntryData* chosen = nullptr;
            Count available = 0;
            const FormID formId = bound->GetFormID();
            if (auto it = entriesByForm.find(formId); it != entriesByForm.end()) {
                available = GetSellableJunkCount(it->second);
                if (available > 0) {
                    return std::pair{ it->second, available };
                }
                entriesByForm.erase(it);
            }
            ForEachInventoryEntry(from, [&](InventoryEntryData* entry) {
                if (chosen || !entry || entry->object != bound) {
                    return;
                }
                const Count n = GetSellableJunkCount(entry);
                if (n > 0) {
                    chosen = entry;
                    available = n;
                }
            });
            return std::pair{ chosen, available };
        };

        Count sold = 0;
        while (!remaining.empty() && sold < maxUnits) {
            auto& item = remaining.front();
            auto* bound = LookupBoundObject(item.formId);
            if (!bound) {
                remaining.erase(remaining.begin());
                continue;
            }

            const Count take = std::min(item.count, maxUnits - sold);
            Count soldThis = 0;
            while (soldThis < take) {
                auto [chosen, available] = resolveSellable(bound);
                if (!chosen) {
                    break;
                }
                const Count chunk = std::min(take - soldThis, available);
                if (spdlog::should_log(spdlog::level::debug)) {
                    SKSE::log::debug("Selling {} x{}", bound->GetName(), chunk);
                }
                SellEntryUnits(chosen, from, to, chunk);
                entriesByForm.erase(bound->GetFormID());
                if (spdlog::should_log(spdlog::level::debug)) {
                    SKSE::log::debug("Transaction for {} {} complete", chunk, bound->GetName());
                }
                soldThis += chunk;
            }

            item.count -= soldThis;
            sold += soldThis;
            if (item.count <= 0 || soldThis == 0) {
                remaining.erase(remaining.begin());
            }
        }
    }

    void JunkHandler::ContinueChunkedSell(SellSession session) {
        auto* player = PlayerCharacter::GetSingleton();
        auto* vendorActorRef = LookupRefr(session.vendorActorId);
        auto* vendorContainer = LookupRefr(session.vendorContainerId);
        if (!player || !vendorActorRef || !vendorContainer) {
            SKSE::log::error("Chunked sale aborted: missing player or vendor reference");
            CompleteOperation();
            return;
        }

        const Count chunkSize = Settings::GetSellChunkSize();
        Count remainingUnits = 0;
        for (const auto& item : session.remaining) {
            remainingUnits += item.count;
        }

        SKSE::log::info("Selling chunk of up to {} items ({} remaining)", chunkSize, remainingUnits);
        SellWorkUnits(session.remaining, player, vendorContainer, chunkSize);

        if (session.remaining.empty()) {
            FinishSell(player, vendorActorRef, vendorContainer, session.totalSellValue, session.totalToSell, session.totalPossibleToSell, session.uniqueTypes);
            return;
        }

        const auto ui = RE::UI::GetSingleton();
        auto menu = ui ? ui->GetMenu<BarterMenu>() : nullptr;
        if (!menu || !menu->uiMovie) {
            SKSE::log::info("Barter menu closed during chunked sale, selling remaining items");
            remainingUnits = 0;
            for (const auto& item : session.remaining) {
                remainingUnits += item.count;
            }
            SellWorkUnits(session.remaining, player, vendorContainer, remainingUnits);
            FinishSell(player, vendorActorRef, vendorContainer, session.totalSellValue, session.totalToSell, session.totalPossibleToSell, session.uniqueTypes);
            return;
        }

        if (vendorContainer != vendorActorRef) {
            SendInventoryUpdate(vendorContainer);
        }

        const int deferredFrames = HeavyLoadDeferredFrames();
        ScheduleInventoryUIRefresh(
            player->GetFormID(),
            session.vendorActorId,
            deferredFrames,
            [session = std::move(session)]() mutable {
                ContinueChunkedSell(std::move(session));
            },
            false);
    }

    void JunkHandler::FinishSell(
        TESObjectREFR* player,
        TESObjectREFR* vendorActorRef,
        TESObjectREFR* vendorContainer,
        Count totalSellValue,
        Count totalToSell,
        Count totalPossibleToSell,
        std::size_t uniqueTypes) {
        if (auto* playerActor = PlayerCharacter::GetSingleton()) {
            SKSE::log::info("Adding {} Speech experience", totalSellValue);
            playerActor->AddSkillExperience(RE::ActorValue::kSpeech, static_cast<float>(totalSellValue));
        } else {
            SKSE::log::error("Speech experience skipped: player character is missing");
        }

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

        RefreshMenusAfterBulk(player, vendorActorRef, uniqueTypes, totalToSell);
        if (vendorContainer && vendorContainer != vendorActorRef) {
            SendInventoryUpdate(vendorContainer);
        }
        SKSE::log::info("---- Sale Execution Complete ----");
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

        const Count chunkSize = Settings::GetSellChunkSize();
        SKSE::log::info("SellList Size: {}", itemsToSell.size());

        if (totalToSell <= chunkSize) {
            SKSE::log::info("Transferring junk items to vendor...");
            for (const auto& [entryData, count] : itemsToSell) {
                if (count > 0 && entryData && entryData->object) {
                    if (spdlog::should_log(spdlog::level::debug)) {
                        SKSE::log::debug("Selling {} x{}", entryData->object->GetName(), count);
                    }
                    SellEntryUnits(entryData, player, vendorContainer, count);
                    if (spdlog::should_log(spdlog::level::debug)) {
                        SKSE::log::debug("Transaction for {} {} complete", count, entryData->object->GetName());
                    }
                }
            }
            FinishSell(player, vendorActorRef, vendorContainer, totalSellValue, totalToSell, totalPossibleToSell, itemsToSell.size());
            return;
        }

        SKSE::log::info("Chunked sale: {} items in chunks of {}", totalToSell, chunkSize);
        SellSession session;
        session.remaining = BuildSellWorkList(itemsToSell);
        session.vendorActorId = vendorActorRef->GetFormID();
        session.vendorContainerId = vendorContainer->GetFormID();
        session.totalSellValue = totalSellValue;
        session.totalToSell = totalToSell;
        session.totalPossibleToSell = totalPossibleToSell;
        session.uniqueTypes = itemsToSell.size();
        ContinueChunkedSell(std::move(session));
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
            SKSE::log::debug("No item selected in ItemListMenu. Updating UI and trying again");
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

        bool playerOwned = false;
        if (auto* player = PlayerCharacter::GetSingleton()) {
            playerOwned = selectedItem->data.owner == player->GetHandle().native_handle();
        }

        if (inventoryEntry->IsQuestObject()) {
            if (spdlog::should_log(spdlog::level::debug)) {
                SKSE::log::debug(
                    "Cannot mark quest item {} [{}] as junk",
                    itemForm->GetName(),
                    FormUtil::Form::GetFormConfigString(itemForm));
            }
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
                if (spdlog::should_log(spdlog::level::debug)) {
                    SKSE::log::debug(
                        "Item is equipped and protected: {} [{}]",
                        itemForm->GetName(),
                        FormUtil::Form::GetFormConfigString(itemForm));
                }
                needsConfirmation = true;
                protectionReason = "equipped";
            } else if (Settings::ProtectFavorites() && inventoryEntry->IsFavorited()) {
                if (spdlog::should_log(spdlog::level::debug)) {
                    SKSE::log::debug(
                        "Item is favorited and protected: {} [{}]",
                        itemForm->GetName(),
                        FormUtil::Form::GetFormConfigString(itemForm));
                }
                needsConfirmation = true;
                protectionReason = "favorited";
            }

            if (needsConfirmation) {
                SKSE::log::debug("Showing confirmation dialog for protected item");
                std::string confirmText = Translation::Format("$JunkIt_MarkProtectedConfirm", protectionReason);
                ShowConfirmationMessageBox(confirmText.c_str(),
                    { Translation::Get("$JunkIt_Yes"), Translation::Get("$JunkIt_ConfirmNo") },
                    [inventoryEntry, itemForm, itemObject, playerOwned, owner = selectedItem->data.owner](unsigned int choice) {
                        if (choice == 0) {
                            SKSE::log::debug("User confirmed marking protected item as junk");
                            if (spdlog::should_log(spdlog::level::debug)) {
                                SKSE::log::debug(
                                    "Adding junk status to {} [{}]",
                                    itemForm->GetName(),
                                    FormUtil::Form::GetFormConfigString(itemForm));
                            }
                            auto& junkManager = JunkDataManager::GetSingleton();
                            const auto addedIdentity = junkManager.AddJunkItem(inventoryEntry);

                            if (junkManager.IsJunk(inventoryEntry)) {
                                SkyPromptIntegration::GetSingleton().OnJunkToggled(inventoryEntry, true, playerOwned);
                            }

                            RefreshJunkListIcons(UIUtil::ItemList::GetOpenList(), itemObject, owner, true);

                            if (addedIdentity) {
                                SKSE::log::debug("Form marked as junk: {}", *addedIdentity);
                            } else {
                                SKSE::log::warn("Failed to mark form as junk: {}", itemForm->GetName());
                            }
                            if (Settings::GetNotifyOnMarkUnmark()) {
                                std::string msg = fmt::format("JunkIt - {} has been marked as junk", itemForm->GetName());
                                SendHUDMessage::ShowHUDMessage(msg.c_str());
                            }
                        } else {
                            SKSE::log::debug("User cancelled marking protected item as junk");
                        }
                    });
                return itemForm;
            }
        }

        std::optional<std::string> junkIdentity;
        if (isJunk) {
            if (spdlog::should_log(spdlog::level::debug)) {
                SKSE::log::debug(
                    "Removing junk status from {} [{}]",
                    itemForm->GetName(),
                    FormUtil::Form::GetFormConfigString(itemForm));
            }
            junkIdentity = junkManager.RemoveJunkItem(inventoryEntry);
        } else {
            if (spdlog::should_log(spdlog::level::debug)) {
                SKSE::log::debug(
                    "Adding junk status to {} [{}]",
                    itemForm->GetName(),
                    FormUtil::Form::GetFormConfigString(itemForm));
            }
            junkIdentity = junkManager.AddJunkItem(inventoryEntry);
        }

        bool isNowJunk = junkManager.IsJunk(inventoryEntry);

        if (isJunk != isNowJunk) {
            SkyPromptIntegration::GetSingleton().OnJunkToggled(inventoryEntry, isNowJunk, playerOwned);
        }

        RefreshJunkListIcons(itemListMenu, itemObject, selectedItem->data.owner, isNowJunk);

        if (isNowJunk) {
            if (junkIdentity) {
                SKSE::log::debug("Form marked as junk: {}", *junkIdentity);
            } else {
                SKSE::log::warn("Form marked as junk but no identity was returned for {}", itemForm->GetName());
            }
            if (Settings::GetNotifyOnMarkUnmark()) {
                std::string msg = fmt::format("JunkIt - {} has been marked as junk", itemForm->GetName());
                SendHUDMessage::ShowHUDMessage(msg.c_str());
            }
        } else {
            if (spdlog::should_log(spdlog::level::debug)) {
                SKSE::log::debug("Form: {} is no longer marked as junk", itemForm->GetName());
            }
            if (Settings::GetNotifyOnMarkUnmark()) {
                std::string msg = fmt::format("JunkIt - {} is no longer marked as junk", itemForm->GetName());
                SendHUDMessage::ShowHUDMessage(msg.c_str());
            }
        }

        return itemForm;
    }

    namespace {
        class TrashMenuHandler : public RE::BSTEventSink<RE::MenuOpenCloseEvent> {
        public:
            static TrashMenuHandler* GetSingleton() {
                static TrashMenuHandler singleton;
                return &singleton;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent* a_event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
                if (!a_event) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                const auto& name = a_event->menuName;
                const bool inventoryLike =
                    name == "InventoryMenu" || name == "ContainerMenu" || name == "BarterMenu";
                if (!inventoryLike) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                if (a_event->opening) {
                    JunkHandler::TryExpireTrash();
                } else {
                    JunkHandler::TryExpireTrash();
                }
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            TrashMenuHandler() = default;
        };
    }

    void JunkHandler::Install() {
        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            SKSE::log::error("UI singleton unavailable; trash menu sink not registered");
            return;
        }
        ui->AddEventSink<RE::MenuOpenCloseEvent>(TrashMenuHandler::GetSingleton());
        SKSE::log::info("Registered trash menu event handler");
    }

    TESObjectREFR* JunkHandler::PrepareTrashContainer() {
        auto* chest = Settings::GetTrashContainer();
        if (!chest) {
            return nullptr;
        }
        if (chest->IsDisabled()) {
            chest->Enable(false);
            SKSE::log::info("Enabled disabled trash container");
        }
        chest->InitInventoryIfRequired();
        return chest;
    }

    bool JunkHandler::TrashContainerIsEmpty(TESObjectREFR* chest) {
        if (!chest) {
            return true;
        }
        chest->InitInventoryIfRequired();
        bool empty = true;
        ForEachInventoryCount(chest, [&](TESBoundObject* obj, Count count) {
            if (obj && count > 0) {
                empty = false;
            }
        });
        return empty;
    }

    void JunkHandler::EmptyTrashContainer(TESObjectREFR* chest) {
        if (!chest) {
            return;
        }
        chest->InitInventoryIfRequired();
        std::vector<std::pair<TESBoundObject*, Count>> toRemove;
        ForEachInventoryCount(chest, [&](TESBoundObject* obj, Count count) {
            if (obj && count > 0) {
                toRemove.emplace_back(obj, count);
            }
        });
        for (auto& [obj, count] : toRemove) {
            chest->RemoveItem(obj, count, ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
        }
        trashFilledGameDays = 0.0f;
        trashStampPending = false;
        SKSE::log::info("Emptied trash container");
    }

    void JunkHandler::NoteTrashDeposit() {
        if (trashFilledGameDays > 0.0f) {
            return;
        }
        auto* calendar = RE::Calendar::GetSingleton();
        if (!calendar) {
            trashStampPending = true;
            return;
        }
        trashFilledGameDays = calendar->GetDaysPassed();
        trashStampPending = false;
        SKSE::log::info("Trash fill stamp set to {:.2f} days passed", trashFilledGameDays);
    }

    void JunkHandler::ClearTrashStampIfEmpty() {
        auto* chest = PrepareTrashContainer();
        if (!chest) {
            return;
        }
        if (TrashContainerIsEmpty(chest)) {
            trashFilledGameDays = 0.0f;
            trashStampPending = false;
        }
    }

    void JunkHandler::TryExpireTrash() {
        ClearTrashStampIfEmpty();

        if (!Settings::IsTrashAvailable()) {
            return;
        }

        if (trashStampPending && trashFilledGameDays <= 0.0f) {
            auto* calendar = RE::Calendar::GetSingleton();
            auto* chest = PrepareTrashContainer();
            if (calendar && chest && !TrashContainerIsEmpty(chest)) {
                trashFilledGameDays = calendar->GetDaysPassed();
                trashStampPending = false;
                SKSE::log::info("Trash fill stamp set to {:.2f} days passed", trashFilledGameDays);
            }
        }

        const auto expireDays = Settings::GetTrashExpireDays();
        if (expireDays <= 0 || trashFilledGameDays <= 0.0f) {
            return;
        }

        auto* calendar = RE::Calendar::GetSingleton();
        auto* chest = PrepareTrashContainer();
        if (!calendar || !chest) {
            return;
        }

        const float elapsed = calendar->GetDaysPassed() - trashFilledGameDays;
        if (elapsed >= static_cast<float>(expireDays)) {
            SKSE::log::info("Trash expired after {:.2f} in-game days", elapsed);
            EmptyTrashContainer(chest);
        }
    }

    void JunkHandler::SaveTrashState(SKSE::SerializationInterface* intfc) {
        if (!intfc) {
            return;
        }
        if (!intfc->WriteRecordData(&trashFilledGameDays, sizeof(trashFilledGameDays))) {
            SKSE::log::error("Failed to write trash fill stamp");
        }
    }

    void JunkHandler::LoadTrashState(SKSE::SerializationInterface* intfc, std::uint32_t recordVersion) {
        if (!intfc || recordVersion != 1) {
            SKSE::log::warn("Skipping unsupported TRSH record version {}", recordVersion);
            return;
        }
        float days = 0.0f;
        if (!intfc->ReadRecordData(&days, sizeof(days))) {
            SKSE::log::error("Failed to read trash fill stamp");
            return;
        }
        trashFilledGameDays = days;
        SKSE::log::info("Loaded trash fill stamp {:.2f}", trashFilledGameDays);
    }

    void JunkHandler::RevertTrashState() {
        trashFilledGameDays = 0.0f;
        trashStampPending = false;
    }

    JunkHandler::Count JunkHandler::TrashEntryUnits(InventoryEntryData* a_entry, TESObjectREFR* a_from, TESObjectREFR* a_to) {
        if (!a_entry || !a_entry->object || !a_from || !a_to) {
            return 0;
        }

        const auto scan = ScanEntryJunk(a_entry, true);
        if (scan.junkCount <= 0) {
            return 0;
        }

        if (scan.fullyJunk) {
            MoveItems(a_entry->object, a_from, a_to, ITEM_REMOVE_REASON::kStoreInContainer, scan.junkCount, nullptr);
            return scan.junkCount;
        }

        Count remaining = scan.junkCount;
        for (const auto& [extraList, extraCount] : scan.junkExtras) {
            if (remaining <= 0) {
                break;
            }
            const Count toMove = std::min(remaining, extraCount);
            MoveItems(a_entry->object, a_from, a_to, ITEM_REMOVE_REASON::kStoreInContainer, toMove, extraList);
            remaining -= toMove;
        }

        if (remaining > 0 && scan.plainIsJunk) {
            MoveItems(a_entry->object, a_from, a_to, ITEM_REMOVE_REASON::kStoreInContainer, remaining, nullptr);
        }
        return scan.junkCount;
    }

    bool JunkHandler::EntryIsTrashable(InventoryEntryData* entry) {
        if (!entry || !entry->object) {
            return false;
        }
        if (!EntryPassesPreviewFilters(entry, false)) {
            return false;
        }
        return GetSellableJunkCount(entry) > 0;
    }

    bool JunkHandler::HasTrashablePlayerJunk() {
        auto* player = PlayerCharacter::GetSingleton();
        if (!player) {
            return false;
        }

        bool found = false;
        ForEachInventoryEntry(player, [&](InventoryEntryData* entry) {
            if (!found && EntryIsTrashable(entry)) {
                found = true;
            }
        });
        return found;
    }

    JunkHandler::Count JunkHandler::CountInventoryTrashUnits() {
        Count total = 0;
        auto* player = PlayerCharacter::GetSingleton();
        if (!player) {
            return 0;
        }

        ForEachInventoryEntry(player, [&](InventoryEntryData* entry) {
            if (!entry || !entry->object) {
                return;
            }
            if (!EntryPassesPreviewFilters(entry, false)) {
                return;
            }
            total += GetSellableJunkCount(entry);
        });
        return total;
    }

    std::vector<InventoryEntryData*> JunkHandler::BuildInventoryTrashList() {
        std::vector<InventoryEntryData*> trashList;
        auto* player = PlayerCharacter::GetSingleton();
        if (!player) {
            return trashList;
        }

        ForEachInventoryEntry(player, [&](InventoryEntryData* entry) {
            if (EntryIsTrashable(entry)) {
                trashList.push_back(entry);
            }
        });
        return trashList;
    }

    void JunkHandler::ExecuteTrash(TESObjectREFR* from, TESBoundObject* item, Count count, ExtraDataList* extraList) {
        auto* chest = PrepareTrashContainer();
        if (!from || !item || !chest || count <= 0) {
            CompleteOperation();
            return;
        }
        MoveItems(item, from, chest, ITEM_REMOVE_REASON::kStoreInContainer, count, extraList);
        NoteTrashDeposit();
        SendInventoryUpdate(chest);
        RefreshMenusAfterBulk(from, GetOpenMenuListSecondary(), 1, count);
    }

    void JunkHandler::ExecuteBulkTrash(std::vector<InventoryEntryData*> trashList) {
        auto* player = PlayerCharacter::GetSingleton();
        auto* chest = PrepareTrashContainer();
        if (!player || !chest) {
            CompleteOperation();
            return;
        }

        Count total = 0;
        for (auto* entry : trashList) {
            if (!entry || !entry->object) {
                continue;
            }
            total += TrashEntryUnits(entry, player, chest);
        }
        if (total > 0) {
            NoteTrashDeposit();
        }
        SendInventoryUpdate(chest);
        RefreshMenusAfterBulk(player, GetOpenMenuListSecondary(), trashList.size(), total);
    }

    void JunkHandler::TrashSelectedItem() {
        TryExpireTrash();

        bool expected = false;
        if (!operationInProgress.compare_exchange_strong(expected, true)) {
            return;
        }

        auto* chest = PrepareTrashContainer();
        if (!chest) {
            SendHUDMessage::ShowHUDMessage(Translation::Get("$JunkIt_TrashUnavailable").c_str());
            operationInProgress.store(false);
            return;
        }

        ItemList* itemListMenu = UIUtil::ItemList::GetOpenList();
        if (!itemListMenu) {
            SendHUDMessage::ShowHUDMessage(Translation::Get("$JunkIt_TrashNoItem").c_str());
            operationInProgress.store(false);
            return;
        }

        ItemList::Item* selectedItem = itemListMenu->GetSelectedItem();
        if (!selectedItem) {
            itemListMenu->Update();
            selectedItem = itemListMenu->GetSelectedItem();
        }
        if (!selectedItem || !selectedItem->data.objDesc) {
            SendHUDMessage::ShowHUDMessage(Translation::Get("$JunkIt_TrashNoItem").c_str());
            operationInProgress.store(false);
            return;
        }

        auto* player = PlayerCharacter::GetSingleton();
        if (!player || selectedItem->data.owner != player->GetHandle().native_handle()) {
            SendHUDMessage::ShowHUDMessage(Translation::Get("$JunkIt_TrashPlayerOnly").c_str());
            operationInProgress.store(false);
            return;
        }

        InventoryEntryData* inventoryEntry = selectedItem->data.objDesc;
        TESBoundObject* itemObject = inventoryEntry->object;
        if (!itemObject) {
            SendHUDMessage::ShowHUDMessage(Translation::Get("$JunkIt_TrashNoItem").c_str());
            operationInProgress.store(false);
            return;
        }

        if (inventoryEntry->IsQuestObject()) {
            SendHUDMessage::ShowHUDMessage(Translation::Get("$JunkIt_TrashQuest").c_str());
            operationInProgress.store(false);
            return;
        }

        Count count = static_cast<Count>(selectedItem->data.GetCount());
        if (count <= 0) {
            SendHUDMessage::ShowHUDMessage(Translation::Get("$JunkIt_TrashNoItem").c_str());
            operationInProgress.store(false);
            return;
        }

        const FormID itemFormId = itemObject->GetFormID();
        const std::uint16_t uniqueID = CaptureSelectedUniqueID(inventoryEntry, count);
        std::string confirmText = Translation::Format("$JunkIt_TrashConfirm", itemObject->GetName());
        ShowConfirmationMessageBox(
            confirmText.c_str(),
            { Translation::Get("$JunkIt_TrashConfirmYes"), Translation::Get("$JunkIt_ConfirmNo") },
            [itemFormId, count, uniqueID](unsigned int choice) {
                if (choice == 0) {
                    OperationOverlay::RunWithOverlay(OperationOverlay::Action::Trash, [itemFormId, count, uniqueID] {
                        auto* player = PlayerCharacter::GetSingleton();
                        auto* item = LookupBoundObject(itemFormId);
                        auto* extraList = FindExtraByUniqueID(player, item, uniqueID);
                        ExecuteTrash(player, item, count, extraList);
                    });
                } else {
                    operationInProgress.store(false);
                }
            });
    }

    void JunkHandler::TrashAllJunk() {
        TryExpireTrash();

        bool expected = false;
        if (!operationInProgress.compare_exchange_strong(expected, true)) {
            return;
        }

        auto* chest = PrepareTrashContainer();
        if (!chest) {
            SendHUDMessage::ShowHUDMessage(Translation::Get("$JunkIt_TrashUnavailable").c_str());
            operationInProgress.store(false);
            return;
        }

        const Count totalCount = CountInventoryTrashUnits();
        if (totalCount <= 0) {
            RE::DebugMessageBox(Translation::Get("$JunkIt_TrashNone").c_str());
            operationInProgress.store(false);
            return;
        }

        std::string confirmText = Translation::Format("$JunkIt_TrashBulkConfirm", totalCount);
        ShowConfirmationMessageBox(
            confirmText.c_str(),
            { Translation::Get("$JunkIt_TrashConfirmYes"), Translation::Get("$JunkIt_ConfirmNo") },
            [](unsigned int choice) {
                if (choice == 0) {
                    OperationOverlay::RunWithOverlay(OperationOverlay::Action::Trash, [] {
                        ExecuteBulkTrash(BuildInventoryTrashList());
                    });
                } else {
                    SkyPromptIntegration::GetSingleton().ScheduleLabelSync();
                    operationInProgress.store(false);
                }
            });
    }

    void JunkHandler::HideOpenInventoryMenus() {
        auto* queue = RE::UIMessageQueue::GetSingleton();
        auto* strings = RE::InterfaceStrings::GetSingleton();
        auto* ui = RE::UI::GetSingleton();
        if (!queue || !strings || !ui) {
            return;
        }
        if (ui->IsMenuOpen(strings->inventoryMenu)) {
            queue->AddMessage(strings->inventoryMenu, RE::UI_MESSAGE_TYPE::kHide, nullptr);
        }
        if (ui->IsMenuOpen(strings->containerMenu)) {
            queue->AddMessage(strings->containerMenu, RE::UI_MESSAGE_TYPE::kHide, nullptr);
        }
        if (ui->IsMenuOpen(strings->barterMenu)) {
            queue->AddMessage(strings->barterMenu, RE::UI_MESSAGE_TYPE::kHide, nullptr);
        }
    }

    void JunkHandler::ScheduleActivateTrash(int framesRemaining) {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            auto* player = PlayerCharacter::GetSingleton();
            auto* chest = PrepareTrashContainer();
            if (player && chest) {
                chest->ActivateRef(player, 0, nullptr, 1, false);
            }
            return;
        }

        tasks->AddUITask([framesRemaining]() {
            if (InventoryLikeMenuOpen() && framesRemaining > 0) {
                ScheduleActivateTrash(framesRemaining - 1);
                return;
            }
            auto* player = PlayerCharacter::GetSingleton();
            auto* chest = PrepareTrashContainer();
            if (!player || !chest) {
                SendHUDMessage::ShowHUDMessage(Translation::Get("$JunkIt_TrashUnavailable").c_str());
                return;
            }
            if (auto* cell = chest->GetParentCell()) {
                if (!cell->IsAttached()) {
                    SKSE::log::warn("Trash container cell is not attached; attempting ActivateRef");
                }
            } else {
                SKSE::log::warn("Trash container has no parent cell; attempting ActivateRef");
            }
            chest->ActivateRef(player, 0, nullptr, 1, false);
        });
    }

    bool JunkHandler::IsGameWorldReady() {
        auto* ui = RE::UI::GetSingleton();
        auto* strings = RE::InterfaceStrings::GetSingleton();
        if (ui && strings && ui->IsMenuOpen(strings->mainMenu)) {
            return false;
        }

        auto* player = PlayerCharacter::GetSingleton();
        return player && player->GetParentCell();
    }

    std::optional<std::int32_t> JunkHandler::GetTrashItemCount() {
        if (!IsGameWorldReady()) {
            return std::nullopt;
        }

        auto* chest = Settings::GetTrashContainer();
        if (!chest) {
            return std::nullopt;
        }

        chest->InitInventoryIfRequired();
        Count total = 0;
        for (const auto& [obj, data] : chest->GetInventory()) {
            if (obj && data.first > 0) {
                total += data.first;
            }
        }
        return total;
    }

    std::optional<float> JunkHandler::GetTrashDaysRemaining() {
        if (!IsGameWorldReady() || !Settings::GetTrashContainer()) {
            return std::nullopt;
        }

        const auto expireDays = Settings::GetTrashExpireDays();
        if (expireDays <= 0 || trashFilledGameDays <= 0.0f) {
            return std::nullopt;
        }

        auto* calendar = RE::Calendar::GetSingleton();
        if (!calendar) {
            return std::nullopt;
        }

        const float remaining = static_cast<float>(expireDays) - (calendar->GetDaysPassed() - trashFilledGameDays);
        return remaining > 0.0f ? remaining : 0.0f;
    }

    void JunkHandler::OpenTrashContainer() {
        if (!IsGameWorldReady()) {
            return;
        }

        TryExpireTrash();
        auto* chest = PrepareTrashContainer();
        if (!chest) {
            SendHUDMessage::ShowHUDMessage(Translation::Get("$JunkIt_TrashUnavailable").c_str());
            return;
        }

        UI::CloseFrameworkOverlay();
        HideOpenInventoryMenus();
        ScheduleActivateTrash(10);
    }

    void JunkHandler::EmptyTrashBin() {
        if (!IsGameWorldReady()) {
            return;
        }

        auto empty = []() {
            auto* chest = PrepareTrashContainer();
            if (chest) {
                EmptyTrashContainer(chest);
            }
        };

        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            empty();
            return;
        }

        tasks->AddTask(std::move(empty));
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

        const auto& listItems = itemListMenu->items;
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

        const auto& listItems = itemListMenu->items;
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
