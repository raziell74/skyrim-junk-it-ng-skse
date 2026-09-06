#include "JunkData.h"
#include "InventoryWalk.h"
#include "junk.h"
#include "util.h"
#include <json/json.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <unordered_map>

#include "RE/E/ExtraEnchantment.h"

namespace JunkIt {
    namespace {
        constexpr std::uint32_t kJunkRecord = 'JNKT';
        constexpr std::uint32_t kJunkRecordVersion = 3;
        constexpr std::uint32_t kAutoJunkRecord = 'AJNK';
        constexpr std::uint32_t kAutoJunkRecordVersion = 1;
        constexpr std::uint32_t kTrashRecord = 'TRSH';
        constexpr std::uint32_t kTrashRecordVersion = 1;
        constexpr std::uint32_t kJsonJunkVersion = 2;
        constexpr auto kJsonJunkPath = "Data/SKSE/Plugins/JunkIt/junklist.json";
        const std::regex kCanonicalIdentityRegex(
            R"(^(0x[0-9A-Fa-f]+(?:~[^|]+)?)\|([^|]+)\|((?:0x[0-9A-Fa-f]+(?:~[^|]+)?)|none)$)");

        std::vector<std::string> BuildEntryIdentities(RE::InventoryEntryData* entry) {
            std::vector<std::string> identities;
            if (!entry || !entry->object) {
                return identities;
            }
            const auto base = JunkDataManager::CaptureIdentityBase(entry->object, entry->GetDisplayName());
            if (!base) {
                return identities;
            }
            if (!entry->extraLists || entry->extraLists->empty()) {
                identities.push_back(JunkDataManager::BuildIdentity(*base, nullptr));
            } else {
                for (auto* extraList : *entry->extraLists) {
                    identities.push_back(JunkDataManager::BuildIdentity(*base, extraList));
                }
            }
            return identities;
        }
    }

    std::string JunkDataManager::GetEnchantmentFormConfig(const RE::ExtraDataList* extraList) {
        if (!extraList) {
            return "none";
        }

        const auto* extraEnchantment = extraList->GetByType<RE::ExtraEnchantment>();
        if (!extraEnchantment || !extraEnchantment->enchantment) {
            return "none";
        }

        const auto configString = FormUtil::Form::GetFormConfigString(extraEnchantment->enchantment);
        return configString.empty() ? "none" : configString;
    }

    std::optional<JunkDataManager::IdentityBase> JunkDataManager::CaptureIdentityBase(
        RE::TESBoundObject* object,
        std::string_view displayName) {
        if (!object) {
            return std::nullopt;
        }

        IdentityBase base;
        base.formConfig = FormUtil::Form::GetFormConfigString(object->As<RE::TESForm>());
        if (base.formConfig.empty()) {
            return std::nullopt;
        }

        base.displayName = std::string(displayName);
        if (base.displayName.empty()) {
            base.displayName = object->GetName();
        }
        if (base.displayName.empty()) {
            return std::nullopt;
        }

        std::replace(base.displayName.begin(), base.displayName.end(), '|', ':');
        return base;
    }

    std::string JunkDataManager::BuildIdentity(const IdentityBase& base, const RE::ExtraDataList* extraList) {
        if (base.formConfig.empty() || base.displayName.empty()) {
            return "";
        }
        return fmt::format(
            "{}|{}|{}",
            base.formConfig,
            base.displayName,
            GetEnchantmentFormConfig(extraList));
    }

    std::string JunkDataManager::BuildIdentity(RE::TESBoundObject* object, const RE::ExtraDataList* extraList, std::string_view displayName) {
        const auto base = CaptureIdentityBase(object, displayName);
        if (!base) {
            return "";
        }
        return BuildIdentity(*base, extraList);
    }

    bool JunkDataManager::IsCanonicalIdentity(const std::string& identity) {
        return std::regex_match(identity, kCanonicalIdentityRegex);
    }

