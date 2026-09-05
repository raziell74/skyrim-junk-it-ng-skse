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
            SKSE::log::debug("I4 processList hook skipped, {} is not an object", a_pathToObj);
            return;
        }

        SKSE::log::trace("Hooking {}.processList", a_pathToObj);

        RE::GFxValue oldProcessList;
        obj.GetMember("processList", &oldProcessList);

        RE::GFxValue newProcessList;
        auto impl = RE::make_gptr<ProcessListFunc>(oldProcessList);
        a_view->CreateFunction(&newProcessList, impl.get());
        obj.SetMember("processList", newProcessList);
        SKSE::log::debug("Hooked {}.processList existingFunc={}", a_pathToObj, oldProcessList.IsObject());
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
            SKSE::log::debug("ReprocessOpenList skipped, no movie");
            return;
        }

        RE::GFxValue itemList;
        movie->GetVariable(&itemList, "_root.Menu_mc.inventoryLists.itemList");
        if (!itemList.IsObject()) {
            SKSE::log::debug("ReprocessOpenList skipped, no itemList");
            return;
        }

        RE::GFxValue setter;
        movie->GetVariable(&setter, "_global.InventoryIconSetter.prototype");
        if (!setter.IsObject()) {
            SKSE::log::debug("ReprocessOpenList skipped, no InventoryIconSetter");
            return;
        }

        SKSE::log::trace("ReprocessOpenList invoking InventoryIconSetter.processList");
        setter.Invoke("processList", nullptr, &itemList, 1);
        SKSE::log::trace("ReprocessOpenList original processList returned");
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
                    SKSE::log::trace("Live cache include skipped, null owner");
                    return;
                }
                const auto key = owner->GetHandle().native_handle();
                if (entries.contains(key)) {
                    return;
                }

                auto& list = entries[key];
                auto* changes = owner->GetInventoryChanges(true);
                if (!changes || !changes->entryList) {
                    if (spdlog::should_log(spdlog::level::debug)) {
                        SKSE::log::debug(
                            "Live cache include {} [{}] has no inventory changes",
                            owner->GetName(),
                            FormUtil::Form::GetFormConfigString(owner));
                    }
                    return;
                }
                for (auto& entry : *changes->entryList) {
                    if (entry && entry->object) {
                        list.push_back(entry);
                    }
                }
                if (spdlog::should_log(spdlog::level::debug)) {
                    SKSE::log::debug(
                        "Live cache include {} [{}] entries={}",
                        owner->GetName(),
                        FormUtil::Form::GetFormConfigString(owner),
                        list.size());
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
                    if (spdlog::should_log(spdlog::level::trace) && owner && object) {
                        SKSE::log::trace(
                            "     Live junk match on owner {} [{}] for {} [{}]",
                            owner->GetName(),
                            FormUtil::Form::GetFormConfigString(owner),
                            object->GetName(),
                            FormUtil::Form::GetFormConfigString(object));
                    }
                    return true;
                }

                const auto ui = RE::UI::GetSingleton();
                if (!ui || !ui->IsMenuOpen("BarterMenu")) {
                    return false;
                }

                auto* barterTarget = UIUtil::Menu::GetBarterMenuTargetRef();
                if (HasJunk(barterTarget, object)) {
                    if (spdlog::should_log(spdlog::level::trace) && barterTarget && object) {
                        SKSE::log::trace(
                            "     Live junk match on vendor {} [{}] for {} [{}]",
                            barterTarget->GetName(),
                            FormUtil::Form::GetFormConfigString(barterTarget),
                            object->GetName(),
                            FormUtil::Form::GetFormConfigString(object));
                    }
                    return true;
                }

                auto* merchantContainer = UIUtil::Menu::GetMerchantContainer();
                const bool merchantJunk = HasJunk(merchantContainer, object);
                if (merchantJunk && spdlog::should_log(spdlog::level::trace) && merchantContainer && object) {
                    SKSE::log::trace(
                        "     Live junk match on merchant container {} [{}] for {} [{}]",
                        merchantContainer->GetName(),
                        FormUtil::Form::GetFormConfigString(merchantContainer),
                        object->GetName(),
                        FormUtil::Form::GetFormConfigString(object));
                }
                return merchantJunk;
            }
        };
    }

    void I4Integration::ProcessListFunc::Call(Params& a_params) {
        SKSE::log::trace("Running I4Integration.processList hook");

        auto* itemList = UIUtil::ItemList::GetOpenList();
        LiveInventoryCache liveInventories;
        auto& junkManager = JunkDataManager::GetSingleton();

        const auto ui = RE::UI::GetSingleton();
        const bool barterOpen = ui && ui->IsMenuOpen("BarterMenu");
        SKSE::log::debug(
            "processList itemList={} items={} args={} barter={} inventory={} container={}",
            itemList != nullptr,
            itemList ? itemList->items.size() : 0,
            a_params.argCount,
            barterOpen,
            ui && ui->IsMenuOpen("InventoryMenu"),
            ui && ui->IsMenuOpen("ContainerMenu"));
        if (barterOpen && spdlog::should_log(spdlog::level::debug)) {
            if (auto* target = UIUtil::Menu::GetBarterMenuTargetRef()) {
                SKSE::log::debug(
                    "     Vendor {} [{}]",
                    target->GetName(),
                    FormUtil::Form::GetFormConfigString(target));
            } else {
                SKSE::log::debug("     Vendor ref not resolved");
            }
            if (auto* rawTarget = UIUtil::Menu::GetContainer<RE::BarterMenu>()) {
                SKSE::log::debug(
                    "     Barter menu target {} [{}] actor={}",
                    rawTarget->GetName(),
                    FormUtil::Form::GetFormConfigString(rawTarget),
                    rawTarget->As<RE::Actor>() != nullptr);
            } else {
                SKSE::log::debug("     Barter menu target handle not resolved");
            }
        }

        std::uint32_t descCount = 0;
        std::uint32_t descJunk = 0;
        std::uint32_t liveCount = 0;
        std::uint32_t liveJunk = 0;

        if (itemList && itemList->items.size() > 0) {
            const auto size = itemList->items.size();
            SKSE::log::debug("Processing itemList, {} items", size);
            for (std::uint32_t i = 0; i < size; i++) {
                auto* item = itemList->items[i];
                if (!item || !item->obj.IsObject()) {
                    SKSE::log::trace("     [{}] skipped, item={} gfxObject={}", i, item != nullptr, item && item->obj.IsObject());
                    continue;
                }

                if (auto* objDesc = item->data.objDesc) {
                    const bool isJunk = junkManager.IsJunk(objDesc);
                    descCount++;
                    if (isJunk) {
                        descJunk++;
                    }
                    if (spdlog::should_log(spdlog::level::trace) && objDesc->object) {
                        SKSE::log::trace(
                            "     [{}] {} [{}] objDesc junk={}",
                            i,
                            objDesc->object->GetName(),
                            FormUtil::Form::GetFormConfigString(objDesc->object),
                            isJunk);
                    }
                    SetJunkFlags(item->obj, isJunk);
                } else {
                    auto* bound = BoundFromGFxEntry(item->obj);
                    const bool isJunk = liveInventories.LiveIsJunk(ResolveOwner(item->data.owner), bound);
                    liveCount++;
                    if (isJunk) {
                        liveJunk++;
                    }
                    if (spdlog::should_log(spdlog::level::trace)) {
                        SKSE::log::trace(
                            "     [{}] {} [{}] live junk={}",
                            i,
                            bound ? bound->GetName() : "",
                            bound ? FormUtil::Form::GetFormConfigString(bound) : "",
                            isJunk);
                    }
                    SetJunkFlags(item->obj, isJunk);
                }
            }
            SKSE::log::debug(
                "itemList junk flags objDesc={}/{} live={}/{}",
                descJunk,
                descCount,
                liveJunk,
                liveCount);
        } else if (a_params.argCount >= 1) {
            auto& a_list = a_params.args[0];
            RE::GFxValue entryList;
            if (a_list.IsObject()) {
                a_list.GetMember("_entryList", &entryList);
            }

            if (entryList.IsArray()) {
                const auto size = entryList.GetArraySize();
                SKSE::log::debug("Processing GFx _entryList, {} entries", size);
                for (std::uint32_t i = 0; i < size; i++) {
                    RE::GFxValue entryObject;
                    entryList.GetElement(i, &entryObject);
                    if (!entryObject.IsObject()) {
                        SKSE::log::trace("     [{}] skipped, entry is not an object", i);
                        continue;
                    }

                    auto* bound = BoundFromGFxEntry(entryObject);
                    const bool isJunk = liveInventories.LiveIsJunk(RE::PlayerCharacter::GetSingleton(), bound);
                    liveCount++;
                    if (isJunk) {
                        liveJunk++;
                    }
                    if (spdlog::should_log(spdlog::level::trace)) {
                        SKSE::log::trace(
                            "     [{}] {} [{}] live junk={}",
                            i,
                            bound ? bound->GetName() : "",
                            bound ? FormUtil::Form::GetFormConfigString(bound) : "",
                            isJunk);
                    }
                    SetJunkFlags(entryObject, isJunk);
                }
                SKSE::log::debug("entryList junk flags live={}/{}", liveJunk, liveCount);
            } else {
                SKSE::log::debug("processList fallback has no _entryList array");
            }
        } else {
            SKSE::log::debug("No itemList or _entryList to process");
        }

        if (_oldFunc.IsObject()) {
            SKSE::log::trace("Invoking original processList");
            _oldFunc.Invoke(
                "call",
                a_params.retVal,
                a_params.argsWithThisRef,
                static_cast<std::uint32_t>(a_params.argCount) + 1);
            SKSE::log::trace("Original processList returned");
        } else {
            SKSE::log::debug("No original processList function to invoke");
        }
    }
}
