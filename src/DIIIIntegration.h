#pragma once
#include "DIII_API.h"
#include "JunkData.h"

namespace JunkIt {
    class IsJunkCondition : public DIII::ICondition {
    public:
        bool Match(RE::InventoryEntryData* entry) const override {
            return JunkDataManager::GetSingleton().IsJunk(entry);
        }
    };
}
