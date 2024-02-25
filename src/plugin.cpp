#include "log.h"
#include "settings.h"

void OnDataLoaded() {
   
}

void MessageHandler(SKSE::MessagingInterface::Message* a_msg) {
	switch (a_msg->type) {
		case SKSE::MessagingInterface::kDataLoaded:
			break;
		case SKSE::MessagingInterface::kPostLoad:
			break;
		case SKSE::MessagingInterface::kPreLoadGame:
			break;
		case SKSE::MessagingInterface::kPostLoadGame:
			JunkIt::Settings::Load();
			break;
		case SKSE::MessagingInterface::kNewGame:
			break;
	}
}

void RefreshDllSettings(RE::StaticFunctionTag*) {
	SKSE::log::info(" ");
	SKSE::log::info("RefreshDllSettings called");
	JunkIt::Settings::Load();
}

std::int32_t AddJunkKeyword(RE::StaticFunctionTag*, RE::TESForm* a_form) {
	SKSE::log::info("Adding IsJunk keyword to {}", a_form->GetName());
	RE::BGSKeyword* isJunkKYWD = JunkIt::Settings::GetIsJunkKYWD();

	RE::BGSKeywordForm* keywordForm = a_form->As<RE::BGSKeywordForm>();
	if (!keywordForm) {
		SKSE::log::error("Error attempting to add IsJunk keyword to {}. Failed to typecast to BGSKeywordForm", a_form->GetName());
		return false;
	}

	keywordForm->AddKeyword(isJunkKYWD);
	return true;
}

std::int32_t RemoveJunkKeyword(RE::StaticFunctionTag*, RE::TESForm* a_form) {
	SKSE::log::info("Remove IsJunk keyword from {}", a_form->GetName());
	RE::BGSKeyword* isJunkKYWD = JunkIt::Settings::GetIsJunkKYWD();

	RE::BGSKeywordForm* keywordForm = a_form->As<RE::BGSKeywordForm>();
	if (!keywordForm) {
		SKSE::log::error("Error attempting to add IsJunk keyword to {}. Failed to typecast to BGSKeywordForm", a_form->GetName());
		return false;
	}

	keywordForm->RemoveKeyword(isJunkKYWD);
	return true;
}

void RefreshUIIcons(RE::StaticFunctionTag*) {
	const auto UI = RE::UI::GetSingleton();
	RE::ItemList* itemListMenu = nullptr;
	if (UI && UI->IsMenuOpen("InventoryMenu")) {
		itemListMenu = UI->GetMenu<RE::InventoryMenu>()->GetRuntimeData().itemList;
	} else if (UI && UI->IsMenuOpen("ContainerMenu")) {
		itemListMenu = UI->GetMenu<RE::ContainerMenu>()->GetRuntimeData().itemList;
	} else if (UI && UI->IsMenuOpen("BarterMenu")) {
		itemListMenu = UI->GetMenu<RE::BarterMenu>()->GetRuntimeData().itemList;
	} else	{
		SKSE::log::info("No open menu found");
		return;
	}

	if (!itemListMenu) {
		SKSE::log::info("No ItemListMenu found");
		return;
	}
	
	itemListMenu->Update();
}

RE::TESObjectREFR* GetContainerMenuContainer(RE::StaticFunctionTag*) {
	RE::TESObjectREFR* container = nullptr;

	const auto UI = RE::UI::GetSingleton();
	const auto menu = UI ? UI->GetMenu<RE::ContainerMenu>() : nullptr;
	if (menu) {
		const auto           refHandle = menu->GetTargetRefHandle();
		RE::TESObjectREFRPtr refr;
		RE::LookupReferenceByHandle(refHandle, refr);

		container = refr.get();
	}

	return container;
}

