#include "I4Integration.h"
#include "JunkData.h"
#include "settings.h"
#include "util.h"

#include <json/json.h>
#include <map>
#include <vector>

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

    void I4Integration::SetJunkFlags(RE::GFxValue& obj, bool isJunk) {
        if (!obj.IsObject()) {
            return;
        }
        obj.SetMember("isJunk", isJunk);
        obj.SetMember("isJunkIcon", isJunk && Settings::GetUpdateItemIcon());
        obj.SetMember("isJunkSubType", isJunk && Settings::GetUpdateSubTypeDisplay());
    }

    void I4Integration::ReprocessOpenList(RE::GFxMovieView* movie) {
        if (!movie) {
            return;
        }

        RE::GFxValue itemList;
        movie->GetVariable(&itemList, "_root.Menu_mc.inventoryLists.itemList");
        if (!itemList.IsObject()) {
            return;
        }

        RE::GFxValue setter;
        movie->GetVariable(&setter, "_global.InventoryIconSetter.prototype");
        if (!setter.IsObject()) {
            return;
        }

        setter.Invoke("processList", nullptr, &itemList, 1);
    }

    namespace {
        RE::TESBoundObject* BoundFromGFxEntry(RE::GFxValue& entryObject) {
            RE::GFxValue formIdVal;
            if (!entryObject.GetMember("formId", &formIdVal) || !formIdVal.IsNumber()) {
                return nullptr;
            }
            auto* form = RE::TESForm::LookupByID(static_cast<RE::FormID>(formIdVal.GetNumber()));
            return form ? form->As<RE::TESBoundObject>() : nullptr;
        }

        template <class Handle>
        RE::TESObjectREFR* ResolveOwner(Handle handle) {
            RE::TESObjectREFRPtr refr;
            LookupReferenceByHandle(handle, refr);
            return refr.get();
        }

        struct LiveInventoryCache {
            std::map<std::uint32_t, std::vector<RE::InventoryEntryData*>> entries;

            void Include(RE::TESObjectREFR* owner) {
                if (!owner) {
                    return;
                }
                const auto key = owner->GetHandle().native_handle();
                if (entries.contains(key)) {
                    return;
                }

                auto& list = entries[key];
                auto* changes = owner->GetInventoryChanges(true);
                if (!changes || !changes->entryList) {
                    return;
                }
                for (auto& entry : *changes->entryList) {
                    if (entry && entry->object) {
                        list.push_back(entry);
                    }
                }
            }

            bool HasJunk(RE::TESObjectREFR* owner, RE::TESBoundObject* object) {
                if (!owner || !object) {
                    return false;
                }
                Include(owner);
                const auto key = owner->GetHandle().native_handle();
                auto it = entries.find(key);
                if (it == entries.end()) {
                    return false;
                }

                auto& junkManager = JunkDataManager::GetSingleton();
                for (auto* entry : it->second) {
                    if (entry->object == object && junkManager.IsJunk(entry)) {
                        return true;
                    }
                }
                return false;
            }

            bool LiveIsJunk(RE::TESObjectREFR* owner, RE::TESBoundObject* object) {
                if (HasJunk(owner, object)) {
                    return true;
                }

                const auto ui = RE::UI::GetSingleton();
                if (!ui || !ui->IsMenuOpen("BarterMenu")) {
                    return false;
                }
                if (HasJunk(UIUtil::Menu::GetBarterMenuTargetRef(), object)) {
                    return true;
                }
                return HasJunk(UIUtil::Menu::GetMerchantContainer(), object);
            }
        };
    }

    void I4Integration::ProcessListFunc::Call(Params& a_params) {
        SKSE::log::trace("Running I4Integration.processList hook");

        auto* itemList = UIUtil::ItemList::GetOpenList();
        LiveInventoryCache liveInventories;
        auto& junkManager = JunkDataManager::GetSingleton();

        if (itemList && itemList->items.size() > 0) {
            for (std::uint32_t i = 0, size = itemList->items.size(); i < size; i++) {
                auto* item = itemList->items[i];
                if (!item || !item->obj.IsObject()) {
                    continue;
                }

                if (auto* objDesc = item->data.objDesc) {
                    SetJunkFlags(item->obj, junkManager.IsJunk(objDesc));
                } else {
                    auto* bound = BoundFromGFxEntry(item->obj);
                    SetJunkFlags(item->obj, liveInventories.LiveIsJunk(ResolveOwner(item->data.owner), bound));
                }
            }
        } else if (a_params.argCount >= 1) {
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

                    auto* bound = BoundFromGFxEntry(entryObject);
                    SetJunkFlags(
                        entryObject,
                        liveInventories.LiveIsJunk(RE::PlayerCharacter::GetSingleton(), bound));
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
