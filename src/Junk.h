#pragma once 
#include "settings.h"
#include "InventoryWalk.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>
using namespace RE;

namespace JunkIt {

    class JunkItMessageBoxCallback : public RE::IMessageBoxCallback {
    public:
        using CallbackFunc = std::function<void(unsigned int)>;

        JunkItMessageBoxCallback(CallbackFunc a_callback, unsigned int a_offset = 0)
            : callback(std::move(a_callback)), offset(a_offset) {}

        void Run(std::uint8_t a_button) override {
            unsigned int rawVal = static_cast<unsigned int>(a_button);
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
        static void StartAggressiveRefresh();

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
        static void TrashSelectedItem();
        static void TrashAllJunk();
        static void TryExpireTrash();
        static void OpenTrashContainer();
        static void Install();

        static void SaveTrashState(SKSE::SerializationInterface* intfc);
        static void LoadTrashState(SKSE::SerializationInterface* intfc, std::uint32_t recordVersion);
        static void RevertTrashState();

        struct ContainerPreviewCounts {
            std::int32_t storeCount = 0;
            std::int32_t retrieveCount = 0;
        };

        struct SellPreviewCapture {
            std::optional<std::int32_t> gold;
            float sellMult = 0.5f;
            bool pricesReady = false;
        };

        [[nodiscard]] static std::optional<ContainerPreviewCounts> CaptureContainerPreview();
        [[nodiscard]] static SellPreviewCapture CaptureSellPreview();
        static void CollectEntryIdentities(InventoryEntryData* entry, std::vector<std::string>& out);
        [[nodiscard]] static std::int32_t CountPreviewIdentities(TESObjectREFR* container, const std::vector<std::string>& identities, bool sellFilters);
        [[nodiscard]] static bool MovedItemIsPreviewableJunk(TESObjectREFR* dest, FormID baseObj, std::uint16_t uniqueID, bool sellFilters);
        [[nodiscard]] static std::int32_t CountJunkUnits(InventoryEntryData* entry);
        struct JunkPreviewUnit {
            std::int32_t count = 0;
            std::int32_t gold = 0;
            bool favorited = false;
            bool enchanted = false;
            bool worn = false;
        };
        [[nodiscard]] static std::optional<JunkPreviewUnit> LookupJunkPreviewUnit(
            TESObjectREFR* dest,
            FormID baseObj,
            std::uint16_t uniqueID,
            float sellMult = 0.0f);
        [[nodiscard]] static std::int32_t ComputeSellGoldDelta(InventoryEntryData* entry, std::int32_t count, float sellMult);
        [[nodiscard]] static std::optional<std::int32_t> ComputeMovedItemSellGold(TESObjectREFR* dest, FormID baseObj, std::uint16_t uniqueID, std::int32_t count, float sellMult);

        [[nodiscard]] static InventoryCountMap* GetContainerInventoryCountMap(TESObjectREFR* a_container) {
            if (cInventoryContainerId == a_container->GetFormID()) return &cInventoryCountMap;
            
            cInventoryCountMap.clear();
            ForEachInventoryCount(a_container, [&](TESBoundObject* obj, Count count) {
                if (obj) {
                    cInventoryCountMap[obj] = count;
                }
            });
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
        static bool EntryPassesPreviewFilters(InventoryEntryData* a_entry, bool sellFilters);
        static void SellEntryUnits(InventoryEntryData* a_entry, TESObjectREFR* a_from, TESObjectREFR* a_to, Count a_count);

        struct SellTotals {
            std::vector<std::pair<InventoryEntryData*, Count>> itemsToSell;
            Count totalToSell = 0;
            Count totalPossibleToSell = 0;
            Count roundedSellValue = 0;
        };

        static bool TryReadBarterPrices(float& vendorGold, float& sellMult);
        static SellTotals ComputeSellTotals(
            const std::vector<std::pair<InventoryEntryData*, Count>>& sellList,
            float vendorGold,
            float sellMult);

        static void ExecuteTransfer(std::vector<InventoryEntryData*> transferList, TESObjectREFR* transferContainer, ContainerMenu::ContainerMode containerMode, int menuView);
        static void ExecuteSell(std::vector<std::pair<InventoryEntryData*, std::int32_t>> itemsToSell, TESObjectREFR* vendorActor, TESObjectREFR* vendorContainer, std::int32_t totalSellValue, std::int32_t totalToSell, std::int32_t totalPossibleToSell, float vendorGoldDisplay);

        static TESObjectREFR* PrepareTrashContainer();
        static bool TrashContainerIsEmpty(TESObjectREFR* chest);
        static void EmptyTrashContainer(TESObjectREFR* chest);
        static void NoteTrashDeposit();
        static void ClearTrashStampIfEmpty();
        static std::vector<InventoryEntryData*> BuildInventoryTrashList();
        static void TrashEntryUnits(InventoryEntryData* a_entry, TESObjectREFR* a_from, TESObjectREFR* a_to);
        static void ExecuteTrash(TESObjectREFR* from, TESBoundObject* item, Count count, ExtraDataList* extraList);
        static void ExecuteBulkTrash(std::vector<InventoryEntryData*> trashList);
        static void HideOpenInventoryMenus();
        static void ScheduleActivateTrash(int framesRemaining);

        struct SellWorkItem {
            FormID formId = 0;
            Count count = 0;
        };

        struct SellSession {
            std::vector<SellWorkItem> remaining;
            FormID vendorActorId = 0;
            FormID vendorContainerId = 0;
            Count totalSellValue = 0;
            Count totalToSell = 0;
            Count totalPossibleToSell = 0;
            std::size_t uniqueTypes = 0;
        };

        static std::vector<SellWorkItem> BuildSellWorkList(const std::vector<std::pair<InventoryEntryData*, Count>>& itemsToSell);
        static void SellWorkUnits(std::vector<SellWorkItem>& remaining, TESObjectREFR* from, TESObjectREFR* to, Count maxUnits);
        static void ContinueChunkedSell(SellSession session);
        static void FinishSell(TESObjectREFR* player, TESObjectREFR* vendorActorRef, TESObjectREFR* vendorContainer, Count totalSellValue, Count totalToSell, Count totalPossibleToSell, std::size_t uniqueTypes);

        static void ApplyInventoryUIRefresh(TESObjectREFR* primary, TESObjectREFR* secondary);
        static void ScheduleInventoryUIRefresh(FormID primaryId, FormID secondaryId, int framesRemaining, std::function<void()> onComplete = {});
        static void RefreshMenusAfterBulk(TESObjectREFR* primary, TESObjectREFR* secondary, std::size_t uniqueTypes, Count totalItems);

        static void ShowConfirmationMessageBox(const char* bodyText, std::vector<std::string> buttons, std::function<void(unsigned int)> callback);

        static inline float trashFilledGameDays = 0.0f;
        static inline bool trashStampPending = false;
    };
}
