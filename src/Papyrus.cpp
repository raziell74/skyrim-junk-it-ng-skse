#include "Papyrus.h"
#include "JunkData.h"

namespace JunkIt::Papyrus {
    bool IsJunk(RE::StaticFunctionTag*, RE::TESForm* a_form) {
        if (!a_form) {
            return false;
        }

        auto& manager = JunkDataManager::GetSingleton();
        if (auto* refr = a_form->As<RE::TESObjectREFR>()) {
            const char* name = refr->GetDisplayFullName();
            return manager.IsJunk(refr->GetBaseObject(), &refr->extraList, name ? name : "");
        }

        auto* bound = a_form->As<RE::TESBoundObject>();
        if (!bound) {
            return false;
        }
        const char* name = bound->GetName();
        return manager.IsJunk(bound, nullptr, name ? name : "");
    }

    bool Register(RE::BSScript::IVirtualMachine* a_vm) {
        a_vm->RegisterFunction("IsJunk", "JunkIt", IsJunk);
        return true;
    }
}
