
#include "event.h"

namespace JunkIt {
    // RE::BSEventNotifyControl ButtonEventHandler::ProcessEvent(const ButtonEvent *a_event, RE::BSTEventSource<ButtonEvent> *a_eventSource) {
    //     using Result = RE::BSEventNotifyControl;

    //     JUNKIT_EVENT_TYPE eventType = JUNKIT_EVENT_TYPE::kNone;

    //     if (a_event->value == Settings::GetMarkJunkKey() && a_event->IsUp()) {
    //         SKSE::log::info("Mark Junk Key Down");
    //         eventType = JUNKIT_EVENT_TYPE::kMark;
    //     } else if (a_event->value == Settings::GetTransferJunkKey() && a_event->IsUp()) {
    //         SKSE::log::info("Transfer Junk Key Down");
    //         eventType = JUNKIT_EVENT_TYPE::kTransfer;
    //     } else {
    //         return Result::kContinue;
    //     }

    //     if (eventType == JUNKIT_EVENT_TYPE::kMark) {
    //         SKSE::log::info("Marking Junk");
    //         // JunkHandler::MarkJunk(itemListMenu);
    //     } else if (eventType == JUNKIT_EVENT_TYPE::kTransfer) {
    //         SKSE::log::info("Transferring Junk");
    //         // JunkHandler::TransferJunk(itemListMenu);
    //     }

    //     return Result::kContinue; 
    // }
}