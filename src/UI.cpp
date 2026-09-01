#include "UI.h"

#include "AutoJunk.h"
#include "JunkData.h"
#include "SkyPromptIntegration.h"
#include "Translation.h"
#include "junk.h"
#include "settings.h"
#include "util.h"

#define ImGui SKSEMenuImGui
#include "SKSEMenuFramework.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace JunkIt {
    namespace {
        enum class CaptureSlot {
            kNone = 0,
            kMark = 1,
            kTransfer = 2,
            kGamepad = 3,
            kTrash = 4
        };

        enum class StatusKind {
            kNone = 0,
            kSuccess = 1,
            kFailure = 2
        };

        CaptureSlot g_capture = CaptureSlot::kNone;
        bool g_captureWaitMouseUp = false;
        bool g_prevKeyCtrl = false;
        bool g_prevKeyShift = false;
        bool g_prevKeyAlt = false;
        bool g_prevKeySuper = false;
        char g_junkFilter[128] = {};
        char g_customAutoJunkType[64] = {};
        char g_customAutoJunkMaterial[64] = {};
        char g_customAutoJunkKeyword[64] = {};
        int g_knownAutoJunkIndex = 0;
        int g_knownAutoJunkMaterialIndex = 0;
        bool g_showAutoJunkExclusions = false;
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
        constexpr unsigned kIconSwitch = 0xf362;

        const char* KeyName(std::uint32_t keyCode) {
            if (keyCode == 0) {
                return Translation::Get("$JunkIt_Unbound").c_str();
            }
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

        std::uint32_t ImGuiKeyToSkyrimKeycode(ImGuiKey key) {
            using KeyboardKey = RE::BSKeyboardDevice::Key;
            using GamepadOffset = KeyUtil::GAMEPAD_OFFSETS;
            constexpr auto kMouseOffset = static_cast<std::uint32_t>(KeyUtil::KBM_OFFSETS::kMacro_MouseButtonOffset);

            switch (key) {
                case ImGuiKey_Tab: return KeyboardKey::kTab;
                case ImGuiKey_LeftArrow: return KeyboardKey::kLeft;
                case ImGuiKey_RightArrow: return KeyboardKey::kRight;
                case ImGuiKey_UpArrow: return KeyboardKey::kUp;
                case ImGuiKey_DownArrow: return KeyboardKey::kDown;
                case ImGuiKey_PageUp: return KeyboardKey::kPageUp;
                case ImGuiKey_PageDown: return KeyboardKey::kPageDown;
                case ImGuiKey_Home: return KeyboardKey::kHome;
                case ImGuiKey_End: return KeyboardKey::kEnd;
                case ImGuiKey_Insert: return KeyboardKey::kInsert;
                case ImGuiKey_Delete: return KeyboardKey::kDelete;
                case ImGuiKey_Backspace: return KeyboardKey::kBackspace;
                case ImGuiKey_Space: return KeyboardKey::kSpacebar;
                case ImGuiKey_Enter: return KeyboardKey::kEnter;
                case ImGuiKey_Escape: return KeyboardKey::kEscape;
                case ImGuiKey_LeftCtrl: return KeyboardKey::kLeftControl;
                case ImGuiKey_LeftShift: return KeyboardKey::kLeftShift;
                case ImGuiKey_LeftAlt: return KeyboardKey::kLeftAlt;
                case ImGuiKey_LeftSuper: return KeyboardKey::kLeftWin;
                case ImGuiKey_RightCtrl: return KeyboardKey::kRightControl;
                case ImGuiKey_RightShift: return KeyboardKey::kRightShift;
                case ImGuiKey_RightAlt: return KeyboardKey::kRightAlt;
                case ImGuiKey_RightSuper: return KeyboardKey::kRightWin;
                case ImGuiKey_0: return KeyboardKey::kNum0;
                case ImGuiKey_1: return KeyboardKey::kNum1;
                case ImGuiKey_2: return KeyboardKey::kNum2;
                case ImGuiKey_3: return KeyboardKey::kNum3;
                case ImGuiKey_4: return KeyboardKey::kNum4;
                case ImGuiKey_5: return KeyboardKey::kNum5;
                case ImGuiKey_6: return KeyboardKey::kNum6;
                case ImGuiKey_7: return KeyboardKey::kNum7;
                case ImGuiKey_8: return KeyboardKey::kNum8;
                case ImGuiKey_9: return KeyboardKey::kNum9;
                case ImGuiKey_A: return KeyboardKey::kA;
                case ImGuiKey_B: return KeyboardKey::kB;
                case ImGuiKey_C: return KeyboardKey::kC;
                case ImGuiKey_D: return KeyboardKey::kD;
                case ImGuiKey_E: return KeyboardKey::kE;
                case ImGuiKey_F: return KeyboardKey::kF;
                case ImGuiKey_G: return KeyboardKey::kG;
                case ImGuiKey_H: return KeyboardKey::kH;
                case ImGuiKey_I: return KeyboardKey::kI;
                case ImGuiKey_J: return KeyboardKey::kJ;
                case ImGuiKey_K: return KeyboardKey::kK;
                case ImGuiKey_L: return KeyboardKey::kL;
                case ImGuiKey_M: return KeyboardKey::kM;
                case ImGuiKey_N: return KeyboardKey::kN;
                case ImGuiKey_O: return KeyboardKey::kO;
                case ImGuiKey_P: return KeyboardKey::kP;
                case ImGuiKey_Q: return KeyboardKey::kQ;
                case ImGuiKey_R: return KeyboardKey::kR;
                case ImGuiKey_S: return KeyboardKey::kS;
                case ImGuiKey_T: return KeyboardKey::kT;
                case ImGuiKey_U: return KeyboardKey::kU;
                case ImGuiKey_V: return KeyboardKey::kV;
                case ImGuiKey_W: return KeyboardKey::kW;
                case ImGuiKey_X: return KeyboardKey::kX;
                case ImGuiKey_Y: return KeyboardKey::kY;
                case ImGuiKey_Z: return KeyboardKey::kZ;
                case ImGuiKey_F1: return KeyboardKey::kF1;
                case ImGuiKey_F2: return KeyboardKey::kF2;
                case ImGuiKey_F3: return KeyboardKey::kF3;
                case ImGuiKey_F4: return KeyboardKey::kF4;
                case ImGuiKey_F5: return KeyboardKey::kF5;
                case ImGuiKey_F6: return KeyboardKey::kF6;
                case ImGuiKey_F7: return KeyboardKey::kF7;
                case ImGuiKey_F8: return KeyboardKey::kF8;
                case ImGuiKey_F9: return KeyboardKey::kF9;
                case ImGuiKey_F10: return KeyboardKey::kF10;
                case ImGuiKey_F11: return KeyboardKey::kF11;
                case ImGuiKey_F12: return KeyboardKey::kF12;
                case ImGuiKey_Apostrophe: return KeyboardKey::kApostrophe;
                case ImGuiKey_Comma: return KeyboardKey::kComma;
                case ImGuiKey_Minus: return KeyboardKey::kMinus;
                case ImGuiKey_Period: return KeyboardKey::kPeriod;
                case ImGuiKey_Slash: return KeyboardKey::kSlash;
                case ImGuiKey_Semicolon: return KeyboardKey::kSemicolon;
                case ImGuiKey_Equal: return KeyboardKey::kEquals;
                case ImGuiKey_LeftBracket: return KeyboardKey::kBracketLeft;
                case ImGuiKey_Backslash: return KeyboardKey::kBackslash;
                case ImGuiKey_RightBracket: return KeyboardKey::kBracketRight;
                case ImGuiKey_GraveAccent: return KeyboardKey::kTilde;
                case ImGuiKey_CapsLock: return KeyboardKey::kCapsLock;
                case ImGuiKey_ScrollLock: return KeyboardKey::kScrollLock;
                case ImGuiKey_NumLock: return KeyboardKey::kNumLock;
                case ImGuiKey_PrintScreen: return KeyboardKey::kPrintScreen;
                case ImGuiKey_Pause: return KeyboardKey::kPause;
                case ImGuiKey_Keypad0: return KeyboardKey::kKP_0;
                case ImGuiKey_Keypad1: return KeyboardKey::kKP_1;
                case ImGuiKey_Keypad2: return KeyboardKey::kKP_2;
                case ImGuiKey_Keypad3: return KeyboardKey::kKP_3;
                case ImGuiKey_Keypad4: return KeyboardKey::kKP_4;
                case ImGuiKey_Keypad5: return KeyboardKey::kKP_5;
                case ImGuiKey_Keypad6: return KeyboardKey::kKP_6;
                case ImGuiKey_Keypad7: return KeyboardKey::kKP_7;
                case ImGuiKey_Keypad8: return KeyboardKey::kKP_8;
                case ImGuiKey_Keypad9: return KeyboardKey::kKP_9;
                case ImGuiKey_KeypadDecimal: return KeyboardKey::kKP_Decimal;
                case ImGuiKey_KeypadDivide: return KeyboardKey::kKP_Divide;
                case ImGuiKey_KeypadMultiply: return KeyboardKey::kKP_Multiply;
                case ImGuiKey_KeypadSubtract: return KeyboardKey::kKP_Subtract;
                case ImGuiKey_KeypadAdd: return KeyboardKey::kKP_Plus;
                case ImGuiKey_KeypadEnter: return KeyboardKey::kKP_Enter;
                case ImGuiKey_MouseLeft: return kMouseOffset + 0;
                case ImGuiKey_MouseRight: return kMouseOffset + 1;
                case ImGuiKey_MouseMiddle: return kMouseOffset + 2;
                case ImGuiKey_MouseX1: return kMouseOffset + 3;
                case ImGuiKey_MouseX2: return kMouseOffset + 4;
                case ImGuiKey_GamepadDpadUp: return static_cast<std::uint32_t>(GamepadOffset::kGamepadButtonOffset_DPAD_UP);
                case ImGuiKey_GamepadDpadDown: return static_cast<std::uint32_t>(GamepadOffset::kGamepadButtonOffset_DPAD_DOWN);
                case ImGuiKey_GamepadDpadLeft: return static_cast<std::uint32_t>(GamepadOffset::kGamepadButtonOffset_DPAD_LEFT);
                case ImGuiKey_GamepadDpadRight: return static_cast<std::uint32_t>(GamepadOffset::kGamepadButtonOffset_DPAD_RIGHT);
                case ImGuiKey_GamepadStart: return static_cast<std::uint32_t>(GamepadOffset::kGamepadButtonOffset_START);
                case ImGuiKey_GamepadBack: return static_cast<std::uint32_t>(GamepadOffset::kGamepadButtonOffset_BACK);
                case ImGuiKey_GamepadL3: return static_cast<std::uint32_t>(GamepadOffset::kGamepadButtonOffset_LEFT_THUMB);
                case ImGuiKey_GamepadR3: return static_cast<std::uint32_t>(GamepadOffset::kGamepadButtonOffset_RIGHT_THUMB);
                case ImGuiKey_GamepadL1: return static_cast<std::uint32_t>(GamepadOffset::kGamepadButtonOffset_LEFT_SHOULDER);
                case ImGuiKey_GamepadR1: return static_cast<std::uint32_t>(GamepadOffset::kGamepadButtonOffset_RIGHT_SHOULDER);
                case ImGuiKey_GamepadFaceDown: return static_cast<std::uint32_t>(GamepadOffset::kGamepadButtonOffset_A);
                case ImGuiKey_GamepadFaceRight: return static_cast<std::uint32_t>(GamepadOffset::kGamepadButtonOffset_B);
                case ImGuiKey_GamepadFaceLeft: return static_cast<std::uint32_t>(GamepadOffset::kGamepadButtonOffset_X);
                case ImGuiKey_GamepadFaceUp: return static_cast<std::uint32_t>(GamepadOffset::kGamepadButtonOffset_Y);
                default: return 0;
            }
        }

        bool TryConsumeImGuiKey(ImGuiKey key) {
            if (!ImGui::IsKeyPressed(key, false)) {
                return false;
            }
            const std::uint32_t keyCode = ImGuiKeyToSkyrimKeycode(key);
            if (keyCode == 0) {
                return false;
            }
            UI::ConsumeKeyCapture(keyCode);
            return true;
        }

        void PollKeyCapture() {
            ImGuiIO* io = ImGui::GetIO();
            const bool ctrl = io && io->KeyCtrl;
            const bool shift = io && io->KeyShift;
            const bool alt = io && io->KeyAlt;
            const bool super = io && io->KeySuper;

            auto storeModEdges = [&]() {
                g_prevKeyCtrl = ctrl;
                g_prevKeyShift = shift;
                g_prevKeyAlt = alt;
                g_prevKeySuper = super;
            };

            if (g_capture == CaptureSlot::kNone) {
                g_captureWaitMouseUp = false;
                storeModEdges();
                return;
            }

            if (g_captureWaitMouseUp) {
                if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                    g_captureWaitMouseUp = false;
                }
                storeModEdges();
                return;
            }

            if (ctrl && !g_prevKeyCtrl) {
                storeModEdges();
                UI::ConsumeKeyCapture(RE::BSKeyboardDevice::Key::kLeftControl);
                return;
            }
            if (shift && !g_prevKeyShift) {
                storeModEdges();
                UI::ConsumeKeyCapture(RE::BSKeyboardDevice::Key::kLeftShift);
                return;
            }
            if (alt && !g_prevKeyAlt) {
                storeModEdges();
                UI::ConsumeKeyCapture(RE::BSKeyboardDevice::Key::kLeftAlt);
                return;
            }
            if (super && !g_prevKeySuper) {
                storeModEdges();
                UI::ConsumeKeyCapture(RE::BSKeyboardDevice::Key::kLeftWin);
                return;
            }

            storeModEdges();

            for (int key = ImGuiKey_NamedKey_BEGIN; key < ImGuiKey_NamedKey_END; ++key) {
                if (TryConsumeImGuiKey(static_cast<ImGuiKey>(key))) {
                    return;
                }
            }
        }

        void HelpMarker(const char* helpKey) {
            if (!helpKey || !*helpKey) {
                return;
            }

            ImGui::SameLine();
            ImGui::PushID(helpKey);
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered(
                    ImGuiHoveredFlags_DelayNormal | ImGuiHoveredFlags_Stationary | ImGuiHoveredFlags_AllowWhenDisabled)) {
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
                ImGui::PushID(labelKey);
                HelpMarker(helpKey);
                ImGui::PopID();
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
                g_captureWaitMouseUp = true;
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

        void CenterIconButtonGroup(unsigned codepoint, const char* labelKey) {
            const std::string label = FontAwesome::UnicodeToUtf8(codepoint) + "  " + Translation::Get(labelKey);
            FontAwesome::PushSolid();
            ImVec2 textSize{};
            ImGui::CalcTextSize(&textSize, label.c_str(), nullptr, false, -1.0f);
            FontAwesome::Pop();
            const ImGuiStyle* style = ImGui::GetStyle();
            const float buttonW = textSize.x + style->FramePadding.x * 2.0f;
            ImVec2 helpSize{};
            ImGui::CalcTextSize(&helpSize, "(?)", nullptr, false, -1.0f);
            const float groupW = buttonW + style->ItemSpacing.x + helpSize.x;
            ImVec2 avail{};
            ImGui::GetContentRegionAvail(&avail);
            if (avail.x > groupW) {
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail.x - groupW) * 0.5f);
            }
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

        const char* const* SkyPromptPlacementItems() {
            static const char* items[2];
            items[0] = Translation::Get("$JunkIt_SkyPromptPlacement_Attach_ENUM").c_str();
            items[1] = Translation::Get("$JunkIt_SkyPromptPlacement_LowerRight_ENUM").c_str();
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

        void RenderTrash() {
            PushBrandColors();
            RenderPageHeader("$JunkIt_Page_Trash");

            ImVec2 avail{};
            ImGui::GetContentRegionAvail(&avail);
            const float leftWidth = avail.x * 0.52f;

            const bool trashResolved = Settings::GetTrashContainer() != nullptr;
            const bool trashAvailable = Settings::IsTrashAvailable();
            const bool worldReady = JunkHandler::IsGameWorldReady();
            const auto itemCount = JunkHandler::GetTrashItemCount();

            ImGui::BeginChild(
                "trashLeft",
                ImVec2(leftWidth, 0.0f),
                ImGuiChildFlags_Border,
                ImGuiWindowFlags_AlwaysVerticalScrollbar);

            if (!trashResolved) {
                ImGui::TextWrapped("%s", Translation::Get("$JunkIt_TrashRequiresEsp").c_str());
                ImGui::BeginDisabled();
            }
            if (BeginSettingsTable("trashSettings")) {
                const bool enableChanged = CheckboxRow(
                    "$JunkIt_EnableTrash",
                    "$JunkIt_EnableTrash_Help",
                    Settings::EnableTrashValue());
                SaveIfChanged(enableChanged);
                if (enableChanged) {
                    SkyPromptIntegration::GetSingleton().RefreshPrompts();
                }
                ImGui::BeginDisabled(!trashAvailable);
                const char* expireHelp = !trashResolved
                    ? "$JunkIt_TrashRequiresEsp"
                    : (trashAvailable ? "$JunkIt_TrashExpireDays_Help" : "$JunkIt_EnableTrash_Help");
                SaveIfChanged(SliderIntRow(
                    "$JunkIt_TrashExpireDays",
                    expireHelp,
                    Settings::TrashExpireDaysValue(),
                    0,
                    30));
                ImGui::EndDisabled();
                ImGui::EndTable();
            }

            ImGui::Dummy(ImVec2(0.0f, 12.0f));
            const bool emptyDisabled = !trashResolved || !worldReady || !itemCount || *itemCount <= 0;
            if (emptyDisabled) {
                ImGui::BeginDisabled();
            }
            CenterIconButtonGroup(kIconTrash, "$JunkIt_EmptyTrashBin");
            const bool emptyClicked = IconButton(kIconTrash, "$JunkIt_EmptyTrashBin");
            const char* emptyHelp = !trashResolved
                ? "$JunkIt_TrashRequiresEsp"
                : (worldReady ? "$JunkIt_EmptyTrashBin_Help" : "$JunkIt_EmptyTrashBin_MainMenu");
            HelpMarker(emptyHelp);
            if (emptyDisabled) {
                ImGui::EndDisabled();
            }
            if (!trashResolved) {
                ImGui::EndDisabled();
            }

            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("trashRight", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Border);
            ImGui::SeparatorText(Translation::Get("$JunkIt_TrashInfoHeader").c_str());
            const auto daysRemaining = JunkHandler::GetTrashDaysRemaining();
            const std::string& na = Translation::Get("$JunkIt_TrashInfoNA");
            const std::string daysText = daysRemaining
                ? fmt::format("{:.1f}", *daysRemaining)
                : na;
            const std::string itemsText = itemCount
                ? fmt::format("{}", *itemCount)
                : na;
            if (BeginSettingsTable("trashInfo")) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(Translation::Get("$JunkIt_TrashInfoDays").c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(daysText.c_str());
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(Translation::Get("$JunkIt_TrashInfoItems").c_str());
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(itemsText.c_str());
                ImGui::EndTable();
            }

            ImGui::Dummy(ImVec2(0.0f, 12.0f));
            const bool openDisabled = !trashResolved || !worldReady;
            if (openDisabled) {
                ImGui::BeginDisabled();
            }
            CenterIconButtonGroup(kIconTrash, "$JunkIt_OpenTrash");
            if (IconButton(kIconTrash, "$JunkIt_OpenTrash")) {
                JunkHandler::OpenTrashContainer();
            }
            const char* openHelp = !trashResolved
                ? "$JunkIt_TrashRequiresEsp"
                : (worldReady ? "$JunkIt_OpenTrash_Help" : "$JunkIt_OpenTrash_MainMenu");
            HelpMarker(openHelp);
            if (openDisabled) {
                ImGui::EndDisabled();
            }
            ImGui::EndChild();

            if (emptyClicked) {
                OpenConfirmPopup("EmptyTrashBin", "$JunkIt_EmptyTrashBin");
            }
            if (ConfirmPopup("EmptyTrashBin", "$JunkIt_EmptyTrashBin", "$JunkIt_EmptyTrashBinConfirm")) {
                JunkHandler::EmptyTrashBin();
            }

            RenderStatus();
            PopBrandColors();
        }

        void RenderHotkeys() {
            PushBrandColors();
            RenderPageHeader("$JunkIt_Page_Hotkeys");

            const bool trashResolved = Settings::GetTrashContainer() != nullptr;
            const bool trashAvailable = Settings::IsTrashAvailable();
            const char* trashDisabledHelp = !trashResolved
                ? "$JunkIt_TrashRequiresEsp"
                : "$JunkIt_EnableTrash_Help";

            ImGui::SeparatorText(Translation::Get("$JunkIt_HotkeyHeader").c_str());
            if (BeginSettingsTable("hotkeysKeyboard")) {
                KeyBindRow("$JunkIt_Text_Hotkey", "$JunkIt_Help_Hotkey", Settings::MarkJunkKeyValue(), CaptureSlot::kMark);
                ImGui::BeginDisabled(!trashAvailable);
                const char* holdHelp = trashAvailable ? "$JunkIt_TrashHoldSeconds_Help" : trashDisabledHelp;
                const bool holdChanged = SliderIntRow(
                    "$JunkIt_TrashHoldSeconds",
                    holdHelp,
                    Settings::TrashHoldSecondsValue(),
                    0,
                    10);
                SaveIfChanged(holdChanged);
                if (holdChanged) {
                    SkyPromptIntegration::GetSingleton().RefreshPrompts();
                }
                ImGui::EndDisabled();
                KeyBindRow("$JunkIt_Transfer_Hotkey", "$JunkIt_Transfer_Hotkey_Help", Settings::TransferJunkKeyValue(), CaptureSlot::kTransfer);
                ImGui::BeginDisabled(!trashAvailable);
                const char* trashKeyHelp = trashAvailable ? "$JunkIt_TrashJunkKey_Help" : trashDisabledHelp;
                KeyBindRow(
                    "$JunkIt_TrashJunkKey",
                    trashKeyHelp,
                    Settings::TrashJunkKeyValue(),
                    CaptureSlot::kTrash);
                ImGui::EndDisabled();
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
                ImGui::BeginDisabled(!trashAvailable);
                const char* gamepadTrashHelp = trashAvailable
                    ? "$JunkIt_GamepadTrashHoldSeconds_Help"
                    : trashDisabledHelp;
                const bool gamepadTrashChanged = SliderIntRow(
                    "$JunkIt_GamepadTrashHoldSeconds",
                    gamepadTrashHelp,
                    Settings::GamepadTrashHoldSecondsValue(),
                    Settings::GamepadTransferHoldTimeValue(),
                    30);
                SaveIfChanged(gamepadTrashChanged);
                if (gamepadTrashChanged) {
                    SkyPromptIntegration::GetSingleton().RefreshPrompts();
                }
                ImGui::EndDisabled();
                ImGui::EndTable();
            }

            PollKeyCapture();
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
            if (BeginSettingsTable("skyprompt")) {
                ImGui::BeginDisabled(!Settings::IsSkyPromptInstalled());
                const bool enabledChanged = CheckboxRow(
                    "$JunkIt_SkyPromptEnabled",
                    "$JunkIt_SkyPromptEnabled_Help",
                    Settings::SkyPromptEnabledValue());
                ImGui::BeginDisabled(!Settings::SkyPromptEnabledValue());
                const bool placementChanged = ComboRow(
                    "$JunkIt_SkyPromptButtonPlacement",
                    "$JunkIt_SkyPromptButtonPlacement_Help",
                    Settings::SkyPromptButtonPlacementValue(),
                    SkyPromptPlacementItems(),
                    2);
                const bool countsChanged = CheckboxRow(
                    "$JunkIt_SkyPromptShowCounts",
                    "$JunkIt_SkyPromptShowCounts_Help",
                    Settings::SkyPromptShowCountsValue());
                ImGui::EndDisabled();
                ImGui::EndDisabled();
                ImGui::EndTable();
                if (enabledChanged || placementChanged || countsChanged) {
                    SaveSettings();
                    SkyPromptIntegration::GetSingleton().RefreshPrompts();
                }
            }

            RenderStatus();
            PopBrandColors();
        }

        bool ListContainsInsensitive(const std::vector<std::string>& list, std::string_view value) {
            for (const auto& existing : list) {
                if (Util::String::iEquals(existing, value)) {
                    return true;
                }
            }
            return false;
        }

        bool TypeAlreadyAdded(std::string_view type) {
            return ListContainsInsensitive(Settings::GetAutoJunkTypes(), type);
        }

        bool MaterialAlreadyAdded(std::string_view material) {
            return ListContainsInsensitive(Settings::GetAutoJunkMaterials(), material);
        }

        bool AddAutoJunkTypeFromUi(std::string_view type) {
            if (!Settings::TryAddAutoJunkType(type)) {
                return false;
            }
            SaveSettings();
            AutoJunk::ApplyToPlayerInventory();
            UIUtil::ItemList::Refresh();
            return true;
        }

        bool AddAutoJunkMaterialFromUi(std::string_view material) {
            if (!Settings::TryAddAutoJunkMaterial(material)) {
                return false;
            }
            SaveSettings();
            AutoJunk::ApplyToPlayerInventory();
            UIUtil::ItemList::Refresh();
            return true;
        }

        bool AddAutoJunkKeywordFromUi(std::string_view keyword) {
            if (!Settings::TryAddAutoJunkKeyword(keyword)) {
                return false;
            }
            SaveSettings();
            AutoJunk::ApplyToPlayerInventory();
            UIUtil::ItemList::Refresh();
            return true;
        }

        void RenderAutoJunkList(
            const char* tableId,
            const char* headerKey,
            const char* emptyKey,
            const std::vector<std::string>& values,
            bool (*removeAt)(std::size_t)) {
            ImGui::SeparatorText(Translation::Get(headerKey).c_str());
            if (values.empty()) {
                ImGui::TextWrapped("%s", Translation::Get(emptyKey).c_str());
                return;
            }
            const ImGuiTableFlags flags =
                ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp;
            if (!ImGui::BeginTable(tableId, 2, flags, ImVec2(0.0f, 0.0f))) {
                return;
            }
            ImGui::TableSetupColumn(Translation::Get(headerKey).c_str(), ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn(Translation::Get("$JunkIt_Remove").c_str(), ImGuiTableColumnFlags_WidthFixed, 140.0f);
            ImGui::TableHeadersRow();
            for (std::size_t index = 0; index < values.size(); ++index) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(values[index].c_str());
                ImGui::TableNextColumn();
                ImGui::PushID(tableId);
                ImGui::PushID(static_cast<int>(index));
                if (IconButton(kIconRemove, "$JunkIt_Remove")) {
                    if (removeAt(index)) {
                        SaveSettings();
                    }
                }
                ImGui::PopID();
                ImGui::PopID();
            }
            ImGui::EndTable();
        }

        void RenderAutoJunk() {
            PushBrandColors();
            RenderPageHeader("$JunkIt_Page_AutoJunk");

            ImVec2 avail{};
            ImGui::GetContentRegionAvail(&avail);
            const float leftWidth = avail.x * 0.52f;

            ImGui::BeginChild(
                "autoJunkLeft",
                ImVec2(leftWidth, 0.0f),
                ImGuiChildFlags_Border,
                ImGuiWindowFlags_AlwaysVerticalScrollbar);
            ImGui::SeparatorText(Translation::Get("$JunkIt_AutoJunk_TypesHeader").c_str());
            HelpMarker("$JunkIt_AutoJunk_TypesHeader_Help");

            ImGui::TextUnformatted(Translation::Get("$JunkIt_AutoJunk_AddKnown").c_str());
            std::vector<const char*> availableKnown;
            for (const char* type : AutoJunk::KnownItemTypes()) {
                if (!TypeAlreadyAdded(type)) {
                    availableKnown.push_back(type);
                }
            }
            if (g_knownAutoJunkIndex >= static_cast<int>(availableKnown.size())) {
                g_knownAutoJunkIndex = 0;
            }
            ImGui::BeginDisabled(availableKnown.empty());
            ImVec2 knownRowAvail{};
            ImGui::GetContentRegionAvail(&knownRowAvail);
            ImGui::SetNextItemWidth(knownRowAvail.x - 128.0f);
            if (!availableKnown.empty()) {
                ImGui::Combo("##knownAutoJunk", &g_knownAutoJunkIndex, availableKnown.data(), static_cast<int>(availableKnown.size()));
            } else {
                const char* empty = "";
                int unused = 0;
                ImGui::Combo("##knownAutoJunk", &unused, &empty, 0);
            }
            ImGui::SameLine();
            if (ImGui::Button(Translation::Get("$JunkIt_AutoJunk_Add").c_str()) && !availableKnown.empty()) {
                AddAutoJunkTypeFromUi(availableKnown[static_cast<std::size_t>(g_knownAutoJunkIndex)]);
            }
            ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::TextUnformatted(Translation::Get("$JunkIt_AutoJunk_AddCustom").c_str());
            ImVec2 customRowAvail{};
            ImGui::GetContentRegionAvail(&customRowAvail);
            ImGui::SetNextItemWidth(customRowAvail.x - 128.0f);
            ImGui::InputTextWithHint(
                "##customAutoJunk",
                Translation::Get("$JunkIt_AutoJunk_CustomHint").c_str(),
                g_customAutoJunkType,
                sizeof(g_customAutoJunkType));
            ImGui::SameLine();
            if (ImGui::Button((Translation::Get("$JunkIt_AutoJunk_Add") + "##custom").c_str())) {
                if (AddAutoJunkTypeFromUi(g_customAutoJunkType)) {
                    g_customAutoJunkType[0] = '\0';
                }
            }

            ImGui::Spacing();
            ImGui::SeparatorText(Translation::Get("$JunkIt_AutoJunk_MaterialsHeader").c_str());
            HelpMarker("$JunkIt_AutoJunk_MaterialsHeader_Help");

            ImGui::TextUnformatted(Translation::Get("$JunkIt_AutoJunk_AddKnownMaterial").c_str());
            std::vector<const char*> availableMaterials;
            for (const char* material : AutoJunk::KnownItemMaterials()) {
                if (!MaterialAlreadyAdded(material)) {
                    availableMaterials.push_back(material);
                }
            }
            if (g_knownAutoJunkMaterialIndex >= static_cast<int>(availableMaterials.size())) {
                g_knownAutoJunkMaterialIndex = 0;
            }
            ImGui::BeginDisabled(availableMaterials.empty());
            ImVec2 knownMaterialAvail{};
            ImGui::GetContentRegionAvail(&knownMaterialAvail);
            ImGui::SetNextItemWidth(knownMaterialAvail.x - 128.0f);
            if (!availableMaterials.empty()) {
                ImGui::Combo(
                    "##knownAutoJunkMaterial",
                    &g_knownAutoJunkMaterialIndex,
                    availableMaterials.data(),
                    static_cast<int>(availableMaterials.size()));
            } else {
                const char* empty = "";
                int unused = 0;
                ImGui::Combo("##knownAutoJunkMaterial", &unused, &empty, 0);
            }
            ImGui::SameLine();
            if (ImGui::Button((Translation::Get("$JunkIt_AutoJunk_Add") + "##knownMaterial").c_str()) &&
                !availableMaterials.empty()) {
                AddAutoJunkMaterialFromUi(availableMaterials[static_cast<std::size_t>(g_knownAutoJunkMaterialIndex)]);
            }
            ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::TextUnformatted(Translation::Get("$JunkIt_AutoJunk_AddCustomMaterial").c_str());
            ImVec2 customMaterialAvail{};
            ImGui::GetContentRegionAvail(&customMaterialAvail);
            ImGui::SetNextItemWidth(customMaterialAvail.x - 128.0f);
            ImGui::InputTextWithHint(
                "##customAutoJunkMaterial",
                Translation::Get("$JunkIt_AutoJunk_CustomMaterialHint").c_str(),
                g_customAutoJunkMaterial,
                sizeof(g_customAutoJunkMaterial));
            ImGui::SameLine();
            if (ImGui::Button((Translation::Get("$JunkIt_AutoJunk_Add") + "##customMaterial").c_str())) {
                if (AddAutoJunkMaterialFromUi(g_customAutoJunkMaterial)) {
                    g_customAutoJunkMaterial[0] = '\0';
                }
            }

            ImGui::Spacing();
            ImGui::SeparatorText(Translation::Get("$JunkIt_AutoJunk_KeywordsHeader").c_str());
            HelpMarker("$JunkIt_AutoJunk_KeywordsHeader_Help");

            ImGui::TextUnformatted(Translation::Get("$JunkIt_AutoJunk_AddKeyword").c_str());
            ImVec2 customKeywordAvail{};
            ImGui::GetContentRegionAvail(&customKeywordAvail);
            ImGui::SetNextItemWidth(customKeywordAvail.x - 128.0f);
            ImGui::InputTextWithHint(
                "##customAutoJunkKeyword",
                Translation::Get("$JunkIt_AutoJunk_CustomKeywordHint").c_str(),
                g_customAutoJunkKeyword,
                sizeof(g_customAutoJunkKeyword));
            ImGui::SameLine();
            if (ImGui::Button((Translation::Get("$JunkIt_AutoJunk_Add") + "##customKeyword").c_str())) {
                if (AddAutoJunkKeywordFromUi(g_customAutoJunkKeyword)) {
                    g_customAutoJunkKeyword[0] = '\0';
                }
            }

            ImGui::Spacing();
            ImGui::SeparatorText(Translation::Get("$JunkIt_AutoJunk_WhenHeader").c_str());
            if (BeginSettingsTable("autoJunkWhen")) {
                SaveIfChanged(CheckboxRow(
                    "$JunkIt_AutoJunk_OnPickup",
                    "$JunkIt_AutoJunk_OnPickup_Help",
                    Settings::AutoJunkOnPickupValue()));
                SaveIfChanged(CheckboxRow(
                    "$JunkIt_AutoJunk_OnMenuOpen",
                    "$JunkIt_AutoJunk_OnMenuOpen_Help",
                    Settings::AutoJunkOnMenuOpenValue()));
                ImGui::EndTable();
            }
            ImGui::EndChild();

            ImGui::SameLine();
            ImGui::BeginChild("autoJunkRight", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Border);
            ImVec2 rightAvail{};
            ImGui::GetContentRegionAvail(&rightAvail);
            const float thirdHeight = rightAvail.y / 3.0f;
            ImGui::BeginChild("autoJunkRightTypes", ImVec2(0.0f, thirdHeight));
            RenderAutoJunkList(
                "autoJunkTypes",
                "$JunkIt_AutoJunk_ListHeader",
                "$JunkIt_AutoJunk_EmptyList",
                Settings::GetAutoJunkTypes(),
                Settings::RemoveAutoJunkTypeAt);
            ImGui::EndChild();
            ImGui::BeginChild("autoJunkRightMaterials", ImVec2(0.0f, thirdHeight));
            RenderAutoJunkList(
                "autoJunkMaterials",
                "$JunkIt_AutoJunk_MaterialListHeader",
                "$JunkIt_AutoJunk_EmptyMaterialList",
                Settings::GetAutoJunkMaterials(),
                Settings::RemoveAutoJunkMaterialAt);
            ImGui::EndChild();
            ImGui::BeginChild("autoJunkRightKeywords", ImVec2(0.0f, 0.0f));
            RenderAutoJunkList(
                "autoJunkKeywords",
                "$JunkIt_AutoJunk_KeywordListHeader",
                "$JunkIt_AutoJunk_EmptyKeywordList",
                Settings::GetAutoJunkKeywords(),
                Settings::RemoveAutoJunkKeywordAt);
            ImGui::EndChild();
            ImGui::EndChild();

            RenderStatus();
            PopBrandColors();
        }

        void RenderJunkList() {
            PushBrandColors();
            RenderPageHeader("$JunkIt_JunkList_Page");

            auto& manager = JunkDataManager::GetSingleton();
            const char* listHeaderKey = g_showAutoJunkExclusions
                ? "$JunkIt_AutoJunkExclusions_ListHeader"
                : "$JunkIt_JunkList_ListHeader";
            const std::size_t listCount = g_showAutoJunkExclusions ? manager.NoAutoJunkSize() : manager.Size();
            ImGui::Text("%s (%zu)", Translation::Get(listHeaderKey).c_str(), listCount);
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
            if (g_showAutoJunkExclusions) {
                if (IconButton(kIconTrash, "$JunkIt_ResetAutoJunkExclusions")) {
                    OpenConfirmPopup("ResetAutoJunkExclusions", "$JunkIt_ResetAutoJunkExclusions");
                }
                HelpMarker("$JunkIt_ResetAutoJunkExclusions_Help");
            } else {
                if (IconButton(kIconTrash, "$JunkIt_ResetJunk")) {
                    OpenConfirmPopup("ResetJunk", "$JunkIt_ResetJunk");
                }
                HelpMarker("$JunkIt_ResetJunk_Help");
            }
            ImGui::SameLine();
            if (IconButton(
                    kIconSwitch,
                    g_showAutoJunkExclusions ? "$JunkIt_ViewJunkList" : "$JunkIt_ViewAutoJunkExclusions")) {
                g_showAutoJunkExclusions = !g_showAutoJunkExclusions;
            }

            if (ConfirmPopup("ResetJunk", "$JunkIt_ResetJunk", "$JunkIt_ResetJunkConfirm")) {
                manager.Clear();
                UIUtil::ItemList::Refresh();
                SetStatus("$JunkIt_JunkReset", true);
            }
            if (ConfirmPopup(
                    "ResetAutoJunkExclusions",
                    "$JunkIt_ResetAutoJunkExclusions",
                    "$JunkIt_ResetAutoJunkExclusionsConfirm")) {
                manager.ClearNoAutoJunk();
                SetStatus("$JunkIt_AutoJunkExclusionsReset", true);
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

            const auto items = g_showAutoJunkExclusions ? manager.GetAllNoAutoJunkItems() : manager.GetAllJunkItems();
            if (items.empty()) {
                ImGui::TextWrapped(
                    "%s",
                    Translation::Get(
                        g_showAutoJunkExclusions ? "$JunkIt_EmptyAutoJunkExclusions" : "$JunkIt_EmptyJunkList")
                        .c_str());
                PopBrandColors();
                return;
            }

            const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
            if (ImGui::BeginTable("junkList", 2, flags, ImVec2(0.0f, 0.0f))) {
                ImGui::TableSetupColumn(Translation::Get(listHeaderKey).c_str(), ImGuiTableColumnFlags_WidthStretch);
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
                        if (g_showAutoJunkExclusions) {
                            manager.RemoveNoAutoJunkIdentity(item.identity);
                        } else if (manager.RemoveJunkItemAtIndex(index)) {
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
                    SaveIfChanged(SliderIntRow(
                        "$JunkIt_SellChunkSize",
                        "$JunkIt_SellChunkSize_Help",
                        Settings::SellChunkSizeValue(),
                        50,
                        1500));
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
                SkyPromptIntegration::GetSingleton().RefreshPrompts();
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
                SkyPromptIntegration::GetSingleton().RefreshPrompts();
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
        SKSEMenuFramework::AddSectionItem(Translation::Get("$JunkIt_Page_Trash"), RenderTrash);
        SKSEMenuFramework::AddSectionItem(Translation::Get("$JunkIt_Page_AutoJunk"), RenderAutoJunk);
        SKSEMenuFramework::AddSectionItem(Translation::Get("$JunkIt_JunkList_Page"), RenderJunkList);
        SKSEMenuFramework::AddSectionItem(Translation::Get("$JunkIt_Page_Integrations"), RenderIntegrations);
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
            g_captureWaitMouseUp = false;
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
            case CaptureSlot::kTrash:
                Settings::TrashJunkKeyValue() = keyCode;
                break;
            default:
                break;
        }

        SaveSettings();
        g_capture = CaptureSlot::kNone;
        g_captureWaitMouseUp = false;
        SkyPromptIntegration::GetSingleton().RefreshPrompts();
        return true;
    }

    void UI::CloseFrameworkOverlay() {
        auto* main = SKSEMenuFramework::GetMainWindow();
        if (!main) {
            SKSE::log::warn("SKSE Menu Framework has no GetMainWindow export; cannot close overlay");
            return;
        }
        main->IsOpen = false;
    }
}
