#pragma once

namespace JunkIt {

    class QuickLootIntegration final {
    public:
        QuickLootIntegration() = delete;

        static void Install();
        static void RefreshMenu();
        static void NoteInputDevice(RE::INPUT_DEVICE device);
        static void ToggleSelectedJunk();

        [[nodiscard]] static bool IsReady();
        [[nodiscard]] static bool IsMenuOpen();
        [[nodiscard]] static bool MarkAllowed();
    };

}
