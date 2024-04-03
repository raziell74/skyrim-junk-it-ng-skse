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

                std::string priorityString = "";
                JunkList = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x804)->As<BGSListForm>();
                UnjunkedList = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x80E)->As<BGSListForm>();
                IsJunkKYWD = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x802)->As<BGSKeyword>();

                MarkJunkKey = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x817)->As<TESGlobal>()->value;
                TransferJunkKey = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x818)->As<TESGlobal>()->value;

                TESGlobal* ConfirmTransfer = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x808)->As<TESGlobal>();
                TESGlobal* TransferPriority = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x80A)->As<TESGlobal>();
                BGSListForm* TransferList = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x80C)->As<BGSListForm>();

                JunkTransfer.ConfirmTransfer = ConfirmTransfer->value != 0;
                JunkTransfer.TransferPriority = static_cast<SortPriority>(TransferPriority->value);
                JunkTransfer.TransferList = TransferList;

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

                TESGlobal* ConfirmSell = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x809)->As<TESGlobal>();
                TESGlobal* SellPriority = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x80B)->As<TESGlobal>();
                BGSListForm* SellList = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x80D)->As<BGSListForm>();

                JunkSell.ConfirmSell = ConfirmSell->value != 0;
                JunkSell.SellPriority = static_cast<SortPriority>(SellPriority->value);
                JunkSell.SellList = SellList;

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

                TESGlobal* ProtectEquipped = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x810)->As<TESGlobal>();
                TESGlobal* ProtectFavorites = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x811)->As<TESGlobal>();
                TESGlobal* ProtectEnchanted = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x813)->As<TESGlobal>();

                JunkProtection.ProtectEquipped = ProtectEquipped->value != 0;
                JunkProtection.ProtectFavorites = ProtectFavorites->value != 0;
                JunkProtection.ProtectEnchanted = ProtectEnchanted->value != 0;

                SKSE::log::info(
                    "Protection Settings | ProtectEquipped: {} | ProtectFavorites: {} | ProtectEnchanted: {}",
                    JunkProtection.ProtectEquipped,
                    JunkProtection.ProtectFavorites,
                    JunkProtection.ProtectEnchanted
                );

                TESGlobal* AutoLoadJunkListFromFileGlobal = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x81A)->As<TESGlobal>();
                AutoLoadJunkListFromFile = AutoLoadJunkListFromFileGlobal->value != 0;

                TESGlobal* AutoSaveJunkListToFileGlobal = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x81B)->As<TESGlobal>();
                AutoSaveJunkListToFile = AutoSaveJunkListToFileGlobal->value != 0;

                TESGlobal* ReplaceJunkListOnLoadGlobal = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x81E)->As<TESGlobal>();
                ReplaceJunkListOnLoad = ReplaceJunkListOnLoadGlobal->value != 0;

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

            [[nodiscard]] static bool GetAutoSaveJunkListToFile() { return AutoSaveJunkListToFile; }
            [[nodiscard]] static bool GetAutoLoadJunkListFromFile() { return AutoLoadJunkListFromFile; }

        private: 

            static inline float MarkJunkKey = 0x32;
            static inline float TransferJunkKey = 0x49;
            
            static inline bool AutoSaveJunkListToFile = false;
            static inline bool AutoLoadJunkListFromFile = false;
            static inline bool ReplaceJunkListOnLoad = false;

            static inline BGSKeyword* IsJunkKYWD;
            static inline BGSListForm* JunkList;
            static inline BGSListForm* UnjunkedList;
            static inline JunkTransfer JunkTransfer;
            static inline JunkSell JunkSell;
            static inline JunkProtection JunkProtection;
    };   
}