RE::TESObjectREFR* GetBarterMenuContainer(RE::StaticFunctionTag*) {
	RE::TESObjectREFR* container = nullptr;
	
	const auto UI = RE::UI::GetSingleton();
	const auto menu = UI ? UI->GetMenu<RE::BarterMenu>() : nullptr;
	if (menu) {
		const auto           refHandle = menu->GetTargetRefHandle();
		RE::TESObjectREFRPtr refr;
		RE::LookupReferenceByHandle(refHandle, refr);
		container = refr.get();
	}

	return container;
}

RE::TESObjectREFR* GetBarterMenuMerchantContainer(RE::StaticFunctionTag*) {
	RE::TESObjectREFR* container = nullptr;
	
	const auto UI = RE::UI::GetSingleton();
	const auto menu = UI ? UI->GetMenu<RE::BarterMenu>() : nullptr;
	if (menu) {
		const auto           refHandle = menu->GetTargetRefHandle();
		RE::TESObjectREFRPtr refr;
		RE::LookupReferenceByHandle(refHandle, refr);

		RE::TESObjectREFR* merchantRef = nullptr;
		merchantRef = refr.get();
		
		RE::Actor* merchant = merchantRef->As<RE::Actor>();
		RE::TESFaction* merchantFaction = merchant->GetVendorFaction();
		container = merchantFaction->vendorData.merchantContainer;

		if (!container) {
			container = merchantRef;
		}
	}

	return container;
}

RE::ContainerMenu::ContainerMode GetContainerMode(RE::StaticFunctionTag*) {
	const auto UI = RE::UI::GetSingleton();
	const auto containerMenu = UI ? UI->GetMenu<RE::ContainerMenu>() : nullptr;
	if (!containerMenu) {
		SKSE::log::info("No open menu found");
		return RE::ContainerMenu::ContainerMode::kLoot;
	}

	RE::ContainerMenu::ContainerMode mode = containerMenu->GetContainerMode();
	SKSE::log::info("Container Mode: {}", static_cast<std::uint32_t>(mode));
	return mode;
}

