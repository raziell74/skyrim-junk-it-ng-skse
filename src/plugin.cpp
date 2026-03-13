#include "log.h"
#include "settings.h"
#include "junk.h"
#include "event.h"

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
			JunkIt::JunkHandler::UpdateItemKeywords();
			break;
		case SKSE::MessagingInterface::kNewGame:
			break;
		case SKSE::MessagingInterface::kSaveGame:
			if (JunkIt::Settings::GetAutoSaveJunkListToFile()) {
				JunkIt::Settings::SaveJunkListToFile();
			}
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
	return JunkIt::JunkHandler::ToggleSelectedItemKeyword();
}

std::int32_t AddJunkKeyword(RE::StaticFunctionTag*, RE::TESForm* a_form) {
	if (!a_form) {
		SKSE::log::error("Error attempting to add IsJunk keyword to nullptr");
		return false;
	}

	SKSE::log::info("Adding IsJunk keyword to {}", a_form->GetName());
	RE::BGSKeyword* isJunkKYWD = JunkIt::Settings::GetIsJunkKYWD();

	if (a_form->GetFormType() == RE::FormType::Ammo) {
		RE::TESAmmo* ammo = a_form->As<RE::TESAmmo>();
		ammo->AsKeywordForm()->AddKeyword(isJunkKYWD);
		return true;
	}

	RE::BGSKeywordForm* keywordForm = a_form->As<RE::BGSKeywordForm>();
	if (!keywordForm) {
		SKSE::log::error("Error attempting to add IsJunk keyword to {}. Failed to typecast to BGSKeywordForm", a_form->GetName());
		return false;
	}

	keywordForm->AddKeyword(isJunkKYWD);
	UIUtil::ItemList::Refresh();
	return true;
}

std::int32_t RemoveJunkKeyword(RE::StaticFunctionTag*, RE::TESForm* a_form) {
	if (!a_form) {
		SKSE::log::error("Error attempting to remove IsJunk keyword from nullptr");
		return false;
	}

	SKSE::log::info("Remove IsJunk keyword from {}", a_form->GetName());
	RE::BGSKeyword* isJunkKYWD = JunkIt::Settings::GetIsJunkKYWD();

	if (a_form->GetFormType() == RE::FormType::Ammo) {
		RE::TESAmmo* ammo = a_form->As<RE::TESAmmo>();
		ammo->AsKeywordForm()->RemoveKeyword(isJunkKYWD);
		return true;
	}

	RE::BGSKeywordForm* keywordForm = a_form->As<RE::BGSKeywordForm>();
	if (!keywordForm) {
		SKSE::log::error("Error attempting to remove IsJunk keyword from {}. Failed to typecast to BGSKeywordForm", a_form->GetName());
		return false;
	}

	keywordForm->RemoveKeyword(isJunkKYWD);
	UIUtil::ItemList::Refresh();
	return true;
}

RE::TESObjectREFR* GetContainerMenuContainer(RE::StaticFunctionTag*) {
	return JunkIt::JunkHandler::GetContainerMenuContainer();
}

RE::TESObjectREFR* GetBarterMenuContainer(RE::StaticFunctionTag*) {
	return JunkIt::JunkHandler::GetBarterMenuContainer();
}

RE::TESObjectREFR* GetBarterMenuMerchantContainer(RE::StaticFunctionTag*) {
	return JunkIt::JunkHandler::GetBarterMenuMerchantContainer();
}

RE::ContainerMenu::ContainerMode GetContainerMode(RE::StaticFunctionTag*) {
	return JunkIt::JunkHandler::GetContainerMode();
}

RE::BGSListForm* GetTransferFormList(RE::StaticFunctionTag*) {
	return JunkIt::JunkHandler::BuildTransferFormList();
}

RE::BGSListForm* GetSellFormList(RE::StaticFunctionTag*) {
	return JunkIt::JunkHandler::BuildSellFormList();
}

std::int32_t GetMenuItemValue(RE::StaticFunctionTag*, RE::TESForm* a_form) {
	return JunkIt::JunkHandler::GetMenuItemValue(a_form);
}

