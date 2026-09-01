#include "event.h"
#include "junk.h"
#include "SkyPromptIntegration.h"
#include "UI.h"

namespace JunkIt {

    InputEventHandler::ActiveMenuType InputEventHandler::GetActiveMenu() {
        const auto ui = RE::UI::GetSingleton();
        if (!ui) return ActiveMenuType::kNone;

        if (ui->IsMenuOpen("ContainerMenu")) return ActiveMenuType::kContainer;
        if (ui->IsMenuOpen("BarterMenu")) return ActiveMenuType::kBarter;
        if (ui->IsMenuOpen("InventoryMenu")) return ActiveMenuType::kInventory;

        return ActiveMenuType::kNone;
    }

    void InputEventHandler::ExecuteAction(JUNKIT_EVENT_TYPE type) {
        AtomicGuard guard(busy);
        if (!guard) {
            return;
        }

        if (type == JUNKIT_EVENT_TYPE::kMark && JunkHandler::operationInProgress.load()) {
            return;
        }

        if (Settings::GetAggressiveRefresh()) {
            UIUtil::ItemList::Refresh();
            JunkHandler::StartAggressiveRefresh();
        }

        switch (type) {
            case JUNKIT_EVENT_TYPE::kMark:
                JunkHandler::ToggleIsJunk();
                break;
            case JUNKIT_EVENT_TYPE::kTransfer:
                JunkHandler::TransferJunk();
                break;
            case JUNKIT_EVENT_TYPE::kSell:
                JunkHandler::SellJunk();
                break;
            case JUNKIT_EVENT_TYPE::kTrash:
                JunkHandler::TrashSelectedItem();
                break;
            case JUNKIT_EVENT_TYPE::kTrashBulk:
                JunkHandler::TrashAllJunk();
                break;
            default:
                break;
        }

        const bool trashConfirmPending =
            type == JUNKIT_EVENT_TYPE::kTrash || type == JUNKIT_EVENT_TYPE::kTrashBulk;
        if (Settings::GetAggressiveRefresh() && !trashConfirmPending) {
            UIUtil::ItemList::Refresh();
            JunkHandler::StartAggressiveRefresh();
        }

        if (type == JUNKIT_EVENT_TYPE::kMark) {
            SkyPromptIntegration::GetSingleton().ScheduleLabelSync();
        }
    }

    void InputEventHandler::HandleMarkKey(RE::ButtonEvent* buttonEvent, ActiveMenuType, bool skyPromptShowing) {
        const auto holdSeconds = Settings::IsTrashAvailable() ? Settings::GetTrashHoldSeconds() : 0;
        if (holdSeconds <= 0) {
            if (!skyPromptShowing && buttonEvent->IsDown()) {
                SKSE::log::info("Mark/Unmark Junk key pressed (KeyCode: 0x{:X})", buttonEvent->GetIDCode());
                ExecuteAction(JUNKIT_EVENT_TYPE::kMark);
            }
            return;
        }

        if (buttonEvent->IsDown()) {
            markHoldArmed = true;
            markTrashFired = false;
            SkyPromptIntegration::GetSingleton().ResetMarkHoldVisual();
            return;
        }

        if (markHoldArmed && !markTrashFired && buttonEvent->IsPressed()) {
            auto& skyPrompt = SkyPromptIntegration::GetSingleton();
            skyPrompt.UpdateMarkHoldVisual(buttonEvent->HeldDuration());
            if (buttonEvent->HeldDuration() >= static_cast<float>(holdSeconds)) {
                markTrashFired = true;
                SKSE::log::info("Mark key held {:.2f}s; trashing selected item", buttonEvent->HeldDuration());
                skyPrompt.ResetMarkHoldVisual();
                ExecuteAction(JUNKIT_EVENT_TYPE::kTrash);
            }
            return;
        }

        if (buttonEvent->IsUp()) {
            SkyPromptIntegration::GetSingleton().ResetMarkHoldVisual();
            if (markHoldArmed && !markTrashFired) {
                if (buttonEvent->HeldDuration() < SkyPromptIntegration::kMarkHoldTrashDelay) {
                    SKSE::log::info("Mark/Unmark Junk key released before trash hold");
                    ExecuteAction(JUNKIT_EVENT_TYPE::kMark);
                } else {
                    SKSE::log::info(
                        "Mark key released after trash hold started ({:.2f}s); mark skipped",
                        buttonEvent->HeldDuration());
                }
            }
            markHoldArmed = false;
            markTrashFired = false;
        }
    }

