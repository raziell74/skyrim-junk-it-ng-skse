#pragma once

#include <functional>

namespace JunkIt {
    class OperationOverlay {
    public:
        enum class Action {
            Store,
            Retrieve,
            Sell,
            Trash
        };

        static void Install();
        static void Show(Action action);
        static void Hide();
        static void RunWithOverlay(Action action, std::function<void()> work);

    private:
        OperationOverlay() = delete;
    };
}
