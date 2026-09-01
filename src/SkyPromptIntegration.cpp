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
        constexpr float kMarkHoldTrashDelay = 0.5f;
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
            SKSE::log::info("SkyPrompt not installed; on-screen prompts disabled");
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
        if (!IsEnabled()) {
            RefreshPrompts();
            return;
        }

        const auto menu = GetActiveMenu();
        if (menu == MenuKind::kContainer) {
            previewMenu_ = MenuKind::kContainer;
            CaptureContainerPreview();
            if (!containerPreview_.valid) {
                ScheduleLabelSync();
            }
        } else if (menu == MenuKind::kBarter) {
            previewMenu_ = MenuKind::kBarter;
            CaptureSellPreview();
            if (!sellPreview_.valid) {
                ScheduleLabelSync();
            }
        }
        RefreshPrompts();
    }

    void SkyPromptIntegration::ScheduleFullRefresh(int framesRemaining) {
        if (!IsEnabled()) {
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
                    InputEventHandler::GetSingleton()->ExecuteAction(JUNKIT_EVENT_TYPE::kTrashBulk);
                    break;
                default:
                    break;
            }
            self.RefreshPrompts();
            return;
        }

        if (event.type == SkyPromptAPI::PromptEventType::kDeclined &&
            static_cast<PromptActionID>(event.prompt.actionID) == PromptActionID::kMark &&
            MarkHoldTrashEnabled()) {
            return;
        }

        if (event.type == SkyPromptAPI::PromptEventType::kTimingOut ||
            event.type == SkyPromptAPI::PromptEventType::kTimeout) {
            if (static_cast<PromptActionID>(event.prompt.actionID) == PromptActionID::kMark &&
                MarkHoldTrashEnabled()) {
                return;
            }
            self.RefreshPrompts();
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
                RefreshPrompts();
            }
            return RE::BSEventNotifyControl::kContinue;
        }

        if (name == "QuantityMenu") {
            if (GetActiveMenu() != MenuKind::kNone) {
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

        RefreshPrompts();
        if (a_event->opening &&
            Settings::GetSkyPromptButtonPlacement() == Settings::SkyPromptButtonPlacement::kAttachToItemModel &&
            Inventory3DModelPending()) {
            ScheduleLabelSync();
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

        if (auto* gold = Settings::GetGold001(); gold && a_event->baseObj == gold->GetFormID()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (!EventIsPlayerAndOpenTarget(a_event->oldContainer, a_event->newContainer)) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (previewMenu_ == MenuKind::kContainer && containerPreview_.valid) {
            ApplyContainerMove(a_event);
        } else if (previewMenu_ == MenuKind::kBarter && sellPreview_.valid) {
            ApplyBarterMove(a_event);
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

    bool SkyPromptIntegration::MarkHoldTrashEnabled() {
        return Settings::IsTrashAvailable() && Settings::GetTrashHoldSeconds() > 0;
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

    std::string SkyPromptIntegration::FormatTransferPrompt() {
        bool storeView = false;
        const auto ui = RE::UI::GetSingleton();
        auto menu = ui ? ui->GetMenu<RE::ContainerMenu>() : nullptr;
        if (menu && menu->uiMovie) {
            RE::GFxValue result;
            if (menu->uiMovie->GetVariable(&result, "_root.Menu_mc.inventoryLists.categoryList.activeSegment") && result.IsNumber()) {
                storeView = static_cast<int>(result.GetNumber()) != 0;
            }
        }

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

        if (!sellPreview_.valid || !sellPreview_.gold) {
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

        TryEnsurePreview();
        ApplySelectedFavoriteChange();

        const auto markAction = static_cast<SkyPromptAPI::ActionID>(PromptActionID::kMark);
        const auto transferAction = static_cast<SkyPromptAPI::ActionID>(PromptActionID::kTransfer);
        const auto sellAction = static_cast<SkyPromptAPI::ActionID>(PromptActionID::kSell);
        const auto wantedRefId = PromptAttachRefID();

        bool hasMark = false;
        bool hasTransfer = false;
        bool hasSell = false;
        bool refidMismatch = false;
        for (const auto& prompt : prompts_) {
            if (prompt.actionID == markAction) {
                hasMark = true;
            } else if (prompt.actionID == transferAction) {
                hasTransfer = true;
            } else if (prompt.actionID == sellAction) {
                hasSell = true;
            }
            if (prompt.refid != wantedRefId) {
                refidMismatch = true;
            }
        }

        const char* wantedMark = MarkPromptText();
        std::string wantedTransfer;
        if (menu == MenuKind::kContainer) {
            wantedTransfer = FormatTransferPrompt();
        }
        std::string wantedSell;
        if (menu == MenuKind::kBarter) {
            wantedSell = FormatSellPrompt();
        }

        if (hasMark != (wantedMark != nullptr) ||
            hasTransfer != !wantedTransfer.empty() ||
            hasSell != !wantedSell.empty() ||
            refidMismatch) {
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

        const auto attachRefId = PromptAttachRefID();

        if (!markKeys_.empty()) {
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

        if (menu == MenuKind::kInventory) {
            if (!trashKeys_.empty()) {
                prompts_.emplace_back(
                    "$JunkIt_Prompt_Trash",
                    static_cast<SkyPromptAPI::EventID>(PromptEventID::kTrash),
                    static_cast<SkyPromptAPI::ActionID>(PromptActionID::kTrash),
                    SkyPromptAPI::PromptType::kSinglePress,
                    attachRefId,
                    trashKeys_);
            }
            return;
        }

        if (transferKeys_.empty()) {
            return;
        }

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
        transferLabel_.clear();
        sellLabel_.clear();
        markHoldVisualActive_ = false;
    }

    void SkyPromptIntegration::InvalidatePreviews() {
        containerPreview_ = {};
        sellPreview_ = {};
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

    void SkyPromptIntegration::CaptureSellPreview() {
        sellPreview_ = {};
        previewPlayerId_ = 0;
        previewVendorId_ = 0;
        previewMerchantId_ = 0;
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            previewPlayerId_ = player->GetFormID();
        }
        if (auto* vendor = JunkHandler::GetBarterMenuContainer()) {
            previewVendorId_ = vendor->GetFormID();
        }
        if (auto* merchant = JunkHandler::GetBarterMenuMerchantContainer()) {
            previewMerchantId_ = merchant->GetFormID();
        }

        const auto captured = JunkHandler::CaptureSellPreview();
        sellPreview_.sellMult = captured.sellMult;
        if (!captured.pricesReady) {
            return;
        }

        sellPreview_.gold = captured.gold;
        sellPreview_.valid = true;
    }

    void SkyPromptIntegration::TryEnsurePreview() {
        if (previewMenu_ == MenuKind::kContainer && !containerPreview_.valid) {
            CaptureContainerPreview();
        } else if (previewMenu_ == MenuKind::kBarter && !sellPreview_.valid) {
            CaptureSellPreview();
        }
    }

    void SkyPromptIntegration::OnJunkToggled(RE::InventoryEntryData* entry, bool nowJunk, bool playerOwned) {
        if (!entry || !IsEnabled()) {
            return;
        }

        std::vector<std::string> identities;
        JunkHandler::CollectEntryIdentities(entry, identities);
        if (identities.empty()) {
            return;
        }

        const std::int32_t sign = nowJunk ? 1 : -1;

        if (previewMenu_ == MenuKind::kContainer && containerPreview_.valid) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            RE::TESObjectREFR* container = nullptr;
            if (auto* form = RE::TESForm::LookupByID(previewContainerId_)) {
                container = form->As<RE::TESObjectREFR>();
            }
            const auto playerCount = JunkHandler::CountPreviewIdentities(player, identities, false);
            const auto containerCount = JunkHandler::CountPreviewIdentities(container, identities, false);
            containerPreview_.storeCount = ClampNonNegative(containerPreview_.storeCount + sign * playerCount);
            containerPreview_.retrieveCount = ClampNonNegative(containerPreview_.retrieveCount + sign * containerCount);
        } else if (previewMenu_ == MenuKind::kBarter && sellPreview_.valid && playerOwned) {
            auto* player = RE::PlayerCharacter::GetSingleton();
            const auto count = JunkHandler::CountPreviewIdentities(player, identities, true);
            const auto goldDelta = JunkHandler::ComputeSellGoldDelta(entry, count, sellPreview_.sellMult);
            ApplySellGoldDelta(sign * goldDelta);
        }
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

    void SkyPromptIntegration::ApplyBarterMove(const RE::TESContainerChangedEvent* a_event) {
        if (!a_event || previewPlayerId_ == 0) {
            return;
        }

        const bool fromPlayer = a_event->oldContainer == previewPlayerId_;
        const bool toPlayer = a_event->newContainer == previewPlayerId_;
        if (fromPlayer == toPlayer) {
            return;
        }

        auto* dest = RE::TESForm::LookupByID(a_event->newContainer);
        auto* destRef = dest ? dest->As<RE::TESObjectREFR>() : nullptr;
        const auto goldDelta = JunkHandler::ComputeMovedItemSellGold(
            destRef,
            a_event->baseObj,
            a_event->uniqueID,
            a_event->itemCount,
            sellPreview_.sellMult);
        if (!goldDelta) {
            return;
        }

        if (toPlayer) {
            ApplySellGoldDelta(*goldDelta);
        } else {
            ApplySellGoldDelta(-*goldDelta);
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

    void SkyPromptIntegration::ApplySellGoldDelta(std::int32_t goldDelta) {
        if (goldDelta == 0 || !sellPreview_.valid) {
            return;
        }

        if (goldDelta > 0) {
            if (!sellPreview_.gold) {
                sellPreview_.gold = goldDelta;
            } else {
                *sellPreview_.gold += goldDelta;
                if (*sellPreview_.gold < 0) {
                    *sellPreview_.gold = 0;
                }
            }
            return;
        }

        if (sellPreview_.gold) {
            *sellPreview_.gold = ClampNonNegative(*sellPreview_.gold + goldDelta);
        }
    }

    void SkyPromptIntegration::ApplyTransferableDelta(
        std::int32_t sign,
        std::int32_t count,
        bool playerSide,
        std::int32_t goldDelta) {
        if (previewMenu_ == MenuKind::kContainer && containerPreview_.valid) {
            if (count <= 0) {
                return;
            }
            const auto delta = sign * count;
            if (playerSide) {
                containerPreview_.storeCount = ClampNonNegative(containerPreview_.storeCount + delta);
            } else {
                containerPreview_.retrieveCount = ClampNonNegative(containerPreview_.retrieveCount + delta);
            }
            return;
        }

        if (previewMenu_ == MenuKind::kBarter && sellPreview_.valid && playerSide) {
            ApplySellGoldDelta(sign * goldDelta);
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
                const auto count = JunkHandler::CountJunkUnits(entry);
                const auto gold = previewMenu_ == MenuKind::kBarter
                    ? JunkHandler::ComputeSellGoldDelta(entry, count, sellPreview_.sellMult)
                    : 0;
                ApplyTransferableDelta(favorited ? -1 : 1, count, playerSide, gold);
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
        if (previewMenu_ == MenuKind::kBarter && !sellPreview_.valid) {
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

        const float sellMult = previewMenu_ == MenuKind::kBarter ? sellPreview_.sellMult : 0.0f;
        const auto unit = JunkHandler::LookupJunkPreviewUnit(
            actor,
            a_event->baseObject,
            a_event->uniqueID,
            sellMult);
        if (!unit) {
            return;
        }
        if (Settings::ProtectFavorites() && unit->favorited) {
            return;
        }
        if (previewMenu_ == MenuKind::kBarter && Settings::ProtectEnchanted() && unit->enchanted) {
            return;
        }

        ApplyTransferableDelta(a_event->equipped ? -1 : 1, unit->count, playerSide, unit->gold);
    }
}