    std::string JunkDataManager::GetDisplayNameFromIdentity(const std::string& identity) {
        const auto firstPipe = identity.find('|');
        if (firstPipe == std::string::npos) {
            return "";
        }
        const auto secondPipe = identity.find('|', firstPipe + 1);
        if (secondPipe == std::string::npos || secondPipe <= firstPipe + 1) {
            return "";
        }
        return identity.substr(firstPipe + 1, secondPipe - firstPipe - 1);
    }

    std::string JunkDataManager::GetFormConfigFromIdentity(const std::string& identity) {
        const auto firstPipe = identity.find('|');
        if (firstPipe == std::string::npos || firstPipe == 0) {
            return "";
        }
        return identity.substr(0, firstPipe);
    }

    void JunkDataManager::DropFormConfigIfUnusedLocked(const std::string& formConfig) {
        if (formConfig.empty()) {
            return;
        }
        for (const auto& identity : junkSet) {
            if (GetFormConfigFromIdentity(identity) == formConfig) {
                return;
            }
        }
        junkFormConfigs.erase(formConfig);
    }

    std::string JunkDataManager::BuildIdentityForEntry(RE::InventoryEntryData* entry, const RE::ExtraDataList* extraList) {
        if (!entry || !entry->object) {
            return "";
        }
        return BuildIdentity(entry->object, extraList, entry->GetDisplayName());
    }

    bool JunkDataManager::IsJunk(const std::string& identity) const {
        if (identity.empty()) {
            return false;
        }
        std::lock_guard<std::mutex> guard(lock);
        return junkSet.find(identity) != junkSet.end();
    }

    bool JunkDataManager::IsJunk(RE::TESBoundObject* object, const RE::ExtraDataList* extraList, std::string_view displayName) const {
        return IsJunk(BuildIdentity(object, extraList, displayName));
    }

    bool JunkDataManager::IsAnyJunkForForm(RE::TESForm* form) const {
        if (!form) {
            return false;
        }
        const auto formConfig = FormUtil::Form::GetFormConfigString(form);
        if (formConfig.empty()) {
            return false;
        }
        std::lock_guard<std::mutex> guard(lock);
        return junkFormConfigs.contains(formConfig);
    }

    std::optional<std::string> JunkDataManager::AddJunkItem(RE::InventoryEntryData* entry) {
        if (!entry || !entry->object || entry->IsQuestObject()) {
            return std::nullopt;
        }

        const auto identities = BuildEntryIdentities(entry);

        std::optional<std::string> addedIdentity;
        std::lock_guard<std::mutex> guard(lock);
        for (const auto& identity : identities) {
            if (identity.empty()) {
                continue;
            }
            if (junkSet.insert(identity).second) {
                junkItems.emplace_back(identity, GetDisplayNameFromIdentity(identity));
                const auto formConfig = GetFormConfigFromIdentity(identity);
                if (!formConfig.empty()) {
                    junkFormConfigs.insert(formConfig);
                }
                if (!addedIdentity) {
                    addedIdentity = identity;
                }
            }
        }

        return addedIdentity;
    }

    std::optional<std::string> JunkDataManager::AddJunkIdentity(const std::string& identity, bool autoJunked) {
        if (!IsCanonicalIdentity(identity)) {
            SKSE::log::warn("Skipping add for non-canonical identity: {}", identity);
            return std::nullopt;
        }

        std::lock_guard<std::mutex> guard(lock);
        if (!junkSet.insert(identity).second) {
            return std::nullopt;
        }

        junkItems.emplace_back(identity, GetDisplayNameFromIdentity(identity));
        const auto formConfig = GetFormConfigFromIdentity(identity);
        if (!formConfig.empty()) {
            junkFormConfigs.insert(formConfig);
        }
        if (autoJunked) {
            autoJunkedSet.insert(identity);
            noAutoJunkSet.erase(identity);
        }
        return identity;
    }

    void JunkDataManager::ApplyUnmarkLocked(const std::string& identity) {
        if (autoJunkedSet.erase(identity) > 0) {
            noAutoJunkSet.insert(identity);
        }
    }

