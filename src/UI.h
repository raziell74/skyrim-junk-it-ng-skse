#pragma once

#include <cstdint>

namespace JunkIt {
    class UI {
        public:
            static void Register();
            static bool ConsumeKeyCapture(std::uint32_t keyCode);
            static void CloseFrameworkOverlay();
    };
}
