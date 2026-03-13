#include "log.h"
#include "settings.h"
#include "junk.h"
#include "JunkData.h"
#include "event.h"
#include "DIIIIntegration.h"

void DIIIMessageHandler(SKSE::MessagingInterface::Message* msg) {
	if (msg->type == DIII::kMessage_GetAPI) {
		auto* api = static_cast<DIII::IAPI*>(msg->data);
		api->RegisterCondition("isJunk",
			[](const Json::Value&, RE::FormType) -> std::unique_ptr<DIII::ICondition> {
				return std::make_unique<JunkIt::IsJunkCondition>();
			});
		SKSE::log::info("Registered DIII condition: isJunk");
	}
}

void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
	switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			JunkIt::InputEventHandler::Install();
			break;
		case SKSE::MessagingInterface::kPostLoad:
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			JunkIt::Settings::Load();
			
			// Migration: if co-save is empty but old FormList has data, migrate
			if (JunkIt::JunkDataManager::GetSingleton().Size() == 0) {
				auto* oldJunkList = JunkIt::Settings::GetJunkList();
				if (oldJunkList && oldJunkList->forms.size() > 0) {
					SKSE::log::info("Migrating from old FormList to JunkDataManager...");
					JunkIt::JunkDataManager::GetSingleton().MigrateFromFormList(oldJunkList);
				}
			}
			break;
		case SKSE::MessagingInterface::kNewGame:
			break;
		case SKSE::MessagingInterface::kSaveGame:
			break;
	}
}

void RefreshDllSettings(RE::StaticFunctionTag*) {
	SKSE::log::info(" ");
	SKSE::log::info("RefreshDllSettings called");
	JunkIt::Settings::Load();
}

void RefreshUIIcons(RE::StaticFunctionTag*) {
	UIUtil::ItemList::Refresh();
}

RE::TESForm* ToggleSelectedAsJunk(RE::StaticFunctionTag*) {
	return JunkIt::JunkHandler::ToggleSelectedItemJunk();
}

bool IsItemJunk(RE::StaticFunctionTag*, RE::TESForm* a_form) {
	if (!a_form) {
		return false;
	}
	return JunkIt::JunkDataManager::GetSingleton().IsJunk(a_form);
}

std::int32_t GetJunkListSize(RE::StaticFunctionTag*) {
	return static_cast<std::int32_t>(JunkIt::JunkDataManager::GetSingleton().Size());
}

RE::BSFixedString GetJunkItemNameAt(RE::StaticFunctionTag*, std::int32_t index) {
	auto item = JunkIt::JunkDataManager::GetSingleton().GetJunkItemAt(index);
	return RE::BSFixedString(item.displayName.c_str());
}

bool RemoveJunkItemAtIndex(RE::StaticFunctionTag*, std::int32_t index) {
	bool result = JunkIt::JunkDataManager::GetSingleton().RemoveJunkItemAtIndex(index);
	if (result) {
		UIUtil::ItemList::Refresh();
	}
	return result;
}

void ClearAllJunk(RE::StaticFunctionTag*) {
	JunkIt::JunkDataManager::GetSingleton().Clear();
	UIUtil::ItemList::Refresh();
}

RE::ContainerMenu::ContainerMode GetContainerMode(RE::StaticFunctionTag*) {
	return JunkIt::JunkHandler::GetContainerMode();
}

std::int32_t GetMenuItemValue(RE::StaticFunctionTag*, RE::TESForm* a_form) {
	return JunkIt::JunkHandler::GetMenuItemValue(a_form);
}

bool BindPapyrusFunctions(RE::BSScript::IVirtualMachine* vm) {
	vm->RegisterFunction("RefreshUIIcons", "JunkIt_MCM", RefreshUIIcons);
	vm->RegisterFunction("ToggleSelectedAsJunk", "JunkIt_MCM", ToggleSelectedAsJunk);
	
	vm->RegisterFunction("IsItemJunk", "JunkIt_MCM", IsItemJunk);
	vm->RegisterFunction("GetJunkListSize", "JunkIt_MCM", GetJunkListSize);
	vm->RegisterFunction("GetJunkItemNameAt", "JunkIt_MCM", GetJunkItemNameAt);
	vm->RegisterFunction("RemoveJunkItemAtIndex", "JunkIt_MCM", RemoveJunkItemAtIndex);
	vm->RegisterFunction("ClearAllJunk", "JunkIt_MCM", ClearAllJunk);
	
	vm->RegisterFunction("GetContainerMode", "JunkIt_MCM", GetContainerMode);
	vm->RegisterFunction("GetMenuItemValue", "JunkIt_MCM", GetMenuItemValue);
	vm->RegisterFunction("RefreshDllSettings", "JunkIt_MCM", RefreshDllSettings);
	
	SKSE::log::info("Registered JunkIt Native Functions");
    return true;
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SKSE::Init(skse);
	SetupLog();

	DIII::ListenForRegistration(&DIIIMessageHandler);

    auto messaging = SKSE::GetMessagingInterface();
	if (!messaging->RegisterListener("SKSE", MessageHandler)) {
		return false;
	}

	auto serialization = SKSE::GetSerializationInterface();
	if (serialization) {
		serialization->SetUniqueID('JNKT');
		serialization->SetSaveCallback(JunkIt::JunkDataManager::OnSave);
		serialization->SetLoadCallback(JunkIt::JunkDataManager::OnLoad);
		serialization->SetRevertCallback(JunkIt::JunkDataManager::OnRevert);
		SKSE::log::info("Registered SKSE serialization callbacks");
	} else {
		SKSE::log::error("Failed to get SerializationInterface");
		return false;
	}

	SKSE::GetPapyrusInterface()->Register(BindPapyrusFunctions);

	SKSE::log::info("Setup Complete");
	
    return true;
}
