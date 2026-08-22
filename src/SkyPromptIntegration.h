#pragma once

#include "SkyPrompt/API.hpp"

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
        public RE::BSTEventSink<RE::TESContainerChangedEvent> {
    public:
        static SkyPromptIntegration& GetSingleton();

        void Install();
        [[nodiscard]] bool IsShowing() const;
        void RefreshPrompts();
        void SyncPromptLabels();
        void ScheduleLabelSync();

        void ProcessEvent(SkyPromptAPI::PromptEvent event) const override;
        std::span<const SkyPromptAPI::Prompt> GetPrompts() const override;
        RE::BSEventNotifyControl ProcessEvent(
            const RE::MenuOpenCloseEvent* a_event,
            RE::BSTEventSource<RE::MenuOpenCloseEvent>* a_eventSource) override;
        RE::BSEventNotifyControl ProcessEvent(
            const RE::TESContainerChangedEvent* a_event,
            RE::BSTEventSource<RE::TESContainerChangedEvent>* a_eventSource) override;

    private:
        enum class MenuKind {
            kNone = 0,
            kInventory = 1,
            kContainer = 2,
            kBarter = 3
        };

        enum class PromptEventID : SkyPromptAPI::EventID {
            kMark = 0,
            kTransfer = 1
        };

        enum class PromptActionID : SkyPromptAPI::ActionID {
            kMark = 0,
            kTransfer = 1,
            kSell = 2
        };

        SkyPromptIntegration() = default;
        SkyPromptIntegration(const SkyPromptIntegration&) = delete;
        SkyPromptIntegration(SkyPromptIntegration&&) = delete;
        SkyPromptIntegration& operator=(const SkyPromptIntegration&) = delete;
        SkyPromptIntegration& operator=(SkyPromptIntegration&&) = delete;

        static MenuKind GetActiveMenu();
        static std::optional<std::pair<RE::INPUT_DEVICE, SkyPromptAPI::ButtonID>> ToSkyPromptButton(std::uint32_t keyCode);
        static bool SelectedItemIsJunk();
        static const char* MarkPromptText();
        std::string FormatTransferPrompt();
        std::string FormatSellPrompt();
        void RebuildPrompts(MenuKind menu);
        bool EventInvolvesOpenMenu(RE::FormID oldContainer, RE::FormID newContainer);
        void Send();
        void Remove();

        SkyPromptAPI::ClientID clientID_{ 0 };
        bool showing_{ false };
        std::vector<std::pair<RE::INPUT_DEVICE, SkyPromptAPI::ButtonID>> markKeys_;
        std::vector<std::pair<RE::INPUT_DEVICE, SkyPromptAPI::ButtonID>> transferKeys_;
        std::vector<SkyPromptAPI::Prompt> prompts_;
        std::string transferLabel_;
        std::string sellLabel_;
    };
}
