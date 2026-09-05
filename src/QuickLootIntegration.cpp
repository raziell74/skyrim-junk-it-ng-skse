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
        std::uint32_t g_lootOwner = 0;

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
            g_lootOwner = 0;
        }

        void OnPopulateInfoBar(QuickLoot::API::PopulateInfoBarEvent* e) {
            if (!e || !e->stack || !e->stack->entry) {
                return;
            }
            if (!Settings::GetQuickLootEnabled()) {
                return;
            }
            if (!JunkDataManager::GetSingleton().IsJunk(e->stack->entry)) {
                return;
            }

            const auto& label = I4JunkConfig::GetSingleton().subTypeDisplay;
            e->result.push_back(label.empty() ? "Junk" : label.c_str());
        }

        void OnPopulateButtonBar(QuickLoot::API::PopulateButtonBarEvent* e) {
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

            e->result.push_back({
                label.c_str(),
                static_cast<std::uint16_t>(artKey)});
        }

        void OnOpenLootMenu(QuickLoot::API::OpenLootMenuEvent* e) {
            if (!e) {
                return;
            }
            g_lootOwner = e->container.native_handle();
        }

        void OnSelectItem(QuickLoot::API::SelectItemEvent* e) {
            if (!e || !e->stack || !e->stack->entry) {
                g_selectedEntry = nullptr;
                g_selectedOwner = 0;
                return;
            }
            g_selectedEntry = e->stack->entry;
            g_selectedOwner = e->container.native_handle();
            g_lootOwner = g_selectedOwner;
        }

        void OnCloseLootMenu(QuickLoot::API::CloseLootMenuEvent*) {
            ClearSelection();
        }
    }

    void QuickLootIntegration::Install() {
        if (GetModuleHandleW(L"QuickLootIE") == nullptr) {
            SKSE::log::debug("QuickLootIE not installed; loot menu API integration skipped");
            return;
        }

        if (!QuickLoot::API::QuickLootAPI::Init("JunkIt")) {
            SKSE::log::info("QuickLootIE API V20 unavailable; loot menu mark button disabled");
            return;
        }

        QuickLoot::API::QuickLootAPI::RegisterPopulateInfoBarHandler(&OnPopulateInfoBar);
        QuickLoot::API::QuickLootAPI::RegisterPopulateButtonBarHandler(&OnPopulateButtonBar);
        QuickLoot::API::QuickLootAPI::RegisterOpenLootMenuHandler(&OnOpenLootMenu);
        QuickLoot::API::QuickLootAPI::RegisterSelectItemHandler(&OnSelectItem);
        QuickLoot::API::QuickLootAPI::RegisterCloseLootMenuHandler(&OnCloseLootMenu);
        g_ready = true;
        SKSE::log::info("QuickLootIE V20 API integration installed");
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

    RE::TESObjectREFR* QuickLootIntegration::GetLootContainer() {
        if (g_lootOwner == 0) {
            return nullptr;
        }
        return RE::TESObjectREFR::LookupByHandle(g_lootOwner).get();
    }
}
