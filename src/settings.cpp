#include "settings.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <system_error>

namespace JunkIt {
    namespace {
        constexpr auto kIniPath = "Data/SKSE/Plugins/JunkIt.ini";
        constexpr auto kMcmIniPath = "Data/MCM/Settings/JunkIt.ini";

        struct Values {
            std::uint32_t markJunkKey = 50;
            std::uint32_t transferJunkKey = 49;
            std::uint32_t gamepadJunkKey = 270;
            std::int32_t gamepadTransferHoldTime = 2;

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
            float heavyLoadDelayMultiplier = 1.0f;
            std::int32_t largeUniqueTypes = 500;
            std::int32_t largeTotalItems = 1000;

            bool updateItemIcon = true;
            bool updateSubTypeDisplay = true;
            bool useDynamicInventoryIcon = true;
            bool skyPromptShowCounts = true;

            bool autoExport = false;
            bool autoImport = false;
            bool replaceJunkListOnLoad = false;
            bool aggressiveRefresh = false;
            std::int32_t aggressiveRefreshMaxInterval = 10;

            bool diiiInstalled = false;
            bool skyPromptInstalled = false;
            RE::TESObjectMISC* gold001 = nullptr;
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

        bool ApplyIni(const IniMap& ini) {
            bool complete = true;
            complete &= ReadUInt(ini, "Hotkey", {}, "iJunkKey", g_values.markJunkKey);
            complete &= ReadUInt(ini, "Hotkey", {}, "iTransferJunkKey", g_values.transferJunkKey);
            complete &= ReadUInt(ini, "Hotkey", {}, "iGamepadJunkKey", g_values.gamepadJunkKey);
            complete &= ReadInt(ini, "Hotkey", {}, "iGamepadTransferHoldTime", g_values.gamepadTransferHoldTime);

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
            complete &= ReadFloat(ini, "Misc", "MiscSettings", "fHeavyLoadDelayMultiplier", g_values.heavyLoadDelayMultiplier);
            complete &= ReadInt(ini, "Misc", "MiscSettings", "iLargeUniqueTypes", g_values.largeUniqueTypes);
            complete &= ReadInt(ini, "Misc", "MiscSettings", "iLargeTotalItems", g_values.largeTotalItems);

            complete &= ReadBool(ini, "Integration", "IntegrationSettings", "bUpdateItemIcon", g_values.updateItemIcon);
            complete &= ReadBool(ini, "Integration", "IntegrationSettings", "bUpdateSubTypeDisplay", g_values.updateSubTypeDisplay);
            complete &= ReadBool(ini, "Integration", "IntegrationSettings", "bUseDynamicInventoryIcon", g_values.useDynamicInventoryIcon);
            complete &= ReadBool(ini, "Integration", "IntegrationSettings", "bSkyPromptShowCounts", g_values.skyPromptShowCounts);

            complete &= ReadBool(ini, "Utility", {}, "bReplaceJunkListOnLoad", g_values.replaceJunkListOnLoad);
            complete &= ReadBool(ini, "Utility", {}, "bAggressiveRefresh", g_values.aggressiveRefresh);
            complete &= ReadInt(ini, "Utility", {}, "iAggressiveRefreshMaxInterval", g_values.aggressiveRefreshMaxInterval);

            complete &= ReadBool(ini, "Maintenance", {}, "bAutoSaveJunkListToFile", g_values.autoExport);
            complete &= ReadBool(ini, "Maintenance", {}, "bAutoLoadJunkListFromFile", g_values.autoImport);
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
            SKSE::log::info(
                "Hotkey Settings | MarkJunkKey: {} | TransferJunkKey: {} | GamepadJunkKey: {} | GamepadTransferHoldTime: {}",
                g_values.markJunkKey,
                g_values.transferJunkKey,
                g_values.gamepadJunkKey,
                g_values.gamepadTransferHoldTime);
            SKSE::log::info(
                "Misc Settings | AggressiveRefresh: {} | AutoExport: {} | AutoImport: {} | HeavyLoadDelayMultiplier: {:.2f} | LargeUniqueTypes: {} | LargeTotalItems: {}",
                g_values.aggressiveRefresh,
                g_values.autoExport,
                g_values.autoImport,
                g_values.heavyLoadDelayMultiplier,
                g_values.largeUniqueTypes,
                g_values.largeTotalItems);
            SKSE::log::info(
                "Integration Settings | UpdateItemIcon: {} | UpdateSubTypeDisplay: {} | UseDynamicInventoryIcon: {} | SkyPromptShowCounts: {}",
                g_values.updateItemIcon,
                g_values.updateSubTypeDisplay,
                g_values.useDynamicInventoryIcon,
                g_values.skyPromptShowCounts);
            SKSE::log::info(" ");
        }

        void DetectDIII() {
            g_values.diiiInstalled = GetModuleHandleA("DynamicInventoryIconInjector.dll") != nullptr;
        }

        void DetectSkyPrompt() {
            g_values.skyPromptInstalled = GetModuleHandleW(L"SkyPrompt") != nullptr;
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
        g_values.transferPriority = std::clamp(g_values.transferPriority, 0, 6);
        g_values.sellPriority = std::clamp(g_values.sellPriority, 0, 6);
        g_values.heavyLoadDelayMultiplier = std::clamp(g_values.heavyLoadDelayMultiplier, 0.5f, 5.0f);
        g_values.largeUniqueTypes = std::clamp(g_values.largeUniqueTypes, 1, 500);
        g_values.largeTotalItems = std::clamp(g_values.largeTotalItems, 1, 1000);
        g_values.aggressiveRefreshMaxInterval = std::clamp(g_values.aggressiveRefreshMaxInterval, 1, 60);
    }

