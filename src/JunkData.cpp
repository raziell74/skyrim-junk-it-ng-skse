#include "JunkData.h"
#include "util.h"

namespace JunkIt {

    uint32_t JunkDataManager::ComputeExtraDataHash(RE::InventoryEntryData* entry) {
        if (!entry || !entry->extraLists || entry->extraLists->empty()) {
            return 0;
        }

        uint32_t hash = 0;
        for (auto* extraList : *entry->extraLists) {
            if (extraList) {
                hash ^= ComputeExtraDataHash(extraList);
            }
        }

        return hash;
    }

    uint32_t JunkDataManager::ComputeExtraDataHash(RE::ExtraDataList* extraList) {
        if (!extraList) {
            return 0;
        }

        uint32_t hash = 0;

        if (auto* enchantment = extraList->GetByType<RE::ExtraEnchantment>()) {
            if (enchantment->enchantment) {
                hash ^= std::hash<RE::FormID>{}(enchantment->enchantment->GetFormID());
            }
        }

        if (auto* health = extraList->GetByType<RE::ExtraHealth>()) {
            hash ^= std::hash<float>{}(health->health);
        }

        if (auto* textDisplay = extraList->GetByType<RE::ExtraTextDisplayData>()) {
            if (textDisplay->displayName && textDisplay->displayNameLen > 0) {
                std::string name(textDisplay->displayName, textDisplay->displayNameLen);
                hash ^= std::hash<std::string>{}(name);
            }
        }

        if (auto* ownership = extraList->GetByType<RE::ExtraOwnership>()) {
            if (ownership->owner) {
                hash ^= std::hash<RE::FormID>{}(ownership->owner->GetFormID());
            }
        }

        return hash;
    }

    bool JunkDataManager::AddJunkItem(RE::InventoryEntryData* entry) {
        if (!entry || !entry->object) {
            return false;
        }

        RE::FormID baseFormID = entry->object->GetFormID();
        
        std::lock_guard<std::mutex> guard(lock);
        
        uint32_t extraHash = ComputeExtraDataHash(entry);
        uint64_t packedKey = (static_cast<uint64_t>(baseFormID) << 32) | extraHash;

        if (junkSet.find(packedKey) != junkSet.end()) {
            return false;
        }

        junkSet.insert(packedKey);

        std::string displayName = entry->GetDisplayName();
        junkItems.emplace_back(baseFormID, extraHash, displayName);

        SKSE::log::info("Added junk item: {} [FormID: 0x{:X}, Hash: 0x{:X}]", 
            displayName, baseFormID, extraHash);

        return true;
    }

    bool JunkDataManager::RemoveJunkItem(RE::InventoryEntryData* entry) {
        if (!entry || !entry->object) {
            return false;
        }

        RE::FormID baseFormID = entry->object->GetFormID();
        
        std::lock_guard<std::mutex> guard(lock);
        
        uint32_t extraHash = ComputeExtraDataHash(entry);
        uint64_t packedKey = (static_cast<uint64_t>(baseFormID) << 32) | extraHash;

        auto it = junkSet.find(packedKey);
        if (it == junkSet.end()) {
            return false;
        }

        junkSet.erase(it);
        RebuildJunkItemsVector();

        SKSE::log::info("Removed junk item: {} [FormID: 0x{:X}, Hash: 0x{:X}]", 
            entry->GetDisplayName(), baseFormID, extraHash);

        return true;
    }

    bool JunkDataManager::IsJunk(RE::InventoryEntryData* entry) const {
        if (!entry || !entry->object) {
            return false;
        }

        RE::FormID baseFormID = entry->object->GetFormID();
        
        std::lock_guard<std::mutex> guard(lock);
        
        uint32_t extraHash = ComputeExtraDataHash(entry);
        uint64_t packedKey = (static_cast<uint64_t>(baseFormID) << 32) | extraHash;
        return junkSet.find(packedKey) != junkSet.end();
    }

    bool JunkDataManager::IsJunk(RE::FormID baseFormID, uint32_t extraDataHash) const {
        std::lock_guard<std::mutex> guard(lock);
        uint64_t packedKey = (static_cast<uint64_t>(baseFormID) << 32) | extraDataHash;
        return junkSet.find(packedKey) != junkSet.end();
    }

