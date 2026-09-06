#pragma once

#include "util.h"

#include <string>
#include <string_view>
#include <vector>

namespace JunkIt {
    class Settings {
        public:
            enum class SortPriority {
                kWeightHighLow = 0,
                kWeightLowHigh = 1,
                kValueHighLow = 2,
                kValueLowHigh = 3,
                kValueWeightHighLow = 4,
                kValueWeightLowHigh = 5,
                kChaos = 6
            };

            enum class SkyPromptButtonPlacement {
                kAttachToItemModel = 0,
                kLowerRight = 1
            };

            enum class LogLevel {
                kTrace = 0,
                kDebug = 1,
                kInfo = 2,
                kWarn = 3,
                kError = 4
            };

            // Update after placing the trash container in JunkIt.esp (placed REFR local FormID, not the CONT base).
            static constexpr RE::FormID kTrashContainerFormID = 0x829;
            static constexpr std::string_view kTrashContainerPlugin = "JunkIt.esp";

            static void LoadFromIni();
            static bool SaveToIni();
            static void ResetToDefaults();
            static void LoadGameForms();

            [[nodiscard]] static bool ConfirmTransfer();
            [[nodiscard]] static SortPriority GetTransferPriority();

            [[nodiscard]] static bool ConfirmSell();
            [[nodiscard]] static SortPriority GetSellPriority();

            [[nodiscard]] static bool ProtectEquipped();
            [[nodiscard]] static bool ProtectFavorites();
            [[nodiscard]] static bool ProtectEnchanted();

            [[nodiscard]] static float GetMarkJunkKey();
            [[nodiscard]] static float GetTransferJunkKey();
            [[nodiscard]] static float GetGamepadJunkKey();
            [[nodiscard]] static float GetGamepadTransferHoldTime();
            [[nodiscard]] static float GetTrashJunkKey();
            [[nodiscard]] static std::int32_t GetTrashHoldSeconds();
            [[nodiscard]] static std::int32_t GetGamepadTrashHoldSeconds();
            [[nodiscard]] static std::int32_t GetTrashExpireDays();
            [[nodiscard]] static RE::TESObjectREFR* GetTrashContainer();
            [[nodiscard]] static bool IsTrashAvailable();

            [[nodiscard]] static LogLevel GetLogLevel();
            [[nodiscard]] static bool GetNotifyOnMarkUnmark();
            [[nodiscard]] static bool GetNotifyOnJunkTransfer();
            [[nodiscard]] static bool GetNotifyOnJunkSell();
            [[nodiscard]] static std::int32_t GetOverlayOpacity();
            [[nodiscard]] static bool GetAggressiveRefresh();
            [[nodiscard]] static std::int32_t GetAggressiveRefreshMaxInterval();
            [[nodiscard]] static float GetHeavyLoadDelayMultiplier();
            [[nodiscard]] static std::size_t GetLargeUniqueTypes();
            [[nodiscard]] static std::int32_t GetLargeTotalItems();
            [[nodiscard]] static std::int32_t GetSellChunkSize();

            [[nodiscard]] static bool GetAutoExport();
            [[nodiscard]] static bool GetAutoImport();
            [[nodiscard]] static bool GetReplaceJunkListOnLoad();

            [[nodiscard]] static bool GetUpdateItemIcon();
            [[nodiscard]] static bool GetUpdateSubTypeDisplay();
            [[nodiscard]] static bool GetUseDynamicInventoryIcon();
            [[nodiscard]] static bool GetSkyPromptEnabled();
            [[nodiscard]] static SkyPromptButtonPlacement GetSkyPromptButtonPlacement();
            [[nodiscard]] static bool GetSkyPromptShowCounts();
            [[nodiscard]] static bool GetQuickLootEnabled();
            [[nodiscard]] static bool GetQuickLootMarkButton();

            [[nodiscard]] static bool GetAutoJunkOnPickup();
            [[nodiscard]] static bool GetAutoJunkOnMenuOpen();
            [[nodiscard]] static const std::vector<std::string>& GetAutoJunkTypes();
            static bool TryAddAutoJunkType(std::string_view type);
            static bool RemoveAutoJunkTypeAt(std::size_t index);
            [[nodiscard]] static const std::vector<std::string>& GetAutoJunkMaterials();
            static bool TryAddAutoJunkMaterial(std::string_view material);
            static bool RemoveAutoJunkMaterialAt(std::size_t index);
            [[nodiscard]] static const std::vector<std::string>& GetAutoJunkKeywords();
            static bool TryAddAutoJunkKeyword(std::string_view keyword);
            static bool RemoveAutoJunkKeywordAt(std::size_t index);

            [[nodiscard]] static bool IsDIIIInstalled();
            [[nodiscard]] static bool IsSkyPromptInstalled();
            [[nodiscard]] static bool IsQuickLootInstalled();
            [[nodiscard]] static RE::TESObjectMISC* GetGold001();

            static std::uint32_t& MarkJunkKeyValue();
            static std::uint32_t& TransferJunkKeyValue();
            static std::uint32_t& GamepadJunkKeyValue();
            static std::uint32_t& TrashJunkKeyValue();
            static std::int32_t& GamepadTransferHoldTimeValue();
            static bool& EnableTrashValue();
            static std::int32_t& TrashHoldSecondsValue();
            static std::int32_t& GamepadTrashHoldSecondsValue();
            static std::int32_t& TrashExpireDaysValue();

            static bool& ConfirmTransferValue();
            static bool& ConfirmSellValue();
            static std::int32_t& TransferPriorityValue();
            static std::int32_t& SellPriorityValue();

            static bool& ProtectEquippedValue();
            static bool& ProtectFavoritesValue();
            static bool& ProtectEnchantedValue();

            static std::int32_t& LogLevelValue();
            static bool& NotifyOnMarkUnmarkValue();
            static bool& NotifyOnJunkTransferValue();
            static bool& NotifyOnJunkSellValue();
            static std::int32_t& OverlayOpacityValue();
            static float& HeavyLoadDelayMultiplierValue();
            static std::int32_t& LargeUniqueTypesValue();
            static std::int32_t& LargeTotalItemsValue();
            static std::int32_t& SellChunkSizeValue();

            static bool& UpdateItemIconValue();
            static bool& UpdateSubTypeDisplayValue();
            static bool& UseDynamicInventoryIconValue();
            static bool& SkyPromptEnabledValue();
            static std::int32_t& SkyPromptButtonPlacementValue();
            static bool& SkyPromptShowCountsValue();
            static bool& QuickLootEnabledValue();
            static bool& QuickLootMarkButtonValue();

            static bool& AutoJunkOnPickupValue();
            static bool& AutoJunkOnMenuOpenValue();

            static bool& AutoExportValue();
            static bool& AutoImportValue();
            static bool& ReplaceJunkListOnLoadValue();
            static bool& AggressiveRefreshValue();
            static std::int32_t& AggressiveRefreshMaxIntervalValue();

            static void ApplyLogLevel();
            static void ApplyIntegrationGuards();
            static void ClampValues();
            static const char* SortPriorityLabel(SortPriority priority);
            static const char* LogLevelLabel(LogLevel level);
    };
}
