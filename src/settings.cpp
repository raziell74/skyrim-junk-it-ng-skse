#include "settings.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <system_error>
#include <vector>

namespace JunkIt {
    namespace {
        constexpr auto kIniPath = "Data/SKSE/Plugins/JunkIt.ini";
        constexpr auto kMcmIniPath = "Data/MCM/Settings/JunkIt.ini";

        struct Values {
            std::uint32_t markJunkKey = 50;
            std::uint32_t transferJunkKey = 49;
            std::uint32_t gamepadJunkKey = 270;
            std::uint32_t trashJunkKey = 48;
            std::int32_t gamepadTransferHoldTime = 2;
            bool enableTrash = true;
            std::int32_t trashHoldSeconds = 5;
            std::int32_t gamepadTrashHoldSeconds = 5;
            std::int32_t trashExpireDays = 7;

            bool confirmTransfer = true;
            bool confirmSell = true;
            std::int32_t transferPriority = 0;
            std::int32_t sellPriority = 4;

            bool protectEquipped = true;
            bool protectFavorites = true;
            bool protectEnchanted = false;

            bool notifyOnMarkUnmark = true;
            bool notifyOnJunkTransfer = true;
            bool notifyOnJunkSell = true;
            std::int32_t overlayOpacity = 50;
            std::int32_t logLevel = 2;
            float heavyLoadDelayMultiplier = 1.0f;
            std::int32_t largeUniqueTypes = 500;
            std::int32_t largeTotalItems = 1000;
            std::int32_t sellChunkSize = 250;

            bool updateItemIcon = true;
            bool updateSubTypeDisplay = true;
            bool useDynamicInventoryIcon = true;
            bool skyPromptEnabled = true;
            std::int32_t skyPromptButtonPlacement = 0;
            bool skyPromptShowCounts = true;
            bool quickLootEnabled = true;
            bool quickLootMarkButton = true;

            bool autoJunkOnPickup = true;
            bool autoJunkOnMenuOpen = true;
            std::vector<std::string> autoJunkTypes;
            std::vector<std::string> autoJunkMaterials;
            std::vector<std::string> autoJunkKeywords;

            bool autoExport = false;
            bool autoImport = false;
            bool replaceJunkListOnLoad = false;
            bool aggressiveRefresh = false;
            std::int32_t aggressiveRefreshMaxInterval = 10;

            bool diiiInstalled = false;
            bool skyPromptInstalled = false;
            bool quickLootInstalled = false;
            RE::TESObjectMISC* gold001 = nullptr;
            RE::TESObjectREFR* trashContainer = nullptr;
        };

        Values g_values;

        using SectionMap = std::map<std::string, std::string>;
        using IniMap = std::map<std::string, SectionMap>;

        std::string Trim(std::string_view text) {
            const auto start = text.find_first_not_of(" \t\r\n");
            if (start == std::string_view::npos) {
                return {};
            }
            const auto end = text.find_last_not_of(" \t\r\n");
            return std::string(text.substr(start, end - start + 1));
        }

        IniMap ParseIni(const std::filesystem::path& path) {
            IniMap result;
            std::ifstream in(path);
            if (!in) {
                return result;
            }

            std::string section;
            std::string line;
            while (std::getline(in, line)) {
                auto trimmed = Trim(line);
                if (trimmed.empty() || trimmed[0] == ';' || trimmed[0] == '#') {
                    continue;
                }
                if (trimmed.front() == '[' && trimmed.back() == ']') {
                    section = trimmed.substr(1, trimmed.size() - 2);
                    continue;
                }
                const auto eq = trimmed.find('=');
                if (eq == std::string::npos || section.empty()) {
                    continue;
                }
                result[section][Trim(trimmed.substr(0, eq))] = Trim(trimmed.substr(eq + 1));
            }
            return result;
        }

        const std::string* FindValue(const IniMap& ini, std::string_view section, std::string_view altSection, std::string_view key) {
            auto findIn = [&](std::string_view name) -> const std::string* {
                auto sectionIt = ini.find(std::string(name));
                if (sectionIt == ini.end()) {
                    return nullptr;
                }
                auto keyIt = sectionIt->second.find(std::string(key));
                if (keyIt == sectionIt->second.end()) {
                    return nullptr;
                }
                return &keyIt->second;
            };

            if (auto* value = findIn(section)) {
                return value;
            }
            if (!altSection.empty()) {
                return findIn(altSection);
            }
            return nullptr;
        }

        bool ParseBool(std::string_view text, bool fallback) {
            if (text == "1" || Util::String::iEquals(text, "true")) {
                return true;
            }
            if (text == "0" || Util::String::iEquals(text, "false")) {
                return false;
            }
            return fallback;
        }

        std::int32_t ParseInt(std::string_view text, std::int32_t fallback) {
            try {
                return std::stoi(std::string(text));
            } catch (...) {
                return fallback;
            }
        }

        float ParseFloat(std::string_view text, float fallback) {
            try {
                return std::stof(std::string(text));
            } catch (...) {
                return fallback;
            }
        }

        bool ReadBool(const IniMap& ini, std::string_view section, std::string_view altSection, std::string_view key, bool& value) {
            if (auto* raw = FindValue(ini, section, altSection, key)) {
                value = ParseBool(*raw, value);
                return true;
            }
            return false;
        }

