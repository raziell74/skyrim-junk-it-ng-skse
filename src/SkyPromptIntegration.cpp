#include "SkyPromptIntegration.h"

#include "JunkData.h"
#include "Translation.h"
#include "event.h"
#include "junk.h"
#include "settings.h"
#include "util.h"

#include <SKSE/API.h>
#include <fmt/format.h>
#include <algorithm>
#include <vector>

namespace JunkIt {
    namespace {
        constexpr float kMarkHoldProgressMin = 0.01f;
        constexpr float kMarkHoldProgressMax = 0.99f;

        std::int32_t ClampNonNegative(std::int32_t value) {
            return value < 0 ? 0 : value;
        }

        RE::FormID Inventory3DAttachRefID() {
            auto* mgr = RE::Inventory3DManager::GetSingleton();
            if (!mgr || !mgr->tempRef) {
                return 0;
            }

            const auto id = mgr->tempRef->GetFormID();
            if (id == 0) {
                return 0;
            }

            const auto& models = mgr->GetRuntimeData().loadedModels;
            if (models.empty() || !models.back().spModel) {
                return 0;
            }

            return id;
        }

        bool Inventory3DModelPending() {
            auto* mgr = RE::Inventory3DManager::GetSingleton();
            if (!mgr || !mgr->tempRef || mgr->tempRef->GetFormID() == 0) {
                return false;
            }
            return Inventory3DAttachRefID() == 0;
        }
    }
    SkyPromptIntegration& SkyPromptIntegration::GetSingleton() {
        static SkyPromptIntegration singleton;
        return singleton;
    }

    void SkyPromptIntegration::Install() {
        if (GetModuleHandleW(L"SkyPrompt") == nullptr) {
            SKSE::log::debug("SkyPrompt not installed; on-screen prompts disabled");
            return;
        }

        clientID_ = SkyPromptAPI::RequestClientID();
        if (clientID_ == 0) {
            SKSE::log::warn("SkyPrompt RequestClientID failed; on-screen prompts disabled");
            return;
        }

        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            SKSE::log::error("UI singleton unavailable; SkyPrompt menu sink not registered");
            return;
        }