    void InputEventHandler::HandleTrashKey(RE::ButtonEvent* buttonEvent) {
        auto& skyPrompt = SkyPromptIntegration::GetSingleton();
        if (!SkyPromptIntegration::IsPlayerInventoryView()) {
            if (buttonEvent->IsUp()) {
                skyPrompt.ResetTrashHoldVisual();
                trashHoldArmed = false;
                trashBulkFired = false;
            }
            return;
        }
        const auto holdSeconds = Settings::IsTrashAvailable() ? Settings::GetTrashHoldSeconds() : 0;
        if (holdSeconds <= 0) {
            if (buttonEvent->IsDown()) {
                SKSE::log::info("Trash Junk key pressed (KeyCode: 0x{:X})", buttonEvent->GetIDCode());
                ExecuteAction(JUNKIT_EVENT_TYPE::kTrashBulk);
            }
            return;
        }

        if (buttonEvent->IsDown()) {
            trashHoldArmed = true;
            trashBulkFired = false;
            skyPrompt.ResetTrashHoldVisual();
            return;
        }

        if (trashHoldArmed && !trashBulkFired && buttonEvent->IsPressed()) {
            skyPrompt.UpdateTrashHoldVisual(buttonEvent->HeldDuration());
            if (buttonEvent->HeldDuration() >= static_cast<float>(holdSeconds)) {
                trashBulkFired = true;
                SKSE::log::info("Trash Junk key held {:.2f}s; trashing all junk", buttonEvent->HeldDuration());
                skyPrompt.ResetTrashHoldVisual();
                ExecuteAction(JUNKIT_EVENT_TYPE::kTrashBulk);
            }
            return;
        }

        if (buttonEvent->IsUp()) {
            skyPrompt.ResetTrashHoldVisual();
            trashHoldArmed = false;
            trashBulkFired = false;
        }
    }

    void InputEventHandler::HandleGamepadJunkKey(RE::ButtonEvent* buttonEvent, ActiveMenuType activeMenu) {
        auto& skyPrompt = SkyPromptIntegration::GetSingleton();
        const float trashHold = Settings::IsTrashAvailable()
            ? static_cast<float>(Settings::GetGamepadTrashHoldSeconds())
            : 0.f;
        const bool canTrash = SkyPromptIntegration::IsPlayerInventoryView() && trashHold > 0.f &&
            JunkHandler::HasTrashablePlayerJunk();

        if (buttonEvent->IsDown()) {
            gamepadTrashFired = false;
            skyPrompt.ResetGamepadHoldVisual();
            return;
        }

        if (!gamepadTrashFired && buttonEvent->IsPressed()) {
            skyPrompt.UpdateGamepadHoldVisual(buttonEvent->HeldDuration());
            if (canTrash && buttonEvent->HeldDuration() >= trashHold) {
                gamepadTrashFired = true;
                SKSE::log::info(
                    "Gamepad junk button held {:.2f}s; trashing all junk",
                    buttonEvent->HeldDuration());
                skyPrompt.ResetGamepadHoldVisual();
                ExecuteAction(JUNKIT_EVENT_TYPE::kTrashBulk);
            }
            return;
        }

        if (buttonEvent->IsUp()) {
            skyPrompt.ResetGamepadHoldVisual();
            if (!gamepadTrashFired) {
                HandleGamepadKeyUp(buttonEvent->HeldDuration(), activeMenu);
            }
            gamepadTrashFired = false;
        }
    }

    void InputEventHandler::HandleKeyDown(uint32_t keyCode, ActiveMenuType activeMenu) {
        uint32_t markKey = static_cast<uint32_t>(Settings::GetMarkJunkKey());
        uint32_t transferKey = static_cast<uint32_t>(Settings::GetTransferJunkKey());

        if (keyCode == markKey) {
            SKSE::log::info("Mark/Unmark Junk key pressed (KeyCode: 0x{:X})", keyCode);
            ExecuteAction(JUNKIT_EVENT_TYPE::kMark);
        } else if (keyCode == transferKey) {
            if (activeMenu == ActiveMenuType::kContainer) {
                SKSE::log::info("Transfer Junk key pressed (KeyCode: 0x{:X}) in Container menu", keyCode);
                ExecuteAction(JUNKIT_EVENT_TYPE::kTransfer);
            } else if (activeMenu == ActiveMenuType::kBarter) {
                SKSE::log::info("Sell Junk key pressed (KeyCode: 0x{:X}) in Barter menu", keyCode);
                ExecuteAction(JUNKIT_EVENT_TYPE::kSell);
            }
        }
    }