    void JunkDataManager::PruneAutoJunkedLocked() {
        for (auto it = autoJunkedSet.begin(); it != autoJunkedSet.end();) {
            if (junkSet.contains(*it)) {
                ++it;
            } else {
                it = autoJunkedSet.erase(it);
            }
        }
    }

    std::optional<std::string> JunkDataManager::RemoveJunkItem(RE::InventoryEntryData* entry) {
        if (!entry || !entry->object) {
            return std::nullopt;
        }

        const auto identities = BuildEntryIdentities(entry);

        std::optional<std::string> removedIdentity;
        std::unordered_set<std::string> removedIdentities;
        std::unordered_set<std::string> removedFormConfigs;
        std::lock_guard<std::mutex> guard(lock);
        for (const auto& identity : identities) {
            if (junkSet.erase(identity) > 0) {
                ApplyUnmarkLocked(identity);
                removedIdentities.insert(identity);
                const auto formConfig = GetFormConfigFromIdentity(identity);
                if (!formConfig.empty()) {
                    removedFormConfigs.insert(formConfig);
                }
                if (!removedIdentity) {
                    removedIdentity = identity;
                }
            }
        }
        if (!removedIdentity) {
            return std::nullopt;
        }

        std::erase_if(junkItems, [&](const JunkItem& item) {
            return removedIdentities.contains(item.identity);
        });
        for (const auto& formConfig : removedFormConfigs) {
            DropFormConfigIfUnusedLocked(formConfig);
        }
        return removedIdentity;
    }

    bool JunkDataManager::IsNoAutoJunk(const std::string& identity) const {
        if (identity.empty()) {
            return false;
        }
        std::lock_guard<std::mutex> guard(lock);
        return noAutoJunkSet.contains(identity);
    }

    bool JunkDataManager::IsJunk(RE::InventoryEntryData* entry) const {
        if (!entry || !entry->object || entry->IsQuestObject()) {
            return false;
        }

        if (!IsAnyJunkForForm(entry->object)) {
            return false;
        }

        const auto identities = BuildEntryIdentities(entry);
        std::lock_guard<std::mutex> guard(lock);
        for (const auto& identity : identities) {
            if (!identity.empty() && junkSet.find(identity) != junkSet.end()) {
                return true;
            }
        }
        return false;
    }

    void JunkDataManager::Clear() {
        std::lock_guard<std::mutex> guard(lock);
        junkSet.clear();
        junkFormConfigs.clear();
        junkItems.clear();
        autoJunkedSet.clear();
    }

    void JunkDataManager::ClearNoAutoJunk() {
        std::lock_guard<std::mutex> guard(lock);
        noAutoJunkSet.clear();
    }

    size_t JunkDataManager::Size() const {
        std::lock_guard<std::mutex> guard(lock);
        return junkItems.size();
    }

    size_t JunkDataManager::NoAutoJunkSize() const {
        std::lock_guard<std::mutex> guard(lock);
        return noAutoJunkSet.size();
    }

    std::vector<JunkItem> JunkDataManager::GetAllJunkItems() const {
        std::lock_guard<std::mutex> guard(lock);
        return junkItems;
    }

    std::vector<JunkItem> JunkDataManager::GetAllNoAutoJunkItems() const {
        std::lock_guard<std::mutex> guard(lock);
        std::vector<JunkItem> result;
        result.reserve(noAutoJunkSet.size());
        for (const auto& identity : noAutoJunkSet) {
            result.emplace_back(identity, GetDisplayNameFromIdentity(identity));
        }
        std::ranges::sort(result, [](const JunkItem& left, const JunkItem& right) {
            return left.displayName < right.displayName;
        });
        return result;
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

        const auto identity = junkItems[index].identity;
        junkItems.erase(junkItems.begin() + index);
        if (junkSet.erase(identity) == 0) {
            return false;
        }
        ApplyUnmarkLocked(identity);
        DropFormConfigIfUnusedLocked(GetFormConfigFromIdentity(identity));
        return true;
    }

