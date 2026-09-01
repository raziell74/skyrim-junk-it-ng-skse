#include "OperationOverlay.h"
#include "Translation.h"

#include <imgui.h>
#include <imgui_impl_dx11.h>
#include <WICTextureLoader.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <utility>

namespace JunkIt {
    namespace {
        constexpr float kSplashSize = 512.0f;
        constexpr float kFadeSeconds = 0.4f;
        constexpr ImVec4 kSplashTint{ 1.0f, 1.0f, 1.0f, 0.25f };
        constexpr const wchar_t* kSplashPath = L"Data\\Interface\\JunkIt\\JunkIt_splash_512x512.png";

        using InitD3D_t = void();
        using Present_t = void(std::uint32_t);
        using Clock = std::chrono::steady_clock;

        std::mutex g_mutex;
        std::atomic<bool> g_hooksInstalled{ false };
        bool g_imguiReady = false;
        bool g_visible = false;
        float g_fade = 0.0f;
        Clock::time_point g_lastPresent{};
        std::string g_label;
        std::function<void()> g_pendingWork;
        ID3D11DeviceContext* g_context = nullptr;
        ID3D11ShaderResourceView* g_splash = nullptr;
        InitD3D_t* g_initD3D = nullptr;
        Present_t* g_present = nullptr;

        const char* TranslationKey(OperationOverlay::Action action) {
            switch (action) {
                case OperationOverlay::Action::Store:
                    return "$JunkIt_Overlay_Storing";
                case OperationOverlay::Action::Retrieve:
                    return "$JunkIt_Overlay_Retrieving";
                case OperationOverlay::Action::Sell:
                    return "$JunkIt_Overlay_Selling";
                case OperationOverlay::Action::Trash:
                    return "$JunkIt_Overlay_Trashing";
            }
            return "$JunkIt_Overlay_Storing";
        }

        bool TryInitImGui() {
            if (g_imguiReady) {
                return true;
            }

            auto* rexDevice = RE::BSGraphics::Renderer::GetDevice();
            if (!rexDevice) {
                return false;
            }

            auto* device = reinterpret_cast<ID3D11Device*>(rexDevice);
            ID3D11DeviceContext* context = nullptr;
            device->GetImmediateContext(&context);
            if (!context) {
                return false;
            }

            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            auto& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.LogFilename = nullptr;
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

            ImGui_ImplDX11_Init(device, context);

            DirectX::CreateWICTextureFromFile(device, kSplashPath, nullptr, &g_splash);
            if (!g_splash) {
                SKSE::log::warn("Operation overlay splash texture failed to load");
            }

            g_context = context;
            g_imguiReady = true;
            SKSE::log::info("Operation overlay ImGui initialized");
            return true;
        }

        void BeginVisible(OperationOverlay::Action action) {
            g_label = Translation::Get(TranslationKey(action));
            g_visible = true;
            g_fade = 0.0f;
            g_lastPresent = Clock::now();
        }

        void AdvanceFade() {
            const auto now = Clock::now();
            float dt = std::chrono::duration<float>(now - g_lastPresent).count();
            g_lastPresent = now;
            dt = std::clamp(dt, 0.0f, 0.05f);
            g_fade = std::min(1.0f, g_fade + dt / kFadeSeconds);
        }

        void DrawOverlay() {
            const auto screen = RE::BSGraphics::Renderer::GetScreenSize();
            auto& io = ImGui::GetIO();
            io.DisplaySize = ImVec2(static_cast<float>(screen.width), static_cast<float>(screen.height));
            io.DeltaTime = std::max(io.DeltaTime, 1.0f / 60.0f);

            if (auto* window = RE::BSGraphics::Renderer::GetCurrentRenderWindow()) {
                auto* rtv = reinterpret_cast<ID3D11RenderTargetView*>(window->renderView);
                if (rtv && g_context) {
                    g_context->OMSetRenderTargets(1, &rtv, nullptr);
                }
            }

            ImGui_ImplDX11_NewFrame();
            ImGui::NewFrame();

            ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
            ImGui::SetNextWindowSize(io.DisplaySize);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::Begin(
                "##JunkItOperationOverlay",
                nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav |
                    ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
                    ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoBringToFrontOnFocus);

            auto* drawList = ImGui::GetWindowDrawList();
            drawList->AddRectFilled(
                ImVec2(0.0f, 0.0f),
                io.DisplaySize,
                IM_COL32(0, 0, 0, static_cast<int>(64.0f * g_fade + 0.5f)));

            const float splash = (io.DisplaySize.y < kSplashSize)
                ? (std::max)(128.0f, io.DisplaySize.y * 0.45f)
                : kSplashSize;
            const ImVec2 splashPos((io.DisplaySize.x - splash) * 0.5f, (io.DisplaySize.y - splash) * 0.5f);
            if (g_splash) {
                ImGui::SetCursorPos(splashPos);
                ImVec4 tint = kSplashTint;
                tint.w = 0.25f * g_fade;
                ImGui::ImageWithBg(
                    reinterpret_cast<ImTextureID>(g_splash),
                    ImVec2(splash, splash),
                    ImVec2(0.0f, 0.0f),
                    ImVec2(1.0f, 1.0f),
                    ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
                    tint);
            }

            const float baseSize = ImGui::GetFontSize();
            const float fontSize = std::clamp(io.DisplaySize.y * 0.05f, 32.0f, 72.0f);
            ImGui::SetWindowFontScale(fontSize / baseSize);
            const ImVec2 textSize = ImGui::CalcTextSize(g_label.c_str());
            const float textX = (io.DisplaySize.x - textSize.x) * 0.5f;
            const float textY = (io.DisplaySize.y - textSize.y) * 0.5f;
            ImGui::SetCursorPos(ImVec2(textX + 2.0f, textY + 2.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, g_fade));
            ImGui::TextUnformatted(g_label.c_str());
            ImGui::PopStyleColor();
            ImGui::SetCursorPos(ImVec2(textX, textY));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(194.0f / 255.0f, 194.0f / 255.0f, 194.0f / 255.0f, g_fade));
            ImGui::TextUnformatted(g_label.c_str());
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);