        bool ReadInt(const IniMap& ini, std::string_view section, std::string_view altSection, std::string_view key, std::int32_t& value) {
            if (auto* raw = FindValue(ini, section, altSection, key)) {
                value = ParseInt(*raw, value);
                return true;
            }
            return false;
        }

        bool ReadUInt(const IniMap& ini, std::string_view section, std::string_view altSection, std::string_view key, std::uint32_t& value) {
            if (auto* raw = FindValue(ini, section, altSection, key)) {
                value = static_cast<std::uint32_t>(ParseInt(*raw, static_cast<std::int32_t>(value)));
                return true;
            }
            return false;
        }

        bool ReadFloat(const IniMap& ini, std::string_view section, std::string_view altSection, std::string_view key, float& value) {
            if (auto* raw = FindValue(ini, section, altSection, key)) {
                value = ParseFloat(*raw, value);
                return true;
            }
            return false;
        }

        bool ReadString(const IniMap& ini, std::string_view section, std::string_view altSection, std::string_view key, std::string& value) {
            if (auto* raw = FindValue(ini, section, altSection, key)) {
                value = *raw;
                return true;
            }
            return false;
        }

        std::vector<std::string> ParseTypeList(std::string_view text) {
            std::vector<std::string> types;
            for (const auto& token : Util::String::Split(std::string(text), ",")) {
                auto trimmed = Trim(token);
                if (trimmed.empty() || trimmed.find(',') != std::string::npos) {
                    continue;
                }
                const bool duplicate = std::ranges::any_of(types, [&](const std::string& existing) {
                    return Util::String::iEquals(existing, trimmed);
                });
                if (!duplicate) {
                    types.push_back(std::move(trimmed));
                }
            }
            return types;
        }

        bool ApplyIni(const IniMap& ini) {
            bool complete = true;
            complete &= ReadUInt(ini, "Hotkey", {}, "iJunkKey", g_values.markJunkKey);
            complete &= ReadUInt(ini, "Hotkey", {}, "iTransferJunkKey", g_values.transferJunkKey);
            complete &= ReadUInt(ini, "Hotkey", {}, "iGamepadJunkKey", g_values.gamepadJunkKey);
            complete &= ReadUInt(ini, "Hotkey", {}, "iTrashJunkKey", g_values.trashJunkKey);
            complete &= ReadInt(ini, "Hotkey", {}, "iGamepadTransferHoldTime", g_values.gamepadTransferHoldTime);
            complete &= ReadBool(ini, "Trash", {}, "bEnableTrash", g_values.enableTrash);
            complete &= ReadInt(ini, "Trash", {}, "iTrashHoldSeconds", g_values.trashHoldSeconds);
            complete &= ReadInt(ini, "Trash", {}, "iGamepadTrashHoldSeconds", g_values.gamepadTrashHoldSeconds);
            complete &= ReadInt(ini, "Trash", {}, "iTrashExpireDays", g_values.trashExpireDays);

            complete &= ReadBool(ini, "Confirmation", {}, "bConfirmTransfer", g_values.confirmTransfer);
            complete &= ReadBool(ini, "Confirmation", {}, "bConfirmSell", g_values.confirmSell);

            complete &= ReadInt(ini, "Priority", {}, "iTransferPriority", g_values.transferPriority);
            complete &= ReadInt(ini, "Priority", {}, "iSellPriority", g_values.sellPriority);

            complete &= ReadBool(ini, "Protection", {}, "bProtectEquipped", g_values.protectEquipped);
            complete &= ReadBool(ini, "Protection", {}, "bProtectFavorites", g_values.protectFavorites);
            complete &= ReadBool(ini, "Protection", {}, "bProtectEnchanted", g_values.protectEnchanted);

            complete &= ReadBool(ini, "Misc", "MiscSettings", "bNotifyOnMarkUnmark", g_values.notifyOnMarkUnmark);
            complete &= ReadBool(ini, "Misc", "MiscSettings", "bNotifyOnJunkTransfer", g_values.notifyOnJunkTransfer);
            complete &= ReadBool(ini, "Misc", "MiscSettings", "bNotifyOnJunkSell", g_values.notifyOnJunkSell);
            complete &= ReadInt(ini, "Overlay", {}, "iOverlayOpacity", g_values.overlayOpacity);
            complete &= ReadInt(ini, "Misc", "MiscSettings", "iLogLevel", g_values.logLevel);
            complete &= ReadFloat(ini, "Misc", "MiscSettings", "fHeavyLoadDelayMultiplier", g_values.heavyLoadDelayMultiplier);
            complete &= ReadInt(ini, "Misc", "MiscSettings", "iLargeUniqueTypes", g_values.largeUniqueTypes);
            complete &= ReadInt(ini, "Misc", "MiscSettings", "iLargeTotalItems", g_values.largeTotalItems);
            complete &= ReadInt(ini, "Misc", "MiscSettings", "iSellChunkSize", g_values.sellChunkSize);

            complete &= ReadBool(ini, "Integration", "IntegrationSettings", "bUpdateItemIcon", g_values.updateItemIcon);
            complete &= ReadBool(ini, "Integration", "IntegrationSettings", "bUpdateSubTypeDisplay", g_values.updateSubTypeDisplay);
            complete &= ReadBool(ini, "Integration", "IntegrationSettings", "bUseDynamicInventoryIcon", g_values.useDynamicInventoryIcon);
            complete &= ReadBool(ini, "Integration", "IntegrationSettings", "bSkyPromptEnabled", g_values.skyPromptEnabled);
            complete &= ReadInt(ini, "Integration", "IntegrationSettings", "iSkyPromptButtonPlacement", g_values.skyPromptButtonPlacement);
            complete &= ReadBool(ini, "Integration", "IntegrationSettings", "bSkyPromptShowCounts", g_values.skyPromptShowCounts);
            complete &= ReadBool(ini, "Integration", "IntegrationSettings", "bQuickLootEnabled", g_values.quickLootEnabled);
            complete &= ReadBool(ini, "Integration", "IntegrationSettings", "bQuickLootMarkButton", g_values.quickLootMarkButton);

            complete &= ReadBool(ini, "Utility", {}, "bReplaceJunkListOnLoad", g_values.replaceJunkListOnLoad);
            complete &= ReadBool(ini, "Utility", {}, "bAggressiveRefresh", g_values.aggressiveRefresh);
            complete &= ReadInt(ini, "Utility", {}, "iAggressiveRefreshMaxInterval", g_values.aggressiveRefreshMaxInterval);

            complete &= ReadBool(ini, "Maintenance", {}, "bAutoSaveJunkListToFile", g_values.autoExport);
            complete &= ReadBool(ini, "Maintenance", {}, "bAutoLoadJunkListFromFile", g_values.autoImport);

            complete &= ReadBool(ini, "AutoJunk", {}, "bAutoJunkOnPickup", g_values.autoJunkOnPickup);
            complete &= ReadBool(ini, "AutoJunk", {}, "bAutoJunkOnMenuOpen", g_values.autoJunkOnMenuOpen);
            std::string autoJunkTypesRaw;
            if (ReadString(ini, "AutoJunk", {}, "sAutoJunkTypes", autoJunkTypesRaw)) {
                g_values.autoJunkTypes = ParseTypeList(autoJunkTypesRaw);
            } else {
                complete = false;
            }
            std::string autoJunkMaterialsRaw;
            if (ReadString(ini, "AutoJunk", {}, "sAutoJunkMaterials", autoJunkMaterialsRaw)) {
                g_values.autoJunkMaterials = ParseTypeList(autoJunkMaterialsRaw);
            } else {
                complete = false;
            }
            std::string autoJunkKeywordsRaw;
            if (ReadString(ini, "AutoJunk", {}, "sAutoJunkKeywords", autoJunkKeywordsRaw)) {
                g_values.autoJunkKeywords = ParseTypeList(autoJunkKeywordsRaw);
            } else {
                complete = false;
            }
            return complete;
        }

