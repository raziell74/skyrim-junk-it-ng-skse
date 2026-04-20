#include "JunkData.h"
#include "util.h"
#include <json/json.h>
#include <fstream>
#include <filesystem>
#include <cmath>

#include "RE/E/ExtraCharge.h"
#include "RE/E/ExtraEnchantment.h"
#include "RE/E/ExtraHealth.h"
#include "RE/E/ExtraHotkey.h"
#include "RE/E/ExtraOwnership.h"
#include "RE/E/ExtraPoison.h"
#include "RE/E/ExtraSoul.h"
#include "RE/E/ExtraTextDisplayData.h"
#include "RE/E/ExtraUniqueID.h"
#include "RE/E/ExtraWorn.h"
#include "RE/E/ExtraWornLeft.h"

namespace JunkIt {

    namespace {

        constexpr uint32_t kFnvOffsetBasis = 2166136261u;
        constexpr uint32_t kFnvPrime = 16777619u;

        uint32_t MixU32(uint32_t h, uint32_t v) noexcept {
            h ^= v;
            h *= kFnvPrime;
            return h;
        }

        uint32_t QuantizeFloat(float a_value) noexcept {
            return static_cast<uint32_t>(std::lround(static_cast<double>(a_value) * 1000.0));
        }

        uint32_t HashExtraListV2(const RE::ExtraDataList* extraList) {
            if (!extraList) {
                return 0;
            }

            uint32_t h = kFnvOffsetBasis;

            if (auto* enchantment = extraList->GetByType<RE::ExtraEnchantment>()) {
                if (enchantment->enchantment) {
                    h = MixU32(h, 0xE001u);
                    h = MixU32(h, static_cast<uint32_t>(enchantment->enchantment->GetFormID()));
                }
            }

            if (auto* health = extraList->GetByType<RE::ExtraHealth>()) {
                h = MixU32(h, 0xE002u);
                h = MixU32(h, QuantizeFloat(health->health));
            }

            if (auto* textDisplay = extraList->GetByType<RE::ExtraTextDisplayData>()) {
                if (textDisplay->customNameLength > 0) {
                    h = MixU32(h, 0xE003u);
                    const std::string name(textDisplay->displayName.c_str());
                    uint32_t nameHash = kFnvOffsetBasis;
                    for (unsigned char c : name) {
                        nameHash = MixU32(nameHash, static_cast<uint32_t>(c));
                    }
                    h = MixU32(h, nameHash);
                }
            }

            if (auto* ownership = extraList->GetByType<RE::ExtraOwnership>()) {
                if (ownership->owner) {
                    h = MixU32(h, 0xE004u);
                    h = MixU32(h, static_cast<uint32_t>(ownership->owner->GetFormID()));
                }
            }

            if (auto* charge = extraList->GetByType<RE::ExtraCharge>()) {
                h = MixU32(h, 0xE005u);
                h = MixU32(h, QuantizeFloat(charge->charge));
            }

            if (auto* poison = extraList->GetByType<RE::ExtraPoison>()) {
                h = MixU32(h, 0xE006u);
                if (poison->poison) {
                    h = MixU32(h, static_cast<uint32_t>(poison->poison->GetFormID()));
                }
                h = MixU32(h, static_cast<uint32_t>(poison->count));
            }

            if (auto* soul = extraList->GetByType<RE::ExtraSoul>()) {
                h = MixU32(h, 0xE007u);
                h = MixU32(h, static_cast<uint32_t>(soul->GetContainedSoul()));
            }

            if (auto* extraHotkey = extraList->GetByType<RE::ExtraHotkey>()) {
                h = MixU32(h, 0xE008u);
                h = MixU32(h, static_cast<uint32_t>(static_cast<std::uint8_t>(extraHotkey->hotkey.underlying())));
            }

            if (auto* unique = extraList->GetByType<RE::ExtraUniqueID>()) {
                h = MixU32(h, 0xE009u);
                h = MixU32(h, static_cast<uint32_t>(unique->baseID));
                h = MixU32(h, static_cast<uint32_t>(unique->uniqueID));
            }

            if (extraList->HasType<RE::ExtraWorn>()) {
                h = MixU32(h, 0xE00Au);
            }
            if (extraList->HasType<RE::ExtraWornLeft>()) {
                h = MixU32(h, 0xE00Bu);
            }

            return h;
        }