RE::BGSListForm* GetTransferFormList(RE::StaticFunctionTag*) {
	SKSE::log::info(" ");
	SKSE::log::info("---- Finding Transferrable Junk ----");

	RE::BGSListForm* JunkList = JunkIt::Settings::GetJunkList();
	RE::BGSListForm* transferList = JunkIt::Settings::GetTransferList();
	transferList->ClearData();

	// Get access to the UI Menu so we can limit our transfer based on the current view and Equip/Favorite state
	const auto UI = RE::UI::GetSingleton();
	RE::GPtr<RE::ContainerMenu> containerMenu = UI ? UI->GetMenu<RE::ContainerMenu>() : nullptr;
	RE::ItemList* itemListMenu = containerMenu ? containerMenu->GetRuntimeData().itemList : nullptr;
	if (!itemListMenu) {
		SKSE::log::error("No ItemListMenu found");
		return transferList;
	}

	// Get entry list from the ItemListMenu
	RE::BSTArray<RE::ItemList::Item*> listItems = itemListMenu->items;
	std::vector<RE::InventoryEntryData*> sortFormData;
	
	SKSE::log::info("Processing Entry List for transferable junk items");
	for (std::uint32_t i = 0, size = listItems.size(); i < size; i++) {
		RE::ItemList::Item* entryItem = listItems[i];
		if (!entryItem) {
			continue;
		}

		// Skip items that are not in the JunkList
		if (!JunkList->HasForm(entryItem->data.objDesc->GetObject())) {
			continue;
		}

		bool equipped = entryItem->data.objDesc->IsWorn();
		if (JunkIt::Settings::ProtectEquipped() && equipped) {
			SKSE::log::info("Junk Item Equipped - Skipping {}", entryItem->data.objDesc->GetObject()->GetName());
			continue;
		}
		std::uint32_t favorite = entryItem->data.objDesc->IsFavorited();
		if (JunkIt::Settings::ProtectFavorites() && favorite) {
			SKSE::log::info("Junk Item Favorited - Skipping {}", entryItem->data.objDesc->GetObject()->GetName());
			continue;
		}

		sortFormData.push_back(entryItem->data.objDesc);
	}

	// Sort the list of items based on the settings
	if (JunkIt::Settings::GetTransferPriority() == JunkIt::Settings::SortPriority::kWeightHighLow)
	{
		SKSE::log::info("Applying Weight [High > Low] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const RE::InventoryEntryData* a, const RE::InventoryEntryData* b) {
			return a->GetWeight() > b->GetWeight();
		});
	}
	else if (JunkIt::Settings::GetTransferPriority() == JunkIt::Settings::SortPriority::kWeightLowHigh)
	{
		SKSE::log::info("Applying Weight [Low > High] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const RE::InventoryEntryData* a, const RE::InventoryEntryData* b) {
			return a->GetWeight() < b->GetWeight();
		});
	}
	else if (JunkIt::Settings::GetTransferPriority() == JunkIt::Settings::SortPriority::kValueHighLow)
	{
		SKSE::log::info("Applying Value [High > Low] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const RE::InventoryEntryData* a, const RE::InventoryEntryData* b) {
			return a->GetValue() > b->GetValue();
		});
	}
	else if (JunkIt::Settings::GetTransferPriority() == JunkIt::Settings::SortPriority::kValueLowHigh)
	{
		SKSE::log::info("Applying Value [Low > High] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const RE::InventoryEntryData* a, const RE::InventoryEntryData* b) {
			return a->GetValue() < b->GetValue();
		});
	}
	else if (JunkIt::Settings::GetTransferPriority() == JunkIt::Settings::SortPriority::kValueWeightHighLow)
	{
		SKSE::log::info("Applying Value/Weight [High > Low] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const RE::InventoryEntryData* a, const RE::InventoryEntryData* b) {
			return (a->GetValue() / a->GetWeight()) > (b->GetValue() / b->GetWeight());
		});
	}
	else if (JunkIt::Settings::GetTransferPriority() == JunkIt::Settings::SortPriority::kValueWeightLowHigh)
	{
		SKSE::log::info("Applying Value/Weight [Low > High] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const RE::InventoryEntryData* a, const RE::InventoryEntryData* b) {
			return (a->GetValue() / a->GetWeight()) < (b->GetValue() / b->GetWeight());
		});
	}

	// Add the sorted list of items to the transfer list
	SKSE::log::info("Final TransferList:");
	for (const RE::InventoryEntryData* entryData : sortFormData) {
		const RE::TESBoundObject* entryObject = entryData->GetObject();
		if (!entryObject) {
			continue;
		}
		if (!transferList->HasForm(entryObject)) {
			SKSE::log::info("     {} - Val({}) Weight({})", entryObject->GetName(), entryData->GetValue(), entryData->GetWeight());
			transferList->AddForm(RE::TESForm::LookupByID(entryObject->GetFormID()));
		}
	}

	SKSE::log::info(" ");
	return transferList;
}

