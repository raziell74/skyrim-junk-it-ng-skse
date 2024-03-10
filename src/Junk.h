#pragma once 
#include "settings.h"
using namespace RE;

namespace JunkIt {
    class JunkHandler {
        public: 
            static void UpdateItemKeywords() {
                BGSKeyword* isJunkKYWD = JunkIt::Settings::GetIsJunkKYWD();

                BGSListForm* JustList = Settings::GetJunkList();
                JustList->ForEachForm([&](RE::TESForm& form) {
                    BGSKeywordForm* keywordForm = nullptr;
                    
                    if (form.GetFormType() == FormType::Ammo) {
                        // Ammo has to be treated differently as it does not inherit from BGSKeywordForm
                        TESAmmo* ammo = form.As<TESAmmo>();
                        keywordForm = ammo->AsKeywordForm();
                    } else {
                        // Generalized handling for all other form types
                        keywordForm = form.As<BGSKeywordForm>();
                    }

                    if (keywordForm && !keywordForm->HasKeyword(isJunkKYWD)) {
                        // SKSE::log::info("[OnGameLoad] keyword correction: Adding IsJunk keyword on {}", form.GetName());
                        keywordForm->AddKeyword(isJunkKYWD);
                    }

                    return RE::BSContainer::ForEachResult::kContinue;
                });

                BGSListForm* UnjunkedList = Settings::GetUnjunkedList();
                UnjunkedList->ForEachForm([&](RE::TESForm& form) {
                    BGSKeywordForm* keywordForm = nullptr;
                    
                    if (form.GetFormType() == FormType::Ammo) {
                        // Ammo has to be treated differently as it does not inherit from BGSKeywordForm
                        TESAmmo* ammo = form.As<TESAmmo>();
                        keywordForm = ammo->AsKeywordForm();
                    } else {
                        // Generalized handling for all other form types
                        keywordForm = form.As<BGSKeywordForm>();
                    }

                    if (keywordForm && keywordForm->HasKeyword(isJunkKYWD)) {
                        // SKSE::log::info("[OnGameLoad] keyword correction: Removing IsJunk keyword on {}", form.GetName());
                        keywordForm->RemoveKeyword(isJunkKYWD);
                    }

                    return RE::BSContainer::ForEachResult::kContinue;
                });
            }

            static ItemList* GetItemListMenu();

        private:

            static inline ItemList* ItemListMenu;
            static inline std::string MenuName;
    };
}