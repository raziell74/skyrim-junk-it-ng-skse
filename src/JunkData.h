#pragma once

#include <mutex>
#include <unordered_set>
#include <vector>
#include <string>
#include <optional>

namespace JunkIt {

    struct JunkItem {
        std::string identity;
        std::string displayName;

        JunkItem() : identity(""), displayName("") {}
        JunkItem(std::string itemIdentity, std::string name)
            : identity(std::move(itemIdentity)), displayName(std::move(name)) {}
    };

    struct InventoryJunkEntry {
        std::string identity;
        std::string displayName;
        int32_t count;

        InventoryJunkEntry() : identity(""), displayName(""), count(0) {}
        InventoryJunkEntry(std::string itemIdentity, std::string name, int32_t itemCount)
            : identity(std::move(itemIdentity)), displayName(std::move(name)), count(itemCount) {}
    };

    class JunkDataManager {
    public:
        static JunkDataManager& GetSingleton() {
            static JunkDataManager instance;
            return instance;
        }

        std::optional<std::string> AddJunkItem(RE::InventoryEntryData* entry);
        std::optional<std::string> AddJunkIdentity(const std::string& identity, bool autoJunked);
        std::optional<std::string> RemoveJunkItem(RE::InventoryEntryData* entry);
        bool IsJunk(RE::InventoryEntryData* entry) const;
        bool IsAnyJunkForForm(RE::TESForm* form) const;
        bool IsJunk(RE::TESBoundObject* object, const RE::ExtraDataList* extraList, std::string_view displayName) const;
        bool IsJunk(const std::string& identity) const;
        bool IsNoAutoJunk(const std::string& identity) const;
        static std::string BuildIdentityForEntry(RE::InventoryEntryData* entry, const RE::ExtraDataList* extraList);

        void Clear();
        void ClearNoAutoJunk();
        size_t Size() const;
        size_t NoAutoJunkSize() const;

        std::vector<JunkItem> GetAllJunkItems() const;
        std::vector<JunkItem> GetAllNoAutoJunkItems() const;
        JunkItem GetJunkItemAt(int32_t index) const;
        bool RemoveJunkItemAtIndex(int32_t index);
        bool RemoveNoAutoJunkIdentity(const std::string& identity);

        std::vector<InventoryJunkEntry> GetPlayerJunkInventory() const;

        bool SaveToFile();
        bool LoadFromFile(bool replace);

        void Save(SKSE::SerializationInterface* intfc);
        void Load(SKSE::SerializationInterface* intfc, uint32_t recordVersion);
        void SaveAutoJunk(SKSE::SerializationInterface* intfc);
        void LoadAutoJunk(SKSE::SerializationInterface* intfc, uint32_t recordVersion);
        void Revert(SKSE::SerializationInterface* intfc);

        static void OnSave(SKSE::SerializationInterface* intfc);
        static void OnLoad(SKSE::SerializationInterface* intfc);
        static void OnRevert(SKSE::SerializationInterface* intfc);

    private:
        static std::string BuildIdentity(RE::TESBoundObject* object, const RE::ExtraDataList* extraList, std::string_view displayName);
        static std::string GetEnchantmentFormConfig(const RE::ExtraDataList* extraList);
        static bool IsCanonicalIdentity(const std::string& identity);
        static std::string GetDisplayNameFromIdentity(const std::string& identity);
        void ApplyUnmarkLocked(const std::string& identity);
        void PruneAutoJunkedLocked();

        JunkDataManager() = default;
        JunkDataManager(const JunkDataManager&) = delete;
        JunkDataManager& operator=(const JunkDataManager&) = delete;

        std::unordered_set<std::string> junkSet;
        std::unordered_set<std::string> autoJunkedSet;
        std::unordered_set<std::string> noAutoJunkSet;
        std::vector<JunkItem> junkItems;
        mutable std::mutex lock;
    };
}