    void InputEventHandler::HandleGamepadKeyUp(float holdTime, ActiveMenuType activeMenu) {
        float holdThreshold = Settings::GetGamepadTransferHoldTime() - 1.0f;

        if (holdTime < holdThreshold || activeMenu == ActiveMenuType::kInventory) {
            SKSE::log::info("Gamepad Mark/Unmark Junk button released (Hold: {:.2f}s, Threshold: {:.2f}s)", holdTime, holdThreshold);
            ExecuteAction(JUNKIT_EVENT_TYPE::kMark);
        } else if (activeMenu == ActiveMenuType::kContainer) {
            SKSE::log::info("Gamepad Transfer Junk button held (Hold: {:.2f}s) in Container menu", holdTime);
            ExecuteAction(JUNKIT_EVENT_TYPE::kTransfer);
        } else if (activeMenu == ActiveMenuType::kBarter) {
            SKSE::log::info("Gamepad Sell Junk button held (Hold: {:.2f}s) in Barter menu", holdTime);
            ExecuteAction(JUNKIT_EVENT_TYPE::kSell);
        }
    }

    RE::BSEventNotifyControl InputEventHandler::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) {
        using Result = RE::BSEventNotifyControl;

        if (!a_event) return Result::kContinue;

        const auto ui = RE::UI::GetSingleton();
        if (!ui) return Result::kContinue;

        auto controlMap = RE::ControlMap::GetSingleton();
        const bool textEntry = controlMap && controlMap->GetRuntimeData().textEntryCount > 0;

        for (auto event = *a_event; event; event = event->next) {
            auto buttonEvent = event->AsButtonEvent();
            if (buttonEvent && !buttonEvent->IsUp()) {
                SkyPromptIntegration::GetSingleton().NoteInputDevice(buttonEvent->GetDevice());
            }
            if (!buttonEvent || !buttonEvent->IsDown()) {
                continue;
            }

            uint32_t keyCode = buttonEvent->GetIDCode();
            if (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
                keyCode = KeyUtil::Interpreter::GamepadMaskToKeycode(keyCode);
            }

            if (UI::ConsumeKeyCapture(keyCode)) {
                return Result::kContinue;
            }
        }

        ActiveMenuType activeMenu = GetActiveMenu();
        if (activeMenu == ActiveMenuType::kNone) return Result::kContinue;

        if (textEntry) return Result::kContinue;

        uint32_t markKey = static_cast<uint32_t>(Settings::GetMarkJunkKey());
        uint32_t transferKey = static_cast<uint32_t>(Settings::GetTransferJunkKey());
        uint32_t gamepadKey = static_cast<uint32_t>(Settings::GetGamepadJunkKey());
        uint32_t trashKey = static_cast<uint32_t>(Settings::GetTrashJunkKey());
        const bool skyPromptShowing = SkyPromptIntegration::GetSingleton().IsShowing();
        const bool trashAvailable = Settings::IsTrashAvailable();
        const bool nativeOwnsMark =
            SkyPromptIntegration::GetSingleton().MarkHoldTrashEnabled() || !skyPromptShowing;
        const bool nativeOwnsTrash =
            trashAvailable && trashKey != 0 &&
            (SkyPromptIntegration::GetSingleton().KeyboardTrashHoldEnabled() || !skyPromptShowing);

        bool sawHoldRepeat = false;
        bool sawOtherInput = false;
        for (auto event = *a_event; event; event = event->next) {
            auto buttonEvent = event->AsButtonEvent();
            if (!buttonEvent) continue;

            uint32_t keyCode = buttonEvent->GetIDCode();

            if (buttonEvent->GetDevice() == RE::INPUT_DEVICE::kGamepad) {
                keyCode = KeyUtil::Interpreter::GamepadMaskToKeycode(keyCode);
            }

            const bool heldRepeat = buttonEvent->IsPressed() && !buttonEvent->IsDown() && !buttonEvent->IsUp();
            const bool holdKey = keyCode == markKey || keyCode == gamepadKey || keyCode == trashKey;
            if (heldRepeat && holdKey) {
                sawHoldRepeat = true;
            } else {
                sawOtherInput = true;
            }

            if (keyCode == gamepadKey) {
                HandleGamepadJunkKey(buttonEvent, GetActiveMenu());
            } else if (nativeOwnsMark && keyCode == markKey) {
                HandleMarkKey(buttonEvent, activeMenu, skyPromptShowing);
            } else if (!skyPromptShowing && keyCode == transferKey) {
                if (buttonEvent->IsDown()) {
                    HandleKeyDown(keyCode, activeMenu);
                }
            } else if (nativeOwnsTrash && keyCode == trashKey) {
                HandleTrashKey(buttonEvent);
            }
        }

        if (sawOtherInput || !sawHoldRepeat) {
            SkyPromptIntegration::GetSingleton().SyncPromptLabels();
            SkyPromptIntegration::GetSingleton().ScheduleLabelSync();
        }

        return Result::kContinue;
    }
}
