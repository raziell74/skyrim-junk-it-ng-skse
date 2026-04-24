#include "JunkData.h"
#include "util.h"
#include <json/json.h>
#include <filesystem>
#include <fstream>
#include <regex>
#include <unordered_map>

#include "RE/E/ExtraEnchantment.h"
#include "RE/E/ExtraUniqueID.h"

namespace JunkIt {
    namespace {
        constexpr std::uint32_t kJunkRecord = 'JNKT';
        constexpr std::uint32_t kJunkRecordVersion = 3;
        constexpr auto kJsonJunkPath = "Data/SKSE/Plugins/JunkIt/junklist.json";
        const std::regex kCanonicalIdentityRegex(
            R"(^(0x[0-9A-Fa-f]+(?:~[^|]+)?)\|([^|]+)\|((?:0x[0-9A-Fa-f]+(?:~[^|]+)?)|none)\|([0-9]+|none)$)");
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

    std::string JunkDataManager::GetUniqueIdField(const RE::ExtraDataList* extraList) {
        if (!extraList) {
            return "none";
        }

        const auto* unique = extraList->GetByType<RE::ExtraUniqueID>();
        if (!unique || unique->uniqueID == 0) {
            return "none";
        }

        return std::to_string(unique->uniqueID);
    }

    std::string JunkDataManager::BuildIdentity(RE::TESBoundObject* object, const RE::ExtraDataList* extraList, std::string_view displayName) {
        if (!object) {
            return "";
        }

        const auto formConfig = FormUtil::Form::GetFormConfigString(object->As<RE::TESForm>());
        if (formConfig.empty()) {
            return "";
        }

        std::string uiDisplayName(displayName);
        if (uiDisplayName.empty()) {
            uiDisplayName = object->GetName();
        }
        if (uiDisplayName.empty()) {
            return "";
        }

        return fmt::format(
            "{}|{}|{}|{}",
            formConfig,
            uiDisplayName,
            GetEnchantmentFormConfig(extraList),
            GetUniqueIdField(extraList));
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
        const std::string prefix = formConfig + "|";
        std::lock_guard<std::mutex> guard(lock);
        for (const auto& identity : junkSet) {
            if (identity.rfind(prefix, 0) == 0) {
                return true;
            }
        }
        return false;
    }

    std::optional<std::string> JunkDataManager::AddJunkItem(RE::InventoryEntryData* entry) {
        if (!entry || !entry->object || entry->IsQuestObject()) {
            return std::nullopt;
        }

        std::vector<std::string> identities;
        if (!entry->extraLists || entry->extraLists->empty()) {
            identities.push_back(BuildIdentityForEntry(entry, nullptr));
        } else {
            for (auto* extraList : *entry->extraLists) {
                identities.push_back(BuildIdentityForEntry(entry, extraList));
            }
        }

        std::optional<std::string> addedIdentity;
        std::lock_guard<std::mutex> guard(lock);
        for (const auto& identity : identities) {
            if (!IsCanonicalIdentity(identity)) {
                SKSE::log::warn("Skipping add for non-canonical identity: {}", identity);
                continue;
            }
            if (junkSet.insert(identity).second) {
                junkItems.emplace_back(identity, GetDisplayNameFromIdentity(identity));
                if (!addedIdentity) {
                    addedIdentity = identity;
                }
            }
        }

        return addedIdentity;
    }

    std::optional<std::string> JunkDataManager::RemoveJunkItem(RE::InventoryEntryData* entry) {
        if (!entry || !entry->object) {
            return std::nullopt;
        }

        std::vector<std::string> identities;
        if (!entry->extraLists || entry->extraLists->empty()) {
            identities.push_back(BuildIdentityForEntry(entry, nullptr));
        } else {
            for (auto* extraList : *entry->extraLists) {
                identities.push_back(BuildIdentityForEntry(entry, extraList));
            }
        }

        std::optional<std::string> removedIdentity;
        std::lock_guard<std::mutex> guard(lock);
        for (const auto& identity : identities) {
            if (junkSet.erase(identity) > 0 && !removedIdentity) {
                removedIdentity = identity;
            }
        }
        if (!removedIdentity) {
            return std::nullopt;
        }

        junkItems.clear();
        junkItems.reserve(junkSet.size());
        for (const auto& identity : junkSet) {
            junkItems.emplace_back(identity, GetDisplayNameFromIdentity(identity));
        }
        return removedIdentity;
    }

