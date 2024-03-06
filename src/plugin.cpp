#include "log.h"
#include "settings.h"
#include "junk.h"

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
			JunkIt::JunkHandler::UpdateItemKeywords();
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

RE::ItemList* GetItemListMenu() {
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
		return nullptr;
	}

	if (!itemListMenu) {
		SKSE::log::info("No ItemListMenu found");
		return nullptr;
	}

	return itemListMenu;
}

RE::TESForm* ToggleSelectedAsJunk(RE::StaticFunctionTag*) {
	RE::ItemList* itemListMenu = GetItemListMenu();
	if (!itemListMenu) {
		SKSE::log::error("No ItemListMenu found");
		return nullptr;
	}

	RE::ItemList::Item* selectedItem = itemListMenu->GetSelectedItem();
	if(!selectedItem) {
		SKSE::log::error("No item selected in ItemListMenu");
		return nullptr;
	}

	RE::InventoryEntryData* inventoryEntry = selectedItem->data.objDesc;
	RE::TESForm* itemForm = inventoryEntry->GetObject()->As<RE::TESForm>();
	if (!itemForm) {
		SKSE::log::error("Error getting item as form for {}", inventoryEntry->GetDisplayName());
		return nullptr;
	}

	RE::BGSKeyword* isJunkKYWD = JunkIt::Settings::GetIsJunkKYWD();
	RE::BGSKeywordForm* keywordForm = nullptr;

	// Ammo has to be treated differently as it does not inherit from BGSKeywordForm
	if (itemForm->GetFormType() == RE::FormType::Ammo) {
		RE::TESAmmo* ammo = itemForm->As<RE::TESAmmo>();
		keywordForm = ammo->AsKeywordForm();
	} else {
		// Generalized handling for all other form types
		keywordForm = itemForm->As<RE::BGSKeywordForm>();
	}
	
	if (!keywordForm) {
		SKSE::log::error("Error attempting to add IsJunk keyword to {}. Failed to typecast to BGSKeywordForm", itemForm->GetName());
		return nullptr;
	}

	if (inventoryEntry->IsQuestObject()) {
		SKSE::log::info("Cannot mark quest item {} as junk", inventoryEntry->GetDisplayName());
		if (!keywordForm->HasKeyword(isJunkKYWD)) { // Allow removal of keyword if it has it already for backward compatibility
			return nullptr;
		}
	}

	if (keywordForm->HasKeyword(isJunkKYWD)) {
		SKSE::log::info("Removing IsJunk keyword to {}", itemForm->GetName());
		keywordForm->RemoveKeyword(isJunkKYWD);
	} else {
		SKSE::log::info("Adding IsJunk keyword to {}", itemForm->GetName());
		keywordForm->AddKeyword(isJunkKYWD);
	}

	itemListMenu->Update();
	return itemForm;
}

std::int32_t AddJunkKeyword(RE::StaticFunctionTag*, RE::TESForm* a_form) {
	if (!a_form) {
		SKSE::log::error("Error attempting to add IsJunk keyword to nullptr");
		return false;
	}

	SKSE::log::info("Adding IsJunk keyword to {}", a_form->GetName());
	RE::BGSKeyword* isJunkKYWD = JunkIt::Settings::GetIsJunkKYWD();

	// Ammo has to be treated differently as it does not inherit from BGSKeywordForm
	if (a_form->GetFormType() == RE::FormType::Ammo) {
		RE::TESAmmo* ammo = a_form->As<RE::TESAmmo>();
		ammo->AsKeywordForm()->AddKeyword(isJunkKYWD);
		return true;
	}

	// Generalized handling for all other form types
	RE::BGSKeywordForm* keywordForm = a_form->As<RE::BGSKeywordForm>();
	if (!keywordForm) {
		SKSE::log::error("Error attempting to add IsJunk keyword to {}. Failed to typecast to BGSKeywordForm", a_form->GetName());
		return false;
	}

	keywordForm->AddKeyword(isJunkKYWD);

	RE::ItemList* itemListMenu = GetItemListMenu();
	if (itemListMenu) {
		itemListMenu->Update();
	}

	return true;
}

