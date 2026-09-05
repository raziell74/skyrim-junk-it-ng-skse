#include "QuickLootIntegration.h"

#include "I4Integration.h"
#include "JunkData.h"
#include "Translation.h"
#include "junk.h"
#include "settings.h"

#include <windows.h>

#include "QuickLootAPI.h"

namespace JunkIt {
    namespace {
        bool g_ready = false;
        bool g_gamepadInput = false;
        bool g_gamepadInputKnown = false;
        RE::InventoryEntryData* g_selectedEntry = nullptr;
        std::uint32_t g_selectedOwner = 0;

        bool IconsAllowed() {
            return g_ready && Settings::GetQuickLootEnabled() && Settings::GetQuickLootIcons();
        }

        bool UsingGamepad() {
            if (g_gamepadInputKnown) {
                return g_gamepadInput;
            }
            auto* mgr = RE::BSInputDeviceManager::GetSingleton();
            return mgr && mgr->IsGamepadEnabled();
        }

        void ClearSelection() {
            g_selectedEntry = nullptr;
            g_selectedOwner = 0;
        }

        void OnModifyItemData(QuickLoot::API::ModifyItemDataEvent* e) {
            if (!e || !IconsAllowed()) {
                return;
            }

            const bool isJunk = e->stack && e->stack->entry && JunkDataManager::GetSingleton().IsJunk(e->stack->entry);
            if (!e->data.IsObject()) {
                return;
            }
            e->data.SetMember("isJunk", isJunk);
            e->data.SetMember("isJunkIcon", isJunk);
            e->data.SetMember("isJunkSubType", isJunk);
        }

        void OnPopulateInfoBar(QuickLoot::API::PopulateInfoBarEvent* e) {
            if (!e || !e->stack || !e->stack->entry) {
                return;
            }
            if (!IconsAllowed()) {
                return;
            }
            if (!JunkDataManager::GetSingleton().IsJunk(e->stack->entry)) {
                return;
            }

            const auto& label = I4JunkConfig::GetSingleton().subTypeDisplay;
            e->result.push_back(label.empty() ? "Junk" : label.c_str());
        }

        void OnModifyButtonBar(QuickLoot::API::ModifyButtonBarEvent* e) {
            if (!e || !QuickLootIntegration::MarkAllowed()) {
                return;
            }
            if (!e->stack || !e->stack->entry) {
                return;
            }

            const auto artKey = UsingGamepad()
                ? static_cast<std::uint32_t>(Settings::GetGamepadJunkKey())
                : static_cast<std::uint32_t>(Settings::GetMarkJunkKey());
            if (artKey == 0) {
                return;
            }

            const bool isJunk = JunkDataManager::GetSingleton().IsJunk(e->stack->entry);
            const auto& label = Translation::Get(
                isJunk ? "$JunkIt_QuickLoot_Unmark" : "$JunkIt_QuickLoot_Mark");

            e->buttons.emplace_back(
                label.c_str(),
                static_cast<std::uint16_t>(artKey),
                false,
                QuickLoot::API::QuickLootAction::kNone);
        }

        void OnSelectItem(QuickLoot::API::SelectItemEvent* e) {
            if (!e || !e->stack || !e->stack->entry) {
                ClearSelection();
                return;
            }
            g_selectedEntry = e->stack->entry;
            g_selectedOwner = e->container.native_handle();
        }

        void OnCloseLootMenu(QuickLoot::API::CloseLootMenuEvent*) {
            ClearSelection();
        }
    }

    void QuickLootIntegration::Install() {
        if (GetModuleHandleW(L"QuickLootIE") == nullptr) {
            SKSE::log::debug("QuickLootIE not installed; loot menu junk icons disabled");
            return;
        }

        if (!QuickLoot::API::QuickLootAPI::Init("JunkIt", QuickLoot::API::ApiVersion::kV21)) {
            SKSE::log::warn("QuickLootIE API v21 unavailable; loot menu junk icons disabled");
            return;
        }

        QuickLoot::API::QuickLootAPI::RegisterModifyItemDataHandler(&OnModifyItemData);
        QuickLoot::API::QuickLootAPI::RegisterPopulateInfoBarHandler(&OnPopulateInfoBar);
        QuickLoot::API::QuickLootAPI::RegisterModifyButtonBarHandler(&OnModifyButtonBar);
        QuickLoot::API::QuickLootAPI::RegisterSelectItemHandler(&OnSelectItem);
        QuickLoot::API::QuickLootAPI::RegisterCloseLootMenuHandler(&OnCloseLootMenu);
        g_ready = true;
        SKSE::log::info("QuickLootIE junk icon and mark button integration installed");
    }

    void QuickLootIntegration::RefreshMenu() {
        if (!g_ready) {
            return;
        }
        QuickLoot::API::QuickLootAPI::RefreshLootMenu();
    }

    void QuickLootIntegration::NoteInputDevice(RE::INPUT_DEVICE device) {
        bool gamepad = false;
        switch (device) {
            case RE::INPUT_DEVICE::kGamepad:
                gamepad = true;
                break;
            case RE::INPUT_DEVICE::kKeyboard:
            case RE::INPUT_DEVICE::kMouse:
                gamepad = false;
                break;
            default:
                return;
        }

        if (g_gamepadInputKnown && g_gamepadInput == gamepad) {
            return;
        }
        g_gamepadInputKnown = true;
        g_gamepadInput = gamepad;
        if (IsMenuOpen()) {
            RefreshMenu();
        }
    }

    void QuickLootIntegration::ToggleSelectedJunk() {
        if (!MarkAllowed()) {
            return;
        }
        if (!g_selectedEntry) {
            SKSE::log::debug("QuickLoot mark ignored: no selected item");
            return;
        }

        auto* entry = g_selectedEntry;
        const auto owner = g_selectedOwner;
        JunkHandler::ToggleEntryJunk(entry, owner, JunkHandler::JunkToggleUi::kLootMenu);
    }

    bool QuickLootIntegration::IsReady() {
        return g_ready;
    }

    bool QuickLootIntegration::IsMenuOpen() {
        if (!g_ready) {
            return false;
        }
        const auto ui = RE::UI::GetSingleton();
        return ui && ui->IsMenuOpen("LootMenu");
    }

    bool QuickLootIntegration::MarkAllowed() {
        return g_ready && Settings::GetQuickLootEnabled() && Settings::GetQuickLootMarkButton();
    }
}