void SaveJunkListToFile(RE::StaticFunctionTag*) {
	JunkIt::Settings::SaveJunkListToFile();
}

RE::BGSListForm* LoadJunkListFromFile(RE::StaticFunctionTag*) {
	return JunkIt::Settings::LoadJunkListFromFile();
}

void UpdateItemKeywords(RE::StaticFunctionTag*) {
	JunkIt::JunkHandler::UpdateItemKeywords();
}

std::int32_t ProcessItemListTransfer(RE::StaticFunctionTag*, RE::BGSListForm* a_itemList, RE::TESObjectREFR* a_fromContainer, RE::TESObjectREFR* a_toContainer, std::int32_t a_isBarter = 0) {
	return JunkIt::JunkHandler::ProcessItemListTransfer(a_itemList, a_fromContainer, a_toContainer, a_isBarter);
}

std::int32_t GetContainerItemListCount(RE::StaticFunctionTag*, RE::TESObjectREFR* a_container, RE::BGSListForm* a_itemList) {
	return JunkIt::JunkHandler::GetContainerItemListCount(a_container, a_itemList);
}

std::int32_t GetContainerSingleItemCount(RE::StaticFunctionTag*, RE::TESObjectREFR* a_container, RE::TESForm* a_item) {
	return JunkIt::JunkHandler::GetContainerSingleItemCount(a_container, a_item);
}

bool BindPapyrusFunctions(RE::BSScript::IVirtualMachine* vm) {
	vm->RegisterFunction("RefreshUIIcons", "JunkIt_MCM", RefreshUIIcons);

	vm->RegisterFunction("ToggleSelectedAsJunk", "JunkIt_MCM", ToggleSelectedAsJunk);
	vm->RegisterFunction("UpdateItemKeywords", "JunkIt_MCM", UpdateItemKeywords);

	vm->RegisterFunction("GetContainerMode", "JunkIt_MCM", GetContainerMode);
	vm->RegisterFunction("GetContainerMenuContainer", "JunkIt_MCM", GetContainerMenuContainer);
	vm->RegisterFunction("GetBarterMenuContainer", "JunkIt_MCM", GetBarterMenuContainer);
	vm->RegisterFunction("GetBarterMenuMerchantContainer", "JunkIt_MCM", GetBarterMenuMerchantContainer);
	vm->RegisterFunction("GetTransferFormList", "JunkIt_MCM", GetTransferFormList);
	vm->RegisterFunction("GetSellFormList", "JunkIt_MCM", GetSellFormList);
	vm->RegisterFunction("GetMenuItemValue", "JunkIt_MCM", GetMenuItemValue);
	
	vm->RegisterFunction("RefreshDllSettings", "JunkIt_MCM", RefreshDllSettings);

	vm->RegisterFunction("AddJunkKeyword", "JunkIt_MCM", AddJunkKeyword);
	vm->RegisterFunction("RemoveJunkKeyword", "JunkIt_MCM", RemoveJunkKeyword);

	vm->RegisterFunction("SaveJunkListToFile", "JunkIt_MCM", SaveJunkListToFile);
	vm->RegisterFunction("LoadJunkListFromFile", "JunkIt_MCM", LoadJunkListFromFile);

	vm->RegisterFunction("ProcessItemListTransfer", "JunkIt_MCM", ProcessItemListTransfer);
	vm->RegisterFunction("GetContainerItemListCount", "JunkIt_MCM", GetContainerItemListCount);
	vm->RegisterFunction("GetContainerSingleItemCount", "JunkIt_MCM", GetContainerSingleItemCount);
	
	SKSE::log::info("Registered JunkIt Native Functions");
    return true;
}

SKSEPluginLoad(const SKSE::LoadInterface *skse) {
    SKSE::Init(skse);
	SetupLog();

    auto messaging = SKSE::GetMessagingInterface();
	if (!messaging->RegisterListener("SKSE", MessageHandler)) {
		return false;
	}

	SKSE::GetPapyrusInterface()->Register(BindPapyrusFunctions);

	SKSE::log::info("Setup Complete");
	
    return true;
}