    bool JunkDataManager::IsJunk(RE::TESForm* form) const {
        if (!form) {
            return false;
        }
        return IsJunk(form->GetFormID(), 0);
    }

    void JunkDataManager::Clear() {
        std::lock_guard<std::mutex> guard(lock);
        junkSet.clear();
        junkItems.clear();
        SKSE::log::info("Cleared all junk items");
    }

    size_t JunkDataManager::Size() const {
        std::lock_guard<std::mutex> guard(lock);
        return junkItems.size();
    }

    std::vector<JunkItem> JunkDataManager::GetAllJunkItems() const {
        std::lock_guard<std::mutex> guard(lock);
        return junkItems;
    }

    JunkItem JunkDataManager::GetJunkItemAt(int32_t index) const {
        std::lock_guard<std::mutex> guard(lock);
        if (index < 0 || index >= static_cast<int32_t>(junkItems.size())) {
            return JunkItem();
        }
        return junkItems[index];
    }

    bool JunkDataManager::RemoveJunkItemAtIndex(int32_t index) {
        std::lock_guard<std::mutex> guard(lock);
        
        if (index < 0 || index >= static_cast<int32_t>(junkItems.size())) {
            return false;
        }

        const auto& item = junkItems[index];
        uint64_t packedKey = item.GetPackedKey();
        std::string displayName = item.displayName;
        RE::FormID baseFormID = item.baseFormID;
        uint32_t extraDataHash = item.extraDataHash;
        
        junkSet.erase(packedKey);
        junkItems.erase(junkItems.begin() + index);

        SKSE::log::info("Removed junk item at index {}: {} [FormID: 0x{:X}, Hash: 0x{:X}]", 
            index, displayName, baseFormID, extraDataHash);

        return true;
    }

    void JunkDataManager::RebuildJunkItemsVector() {
        junkItems.clear();
        
        for (uint64_t packedKey : junkSet) {
            RE::FormID baseFormID = static_cast<RE::FormID>(packedKey >> 32);
            uint32_t extraHash = static_cast<uint32_t>(packedKey & 0xFFFFFFFF);
            
            auto* form = RE::TESForm::LookupByID(baseFormID);
            std::string displayName = form ? form->GetName() : "Unknown Item";
            
            junkItems.emplace_back(baseFormID, extraHash, displayName);
        }
    }

    void JunkDataManager::Save(SKSE::SerializationInterface* intfc) {
        if (!intfc) {
            SKSE::log::error("JunkDataManager::Save - SerializationInterface is null");
            return;
        }

        std::lock_guard<std::mutex> guard(lock);

        uint32_t itemCount = static_cast<uint32_t>(junkItems.size());
        
        if (!intfc->WriteRecordData(&itemCount, sizeof(itemCount))) {
            SKSE::log::error("Failed to write junk item count");
            return;
        }

        SKSE::log::info("Saving {} junk items to co-save", itemCount);

        for (const auto& item : junkItems) {
            if (!intfc->WriteRecordData(&item.baseFormID, sizeof(item.baseFormID))) {
                SKSE::log::error("Failed to write baseFormID");
                return;
            }

            if (!intfc->WriteRecordData(&item.extraDataHash, sizeof(item.extraDataHash))) {
                SKSE::log::error("Failed to write extraDataHash");
                return;
            }

            uint16_t nameLength = static_cast<uint16_t>(item.displayName.length());
            if (!intfc->WriteRecordData(&nameLength, sizeof(nameLength))) {
                SKSE::log::error("Failed to write displayName length");
                return;
            }

            if (nameLength > 0) {
                if (!intfc->WriteRecordData(item.displayName.c_str(), nameLength)) {
                    SKSE::log::error("Failed to write displayName");
                    return;
                }
            }
        }

        SKSE::log::info("Successfully saved {} junk items", itemCount);
    }

