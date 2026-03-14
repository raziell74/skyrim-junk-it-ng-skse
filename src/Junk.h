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
        static std::vector<InventoryEntryData*> BuildSellList();
        static std::int32_t GetMenuItemValue(TESForm* a_form);

        static TESObjectREFR* GetContainerMenuContainer();
        static TESObjectREFR* GetBarterMenuContainer();
        static TESObjectREFR* GetBarterMenuMerchantContainer();
        static ContainerMenu::ContainerMode GetContainerMode();

        static void TransferJunk();
        static void SellJunk();

        static void TransferItem(TESBoundObject* a_item, TESObjectREFR* a_fromContainer, TESObjectREFR* a_toContainer, ITEM_REMOVE_REASON a_reason, std::int32_t a_count, InventoryEntryData* a_invData);

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

        static bool WarnLargeInventory(TESObjectREFR* a_container1, TESObjectREFR* a_container2);

        static void ScheduleVerifyAndDelayedRefresh(TESObjectREFR* sourceRef, std::vector<std::pair<TESBoundObject*, std::int32_t>> expectedCountsInSource);

        static void ExecuteTransfer(std::vector<InventoryEntryData*> transferList, TESObjectREFR* transferContainer, ContainerMenu::ContainerMode containerMode, int menuView);
        static void ExecuteSell(std::vector<std::pair<InventoryEntryData*, std::int32_t>> itemsToSell, TESObjectREFR* vendorActor, TESObjectREFR* vendorContainer, float totalSellValue, std::int32_t totalToSell, std::int32_t totalPossibleToSell, float vendorGoldDisplay, float playerCarryWeight);

        static void ShowConfirmationMessageBox(const char* bodyText, std::vector<std::string> buttons, std::function<void(unsigned int)> callback);
    };
}
