#pragma once

#include <unordered_map>
#include <unordered_set>

namespace JunkIt {
    template <class Fn>
    void ForEachInventoryEntry(RE::TESObjectREFR* refr, Fn&& fn, bool noInit = false) {
        if (!refr) {
            return;
        }

        std::unordered_set<RE::TESBoundObject*> seen;
        std::unordered_set<RE::TESBoundObject*> leveled;

        if (auto* changes = refr->GetInventoryChanges(noInit); changes && changes->entryList) {
            for (auto& entry : *changes->entryList) {
                if (!entry || !entry->object) {
                    continue;
                }
                seen.insert(entry->object);
                if (entry->IsLeveled()) {
                    leveled.insert(entry->object);
                }
                fn(entry);
            }
        }

        auto* container = refr->GetContainer();
        if (!container) {
            return;
        }

        container->ForEachContainerObject([&](auto& obj) {
            if (!obj.obj || seen.contains(obj.obj) || leveled.contains(obj.obj)) {
                return RE::BSContainer::ForEachResult::kContinue;
            }
            RE::InventoryEntryData temp(obj.obj, 0);
            fn(&temp);
            return RE::BSContainer::ForEachResult::kContinue;
        });
    }

    template <class Fn>
    void ForEachInventoryCount(RE::TESObjectREFR* refr, Fn&& fn, bool noInit = false) {
        if (!refr) {
            return;
        }

        std::unordered_map<RE::TESBoundObject*, std::int32_t> counts;
        std::unordered_set<RE::TESBoundObject*> leveled;

        if (auto* changes = refr->GetInventoryChanges(noInit); changes && changes->entryList) {
            for (auto& entry : *changes->entryList) {
                if (!entry || !entry->object) {
                    continue;
                }
                counts[entry->object] += entry->countDelta;
                if (entry->IsLeveled()) {
                    leveled.insert(entry->object);
                }
            }
        }

        if (auto* container = refr->GetContainer()) {
            container->ForEachContainerObject([&](auto& obj) {
                if (!obj.obj || leveled.contains(obj.obj)) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                counts[obj.obj] += obj.count;
                return RE::BSContainer::ForEachResult::kContinue;
            });
        }

        for (auto& [obj, count] : counts) {
            fn(obj, count);
        }
    }
}