            ImGui::End();
            ImGui::PopStyleVar(2);

            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        }

        void InitD3DHook() {
            if (g_initD3D) {
                g_initD3D();
            }
            std::lock_guard lock(g_mutex);
            TryInitImGui();
        }

        void PresentHook(std::uint32_t a_timer) {
            std::function<void()> work;
            {
                std::lock_guard lock(g_mutex);
                if (g_visible) {
                    if (TryInitImGui()) {
                        AdvanceFade();
                        DrawOverlay();
                        if (g_fade >= 1.0f) {
                            work = std::move(g_pendingWork);
                        }
                    } else {
                        g_visible = false;
                        work = std::move(g_pendingWork);
                    }
                }
            }

            if (work) {
                if (auto* tasks = SKSE::GetTaskInterface()) {
                    tasks->AddTask(std::move(work));
                } else {
                    work();
                }
            }

            if (g_present) {
                g_present(a_timer);
            }
        }

        bool InstallCallHook(REL::Relocation<std::uintptr_t> target, std::uintptr_t hook, std::uintptr_t& original, const char* name) {
            if (!REL::make_pattern<"E8">().match(target.address())) {
                SKSE::log::error("Operation overlay {} hook pattern mismatch", name);
                return false;
            }

            auto& trampoline = SKSE::GetTrampoline();
            original = trampoline.write_call<5>(target.address(), hook);
            return true;
        }
    }

    void OperationOverlay::Install() {
        REL::Relocation<std::uintptr_t> initTarget{ RELOCATION_ID(75595, 77226), REL::VariantOffset(0x9, 0x275, 0x9) };
        std::uintptr_t initOriginal = 0;
        if (InstallCallHook(initTarget, reinterpret_cast<std::uintptr_t>(&InitD3DHook), initOriginal, "InitD3D")) {
            g_initD3D = reinterpret_cast<InitD3D_t*>(initOriginal);
        }

        REL::Relocation<std::uintptr_t> presentTarget{ RELOCATION_ID(75461, 77246), REL::VariantOffset(0x9, 0x9, 0x9) };
        std::uintptr_t presentOriginal = 0;
        if (!InstallCallHook(presentTarget, reinterpret_cast<std::uintptr_t>(&PresentHook), presentOriginal, "Present")) {
            SKSE::log::error("Operation overlay disabled: Present hook failed");
            return;
        }

        g_present = reinterpret_cast<Present_t*>(presentOriginal);
        g_hooksInstalled.store(true);

        {
            std::lock_guard lock(g_mutex);
            TryInitImGui();
        }

        SKSE::log::info("Operation overlay hooks installed");
    }

    void OperationOverlay::Show(Action action) {
        std::lock_guard lock(g_mutex);
        BeginVisible(action);
    }

    void OperationOverlay::Hide() {
        std::lock_guard lock(g_mutex);
        g_visible = false;
        g_fade = 0.0f;
        g_pendingWork = {};
    }

    void OperationOverlay::RunWithOverlay(Action action, std::function<void()> work) {
        if (!work) {
            return;
        }

        if (!g_hooksInstalled.load()) {
            work();
            return;
        }

        {
            std::lock_guard lock(g_mutex);
            BeginVisible(action);
            g_pendingWork = std::move(work);
        }
    }
}
