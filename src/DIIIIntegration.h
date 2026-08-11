#pragma once
#include "DIII_API.h"
#include "JunkData.h"
#include "settings.h"

namespace JunkIt {
    class IsJunkCondition : public DIII::ICondition {
    public:
        bool Match(RE::InventoryEntryData* entry) const override {
            if (!Settings::GetUseDynamicInventoryIcon()) {
                return false;
            }

            if (!entry || !entry->object) {
                return false;
            }

            if (entry->IsQuestObject()) {
                return false;
            }

            return JunkDataManager::GetSingleton().IsJunk(entry);
        }
    };
}
