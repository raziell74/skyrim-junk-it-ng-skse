#pragma once
#include "settings.h"
#include <atomic>

using namespace RE;
namespace JunkIt {

    enum class JUNKIT_EVENT_TYPE {
        kNone = 0,
        kMark = 1,
        kTransfer = 2,
        kSell = 3,
        kTrash = 4,
        kTrashBulk = 5
    };

    struct AtomicGuard {
        std::atomic<bool>& flag;
        bool acquired;

        AtomicGuard(std::atomic<bool>& a_flag) : flag(a_flag), acquired(false) {
            bool expected = false;
            acquired = flag.compare_exchange_strong(expected, true);
        }

        ~AtomicGuard() {
            if (acquired) {
                flag.store(false);
            }
        }

        explicit operator bool() const { return acquired; }

        AtomicGuard(const AtomicGuard&) = delete;
        AtomicGuard& operator=(const AtomicGuard&) = delete;
    };

    class InputEventHandler : public RE::BSTEventSink<RE::InputEvent*> {
    public:
        static InputEventHandler* GetSingleton() {
            static InputEventHandler singleton;
            return &singleton;
        }

        static void Install() {
            RE::BSInputDeviceManager::GetSingleton()->AddEventSink(GetSingleton());
            SKSE::log::info("Registered InputEventHandler");
        }

        RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;
        void ExecuteAction(JUNKIT_EVENT_TYPE type);

    private:
        InputEventHandler() = default;
        InputEventHandler(const InputEventHandler&) = delete;
        InputEventHandler(InputEventHandler&&) = delete;
        InputEventHandler& operator=(const InputEventHandler&) = delete;
        InputEventHandler& operator=(InputEventHandler&&) = delete;

        std::atomic<bool> busy{ false };

        enum class ActiveMenuType {
            kNone = 0,
            kInventory = 1,
            kContainer = 2,
            kBarter = 3,
            kLootMenu = 4
        };

        ActiveMenuType GetActiveMenu();
        void HandleKeyDown(uint32_t keyCode, ActiveMenuType activeMenu);
        void HandleGamepadKeyUp(float holdTime, ActiveMenuType activeMenu);
        void HandleMarkKey(RE::ButtonEvent* buttonEvent, ActiveMenuType activeMenu, bool skyPromptShowing);
        void HandleTrashKey(RE::ButtonEvent* buttonEvent);
        void HandleGamepadJunkKey(RE::ButtonEvent* buttonEvent, ActiveMenuType activeMenu);

        bool markHoldArmed{ false };
        bool markTrashFired{ false };
        bool trashHoldArmed{ false };
        bool trashBulkFired{ false };
        bool gamepadTrashFired{ false };
    };
}