        void LogSettings() {
            SKSE::log::info(" ");
            SKSE::log::info("Updating Settings...");
            SKSE::log::info("DIII Detection | DynamicInventoryIconInjector.dll loaded: {}", g_values.diiiInstalled);
            SKSE::log::info(
                "Transfer Option Settings | ConfirmTransfer: {} | TransferPriority: {}",
                g_values.confirmTransfer,
                Settings::SortPriorityLabel(static_cast<Settings::SortPriority>(g_values.transferPriority)));
            SKSE::log::info(
                "Sell Option Settings | ConfirmSell: {} | SellPriority: {}",
                g_values.confirmSell,
                Settings::SortPriorityLabel(static_cast<Settings::SortPriority>(g_values.sellPriority)));
            SKSE::log::info(
                "Protection Settings | ProtectEquipped: {} | ProtectFavorites: {} | ProtectEnchanted: {}",
                g_values.protectEquipped,
                g_values.protectFavorites,
                g_values.protectEnchanted);
            SKSE::log::info(
                "Notification Settings | NotifyOnMarkUnmark: {} | NotifyOnJunkTransfer: {} | NotifyOnJunkSell: {}",
                g_values.notifyOnMarkUnmark,
                g_values.notifyOnJunkTransfer,
                g_values.notifyOnJunkSell);
            SKSE::log::info("Overlay Settings | OverlayOpacity: {}", g_values.overlayOpacity);
            SKSE::log::info(
                "Hotkey Settings | MarkJunkKey: {} | TransferJunkKey: {} | GamepadJunkKey: {} | GamepadTransferHoldTime: {} | TrashJunkKey: {}",
                g_values.markJunkKey,
                g_values.transferJunkKey,
                g_values.gamepadJunkKey,
                g_values.gamepadTransferHoldTime,
                g_values.trashJunkKey);
            SKSE::log::info(
                "Trash Settings | Enabled: {} | HoldSeconds: {} | GamepadTrashHoldSeconds: {} | ExpireDays: {}",
                g_values.enableTrash,
                g_values.trashHoldSeconds,
                g_values.gamepadTrashHoldSeconds,
                g_values.trashExpireDays);
            SKSE::log::info(
                "Misc Settings | LogLevel: {} | AggressiveRefresh: {} | AutoExport: {} | AutoImport: {} | HeavyLoadDelayMultiplier: {:.2f} | LargeUniqueTypes: {} | LargeTotalItems: {} | SellChunkSize: {}",
                Settings::LogLevelLabel(static_cast<Settings::LogLevel>(g_values.logLevel)),
                g_values.aggressiveRefresh,
                g_values.autoExport,
                g_values.autoImport,
                g_values.heavyLoadDelayMultiplier,
                g_values.largeUniqueTypes,
                g_values.largeTotalItems,
                g_values.sellChunkSize);
            SKSE::log::info(
                "Integration Settings | UpdateItemIcon: {} | UpdateSubTypeDisplay: {} | UseDynamicInventoryIcon: {} | SkyPromptEnabled: {} | SkyPromptButtonPlacement: {} | SkyPromptShowCounts: {} | QuickLootEnabled: {} | QuickLootMarkButton: {}",
                g_values.updateItemIcon,
                g_values.updateSubTypeDisplay,
                g_values.useDynamicInventoryIcon,
                g_values.skyPromptEnabled,
                g_values.skyPromptButtonPlacement,
                g_values.skyPromptShowCounts,
                g_values.quickLootEnabled,
                g_values.quickLootMarkButton);
            SKSE::log::info(
                "Auto Junk Settings | OnPickup: {} | OnMenuOpen: {} | Types: {} | Materials: {} | Keywords: {}",
                g_values.autoJunkOnPickup,
                g_values.autoJunkOnMenuOpen,
                Util::String::Join(g_values.autoJunkTypes, ", "),
                Util::String::Join(g_values.autoJunkMaterials, ", "),
                Util::String::Join(g_values.autoJunkKeywords, ", "));
            SKSE::log::info(" ");
        }

