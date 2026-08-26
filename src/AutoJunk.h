#pragma once

#include <span>
#include <string_view>

namespace JunkIt {
    class AutoJunk {
    public:
        static void Install();
        [[nodiscard]] static std::span<const char* const> KnownItemTypes();
        static bool TryMarkEntry(RE::InventoryEntryData* entry);
        static bool ApplyToPlayerInventory();
        static bool ApplyToReferenceInventory(RE::TESObjectREFR* container);
        static bool ApplyToOpenMenus();

    private:
        AutoJunk() = default;
    };
}
