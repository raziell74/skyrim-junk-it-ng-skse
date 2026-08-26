#pragma once

#include <cstdint>

namespace JunkIt {
    constexpr const char* PLUGIN_NAME = "JunkIt";
    constexpr std::uint32_t API_VERSION = 1;
    constexpr std::uint32_t kMessage_GetAPI = 'JAPI';

    class IAPI {
    public:
        virtual std::uint32_t GetVersion() const = 0;
        virtual bool IsJunk(RE::InventoryEntryData* a_entry) const = 0;
        virtual bool IsJunk(RE::TESBoundObject* a_object, const RE::ExtraDataList* a_extraList, const char* a_displayName) const = 0;
        virtual bool IsAnyJunkForForm(RE::TESForm* a_form) const = 0;
    };

    inline void ListenForAPI(SKSE::MessagingInterface::EventCallback* cb) {
        SKSE::GetMessagingInterface()->RegisterListener(PLUGIN_NAME, cb);
    }

    // SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    //     SKSE::Init(skse);
    //     JunkIt::ListenForAPI([](SKSE::MessagingInterface::Message* msg) {
    //         if (msg->type == JunkIt::kMessage_GetAPI) {
    //             auto* api = static_cast<JunkIt::IAPI*>(msg->data);
    //             bool junk = api->IsJunk(entry);
    //             bool any = api->IsAnyJunkForForm(form);
    //         }
    //     });
    //     return true;
    // }
}