        void DetectDIII() {
            g_values.diiiInstalled = GetModuleHandleA("DynamicInventoryIconInjector.dll") != nullptr;
        }

        void DetectSkyPrompt() {
            g_values.skyPromptInstalled = GetModuleHandleW(L"SkyPrompt") != nullptr;
        }

        void DetectQuickLoot() {
            g_values.quickLootInstalled = GetModuleHandleW(L"QuickLootIE") != nullptr;
        }

        std::filesystem::path AbsolutePath(const char* relativePath) {
            std::error_code ec;
            auto path = std::filesystem::absolute(relativePath, ec);
            if (ec) {
                return std::filesystem::path(relativePath);
            }
            return path;
        }
    }

    void Settings::ClampValues() {
        g_values.gamepadTransferHoldTime = std::clamp(g_values.gamepadTransferHoldTime, 2, 30);
        if (g_values.gamepadTrashHoldSeconds < g_values.gamepadTransferHoldTime) {
            g_values.gamepadTrashHoldSeconds = g_values.gamepadTransferHoldTime;
        }
        g_values.gamepadTrashHoldSeconds = std::clamp(
            g_values.gamepadTrashHoldSeconds,
            g_values.gamepadTransferHoldTime,
            30);
        g_values.trashHoldSeconds = std::clamp(g_values.trashHoldSeconds, 0, 10);
        g_values.trashExpireDays = std::clamp(g_values.trashExpireDays, 0, 30);
        g_values.transferPriority = std::clamp(g_values.transferPriority, 0, 6);
        g_values.sellPriority = std::clamp(g_values.sellPriority, 0, 6);
        g_values.heavyLoadDelayMultiplier = std::clamp(g_values.heavyLoadDelayMultiplier, 0.5f, 5.0f);
        g_values.largeUniqueTypes = std::clamp(g_values.largeUniqueTypes, 1, 500);
        g_values.largeTotalItems = std::clamp(g_values.largeTotalItems, 1, 1000);
        g_values.sellChunkSize = std::clamp(g_values.sellChunkSize, 50, 1500);
        g_values.aggressiveRefreshMaxInterval = std::clamp(g_values.aggressiveRefreshMaxInterval, 1, 60);
        g_values.skyPromptButtonPlacement = std::clamp(g_values.skyPromptButtonPlacement, 0, 1);
        g_values.logLevel = std::clamp(g_values.logLevel, 0, 4);
        g_values.overlayOpacity = std::clamp(g_values.overlayOpacity, 0, 100);
    }

    void Settings::ApplyIntegrationGuards() {
        DetectDIII();
        DetectSkyPrompt();
        DetectQuickLoot();
        if (!g_values.diiiInstalled && g_values.useDynamicInventoryIcon) {
            SKSE::log::debug("DIII not installed, forcing UseDynamicInventoryIcon to false");
            g_values.useDynamicInventoryIcon = false;
        }
    }

    void Settings::LoadFromIni() {
        DetectDIII();

        const auto iniPath = AbsolutePath(kIniPath);
        const auto mcmPath = AbsolutePath(kMcmIniPath);

        if (std::filesystem::exists(iniPath)) {
            ResetToDefaults();
            const bool complete = ApplyIni(ParseIni(iniPath));
            SKSE::log::info("Loaded settings from {}", iniPath.string());
            ClampValues();
            ApplyLogLevel();
            ApplyIntegrationGuards();
            if (!complete) {
                SaveToIni();
                SKSE::log::info("Filled missing settings in {}", iniPath.string());
            }
            LogSettings();
        } else if (std::filesystem::exists(mcmPath)) {
            ApplyIni(ParseIni(mcmPath));
            SKSE::log::info("Migrated settings from {}", mcmPath.string());
            ClampValues();
            ApplyLogLevel();
            ApplyIntegrationGuards();
            SaveToIni();
            LogSettings();
        } else {
            SKSE::log::info("No settings INI found, writing defaults to {}", iniPath.string());
            ResetToDefaults();
            SaveToIni();
            LogSettings();
        }
    }

