#pragma once

namespace JunkIt {

    class QuickLootIntegration final {
    public:
        QuickLootIntegration() = delete;

        static void Install();
        static void RefreshMenu();
    };

}
