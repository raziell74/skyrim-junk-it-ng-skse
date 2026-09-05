#include "QuickLootIntegration.h"

#include "I4Integration.h"
#include "JunkData.h"
#include "settings.h"

#include <windows.h>

#include "QuickLootAPI.h"

namespace JunkIt {
    namespace {
        bool g_ready = false;

        void OnModifyItemData(QuickLoot::API::ModifyItemDataEvent* e) {
            if (!e) {
                return;
            }

            const bool isJunk = e->stack && e->stack->entry && JunkDataManager::GetSingleton().IsJunk(e->stack->entry);
            I4Integration::SetJunkFlags(e->data, isJunk);
        }

        void OnPopulateInfoBar(QuickLoot::API::PopulateInfoBarEvent* e) {
            if (!e || !e->stack || !e->stack->entry) {
                return;
            }
            if (!Settings::GetUpdateSubTypeDisplay()) {
                return;
            }
            if (!JunkDataManager::GetSingleton().IsJunk(e->stack->entry)) {
                return;
            }

            const auto& label = I4JunkConfig::GetSingleton().subTypeDisplay;
            e->result.push_back(label.empty() ? "Junk" : label.c_str());
        }
    }

    void QuickLootIntegration::Install() {
        if (GetModuleHandleW(L"QuickLootIE") == nullptr) {
            SKSE::log::info("QuickLootIE not installed; loot menu junk icons disabled");
            return;
        }

        if (!QuickLoot::API::QuickLootAPI::Init("JunkIt", QuickLoot::API::ApiVersion::kV21)) {
            SKSE::log::warn("QuickLootIE API v21 unavailable; loot menu junk icons disabled");
            return;
        }

        QuickLoot::API::QuickLootAPI::RegisterModifyItemDataHandler(&OnModifyItemData);
        QuickLoot::API::QuickLootAPI::RegisterPopulateInfoBarHandler(&OnPopulateInfoBar);
        g_ready = true;
        SKSE::log::info("QuickLootIE junk icon integration installed");
    }

    void QuickLootIntegration::RefreshMenu() {
        if (!g_ready) {
            return;
        }
        QuickLoot::API::QuickLootAPI::RefreshLootMenu();
    }
}