    bool Settings::SaveToIni() {
        ClampValues();
        ApplyLogLevel();
        ApplyIntegrationGuards();

        const auto iniPath = AbsolutePath(kIniPath);
        auto tmpPath = iniPath;
        tmpPath += ".tmp";

        std::error_code ec;
        std::filesystem::create_directories(iniPath.parent_path(), ec);
        if (ec) {
            SKSE::log::error("Failed to create {}: {}", iniPath.parent_path().string(), ec.message());
            return false;
        }

        {
            std::ofstream out(tmpPath, std::ios::trunc);
            if (!out) {
                SKSE::log::error("Failed to write {}", tmpPath.string());
                return false;
            }

            out << "[Hotkey]\n";
            out << "iJunkKey=" << g_values.markJunkKey << "\n";
            out << "iTransferJunkKey=" << g_values.transferJunkKey << "\n";
            out << "iGamepadJunkKey=" << g_values.gamepadJunkKey << "\n";
            out << "iTrashJunkKey=" << g_values.trashJunkKey << "\n";
            out << "iGamepadTransferHoldTime=" << g_values.gamepadTransferHoldTime << "\n\n";

            out << "[Trash]\n";
            out << "bEnableTrash=" << (g_values.enableTrash ? 1 : 0) << "\n";
            out << "iTrashHoldSeconds=" << g_values.trashHoldSeconds << "\n";
            out << "iGamepadTrashHoldSeconds=" << g_values.gamepadTrashHoldSeconds << "\n";
            out << "iTrashExpireDays=" << g_values.trashExpireDays << "\n\n";

            out << "[Confirmation]\n";
            out << "bConfirmTransfer=" << (g_values.confirmTransfer ? 1 : 0) << "\n";
            out << "bConfirmSell=" << (g_values.confirmSell ? 1 : 0) << "\n\n";

            out << "[Priority]\n";
            out << "iTransferPriority=" << g_values.transferPriority << "\n";
            out << "iSellPriority=" << g_values.sellPriority << "\n\n";

            out << "[Protection]\n";
            out << "bProtectEquipped=" << (g_values.protectEquipped ? 1 : 0) << "\n";
            out << "bProtectFavorites=" << (g_values.protectFavorites ? 1 : 0) << "\n";
            out << "bProtectEnchanted=" << (g_values.protectEnchanted ? 1 : 0) << "\n\n";

            out << "[Overlay]\n";
            out << "iOverlayOpacity=" << g_values.overlayOpacity << "\n\n";

            out << "[Misc]\n";
            out << "bNotifyOnMarkUnmark=" << (g_values.notifyOnMarkUnmark ? 1 : 0) << "\n";
            out << "bNotifyOnJunkTransfer=" << (g_values.notifyOnJunkTransfer ? 1 : 0) << "\n";
            out << "bNotifyOnJunkSell=" << (g_values.notifyOnJunkSell ? 1 : 0) << "\n";
            out << "iLogLevel=" << g_values.logLevel << "\n";
            out << "fHeavyLoadDelayMultiplier=" << g_values.heavyLoadDelayMultiplier << "\n";
            out << "iLargeUniqueTypes=" << g_values.largeUniqueTypes << "\n";
            out << "iLargeTotalItems=" << g_values.largeTotalItems << "\n";
            out << "iSellChunkSize=" << g_values.sellChunkSize << "\n\n";

            out << "[Integration]\n";
            out << "bUpdateItemIcon=" << (g_values.updateItemIcon ? 1 : 0) << "\n";
            out << "bUpdateSubTypeDisplay=" << (g_values.updateSubTypeDisplay ? 1 : 0) << "\n";
            out << "bUseDynamicInventoryIcon=" << (g_values.useDynamicInventoryIcon ? 1 : 0) << "\n";
            out << "bSkyPromptEnabled=" << (g_values.skyPromptEnabled ? 1 : 0) << "\n";
            out << "iSkyPromptButtonPlacement=" << g_values.skyPromptButtonPlacement << "\n";
            out << "bSkyPromptShowCounts=" << (g_values.skyPromptShowCounts ? 1 : 0) << "\n";
            out << "bQuickLootEnabled=" << (g_values.quickLootEnabled ? 1 : 0) << "\n";
            out << "bQuickLootMarkButton=" << (g_values.quickLootMarkButton ? 1 : 0) << "\n\n";

            out << "[Utility]\n";
            out << "bReplaceJunkListOnLoad=" << (g_values.replaceJunkListOnLoad ? 1 : 0) << "\n";
            out << "bAggressiveRefresh=" << (g_values.aggressiveRefresh ? 1 : 0) << "\n";
            out << "iAggressiveRefreshMaxInterval=" << g_values.aggressiveRefreshMaxInterval << "\n\n";

            out << "[Maintenance]\n";
            out << "bAutoSaveJunkListToFile=" << (g_values.autoExport ? 1 : 0) << "\n";
            out << "bAutoLoadJunkListFromFile=" << (g_values.autoImport ? 1 : 0) << "\n\n";

            out << "[AutoJunk]\n";
            out << "sAutoJunkTypes=" << Util::String::Join(g_values.autoJunkTypes, ", ") << "\n";
            out << "sAutoJunkMaterials=" << Util::String::Join(g_values.autoJunkMaterials, ", ") << "\n";
            out << "sAutoJunkKeywords=" << Util::String::Join(g_values.autoJunkKeywords, ", ") << "\n";
            out << "bAutoJunkOnPickup=" << (g_values.autoJunkOnPickup ? 1 : 0) << "\n";
            out << "bAutoJunkOnMenuOpen=" << (g_values.autoJunkOnMenuOpen ? 1 : 0) << "\n";

            out.flush();
            if (!out) {
                SKSE::log::error("Failed to write {}", tmpPath.string());
                out.close();
                std::filesystem::remove(tmpPath, ec);
                return false;
            }
        }

        std::filesystem::remove(iniPath, ec);
        std::filesystem::rename(tmpPath, iniPath, ec);
        if (ec) {
            SKSE::log::error("Failed to replace {}: {}", iniPath.string(), ec.message());
            return false;
        }

        static bool loggedSuccess = false;
        if (!loggedSuccess) {
            SKSE::log::debug("Saved settings to {}", iniPath.string());
            loggedSuccess = true;
        }
        return true;
    }

