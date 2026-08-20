#pragma once

#include <fmt/format.h>
#include <string>
#include <string_view>
#include <unordered_map>

namespace JunkIt {
    class Translation {
        public:
            static void Load();
            static const std::string& Get(std::string_view key);

            template <class... Args>
            static std::string Format(std::string_view key, Args&&... args) {
                return fmt::format(fmt::runtime(Get(key)), std::forward<Args>(args)...);
            }

        private:
            static inline std::unordered_map<std::string, std::string> strings;
            static inline std::string missing;
    };
}
