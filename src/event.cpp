#include "event.h"
#include "junk.h"

namespace JunkIt {

    InputEventHandler::ActiveMenuType InputEventHandler::GetActiveMenu() {
        const auto ui = RE::UI::GetSingleton();
        if (!ui) return ActiveMenuType::kNone;

        if (ui->IsMenuOpen("ContainerMenu")) return ActiveMenuType::kContainer;
        if (ui->IsMenuOpen("BarterMenu")) return ActiveMenuType::kBarter;
        if (ui->IsMenuOpen("InventoryMenu")) return ActiveMenuType::kInventory;

        return ActiveMenuType::kNone;
    }

    void InputEventHandler::HandleKeyDown(uint32_t keyCode, ActiveMenuType activeMenu) {
        uint32_t markKey = static_cast<uint32_t>(Settings::GetMarkJunkKey());
        uint32_t transferKey = static_cast<uint32_t>(Settings::GetTransferJunkKey());

        if (keyCode == markKey) {
            JunkHandler::ToggleIsJunk();
        } else if (keyCode == transferKey) {
            if (activeMenu == ActiveMenuType::kContainer) {
                JunkHandler::TransferJunk();
            } else if (activeMenu == ActiveMenuType::kBarter) {
                JunkHandler::SellJunk();
            }
        }
    }

    void InputEventHandler::HandleGamepadKeyUp(float holdTime, ActiveMenuType activeMenu) {
        float holdThreshold = Settings::GetGamepadTransferHoldTime() - 1.0f;

        if (holdTime < holdThreshold) {
            JunkHandler::ToggleIsJunk();
        } else {
            if (activeMenu == ActiveMenuType::kContainer) {
                JunkHandler::TransferJunk();
            } else if (activeMenu == ActiveMenuType::kBarter) {
                JunkHandler::SellJunk();
            }
        }
    }

    RE::BSEventNotifyControl InputEventHandler::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) {
        using Result = RE::BSEventNotifyControl;

        if (!a_event) return Result::kContinue;

        const auto ui = RE::UI::GetSingleton();
        if (!ui) return Result::kContinue;

        ActiveMenuType activeMenu = GetActiveMenu();
        if (activeMenu == ActiveMenuType::kNone) return Result::kContinue;

        auto controlMap = RE::ControlMap::GetSingleton();
        if (controlMap && controlMap->GetRuntimeData().textEntryCount > 0) return Result::kContinue;

        uint32_t markKey = static_cast<uint32_t>(Settings::GetMarkJunkKey());
        uint32_t transferKey = static_cast<uint32_t>(Settings::GetTransferJunkKey());
        uint32_t gamepadKey = static_cast<uint32_t>(Settings::GetGamepadJunkKey());

        for (auto event = *a_event; event; event = event->next) {
            auto buttonEvent = event->AsButtonEvent();
            if (!buttonEvent) continue;

            uint32_t keyCode = buttonEvent->GetIDCode();

            if (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
                keyCode = KeyUtil::Interpreter::GamepadMaskToKeycode(keyCode);
            }

            if (keyCode == gamepadKey) {
                if (buttonEvent->IsUp()) {
                    ActiveMenuType currentMenu = GetActiveMenu();
                    if (currentMenu == ActiveMenuType::kNone) continue;

                    AtomicGuard guard(busy);
                    if (!guard) continue;

                    if (Settings::GetAggressiveRefresh()) {
                        UIUtil::ItemList::Refresh();
                    }

                    HandleGamepadKeyUp(buttonEvent->HeldDuration(), currentMenu);

                    if (Settings::GetAggressiveRefresh()) {
                        UIUtil::ItemList::Refresh();
                    }
                }
            } else if (keyCode == markKey || keyCode == transferKey) {
                if (buttonEvent->IsDown()) {
                    AtomicGuard guard(busy);
                    if (!guard) continue;

                    if (Settings::GetAggressiveRefresh()) {
                        UIUtil::ItemList::Refresh();
                    }

                    HandleKeyDown(keyCode, activeMenu);

                    if (Settings::GetAggressiveRefresh()) {
                        UIUtil::ItemList::Refresh();
                    }
                }
            }
        }

        return Result::kContinue;
    }
}