    void Settings::ApplyIntegrationGuards() {
        DetectDIII();
        DetectSkyPrompt();
        if (!g_values.diiiInstalled && g_values.useDynamicInventoryIcon) {
            SKSE::log::info("DIII not installed, forcing UseDynamicInventoryIcon to false");
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
            out << "iGamepadTransferHoldTime=" << g_values.gamepadTransferHoldTime << "\n\n";

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

            out << "[Misc]\n";
            out << "bNotifyOnMarkUnmark=" << (g_values.notifyOnMarkUnmark ? 1 : 0) << "\n";
            out << "bNotifyOnJunkTransfer=" << (g_values.notifyOnJunkTransfer ? 1 : 0) << "\n";
            out << "bNotifyOnJunkSell=" << (g_values.notifyOnJunkSell ? 1 : 0) << "\n";
            out << "fHeavyLoadDelayMultiplier=" << g_values.heavyLoadDelayMultiplier << "\n";
            out << "iLargeUniqueTypes=" << g_values.largeUniqueTypes << "\n";
            out << "iLargeTotalItems=" << g_values.largeTotalItems << "\n\n";

            out << "[Integration]\n";
            out << "bUpdateItemIcon=" << (g_values.updateItemIcon ? 1 : 0) << "\n";
            out << "bUpdateSubTypeDisplay=" << (g_values.updateSubTypeDisplay ? 1 : 0) << "\n";
            out << "bUseDynamicInventoryIcon=" << (g_values.useDynamicInventoryIcon ? 1 : 0) << "\n";
            out << "bSkyPromptShowCounts=" << (g_values.skyPromptShowCounts ? 1 : 0) << "\n\n";

            out << "[Utility]\n";
            out << "bReplaceJunkListOnLoad=" << (g_values.replaceJunkListOnLoad ? 1 : 0) << "\n";
            out << "bAggressiveRefresh=" << (g_values.aggressiveRefresh ? 1 : 0) << "\n";
            out << "iAggressiveRefreshMaxInterval=" << g_values.aggressiveRefreshMaxInterval << "\n\n";

            out << "[Maintenance]\n";
            out << "bAutoSaveJunkListToFile=" << (g_values.autoExport ? 1 : 0) << "\n";
            out << "bAutoLoadJunkListFromFile=" << (g_values.autoImport ? 1 : 0) << "\n";

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
            SKSE::log::info("Saved settings to {}", iniPath.string());
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

    bool Settings::GetNotifyOnMarkUnmark() { return g_values.notifyOnMarkUnmark; }
    bool Settings::GetNotifyOnJunkTransfer() { return g_values.notifyOnJunkTransfer; }
    bool Settings::GetNotifyOnJunkSell() { return g_values.notifyOnJunkSell; }
    bool Settings::GetAggressiveRefresh() { return g_values.aggressiveRefresh; }
    std::int32_t Settings::GetAggressiveRefreshMaxInterval() { return g_values.aggressiveRefreshMaxInterval; }
    float Settings::GetHeavyLoadDelayMultiplier() { return g_values.heavyLoadDelayMultiplier; }
    std::size_t Settings::GetLargeUniqueTypes() { return static_cast<std::size_t>(g_values.largeUniqueTypes); }
    std::int32_t Settings::GetLargeTotalItems() { return g_values.largeTotalItems; }

    bool Settings::GetAutoExport() { return g_values.autoExport; }
    bool Settings::GetAutoImport() { return g_values.autoImport; }
    bool Settings::GetReplaceJunkListOnLoad() { return g_values.replaceJunkListOnLoad; }

    bool Settings::GetUpdateItemIcon() { return g_values.updateItemIcon; }
    bool Settings::GetUpdateSubTypeDisplay() { return g_values.updateSubTypeDisplay; }
    bool Settings::GetUseDynamicInventoryIcon() { return g_values.useDynamicInventoryIcon; }
    bool Settings::GetSkyPromptShowCounts() { return g_values.skyPromptShowCounts; }

    bool Settings::IsDIIIInstalled() { return g_values.diiiInstalled; }
    bool Settings::IsSkyPromptInstalled() { return g_values.skyPromptInstalled; }
    RE::TESObjectMISC* Settings::GetGold001() { return g_values.gold001; }

    std::uint32_t& Settings::MarkJunkKeyValue() { return g_values.markJunkKey; }
    std::uint32_t& Settings::TransferJunkKeyValue() { return g_values.transferJunkKey; }
    std::uint32_t& Settings::GamepadJunkKeyValue() { return g_values.gamepadJunkKey; }
    std::int32_t& Settings::GamepadTransferHoldTimeValue() { return g_values.gamepadTransferHoldTime; }

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
    float& Settings::HeavyLoadDelayMultiplierValue() { return g_values.heavyLoadDelayMultiplier; }
    std::int32_t& Settings::LargeUniqueTypesValue() { return g_values.largeUniqueTypes; }
    std::int32_t& Settings::LargeTotalItemsValue() { return g_values.largeTotalItems; }

    bool& Settings::UpdateItemIconValue() { return g_values.updateItemIcon; }
    bool& Settings::UpdateSubTypeDisplayValue() { return g_values.updateSubTypeDisplay; }
    bool& Settings::UseDynamicInventoryIconValue() { return g_values.useDynamicInventoryIcon; }
    bool& Settings::SkyPromptShowCountsValue() { return g_values.skyPromptShowCounts; }

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
}
