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
        using UEFlag = RE::UserEvents::USER_EVENT_FLAG;

        constexpr UEFlag kJunkItEventGroup = static_cast<UEFlag>(1 << 13);

        bool g_ready = false;
        bool g_gamepadInput = false;
        bool g_gamepadInputKnown = false;
        bool g_inputBlocked = false;
        RE::InventoryEntryData* g_selectedEntry = nullptr;
        std::uint32_t g_selectedOwner = 0;
        std::uint32_t g_lootOwner = 0;

        bool MappingMatchesJunkKey(const RE::ControlMap::UserEventMapping& mapping, RE::INPUT_DEVICE device) {
            if (mapping.inputKey == RE::ControlMap::kInvalid || mapping.userEventGroupFlag.all(UEFlag::kInvalid)) {
                return false;
            }

            const auto markKey = static_cast<std::uint32_t>(Settings::GetMarkJunkKey());
            const auto gamepadKey = static_cast<std::uint32_t>(Settings::GetGamepadJunkKey());

            switch (device) {
                case RE::INPUT_DEVICE::kKeyboard:
                    return markKey > 0 && markKey < SKSE::InputMap::kMacro_NumKeyboardKeys &&
                        mapping.inputKey == static_cast<std::uint16_t>(markKey);
                case RE::INPUT_DEVICE::kMouse: {
                    if (markKey < SKSE::InputMap::kMacro_MouseButtonOffset ||
                        markKey >= SKSE::InputMap::kMacro_GamepadOffset) {
                        return false;
                    }
                    const auto mouseKey = markKey - SKSE::InputMap::kMacro_MouseButtonOffset;
                    return mapping.inputKey == static_cast<std::uint16_t>(mouseKey);
                }
                case RE::INPUT_DEVICE::kGamepad: {
                    if (gamepadKey == 0) {
                        return false;
                    }
                    const auto mask = SKSE::InputMap::GamepadKeycodeToMask(gamepadKey);
                    return mask != 0xFF && mapping.inputKey == static_cast<std::uint16_t>(mask);
                }
                default:
                    return false;
            }
        }

        template <typename Fn>
        void WalkGameplayMappings(Fn&& fn) {
            auto* controlMap = RE::ControlMap::GetSingleton();
            if (!controlMap) {
                return;
            }

            auto* context = controlMap->controlMap[RE::UserEvents::INPUT_CONTEXT_ID::kGameplay];
            if (!context) {
                return;
            }

            constexpr RE::INPUT_DEVICE devices[] = {
                RE::INPUT_DEVICE::kKeyboard,
                RE::INPUT_DEVICE::kMouse,
                RE::INPUT_DEVICE::kGamepad
            };
            for (const auto device : devices) {
                for (auto& mapping : context->deviceMappings[device]) {
                    fn(mapping, device);
                }
            }
        }

        void TagMatchingMappings() {
            WalkGameplayMappings([&](RE::ControlMap::UserEventMapping& mapping, RE::INPUT_DEVICE device) {
                if (MappingMatchesJunkKey(mapping, device)) {
                    mapping.userEventGroupFlag.set(kJunkItEventGroup);
                }
            });
        }

        void ClearTaggedMappings() {
            WalkGameplayMappings([&](RE::ControlMap::UserEventMapping& mapping, RE::INPUT_DEVICE) {
                mapping.userEventGroupFlag.reset(kJunkItEventGroup);
            });
        }

        void UnblockConflictingInputs() {
            if (!g_inputBlocked) {
                return;
            }

            if (auto* controlMap = RE::ControlMap::GetSingleton()) {
                controlMap->ToggleControls(kJunkItEventGroup, true, false);
            }
            ClearTaggedMappings();
            g_inputBlocked = false;
        }

        void BlockConflictingInputs() {
            if (g_inputBlocked) {
                return;
            }

            auto* controlMap = RE::ControlMap::GetSingleton();
            if (!controlMap) {
                return;
            }

            TagMatchingMappings();
            controlMap->ToggleControls(kJunkItEventGroup, false, false);
            g_inputBlocked = true;
        }

        void SyncInputBlock(bool menuOpen) {
            if (g_inputBlocked) {
                UnblockConflictingInputs();
            }
            if (menuOpen && QuickLootIntegration::MarkAllowed()) {
                BlockConflictingInputs();
            }
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
            SyncInputBlock(true);
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
            UnblockConflictingInputs();
        }

        void OnInvalidateLootMenu(QuickLoot::API::InvalidateLootMenuEvent*) {
            g_selectedEntry = nullptr;
            g_selectedOwner = 0;
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
        QuickLoot::API::QuickLootAPI::RegisterInvalidateLootMenuHandler(&OnInvalidateLootMenu);
        g_ready = true;
        SKSE::log::info("QuickLootIE V20 API integration installed");
    }

    void QuickLootIntegration::RefreshMenu() {
        if (!g_ready) {
            return;
        }
        QuickLoot::API::QuickLootAPI::RefreshLootMenu();
        SyncInputBlock(IsMenuOpen());
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
