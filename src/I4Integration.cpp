#include "I4Integration.h"
#include "JunkData.h"
#include "settings.h"
#include "util.h"

#include <json/json.h>

namespace JunkIt {

    void I4JunkConfig::Load() {
        RE::BSResourceNiBinaryStream fileStream{ "SKSE/Plugins/InventoryInjector/JunkIt.json" };
        if (!fileStream.good()) {
            SKSE::log::warn("Could not open i4 config: SKSE/Plugins/InventoryInjector/JunkIt.json");
            return;
        }

        auto size = fileStream.stream->totalSize;
        auto buffer = std::make_unique<char[]>(size);
        fileStream.read(buffer.get(), size);

        Json::CharReaderBuilder builder;
        std::unique_ptr<Json::CharReader> reader{ builder.newCharReader() };

        Json::Value root;
        std::string errs;
        if (!reader->parse(buffer.get(), buffer.get() + size, &root, &errs)) {
            SKSE::log::error("Failed to parse i4 JunkIt config: {}", errs);
            return;
        }

        const auto& rules = root["rules"];
        if (!rules.isArray() || rules.empty()) {
            SKSE::log::warn("No rules found in i4 JunkIt config");
            return;
        }

        const auto& assign = rules[0]["assign"];
        if (!assign.isObject()) {
            SKSE::log::warn("No assign block in first i4 JunkIt rule");
            return;
        }

        if (assign.isMember("iconSource")) {
            iconSource = assign["iconSource"].asString();
        }
        if (assign.isMember("iconLabel")) {
            iconLabel = assign["iconLabel"].asString();
        }
        if (assign.isMember("iconColor")) {
            std::string hex = assign["iconColor"].asString();
            std::size_t prefix = (!hex.empty() && hex[0] == '#') ? 1 : 0;
            iconColor = static_cast<std::uint32_t>(std::stoul(hex.substr(prefix), nullptr, 16));
        }
        if (assign.isMember("subTypeDisplay")) {
            subTypeDisplay = assign["subTypeDisplay"].asString();
        }

        loaded = true;
        SKSE::log::info("Loaded i4 JunkIt config: iconSource={}, iconLabel={}, iconColor=0x{:X}, subTypeDisplay={}",
            iconSource, iconLabel, iconColor, subTypeDisplay);
    }

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
            for (std::uint32_t i = 0, size = itemList->items.size(); i < size; i++) {
                auto* item = itemList->items[i];
                if (!item || !item->data.objDesc) {
                    continue;
                }

                bool isJunk = junkManager.IsJunk(item->data.objDesc);
                item->obj.SetMember("isJunk", isJunk);
                item->obj.SetMember("isJunkIcon", isJunk && Settings::GetUpdateItemIcon());
                item->obj.SetMember("isJunkSubType", isJunk && Settings::GetUpdateSubTypeDisplay());
            }
        } else {
            if (a_params.argCount >= 1) {
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
                        entryObject.SetMember("isJunkIcon", isJunk && Settings::GetUpdateItemIcon());
                        entryObject.SetMember("isJunkSubType", isJunk && Settings::GetUpdateSubTypeDisplay());
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