    void Settings::ResetToDefaults() {
        g_values = Values{};
        DetectDIII();
        ApplyIntegrationGuards();
    }

    void Settings::LoadGameForms() {
        auto* goldForm = RE::TESForm::LookupByID(0xF);
        if (goldForm) {
            g_values.gold001 = goldForm->As<RE::TESObjectMISC>();
        } else {
            SKSE::log::error("Failed to lookup Gold001 (0xF)");
        }

        g_values.trashContainer = nullptr;
        auto* dataHandler = RE::TESDataHandler::GetSingleton();
        const bool trashPluginLoaded = dataHandler && dataHandler->LookupModByName(kTrashContainerPlugin);
        if (!trashPluginLoaded) {
            SKSE::log::warn("{} is not loaded; trash disabled", kTrashContainerPlugin);
        } else if (kTrashContainerFormID == 0) {
            SKSE::log::warn(
                "Trash container FormID is unset in settings.h; trash disabled until the JunkIt.esp REFR is assigned");
        } else {
            g_values.trashContainer = dataHandler->LookupForm<RE::TESObjectREFR>(
                kTrashContainerFormID,
                kTrashContainerPlugin);
            if (g_values.trashContainer) {
                SKSE::log::info(
                    "Resolved trash container {:08X} from {}",
                    g_values.trashContainer->GetFormID(),
                    kTrashContainerPlugin);
            } else {
                SKSE::log::warn(
                    "Trash container {:X} not found in {}; trash disabled",
                    kTrashContainerFormID,
                    kTrashContainerPlugin);
            }
        }

        ApplyIntegrationGuards();
    }

    bool Settings::ConfirmTransfer() { return g_values.confirmTransfer; }
    Settings::SortPriority Settings::GetTransferPriority() {
        return static_cast<SortPriority>(g_values.transferPriority);
    }

    bool Settings::ConfirmSell() { return g_values.confirmSell; }
    Settings::SortPriority Settings::GetSellPriority() {
        return static_cast<SortPriority>(g_values.sellPriority);
    }

    bool Settings::ProtectEquipped() { return g_values.protectEquipped; }
    bool Settings::ProtectFavorites() { return g_values.protectFavorites; }
    bool Settings::ProtectEnchanted() { return g_values.protectEnchanted; }

    float Settings::GetMarkJunkKey() { return static_cast<float>(g_values.markJunkKey); }
    float Settings::GetTransferJunkKey() { return static_cast<float>(g_values.transferJunkKey); }
    float Settings::GetGamepadJunkKey() { return static_cast<float>(g_values.gamepadJunkKey); }
    float Settings::GetGamepadTransferHoldTime() { return static_cast<float>(g_values.gamepadTransferHoldTime); }
    float Settings::GetTrashJunkKey() { return static_cast<float>(g_values.trashJunkKey); }
    std::int32_t Settings::GetTrashHoldSeconds() { return g_values.trashHoldSeconds; }
    std::int32_t Settings::GetGamepadTrashHoldSeconds() { return g_values.gamepadTrashHoldSeconds; }
    std::int32_t Settings::GetTrashExpireDays() { return g_values.trashExpireDays; }
    RE::TESObjectREFR* Settings::GetTrashContainer() { return g_values.trashContainer; }
    bool Settings::IsTrashAvailable() { return g_values.enableTrash && g_values.trashContainer != nullptr; }

    bool Settings::GetNotifyOnMarkUnmark() { return g_values.notifyOnMarkUnmark; }
    bool Settings::GetNotifyOnJunkTransfer() { return g_values.notifyOnJunkTransfer; }
    bool Settings::GetNotifyOnJunkSell() { return g_values.notifyOnJunkSell; }
    std::int32_t Settings::GetOverlayOpacity() { return g_values.overlayOpacity; }
    Settings::LogLevel Settings::GetLogLevel() {
        return static_cast<LogLevel>(g_values.logLevel);
    }
    bool Settings::GetAggressiveRefresh() { return g_values.aggressiveRefresh; }
    std::int32_t Settings::GetAggressiveRefreshMaxInterval() { return g_values.aggressiveRefreshMaxInterval; }
    float Settings::GetHeavyLoadDelayMultiplier() { return g_values.heavyLoadDelayMultiplier; }
    std::size_t Settings::GetLargeUniqueTypes() { return static_cast<std::size_t>(g_values.largeUniqueTypes); }
    std::int32_t Settings::GetLargeTotalItems() { return g_values.largeTotalItems; }
    std::int32_t Settings::GetSellChunkSize() { return g_values.sellChunkSize; }