    void JunkDataManager::Load(SKSE::SerializationInterface* intfc) {
        if (!intfc) {
            SKSE::log::error("JunkDataManager::Load - SerializationInterface is null");
            return;
        }

        std::lock_guard<std::mutex> guard(lock);
        junkSet.clear();
        junkItems.clear();

        uint32_t itemCount = 0;
        if (!intfc->ReadRecordData(&itemCount, sizeof(itemCount))) {
            SKSE::log::error("Failed to read junk item count");
            return;
        }

        SKSE::log::info("Loading {} junk items from co-save", itemCount);

        for (uint32_t i = 0; i < itemCount; ++i) {
            RE::FormID baseFormID = 0;
            uint32_t extraDataHash = 0;
            uint16_t nameLength = 0;

            if (!intfc->ReadRecordData(&baseFormID, sizeof(baseFormID))) {
                SKSE::log::error("Failed to read baseFormID at index {}", i);
                return;
            }

            if (!intfc->ReadRecordData(&extraDataHash, sizeof(extraDataHash))) {
                SKSE::log::error("Failed to read extraDataHash at index {}", i);
                return;
            }

            if (!intfc->ReadRecordData(&nameLength, sizeof(nameLength))) {
                SKSE::log::error("Failed to read displayName length at index {}", i);
                return;
            }

            std::string displayName;
            if (nameLength > 0) {
                displayName.resize(nameLength);
                if (!intfc->ReadRecordData(displayName.data(), nameLength)) {
                    SKSE::log::error("Failed to read displayName at index {}", i);
                    return;
                }
            }

            if (!intfc->ResolveFormID(baseFormID, baseFormID)) {
                SKSE::log::warn("Failed to resolve FormID 0x{:X} at index {}, skipping", baseFormID, i);
                continue;
            }

            uint64_t packedKey = (static_cast<uint64_t>(baseFormID) << 32) | extraDataHash;
            junkSet.insert(packedKey);
            junkItems.emplace_back(baseFormID, extraDataHash, displayName);
        }

        SKSE::log::info("Successfully loaded {} junk items", junkItems.size());
    }

    void JunkDataManager::Revert(SKSE::SerializationInterface* intfc) {
        std::lock_guard<std::mutex> guard(lock);
        junkSet.clear();
        junkItems.clear();
        SKSE::log::info("JunkDataManager reverted (new game)");
    }

    void JunkDataManager::OnSave(SKSE::SerializationInterface* intfc) {
        if (!intfc->OpenRecord('JNKT', 1)) {
            SKSE::log::error("Failed to open JNKT record for saving");
            return;
        }
        GetSingleton().Save(intfc);
    }

    void JunkDataManager::OnLoad(SKSE::SerializationInterface* intfc) {
        uint32_t type;
        uint32_t version;
        uint32_t length;

        while (intfc->GetNextRecord(type, version, length)) {
            if (type == 'JNKT') {
                if (version != 1) {
                    SKSE::log::error("Unknown JNKT record version: {}", version);
                    continue;
                }
                GetSingleton().Load(intfc);
            }
        }
    }

    void JunkDataManager::OnRevert(SKSE::SerializationInterface* intfc) {
        GetSingleton().Revert(intfc);
    }

    void JunkDataManager::MigrateFromFormList(RE::BGSListForm* oldJunkList) {
        if (!oldJunkList) {
            return;
        }

        std::lock_guard<std::mutex> guard(lock);

        SKSE::log::info("Migrating {} items from old FormList to JunkDataManager", oldJunkList->forms.size());

        for (auto* form : oldJunkList->forms) {
            if (!form) continue;

            RE::FormID baseFormID = form->GetFormID();
            uint32_t extraHash = 0;
            uint64_t packedKey = (static_cast<uint64_t>(baseFormID) << 32) | extraHash;

            if (junkSet.find(packedKey) == junkSet.end()) {
                junkSet.insert(packedKey);
                junkItems.emplace_back(baseFormID, extraHash, form->GetName());
                SKSE::log::info("Migrated: {} [0x{:X}]", form->GetName(), baseFormID);
            }
        }

        SKSE::log::info("Migration complete. Total junk items: {}", junkItems.size());
    }
}
