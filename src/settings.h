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

            [[nodiscard]] static bool ProtectEquipped() { return JunkProtection.ProtectEquipped; }
            [[nodiscard]] static bool ProtectFavorites() { return JunkProtection.ProtectFavorites; }
            [[nodiscard]] static bool ProtectEnchanted() { return JunkProtection.ProtectEnchanted; }

            [[nodiscard]] static std::int32_t GetIndividualizedMerchantInventory(std::int32_t merchantID) { return IndividualizedMerchantInventories[merchantID]; }

        private: 

            static inline BGSKeyword* IsJunkKYWD;
            static inline BGSListForm* JunkList;
            static inline JunkTransfer JunkTransfer;
            static inline JunkSell JunkSell;
            static inline JunkProtection JunkProtection;

            static inline // Individualized Merchant Inventories - For merchants that fail the merchant faction lookup
            std::unordered_map<std::int32_t, std::int32_t> IndividualizedMerchantInventories = {
                { 0x01A680 , 0x0ABB3F }, // Anoriath > MerchantWhiterunAnoriathChest  
                { 0x01A675 , 0x0ABB41 }, // Carlotta > MerchantWhiterunCarlottaChest  
                { 0x01A684 , 0x10C1D4 }, // Fralia > MerchantWhiterunFraliaChest  
                { 0x01A66E , 0x09CAFB }, // Hulda > MerchantWhiterunBannerdMareChest 
                { 0x0D7505 , 0x09CAFB }, // Saadia > MerchantWhiterunBannerdMareChest 
                { 0x02BA8D , 0x0B298A }, // Sabjorn > MerchantHonningbrewChest 
                { 0x02BA8F , 0x0B298A }, // Mallus Maccius > MerchantHonningbrewChest 
                { 0x01B118 , 0x0A3F00 }, // Elda Early-Dawn > MerchantWindhelmCandlehearthHallChest 
                { 0x01B11B , 0x0A3F00 }, // Susanna the Wicked > MerchantWindhelmCandlehearthHallChest 
                { 0x01BDE7 , 0x0ABD9F }, // Arnbjorn > MerchantDBSanctuaryMerchantChest 
                { 0x01D4BB , 0x0ABD9F }, // Festus Krex > MerchantDBSanctuaryMerchantChest 
                { 0x01BDEB , 0x0ABD9F }, // Gabriella > MerchantDBSanctuaryMerchantChest 
                { 0x01D4BC , 0x0ABD9F }, // Babette > MerchantDBSanctuaryMerchantChest 
                { 0x03A198 , 0x0A6BFA }, // Valga Vinicia > MerchantFalkreathDeadMansDrinkChest  
                { 0x03A199 , 0x0A6BFA }, // Narri > MerchantFalkreathDeadMansDrinkChest  
                { 0x0198D4 , 0x0A6C14 }, // Addvar > MerchantSolitudeFishAddvar  
                { 0x0198B3 , 0x0A6C15 }, // Evette San > MerchantSolitudeFruitEvette  
                { 0x0198AF , 0x0A6C16 }, // Jala > MerchantSolitudeFruitJala  
                { 0x0198A0 , 0x0A6BF8 }, // Corpulus Vinius > MerchantSolitudeWinkingSkeeverChest  
                { 0x019A29 , 0x0A6BF4 }, // Faida > MerchantDragonBridgeFourShieldsTavernChest  
                { 0x0198EC , 0x094385 }, // Kleppr > MerchantMarkarthSilverFishInnChest  
                { 0x0198ED , 0x094385 }, // Frabbi > MerchantMarkarthSilverFishInnChest  
                { 0x0284AD , 0x09437D }, // Hogni Red-Arm > MerchantMarkarthBolisChest  
                { 0x0198F2 , 0x09437B }, // Kerahs > MerchantMarkarthKerahsChest  
                { 0x019DDD , 0x0A31BC }, // Ungrien > MerchantRiftenBlackBriarMeadery  
                { 0x019DC8 , 0x0A0704 }, // Keerava > MerchantRiftenBeeandBarbChest  
                { 0x019DC9 , 0x0A0704 }, // Talen-Jei > MerchantRiftenBeeandBarbChest  
                { 0x021EA6 , 0x0A31BB }, // Madesi > MerchantRiftenGrandPlazaMadesiChest  
                { 0x019DF9 , 0x0A31C0 }, // Vekel the Man > MerchantRiftenRaggedFlagonChest  
                { 0x0AC9E9 , 0x0AC9E3 }, // Arnskar Ember-Master > MerchantTGArnskarChest  
                { 0x019E02 , 0x0A0707 }, // Wilhelm > MerchantIvarsteadVilemyrInnChest  
                { 0x019E03 , 0x0A0707 }, // Lynly Star-Sung > MerchantIvarsteadVilemyrInnChest  
                { 0x013486 , 0x078C0F }, // Orgnar > MerchantRiverwoodSleepingGiantChest  
                { 0x01C1A8 , 0x098BA5 }, // Colette Marence > MerchantWCollegeColetteChest  
                { 0x01C1A6 , 0x098B9F }, // Drevis Neloren > MerchantWCollegeDrevisChest  
                { 0x01C1A5 , 0x098BA6 }, // Faralda > MerchantWCollegeFaraldaChest  
                { 0x01C1A7 , 0x098BA7 }, // Phinis Gestor > MerchantWCollegePhinisChest
            };
    };   
}