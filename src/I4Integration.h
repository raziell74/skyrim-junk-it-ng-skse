#pragma once

namespace JunkIt {

    class I4Integration final {
    public:
        I4Integration() = delete;

        static void Install(RE::GFxMovieView* a_view, const char* a_pathToObj);

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
