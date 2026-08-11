#pragma once

namespace JunkIt {

    class I4MovieHook final {
    public:
        I4MovieHook() = delete;

        static void Install();

    private:
        static void AddScaleformHooks(
            RE::GFxMovieView* a_view,
            RE::GFxMovieView::ScaleModeType a_scaleMode);

        using SetViewScaleMode_t = void(RE::GFxMovieView*, RE::GFxMovieView::ScaleModeType);
        static inline SetViewScaleMode_t* _SetViewScaleMode = nullptr;
    };
}
