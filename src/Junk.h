#pragma once 
#include "settings.h"
#include <atomic>
#include <functional>
using namespace RE;

namespace JunkIt {

    class JunkItMessageBoxCallback : public RE::IMessageBoxCallback {
    public:
        using CallbackFunc = std::function<void(unsigned int)>;

        JunkItMessageBoxCallback(CallbackFunc a_callback, unsigned int a_offset = 0)
            : callback(std::move(a_callback)), offset(a_offset) {}

        void Run(RE::IMessageBoxCallback::Message a_msg) override {
            unsigned int rawVal = static_cast<unsigned int>(a_msg);
            unsigned int adjusted = (rawVal >= offset) ? rawVal - offset : rawVal;
            if (callback) {
                callback(adjusted);
            }
        }

    private:
        CallbackFunc callback;
        unsigned int offset;
    };

    class JunkHandler {
        using Count = std::int32_t;
        using InventoryCountMap = std::map<TESBoundObject*, Count>;
        using InventoryItemMap = std::map<TESBoundObject*, std::pair<Count, std::unique_ptr<InventoryEntryData>>>;

    public: 
        static TESForm* ToggleSelectedItemJunk();
        static void ToggleIsJunk();

        static std::vector<InventoryEntryData*> BuildTransferList();
        static std::vector<std::pair<InventoryEntryData*, std::int32_t>> BuildSellList();
        static std::int32_t GetMenuItemValue(TESForm* a_form);
        static std::int32_t GetMenuItemValue(InventoryEntryData* a_entry);

        static TESObjectREFR* GetContainerMenuContainer();
        static TESObjectREFR* GetBarterMenuContainer();
        static TESObjectREFR* GetBarterMenuMerchantContainer();
        static ContainerMenu::ContainerMode GetContainerMode();

        static void TransferJunk();
        static void SellJunk();

        [[nodiscard]] static InventoryCountMap* GetContainerInventoryCountMap(TESObjectREFR* a_container) {
            if (cInventoryContainerId == a_container->GetFormID()) return &cInventoryCountMap;
            
            cInventoryCountMap.clear();
            cInventoryCountMap = a_container->GetInventoryCounts();
            cInventoryContainerId = a_container->GetFormID();
            return &cInventoryCountMap;
        }

        static InventoryCountMap* SetContainerInventoryCountMap(InventoryCountMap a_invCountMap, TESObjectREFR* a_container) { 
            if (cInventoryContainerId != a_container->GetFormID()) cInventoryContainerId = a_container->GetFormID();
            cInventoryCountMap.clear();
            cInventoryCountMap = a_invCountMap;
            return &cInventoryCountMap;
        }

        static std::atomic<bool> operationInProgress;

    private:

        static inline FormID cInventoryContainerId = 0;
        static inline InventoryCountMap cInventoryCountMap = {};

        static inline ItemList* ItemListMenu;
        static inline std::string MenuName;

        static std::int32_t RoundNumber(float number) {
            float ceiling = std::ceil(number);
            return (ceiling - number > 0.5f) ? static_cast<std::int32_t>(std::floor(number)) : static_cast<std::int32_t>(ceiling);
        }

        static Count GetItemCount(TESObjectREFR* a_container, TESBoundObject* a_item);
        static void MoveItems(TESBoundObject* a_item, TESObjectREFR* a_from, TESObjectREFR* a_to, ITEM_REMOVE_REASON a_reason, Count a_count, ExtraDataList* a_extraList = nullptr);
        static Count GetSellableJunkCount(InventoryEntryData* a_entry);
        static bool EntryIsFullyJunk(InventoryEntryData* a_entry);
        static ExtraDataList* FindJunkExtraList(InventoryEntryData* a_entry);
        static void SellEntryUnits(InventoryEntryData* a_entry, TESObjectREFR* a_from, TESObjectREFR* a_to, Count a_count);

        static void ExecuteTransfer(std::vector<InventoryEntryData*> transferList, TESObjectREFR* transferContainer, ContainerMenu::ContainerMode containerMode, int menuView);
        static void ExecuteSell(std::vector<std::pair<InventoryEntryData*, std::int32_t>> itemsToSell, TESObjectREFR* vendorActor, TESObjectREFR* vendorContainer, std::int32_t totalSellValue, std::int32_t totalToSell, std::int32_t totalPossibleToSell, float vendorGoldDisplay);

        static void ApplyInventoryUIRefresh(TESObjectREFR* primary, TESObjectREFR* secondary, bool nudgeSegment);
        static void ScheduleInventoryUIRefresh(FormID primaryId, FormID secondaryId, bool largeOp, int framesRemaining);
        static void RefreshMenusAfterBulk(TESObjectREFR* primary, TESObjectREFR* secondary, std::size_t uniqueTypes, Count totalItems);

        static void ShowConfirmationMessageBox(const char* bodyText, std::vector<std::string> buttons, std::function<void(unsigned int)> callback);
    };
}