    bool JunkDataManager::RemoveNoAutoJunkIdentity(const std::string& identity) {
        if (identity.empty()) {
            return false;
        }
        std::lock_guard<std::mutex> guard(lock);
        return noAutoJunkSet.erase(identity) > 0;
    }

    std::vector<InventoryJunkEntry> JunkDataManager::GetPlayerJunkInventory() const {
        std::vector<InventoryJunkEntry> result;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return result;
        }

        std::unordered_map<std::string, int32_t> counts;
        std::unordered_map<std::string, std::string> names;

        ForEachInventoryEntry(player, [&](RE::InventoryEntryData* entryData) {
            if (!entryData || !entryData->object) {
                return;
            }

            auto* obj = entryData->object;
            if (!entryData->extraLists || entryData->extraLists->empty()) {
                const std::string displayName = entryData->GetDisplayName();
                const auto identity = BuildIdentity(obj, nullptr, displayName);
                if (IsJunk(identity) && entryData->countDelta > 0) {
                    counts[identity] += entryData->countDelta;
                    names.try_emplace(identity, GetDisplayNameFromIdentity(identity));
                }
                return;
            }

            for (auto* extraList : *entryData->extraLists) {
                if (!extraList) {
                    continue;
                }

                const auto identity = BuildIdentityForEntry(entryData, extraList);
                if (!IsJunk(identity)) {
                    continue;
                }

                counts[identity] += extraList->GetCount();
                names.try_emplace(identity, GetDisplayNameFromIdentity(identity));
            }
        });

