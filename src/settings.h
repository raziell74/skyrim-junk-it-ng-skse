#pragma once

#include "util.h"
#include <list>
#include <string>
#include <fstream>

using namespace RE;

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

            struct JunkTransfer {
                bool ConfirmTransfer = true;
                SortPriority TransferPriority = SortPriority::kChaos;
            };

            struct JunkSell {
                bool ConfirmSell = true;
                SortPriority SellPriority = SortPriority::kChaos;
            };

            struct JunkProtection {
                bool ProtectEquipped = true;
                bool ProtectFavorites = true;
                bool ProtectEnchanted = false;
            };

            static void Load() {
                SKSE::log::info(" ");
                SKSE::log::info("Updating Settings...");

                DIIIInstalled = (GetModuleHandleA("DynamicInventoryIconInjector.dll") != nullptr);
                SKSE::log::info("DIII Detection | DynamicInventoryIconInjector.dll loaded: {}", DIIIInstalled);

                auto getForm = [](const char* formDesc, uint32_t formId) -> TESForm* {
                    TESForm* form = FormUtil::Form::GetFormFromMod("JunkIt.esp", formId);
                    if (!form) {
                        SKSE::log::error("Failed to load {} (0x{:X}) from JunkIt.esp", formDesc, formId);
                    }
                    return form;
                };

                auto getGlobalValue = [&](const char* formDesc, uint32_t formId, float fallback) -> float {
                    TESForm* form = getForm(formDesc, formId);
                    TESGlobal* global = form ? form->As<TESGlobal>() : nullptr;
                    if (!global) {
                        SKSE::log::error("Failed to cast {} (0x{:X}) to TESGlobal, using default {}", formDesc, formId, fallback);
                        return fallback;
                    }
                    return global->value;
                };

                auto getGlobalBool = [&](const char* formDesc, uint32_t formId, bool fallback) -> bool {
                    TESForm* form = getForm(formDesc, formId);
                    TESGlobal* global = form ? form->As<TESGlobal>() : nullptr;
                    if (!global) {
                        SKSE::log::error("Failed to cast {} (0x{:X}) to TESGlobal, using default {}", formDesc, formId, fallback);
                        return fallback;
                    }
                    return global->value != 0;
                };

                std::string priorityString = "";

                // Keep old FormList references for migration only
                if (auto* form = getForm("JunkList", 0x804)) JunkList = form->As<BGSListForm>();

                MarkJunkKey = getGlobalValue("MarkJunkKey", 0x817, MarkJunkKey);
                TransferJunkKey = getGlobalValue("TransferJunkKey", 0x818, TransferJunkKey);
                GamepadJunkKey = getGlobalValue("GamepadJunkKey", 0x81C, GamepadJunkKey);
                GamepadTransferHoldTime = getGlobalValue("GamepadTransferHoldTime", 0x81D, GamepadTransferHoldTime);

                JunkTransfer.ConfirmTransfer = getGlobalBool("ConfirmTransfer", 0x808, JunkTransfer.ConfirmTransfer);
                JunkTransfer.TransferPriority = static_cast<SortPriority>(getGlobalValue("TransferPriority", 0x80A, static_cast<float>(JunkTransfer.TransferPriority)));

                switch (JunkTransfer.TransferPriority) {
                    case SortPriority::kWeightHighLow:
                        priorityString = "Weight [High > Low]";
                        break;
                    case SortPriority::kWeightLowHigh:
                        priorityString = "Weight [Low > High]";
                        break;
                    case SortPriority::kValueHighLow:
                        priorityString = "Value [High > Low]";
                        break;
                    case SortPriority::kValueLowHigh:
                        priorityString = "Value [Low > High]";
                        break;
                    case SortPriority::kValueWeightHighLow:
                        priorityString = "Value/Weight [High > Low]";
                        break;
                    case SortPriority::kValueWeightLowHigh:
                        priorityString = "Value/Weight [Low > High]";
                        break;
                    case SortPriority::kChaos:
                        priorityString = "Chaos";
                        break;
                }

                SKSE::log::info(
                    "Transfer Option Settings | ConfirmTransfer: {} | TransferPriority: {}", 
                    JunkTransfer.ConfirmTransfer,
                    priorityString
                );

                JunkSell.ConfirmSell = getGlobalBool("ConfirmSell", 0x809, JunkSell.ConfirmSell);
                JunkSell.SellPriority = static_cast<SortPriority>(getGlobalValue("SellPriority", 0x80B, static_cast<float>(JunkSell.SellPriority)));

                switch (JunkSell.SellPriority) {
                    case SortPriority::kWeightHighLow:
                        priorityString = "Weight [High > Low]";
                        break;
                    case SortPriority::kWeightLowHigh:
                        priorityString = "Weight [Low > High]";
                        break;
                    case SortPriority::kValueHighLow:
                        priorityString = "Value [High > Low]";
                        break;
                    case SortPriority::kValueLowHigh:
                        priorityString = "Value [Low > High]";
                        break;
                    case SortPriority::kValueWeightHighLow:
                        priorityString = "Value/Weight [High > Low]";
                        break;
                    case SortPriority::kValueWeightLowHigh:
                        priorityString = "Value/Weight [Low > High]";
                        break;
                    case SortPriority::kChaos:
                        priorityString = "Chaos";
                        break;
                }

                SKSE::log::info(
                    "Sell Option Settings | ConfirmSell: {} | SellPriority: {}",
                    JunkSell.ConfirmSell,
                    priorityString
                );

                JunkProtection.ProtectEquipped = getGlobalBool("ProtectEquipped", 0x810, JunkProtection.ProtectEquipped);
                JunkProtection.ProtectFavorites = getGlobalBool("ProtectFavorites", 0x811, JunkProtection.ProtectFavorites);
                JunkProtection.ProtectEnchanted = getGlobalBool("ProtectEnchanted", 0x813, JunkProtection.ProtectEnchanted);

                SKSE::log::info(
                    "Protection Settings | ProtectEquipped: {} | ProtectFavorites: {} | ProtectEnchanted: {}",
                    JunkProtection.ProtectEquipped,
                    JunkProtection.ProtectFavorites,
                    JunkProtection.ProtectEnchanted
                );

                NotifyOnMarkUnmark = getGlobalBool("NotifyOnMarkUnmark", 0x814, NotifyOnMarkUnmark);
                NotifyOnJunkTransfer = getGlobalBool("NotifyOnJunkTransfer", 0x815, NotifyOnJunkTransfer);
                NotifyOnJunkSell = getGlobalBool("NotifyOnJunkSell", 0x816, NotifyOnJunkSell);
                NotifyLargeInventoryLag = getGlobalBool("NotifyLargeInventoryLag", 0x819, NotifyLargeInventoryLag);

                WarnInventorySizeThreshold = static_cast<std::int32_t>(getGlobalValue("WarnInventorySizeThreshold", 0x821, static_cast<float>(WarnInventorySizeThreshold)));
                AggressiveRefresh = getGlobalBool("AggressiveRefresh", 0x822, AggressiveRefresh);

                AutoExport = getGlobalBool("AutoExport", 0x826, AutoExport);
                AutoImport = getGlobalBool("AutoImport", 0x827, AutoImport);

                UpdateSubTypeDisplay = getGlobalBool("UpdateSubTypeDisplay", 0x823, UpdateSubTypeDisplay);
                UpdateItemIcon = getGlobalBool("UpdateItemIcon", 0x824, UpdateItemIcon);
                UseDynamicInventoryIcon = getGlobalBool("UseDynamicInventoryIcon", 0x825, UseDynamicInventoryIcon);

                if (!DIIIInstalled && UseDynamicInventoryIcon) {
                    SKSE::log::info("DIII not installed, forcing UseDynamicInventoryIcon to false");
                    UseDynamicInventoryIcon = false;
                }

                if (auto* form = getForm("TransferConfirmationMsg", 0x805)) TransferConfirmationMsg = form->As<BGSMessage>();
                if (auto* form = getForm("RetrievalConfirmationMsg", 0x806)) RetrievalConfirmationMsg = form->As<BGSMessage>();
                if (auto* form = getForm("SellConfirmationMsg", 0x807)) SellConfirmationMsg = form->As<BGSMessage>();

                auto* goldForm = RE::TESForm::LookupByID(0xF);
                if (goldForm) {
                    Gold001 = goldForm->As<TESObjectMISC>();
                } else {
                    SKSE::log::error("Failed to lookup Gold001 (0xF)");
                }

                SKSE::log::info(
                    "Notification Settings | NotifyOnMarkUnmark: {} | NotifyOnJunkTransfer: {} | NotifyOnJunkSell: {} | NotifyLargeInventoryLag: {}",
                    NotifyOnMarkUnmark,
                    NotifyOnJunkTransfer,
                    NotifyOnJunkSell,
                    NotifyLargeInventoryLag
                );

                SKSE::log::info(
                    "Hotkey Settings | MarkJunkKey: {} | TransferJunkKey: {} | GamepadJunkKey: {} | GamepadTransferHoldTime: {}",
                    MarkJunkKey,
                    TransferJunkKey,
                    GamepadJunkKey,
                    GamepadTransferHoldTime
                );

                SKSE::log::info(
                    "Misc Settings | WarnInventorySizeThreshold: {} | AggressiveRefresh: {} | AutoExport: {} | AutoImport: {}",
                    WarnInventorySizeThreshold,
                    AggressiveRefresh,
                    AutoExport,
                    AutoImport
                );

                SKSE::log::info(
                    "Integration Settings | UpdateItemIcon: {} | UpdateSubTypeDisplay: {} | UseDynamicInventoryIcon: {}",
                    UpdateItemIcon,
                    UpdateSubTypeDisplay,
                    UseDynamicInventoryIcon
                );

                SKSE::log::info(" ");
            }

            [[nodiscard]] static BGSListForm* GetJunkList() { return JunkList; }

            [[nodiscard]] static bool ConfirmTransfer() { return JunkTransfer.ConfirmTransfer; }
            [[nodiscard]] static SortPriority GetTransferPriority() { return JunkTransfer.TransferPriority; }

            [[nodiscard]] static bool ConfirmSell() { return JunkSell.ConfirmSell; }
            [[nodiscard]] static SortPriority GetSellPriority() { return JunkSell.SellPriority; }

            [[nodiscard]] static bool ProtectEquipped() { return JunkProtection.ProtectEquipped; }
            [[nodiscard]] static bool ProtectFavorites() { return JunkProtection.ProtectFavorites; }
            [[nodiscard]] static bool ProtectEnchanted() { return JunkProtection.ProtectEnchanted; }

            [[nodiscard]] static float GetMarkJunkKey() { return MarkJunkKey; }
            [[nodiscard]] static float GetTransferJunkKey() { return TransferJunkKey; }
            [[nodiscard]] static float GetGamepadJunkKey() { return GamepadJunkKey; }
            [[nodiscard]] static float GetGamepadTransferHoldTime() { return GamepadTransferHoldTime; }

            [[nodiscard]] static bool GetNotifyOnMarkUnmark() { return NotifyOnMarkUnmark; }
            [[nodiscard]] static bool GetNotifyOnJunkTransfer() { return NotifyOnJunkTransfer; }
            [[nodiscard]] static bool GetNotifyOnJunkSell() { return NotifyOnJunkSell; }
            [[nodiscard]] static bool GetNotifyLargeInventoryLag() { return NotifyLargeInventoryLag; }
            [[nodiscard]] static std::int32_t GetWarnInventorySizeThreshold() { return WarnInventorySizeThreshold; }
            [[nodiscard]] static bool GetAggressiveRefresh() { return AggressiveRefresh; }

            [[nodiscard]] static bool GetAutoExport() { return AutoExport; }
            [[nodiscard]] static bool GetAutoImport() { return AutoImport; }

            [[nodiscard]] static bool GetUpdateItemIcon() { 
                if (DIIIInstalled && UseDynamicInventoryIcon) {
                    return false;
                }
                return UpdateItemIcon;
            }
            [[nodiscard]] static bool GetUpdateSubTypeDisplay() { return UpdateSubTypeDisplay; }
            [[nodiscard]] static bool GetUseDynamicInventoryIcon() { return UseDynamicInventoryIcon; }

            [[nodiscard]] static bool IsDIIIInstalled() { return DIIIInstalled; }

            [[nodiscard]] static BGSMessage* GetTransferConfirmationMsg() { return TransferConfirmationMsg; }
            [[nodiscard]] static BGSMessage* GetRetrievalConfirmationMsg() { return RetrievalConfirmationMsg; }
            [[nodiscard]] static BGSMessage* GetSellConfirmationMsg() { return SellConfirmationMsg; }

            [[nodiscard]] static TESObjectMISC* GetGold001() { return Gold001; }

        private: 

            static inline float MarkJunkKey = 0x32;
            static inline float TransferJunkKey = 0x49;
            static inline float GamepadJunkKey = 270.0f;
            static inline float GamepadTransferHoldTime = 2.0f;

            static inline bool NotifyOnMarkUnmark = true;
            static inline bool NotifyOnJunkTransfer = true;
            static inline bool NotifyOnJunkSell = true;
            static inline bool NotifyLargeInventoryLag = true;
            static inline std::int32_t WarnInventorySizeThreshold = 500;
            static inline bool AggressiveRefresh = false;

            static inline bool AutoExport = false;
            static inline bool AutoImport = false;

            static inline bool UpdateSubTypeDisplay = true;
            static inline bool UpdateItemIcon = true;
            static inline bool UseDynamicInventoryIcon = true;

            static inline bool DIIIInstalled = false;

            static inline BGSListForm* JunkList;  // Only for migration
            static inline JunkTransfer JunkTransfer;
            static inline JunkSell JunkSell;
            static inline JunkProtection JunkProtection;

            static inline BGSMessage* TransferConfirmationMsg = nullptr;
            static inline BGSMessage* RetrievalConfirmationMsg = nullptr;
            static inline BGSMessage* SellConfirmationMsg = nullptr;

            static inline TESObjectMISC* Gold001 = nullptr;
    };   
}