#include "log.h"


void OnDataLoaded()
{
   
}

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
	switch (a_msg->type) {
	case SKSE::MessagingInterface::kDataLoaded:
		break;
	case SKSE::MessagingInterface::kPostLoad:
		break;
	case SKSE::MessagingInterface::kPreLoadGame:
		break;
	case SKSE::MessagingInterface::kPostLoadGame:
        break;
	case SKSE::MessagingInterface::kNewGame:
		break;
	}
}

void RefreshUIIcons(RE::StaticFunctionTag*) {
	SKSE::log::info("RefreshUIIcons called");
	
	const auto UI = RE::UI::GetSingleton();
	RE::ItemList* itemListMenu = nullptr;

	const auto invMenu = UI ? UI->GetMenu<RE::InventoryMenu>() : nullptr;
	if (invMenu && !itemListMenu) {
		itemListMenu = invMenu->GetRuntimeData().itemList;
		SKSE::log::info("Attempting to refresh InventoryMenu");
	}

	const auto contMenu = UI ? UI->GetMenu<RE::ContainerMenu>() : nullptr;
	if (contMenu && !itemListMenu) {
		itemListMenu = contMenu->GetRuntimeData().itemList;
		SKSE::log::info("Attempting to refresh ContainerMenu");
	}
	
	const auto bartMenu = UI ? UI->GetMenu<RE::BarterMenu>() : nullptr;
	if (bartMenu && !itemListMenu) {
		itemListMenu = bartMenu->GetRuntimeData().itemList;
		SKSE::log::info("Attempting to refresh BarterMenu");
	}

	if (!itemListMenu) {
		SKSE::log::info("No ItemListMenu found");
		return;
	}

	// Print out the entry list for SCIENCE!
	RE::GFxValue entryList = itemListMenu->entryList;
	if (!entryList.IsArray()) {
		SKSE::log::info("Entry List is not an array");
		return;
	}

	SKSE::log::info("Entry List:");
	for (std::uint32_t i = 0, size = entryList.GetArraySize(); i < size; i++) {
		RE::GFxValue entryObject;
		entryList.GetElement(i, &entryObject);

		if (!entryObject.IsObject()) {
			SKSE::log::info("Entry {} is not an object", i);
			continue;
		}

		RE::GFxValue formId;
		entryObject.GetMember("formId", &formId);
		
		if (!formId.IsNumber())
		{
			SKSE::log::info("Form Id is not valid");
			continue;
		} else {
			SKSE::log::info("Form Id: {}", formId.GetNumber());
		}

		RE::TESObjectREFR* entryRefr = RE::TESForm::LookupByID<RE::TESObjectREFR>(static_cast<RE::FormID>(formId.GetNumber()));
		if (entryRefr) {
			SKSE::log::info("Entry Editor ID: {}", entryRefr->GetFormEditorID());
		} else {
			SKSE::log::info("Failed to look up Form ID: {}", formId.GetNumber());
		}

		RE::GFxValue iconSource;
		entryObject.GetMember("iconSource", &iconSource);

		if (!iconSource.IsString()) {
			SKSE::log::info("Icon Source is not a string");
		} else {
			SKSE::log::info("Icon Source: {}", iconSource.GetString());
		}

		RE::GFxValue iconLabel;
		entryObject.GetMember("iconLabel", &iconLabel);

		if (!iconLabel.IsString()) {
			SKSE::log::info("Icon Label is not a string");
		} else {
			SKSE::log::info("Icon Label: {}", iconLabel.GetString());
		}
		
		if (entryRefr) {
			if (const auto keywordForm = entryRefr->As<RE::BGSKeywordForm>(); keywordForm) {
				// Toggle the IsJunk keyword
				if (keywordForm->HasKeywordString("IsJunk")) {
					SKSE::log::info("Entry marked as junk");
				} else {
					SKSE::log::info("Entry is not junk");
				}
			}
		}
	}

	SKSE::log::info("Updating ItemList View");
	itemListMenu->Update();
}

bool BindPapyrusFunctions(RE::BSScript::IVirtualMachine* vm) {
	vm->RegisterFunction("RefreshUIIcons", "JunkIt_MCM", RefreshUIIcons);
	SKSE::log::info("Registered Papyrus Function: RefreshUIIcons");
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