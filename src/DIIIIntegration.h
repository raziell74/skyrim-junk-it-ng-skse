#pragma once
#include "DIII_API.h"
#include "JunkData.h"

namespace JunkIt {
    class IsJunkCondition : public DIII::ICondition {
    public:
        bool Match(RE::InventoryEntryData* entry) const override {
            if (!entry || !entry->object) {
                return false;
            }

            const char* displayName = entry->GetDisplayName();
            RE::FormID formID = entry->object->GetFormID();
            
            // SKSE::log::info("DIII matching item: {} [FormID: 0x{:X}]", displayName, formID);
            
            bool result = JunkDataManager::GetSingleton().IsJunk(entry);
            
            // SKSE::log::info("DIII match result: {}", result ? "JUNK" : "NOT JUNK");
            
            return result;
        }
    };
}