        result.reserve(counts.size());
        for (const auto& [identity, count] : counts) {
            result.emplace_back(identity, names[identity], count);
        }
        return result;
    }

    bool JunkDataManager::SaveToFile() {
        std::lock_guard<std::mutex> guard(lock);

        Json::Value root;
        root["version"] = kJsonJunkVersion;
        root["items"] = Json::arrayValue;
        for (const auto& item : junkItems) {
            Json::Value itemObj;
            itemObj["identity"] = item.identity;
            itemObj["name"] = item.displayName;
            root["items"].append(itemObj);
        }

        root["exclusions"] = Json::arrayValue;
        for (const auto& identity : noAutoJunkSet) {
            Json::Value itemObj;
            itemObj["identity"] = identity;
            itemObj["name"] = GetDisplayNameFromIdentity(identity);
            root["exclusions"].append(itemObj);
        }

        const std::filesystem::path filePath(kJsonJunkPath);
        std::error_code ec;
        std::filesystem::create_directories(filePath.parent_path(), ec);

        std::ofstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            SKSE::log::error("Failed to open JSON junk list for writing: {}", filePath.string());
            return false;
        }

        Json::StreamWriterBuilder writerBuilder;
        writerBuilder["indentation"] = "  ";
        std::unique_ptr<Json::StreamWriter> writer(writerBuilder.newStreamWriter());
        writer->write(root, &file);
        file.flush();
        if (!file) {
            SKSE::log::error("Failed to write JSON junk list: {}", filePath.string());
            return false;
        }

        SKSE::log::info(
            "Exported {} junk item(s) and {} exclusion(s) to {}",
            junkItems.size(),
            noAutoJunkSet.size(),
            filePath.string());
        return true;
    }

    bool JunkDataManager::LoadFromFile(bool replace) {
        const std::filesystem::path filePath(kJsonJunkPath);
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            SKSE::log::warn("Failed to open JSON junk list for reading: {}", filePath.string());
            return false;
        }

        Json::Value root;
        Json::CharReaderBuilder readerBuilder;
        std::string parseErrors;
        if (!Json::parseFromStream(readerBuilder, file, &root, &parseErrors)) {
            SKSE::log::error("Failed to parse JSON junk list: {}", parseErrors);
            return false;
        }

        if (!root.isObject() || !root.isMember("version") || !root["version"].isInt()) {
            SKSE::log::error("Invalid JSON junk list format (expected version): {}", filePath.string());
            return false;
        }

        const bool hasItems = root.isMember("items");
        const bool hasExclusions = root.isMember("exclusions");
        if (hasItems && !root["items"].isArray()) {
            SKSE::log::error("Invalid JSON junk list format (items must be an array): {}", filePath.string());
            return false;
        }
        if (hasExclusions && !root["exclusions"].isArray()) {
            SKSE::log::error("Invalid JSON junk list format (exclusions must be an array): {}", filePath.string());
            return false;
        }
        if (!hasItems && !hasExclusions) {
            SKSE::log::error(
                "Invalid JSON junk list format (expected items and/or exclusions): {}",
                filePath.string());
            return false;
        }

        auto parseIdentityArray = [&](const Json::Value& arr, std::vector<JunkItem>& out, std::size_t& skipped) {
            for (const auto& itemObj : arr) {
                if (!itemObj.isObject() || !itemObj.isMember("identity") || !itemObj["identity"].isString()) {
                    ++skipped;
                    continue;
                }

                const auto identity = itemObj["identity"].asString();
                if (!IsCanonicalIdentity(identity)) {
                    SKSE::log::warn("Skipping invalid identity from JSON load: {}", identity);
                    ++skipped;
                    continue;
                }

                std::string displayName = GetDisplayNameFromIdentity(identity);
                if (itemObj.isMember("name") && itemObj["name"].isString()) {
                    const auto name = itemObj["name"].asString();
                    if (!name.empty()) {
                        displayName = name;
                    }
                }
                out.emplace_back(identity, displayName);
            }
        };

        std::vector<JunkItem> loadedItems;
        std::vector<JunkItem> loadedExclusions;
        std::size_t skipped = 0;
        if (hasItems) {
            parseIdentityArray(root["items"], loadedItems, skipped);
        }
        if (hasExclusions) {
            parseIdentityArray(root["exclusions"], loadedExclusions, skipped);
        }

        if (loadedItems.empty() && loadedExclusions.empty()) {
            SKSE::log::error(
                "JSON junk list contained no valid identities (skipped {}): {}",
                skipped,
                filePath.string());
            return false;
        }

        std::size_t added = 0;
        std::size_t exclusionsAdded = 0;
        std::lock_guard<std::mutex> guard(lock);
        if (replace) {
            junkSet.clear();
            junkFormConfigs.clear();
            junkItems.clear();
            autoJunkedSet.clear();
            if (hasExclusions) {
                noAutoJunkSet.clear();
            }
        }

        for (const auto& item : loadedItems) {
            if (!junkSet.insert(item.identity).second) {
                ++skipped;
                continue;
            }
            junkItems.push_back(item);
            const auto formConfig = GetFormConfigFromIdentity(item.identity);
            if (!formConfig.empty()) {
                junkFormConfigs.insert(formConfig);
            }
            ++added;
        }

        if (hasExclusions) {
            for (const auto& item : loadedExclusions) {
                if (noAutoJunkSet.insert(item.identity).second) {
                    ++exclusionsAdded;
                } else {
                    ++skipped;
                }
            }
        }

        SKSE::log::info(
            "Imported {} junk item(s) and {} exclusion(s) from {} (skipped {}, replace={})",
            added,
            exclusionsAdded,
            filePath.string(),
            skipped,
            replace);
        return true;
    }

    void JunkDataManager::Save(SKSE::SerializationInterface* intfc) {
        if (!intfc) {
            return;
        }

        std::lock_guard<std::mutex> guard(lock);
        const uint32_t itemCount = static_cast<uint32_t>(junkItems.size());
        if (!intfc->WriteRecordData(&itemCount, sizeof(itemCount))) {
            SKSE::log::error("Failed to write junk item count");
            return;
        }

        for (const auto& item : junkItems) {
            const uint16_t identityLength = static_cast<uint16_t>(item.identity.size());
            const uint16_t nameLength = static_cast<uint16_t>(item.displayName.size());

            if (!intfc->WriteRecordData(&identityLength, sizeof(identityLength))) {
                SKSE::log::error("Failed to write identity length");
                return;
            }
            if (identityLength > 0 && !intfc->WriteRecordData(item.identity.c_str(), identityLength)) {
                SKSE::log::error("Failed to write identity");
                return;
            }
            if (!intfc->WriteRecordData(&nameLength, sizeof(nameLength))) {
                SKSE::log::error("Failed to write display name length");
                return;
            }
            if (nameLength > 0 && !intfc->WriteRecordData(item.displayName.c_str(), nameLength)) {
                SKSE::log::error("Failed to write display name");
                return;
            }
        }
    }

    void JunkDataManager::Load(SKSE::SerializationInterface* intfc, uint32_t recordVersion) {
        if (!intfc) {
            return;
        }

        std::lock_guard<std::mutex> guard(lock);
        junkSet.clear();
        junkFormConfigs.clear();
        junkItems.clear();

        if (recordVersion != kJunkRecordVersion) {
            SKSE::log::warn("Skipping unsupported JNKT record version {} (expected {})", recordVersion, kJunkRecordVersion);
            return;
        }

        uint32_t itemCount = 0;
        if (!intfc->ReadRecordData(&itemCount, sizeof(itemCount))) {
            SKSE::log::error("Failed to read junk item count");
            return;
        }

        for (uint32_t i = 0; i < itemCount; ++i) {
            uint16_t identityLength = 0;
            if (!intfc->ReadRecordData(&identityLength, sizeof(identityLength))) {
                SKSE::log::error("Failed to read identity length at index {}", i);
                return;
            }

            std::string identity(identityLength, '\0');
            if (identityLength > 0 && !intfc->ReadRecordData(identity.data(), identityLength)) {
                SKSE::log::error("Failed to read identity at index {}", i);
                return;
            }

            uint16_t nameLength = 0;
            if (!intfc->ReadRecordData(&nameLength, sizeof(nameLength))) {
                SKSE::log::error("Failed to read display name length at index {}", i);
                return;
            }

            std::string displayName(nameLength, '\0');
            if (nameLength > 0 && !intfc->ReadRecordData(displayName.data(), nameLength)) {
                SKSE::log::error("Failed to read display name at index {}", i);
                return;
            }

            if (!IsCanonicalIdentity(identity)) {
                SKSE::log::warn("Invalid junk identity in co-save, skipping: {}", identity);
                continue;
            }

            if (!junkSet.insert(identity).second) {
                continue;
            }

            if (displayName.empty()) {
                displayName = GetDisplayNameFromIdentity(identity);
            }
            junkItems.emplace_back(identity, displayName);
            const auto formConfig = GetFormConfigFromIdentity(identity);
            if (!formConfig.empty()) {
                junkFormConfigs.insert(formConfig);
            }
        }
    }

    void JunkDataManager::SaveAutoJunk(SKSE::SerializationInterface* intfc) {
        if (!intfc) {
            return;
        }

        auto writeSet = [&](const std::unordered_set<std::string>& identities) {
            const uint32_t count = static_cast<uint32_t>(identities.size());
            if (!intfc->WriteRecordData(&count, sizeof(count))) {
                SKSE::log::error("Failed to write auto-junk identity count");
                return false;
            }
            for (const auto& identity : identities) {
                const uint16_t identityLength = static_cast<uint16_t>(identity.size());
                if (!intfc->WriteRecordData(&identityLength, sizeof(identityLength))) {
                    SKSE::log::error("Failed to write auto-junk identity length");
                    return false;
                }
                if (identityLength > 0 && !intfc->WriteRecordData(identity.c_str(), identityLength)) {
                    SKSE::log::error("Failed to write auto-junk identity");
                    return false;
                }
            }
            return true;
        };

        std::lock_guard<std::mutex> guard(lock);
        if (!writeSet(autoJunkedSet) || !writeSet(noAutoJunkSet)) {
            return;
        }
    }

    void JunkDataManager::LoadAutoJunk(SKSE::SerializationInterface* intfc, uint32_t recordVersion) {
        if (!intfc) {
            return;
        }

        std::lock_guard<std::mutex> guard(lock);
        autoJunkedSet.clear();
        noAutoJunkSet.clear();

        if (recordVersion != kAutoJunkRecordVersion) {
            SKSE::log::warn(
                "Skipping unsupported AJNK record version {} (expected {})",
                recordVersion,
                kAutoJunkRecordVersion);
            return;
        }

        auto readSet = [&](std::unordered_set<std::string>& out, const char* label) {
            uint32_t count = 0;
            if (!intfc->ReadRecordData(&count, sizeof(count))) {
                SKSE::log::error("Failed to read {} identity count", label);
                return false;
            }
            for (uint32_t i = 0; i < count; ++i) {
                uint16_t identityLength = 0;
                if (!intfc->ReadRecordData(&identityLength, sizeof(identityLength))) {
                    SKSE::log::error("Failed to read {} identity length at index {}", label, i);
                    return false;
                }

                std::string identity(identityLength, '\0');
                if (identityLength > 0 && !intfc->ReadRecordData(identity.data(), identityLength)) {
                    SKSE::log::error("Failed to read {} identity at index {}", label, i);
                    return false;
                }

                if (!IsCanonicalIdentity(identity)) {
                    SKSE::log::warn("Invalid {} identity in co-save, skipping: {}", label, identity);
                    continue;
                }
                out.insert(std::move(identity));
            }
            return true;
        };

        if (!readSet(autoJunkedSet, "auto-junked") || !readSet(noAutoJunkSet, "no-auto-junk")) {
            autoJunkedSet.clear();
            noAutoJunkSet.clear();
        }
    }

    void JunkDataManager::Revert(SKSE::SerializationInterface*) {
        std::lock_guard<std::mutex> guard(lock);
        junkSet.clear();
        junkFormConfigs.clear();
        junkItems.clear();
        autoJunkedSet.clear();
        noAutoJunkSet.clear();
    }

    void JunkDataManager::OnSave(SKSE::SerializationInterface* intfc) {
        if (!intfc) {
            return;
        }
        if (!intfc->OpenRecord(kJunkRecord, kJunkRecordVersion)) {
            SKSE::log::error("Failed to open JNKT record for save");
            return;
        }
        GetSingleton().Save(intfc);

        if (!intfc->OpenRecord(kAutoJunkRecord, kAutoJunkRecordVersion)) {
            SKSE::log::error("Failed to open AJNK record for save");
            return;
        }
        GetSingleton().SaveAutoJunk(intfc);

        if (!intfc->OpenRecord(kTrashRecord, kTrashRecordVersion)) {
            SKSE::log::error("Failed to open TRSH record for save");
            return;
        }
        JunkHandler::SaveTrashState(intfc);
    }

    void JunkDataManager::OnLoad(SKSE::SerializationInterface* intfc) {
        if (!intfc) {
            return;
        }

        {
            std::lock_guard<std::mutex> guard(GetSingleton().lock);
            GetSingleton().autoJunkedSet.clear();
            GetSingleton().noAutoJunkSet.clear();
        }

        uint32_t type = 0;
        uint32_t version = 0;
        uint32_t length = 0;
        while (intfc->GetNextRecordInfo(type, version, length)) {
            if (type == kJunkRecord) {
                GetSingleton().Load(intfc, version);
            } else if (type == kAutoJunkRecord) {
                GetSingleton().LoadAutoJunk(intfc, version);
            } else if (type == kTrashRecord) {
                JunkHandler::LoadTrashState(intfc, version);
            }
        }
        std::lock_guard<std::mutex> guard(GetSingleton().lock);
        GetSingleton().PruneAutoJunkedLocked();
    }

    void JunkDataManager::OnRevert(SKSE::SerializationInterface* intfc) {
        GetSingleton().Revert(intfc);
        JunkHandler::RevertTrashState();
    }
}
