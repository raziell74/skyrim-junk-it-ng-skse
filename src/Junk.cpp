#include "junk.h"

namespace JunkIt {

    void JunkHandler::TransferItem(
        TESBoundObject* a_item, 
        TESObjectREFR* a_fromContainer, 
        TESObjectREFR* a_toContainer, 
        ITEM_REMOVE_REASON a_reason, 
        std::int32_t a_count, 
        InventoryEntryData* a_invData) 
    {
        using Count = std::int32_t;
        Count remainingCount = a_count;

        // Check if the invData is valid or not, if it's not just do a generic item move
        if (!a_invData || !a_invData->extraLists || a_invData->extraLists->empty()) {
            a_fromContainer->RemoveItem(a_item, a_count, a_reason, nullptr, a_toContainer);
            return;
        }

        // Iterate through the ExtraDataLists and add each one to the receiving container
        std::for_each(a_invData->extraLists->begin(), a_invData->extraLists->end(), [&](ExtraDataList* dataList) {
            if (dataList == nullptr) {
                SKSE::log::error(":: [0x{:X}] ExtraDataList in inventory is null or invalid", a_item->GetFormID());
                return -1;
            }
    
            Count itemCount = dataList->GetCount();
            a_fromContainer->RemoveItem(a_item, itemCount, a_reason, dataList, a_toContainer);
            remainingCount -= itemCount;
            return 1;
        });

        // Get any items missed by the ExtraDataLists
        if (remainingCount > 0) {
            a_fromContainer->RemoveItem(a_item, remainingCount, a_reason, nullptr, a_toContainer);
        }
    }
}
