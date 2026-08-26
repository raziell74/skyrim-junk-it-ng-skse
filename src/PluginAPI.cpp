#include "PluginAPI.h"
#include "JunkIt.h"
#include "JunkData.h"

namespace JunkIt {
    class API final : public IAPI {
    public:
        std::uint32_t GetVersion() const override {
            return API_VERSION;
        }

        bool IsJunk(RE::InventoryEntryData* a_entry) const override {
            return JunkDataManager::GetSingleton().IsJunk(a_entry);
        }

        bool IsJunk(RE::TESBoundObject* a_object, const RE::ExtraDataList* a_extraList, const char* a_displayName) const override {
            return JunkDataManager::GetSingleton().IsJunk(
                a_object,
                a_extraList,
                a_displayName ? a_displayName : "");
        }

        bool IsAnyJunkForForm(RE::TESForm* a_form) const override {
            return JunkDataManager::GetSingleton().IsAnyJunkForForm(a_form);
        }
    };

    void BroadcastAPI() {
        static API api;
        SKSE::GetMessagingInterface()->Dispatch(kMessage_GetAPI, &api, sizeof(void*), nullptr);
    }
}