        ui->AddEventSink<RE::MenuOpenCloseEvent>(this);
        if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
            holder->AddEventSink<RE::TESContainerChangedEvent>(this);
            holder->AddEventSink<RE::TESEquipEvent>(this);
        }
        SKSE::log::info("SkyPrompt client {} registered", clientID_);
        RefreshPrompts();
    }

    bool SkyPromptIntegration::IsShowing() const {
        return showing_;
    }

    bool SkyPromptIntegration::IsEnabled() const {
        return clientID_ != 0 && Settings::GetSkyPromptEnabled();
    }

    bool SkyPromptIntegration::IsGamepadInputActive() const {
        if (gamepadInput_) {
            return *gamepadInput_;
        }
        auto* mgr = RE::BSInputDeviceManager::GetSingleton();
        return mgr && mgr->IsGamepadEnabled();
    }

    void SkyPromptIntegration::NoteInputDevice(RE::INPUT_DEVICE device) {
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

        if (gamepadInput_ && *gamepadInput_ == gamepad) {
            return;
        }

        gamepadInput_ = gamepad;
        if (IsEnabled() && GetActiveMenu() != MenuKind::kNone) {
            RefreshPrompts();
        }
    }

    RE::FormID SkyPromptIntegration::PromptAttachRefID() {
        if (Settings::GetSkyPromptButtonPlacement() == Settings::SkyPromptButtonPlacement::kLowerRight) {
            return 0;
        }
        return Inventory3DAttachRefID();
    }

    void SkyPromptIntegration::RefreshPrompts() {
        if (clientID_ == 0) {
            return;
        }

        Remove();
        if (!Settings::GetSkyPromptEnabled()) {
            return;
        }

        const auto menu = GetActiveMenu();
        if (menu == MenuKind::kNone) {
            return;
        }

        TryEnsurePreview();
        RebuildPrompts(menu);
        Send();
    }

    void SkyPromptIntegration::RecapturePreviews() {
        sellRecapturePending_ = false;
        sellRecaptureRebuild_ = false;
        if (!IsEnabled()) {
            RefreshPrompts();
            return;
        }

        const auto menu = GetActiveMenu();
        if (menu == MenuKind::kNone) {
            RefreshPrompts();
            return;
        }

        if (menu == MenuKind::kContainer) {
            previewMenu_ = MenuKind::kContainer;
            CaptureContainerPreview();
            if (!containerPreview_.valid) {
                ScheduleLabelSync();
                return;
            }
        } else if (menu == MenuKind::kBarter) {
            previewMenu_ = MenuKind::kBarter;
            CaptureSellPreview();
            if (!sellPreview_.valid) {
                ScheduleLabelSync();
                return;
            }
        }
        SyncPromptLabels();
    }

    void SkyPromptIntegration::ScheduleFullRefresh(int framesRemaining) {
        if (!IsEnabled()) {
            sellRecapturePending_ = false;
            sellRecaptureRebuild_ = false;
            return;
        }

        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            RecapturePreviews();
            return;
        }

        tasks->AddUITask([framesRemaining]() {
            auto& self = SkyPromptIntegration::GetSingleton();
            if (framesRemaining > 0) {
                self.ScheduleFullRefresh(framesRemaining - 1);
                return;
            }
            if (GetActiveMenu() != MenuKind::kNone) {
                self.RecapturePreviews();
            } else {
                self.sellRecapturePending_ = false;
                self.sellRecaptureRebuild_ = false;
            }
        });
    }

    void SkyPromptIntegration::ProcessEvent(SkyPromptAPI::PromptEvent event) const {
        auto& self = GetSingleton();

        if (event.type == SkyPromptAPI::PromptEventType::kAccepted) {
            switch (static_cast<PromptActionID>(event.prompt.actionID)) {
                case PromptActionID::kMark:
                    if (MarkHoldTrashEnabled()) {
                        return;
                    }
                    InputEventHandler::GetSingleton()->ExecuteAction(JUNKIT_EVENT_TYPE::kMark);
                    self.ScheduleLabelSync();
                    return;
                case PromptActionID::kTransfer:
                    InputEventHandler::GetSingleton()->ExecuteAction(JUNKIT_EVENT_TYPE::kTransfer);
                    break;
                case PromptActionID::kSell:
                    InputEventHandler::GetSingleton()->ExecuteAction(JUNKIT_EVENT_TYPE::kSell);
                    break;
                case PromptActionID::kTrash:
                    if (self.KeyboardTrashHoldEnabled()) {
                        return;
                    }
                    InputEventHandler::GetSingleton()->ExecuteAction(JUNKIT_EVENT_TYPE::kTrashBulk);
                    break;
                case PromptActionID::kGamepad:
                    return;
                default:
                    break;
            }
            self.ScheduleLabelSync();
            if (!self.prompts_.empty()) {
                self.Send();
            }
            return;
        }

        if (event.type == SkyPromptAPI::PromptEventType::kDeclined) {
            const auto action = static_cast<PromptActionID>(event.prompt.actionID);
            if ((action == PromptActionID::kMark && MarkHoldTrashEnabled()) ||
                (action == PromptActionID::kTrash && self.KeyboardTrashHoldEnabled()) ||
                action == PromptActionID::kGamepad) {
                return;
            }
        }

        if (event.type == SkyPromptAPI::PromptEventType::kTimingOut ||
            event.type == SkyPromptAPI::PromptEventType::kTimeout) {
            const auto action = static_cast<PromptActionID>(event.prompt.actionID);
            if ((action == PromptActionID::kMark && MarkHoldTrashEnabled()) ||
                (action == PromptActionID::kTrash && self.KeyboardTrashHoldEnabled()) ||
                action == PromptActionID::kGamepad) {
                return;
            }
            if (!self.prompts_.empty()) {
                self.Send();
            } else {
                self.RefreshPrompts();
            }
        }
    }

    std::span<const SkyPromptAPI::Prompt> SkyPromptIntegration::GetPrompts() const {
        return prompts_;
    }

    RE::BSEventNotifyControl SkyPromptIntegration::ProcessEvent(
        const RE::MenuOpenCloseEvent* a_event,
        RE::BSTEventSource<RE::MenuOpenCloseEvent>*) {
        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        const auto& name = a_event->menuName;
        if (name == "MessageBoxMenu") {
            if (GetActiveMenu() != MenuKind::kNone) {
                if (a_event->opening) {
                    if (!prompts_.empty()) {
                        Send();
                    }
                } else {
                    ScheduleLabelSync();
                    if (!prompts_.empty()) {
                        Send();
                    }
                }
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        if (name == "QuantityMenu") {
            if (!a_event->opening && GetActiveMenu() == MenuKind::kBarter) {
                RequestSellRecapture(true);
            } else if (GetActiveMenu() != MenuKind::kNone) {
                ScheduleLabelSync();
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        if (name != "InventoryMenu" && name != "ContainerMenu" && name != "BarterMenu") {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (a_event->opening) {
            if (name == "ContainerMenu") {
                previewMenu_ = MenuKind::kContainer;
                sellPreview_ = {};
                CaptureContainerPreview();
                if (!containerPreview_.valid) {
                    ScheduleLabelSync();
                }
            } else if (name == "BarterMenu") {
                previewMenu_ = MenuKind::kBarter;
                containerPreview_ = {};
                CaptureSellPreview();
                if (!sellPreview_.valid) {
                    ScheduleLabelSync();
                }
            } else {
                previewMenu_ = MenuKind::kInventory;
                InvalidatePreviews();
            }
        } else if (name == "ContainerMenu" && previewMenu_ == MenuKind::kContainer) {
            previewMenu_ = MenuKind::kNone;
            InvalidatePreviews();
        } else if (name == "BarterMenu" && previewMenu_ == MenuKind::kBarter) {
            previewMenu_ = MenuKind::kNone;
            InvalidatePreviews();
        } else if (name == "InventoryMenu" && previewMenu_ == MenuKind::kInventory) {
            previewMenu_ = MenuKind::kNone;
        }

        const bool deferAttachRefresh = a_event->opening &&
            Settings::GetSkyPromptButtonPlacement() == Settings::SkyPromptButtonPlacement::kAttachToItemModel &&
            Inventory3DModelPending();
        if (deferAttachRefresh) {
            ScheduleLabelSync();
        } else {
            RefreshPrompts();
        }
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl SkyPromptIntegration::ProcessEvent(
        const RE::TESContainerChangedEvent* a_event,
        RE::BSTEventSource<RE::TESContainerChangedEvent>*) {
        if (!a_event || !IsEnabled() || GetActiveMenu() == MenuKind::kNone) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (JunkHandler::operationInProgress.load()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (a_event->itemCount <= 0) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (!EventIsPlayerAndOpenTarget(a_event->oldContainer, a_event->newContainer)) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (previewMenu_ == MenuKind::kBarter) {
            bool rebuildStacks = false;
            if (auto* gold = Settings::GetGold001(); !gold || a_event->baseObj != gold->GetFormID()) {
                if (auto* form = RE::TESForm::LookupByID(a_event->baseObj)) {
                    rebuildStacks = JunkDataManager::GetSingleton().IsAnyJunkForForm(form);
                }
            }
            RequestSellRecapture(rebuildStacks);
            return RE::BSEventNotifyControl::kContinue;
        }

        if (auto* gold = Settings::GetGold001(); gold && a_event->baseObj == gold->GetFormID()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (previewMenu_ == MenuKind::kContainer && containerPreview_.valid) {
            ApplyContainerMove(a_event);
        }

        ScheduleLabelSync();
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl SkyPromptIntegration::ProcessEvent(
        const RE::TESEquipEvent* a_event,
        RE::BSTEventSource<RE::TESEquipEvent>*) {
        if (!a_event || !IsEnabled() || GetActiveMenu() == MenuKind::kNone) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (JunkHandler::operationInProgress.load() || !Settings::ProtectEquipped()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        ApplyEquipChange(a_event);
        ScheduleLabelSync();
        return RE::BSEventNotifyControl::kContinue;
    }

    SkyPromptIntegration::MenuKind SkyPromptIntegration::GetActiveMenu() {
        const auto ui = RE::UI::GetSingleton();
        if (!ui) {
            return MenuKind::kNone;
        }

        if (ui->IsMenuOpen("ContainerMenu")) {
            return MenuKind::kContainer;
        }
        if (ui->IsMenuOpen("BarterMenu")) {
            return MenuKind::kBarter;
        }
        if (ui->IsMenuOpen("InventoryMenu")) {
            return MenuKind::kInventory;
        }

        return MenuKind::kNone;
    }

    void SkyPromptIntegration::ScheduleLabelSync() {
        if (!IsEnabled()) {
            return;
        }

        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            SyncPromptLabels();
            return;
        }

        tasks->AddUITask([]() {
            auto& self = SkyPromptIntegration::GetSingleton();
            if (GetActiveMenu() != MenuKind::kNone) {
                self.TryEnsurePreview();
                self.SyncPromptLabels();
            }
        });
    }

    bool SkyPromptIntegration::EventIsPlayerAndOpenTarget(RE::FormID oldContainer, RE::FormID newContainer) const {
        if (previewPlayerId_ == 0) {
            return false;
        }

        const bool playerInvolved = oldContainer == previewPlayerId_ || newContainer == previewPlayerId_;
        if (!playerInvolved) {
            return false;
        }

        switch (previewMenu_) {
            case MenuKind::kContainer:
                return previewContainerId_ != 0 &&
                    (oldContainer == previewContainerId_ || newContainer == previewContainerId_);
            case MenuKind::kBarter:
                return (previewVendorId_ != 0 && (oldContainer == previewVendorId_ || newContainer == previewVendorId_)) ||
                    (previewMerchantId_ != 0 && (oldContainer == previewMerchantId_ || newContainer == previewMerchantId_));
            default:
                return false;
        }
    }

    std::optional<std::pair<RE::INPUT_DEVICE, SkyPromptAPI::ButtonID>> SkyPromptIntegration::ToSkyPromptButton(
        std::uint32_t keyCode) {
        constexpr std::uint32_t kKeyboardCount = 256;
        constexpr std::uint32_t kMouseButtonCount = 8;

        if (keyCode < kKeyboardCount) {
            return std::pair{ RE::INPUT_DEVICE::kKeyboard, keyCode };
        }

        const std::uint32_t mouseIndex = keyCode - kKeyboardCount;
        if (mouseIndex < kMouseButtonCount) {
            return std::pair{ RE::INPUT_DEVICE::kMouse, mouseIndex };
        }

        using Off = KeyUtil::GAMEPAD_OFFSETS;
        switch (static_cast<Off>(keyCode)) {
            case Off::kGamepadButtonOffset_DPAD_UP:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kUp) };
            case Off::kGamepadButtonOffset_DPAD_DOWN:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kDown) };
            case Off::kGamepadButtonOffset_DPAD_LEFT:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kLeft) };
            case Off::kGamepadButtonOffset_DPAD_RIGHT:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kRight) };
            case Off::kGamepadButtonOffset_START:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kStart) };
            case Off::kGamepadButtonOffset_BACK:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kBack) };
            case Off::kGamepadButtonOffset_LEFT_THUMB:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kLeftThumb) };
            case Off::kGamepadButtonOffset_RIGHT_THUMB:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kRightThumb) };
            case Off::kGamepadButtonOffset_LEFT_SHOULDER:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kLeftShoulder) };
            case Off::kGamepadButtonOffset_RIGHT_SHOULDER:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kRightShoulder) };
            case Off::kGamepadButtonOffset_A:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kA) };
            case Off::kGamepadButtonOffset_B:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kB) };
            case Off::kGamepadButtonOffset_X:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kX) };
            case Off::kGamepadButtonOffset_Y:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kY) };
            case Off::kGamepadButtonOffset_LT:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kLeftTrigger) };
            case Off::kGamepadButtonOffset_RT:
                return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<SkyPromptAPI::ButtonID>(RE::BSWin32GamepadDevice::Key::kRightTrigger) };
            default:
                break;
        }

        return std::nullopt;
    }

    bool SkyPromptIntegration::HasSelectedItem() {
        auto* itemList = UIUtil::ItemList::GetOpenList();
        if (!itemList) {
            return false;
        }

        auto* selectedItem = itemList->GetSelectedItem();
        return selectedItem && selectedItem->data.objDesc;
    }

    bool SkyPromptIntegration::SelectedItemIsJunk() {
        auto* itemList = UIUtil::ItemList::GetOpenList();
        if (!itemList) {
            return false;
        }

        auto* selectedItem = itemList->GetSelectedItem();
        if (!selectedItem || !selectedItem->data.objDesc) {
            return false;
        }

        return JunkDataManager::GetSingleton().IsJunk(selectedItem->data.objDesc);
    }

    const char* SkyPromptIntegration::MarkPromptText() {
        if (!HasSelectedItem()) {
            return nullptr;
        }

        return SelectedItemIsJunk() ? "$JunkIt_Prompt_Unmark" : "$JunkIt_Prompt_Mark";
    }

    SkyPromptIntegration::SelectedPromptIdentity SkyPromptIntegration::ReadSelectedPromptIdentity() const {
        SelectedPromptIdentity identity;
        auto* itemList = UIUtil::ItemList::GetOpenList();
        auto* selected = itemList ? itemList->GetSelectedItem() : nullptr;
        if (!selected || !selected->data.objDesc || !selected->data.objDesc->object) {
            return identity;
        }

        identity.hasSelection = true;
        identity.formId = selected->data.objDesc->object->GetFormID();
        identity.owner = selected->data.owner;
        identity.playerSide = SelectedRowIsPlayerSide();
        identity.isJunk = JunkDataManager::GetSingleton().IsJunk(selected->data.objDesc);
        return identity;
    }

    bool SkyPromptIntegration::SelectionIdentityChanged() const {
        return ReadSelectedPromptIdentity() != lastSyncedSelection_;
    }

    bool SkyPromptIntegration::MarkHoldTrashEnabled() const {
        if (!Settings::IsTrashAvailable() || Settings::GetTrashHoldSeconds() <= 0) {
            return false;
        }
        return GetActiveMenu() != MenuKind::kBarter || SelectedRowIsPlayerSide();
    }

    bool SkyPromptIntegration::KeyboardTrashHoldEnabled() const {
        return Settings::IsTrashAvailable() && Settings::GetTrashHoldSeconds() > 0;
    }

    bool SkyPromptIntegration::ContainerMenuIsPlayerSegment() {
        const auto ui = RE::UI::GetSingleton();
        auto menu = ui ? ui->GetMenu<RE::ContainerMenu>() : nullptr;
        if (!menu || !menu->uiMovie) {
            return false;
        }

        RE::GFxValue result;
        if (menu->uiMovie->GetVariable(&result, "_root.Menu_mc.inventoryLists.categoryList.activeSegment") &&
            result.IsNumber()) {
            return static_cast<int>(result.GetNumber()) != 0;
        }
        return false;
    }

    bool SkyPromptIntegration::IsPlayerInventoryView() {
        switch (GetActiveMenu()) {
            case MenuKind::kInventory:
                return true;
            case MenuKind::kContainer:
                return ContainerMenuIsPlayerSegment();
            default:
                return false;
        }
    }

    bool SkyPromptIntegration::ShouldShowTrashPrompt(MenuKind menu) {
        if (!Settings::IsTrashAvailable()) {
            return false;
        }
        const auto trashKey = static_cast<std::uint32_t>(Settings::GetTrashJunkKey());
        if (trashKey == 0 || !ToSkyPromptButton(trashKey)) {
            return false;
        }
        if (menu != MenuKind::kInventory &&
            (menu != MenuKind::kContainer || !ContainerMenuIsPlayerSegment())) {
            return false;
        }
        return JunkHandler::HasTrashablePlayerJunk();
    }

    SkyPromptAPI::PromptType SkyPromptIntegration::KeyboardTrashPromptType() {
        return Settings::GetTrashHoldSeconds() > 0
            ? SkyPromptAPI::PromptType::kHint
            : SkyPromptAPI::PromptType::kSinglePress;
    }

    SkyPromptAPI::Prompt* SkyPromptIntegration::FindMarkPrompt() {
        const auto markAction = static_cast<SkyPromptAPI::ActionID>(PromptActionID::kMark);
        for (auto& prompt : prompts_) {
            if (prompt.actionID == markAction) {
                return &prompt;
            }
        }
        return nullptr;
    }

    SkyPromptAPI::Prompt* SkyPromptIntegration::FindTrashPrompt() {
        const auto trashAction = static_cast<SkyPromptAPI::ActionID>(PromptActionID::kTrash);
        for (auto& prompt : prompts_) {
            if (prompt.actionID == trashAction) {
                return &prompt;
            }
        }
        return nullptr;
    }

    SkyPromptAPI::Prompt* SkyPromptIntegration::FindGamepadPrompt() {
        const auto gamepadAction = static_cast<SkyPromptAPI::ActionID>(PromptActionID::kGamepad);
        for (auto& prompt : prompts_) {
            if (prompt.actionID == gamepadAction) {
                return &prompt;
            }
        }
        return nullptr;
    }

    void SkyPromptIntegration::UpdateMarkHoldVisual(float heldDuration) {
        if (!IsEnabled() || !MarkHoldTrashEnabled()) {
            return;
        }

        auto* markPrompt = FindMarkPrompt();
        if (!markPrompt) {
            return;
        }

        const char* wantedText = MarkPromptText();
        float wantedProgress = 0.f;
        bool visualActive = false;

        if (heldDuration >= kMarkHoldTrashDelay) {
            wantedText = "$JunkIt_Prompt_TrashItem";
            visualActive = true;
            const float span = static_cast<float>(Settings::GetTrashHoldSeconds()) - kMarkHoldTrashDelay;
            const float t = span <= 0.f ? kMarkHoldProgressMax : (heldDuration - kMarkHoldTrashDelay) / span;
            wantedProgress = std::clamp(t, kMarkHoldProgressMin, kMarkHoldProgressMax);
        }

        if (!wantedText) {
            return;
        }

        markHoldVisualActive_ = visualActive;
        if (markPrompt->text == wantedText && markPrompt->progress == wantedProgress) {
            return;
        }

        markPrompt->text = wantedText;
        markPrompt->progress = wantedProgress;
        Send();
    }

    void SkyPromptIntegration::ResetMarkHoldVisual() {
        markHoldVisualActive_ = false;

        auto* markPrompt = FindMarkPrompt();
        if (!markPrompt) {
            return;
        }

        const char* wantedText = MarkPromptText();
        bool changed = false;
        if (wantedText && markPrompt->text != wantedText) {
            markPrompt->text = wantedText;
            changed = true;
        }
        if (markPrompt->progress != 0.f) {
            markPrompt->progress = 0.f;
            changed = true;
        }
        if (changed) {
            Send();
        }
    }

    void SkyPromptIntegration::UpdateTrashHoldVisual(float heldDuration) {
        if (!IsEnabled() || !KeyboardTrashHoldEnabled()) {
            return;
        }

        auto* trashPrompt = FindTrashPrompt();
        if (!trashPrompt) {
            return;
        }

        const float span = static_cast<float>(Settings::GetTrashHoldSeconds());
        const float t = span <= 0.f ? kMarkHoldProgressMax : heldDuration / span;
        const float wantedProgress = std::clamp(t, kMarkHoldProgressMin, kMarkHoldProgressMax);

        trashHoldVisualActive_ = true;
        if (trashPrompt->progress == wantedProgress) {
            return;
        }

        trashPrompt->progress = wantedProgress;
        Send();
    }

    void SkyPromptIntegration::ResetTrashHoldVisual() {
        trashHoldVisualActive_ = false;

        auto* trashPrompt = FindTrashPrompt();
        if (!trashPrompt) {
            return;
        }

        if (trashPrompt->progress != 0.f) {
            trashPrompt->progress = 0.f;
            Send();
        }
    }

    void SkyPromptIntegration::UpdateGamepadHoldVisual(float heldDuration) {
        if (!IsEnabled()) {
            return;
        }

        auto* gamepadPrompt = FindGamepadPrompt();
        if (!gamepadPrompt) {
            return;
        }

        const auto menu = GetActiveMenu();
        const float transferHoldRaw = Settings::GetGamepadTransferHoldTime() - 1.0f;
        const float transferHold = transferHoldRaw > 0.01f ? transferHoldRaw : 0.01f;
        const float trashHold = Settings::IsTrashAvailable()
            ? static_cast<float>(Settings::GetGamepadTrashHoldSeconds())
            : 0.f;
        const bool canTrash = IsPlayerInventoryView() && trashHold > 0.f && JunkHandler::HasTrashablePlayerJunk();

        const char* wantedText = MarkPromptText();
        float wantedProgress = 0.f;
        bool visualActive = false;

        if (menu == MenuKind::kInventory) {
            if (canTrash && heldDuration >= kMarkHoldTrashDelay) {
                wantedText = "$JunkIt_Prompt_TrashingJunk";
                visualActive = true;
                const float span = trashHold - kMarkHoldTrashDelay;
                const float t = span <= 0.f ? kMarkHoldProgressMax : (heldDuration - kMarkHoldTrashDelay) / span;
                wantedProgress = std::clamp(t, kMarkHoldProgressMin, kMarkHoldProgressMax);
            }
        } else if (menu == MenuKind::kContainer || menu == MenuKind::kBarter) {
            const char* opText = menu == MenuKind::kBarter
                ? "$JunkIt_Prompt_SellingJunk"
                : (ContainerMenuIsPlayerSegment()
                    ? "$JunkIt_Prompt_StoringJunk"
                    : "$JunkIt_Prompt_RetrievingJunk");
            if (heldDuration < transferHold) {
                wantedText = opText;
                visualActive = true;
                wantedProgress = std::clamp(heldDuration / transferHold, kMarkHoldProgressMin, kMarkHoldProgressMax);
            } else if (canTrash) {
                wantedText = "$JunkIt_Prompt_TrashingJunk";
                visualActive = true;
                const float span = trashHold - transferHold;
                const float t = span <= 0.f ? kMarkHoldProgressMax : (heldDuration - transferHold) / span;
                wantedProgress = std::clamp(t, kMarkHoldProgressMin, kMarkHoldProgressMax);
            } else {
                wantedText = opText;
                visualActive = true;
                wantedProgress = kMarkHoldProgressMax;
            }
        }

        if (!wantedText) {
            wantedText = "$JunkIt_Prompt_Mark";
        }

        gamepadHoldVisualActive_ = visualActive;
        if (gamepadPrompt->text == wantedText && gamepadPrompt->progress == wantedProgress) {
            return;
        }

        gamepadPrompt->text = wantedText;
        gamepadPrompt->progress = wantedProgress;
        Send();
    }

    void SkyPromptIntegration::ResetGamepadHoldVisual() {
        gamepadHoldVisualActive_ = false;

        auto* gamepadPrompt = FindGamepadPrompt();
        if (!gamepadPrompt) {
            return;
        }

        const char* wantedText = MarkPromptText();
        if (!wantedText) {
            wantedText = "$JunkIt_Prompt_Mark";
        }

        bool changed = false;
        if (gamepadPrompt->text != wantedText) {
            gamepadPrompt->text = wantedText;
            changed = true;
        }
        if (gamepadPrompt->progress != 0.f) {
            gamepadPrompt->progress = 0.f;
            changed = true;
        }
        if (changed) {
            Send();
        }
    }

    std::string SkyPromptIntegration::FormatTransferPrompt() {
        const bool storeView = ContainerMenuIsPlayerSegment();

        const char* key = storeView ? "$JunkIt_Prompt_Store" : "$JunkIt_Prompt_Retrieve";
        if (!Settings::GetSkyPromptShowCounts()) {
            return Translation::Get(key);
        }

        if (!containerPreview_.valid) {
            return {};
        }

        const auto count = storeView ? containerPreview_.storeCount : containerPreview_.retrieveCount;
        if (count <= 0) {
            return {};
        }

        return fmt::format("{} ({})", Translation::Get(key), count);
    }

    std::string SkyPromptIntegration::FormatSellPrompt() {
        if (!Settings::GetSkyPromptShowCounts()) {
            return Translation::Get("$JunkIt_Prompt_Sell");
        }

        if (!sellPreview_.valid || !sellPreview_.gold || *sellPreview_.gold <= 0) {
            return {};
        }

        return fmt::format("{} ({}g)", Translation::Get("$JunkIt_Prompt_Sell"), *sellPreview_.gold);
    }

    void SkyPromptIntegration::SyncPromptLabels() {
        if (!IsEnabled()) {
            return;
        }

        const auto menu = GetActiveMenu();
        if (menu == MenuKind::kNone) {
            return;
        }

        lastSyncedSelection_ = ReadSelectedPromptIdentity();
        TryEnsurePreview();
        ApplySelectedFavoriteChange();

        const auto markAction = static_cast<SkyPromptAPI::ActionID>(PromptActionID::kMark);
        const auto transferAction = static_cast<SkyPromptAPI::ActionID>(PromptActionID::kTransfer);
        const auto sellAction = static_cast<SkyPromptAPI::ActionID>(PromptActionID::kSell);
        const auto trashAction = static_cast<SkyPromptAPI::ActionID>(PromptActionID::kTrash);
        const auto gamepadAction = static_cast<SkyPromptAPI::ActionID>(PromptActionID::kGamepad);
        const auto wantedRefId = PromptAttachRefID();

        const bool gamepadInput = IsGamepadInputActive();
        const char* wantedMark = gamepadInput ? nullptr : MarkPromptText();
        const auto wantedMarkType = MarkHoldTrashEnabled()
            ? SkyPromptAPI::PromptType::kHint
            : SkyPromptAPI::PromptType::kSinglePress;
        const bool wantedTrash = !gamepadInput && ShouldShowTrashPrompt(menu);
        const auto wantedTrashType = KeyboardTrashPromptType();
        const auto gamepadKey = static_cast<std::uint32_t>(Settings::GetGamepadJunkKey());
        const bool wantedGamepad = gamepadInput && gamepadKey != 0 && ToSkyPromptButton(gamepadKey).has_value();
        std::string wantedTransfer;
        if (!gamepadInput && menu == MenuKind::kContainer) {
            wantedTransfer = FormatTransferPrompt();
        }
        std::string wantedSell;
        if (!gamepadInput && menu == MenuKind::kBarter) {
            wantedSell = FormatSellPrompt();
        }

        bool hasMark = false;
        bool hasTransfer = false;
        bool hasSell = false;
        bool hasTrash = false;
        bool hasGamepad = false;
        bool refidMismatch = false;
        bool markTypeMismatch = false;
        bool trashTypeMismatch = false;
        for (const auto& prompt : prompts_) {
            if (prompt.actionID == markAction) {
                hasMark = true;
                if (prompt.type != wantedMarkType) {
                    markTypeMismatch = true;
                }
            } else if (prompt.actionID == transferAction) {
                hasTransfer = true;
            } else if (prompt.actionID == sellAction) {
                hasSell = true;
            } else if (prompt.actionID == trashAction) {
                hasTrash = true;
                if (prompt.type != wantedTrashType) {
                    trashTypeMismatch = true;
                }
            } else if (prompt.actionID == gamepadAction) {
                hasGamepad = true;
            }
            if (prompt.refid != wantedRefId) {
                refidMismatch = true;
            }
        }

        if (hasMark != (wantedMark != nullptr) ||
            hasTransfer != !wantedTransfer.empty() ||
            hasSell != !wantedSell.empty() ||
            hasTrash != wantedTrash ||
            hasGamepad != wantedGamepad ||
            refidMismatch ||
            markTypeMismatch ||
            trashTypeMismatch) {
            RefreshPrompts();
            return;
        }

        if (prompts_.empty()) {
            return;
        }

        bool changed = false;
        for (auto& prompt : prompts_) {
            if (prompt.actionID == markAction) {
                if (markHoldVisualActive_) {
                    continue;
                }
                if (prompt.text != wantedMark) {
                    prompt.text = wantedMark;
                    changed = true;
                }
                if (prompt.progress != 0.f) {
                    prompt.progress = 0.f;
                    changed = true;
                }
            } else if (prompt.actionID == trashAction) {
                if (trashHoldVisualActive_) {
                    continue;
                }
                if (prompt.progress != 0.f) {
                    prompt.progress = 0.f;
                    changed = true;
                }
            } else if (prompt.actionID == gamepadAction) {
                if (gamepadHoldVisualActive_) {
                    continue;
                }
                const char* wantedGamepadText = MarkPromptText();
                if (!wantedGamepadText) {
                    wantedGamepadText = "$JunkIt_Prompt_Mark";
                }
                if (prompt.text != wantedGamepadText) {
                    prompt.text = wantedGamepadText;
                    changed = true;
                }
                if (prompt.progress != 0.f) {
                    prompt.progress = 0.f;
                    changed = true;
                }
            } else if (prompt.actionID == transferAction && prompt.text != wantedTransfer) {
                transferLabel_ = std::move(wantedTransfer);
                prompt.text = transferLabel_;
                changed = true;
            } else if (prompt.actionID == sellAction && prompt.text != wantedSell) {
                sellLabel_ = std::move(wantedSell);
                prompt.text = sellLabel_;
                changed = true;
            }
        }

        if (changed) {
            Send();
        }
    }

    void SkyPromptIntegration::RebuildPrompts(MenuKind menu) {
        markKeys_.clear();
        transferKeys_.clear();
        trashKeys_.clear();
        gamepadKeys_.clear();
        prompts_.clear();

        if (auto markButton = ToSkyPromptButton(static_cast<std::uint32_t>(Settings::GetMarkJunkKey()))) {
            markKeys_.push_back(*markButton);
        }
        if (auto transferButton = ToSkyPromptButton(static_cast<std::uint32_t>(Settings::GetTransferJunkKey()))) {
            transferKeys_.push_back(*transferButton);
        }
        const auto trashKey = static_cast<std::uint32_t>(Settings::GetTrashJunkKey());
        if (Settings::IsTrashAvailable() && trashKey != 0) {
            if (auto trashButton = ToSkyPromptButton(trashKey)) {
                trashKeys_.push_back(*trashButton);
            }
        }
        const auto gamepadKey = static_cast<std::uint32_t>(Settings::GetGamepadJunkKey());
        if (gamepadKey != 0) {
            if (auto gamepadButton = ToSkyPromptButton(gamepadKey)) {
                gamepadKeys_.push_back(*gamepadButton);
            }
        }

        const auto attachRefId = PromptAttachRefID();
        const bool gamepadInput = IsGamepadInputActive();

        if (!gamepadInput && !markKeys_.empty()) {
            if (const char* markText = MarkPromptText()) {
                const auto markType = MarkHoldTrashEnabled()
                    ? SkyPromptAPI::PromptType::kHint
                    : SkyPromptAPI::PromptType::kSinglePress;
                prompts_.emplace_back(
                    markText,
                    static_cast<SkyPromptAPI::EventID>(PromptEventID::kMark),
                    static_cast<SkyPromptAPI::ActionID>(PromptActionID::kMark),
                    markType,
                    attachRefId,
                    markKeys_);
            }
        }

        if (!gamepadInput && ShouldShowTrashPrompt(menu) && !trashKeys_.empty()) {
            prompts_.emplace_back(
                "$JunkIt_Prompt_Trash",
                static_cast<SkyPromptAPI::EventID>(PromptEventID::kTrash),
                static_cast<SkyPromptAPI::ActionID>(PromptActionID::kTrash),
                KeyboardTrashPromptType(),
                attachRefId,
                trashKeys_);
        }

        if (!gamepadInput && !transferKeys_.empty()) {
            if (menu == MenuKind::kContainer) {
                transferLabel_ = FormatTransferPrompt();
                if (!transferLabel_.empty()) {
                    prompts_.emplace_back(
                        transferLabel_,
                        static_cast<SkyPromptAPI::EventID>(PromptEventID::kTransfer),
                        static_cast<SkyPromptAPI::ActionID>(PromptActionID::kTransfer),
                        SkyPromptAPI::PromptType::kHold,
                        attachRefId,
                        transferKeys_);
                }
            } else if (menu == MenuKind::kBarter) {
                sellLabel_ = FormatSellPrompt();
                if (!sellLabel_.empty()) {
                    prompts_.emplace_back(
                        sellLabel_,
                        static_cast<SkyPromptAPI::EventID>(PromptEventID::kTransfer),
                        static_cast<SkyPromptAPI::ActionID>(PromptActionID::kSell),
                        SkyPromptAPI::PromptType::kHold,
                        attachRefId,
                        transferKeys_);
                }
            }
        }

        if (gamepadInput && !gamepadKeys_.empty()) {
            const char* gamepadText = MarkPromptText();
            if (!gamepadText) {
                gamepadText = "$JunkIt_Prompt_Mark";
            }
            prompts_.emplace_back(
                gamepadText,
                static_cast<SkyPromptAPI::EventID>(PromptEventID::kGamepad),
                static_cast<SkyPromptAPI::ActionID>(PromptActionID::kGamepad),
                SkyPromptAPI::PromptType::kHint,
                attachRefId,
                gamepadKeys_);
        }

        lastSyncedSelection_ = ReadSelectedPromptIdentity();
    }

    void SkyPromptIntegration::Send() {
        if (clientID_ == 0 || prompts_.empty()) {
            showing_ = false;
            return;
        }

        const bool sent = SkyPromptAPI::SendPrompt(this, clientID_);
        if (sent) {
            showing_ = true;
            return;
        }

        if (!showing_) {
            SKSE::log::warn("SkyPrompt SendPrompt failed");
        }
    }

    void SkyPromptIntegration::Remove() {
        if (clientID_ != 0 && showing_) {
            SkyPromptAPI::RemovePrompt(this, clientID_);
        }
        showing_ = false;
        prompts_.clear();
        markKeys_.clear();
        transferKeys_.clear();
        trashKeys_.clear();
        gamepadKeys_.clear();
        transferLabel_.clear();
        sellLabel_.clear();
        markHoldVisualActive_ = false;
        trashHoldVisualActive_ = false;
        gamepadHoldVisualActive_ = false;
        lastSyncedSelection_ = {};
    }

    void SkyPromptIntegration::InvalidatePreviews() {
        containerPreview_ = {};
        sellPreview_ = {};
        sellRecapturePending_ = false;
        sellRecaptureRebuild_ = false;
        previewPlayerId_ = 0;
        previewContainerId_ = 0;
        previewVendorId_ = 0;
        previewMerchantId_ = 0;
        selectedProtection_ = {};
    }

    void SkyPromptIntegration::CaptureContainerPreview() {
        containerPreview_ = {};
        previewPlayerId_ = 0;
        previewContainerId_ = 0;
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            previewPlayerId_ = player->GetFormID();
        }
        if (auto* container = JunkHandler::GetContainerMenuContainer()) {
            previewContainerId_ = container->GetFormID();
        }

        const auto captured = JunkHandler::CaptureContainerPreview();
        if (!captured) {
            return;
        }

        containerPreview_.storeCount = ClampNonNegative(captured->storeCount);
        containerPreview_.retrieveCount = ClampNonNegative(captured->retrieveCount);
        containerPreview_.valid = true;
    }

    void SkyPromptIntegration::InvalidateSellPreview() {
        sellPreview_ = {};
        sellRecapturePending_ = false;
        sellRecaptureRebuild_ = false;
        SyncPromptLabels();
    }

    void SkyPromptIntegration::CaptureSellPreview() {
        if (JunkHandler::operationInProgress.load()) {
            sellPreview_ = {};
            return;
        }

        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            previewPlayerId_ = player->GetFormID();
        } else {
            previewPlayerId_ = 0;
        }
        if (auto* vendor = JunkHandler::GetBarterMenuContainer()) {
            previewVendorId_ = vendor->GetFormID();
        } else {
            previewVendorId_ = 0;
        }
        if (auto* merchant = JunkHandler::GetBarterMenuMerchantContainer()) {
            previewMerchantId_ = merchant->GetFormID();
        } else {
            previewMerchantId_ = 0;
        }

        const auto captured = JunkHandler::CaptureSellPreview();
        if (!captured.pricesReady) {
            return;
        }

        sellPreview_.stacks = captured.stacks;
        sellPreview_.gold = captured.gold;
        sellPreview_.valid = true;
    }

    void SkyPromptIntegration::TryEnsurePreview() {
        if (previewMenu_ == MenuKind::kContainer && !containerPreview_.valid) {
            CaptureContainerPreview();
        } else if (previewMenu_ == MenuKind::kBarter && !sellPreview_.valid && !sellRecapturePending_) {
            CaptureSellPreview();
        }
    }

    void SkyPromptIntegration::RequestSellRecapture(bool rebuildStacks) {
        if (!IsEnabled() || GetActiveMenu() != MenuKind::kBarter || JunkHandler::operationInProgress.load()) {
            return;
        }

        if (!rebuildStacks && !sellPreview_.valid) {
            rebuildStacks = true;
        }

        if (sellRecapturePending_) {
            if (rebuildStacks) {
                sellRecaptureRebuild_ = true;
            }
            return;
        }

        sellRecapturePending_ = true;
        sellRecaptureRebuild_ = rebuildStacks;
        ScheduleSellPreviewUpdate(1);
    }

    void SkyPromptIntegration::ScheduleSellPreviewUpdate(int framesRemaining) {
        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            ApplyPendingSellRecapture();
            return;
        }

        tasks->AddUITask([framesRemaining]() {
            auto& self = SkyPromptIntegration::GetSingleton();
            if (framesRemaining > 0) {
                self.ScheduleSellPreviewUpdate(framesRemaining - 1);
                return;
            }
            self.ApplyPendingSellRecapture();
        });
    }

    void SkyPromptIntegration::ApplyPendingSellRecapture() {
        const bool rebuildStacks = sellRecaptureRebuild_;
        sellRecapturePending_ = false;
        sellRecaptureRebuild_ = false;

        if (!IsEnabled() || GetActiveMenu() != MenuKind::kBarter || JunkHandler::operationInProgress.load()) {
            return;
        }

        if (rebuildStacks) {
            CaptureSellPreview();
        } else {
            ApplyGoldOnlySellPreview();
        }
        SyncPromptLabels();
    }

    void SkyPromptIntegration::ApplyGoldOnlySellPreview() {
        if (!sellPreview_.valid) {
            CaptureSellPreview();
            return;
        }

        const auto gold = JunkHandler::ComputeSellPreviewGold(sellPreview_.stacks);
        if (!gold) {
            return;
        }

        if (*gold > 0) {
            sellPreview_.gold = gold;
        } else {
            sellPreview_.gold.reset();
        }
    }

    void SkyPromptIntegration::OnJunkToggled(RE::InventoryEntryData* entry, bool nowJunk, bool playerOwned) {
        if (!entry || !IsEnabled()) {
            return;
        }

        if (previewMenu_ == MenuKind::kBarter && playerOwned) {
            if (sellPreview_.valid && JunkHandler::TryPatchSellPreviewStacks(sellPreview_.stacks, entry)) {
                const auto gold = JunkHandler::ComputeSellPreviewGold(sellPreview_.stacks);
                if (gold) {
                    if (*gold > 0) {
                        sellPreview_.gold = gold;
                    } else {
                        sellPreview_.gold.reset();
                    }
                    return;
                }
            }
            RequestSellRecapture(true);
            return;
        }

        if (previewMenu_ != MenuKind::kContainer || !containerPreview_.valid) {
            return;
        }

        std::vector<std::string> identities;
        JunkHandler::CollectEntryIdentities(entry, identities);
        if (identities.empty()) {
            return;
        }

        const std::int32_t sign = nowJunk ? 1 : -1;
        auto* player = RE::PlayerCharacter::GetSingleton();
        RE::TESObjectREFR* container = nullptr;
        if (auto* form = RE::TESForm::LookupByID(previewContainerId_)) {
            container = form->As<RE::TESObjectREFR>();
        }
        const auto playerCount = JunkHandler::CountPreviewIdentities(player, identities, false, entry->object);
        const auto containerCount = JunkHandler::CountPreviewIdentities(container, identities, false, entry->object);
        containerPreview_.storeCount = ClampNonNegative(containerPreview_.storeCount + sign * playerCount);
        containerPreview_.retrieveCount = ClampNonNegative(containerPreview_.retrieveCount + sign * containerCount);
    }

    void SkyPromptIntegration::ApplyContainerMove(const RE::TESContainerChangedEvent* a_event) {
        if (!a_event || previewPlayerId_ == 0 || previewContainerId_ == 0) {
            return;
        }

        const bool toPlayer = a_event->newContainer == previewPlayerId_ && a_event->oldContainer == previewContainerId_;
        const bool toContainer = a_event->oldContainer == previewPlayerId_ && a_event->newContainer == previewContainerId_;
        if (!toPlayer && !toContainer) {
            return;
        }

        auto* destForm = RE::TESForm::LookupByID(a_event->newContainer);
        auto* dest = destForm ? destForm->As<RE::TESObjectREFR>() : nullptr;
        if (!JunkHandler::MovedItemIsPreviewableJunk(dest, a_event->baseObj, a_event->uniqueID, false)) {
            return;
        }

        const auto count = a_event->itemCount;
        if (toPlayer) {
            containerPreview_.retrieveCount = ClampNonNegative(containerPreview_.retrieveCount - count);
            containerPreview_.storeCount += count;
        } else {
            containerPreview_.storeCount = ClampNonNegative(containerPreview_.storeCount - count);
            containerPreview_.retrieveCount += count;
        }
    }

    bool SkyPromptIntegration::SelectedRowIsPlayerSide() const {
        auto* itemList = UIUtil::ItemList::GetOpenList();
        auto* selected = itemList ? itemList->GetSelectedItem() : nullptr;
        if (!selected) {
            return previewMenu_ != MenuKind::kContainer;
        }

        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            if (selected->data.owner == player->GetHandle().native_handle()) {
                return true;
            }
            if (selected->data.owner != 0) {
                return false;
            }
        }

        if (previewMenu_ == MenuKind::kBarter) {
            const auto ui = RE::UI::GetSingleton();
            auto menu = ui ? ui->GetMenu<RE::BarterMenu>() : nullptr;
            if (menu && menu->uiMovie) {
                RE::GFxValue result;
                if (menu->uiMovie->GetVariable(&result, "_root.Menu_mc.inventoryLists.categoryList.activeSegment") && result.IsNumber()) {
                    return static_cast<int>(result.GetNumber()) != 0;
                }
            }
            return false;
        }

        if (previewMenu_ == MenuKind::kContainer) {
            const auto ui = RE::UI::GetSingleton();
            auto menu = ui ? ui->GetMenu<RE::ContainerMenu>() : nullptr;
            if (menu && menu->uiMovie) {
                RE::GFxValue result;
                if (menu->uiMovie->GetVariable(&result, "_root.Menu_mc.inventoryLists.categoryList.activeSegment") && result.IsNumber()) {
                    return static_cast<int>(result.GetNumber()) != 0;
                }
            }
        }

        return true;
    }

    void SkyPromptIntegration::ApplyTransferableDelta(
        std::int32_t sign,
        std::int32_t count,
        bool playerSide) {
        if (previewMenu_ != MenuKind::kContainer || !containerPreview_.valid || count <= 0) {
            return;
        }

        const auto delta = sign * count;
        if (playerSide) {
            containerPreview_.storeCount = ClampNonNegative(containerPreview_.storeCount + delta);
        } else {
            containerPreview_.retrieveCount = ClampNonNegative(containerPreview_.retrieveCount + delta);
        }
    }

    void SkyPromptIntegration::ApplySelectedFavoriteChange() {
        if (previewMenu_ != MenuKind::kContainer && previewMenu_ != MenuKind::kBarter) {
            selectedProtection_ = {};
            return;
        }

        auto* itemList = UIUtil::ItemList::GetOpenList();
        auto* selected = itemList ? itemList->GetSelectedItem() : nullptr;
        if (!selected || !selected->data.objDesc || !selected->data.objDesc->object) {
            selectedProtection_ = {};
            return;
        }

        auto* entry = selected->data.objDesc;
        const auto formId = entry->object->GetFormID();
        const auto owner = selected->data.owner;
        const bool playerSide = SelectedRowIsPlayerSide();
        const bool favorited = entry->IsFavorited();
        const bool junk = JunkDataManager::GetSingleton().IsJunk(entry);

        const bool sameItem = selectedProtection_.valid &&
            selectedProtection_.formId == formId &&
            selectedProtection_.owner == owner &&
            selectedProtection_.playerSide == playerSide;

        if (sameItem && junk && Settings::ProtectFavorites() && selectedProtection_.favorited != favorited) {
            const bool blockedByEquip = Settings::ProtectEquipped() && entry->IsWorn();
            const bool blockedByEnchant =
                previewMenu_ == MenuKind::kBarter && Settings::ProtectEnchanted() && entry->IsEnchanted();
            if (!blockedByEquip && !blockedByEnchant) {
                if (previewMenu_ == MenuKind::kBarter) {
                    RequestSellRecapture(true);
                } else {
                    const auto count = JunkHandler::CountJunkUnits(entry);
                    ApplyTransferableDelta(favorited ? -1 : 1, count, playerSide);
                }
            }
        }

        selectedProtection_.formId = formId;
        selectedProtection_.owner = owner;
        selectedProtection_.playerSide = playerSide;
        selectedProtection_.favorited = favorited;
        selectedProtection_.valid = true;
    }

    void SkyPromptIntegration::ApplyEquipChange(const RE::TESEquipEvent* a_event) {
        if (!a_event || previewMenu_ == MenuKind::kNone || previewMenu_ == MenuKind::kInventory) {
            return;
        }
        if (previewMenu_ == MenuKind::kContainer && !containerPreview_.valid) {
            return;
        }

        auto* actor = a_event->actor ? a_event->actor.get() : nullptr;
        if (!actor) {
            return;
        }

        const bool playerSide = previewPlayerId_ != 0 && actor->GetFormID() == previewPlayerId_;
        const bool containerSide = previewContainerId_ != 0 && actor->GetFormID() == previewContainerId_;
        if (previewMenu_ == MenuKind::kBarter) {
            if (!playerSide) {
                return;
            }
        } else if (!playerSide && !containerSide) {
            return;
        }

        const auto unit = JunkHandler::LookupJunkPreviewUnit(
            actor,
            a_event->baseObject,
            a_event->uniqueID);
        if (!unit) {
            return;
        }
        if (Settings::ProtectFavorites() && unit->favorited) {
            return;
        }
        if (previewMenu_ == MenuKind::kBarter) {
            if (Settings::ProtectEnchanted() && unit->enchanted) {
                return;
            }
            RequestSellRecapture(true);
            return;
        }

        ApplyTransferableDelta(a_event->equipped ? -1 : 1, unit->count, playerSide);
    }
}
