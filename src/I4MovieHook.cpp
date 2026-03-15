#include "I4MovieHook.h"
#include "I4Integration.h"

namespace JunkIt {

    void I4MovieHook::Install() {
        SKSE::log::info("Installing I4MovieHook...");

        REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(80302, 82557), REL::VariantOffset(0x1D9, 0x1DD, 0x1D9) };

        if (!REL::make_pattern<"FF 15">().match(target.address())) {
            SKSE::log::error("Failed to install I4MovieHook - pattern mismatch");
            return;
        }

        auto& trampoline = SKSE::GetTrampoline();
        auto ptr = trampoline.write_call<6>(target.address(), reinterpret_cast<std::uintptr_t>(&AddScaleformHooks));
        _SetViewScaleMode = *reinterpret_cast<SetViewScaleMode_t**>(ptr);

        SKSE::log::info("I4MovieHook installed successfully");
    }

    void I4MovieHook::AddScaleformHooks(
        RE::GFxMovieView* a_view,
        RE::GFxMovieView::ScaleModeType a_scaleMode)
    {
        _SetViewScaleMode(a_view, a_scaleMode);

        I4Integration::Install(a_view, "_global.InventoryIconSetter.prototype");
        I4Integration::Install(a_view, "_global.CraftingIconSetter.prototype");
        I4Integration::Install(a_view, "_global.MagicIconSetter.prototype");
        I4Integration::Install(a_view, "_global.FavoritesIconSetter.prototype");
    }
}