std::int32_t RemoveJunkKeyword(RE::StaticFunctionTag*, RE::TESForm* a_form) {
	if (!a_form) {
		SKSE::log::error("Error attempting to remove IsJunk keyword to nullptr");
		return false;
	}

	SKSE::log::info("Remove IsJunk keyword from {}", a_form->GetName());
	RE::BGSKeyword* isJunkKYWD = JunkIt::Settings::GetIsJunkKYWD();

	// Ammo has to be treated differently as it does not inherit from BGSKeywordForm
	if (a_form->GetFormType() == RE::FormType::Ammo) {
		RE::TESAmmo* ammo = a_form->As<RE::TESAmmo>();
		ammo->AsKeywordForm()->RemoveKeyword(isJunkKYWD);
		return true;
	}

	// Generalized handling for all other form types
	RE::BGSKeywordForm* keywordForm = a_form->As<RE::BGSKeywordForm>();
	if (!keywordForm) {
		SKSE::log::error("Error attempting to add IsJunk keyword to {}. Failed to typecast to BGSKeywordForm", a_form->GetName());
		return false;
	}

	keywordForm->RemoveKeyword(isJunkKYWD);
	
	RE::ItemList* itemListMenu = GetItemListMenu();
	if (itemListMenu) {
		itemListMenu->Update();
	}
	
	return true;
}

void FreezeItemListUI(RE::StaticFunctionTag*) {
	RE::ItemList* itemListMenu = GetItemListMenu();
	if (itemListMenu) {
		itemListMenu->view->SetPause(true);
	}
}

void ThawItemListUI(RE::StaticFunctionTag*) {
	RE::ItemList* itemListMenu = GetItemListMenu();
	if (itemListMenu) {
		itemListMenu->view->SetPause(false);
	}
}

void RefreshUIIcons(RE::StaticFunctionTag*) {
	RE::ItemList* itemListMenu = GetItemListMenu();
	if (itemListMenu) {
		if (itemListMenu->view->IsPaused()) {
			itemListMenu->view->SetPause(false);
			itemListMenu->Update();
		} else {
			itemListMenu->Update();
		}
		itemListMenu->Update();
	}
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
	SKSE::log::info(" ");
	SKSE::log::info("---- Getting Vendor actor ----");
	RE::TESObjectREFR* container = nullptr;
	
	const auto UI = RE::UI::GetSingleton();
	const auto menu = UI ? UI->GetMenu<RE::BarterMenu>() : nullptr;
	if (menu) {
		const auto           refHandle = menu->GetTargetRefHandle();
		RE::TESObjectREFRPtr refr;
		RE::LookupReferenceByHandle(refHandle, refr);
		container = refr.get();
	}

	if (!container) {
		SKSE::log::info("No merchant actor container found");
	}

	return container;
}