RE::BGSListForm* GetSellFormList(RE::StaticFunctionTag*) {
	SKSE::log::info(" ");
	SKSE::log::info("---- Finding Sellable Junk ----");

	RE::BGSListForm* JunkList = JunkIt::Settings::GetJunkList();
	RE::BGSListForm* sellList = JunkIt::Settings::GetSellList();
	sellList->ClearData();

	// Get access to the UI Menu so we can limit our transfer based on the current view and Equip/Favorite state
	const auto UI = RE::UI::GetSingleton();
	RE::GPtr<RE::BarterMenu> barterMenu = UI ? UI->GetMenu<RE::BarterMenu>() : nullptr;
	RE::ItemList* itemListMenu = barterMenu ? barterMenu->GetRuntimeData().itemList : nullptr;
	if (!itemListMenu) {
		SKSE::log::error("No ItemListMenu found");
		return sellList;
	}

	// Get entry list from the ItemListMenu
	RE::BSTArray<RE::ItemList::Item*> listItems = itemListMenu->items;
	std::vector<RE::InventoryEntryData*> sortFormData;
	
	SKSE::log::info("Processing Entry List for sellable junk items");
	for (std::uint32_t i = 0, size = listItems.size(); i < size; i++) {
		RE::ItemList::Item* entryItem = listItems[i];
		if (!entryItem) {
			continue;
		}

		// Skip items that are not in the JunkList
		if (!JunkList->HasForm(entryItem->data.objDesc->GetObject())) {
			continue;
		}

		bool equipped = entryItem->data.objDesc->IsWorn();
		if (JunkIt::Settings::ProtectEquipped() && equipped) {
			SKSE::log::info("Junk Item Equipped - Skipping {}", entryItem->data.objDesc->GetObject()->GetName());
			continue;
		}
		std::uint32_t favorite = entryItem->data.objDesc->IsFavorited();
		if (JunkIt::Settings::ProtectFavorites() && favorite) {
			SKSE::log::info("Junk Item Favorited - Skipping {}", entryItem->data.objDesc->GetObject()->GetName());
			continue;
		}
		std::uint32_t enchanted = entryItem->data.objDesc->IsEnchanted();
		if (JunkIt::Settings::ProtectEnchanted() && enchanted) {
			SKSE::log::info("Junk Item Enchanted - Skipping {}", entryItem->data.objDesc->GetObject()->GetName());
			continue;
		}

		sortFormData.push_back(entryItem->data.objDesc);
	}

	// Sort the list of items based on the settings
	if (JunkIt::Settings::GetSellPriority() == JunkIt::Settings::SortPriority::kWeightHighLow)
	{
		SKSE::log::info("Applying Weight [High > Low] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const RE::InventoryEntryData* a, const RE::InventoryEntryData* b) {
			return a->GetWeight() > b->GetWeight();
		});
	}
	else if (JunkIt::Settings::GetSellPriority() == JunkIt::Settings::SortPriority::kWeightLowHigh)
	{
		SKSE::log::info("Applying Weight [Low > High] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const RE::InventoryEntryData* a, const RE::InventoryEntryData* b) {
			return a->GetWeight() < b->GetWeight();
		});
	}
	else if (JunkIt::Settings::GetSellPriority() == JunkIt::Settings::SortPriority::kValueHighLow)
	{
		SKSE::log::info("Applying Value [High > Low] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const RE::InventoryEntryData* a, const RE::InventoryEntryData* b) {
			return a->GetValue() > b->GetValue();
		});
	}
	else if (JunkIt::Settings::GetSellPriority() == JunkIt::Settings::SortPriority::kValueLowHigh)
	{
		SKSE::log::info("Applying Value [Low > High] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const RE::InventoryEntryData* a, const RE::InventoryEntryData* b) {
			return a->GetValue() < b->GetValue();
		});
	}
	else if (JunkIt::Settings::GetSellPriority() == JunkIt::Settings::SortPriority::kValueWeightHighLow)
	{
		SKSE::log::info("Applying Value/Weight [High > Low] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const RE::InventoryEntryData* a, const RE::InventoryEntryData* b) {
			return (a->GetValue() / a->GetWeight()) > (b->GetValue() / b->GetWeight());
		});
	}
	else if (JunkIt::Settings::GetSellPriority() == JunkIt::Settings::SortPriority::kValueWeightLowHigh)
	{
		SKSE::log::info("Applying Value/Weight [Low > High] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const RE::InventoryEntryData* a, const RE::InventoryEntryData* b) {
			return (a->GetValue() / a->GetWeight()) < (b->GetValue() / b->GetWeight());
		});
	}

	// Add the sorted list of items to the transfer list
	SKSE::log::info("Final SellList:");
	for (const RE::InventoryEntryData* entryData : sortFormData) {
		const RE::TESBoundObject* entryObject = entryData->GetObject();
		if (!entryObject) {
			continue;
		}
		if (!sellList->HasForm(entryObject)) {
			SKSE::log::info("     {} - Val({}) Weight({})", entryObject->GetName(), entryData->GetValue(), entryData->GetWeight());
			sellList->AddForm(RE::TESForm::LookupByID(entryObject->GetFormID()));
		}
	}

	SKSE::log::info("---- Bulk Sell Complete ----");
	SKSE::log::info(" ");
	return sellList;
}