        uint32_t HashEntryV2(const RE::InventoryEntryData* entry) {
            if (!entry || !entry->extraLists || entry->extraLists->empty()) {
                return 0;
            }

            uint32_t hash = 0;
            for (auto* extraList : *entry->extraLists) {
                if (extraList) {
                    hash ^= HashExtraListV2(extraList);
                }
            }
            return hash;
        }
    }  // namespace

    uint32_t JunkDataManager::HashExtraListLegacy(const RE::ExtraDataList* extraList) {
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
            if (textDisplay->customNameLength > 0) {
                std::string name(textDisplay->displayName.c_str());
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

    uint32_t JunkDataManager::HashEntryLegacy(const RE::InventoryEntryData* entry) {
        if (!entry || !entry->extraLists || entry->extraLists->empty()) {
            return 0;
        }

        uint32_t hash = 0;
        for (auto* extraList : *entry->extraLists) {
            if (extraList) {
                hash ^= HashExtraListLegacy(extraList);
            }
        }

        return hash;
    }

    uint32_t JunkDataManager::ComputeExtraDataHash(RE::InventoryEntryData* entry) {
        return HashEntryV2(entry);
    }

    uint32_t JunkDataManager::ComputeExtraDataHash(RE::ExtraDataList* extraList) {
        return HashExtraListV2(extraList);
    }

    uint32_t JunkDataManager::ComputeLegacyExtraDataHash(RE::InventoryEntryData* entry) {
        return HashEntryLegacy(entry);
    }

    uint32_t JunkDataManager::ComputeLegacyExtraDataHash(RE::ExtraDataList* extraList) {
        return HashExtraListLegacy(extraList);
    }

    bool JunkDataManager::InstanceKeysInJunkSet(RE::FormID baseFormID, uint32_t v2Hash, uint32_t legacyHash) const {
        const uint64_t k2 = JunkItem::PackJunkKey(baseFormID, v2Hash);
        const uint64_t k1 = JunkItem::PackJunkKey(baseFormID, legacyHash);
        return junkSet.find(k2) != junkSet.end() || junkSet.find(k1) != junkSet.end();
    }

    bool JunkDataManager::IsJunkKey(uint64_t packedKey) const {
        std::lock_guard<std::mutex> guard(lock);
        return junkSet.find(packedKey) != junkSet.end();
    }

    bool JunkDataManager::AddJunkItem(RE::InventoryEntryData* entry) {
        if (!entry || !entry->object) {
            return false;
        }

        if (entry->IsQuestObject()) {
            return false;
        }

        RE::FormID baseFormID = entry->object->GetFormID();

        std::lock_guard<std::mutex> guard(lock);

        const uint32_t extraHash = HashEntryV2(entry);
        const uint64_t packedKey = JunkItem::PackJunkKey(baseFormID, extraHash);

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

        const uint32_t v2 = HashEntryV2(entry);
        const uint32_t leg = HashEntryLegacy(entry);
        const uint64_t pkV2 = JunkItem::PackJunkKey(baseFormID, v2);
        const uint64_t pkLeg = JunkItem::PackJunkKey(baseFormID, leg);

        if (junkSet.erase(pkV2) != 0u) {
            RebuildJunkItemsVector();
            SKSE::log::info("Removed junk item: {} [FormID: 0x{:X}, Hash: 0x{:X}]",
                entry->GetDisplayName(), baseFormID, v2);
            return true;
        }
        if (junkSet.erase(pkLeg) != 0u) {
            RebuildJunkItemsVector();
            SKSE::log::info("Removed junk item (legacy hash): {} [FormID: 0x{:X}, Hash: 0x{:X}]",
                entry->GetDisplayName(), baseFormID, leg);
            return true;
        }

        return false;
    }

    bool JunkDataManager::IsJunk(RE::InventoryEntryData* entry) const {
        if (!entry || !entry->object) {
            return false;
        }

        if (entry->IsQuestObject()) {
            return false;
        }

        RE::FormID baseFormID = entry->object->GetFormID();

        std::lock_guard<std::mutex> guard(lock);

        const uint32_t v2 = HashEntryV2(entry);
        const uint32_t leg = HashEntryLegacy(entry);
        return InstanceKeysInJunkSet(baseFormID, v2, leg);
    }

    bool JunkDataManager::IsJunk(RE::FormID baseFormID, uint32_t extraDataHash) const {
        std::lock_guard<std::mutex> guard(lock);
        const uint64_t packedKey = JunkItem::PackJunkKey(baseFormID, extraDataHash);
        return junkSet.find(packedKey) != junkSet.end();
    }

    bool JunkDataManager::IsBaseFormMarkedJunk(RE::TESForm* form) const {
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

    std::vector<InventoryJunkEntry> JunkDataManager::GetPlayerJunkInventory() const {
        std::vector<InventoryJunkEntry> result;

        auto player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return result;
        }

        auto playerInventory = player->GetInventory();

        std::lock_guard<std::mutex> guard(lock);

        std::map<uint64_t, int32_t> packedKeyCounts;
        std::map<uint64_t, std::string> packedKeyNames;

        auto addIfJunk = [&](RE::FormID baseFormID, uint32_t v2Hash, uint32_t legacyHash, int32_t itemCount,
                             const std::string& displayName) {
            if (itemCount <= 0) {
                return;
            }
            const uint64_t pkV2 = JunkItem::PackJunkKey(baseFormID, v2Hash);
            const uint64_t pkLeg = JunkItem::PackJunkKey(baseFormID, legacyHash);
            uint64_t keyUsed = 0;
            if (junkSet.find(pkV2) != junkSet.end()) {
                keyUsed = pkV2;
            } else if (junkSet.find(pkLeg) != junkSet.end()) {
                keyUsed = pkLeg;
            } else {
                return;
            }
            packedKeyCounts[keyUsed] += itemCount;
            if (packedKeyNames.find(keyUsed) == packedKeyNames.end()) {
                packedKeyNames[keyUsed] = displayName;
            }
        };

        for (const auto& [obj, data] : playerInventory) {
            if (!obj || data.first <= 0) {
                continue;
            }

            RE::FormID baseFormID = obj->GetFormID();
            auto* entryData = data.second.get();

            if (entryData && entryData->extraLists && !entryData->extraLists->empty()) {
                for (auto* extraList : *entryData->extraLists) {
                    if (!extraList) {
                        continue;
                    }
                    const uint32_t v2 = HashExtraListV2(extraList);
                    const uint32_t leg = HashExtraListLegacy(extraList);
                    addIfJunk(baseFormID, v2, leg, extraList->GetCount(), entryData->GetDisplayName());
                }
            }

            if (junkSet.find(JunkItem::PackJunkKey(baseFormID, 0)) != junkSet.end()) {
                int32_t baseCount = data.first;

                if (entryData && entryData->extraLists && !entryData->extraLists->empty()) {
                    for (auto* extraList : *entryData->extraLists) {
                        if (extraList) {
                            baseCount -= extraList->GetCount();
                        }
                    }
                }

                if (baseCount > 0) {
                    const uint64_t pk = JunkItem::PackJunkKey(baseFormID, 0);
                    packedKeyCounts[pk] += baseCount;
                    if (packedKeyNames.find(pk) == packedKeyNames.end()) {
                        packedKeyNames[pk] = obj->GetName();
                    }
                }
            }
        }

        for (const auto& [packedKey, count] : packedKeyCounts) {
            RE::FormID baseFormID = static_cast<RE::FormID>(packedKey >> 32);
            uint32_t extraHash = static_cast<uint32_t>(packedKey & 0xFFFFFFFF);
            std::string displayName = packedKeyNames[packedKey];

            result.emplace_back(baseFormID, extraHash, displayName, count);
        }

        return result;
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

        SKSE::log::info(" ");
        SKSE::log::info("==== Saving Junk List to Co-Save ====");
        SKSE::log::info("Total items to save: {}", itemCount);
        SKSE::log::info(" ");

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

            SKSE::log::info("  Saving: {} [FormID: 0x{:X}, Hash: 0x{:X}]",
                item.displayName, item.baseFormID, item.extraDataHash);
        }

        SKSE::log::info(" ");
        SKSE::log::info("Successfully saved {} junk items to co-save", itemCount);
        SKSE::log::info("==== Junk List Save Complete ====");
        SKSE::log::info(" ");
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

        SKSE::log::info(" ");
        SKSE::log::info("==== Loading Junk List from Save ====");
        SKSE::log::info("Total items to load: {}", itemCount);
        SKSE::log::info(" ");

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
                SKSE::log::warn("Failed to resolve FormID 0x{:X} for '{}', skipping", baseFormID, displayName);
                continue;
            }

            uint64_t packedKey = JunkItem::PackJunkKey(baseFormID, extraDataHash);
            junkSet.insert(packedKey);
            junkItems.emplace_back(baseFormID, extraDataHash, displayName);

            SKSE::log::info("  [{}] {} [FormID: 0x{:X}, Hash: 0x{:X}]",
                i + 1, displayName, baseFormID, extraDataHash);
        }