RE::TESObjectREFR* GetBarterMenuMerchantContainer(RE::StaticFunctionTag*) {
	SKSE::log::info(" ");
	SKSE::log::info("---- Getting Merchant Vendor Faction Container ----");
	RE::TESObjectREFR* container = nullptr;
	
	const auto UI = RE::UI::GetSingleton();
	const auto menu = UI ? UI->GetMenu<RE::BarterMenu>() : nullptr;
	if (menu) {
		const auto           refHandle = menu->GetTargetRefHandle();
		RE::TESObjectREFRPtr refr;
		RE::LookupReferenceByHandle(refHandle, refr);

		RE::TESObjectREFR* merchantRef = nullptr;
		merchantRef = refr.get();

		if (!merchantRef) {
			SKSE::log::info("No merchant actor found");
		}
		
		RE::Actor* merchant = merchantRef->As<RE::Actor>();
		RE::TESFaction* merchantFaction = merchant->GetVendorFaction();

		if (!merchantFaction) {
			SKSE::log::info("No merchant faction found - using vendor actor as container");
			return merchantRef;
		} else {
			SKSE::log::info("Merchant faction found with id {} - using faction->merchantContainer", merchantFaction->GetFormID());
			container = merchantFaction->vendorData.merchantContainer;
		}

		if (!container) {
			SKSE::log::info("No merchant container found for this actors merchant faction - using vendor actor as container");
			return merchantRef;
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

std::int32_t TransferJunkList(RE::StaticFunctionTag*, RE::BGSListForm* Items, RE::TESObjectREFR* fromContainer, RE::TESObjectREFR* toContainer, std::int32_t a_count = -1) {
	SKSE::log::info(" ");
	SKSE::log::info("---- Transferring Junk ----");

	std::int32_t transferCount = 0;

	RE::ItemList* itemListMenu = GetItemListMenu();
	if (itemListMenu) {
		SKSE::log::info("Pausing ItemListMenu");
		itemListMenu->view->SetPause(true);
	}

	// For each form in the Items RE::BGSListForm, get the count of items in the fromContainer and removeItem from fromContainer and addItem to toContainer
	Items->ForEachForm([&](RE::TESForm& form) {
		RE::TESBoundObject* item = form.As<RE::TESBoundObject>();
		if (!item) {
			return RE::BSContainer::ForEachResult::kContinue;
		}

		std::int32_t itemCount = a_count;

		// If a_count is -1, get the count of items in the fromContainer
		if (a_count == -1) {
			TESContainer* container = fromContainer->GetContainer();
			itemCount = container->CountObjectsInContainer(item);
			SKSE::log::info("Counting {} {} in {}", itemCount, item->GetName(), fromContainer->GetName());
		}

		// If the item is not in the fromContainer, skip it
		if (itemCount > 0) {
			SKSE::log::info("Transferring {} {} from {} to {}", itemCount, item->GetName(), fromContainer->GetName(), toContainer->GetName());
			fromContainer->AddObjectToContainer(item, nullptr, itemCount, toContainer);
			transferCount += itemCount;
		}

		return RE::BSContainer::ForEachResult::kContinue;
	});

	if (itemListMenu) {
		SKSE::log::info("Resuming ItemListMenu and Updating");
		itemListMenu->view->SetPause(false);
		itemListMenu->Update();
	}
	
	return transferCount;
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

	SKSE::log::info("---- Generated Junk Transfer FormList ----");
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

	SKSE::log::info("---- Generated Junk Sell FormList ----");
	SKSE::log::info(" ");
	return sellList;
}

std::int32_t GetMenuItemValue(RE::StaticFunctionTag*, RE::TESForm* a_form) {
	SKSE::log::info(" ");
	SKSE::log::info("---- Getting Item Value as it is listed in the open menu for {} [{}] ----", a_form->GetName(), a_form->GetFormID());
	std::int32_t goldValue = -1;

	// Get access to the UI Menu so we can limit our transfer based on the current view and Equip/Favorite state
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
		return goldValue;
	}

	if (!itemListMenu) {
		SKSE::log::error("No ItemListMenu found");
		return goldValue;
	}

	// Get item list from the ItemListMenu
	RE::BSTArray<RE::ItemList::Item*> listItems = itemListMenu->items;
	SKSE::log::info("Parsing ItemList[{}] for the matching enchanted item", listItems.size());

	// Loop through the entry list to find the selected index for the formId
	for (std::uint32_t i = 0, size = listItems.size(); i < size; i++) {
		RE::ItemList::Item* entryItem = listItems[i];
		if (!entryItem) {
			continue;
		}

		RE::InventoryEntryData* entryData = entryItem->data.objDesc;
		if (!entryData) {
			continue;
		}

		RE::TESBoundObject* entryObject = entryData->GetObject();
		if (!entryObject) {
			continue;
		}

		RE::FormID formId = entryObject->GetFormID();
		if (formId == a_form->GetFormID()) {
			goldValue = entryData->GetValue();
			SKSE::log::info("{} - EntryData->GetValue() {}", entryObject->GetName(), goldValue);
			break;
		}
	}

	return goldValue;
}

bool BindPapyrusFunctions(RE::BSScript::IVirtualMachine* vm) {
	vm->RegisterFunction("RefreshUIIcons", "JunkIt_MCM", RefreshUIIcons);

	vm->RegisterFunction("ToggleSelectedAsJunk", "JunkIt_MCM", ToggleSelectedAsJunk);

	vm->RegisterFunction("GetContainerMode", "JunkIt_MCM", GetContainerMode);
	vm->RegisterFunction("GetContainerMenuContainer", "JunkIt_MCM", GetContainerMenuContainer);
	vm->RegisterFunction("GetBarterMenuContainer", "JunkIt_MCM", GetBarterMenuContainer);
	vm->RegisterFunction("GetBarterMenuMerchantContainer", "JunkIt_MCM", GetBarterMenuMerchantContainer);
	vm->RegisterFunction("GetTransferFormList", "JunkIt_MCM", GetTransferFormList);
	vm->RegisterFunction("GetSellFormList", "JunkIt_MCM", GetSellFormList);
	vm->RegisterFunction("GetMenuItemValue", "JunkIt_MCM", GetMenuItemValue);
	vm->RegisterFunction("FreezeItemListUI", "JunkIt_MCM", FreezeItemListUI);
	vm->RegisterFunction("ThawItemListUI", "JunkIt_MCM", ThawItemListUI);
	vm->RegisterFunction("TransferJunkList", "JunkIt_MCM", TransferJunkList);
	
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