    bool Settings::GetAutoExport() { return g_values.autoExport; }
    bool Settings::GetAutoImport() { return g_values.autoImport; }
    bool Settings::GetReplaceJunkListOnLoad() { return g_values.replaceJunkListOnLoad; }

    bool Settings::GetUpdateItemIcon() { return g_values.updateItemIcon; }
    bool Settings::GetUpdateSubTypeDisplay() { return g_values.updateSubTypeDisplay; }
    bool Settings::GetUseDynamicInventoryIcon() { return g_values.useDynamicInventoryIcon; }
    bool Settings::GetSkyPromptEnabled() { return g_values.skyPromptEnabled; }
    Settings::SkyPromptButtonPlacement Settings::GetSkyPromptButtonPlacement() {
        return static_cast<SkyPromptButtonPlacement>(g_values.skyPromptButtonPlacement);
    }
    bool Settings::GetSkyPromptShowCounts() { return g_values.skyPromptShowCounts; }
    bool Settings::GetQuickLootEnabled() { return g_values.quickLootEnabled; }
    bool Settings::GetQuickLootMarkButton() { return g_values.quickLootMarkButton; }

    bool Settings::GetAutoJunkOnPickup() { return g_values.autoJunkOnPickup; }
    bool Settings::GetAutoJunkOnMenuOpen() { return g_values.autoJunkOnMenuOpen; }
    const std::vector<std::string>& Settings::GetAutoJunkTypes() { return g_values.autoJunkTypes; }

    bool Settings::TryAddAutoJunkType(std::string_view type) {
        auto trimmed = Trim(type);
        if (trimmed.empty() || trimmed.find(',') != std::string::npos) {
            return false;
        }
        const bool duplicate = std::ranges::any_of(g_values.autoJunkTypes, [&](const std::string& existing) {
            return Util::String::iEquals(existing, trimmed);
        });
        if (duplicate) {
            return false;
        }
        g_values.autoJunkTypes.push_back(std::move(trimmed));
        return true;
    }

