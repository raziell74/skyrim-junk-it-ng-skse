#pragma once 
#include "junk.h"
#include "settings.h"

using namespace RE; 
namespace JunkIt {

    enum class JUNKIT_EVENT_TYPE {
		kNone = 0,
        kMark = 1,
		kTransfer = 2,
		kSell = 3
	};

    class ButtonEventHandler : public RE::BSTEventSink<RE::ButtonEvent> {
        // public: 
        //     static void Install() {
        //         ScriptEventSourceHolder::GetSingleton()->GetEventSource<ButtonEvent>()->AddEventSink(GetSingleton()); 
        //         SKSE::log::info("Registered {}", typeid(RE::ButtonEvent).name()); 
        //     }
        //     static ButtonEventHandler* GetSingleton() {
        //         static ButtonEventHandler singleton; 
        //         return &singleton; 
        //     }

        //     virtual RE::BSEventNotifyControl ProcessEvent(const ButtonEvent *a_event, RE::BSTEventSource<ButtonEvent> *a_eventSource) override;
    };
}
