#pragma once

#include <mutex>
#include <unordered_set>
#include <vector>
#include <string>

namespace JunkIt {

    struct JunkItem {
        RE::FormID baseFormID;
        uint32_t extraDataHash;
        std::string displayName;

        JunkItem() : baseFormID(0), extraDataHash(0), displayName("") {}
        JunkItem(RE::FormID formID, uint32_t hash, std::string name) 
            : baseFormID(formID), extraDataHash(hash), displayName(std::move(name)) {}

        uint64_t GetPackedKey() const {
            return (static_cast<uint64_t>(baseFormID) << 32) | extraDataHash;
        }
    };

    struct InventoryJunkEntry {
        RE::FormID baseFormID;
        uint32_t extraDataHash;
        std::string displayName;
        int32_t count;

        InventoryJunkEntry() : baseFormID(0), extraDataHash(0), displayName(""), count(0) {}
        InventoryJunkEntry(RE::FormID formID, uint32_t hash, std::string name, int32_t itemCount)
            : baseFormID(formID), extraDataHash(hash), displayName(std::move(name)), count(itemCount) {}
    };

    class JunkDataManager {
    public:
        static JunkDataManager& GetSingleton() {
            static JunkDataManager instance;
            return instance;
        }

        bool AddJunkItem(RE::InventoryEntryData* entry);
        bool RemoveJunkItem(RE::InventoryEntryData* entry);
        bool IsJunk(RE::InventoryEntryData* entry) const;
        bool IsJunk(RE::FormID baseFormID, uint32_t extraDataHash) const;
        bool IsJunk(RE::TESForm* form) const;
        void Clear();
        size_t Size() const;

        std::vector<JunkItem> GetAllJunkItems() const;
        JunkItem GetJunkItemAt(int32_t index) const;
        bool RemoveJunkItemAtIndex(int32_t index);

        std::vector<InventoryJunkEntry> GetPlayerJunkInventory() const;

        static uint32_t ComputeExtraDataHash(RE::InventoryEntryData* entry);
        static uint32_t ComputeExtraDataHash(RE::ExtraDataList* extraList);

        void Save(SKSE::SerializationInterface* intfc);
        void Load(SKSE::SerializationInterface* intfc);
        void Revert(SKSE::SerializationInterface* intfc);

        static void OnSave(SKSE::SerializationInterface* intfc);
        static void OnLoad(SKSE::SerializationInterface* intfc);
        static void OnRevert(SKSE::SerializationInterface* intfc);

        void MigrateFromFormList(RE::BGSListForm* oldJunkList);

    private:
        JunkDataManager() = default;
        JunkDataManager(const JunkDataManager&) = delete;
        JunkDataManager& operator=(const JunkDataManager&) = delete;

        std::unordered_set<uint64_t> junkSet;
        std::vector<JunkItem> junkItems;
        mutable std::mutex lock;

        void RebuildJunkItemsVector();
    };
}
