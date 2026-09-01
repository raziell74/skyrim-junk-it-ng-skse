#pragma once

#include "SkyPrompt/API.hpp"
#include "junk.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace JunkIt {
    class SkyPromptIntegration :
        public SkyPromptAPI::PromptSink,
        public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
        public RE::BSTEventSink<RE::TESContainerChangedEvent>,
        public RE::BSTEventSink<RE::TESEquipEvent> {
    public:
        static SkyPromptIntegration& GetSingleton();

        void Install();
        [[nodiscard]] bool IsShowing() const;
        void NoteInputDevice(RE::INPUT_DEVICE device);
        void RefreshPrompts();
        void RecapturePreviews();
        void InvalidateSellPreview();
        void ScheduleFullRefresh(int framesRemaining);
        void OnJunkToggled(RE::InventoryEntryData* entry, bool nowJunk, bool playerOwned);
        void SyncPromptLabels();
        void ScheduleLabelSync();
        void UpdateMarkHoldVisual(float heldDuration);
        void ResetMarkHoldVisual();
        void UpdateTrashHoldVisual(float heldDuration);
        void ResetTrashHoldVisual();
        void UpdateGamepadHoldVisual(float heldDuration);
        void ResetGamepadHoldVisual();
        [[nodiscard]] bool MarkHoldTrashEnabled() const;
        [[nodiscard]] bool KeyboardTrashHoldEnabled() const;
        [[nodiscard]] static bool ContainerMenuIsPlayerSegment();
        [[nodiscard]] static bool IsPlayerInventoryView();

        static constexpr float kMarkHoldTrashDelay = 0.5f;

        void ProcessEvent(SkyPromptAPI::PromptEvent event) const override;
        std::span<const SkyPromptAPI::Prompt> GetPrompts() const override;
        RE::BSEventNotifyControl ProcessEvent(
            const RE::MenuOpenCloseEvent* a_event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource) override;
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESContainerChangedEvent* a_event,
            RE::BSTEventSource<RE::TESContainerChangedEvent>* a_eventSource) override;
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESEquipEvent* a_event,
            RE::BSTEventSource<RE::TESEquipEvent>* a_eventSource) override;

    private:
        enum class MenuKind {
            kNone = 0,
            kInventory = 1,
            kContainer = 2,
            kBarter = 3
        };

        enum class PromptEventID : SkyPromptAPI::EventID {
            kMark = 0,
            kTransfer = 1,
            kTrash = 2,
            kGamepad = 3
        };

        enum class PromptActionID : SkyPromptAPI::ActionID {
            kMark = 0,
            kTransfer = 1,
            kSell = 2,
            kTrash = 3,
            kGamepad = 4
        };

        SkyPromptIntegration() = default;
        SkyPromptIntegration(const SkyPromptIntegration&) = delete;
        SkyPromptIntegration(SkyPromptIntegration&&) = delete;
        SkyPromptIntegration& operator=(const SkyPromptIntegration&) = delete;
        SkyPromptIntegration& operator=(SkyPromptIntegration&&) = delete;

        static MenuKind GetActiveMenu();
        [[nodiscard]] bool IsEnabled() const;
        [[nodiscard]] bool IsGamepadInputActive() const;
        static RE::FormID PromptAttachRefID();
        static std::optional<std::pair<RE::INPUT_DEVICE, SkyPromptAPI::ButtonID>> ToSkyPromptButton(std::uint32_t keyCode);
        static bool HasSelectedItem();
        static bool SelectedItemIsJunk();
        static const char* MarkPromptText();
        static bool ShouldShowTrashPrompt(MenuKind menu);
        static SkyPromptAPI::PromptType KeyboardTrashPromptType();
        SkyPromptAPI::Prompt* FindMarkPrompt();
        SkyPromptAPI::Prompt* FindTrashPrompt();
        SkyPromptAPI::Prompt* FindGamepadPrompt();
        std::string FormatTransferPrompt();
        std::string FormatSellPrompt();
        void RebuildPrompts(MenuKind menu);
        bool EventIsPlayerAndOpenTarget(RE::FormID oldContainer, RE::FormID newContainer) const;
        void InvalidatePreviews();
        void CaptureContainerPreview();
        void CaptureSellPreview();
        void TryEnsurePreview();
        void RequestSellRecapture(bool rebuildStacks);
        void ScheduleSellPreviewUpdate(int framesRemaining);
        void ApplyPendingSellRecapture();
        void ApplyGoldOnlySellPreview();
        void ApplyContainerMove(const RE::TESContainerChangedEvent* a_event);
        void ApplySelectedFavoriteChange();
        void ApplyEquipChange(const RE::TESEquipEvent* a_event);
        void ApplyTransferableDelta(std::int32_t sign, std::int32_t count, bool playerSide);
        [[nodiscard]] bool SelectedRowIsPlayerSide() const;
        void Send();
        void Remove();

        SkyPromptAPI::ClientID clientID_{ 0 };
        bool showing_{ false };
        std::optional<bool> gamepadInput_;
        MenuKind previewMenu_{ MenuKind::kNone };
        RE::FormID previewPlayerId_{ 0 };
        RE::FormID previewContainerId_{ 0 };
        RE::FormID previewVendorId_{ 0 };
        RE::FormID previewMerchantId_{ 0 };
        struct ContainerPreview {
            std::int32_t storeCount = 0;
            std::int32_t retrieveCount = 0;
            bool valid = false;
        } containerPreview_;
        struct SellPreview {
            std::optional<std::int32_t> gold;
            std::vector<JunkHandler::SellPreviewStack> stacks;
            bool valid = false;
        } sellPreview_;
        bool sellRecapturePending_{ false };
        bool sellRecaptureRebuild_{ false };
        struct SelectedProtectionCache {
            RE::FormID formId = 0;
            std::uint32_t owner = 0;
            bool playerSide = false;
            bool favorited = false;
            bool valid = false;
        } selectedProtection_;
        std::vector<std::pair<RE::INPUT_DEVICE, SkyPromptAPI::ButtonID>> markKeys_;
        std::vector<std::pair<RE::INPUT_DEVICE, SkyPromptAPI::ButtonID>> transferKeys_;
        std::vector<std::pair<RE::INPUT_DEVICE, SkyPromptAPI::ButtonID>> trashKeys_;
        std::vector<std::pair<RE::INPUT_DEVICE, SkyPromptAPI::ButtonID>> gamepadKeys_;
        std::vector<SkyPromptAPI::Prompt> prompts_;
        std::string transferLabel_;
        std::string sellLabel_;
        bool markHoldVisualActive_{ false };
        bool trashHoldVisualActive_{ false };
        bool gamepadHoldVisualActive_{ false };
    };
}
