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
            return PackJunkKey(baseFormID, extraDataHash);
        }

        static uint64_t PackJunkKey(RE::FormID baseFormID, uint32_t instanceExtraHash) {
            return (static_cast<uint64_t>(baseFormID) << 32) | static_cast<uint64_t>(instanceExtraHash);
        }

        static void UnpackJunkKey(uint64_t packed, RE::FormID& outForm, uint32_t& outHash) {
            outForm = static_cast<RE::FormID>(packed >> 32);
            outHash = static_cast<uint32_t>(packed & 0xFFFFFFFFu);
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

        /** True iff (base form, extra hash 0) is in the junk set — legacy / FormList semantics (all "plain" stacks). */
        bool IsBaseFormMarkedJunk(RE::TESForm* form) const;

        /** @deprecated Prefer IsBaseFormMarkedJunk — same behavior: base form only, hash 0. */
        bool IsJunk(RE::TESForm* form) const { return IsBaseFormMarkedJunk(form); }

        [[nodiscard]] bool IsJunkKey(uint64_t packedKey) const;

        void Clear();
        size_t Size() const;

        std::vector<JunkItem> GetAllJunkItems() const;
        JunkItem GetJunkItemAt(int32_t index) const;
        bool RemoveJunkItemAtIndex(int32_t index);

        std::vector<InventoryJunkEntry> GetPlayerJunkInventory() const;

        /** Stable instance hash (v2) used for new junk entries and UI matching. */
        static uint32_t ComputeExtraDataHash(RE::InventoryEntryData* entry);
        static uint32_t ComputeExtraDataHash(RE::ExtraDataList* extraList);

        /** Previous hash algorithm; co-saves and older JSON lists may still use these values. Runtime matching checks both v2 and legacy. */
        static uint32_t ComputeLegacyExtraDataHash(RE::InventoryEntryData* entry);
        static uint32_t ComputeLegacyExtraDataHash(RE::ExtraDataList* extraList);

        bool SaveToFile();
        bool LoadFromFile(bool replace);

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

        [[nodiscard]] bool InstanceKeysInJunkSet(RE::FormID baseFormID, uint32_t v2Hash, uint32_t legacyHash) const;

        static uint32_t HashExtraListLegacy(const RE::ExtraDataList* extraList);
        static uint32_t HashEntryLegacy(const RE::InventoryEntryData* entry);

        std::unordered_set<uint64_t> junkSet;
        std::vector<JunkItem> junkItems;
        mutable std::mutex lock;

        void RebuildJunkItemsVector();
    };
}
