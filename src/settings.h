#pragma once

#include "util.h"
#include <list>
#include <string>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace RE;
using nlohmann::json;

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
                BGSListForm* TransferList;
            };

            struct JunkSell {
                bool ConfirmSell = true;
                SortPriority SellPriority = SortPriority::kChaos;
                BGSListForm* SellList;
            };

            struct JunkProtection {
                bool ProtectEquipped = true;
                bool ProtectFavorites = true;
                bool ProtectEnchanted = false;
            };

            static void Load() {
                SKSE::log::info(" ");
                SKSE::log::info("Updating Settings...");

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

                if (auto* form = getForm("JunkList", 0x804)) JunkList = form->As<BGSListForm>();
                if (auto* form = getForm("UnjunkedList", 0x80E)) UnjunkedList = form->As<BGSListForm>();
                if (auto* form = getForm("JunkHistory", 0x80F)) JunkHistory = form->As<BGSListForm>();
                if (auto* form = getForm("IsJunkKYWD", 0x802)) IsJunkKYWD = form->As<BGSKeyword>();

                MarkJunkKey = getGlobalValue("MarkJunkKey", 0x817, MarkJunkKey);
                TransferJunkKey = getGlobalValue("TransferJunkKey", 0x818, TransferJunkKey);
                GamepadJunkKey = getGlobalValue("GamepadJunkKey", 0x819, GamepadJunkKey);
                GamepadTransferHoldTime = getGlobalValue("GamepadTransferHoldTime", 0x81C, GamepadTransferHoldTime);

                JunkTransfer.ConfirmTransfer = getGlobalBool("ConfirmTransfer", 0x808, JunkTransfer.ConfirmTransfer);
                JunkTransfer.TransferPriority = static_cast<SortPriority>(getGlobalValue("TransferPriority", 0x80A, static_cast<float>(JunkTransfer.TransferPriority)));
                if (auto* form = getForm("TransferList", 0x80C)) JunkTransfer.TransferList = form->As<BGSListForm>();

                // Translate the SortPriority to a string for log
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
                if (auto* form = getForm("SellList", 0x80D)) JunkSell.SellList = form->As<BGSListForm>();

                // Translate the SortPriority to a string for log
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
                NotifyLargeInventoryLag = getGlobalBool("NotifyLargeInventoryLag", 0x81D, NotifyLargeInventoryLag);

                WarnInventorySizeThreshold = static_cast<std::int32_t>(getGlobalValue("WarnInventorySizeThreshold", 0x81F, static_cast<float>(WarnInventorySizeThreshold)));
                AggressiveRefresh = getGlobalBool("AggressiveRefresh", 0x820, AggressiveRefresh);

                if (auto* form = getForm("TransferConfirmationMsg", 0x806)) TransferConfirmationMsg = form->As<BGSMessage>();
                if (auto* form = getForm("RetrievalConfirmationMsg", 0x807)) RetrievalConfirmationMsg = form->As<BGSMessage>();
                if (auto* form = getForm("SellConfirmationMsg", 0x805)) SellConfirmationMsg = form->As<BGSMessage>();

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

                AutoLoadJunkListFromFile = getGlobalBool("AutoLoadJunkListFromFile", 0x81A, AutoLoadJunkListFromFile);
                AutoSaveJunkListToFile = getGlobalBool("AutoSaveJunkListToFile", 0x81B, AutoSaveJunkListToFile);
                ReplaceJunkListOnLoad = getGlobalBool("ReplaceJunkListOnLoad", 0x81E, ReplaceJunkListOnLoad);

                SKSE::log::info(
                    "Auto Load/Save Settings | AutoLoadJunkListFromFile: {} | AutoSaveJunkListToFile: {} | ReplaceJunkListOnLoad: {}",
                    AutoLoadJunkListFromFile,
                    AutoSaveJunkListToFile,
                    ReplaceJunkListOnLoad
                );

                SKSE::log::info(" ");
            }

            struct JsonJunkListItem {
                std::string name;
                std::string editorId;
                std::string type;
                std::string source;
            };

            static void SaveJunkListToFile() {
                SKSE::log::info(" ");
                SKSE::log::info("Saving JunkList to file...");

                // create an empty structure (null)
                json junkListJson;
                std::vector<JsonJunkListItem> jsonJunkListItems = {};

                // Convert the JunkList to a string array of Editor Ids
                BSTArray<TESForm*> forms = JunkList->forms;
                std::int32_t count = forms.size();

                // Don't save if no item has ever been marked as junk - Typically happens on a new game
                if (count <= 0 && UnjunkedList->forms.size() <= 0) {
                    SKSE::log::error("JunkList is empty. Nothing to save.");
                    RE::DebugNotification("JunkList is empty. Nothing to save.");
                    return;
                }

                for (std::int32_t i = 0; i < count; i++) {
                    TESForm* itemForm = forms[i];

                    if (!itemForm) {
                        SKSE::log::error("Form is null for index: {}", i);
                        continue;
                    }
                    
                    std::string formConfigString = fmt::format("0x{:X}~{}", itemForm->GetLocalFormID(), itemForm->GetFile(0)->GetFilename());
                    // SKSE::log::info("Adding {} - {} to save list", itemForm->GetName(), formConfigString);
                    JsonJunkListItem junkListItem = {};
                    junkListItem.name = itemForm->GetName();
                    junkListItem.editorId = itemForm->GetFormEditorID(); // This does not work, @todo find a workaround to get the editor id
                    junkListItem.type = std::to_string(itemForm->GetFormType());
                    junkListItem.source = formConfigString;

                    jsonJunkListItems.push_back(junkListItem);
                }

                // Convert the vector to a JSON array
                json jsonJunkList = json::array();
                for (const auto& item : jsonJunkListItems) {
                    json jsonItem = {
                        {"name", item.name},
                        {"type", item.type},
                        {"source", item.source}
                    };
                    jsonJunkList.push_back(jsonItem);
                }

                // Assign the JSON array to junkListJson["Junk"]
                junkListJson["Count"] = count;
                junkListJson["Junk"] = jsonJunkList;

                // Write the JSON to a file
                std::ofstream file(L"Data/SKSE/Plugins/JunkIt/JunkList.json");
                file << junkListJson.dump(4) << "\n\n"; 
                file.close();

                SKSE::log::info("JunkList saved to file 'Data/SKSE/Plugins/JunkIt/JunkList.json'.");
                // SKSE::log::info("{}", junkListJson.dump());
            }

            static RE::BGSListForm* LoadJunkListFromFile() {
                SKSE::log::info(" ");
                SKSE::log::info("Loading JunkList From file 'Data/SKSE/Plugins/JunkIt/JunkList.json'...");

                // We don't want to create a new local variable for the new list so we'll repurpose the existing transfer list to save memory
                BGSListForm* NewJunkList = JunkTransfer.TransferList;
                NewJunkList->ClearData();

                std::ifstream f(L"Data/SKSE/Plugins/JunkIt/JunkList.json");
                // exit if file not found
                if (!f.good()) {
                    SKSE::log::error("JunkList file not found.");
                    return NewJunkList;
                }

                // Parse the JSON file and get the JunkList array
                json junkListJson = json::parse(f);
                json jsonJunkListItems = junkListJson["Junk"];

                // Loop through the string array of Editor Ids and then add each form to the JunkList
                for (std::int32_t i = 0; i < jsonJunkListItems.size(); i++) {
                    json junkItem = jsonJunkListItems[i];
                    auto junkItemConfigString = junkItem["source"];
                    // SKSE::log::info("Looking Up Form Config String: {}", junkItemConfigString);

                    TESForm* form = FormUtil::Form::GetFormFromConfigString(junkItemConfigString);
                    if (!form) {
                        SKSE::log::error("Form not found for Config String: {}", junkItemConfigString);
                        continue;
                    }
                    
                    NewJunkList->AddForm(form);
                    SKSE::log::info("Adding form to JunkList: {} [{}]", form->GetName(), junkItemConfigString);
                }

                SKSE::log::info("JunkList loaded from file.");
                return NewJunkList;
            }

            [[nodiscard]] static BGSListForm* GetJunkList() { return JunkList; }
            [[nodiscard]] static BGSListForm* GetUnjunkedList() { return UnjunkedList; }
            [[nodiscard]] static BGSListForm* GetJunkHistory() { return JunkHistory; }
            [[nodiscard]] static BGSKeyword* GetIsJunkKYWD() { return IsJunkKYWD; }

            [[nodiscard]] static bool ConfirmTransfer() { return JunkTransfer.ConfirmTransfer; }
            [[nodiscard]] static SortPriority GetTransferPriority() { return JunkTransfer.TransferPriority; }
            [[nodiscard]] static BGSListForm* GetTransferList() { return JunkTransfer.TransferList; }

            [[nodiscard]] static bool ConfirmSell() { return JunkSell.ConfirmSell; }
            [[nodiscard]] static SortPriority GetSellPriority() { return JunkSell.SellPriority; }
            [[nodiscard]] static BGSListForm* GetSellList() { return JunkSell.SellList; }

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

            [[nodiscard]] static BGSMessage* GetTransferConfirmationMsg() { return TransferConfirmationMsg; }
            [[nodiscard]] static BGSMessage* GetRetrievalConfirmationMsg() { return RetrievalConfirmationMsg; }
            [[nodiscard]] static BGSMessage* GetSellConfirmationMsg() { return SellConfirmationMsg; }

            [[nodiscard]] static TESObjectMISC* GetGold001() { return Gold001; }

            [[nodiscard]] static bool GetAutoSaveJunkListToFile() { return AutoSaveJunkListToFile; }
            [[nodiscard]] static bool GetAutoLoadJunkListFromFile() { return AutoLoadJunkListFromFile; }

        private: 

            static inline float MarkJunkKey = 0x32;
            static inline float TransferJunkKey = 0x49;
            static inline float GamepadJunkKey = 270.0f;
            static inline float GamepadTransferHoldTime = 2.0f;
            
            static inline bool AutoSaveJunkListToFile = false;
            static inline bool AutoLoadJunkListFromFile = false;
            static inline bool ReplaceJunkListOnLoad = false;

            static inline bool NotifyOnMarkUnmark = true;
            static inline bool NotifyOnJunkTransfer = true;
            static inline bool NotifyOnJunkSell = true;
            static inline bool NotifyLargeInventoryLag = true;
            static inline std::int32_t WarnInventorySizeThreshold = 500;
            static inline bool AggressiveRefresh = false;

            static inline BGSKeyword* IsJunkKYWD;
            static inline BGSListForm* JunkList;
            static inline BGSListForm* UnjunkedList;
            static inline BGSListForm* JunkHistory;
            static inline JunkTransfer JunkTransfer;
            static inline JunkSell JunkSell;
            static inline JunkProtection JunkProtection;

            static inline BGSMessage* TransferConfirmationMsg = nullptr;
            static inline BGSMessage* RetrievalConfirmationMsg = nullptr;
            static inline BGSMessage* SellConfirmationMsg = nullptr;

            static inline TESObjectMISC* Gold001 = nullptr;
    };   
}