    bool Settings::RemoveAutoJunkTypeAt(std::size_t index) {
        if (index >= g_values.autoJunkTypes.size()) {
            return false;
        }
        g_values.autoJunkTypes.erase(g_values.autoJunkTypes.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    const std::vector<std::string>& Settings::GetAutoJunkMaterials() { return g_values.autoJunkMaterials; }

    bool Settings::TryAddAutoJunkMaterial(std::string_view material) {
        auto trimmed = Trim(material);
        if (trimmed.empty() || trimmed.find(',') != std::string::npos) {
            return false;
        }
        const bool duplicate = std::ranges::any_of(g_values.autoJunkMaterials, [&](const std::string& existing) {
            return Util::String::iEquals(existing, trimmed);
        });
        if (duplicate) {
            return false;
        }
        g_values.autoJunkMaterials.push_back(std::move(trimmed));
        return true;
    }

    bool Settings::RemoveAutoJunkMaterialAt(std::size_t index) {
        if (index >= g_values.autoJunkMaterials.size()) {
            return false;
        }
        g_values.autoJunkMaterials.erase(g_values.autoJunkMaterials.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    const std::vector<std::string>& Settings::GetAutoJunkKeywords() { return g_values.autoJunkKeywords; }

    bool Settings::TryAddAutoJunkKeyword(std::string_view keyword) {
        auto trimmed = Trim(keyword);
        if (trimmed.empty() || trimmed.find(',') != std::string::npos) {
            return false;
        }
        const bool duplicate = std::ranges::any_of(g_values.autoJunkKeywords, [&](const std::string& existing) {
            return Util::String::iEquals(existing, trimmed);
        });
        if (duplicate) {
            return false;
        }
        g_values.autoJunkKeywords.push_back(std::move(trimmed));
        return true;
    }

    bool Settings::RemoveAutoJunkKeywordAt(std::size_t index) {
        if (index >= g_values.autoJunkKeywords.size()) {
            return false;
        }
        g_values.autoJunkKeywords.erase(g_values.autoJunkKeywords.begin() + static_cast<std::ptrdiff_t>(index));
        return true;
    }

    bool Settings::IsDIIIInstalled() { return g_values.diiiInstalled; }
    bool Settings::IsSkyPromptInstalled() { return g_values.skyPromptInstalled; }
    bool Settings::IsQuickLootInstalled() { return g_values.quickLootInstalled; }
    RE::TESObjectMISC* Settings::GetGold001() { return g_values.gold001; }

    std::uint32_t& Settings::MarkJunkKeyValue() { return g_values.markJunkKey; }
    std::uint32_t& Settings::TransferJunkKeyValue() { return g_values.transferJunkKey; }
    std::uint32_t& Settings::GamepadJunkKeyValue() { return g_values.gamepadJunkKey; }
    std::uint32_t& Settings::TrashJunkKeyValue() { return g_values.trashJunkKey; }
    std::int32_t& Settings::GamepadTransferHoldTimeValue() { return g_values.gamepadTransferHoldTime; }
    bool& Settings::EnableTrashValue() { return g_values.enableTrash; }
    std::int32_t& Settings::TrashHoldSecondsValue() { return g_values.trashHoldSeconds; }
    std::int32_t& Settings::GamepadTrashHoldSecondsValue() { return g_values.gamepadTrashHoldSeconds; }
    std::int32_t& Settings::TrashExpireDaysValue() { return g_values.trashExpireDays; }

    bool& Settings::ConfirmTransferValue() { return g_values.confirmTransfer; }
    bool& Settings::ConfirmSellValue() { return g_values.confirmSell; }
    std::int32_t& Settings::TransferPriorityValue() { return g_values.transferPriority; }
    std::int32_t& Settings::SellPriorityValue() { return g_values.sellPriority; }

    bool& Settings::ProtectEquippedValue() { return g_values.protectEquipped; }
    bool& Settings::ProtectFavoritesValue() { return g_values.protectFavorites; }
    bool& Settings::ProtectEnchantedValue() { return g_values.protectEnchanted; }

    bool& Settings::NotifyOnMarkUnmarkValue() { return g_values.notifyOnMarkUnmark; }
    bool& Settings::NotifyOnJunkTransferValue() { return g_values.notifyOnJunkTransfer; }
    bool& Settings::NotifyOnJunkSellValue() { return g_values.notifyOnJunkSell; }
    std::int32_t& Settings::OverlayOpacityValue() { return g_values.overlayOpacity; }
    std::int32_t& Settings::LogLevelValue() { return g_values.logLevel; }
    float& Settings::HeavyLoadDelayMultiplierValue() { return g_values.heavyLoadDelayMultiplier; }
    std::int32_t& Settings::LargeUniqueTypesValue() { return g_values.largeUniqueTypes; }
    std::int32_t& Settings::LargeTotalItemsValue() { return g_values.largeTotalItems; }
    std::int32_t& Settings::SellChunkSizeValue() { return g_values.sellChunkSize; }

    bool& Settings::UpdateItemIconValue() { return g_values.updateItemIcon; }
    bool& Settings::UpdateSubTypeDisplayValue() { return g_values.updateSubTypeDisplay; }
    bool& Settings::UseDynamicInventoryIconValue() { return g_values.useDynamicInventoryIcon; }
    bool& Settings::SkyPromptEnabledValue() { return g_values.skyPromptEnabled; }
    std::int32_t& Settings::SkyPromptButtonPlacementValue() { return g_values.skyPromptButtonPlacement; }
    bool& Settings::SkyPromptShowCountsValue() { return g_values.skyPromptShowCounts; }
    bool& Settings::QuickLootEnabledValue() { return g_values.quickLootEnabled; }
    bool& Settings::QuickLootMarkButtonValue() { return g_values.quickLootMarkButton; }

    bool& Settings::AutoJunkOnPickupValue() { return g_values.autoJunkOnPickup; }
    bool& Settings::AutoJunkOnMenuOpenValue() { return g_values.autoJunkOnMenuOpen; }

    bool& Settings::AutoExportValue() { return g_values.autoExport; }
    bool& Settings::AutoImportValue() { return g_values.autoImport; }
    bool& Settings::ReplaceJunkListOnLoadValue() { return g_values.replaceJunkListOnLoad; }
    bool& Settings::AggressiveRefreshValue() { return g_values.aggressiveRefresh; }
    std::int32_t& Settings::AggressiveRefreshMaxIntervalValue() { return g_values.aggressiveRefreshMaxInterval; }

    const char* Settings::SortPriorityLabel(SortPriority priority) {
        switch (priority) {
            case SortPriority::kWeightHighLow: return "Weight [High > Low]";
            case SortPriority::kWeightLowHigh: return "Weight [Low > High]";
            case SortPriority::kValueHighLow: return "Value [High > Low]";
            case SortPriority::kValueLowHigh: return "Value [Low > High]";
            case SortPriority::kValueWeightHighLow: return "Value/Weight [High > Low]";
            case SortPriority::kValueWeightLowHigh: return "Value/Weight [Low > High]";
            case SortPriority::kChaos: return "Chaos";
        }
        return "Chaos";
    }

    const char* Settings::LogLevelLabel(LogLevel level) {
        switch (level) {
            case LogLevel::kTrace: return "Trace";
            case LogLevel::kDebug: return "Debug";
            case LogLevel::kInfo: return "Info";
            case LogLevel::kWarn: return "Warn";
            case LogLevel::kError: return "Error";
        }
        return "Info";
    }

    void Settings::ApplyLogLevel() {
        spdlog::level::level_enum level = spdlog::level::info;
        switch (static_cast<LogLevel>(g_values.logLevel)) {
            case LogLevel::kTrace:
                level = spdlog::level::trace;
                break;
            case LogLevel::kDebug:
                level = spdlog::level::debug;
                break;
            case LogLevel::kInfo:
                level = spdlog::level::info;
                break;
            case LogLevel::kWarn:
                level = spdlog::level::warn;
                break;
            case LogLevel::kError:
                level = spdlog::level::err;
                break;
        }
        spdlog::set_level(level);
        spdlog::flush_on(level);
    }
}
