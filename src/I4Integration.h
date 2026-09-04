#pragma once

#include <string>

namespace JunkIt {

    struct I4JunkConfig {
        std::string iconSource;
        std::string iconLabel;
        std::uint32_t iconColor = 0;
        std::string subTypeDisplay;
        bool loaded = false;

        static I4JunkConfig& GetSingleton() {
            static I4JunkConfig instance;
            return instance;
        }

        void Load();

    private:
        I4JunkConfig() = default;
    };

    class I4Integration final {
    public:
        I4Integration() = delete;

        static void Install(RE::GFxMovieView* a_view, const char* a_pathToObj);
        static void SetJunkFlags(RE::GFxValue& obj, bool isJunk);
        static void ReprocessOpenList(RE::GFxMovieView* movie);

    private:
        class ProcessListFunc : public RE::GFxFunctionHandler {
        public:
            ProcessListFunc(const RE::GFxValue& a_oldFunc) : _oldFunc{ a_oldFunc } {}

            void Call(Params& a_params) override;

        private:
            RE::GFxValue _oldFunc;
        };
    };
}
