#pragma once

#include "util.h"

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
                BGSListForm* TransferList;
            };

            struct JunkSell {
                bool ConfirmSell = true;
                SortPriority SellPriority = SortPriority::kChaos;
                BGSListForm* SellList;
            };

            static void Load() {
                SKSE::log::info(" ");
                SKSE::log::info("Updating Settings...");

                std::string priorityString = "";
                JunkList = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x804)->As<BGSListForm>();
                IsJunkKYWD = FormUtil::Form::GetFormFromMod("JunkIt.esp", 0x802)->As<BGSKeyword>();

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
                SKSE::log::info(" ");
            }

            [[nodiscard]] static BGSListForm* GetJunkList() { return JunkList; }
            [[nodiscard]] static BGSKeyword* GetIsJunkKYWD() { return IsJunkKYWD; }

            [[nodiscard]] static bool ConfirmTransfer() { return JunkTransfer.ConfirmTransfer; }
            [[nodiscard]] static SortPriority GetTransferPriority() { return JunkTransfer.TransferPriority; }
            [[nodiscard]] static BGSListForm* GetTransferList() { return JunkTransfer.TransferList; }

            [[nodiscard]] static bool ConfirmSell() { return JunkSell.ConfirmSell; }
            [[nodiscard]] static SortPriority GetSellPriority() { return JunkSell.SellPriority; }
            [[nodiscard]] static BGSListForm* GetSellList() { return JunkSell.SellList; }

        private: 

            static inline BGSKeyword* IsJunkKYWD;
            static inline BGSListForm* JunkList;
            static inline JunkTransfer JunkTransfer;
            static inline JunkSell JunkSell;
    };   
}