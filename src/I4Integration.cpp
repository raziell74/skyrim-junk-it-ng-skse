#include "I4Integration.h"
#include "JunkData.h"
#include "util.h"

namespace JunkIt {

    void I4Integration::Install(RE::GFxMovieView* a_view, const char* a_pathToObj) {
        assert(a_view);

        RE::GFxValue obj;
        a_view->GetVariable(&obj, a_pathToObj);
        if (!obj.IsObject()) {
            return;
        }

        SKSE::log::trace("Hooking {}.processList", a_pathToObj);

        RE::GFxValue oldProcessList;
        obj.GetMember("processList", &oldProcessList);

        RE::GFxValue newProcessList;
        auto impl = RE::make_gptr<ProcessListFunc>(oldProcessList);
        a_view->CreateFunction(&newProcessList, impl.get());
        obj.SetMember("processList", newProcessList);
    }

    void I4Integration::ProcessListFunc::Call(Params& a_params) {
        SKSE::log::trace("Running I4Integration.processList hook");

        auto& junkManager = JunkDataManager::GetSingleton();
        auto* itemList = UIUtil::ItemList::GetOpenList();

        if (itemList && itemList->items.size() > 0) {
            SKSE::log::trace("Using native ItemList with {} items for per-stack junk matching", itemList->items.size());

            for (std::uint32_t i = 0, size = itemList->items.size(); i < size; i++) {
                auto* item = itemList->items[i];
                if (!item || !item->data.objDesc) {
                    continue;
                }

                bool isJunk = junkManager.IsJunk(item->data.objDesc);
                item->obj.SetMember("isJunk", isJunk);
            }
        } else {
            SKSE::log::trace("No native ItemList available, falling back to GFx _entryList with base form matching");

            if (a_params.argCount < 1) {
                SKSE::log::debug("Expected 1 argument, received {}", a_params.argCount);
            } else {
                auto& a_list = a_params.args[0];
                RE::GFxValue entryList;
                if (a_list.IsObject()) {
                    a_list.GetMember("_entryList", &entryList);
                }

                if (entryList.IsArray()) {
                    for (std::uint32_t i = 0, size = entryList.GetArraySize(); i < size; i++) {
                        RE::GFxValue entryObject;
                        entryList.GetElement(i, &entryObject);
                        if (!entryObject.IsObject()) {
                            continue;
                        }

                        RE::GFxValue formId;
                        entryObject.GetMember("formId", &formId);
                        if (!formId.IsNumber()) {
                            continue;
                        }

                        auto* form = RE::TESForm::LookupByID(static_cast<RE::FormID>(formId.GetNumber()));
                        bool isJunk = form ? junkManager.IsJunk(form) : false;
                        entryObject.SetMember("isJunk", isJunk);
                    }
                }
            }
        }

        if (_oldFunc.IsObject()) {
            _oldFunc.Invoke(
                "call",
                a_params.retVal,
                a_params.argsWithThisRef,
                static_cast<std::uint32_t>(a_params.argCount) + 1);
        }
    }
}
