#include "UI.h"

#include "JunkData.h"
#include "SkyPromptIntegration.h"
#include "Translation.h"
#include "settings.h"
#include "util.h"

#include "SKSEMenuFramework.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace JunkIt {
    namespace {
        enum class CaptureSlot {
            kNone = 0,
            kMark = 1,
            kTransfer = 2,
            kGamepad = 3
        };

        enum class StatusKind {
            kNone = 0,
            kSuccess = 1,
            kFailure = 2
        };

        CaptureSlot g_capture = CaptureSlot::kNone;
        char g_junkFilter[128] = {};
        std::string g_status;
        StatusKind g_statusKind = StatusKind::kNone;

        const ImVec4 kMagenta{0.91f, 0.12f, 0.55f, 1.0f};
        const ImVec4 kMagentaBright{0.95f, 0.28f, 0.68f, 1.0f};
        const ImVec4 kBlue{0.17f, 0.36f, 1.0f, 1.0f};
        const ImVec4 kBlueViolet{0.45f, 0.22f, 0.85f, 1.0f};
        const ImVec4 kHeaderBg{0.05f, 0.05f, 0.06f, 1.0f};
        const ImVec4 kSuccess{0.45f, 0.82f, 0.55f, 1.0f};
        const ImVec4 kFailure{0.90f, 0.40f, 0.40f, 1.0f};
        const ImVec4 kWhite{1.0f, 1.0f, 1.0f, 1.0f};
        const ImVec4 kTransparent{0.0f, 0.0f, 0.0f, 0.0f};

        constexpr unsigned kIconExport = 0xf56e;
        constexpr unsigned kIconImport = 0xf56f;
        constexpr unsigned kIconTrash = 0xf2ed;
        constexpr unsigned kIconSync = 0xf2f1;
        constexpr unsigned kIconRemove = 0xf00d;

        const char* KeyName(std::uint32_t keyCode) {
            switch (keyCode) {
                case 1: return "Escape";
                case 2: return "1";
                case 3: return "2";
                case 4: return "3";
                case 5: return "4";
                case 6: return "5";
                case 7: return "6";
                case 8: return "7";
                case 9: return "8";
                case 10: return "9";
                case 11: return "0";
                case 12: return "-";
                case 13: return "=";
                case 14: return "Backspace";
                case 15: return "Tab";
                case 16: return "Q";
                case 17: return "W";
                case 18: return "E";
                case 19: return "R";
                case 20: return "T";
                case 21: return "Y";
                case 22: return "U";
                case 23: return "I";
                case 24: return "O";
                case 25: return "P";
                case 26: return "[";
                case 27: return "]";
                case 28: return "Enter";
                case 29: return "L-Ctrl";
                case 30: return "A";
                case 31: return "S";
                case 32: return "D";
                case 33: return "F";
                case 34: return "G";
                case 35: return "H";
                case 36: return "J";
                case 37: return "K";
                case 38: return "L";
                case 39: return ";";
                case 40: return "'";
                case 41: return "`";
                case 42: return "L-Shift";
                case 43: return "\\";
                case 44: return "Z";
                case 45: return "X";
                case 46: return "C";
                case 47: return "V";
                case 48: return "B";
                case 49: return "N";
                case 50: return "M";
                case 51: return ",";
                case 52: return ".";
                case 53: return "/";
                case 54: return "R-Shift";
                case 56: return "L-Alt";
                case 57: return "Space";
                case 58: return "Caps Lock";
                case 59: return "F1";
                case 60: return "F2";
                case 61: return "F3";
                case 62: return "F4";
                case 63: return "F5";
                case 64: return "F6";
                case 65: return "F7";
                case 66: return "F8";
                case 67: return "F9";
                case 68: return "F10";
                case 87: return "F11";
                case 88: return "F12";
                case 157: return "R-Ctrl";
                case 184: return "R-Alt";
                case 200: return "Up";
                case 203: return "Left";
                case 205: return "Right";
                case 208: return "Down";
                case 210: return "Insert";
                case 211: return "Delete";
                case 256: return "Mouse 1";
                case 257: return "Mouse 2";
                case 258: return "Mouse 3";
                case 259: return "Mouse 4";
                case 260: return "Mouse 5";
                case 266: return "D-Pad Up";
                case 267: return "D-Pad Down";
                case 268: return "D-Pad Left";
                case 269: return "D-Pad Right";
                case 270: return "Start";
                case 271: return "Back";
                case 272: return "L-Thumb";
                case 273: return "R-Thumb";
                case 274: return "LB";
                case 275: return "RB";
                case 276: return "A";
                case 277: return "B";
                case 278: return "X";
                case 279: return "Y";
                case 280: return "LT";
                case 281: return "RT";
                default: break;
            }

            static char fallback[32];
            std::snprintf(fallback, sizeof(fallback), "Key %u", keyCode);
            return fallback;
        }

        void HelpMarker(const char* helpKey) {
            if (!helpKey || !*helpKey) {
                return;
            }

            ImGui::SameLine();
            ImGui::PushID(helpKey);
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_Stationary)) {
                if (ImGui::BeginTooltip()) {
                    ImGui::PushTextWrapPos(ImGui::GetFontSize() * 28.0f);
                    ImGui::TextUnformatted(Translation::Get(helpKey).c_str());
                    ImGui::PopTextWrapPos();
                    ImGui::EndTooltip();
                }
            }
            ImGui::PopID();
        }

        void SetStatus(std::string_view key, bool success) {
            g_status = Translation::Get(key);
            g_statusKind = success ? StatusKind::kSuccess : StatusKind::kFailure;
        }

        void SaveSettings() {
            if (!Settings::SaveToIni()) {
                SetStatus("$JunkIt_SettingsSaveFailed", false);
            }
        }

        void SaveIfChanged(bool changed) {
            if (changed) {
                SaveSettings();
            }
        }

        void RenderStatus() {
            if (g_status.empty()) {
                return;
            }
            const ImVec4 color = g_statusKind == StatusKind::kSuccess ? kSuccess : kFailure;
            ImGui::TextColored(color, "%s", g_status.c_str());
        }

        ImTextureID GetSplashTexture() {
            static bool attempted = false;
            static ImTextureID texture = nullptr;
            if (attempted) {
                return texture;
            }
            attempted = true;
            texture = SKSEMenuFramework::LoadTexture("Data\\Interface\\JunkIt\\JunkIt_splash_256x256.png");
            if (!texture) {
                texture = SKSEMenuFramework::LoadTexture("Data\\Interface\\JunkIt\\JunkIt_splash_512x512.dds");
            }
            return texture;
        }

        void PushBrandColors() {
            ImGui::PushStyleColor(ImGuiCol_CheckMark, kMagenta);
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, kMagenta);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, kMagentaBright);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kBlueViolet);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, kBlue);
            ImGui::PushStyleColor(ImGuiCol_Separator, kMagenta);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kBlueViolet);
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, kMagenta);
        }

        void PopBrandColors() {
            ImGui::PopStyleColor(8);
        }

        void RenderPageHeader(const char* titleKey) {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, kHeaderBg);
            ImGui::PushStyleColor(ImGuiCol_Border, kMagenta);
            ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 8.0f));

            ImVec2 avail{};
            ImGui::GetContentRegionAvail(&avail);
            ImGui::BeginChild("junkitHeader", ImVec2(avail.x, 88.0f), ImGuiChildFlags_Border);

            constexpr float kLogoSize = 72.0f;
            const float rowStartY = ImGui::GetCursorPosY();
            if (const ImTextureID splash = GetSplashTexture()) {
                ImGui::Image(splash, ImVec2(kLogoSize, kLogoSize), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), kWhite, kTransparent);
                ImGui::SameLine();
                ImGui::SetCursorPosY(rowStartY + (kLogoSize - ImGui::GetTextLineHeight()) * 0.5f);
            }

            ImGui::Text("%s", Translation::Get(titleKey).c_str());
            ImGui::EndChild();

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor(2);
            ImGui::Spacing();
        }

        bool BeginSettingsTable(const char* id) {
            const ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings | ImGuiTableFlags_PadOuterX;
            if (!ImGui::BeginTable(id, 2, flags)) {
                return false;
            }
            ImGui::TableSetupColumn("label", ImGuiTableColumnFlags_WidthStretch, 0.55f);
            ImGui::TableSetupColumn("value", ImGuiTableColumnFlags_WidthStretch, 0.45f);
            return true;
        }

        void SettingLabel(const char* labelKey, const char* helpKey) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(Translation::Get(labelKey).c_str());
            if (helpKey && std::strcmp(labelKey, helpKey) != 0) {
                HelpMarker(helpKey);
            }
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-1.0f);
        }

        bool CheckboxRow(const char* labelKey, const char* helpKey, bool& value) {
            SettingLabel(labelKey, helpKey);
            ImGui::PushID(labelKey);
            const bool changed = ImGui::Checkbox("##v", &value);
            ImGui::PopID();
            return changed;
        }

        bool ComboRow(const char* labelKey, const char* helpKey, std::int32_t& value, const char* const* items, int count) {
            SettingLabel(labelKey, helpKey);
            ImGui::PushID(labelKey);
            const bool changed = ImGui::Combo("##v", &value, items, count);
            ImGui::PopID();
            return changed;
        }

        bool SliderIntRow(const char* labelKey, const char* helpKey, std::int32_t& value, int minValue, int maxValue) {
            SettingLabel(labelKey, helpKey);
            ImGui::PushID(labelKey);
            ImGui::SliderInt("##v", &value, minValue, maxValue);
            const bool committed = ImGui::IsItemDeactivatedAfterEdit();
            ImGui::PopID();
            return committed;
        }

        bool SliderFloatRow(const char* labelKey, const char* helpKey, float& value, float minValue, float maxValue, const char* format) {
            SettingLabel(labelKey, helpKey);
            ImGui::PushID(labelKey);
            ImGui::SliderFloat("##v", &value, minValue, maxValue, format);
            const bool committed = ImGui::IsItemDeactivatedAfterEdit();
            ImGui::PopID();
            return committed;
        }

        bool KeyBindRow(const char* labelKey, const char* helpKey, std::uint32_t& value, CaptureSlot slot) {
            SettingLabel(labelKey, helpKey);
            ImGui::PushID(static_cast<int>(slot));
            const char* buttonText = g_capture == slot
                ? Translation::Get("$JunkIt_PressAnyKey").c_str()
                : KeyName(value);
            const bool clicked = ImGui::Button(buttonText, ImVec2(-1.0f, 0.0f));
            ImGui::PopID();
            if (clicked) {
                g_capture = slot;
            }
            return clicked;
        }

        bool IconButton(unsigned codepoint, const char* labelKey) {
            const std::string label = FontAwesome::UnicodeToUtf8(codepoint) + "  " + Translation::Get(labelKey);
            FontAwesome::PushSolid();
            const bool clicked = ImGui::Button(label.c_str());
            FontAwesome::Pop();
            return clicked;
        }

        bool ConfirmPopup(const char* popupId, const char* titleKey, const char* bodyKey) {
            const std::string name = std::string(Translation::Get(titleKey)) + "###" + popupId;
            if (!ImGui::BeginPopupModal(name.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                return false;
            }

            ImGui::TextWrapped("%s", Translation::Get(bodyKey).c_str());
            ImGui::Spacing();

            bool confirmed = false;
            if (ImGui::Button(Translation::Get("$JunkIt_Yes").c_str(), ImVec2(120.0f, 0.0f))) {
                confirmed = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(Translation::Get("$JunkIt_ConfirmNo").c_str(), ImVec2(120.0f, 0.0f))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
            return confirmed;
        }

        void OpenConfirmPopup(const char* popupId, const char* titleKey) {
            const std::string name = std::string(Translation::Get(titleKey)) + "###" + popupId;
            ImGui::OpenPopup(name.c_str());
        }

        const char* const* SortPriorityItems() {
            static const char* items[7];
            items[0] = Translation::Get("$JunkIt_WeightHighToLow_ENUM").c_str();
            items[1] = Translation::Get("$JunkIt_WeightLowToHigh_ENUM").c_str();
            items[2] = Translation::Get("$JunkIt_ValueHighToLow_ENUM").c_str();
            items[3] = Translation::Get("$JunkIt_ValueLowToHigh_ENUM").c_str();
            items[4] = Translation::Get("$JunkIt_ValueWeightHighToLow_ENUM").c_str();
            items[5] = Translation::Get("$JunkIt_ValueWeightLowToHigh_ENUM").c_str();
            items[6] = Translation::Get("$JunkIt_Chaos_ENUM").c_str();
            return items;
        }

        void RenderGeneral() {
            PushBrandColors();
            RenderPageHeader("$JunkIt_Page_General");

            ImGui::SeparatorText(Translation::Get("$JunkIt_ConfirmationsHeader").c_str());
            if (BeginSettingsTable("generalConfirm")) {
                SaveIfChanged(CheckboxRow("$JunkIt_ConfirmTransferToggle", "$JunkIt_ConfirmTransferToggle", Settings::ConfirmTransferValue()));
                SaveIfChanged(CheckboxRow("$JunkIt_ConfirmSellToggle", "$JunkIt_ConfirmSellToggle", Settings::ConfirmSellValue()));
                ImGui::EndTable();
            }

            ImGui::SeparatorText(Translation::Get("$JunkIt_ProtectionsHeader").c_str());
            if (BeginSettingsTable("generalProtect")) {
                SaveIfChanged(CheckboxRow("$JunkIt_ProtectEquippedToggle", "$JunkIt_ProtectEquippedToggle_Help", Settings::ProtectEquippedValue()));
                SaveIfChanged(CheckboxRow("$JunkIt_ProtectFavoritesToggle", "$JunkIt_ProtectFavoritesToggle_Help", Settings::ProtectFavoritesValue()));
                SaveIfChanged(CheckboxRow("$JunkIt_ProtectEnchantedToggle", "$JunkIt_ProtectEnchantedToggle_Help", Settings::ProtectEnchantedValue()));
                ImGui::EndTable();
            }

            ImGui::SeparatorText(Translation::Get("$JunkIt_NotificationsHeader").c_str());
            if (BeginSettingsTable("generalNotify")) {
                SaveIfChanged(CheckboxRow("$JunkIt_NotifyOnMarkUnmark", "$JunkIt_NotifyOnMarkUnmark_Help", Settings::NotifyOnMarkUnmarkValue()));
                SaveIfChanged(CheckboxRow("$JunkIt_NotifyOnJunkTransfer", "$JunkIt_NotifyOnJunkTransfer_Help", Settings::NotifyOnJunkTransferValue()));
                SaveIfChanged(CheckboxRow("$JunkIt_NotifyOnJunkSell", "$JunkIt_NotifyOnJunkSell_Help", Settings::NotifyOnJunkSellValue()));
                ImGui::EndTable();
            }

            ImGui::SeparatorText(Translation::Get("$JunkIt_PriorityHeader").c_str());
            if (BeginSettingsTable("generalPriority")) {
                const char* const* sortItems = SortPriorityItems();
                SaveIfChanged(ComboRow("$JunkIt_TransferPriority", "$JunkIt_TransferPriority_Help", Settings::TransferPriorityValue(), sortItems, 7));
                SaveIfChanged(ComboRow("$JunkIt_SellPriority", "$JunkIt_SellPriority_Help", Settings::SellPriorityValue(), sortItems, 7));
                ImGui::EndTable();
            }

            RenderStatus();
            PopBrandColors();
        }

        void RenderHotkeys() {
            PushBrandColors();
            RenderPageHeader("$JunkIt_Page_Hotkeys");

            ImGui::SeparatorText(Translation::Get("$JunkIt_HotkeyHeader").c_str());
            if (BeginSettingsTable("hotkeysKeyboard")) {
                KeyBindRow("$JunkIt_Text_Hotkey", "$JunkIt_Help_Hotkey", Settings::MarkJunkKeyValue(), CaptureSlot::kMark);
                KeyBindRow("$JunkIt_Transfer_Hotkey", "$JunkIt_Transfer_Hotkey_Help", Settings::TransferJunkKeyValue(), CaptureSlot::kTransfer);
                ImGui::EndTable();
            }

            ImGui::SeparatorText(Translation::Get("$JunkIt_GamepadHotkeyHeader").c_str());
            if (BeginSettingsTable("hotkeysGamepad")) {
                KeyBindRow("$JunkIt_GamepadJunkKey", "$JunkIt_GamepadJunkKey_Help", Settings::GamepadJunkKeyValue(), CaptureSlot::kGamepad);
                SaveIfChanged(SliderIntRow(
                    "$JunkIt_GamepadTransferHoldTime",
                    "$JunkIt_GamepadTransferHoldTime_Help",
                    Settings::GamepadTransferHoldTimeValue(),
                    2,
                    30));
                ImGui::EndTable();
            }

            RenderStatus();
            PopBrandColors();
        }

        void RenderIntegrations() {
            PushBrandColors();
            RenderPageHeader("$JunkIt_Page_Integrations");

            ImGui::SeparatorText(Translation::Get("$JunkIt_I4IntegrationHeader").c_str());
            if (BeginSettingsTable("i4")) {
                bool iconChanged = CheckboxRow("$JunkIt_UpdateItemIcon", "$JunkIt_UpdateItemIcon_Help", Settings::UpdateItemIconValue());
                bool typeChanged = CheckboxRow("$JunkIt_UpdateSubTypeDisplay", "$JunkIt_UpdateSubTypeDisplay_Help", Settings::UpdateSubTypeDisplayValue());
                ImGui::EndTable();
                if (iconChanged || typeChanged) {
                    SaveSettings();
                    UIUtil::ItemList::Refresh();
                }
            }

            ImGui::SeparatorText(Translation::Get("$JunkIt_DIIIIntegrationHeader").c_str());
            if (BeginSettingsTable("diii")) {
                ImGui::BeginDisabled(!Settings::IsDIIIInstalled());
                const bool diiiChanged = CheckboxRow(
                    "$JunkIt_UseDynamicInventoryIcon",
                    "$JunkIt_UseDynamicInventoryIcon_Help",
                    Settings::UseDynamicInventoryIconValue());
                ImGui::EndDisabled();
                ImGui::EndTable();
                if (!Settings::IsDIIIInstalled()) {
                    ImGui::TextWrapped("%s", Translation::Get("$JunkIt_DIIINotInstalled").c_str());
                }
                if (diiiChanged) {
                    Settings::ApplyIntegrationGuards();
                    SaveSettings();
                    UIUtil::ItemList::Refresh();
                }
            }

            ImGui::SeparatorText(Translation::Get("$JunkIt_SkyPromptIntegrationHeader").c_str());
            ImGui::TextWrapped("%s", Translation::Get(
                Settings::IsSkyPromptInstalled() ? "$JunkIt_SkyPromptInstalled" : "$JunkIt_SkyPromptNotInstalled"
            ).c_str());

            RenderStatus();
            PopBrandColors();
        }

        void RenderJunkList() {
            PushBrandColors();
            RenderPageHeader("$JunkIt_JunkList_Page");

            auto& manager = JunkDataManager::GetSingleton();
            ImGui::Text("%s (%zu)", Translation::Get("$JunkIt_JunkList_ListHeader").c_str(), manager.Size());
            ImGui::Spacing();

            if (IconButton(kIconExport, "$JunkIt_SaveJunkListToFile")) {
                const bool saved = manager.SaveToFile();
                SetStatus(saved ? "$JunkIt_JunkSaved" : "$JunkIt_ExportFailed", saved);
            }
            HelpMarker("$JunkIt_SaveJunkListToFile_Help");
            ImGui::SameLine();
            if (IconButton(kIconImport, "$JunkIt_LoadJunkListFromFile")) {
                if (manager.LoadFromFile(Settings::GetReplaceJunkListOnLoad())) {
                    SetStatus(
                        Settings::GetReplaceJunkListOnLoad() ? "$JunkIt_JunkReplaced" : "$JunkIt_JunkLoaded",
                        true);
                    UIUtil::ItemList::Refresh();
                } else {
                    SetStatus("$JunkIt_ImportFailed", false);
                }
            }
            HelpMarker("$JunkIt_LoadJunkListFromFile_Help");
            ImGui::SameLine();
            if (IconButton(kIconTrash, "$JunkIt_ResetJunk")) {
                OpenConfirmPopup("ResetJunk", "$JunkIt_ResetJunk");
            }
            HelpMarker("$JunkIt_ResetJunk_Help");

            if (ConfirmPopup("ResetJunk", "$JunkIt_ResetJunk", "$JunkIt_ResetJunkConfirm")) {
                manager.Clear();
                UIUtil::ItemList::Refresh();
                SetStatus("$JunkIt_JunkReset", true);
            }

            ImGui::Spacing();
            if (BeginSettingsTable("junkListOptions")) {
                SaveIfChanged(CheckboxRow(
                    "$JunkIt_ReplaceJunkListOnLoad",
                    "$JunkIt_ReplaceJunkListOnLoad_Help",
                    Settings::ReplaceJunkListOnLoadValue()));
                ImGui::EndTable();
            }

            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputTextWithHint("##junkFilter", Translation::Get("$JunkIt_SearchJunkList").c_str(), g_junkFilter, sizeof(g_junkFilter));
            RenderStatus();

            const auto items = manager.GetAllJunkItems();
            if (items.empty()) {
                ImGui::TextWrapped("%s", Translation::Get("$JunkIt_EmptyJunkList").c_str());
                PopBrandColors();
                return;
            }

            const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
            if (ImGui::BeginTable("junkList", 2, flags, ImVec2(0.0f, 0.0f))) {
                ImGui::TableSetupColumn(Translation::Get("$JunkIt_JunkList_ListHeader").c_str(), ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn(Translation::Get("$JunkIt_Remove").c_str(), ImGuiTableColumnFlags_WidthFixed, 140.0f);
                ImGui::TableHeadersRow();

                for (std::int32_t index = 0; index < static_cast<std::int32_t>(items.size()); ++index) {
                    const auto& item = items[index];
                    if (g_junkFilter[0] && !Util::String::iContains(item.displayName, g_junkFilter)) {
                        continue;
                    }

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::AlignTextToFramePadding();
                    ImGui::TextUnformatted(item.displayName.c_str());

                    ImGui::TableNextColumn();
                    ImGui::PushID(index);
                    if (IconButton(kIconRemove, "$JunkIt_Remove")) {
                        if (manager.RemoveJunkItemAtIndex(index)) {
                            UIUtil::ItemList::Refresh();
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }

            PopBrandColors();
        }

        void RenderAdvanced() {
            PushBrandColors();
            RenderPageHeader("$JunkIt_Page_Advanced");

            ImGui::SeparatorText(Translation::Get("$AutoloadJunkList_Header").c_str());
            if (BeginSettingsTable("advancedAuto")) {
                SaveIfChanged(CheckboxRow(
                    "$JunkIt_AutoSaveJunkListToFile",
                    "$JunkIt_AutoSaveJunkListToFile_Help",
                    Settings::AutoExportValue()));
                SaveIfChanged(CheckboxRow(
                    "$JunkIt_AutoLoadJunkListFromFile",
                    "$JunkIt_AutoLoadJunkListFromFile_Help",
                    Settings::AutoImportValue()));
                ImGui::EndTable();
            }

            if (ImGui::CollapsingHeader(Translation::Get("$JunkIt_HeavyLoadHeader").c_str())) {
                if (BeginSettingsTable("advancedHeavy")) {
                    SaveIfChanged(SliderFloatRow(
                        "$JunkIt_HeavyLoadDelayMultiplier",
                        "$JunkIt_HeavyLoadDelayMultiplier_Help",
                        Settings::HeavyLoadDelayMultiplierValue(),
                        0.5f,
                        5.0f,
                        "%.1f"));
                    SaveIfChanged(SliderIntRow(
                        "$JunkIt_LargeUniqueTypes",
                        "$JunkIt_LargeUniqueTypes_Help",
                        Settings::LargeUniqueTypesValue(),
                        1,
                        500));
                    SaveIfChanged(SliderIntRow(
                        "$JunkIt_LargeTotalItems",
                        "$JunkIt_LargeTotalItems_Help",
                        Settings::LargeTotalItemsValue(),
                        1,
                        1000));
                    ImGui::EndTable();
                }
            }

            if (ImGui::CollapsingHeader(Translation::Get("$JunkIt_UtilityHeader").c_str())) {
                if (BeginSettingsTable("advancedRefresh")) {
                    SaveIfChanged(CheckboxRow(
                        "$JunkIt_AggressiveRefresh",
                        "$JunkIt_AggressiveRefresh_Help",
                        Settings::AggressiveRefreshValue()));
                    SaveIfChanged(SliderIntRow(
                        "$JunkIt_AggressiveRefreshMaxInterval",
                        "$JunkIt_AggressiveRefreshMaxInterval_Help",
                        Settings::AggressiveRefreshMaxIntervalValue(),
                        1,
                        60));
                    ImGui::EndTable();
                }
            }

            ImGui::Spacing();
            if (IconButton(kIconSync, "$JunkIt_ReloadSettings")) {
                Settings::LoadFromIni();
                SetStatus("$JunkIt_SettingsReloaded", true);
            }
            HelpMarker("$JunkIt_ReloadSettings_Help");
            ImGui::SameLine();
            if (IconButton(kIconTrash, "$ResetSettings")) {
                OpenConfirmPopup("ResetSettings", "$ResetSettings");
            }
            HelpMarker("$ResetSettingsMaintenance_Help");

            if (ConfirmPopup("ResetSettings", "$ResetSettings", "$JunkIt_ResetSettingsConfirm")) {
                Settings::ResetToDefaults();
                if (Settings::SaveToIni()) {
                    SetStatus("$JunkIt_SettingsReset", true);
                } else {
                    SetStatus("$JunkIt_SettingsSaveFailed", false);
                }
            }

            RenderStatus();
            PopBrandColors();
        }
    }

    void UI::Register() {
        static bool registered = false;
        if (registered) {
            return;
        }

        if (!SKSEMenuFramework::IsInstalled()) {
            SKSE::log::error("SKSE Menu Framework is not installed; Junk It settings menu will be unavailable");
            return;
        }

        SKSEMenuFramework::SetSection("Junk It");
        SKSEMenuFramework::AddSectionItem(Translation::Get("$JunkIt_Page_General"), RenderGeneral);
        SKSEMenuFramework::AddSectionItem(Translation::Get("$JunkIt_Page_Hotkeys"), RenderHotkeys);
        SKSEMenuFramework::AddSectionItem(Translation::Get("$JunkIt_Page_Integrations"), RenderIntegrations);
        SKSEMenuFramework::AddSectionItem(Translation::Get("$JunkIt_JunkList_Page"), RenderJunkList);
        SKSEMenuFramework::AddSectionItem(Translation::Get("$JunkIt_Page_Advanced"), RenderAdvanced);
        registered = true;
        SKSE::log::info("Registered SKSE Menu Framework pages");
    }

    bool UI::ConsumeKeyCapture(std::uint32_t keyCode) {
        if (g_capture == CaptureSlot::kNone) {
            return false;
        }

        if (keyCode == 1) {
            g_capture = CaptureSlot::kNone;
            return true;
        }

        switch (g_capture) {
            case CaptureSlot::kMark:
                Settings::MarkJunkKeyValue() = keyCode;
                break;
            case CaptureSlot::kTransfer:
                Settings::TransferJunkKeyValue() = keyCode;
                break;
            case CaptureSlot::kGamepad:
                Settings::GamepadJunkKeyValue() = keyCode;
                break;
            default:
                break;
        }

        SaveSettings();
        g_capture = CaptureSlot::kNone;
        SkyPromptIntegration::GetSingleton().RefreshPrompts();
        return true;
    }
}
