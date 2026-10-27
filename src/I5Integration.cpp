#include "I5Integration.h"

#include "I5API.h"

#include <optional>

namespace JunkIt {
    namespace {
        InventoryInjectorImproved::API::IAPI* g_api = nullptr;

        std::optional<RE::FormID> ParseHexFormID(std::string_view text) {
            if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) {
                text.remove_prefix(2);
            }
            if (text.empty() || text.size() > 8) {
                return std::nullopt;
            }

            RE::FormID value = 0;
            for (const char c : text) {
                value <<= 4;
                if (c >= '0' && c <= '9') {
                    value |= static_cast<RE::FormID>(c - '0');
                } else if (c >= 'a' && c <= 'f') {
                    value |= static_cast<RE::FormID>(c - 'a' + 10);
                } else if (c >= 'A' && c <= 'F') {
                    value |= static_cast<RE::FormID>(c - 'A' + 10);
                } else {
                    return std::nullopt;
                }
            }
            return value;
        }

        void OnI5Message(SKSE::MessagingInterface::Message* msg) {
            if (!msg || msg->type != InventoryInjectorImproved::API::kMessage_GetAPI || !msg->data) {
                return;
            }

            auto* api = static_cast<InventoryInjectorImproved::API::IAPI*>(msg->data);
            if (api->GetVersion() < InventoryInjectorImproved::API::kVersion) {
                SKSE::log::warn(
                    "I5 API version {} is older than required {}",
                    api->GetVersion(),
                    InventoryInjectorImproved::API::kVersion);
                return;
            }

            g_api = api;
            SKSE::log::info("Received I5 API version {}", api->GetVersion());
        }
    }

    void I5Integration::Install() {
        SKSE::GetMessagingInterface()->RegisterListener("InventoryInjectorImproved", OnI5Message);
    }

    void I5Integration::InvalidateItem(RE::FormID formID) {
        if (!g_api) {
            return;
        }

        g_api->InvalidateItem(formID);
        SKSE::log::debug("Invalidated i5 cache for {:08X}", formID);
    }

    void I5Integration::InvalidateIdentity(std::string_view identity) {
        if (!g_api) {
            return;
        }

        const auto pipe = identity.find('|');
        if (pipe == std::string_view::npos || pipe == 0) {
            return;
        }

        const auto formConfig = identity.substr(0, pipe);
        const auto tilde = formConfig.find('~');
        RE::TESForm* form = nullptr;
        if (tilde == std::string_view::npos) {
            const auto formId = ParseHexFormID(formConfig);
            if (!formId) {
                return;
            }
            form = RE::TESForm::LookupByID(*formId);
        } else {
            if (tilde == 0 || tilde + 1 >= formConfig.size()) {
                return;
            }
            const auto localId = ParseHexFormID(formConfig.substr(0, tilde));
            if (!localId) {
                return;
            }
            auto* dataHandler = RE::TESDataHandler::GetSingleton();
            if (!dataHandler) {
                return;
            }
            form = dataHandler->LookupForm(*localId, formConfig.substr(tilde + 1));
        }

        if (!form) {
            SKSE::log::debug("I5 invalidate skipped, unresolved identity {}", identity);
            return;
        }

        InvalidateItem(form->GetFormID());
    }
}
