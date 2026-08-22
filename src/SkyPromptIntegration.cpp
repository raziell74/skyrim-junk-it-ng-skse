#include "SkyPromptIntegration.h"

#include "JunkData.h"
#include "Translation.h"
#include "event.h"
#include "junk.h"
#include "settings.h"
#include "util.h"

#include <SKSE/API.h>
#include <fmt/format.h>

namespace JunkIt {
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
        }
        SKSE::log::info("SkyPrompt client {} registered", clientID_);
        RefreshPrompts();
    }

    bool SkyPromptIntegration::IsShowing() const {
        return showing_;
    }

    void SkyPromptIntegration::RefreshPrompts() {
        if (clientID_ == 0) {
            return;
        }

        Remove();

        const auto menu = GetActiveMenu();
        if (menu == MenuKind::kNone) {
            return;
        }

        RebuildPrompts(menu);
        Send();
    }

    void SkyPromptIntegration::ProcessEvent(SkyPromptAPI::PromptEvent event) const {
        auto& self = GetSingleton();

        if (event.type == SkyPromptAPI::PromptEventType::kAccepted) {
            switch (static_cast<PromptActionID>(event.prompt.actionID)) {
                case PromptActionID::kMark:
                    InputEventHandler::GetSingleton()->ExecuteAction(JUNKIT_EVENT_TYPE::kMark);
                    break;
                case PromptActionID::kTransfer:
                    InputEventHandler::GetSingleton()->ExecuteAction(JUNKIT_EVENT_TYPE::kTransfer);
                    break;
                case PromptActionID::kSell:
                    InputEventHandler::GetSingleton()->ExecuteAction(JUNKIT_EVENT_TYPE::kSell);
                    break;
                default:
                    break;
            }
            self.RefreshPrompts();
            return;
        }

        if (event.type == SkyPromptAPI::PromptEventType::kTimingOut ||
            event.type == SkyPromptAPI::PromptEventType::kTimeout) {
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

        RefreshPrompts();
        return RE::BSEventNotifyControl::kContinue;
    }

    RE::BSEventNotifyControl SkyPromptIntegration::ProcessEvent(
        const RE::TESContainerChangedEvent* a_event,
        RE::BSTEventSource<RE::TESContainerChangedEvent>*) {
        if (!a_event || !showing_ || GetActiveMenu() == MenuKind::kNone) {
            return RE::BSEventNotifyControl::kContinue;
        }

        if (EventInvolvesOpenMenu(a_event->oldContainer, a_event->newContainer)) {
            ScheduleLabelSync();
        }
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
        if (clientID_ == 0) {
            return;
        }

        auto* tasks = SKSE::GetTaskInterface();
        if (!tasks) {
            SyncPromptLabels();
            return;
        }

        tasks->AddUITask([]() {
            auto& self = SkyPromptIntegration::GetSingleton();
            if (self.showing_ && GetActiveMenu() != MenuKind::kNone) {
                self.SyncPromptLabels();
            }
        });
    }

    bool SkyPromptIntegration::EventInvolvesOpenMenu(RE::FormID oldContainer, RE::FormID newContainer) {
        if (auto* player = RE::PlayerCharacter::GetSingleton()) {
            const auto playerId = player->GetFormID();
            if (oldContainer == playerId || newContainer == playerId) {
                return true;
            }
        }

        switch (GetActiveMenu()) {
            case MenuKind::kContainer:
                if (auto* container = JunkHandler::GetContainerMenuContainer()) {
                    const auto id = container->GetFormID();
                    return oldContainer == id || newContainer == id;
                }
                break;
            case MenuKind::kBarter:
                if (auto* vendor = JunkHandler::GetBarterMenuContainer()) {
                    const auto id = vendor->GetFormID();
                    if (oldContainer == id || newContainer == id) {
                        return true;
                    }
                }
                if (auto* merchant = JunkHandler::GetBarterMenuMerchantContainer()) {
                    const auto id = merchant->GetFormID();
                    return oldContainer == id || newContainer == id;
                }
                break;
            default:
                break;
        }

        return false;
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
        return SelectedItemIsJunk() ? "$JunkIt_Prompt_Unmark" : "$JunkIt_Prompt_Mark";
    }

    std::string SkyPromptIntegration::FormatTransferPrompt() {
        const auto count = JunkHandler::PreviewTransferCount();
        if (count <= 0) {
            return {};
        }

        const char* key = "$JunkIt_Prompt_Retrieve";
        const auto ui = RE::UI::GetSingleton();
        auto menu = ui ? ui->GetMenu<RE::ContainerMenu>() : nullptr;
        if (menu && menu->uiMovie) {
            RE::GFxValue result;
            menu->uiMovie->GetVariable(&result, "_root.Menu_mc.inventoryLists.categoryList.activeSegment");
            if (static_cast<int>(result.GetNumber()) != 0) {
                key = "$JunkIt_Prompt_Store";
            }
        }

        return fmt::format("{} ({})", Translation::Get(key), count);
    }

    std::string SkyPromptIntegration::FormatSellPrompt() {
        const auto gold = JunkHandler::PreviewSellGold();
        if (!gold) {
            return {};
        }

        return fmt::format("{} ({}g)", Translation::Get("$JunkIt_Prompt_Sell"), *gold);
    }

    void SkyPromptIntegration::SyncPromptLabels() {
        if (clientID_ == 0 || !showing_ || prompts_.empty()) {
            return;
        }

        const auto menu = GetActiveMenu();
        if (menu == MenuKind::kNone) {
            return;
        }

        const auto markAction = static_cast<SkyPromptAPI::ActionID>(PromptActionID::kMark);
        const auto transferAction = static_cast<SkyPromptAPI::ActionID>(PromptActionID::kTransfer);
        const auto sellAction = static_cast<SkyPromptAPI::ActionID>(PromptActionID::kSell);

        bool hasTransfer = false;
        bool hasSell = false;
        for (const auto& prompt : prompts_) {
            if (prompt.actionID == transferAction) {
                hasTransfer = true;
            } else if (prompt.actionID == sellAction) {
                hasSell = true;
            }
        }

        std::string wantedTransfer;
        if (menu == MenuKind::kContainer) {
            wantedTransfer = FormatTransferPrompt();
        }
        std::string wantedSell;
        if (menu == MenuKind::kBarter) {
            wantedSell = FormatSellPrompt();
        }
        if (hasTransfer != !wantedTransfer.empty() || hasSell != !wantedSell.empty()) {
            RefreshPrompts();
            return;
        }

        const char* wantedMark = MarkPromptText();

        bool changed = false;
        for (auto& prompt : prompts_) {
            if (prompt.actionID == markAction && prompt.text != wantedMark) {
                prompt.text = wantedMark;
                changed = true;
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
        prompts_.clear();

        if (auto markButton = ToSkyPromptButton(static_cast<std::uint32_t>(Settings::GetMarkJunkKey()))) {
            markKeys_.push_back(*markButton);
        }
        if (auto transferButton = ToSkyPromptButton(static_cast<std::uint32_t>(Settings::GetTransferJunkKey()))) {
            transferKeys_.push_back(*transferButton);
        }

        if (!markKeys_.empty()) {
            prompts_.emplace_back(
                MarkPromptText(),
                static_cast<SkyPromptAPI::EventID>(PromptEventID::kMark),
                static_cast<SkyPromptAPI::ActionID>(PromptActionID::kMark),
                SkyPromptAPI::PromptType::kSinglePress,
                0,
                markKeys_);
        }

        if (menu == MenuKind::kInventory || transferKeys_.empty()) {
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
                    0,
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
                    0,
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
        transferLabel_.clear();
        sellLabel_.clear();
    }
}
