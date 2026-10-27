#pragma once

#include <string_view>

namespace JunkIt {
    class I5Integration final {
    public:
        I5Integration() = delete;

        static void Install();
        static void InvalidateItem(RE::FormID formID);
        static void InvalidateIdentity(std::string_view identity);
    };
}