        SKSE::log::info(" ");
        SKSE::log::info("Successfully loaded {} junk items from save", junkItems.size());
        SKSE::log::info("==== Junk List Load Complete ====");
        SKSE::log::info(" ");
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

        while (intfc->GetNextRecordInfo(type, version, length)) {
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

    bool JunkDataManager::SaveToFile() {
        std::lock_guard<std::mutex> guard(lock);

        SKSE::log::info(" ");
        SKSE::log::info("==== Saving Junk List to File ====");
        SKSE::log::info("Total items to export: {}", junkItems.size());

        Json::Value root;
        root["version"] = 2;
        Json::Value itemsArray(Json::arrayValue);

        for (const auto& item : junkItems) {
            auto* form = RE::TESForm::LookupByID(item.baseFormID);
            if (!form) {
                SKSE::log::warn("Failed to lookup form for 0x{:X}, skipping", item.baseFormID);
                continue;
            }

            std::string formIdStr = FormUtil::Form::GetFormConfigString(form);

            Json::Value itemObj;
            itemObj["formId"] = formIdStr;
            itemObj["extraDataHash"] = item.extraDataHash;
            itemObj["name"] = item.displayName;

            itemsArray.append(itemObj);

            SKSE::log::info("  Exporting: {} [{}] (hash: 0x{:X})",
                item.displayName, formIdStr, item.extraDataHash);
        }

        root["items"] = itemsArray;

        std::filesystem::path filePath = "Data/SKSE/Plugins/JunkIt";
        try {
            if (!std::filesystem::exists(filePath)) {
                std::filesystem::create_directories(filePath);
            }

            filePath /= "JunkIt_JunkList.json";

            std::ofstream outFile(filePath);
            if (!outFile.is_open()) {
                SKSE::log::error("Failed to open file for writing: {}", filePath.string());
                return false;
            }

            Json::StreamWriterBuilder builder;
            builder["indentation"] = "  ";
            std::unique_ptr<Json::StreamWriter> writer(builder.newStreamWriter());
            writer->write(root, &outFile);
            outFile.close();

            SKSE::log::info("Successfully exported {} junk items to {}", itemsArray.size(), filePath.string());
            SKSE::log::info("==== Junk List File Export Complete ====");
            SKSE::log::info(" ");
            return true;

        } catch (const std::exception& e) {
            SKSE::log::error("Exception while saving junk list: {}", e.what());
            return false;
        }
    }

    bool JunkDataManager::LoadFromFile(bool replace) {
        SKSE::log::info(" ");
        SKSE::log::info("==== Loading Junk List from File ====");
        SKSE::log::info("Replace mode: {}", replace);

        RE::BSResourceNiBinaryStream fileStream{ "SKSE/Plugins/JunkIt/JunkIt_JunkList.json" };
        if (!fileStream.good()) {
            SKSE::log::warn("Could not open junk list file: SKSE/Plugins/JunkIt/JunkIt_JunkList.json");
            return false;
        }

        auto size = fileStream.stream->totalSize;
        auto buffer = std::make_unique<char[]>(size);
        fileStream.read(buffer.get(), size);

        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader{ builder.newCharReader() };

        Json::Value root;
        std::string errs;
        if (!reader->parse(buffer.get(), buffer.get() + size, &root, &errs)) {
            SKSE::log::error("Failed to parse junk list JSON: {}", errs);
            return false;
        }

        if (!root.isMember("items") || !root["items"].isArray()) {
            SKSE::log::error("Invalid junk list format: missing or invalid 'items' array");
            return false;
        }

        std::lock_guard<std::mutex> guard(lock);

        if (replace) {
            SKSE::log::info("Clearing existing junk list (replace mode)");
            junkSet.clear();
            junkItems.clear();
        }

        const Json::Value& itemsArray = root["items"];
        uint32_t successCount = 0;
        uint32_t skipCount = 0;

        for (const auto& itemObj : itemsArray) {
            if (!itemObj.isMember("formId") || !itemObj.isMember("extraDataHash") || !itemObj.isMember("name")) {
                SKSE::log::warn("Skipping item with missing fields");
                skipCount++;
                continue;
            }

            std::string formIdStr = itemObj["formId"].asString();
            uint32_t extraDataHash = itemObj["extraDataHash"].asUInt();
            std::string displayName = itemObj["name"].asString();

            RE::TESForm* form = nullptr;

            if (formIdStr.find('~') != std::string::npos) {
                form = FormUtil::Form::GetFormFromConfigString(formIdStr);
            } else {
                try {
                    uint32_t formID = std::stoul(formIdStr, nullptr, 16);
                    form = RE::TESForm::LookupByID(formID);
                } catch (...) {
                    SKSE::log::warn("Failed to parse bare hex FormID: {}", formIdStr);
                }
            }

            if (!form) {
                SKSE::log::warn("Failed to resolve form '{}' for '{}', skipping", formIdStr, displayName);
                skipCount++;
                continue;
            }

            RE::FormID baseFormID = form->GetFormID();
            uint64_t packedKey = JunkItem::PackJunkKey(baseFormID, extraDataHash);

            if (junkSet.find(packedKey) != junkSet.end()) {
                SKSE::log::info("  Item already in junk list, skipping: {} [{}]", displayName, formIdStr);
                skipCount++;
                continue;
            }

            junkSet.insert(packedKey);
            junkItems.emplace_back(baseFormID, extraDataHash, displayName);
            successCount++;

            SKSE::log::info("  Imported: {} [{}] (hash: 0x{:X})", displayName, formIdStr, extraDataHash);
        }

        SKSE::log::info("Import complete: {} items added, {} items skipped", successCount, skipCount);
        SKSE::log::info("Total junk items after import: {}", junkItems.size());
        SKSE::log::info("==== Junk List File Import Complete ====");
        SKSE::log::info(" ");

        return successCount > 0;
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
            uint64_t packedKey = JunkItem::PackJunkKey(baseFormID, extraHash);

            if (junkSet.find(packedKey) == junkSet.end()) {
                junkSet.insert(packedKey);
                junkItems.emplace_back(baseFormID, extraHash, form->GetName());
                SKSE::log::info("Migrated: {} [0x{:X}]", form->GetName(), baseFormID);
            }
        }

        SKSE::log::info("Migration complete. Total junk items: {}", junkItems.size());
    }
}