std::int32_t GetFormUIEntryIndex(RE::StaticFunctionTag*, std::string menuName, std::int32_t formId) {
	SKSE::log::info(" ");
	SKSE::log::info("---- Finding FormId {} EntryList Index ----", formId);
	std::int32_t index = -1;
	
	// Get access to the UI Menu so we can limit our transfer based on the current view and Equip/Favorite state
	const auto UI = RE::UI::GetSingleton();
	
	RE::ItemList* itemListMenu = nullptr;
	if (menuName == "InventoryMenu") {
		RE::GPtr<RE::InventoryMenu> invMenu = UI ? UI->GetMenu<RE::InventoryMenu>() : nullptr;
		itemListMenu = invMenu ? invMenu->GetRuntimeData().itemList : nullptr;
	} else if (menuName == "ContainerMenu") {
		RE::GPtr<RE::ContainerMenu> containerMenu = UI ? UI->GetMenu<RE::ContainerMenu>() : nullptr;
		itemListMenu = containerMenu ? containerMenu->GetRuntimeData().itemList : nullptr;
	} else if (menuName == "BarterMenu") {
		RE::GPtr<RE::BarterMenu> barterMenu = UI ? UI->GetMenu<RE::BarterMenu>() : nullptr;
		itemListMenu = barterMenu ? barterMenu->GetRuntimeData().itemList : nullptr;
	}
	
	if (!itemListMenu) {
		SKSE::log::error("No ItemListMenu found");
		return index;
	}

	// Get entry list from the ItemListMenu
	RE::GFxValue entryList = itemListMenu->entryList;
	SKSE::log::info("EntryList size {}", entryList.GetArraySize());

	// Loop through the entry list to find the selected index for the formId
	for (std::uint32_t i = 0, size = entryList.GetArraySize(); i < size; i++) {
		RE::GFxValue entry;
		entryList.GetElement(i, &entry);
		RE::GFxValue formID;
		entry.GetMember("formId", &formID);
		RE::GFxValue itemIndex;
		entry.GetMember("itemIndex", &itemIndex);
		if (formID.GetNumber() == formId) {
			SKSE::log::info("Entry formId {} with itemIndex {}", formID.GetNumber(), itemIndex.GetNumber());
			index = static_cast<std::int32_t>(itemIndex.GetNumber());
			break;
		}
	}

	return index;
}

bool BindPapyrusFunctions(RE::BSScript::IVirtualMachine* vm) {
	vm->RegisterFunction("RefreshUIIcons", "JunkIt_MCM", RefreshUIIcons);

	vm->RegisterFunction("GetContainerMode", "JunkIt_MCM", GetContainerMode);
	vm->RegisterFunction("GetContainerMenuContainer", "JunkIt_MCM", GetContainerMenuContainer);
	vm->RegisterFunction("GetBarterMenuContainer", "JunkIt_MCM", GetBarterMenuContainer);
	vm->RegisterFunction("GetBarterMenuMerchantContainer", "JunkIt_MCM", GetBarterMenuMerchantContainer);
	vm->RegisterFunction("GetTransferFormList", "JunkIt_MCM", GetTransferFormList);
	vm->RegisterFunction("GetSellFormList", "JunkIt_MCM", GetSellFormList);
	// vm->RegisterFunction("GetFormUIEntryIndex", "JunkIt_MCM", GetFormUIEntryIndex);

	vm->RegisterFunction("RefreshDllSettings", "JunkIt_MCM", RefreshDllSettings);

	vm->RegisterFunction("AddJunkKeyword", "JunkIt_MCM", AddJunkKeyword);
	vm->RegisterFunction("RemoveJunkKeyword", "JunkIt_MCM", RemoveJunkKeyword);
	
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