    bool JunkDataManager::IsJunk(RE::InventoryEntryData* entry) const {
        if (!entry || !entry->object || entry->IsQuestObject()) {
            return false;
        }

        if (!entry->extraLists || entry->extraLists->empty()) {
            return IsJunk(BuildIdentityForEntry(entry, nullptr));
        }

        for (auto* extraList : *entry->extraLists) {
            if (IsJunk(BuildIdentityForEntry(entry, extraList))) {
                return true;
            }
        }
        return false;
    }

    void JunkDataManager::Clear() {
        std::lock_guard<std::mutex> guard(lock);
        junkSet.clear();
        junkItems.clear();
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

        const auto identity = junkItems[index].identity;
        junkItems.erase(junkItems.begin() + index);
        return junkSet.erase(identity) > 0;
    }

    std::vector<InventoryJunkEntry> JunkDataManager::GetPlayerJunkInventory() const {
        std::vector<InventoryJunkEntry> result;
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return result;
        }

        auto playerInventory = player->GetInventory();
        std::unordered_map<std::string, int32_t> counts;
        std::unordered_map<std::string, std::string> names;

        for (const auto& [obj, data] : playerInventory) {
            if (!obj || data.first <= 0) {
                continue;
            }

            auto* entryData = data.second.get();
            if (!entryData || !entryData->extraLists || entryData->extraLists->empty()) {
                const std::string displayName = entryData ? entryData->GetDisplayName() : obj->GetName();
                const auto identity = BuildIdentity(obj, nullptr, displayName);
                if (IsJunk(identity)) {
                    counts[identity] += data.first;
                    names.try_emplace(identity, GetDisplayNameFromIdentity(identity));
                }
                continue;
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
        }

        result.reserve(counts.size());
        for (const auto& [identity, count] : counts) {
            result.emplace_back(identity, names[identity], count);
        }
        return result;
    }

    bool JunkDataManager::SaveToFile() {
        std::lock_guard<std::mutex> guard(lock);

        Json::Value root;
        root["version"] = 1;
        root["items"] = Json::arrayValue;
        for (const auto& item : junkItems) {
            Json::Value itemObj;
            itemObj["identity"] = item.identity;
            itemObj["name"] = item.displayName;
            root["items"].append(itemObj);
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
        return true;
    }

    bool JunkDataManager::LoadFromFile(bool replace) {
        const std::filesystem::path filePath(kJsonJunkPath);
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        Json::Value root;
        Json::CharReaderBuilder readerBuilder;
        std::string parseErrors;
        if (!Json::parseFromStream(readerBuilder, file, &root, &parseErrors)) {
            SKSE::log::error("Failed to parse JSON junk list: {}", parseErrors);
            return false;
        }

        if (!root.isObject() || !root["items"].isArray()) {
            SKSE::log::error("Invalid JSON junk list format");
            return false;
        }

        std::lock_guard<std::mutex> guard(lock);
        if (replace) {
            junkSet.clear();
            junkItems.clear();
        }

        for (const auto& itemObj : root["items"]) {
            if (!itemObj.isObject() || !itemObj.isMember("identity")) {
                continue;
            }

            const auto identity = itemObj["identity"].asString();
            if (!IsCanonicalIdentity(identity)) {
                SKSE::log::warn("Skipping invalid identity from JSON load: {}", identity);
                continue;
            }

            if (!junkSet.insert(identity).second) {
                continue;
            }

            std::string displayName = GetDisplayNameFromIdentity(identity);
            if (itemObj.isMember("name")) {
                const auto name = itemObj["name"].asString();
                if (!name.empty()) {
                    displayName = name;
                }
            }
            junkItems.emplace_back(identity, displayName);
        }

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
        }
    }

    void JunkDataManager::Revert(SKSE::SerializationInterface*) {
        Clear();
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
    }

    void JunkDataManager::OnLoad(SKSE::SerializationInterface* intfc) {
        if (!intfc) {
            return;
        }

        uint32_t type = 0;
        uint32_t version = 0;
        uint32_t length = 0;
        while (intfc->GetNextRecordInfo(type, version, length)) {
            if (type == kJunkRecord) {
                GetSingleton().Load(intfc, version);
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
        for (auto* form : oldJunkList->forms) {
            auto* object = form ? form->As<RE::TESBoundObject>() : nullptr;
            if (!object) {
                continue;
            }

            const auto identity = BuildIdentity(object, nullptr, object->GetName());
            if (!IsCanonicalIdentity(identity)) {
                continue;
            }
            if (!junkSet.insert(identity).second) {
                continue;
            }
            junkItems.emplace_back(identity, object->GetName());
        }
    }
}
