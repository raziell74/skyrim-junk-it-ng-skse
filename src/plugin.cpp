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
		case SKSE::MessagingInterface::kSaveGame:
			if (JunkIt::Settings::GetAutoSaveJunkListToFile()) {
				JunkIt::Settings::SaveJunkListToFile();
			}
			break;
	}
}

void RefreshDllSettings(StaticFunctionTag*) {
	SKSE::log::info(" ");
	SKSE::log::info("RefreshDllSettings called");
	JunkIt::Settings::Load();
}

ItemList* GetItemListMenu() {
	const auto UI = UI::GetSingleton();
	ItemList* itemListMenu = nullptr;
	if (UI && UI->IsMenuOpen("ContainerMenu")) {
		itemListMenu = UI->GetMenu<ContainerMenu>()->GetRuntimeData().itemList;
	} else if (UI && UI->IsMenuOpen("BarterMenu")) { 
		itemListMenu = UI->GetMenu<BarterMenu>()->GetRuntimeData().itemList;
	} else if (UI && UI->IsMenuOpen("InventoryMenu")) { 
		itemListMenu = UI->GetMenu<InventoryMenu>()->GetRuntimeData().itemList;
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

/**
 * @brief Toggles the selected item in the ItemListMenu as junk
 * 
 * @return std::int32_t 
 * 		0 = No Item Selected
 * 		1 = Successfully marked/unmarked item as junk
 * 		2 = Generalized Error
 * 		3 = Cannot Mark Quest item
 * 		4 = Equipped items are Protected
 * 		5 = Favorited item are Protected
 */
TESForm* ToggleSelectedAsJunk(StaticFunctionTag*) {
	ItemList* itemListMenu = GetItemListMenu();
	if (!itemListMenu) {
		SKSE::log::error("No ItemListMenu found");
		DebugNotification("JunkIt - No item selected!");
		return nullptr;
	}

	ItemList::Item* selectedItem = itemListMenu->GetSelectedItem();
	if(!selectedItem) {
		// Item list possibly wasn't updated yet after a freeze, run update and try again
		itemListMenu->Update();

		selectedItem = itemListMenu->GetSelectedItem();
		if (!selectedItem) {
			SKSE::log::error("No item selected in ItemListMenu");
			DebugNotification("JunkIt - No item selected!");
			return nullptr;
		}
	}

	InventoryEntryData* inventoryEntry = selectedItem->data.objDesc;
	TESForm* itemForm = inventoryEntry->GetObject()->As<TESForm>();
	if (!itemForm) {
		SKSE::log::error("Error getting item as form for {}", inventoryEntry->GetDisplayName());
		DebugNotification("JunkIt - Failed to mark item as junk!");
		return nullptr;
	}

	std::string itemName = itemForm->GetName();
	std::string hexFormId = fmt::format("0x{:X}~{}", itemForm->GetLocalFormID(), itemForm->GetFile(0)->GetFilename());
	BGSKeyword* isJunkKYWD = JunkIt::Settings::GetIsJunkKYWD();
	BGSKeywordForm* keywordForm = nullptr;

	// Ammo has to be treated differently as it does not inherit from BGSKeywordForm
	if (itemForm->GetFormType() == FormType::Ammo) {
		TESAmmo* ammo = itemForm->As<TESAmmo>();
		keywordForm = ammo->AsKeywordForm();
	} else {
		// Generalized handling for all other form types
		keywordForm = itemForm->As<BGSKeywordForm>();
	}
	
	if (!keywordForm) {
		SKSE::log::error("Error attempting to add IsJunk keyword to {} [{}]. Failed to typecast to BGSKeywordForm", itemName, hexFormId);
		DebugNotification("JunkIt - Failed to mark item as junk!");
		return nullptr;
	}

	// Quest Items cannot be marked as junk
	if (inventoryEntry->IsQuestObject()) {
		SKSE::log::info("Cannot mark quest item {} [{}] as junk", itemName, hexFormId);
		if (!keywordForm->HasKeyword(isJunkKYWD)) { // Allow removal of keyword if it has it already for backward compatibility
			DebugNotification("JunkIt - Quest Items cannot be marked as Junk");
			return nullptr;
		}
	}

	// Equipped Items cannot be marked as junk
	if (JunkIt::Settings::ProtectEquipped() && inventoryEntry->IsWorn()) {
		SKSE::log::info("Cannot mark equipped item {} [{}] as junk", itemName, hexFormId);
		if (!keywordForm->HasKeyword(isJunkKYWD)) { // Allow removal of keyword if it has it already for backward compatibility
			DebugNotification("JunkIt - Equipped Items are protected and cannot be marked as Junk");
			return nullptr;
		}
	}

	// Favorited Items cannot be marked as junk
	if (JunkIt::Settings::ProtectFavorites() && inventoryEntry->IsFavorited()) {
		SKSE::log::info("Cannot mark favorited item {} [{}] as junk", itemName, hexFormId);
		if (!keywordForm->HasKeyword(isJunkKYWD)) { // Allow removal of keyword if it has it already for backward compatibility
			DebugNotification("JunkIt - Favorited Items are protected and cannot be marked as Junk");
			return nullptr;
		}
	}

	bool isJunk = keywordForm->HasKeyword(isJunkKYWD);
	if (isJunk) {
		SKSE::log::info("Removing IsJunk keyword to {} [{}]", itemName, hexFormId);
		keywordForm->RemoveKeyword(isJunkKYWD);
	} else {
		SKSE::log::info("Adding IsJunk keyword to {} [{}]", itemName, hexFormId);
		keywordForm->AddKeyword(isJunkKYWD);
	}

	// Update ItemList UI
	itemListMenu->Update();
	return itemForm;
}

std::int32_t AddJunkKeyword(StaticFunctionTag*, TESForm* a_form) {
	if (!a_form) {
		SKSE::log::error("Error attempting to add IsJunk keyword to nullptr");
		return false;
	}

	SKSE::log::info("Adding IsJunk keyword to {}", a_form->GetName());
	BGSKeyword* isJunkKYWD = JunkIt::Settings::GetIsJunkKYWD();

	// Ammo has to be treated differently as it does not inherit from BGSKeywordForm
	if (a_form->GetFormType() == FormType::Ammo) {
		TESAmmo* ammo = a_form->As<TESAmmo>();
		ammo->AsKeywordForm()->AddKeyword(isJunkKYWD);
		return true;
	}

	// Generalized handling for all other form types
	BGSKeywordForm* keywordForm = a_form->As<BGSKeywordForm>();
	if (!keywordForm) {
		SKSE::log::error("Error attempting to add IsJunk keyword to {}. Failed to typecast to BGSKeywordForm", a_form->GetName());
		return false;
	}

	keywordForm->AddKeyword(isJunkKYWD);

	ItemList* itemListMenu = GetItemListMenu();
	if (itemListMenu) {
		itemListMenu->Update();
	}

	return true;
}

std::int32_t RemoveJunkKeyword(StaticFunctionTag*, TESForm* a_form) {
	if (!a_form) {
		SKSE::log::error("Error attempting to remove IsJunk keyword to nullptr");
		return false;
	}

	SKSE::log::info("Remove IsJunk keyword from {}", a_form->GetName());
	BGSKeyword* isJunkKYWD = JunkIt::Settings::GetIsJunkKYWD();

	// Ammo has to be treated differently as it does not inherit from BGSKeywordForm
	if (a_form->GetFormType() == FormType::Ammo) {
		TESAmmo* ammo = a_form->As<TESAmmo>();
		ammo->AsKeywordForm()->RemoveKeyword(isJunkKYWD);
		return true;
	}

	// Generalized handling for all other form types
	BGSKeywordForm* keywordForm = a_form->As<BGSKeywordForm>();
	if (!keywordForm) {
		SKSE::log::error("Error attempting to add IsJunk keyword to {}. Failed to typecast to BGSKeywordForm", a_form->GetName());
		return false;
	}

	keywordForm->RemoveKeyword(isJunkKYWD);
	
	ItemList* itemListMenu = GetItemListMenu();
	if (itemListMenu) {
		itemListMenu->Update();
	}
	
	return true;
}

void FreezeItemListUI(StaticFunctionTag*) {
	ItemList* itemListMenu = GetItemListMenu();
	if (itemListMenu) {
		itemListMenu->view->SetPause(true);
	}
}

void ThawItemListUI(StaticFunctionTag*) {
	ItemList* itemListMenu = GetItemListMenu();
	if (itemListMenu) {
		itemListMenu->view->SetPause(false);
	}
}

void RefreshItemListUI() {
	ItemList* itemListMenu = GetItemListMenu();
	if (itemListMenu) {
		itemListMenu->Update();
	} else {
		SKSE::log::error("No ItemListMenu found");
	}
}

void RefreshUIIcons(StaticFunctionTag*) {
	RefreshItemListUI();
}

TESObjectREFR* GetContainerMenuContainer(StaticFunctionTag*) {
	TESObjectREFR* container = nullptr;

	const auto UI = UI::GetSingleton();
	const auto menu = UI ? UI->GetMenu<ContainerMenu>() : nullptr;
	if (menu) {
		const auto           refHandle = menu->GetTargetRefHandle();
		TESObjectREFRPtr refr;
		LookupReferenceByHandle(refHandle, refr);

		container = refr.get();
	}

	return container;
}

TESObjectREFR* GetBarterMenuContainer(StaticFunctionTag*) {
	SKSE::log::info(" ");
	SKSE::log::info("---- Getting Vendor actor ----");
	TESObjectREFR* container = nullptr;
	
	const auto UI = UI::GetSingleton();
	const auto menu = UI ? UI->GetMenu<BarterMenu>() : nullptr;
	if (menu) {
		const auto           refHandle = menu->GetTargetRefHandle();
		TESObjectREFRPtr refr;
		LookupReferenceByHandle(refHandle, refr);
		container = refr.get();
	}

	if (!container) {
		SKSE::log::info("No merchant actor container found");
	}

	return container;
}

TESObjectREFR* GetBarterMenuMerchantContainer(StaticFunctionTag*) {
	SKSE::log::info(" ");
	SKSE::log::info("---- Getting Merchant Vendor Faction Container ----");
	TESObjectREFR* container = nullptr;
	
	const auto UI = UI::GetSingleton();
	const auto menu = UI ? UI->GetMenu<BarterMenu>() : nullptr;
	if (menu) {
		const auto           refHandle = menu->GetTargetRefHandle();
		TESObjectREFRPtr refr;
		LookupReferenceByHandle(refHandle, refr);

		TESObjectREFR* merchantRef = nullptr;
		merchantRef = refr.get();

		if (!merchantRef) {
			SKSE::log::error("No merchant actor found");
		}
		
		Actor* merchant = merchantRef->As<Actor>();
		TESFaction* merchantFaction = merchant->GetVendorFaction();

		if (!merchantFaction) {
			SKSE::log::error("No merchant faction found - using vendor actor as container");
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

ContainerMenu::ContainerMode GetContainerMode(StaticFunctionTag*) {
	const auto UI = UI::GetSingleton();
	const auto containerMenu = UI ? UI->GetMenu<ContainerMenu>() : nullptr;
	if (!containerMenu) {
		SKSE::log::info("No open menu found");
		return ContainerMenu::ContainerMode::kLoot;
	}

	ContainerMenu::ContainerMode mode = containerMenu->GetContainerMode();
	SKSE::log::info("Container Mode: {}", static_cast<std::uint32_t>(mode));
	return mode;
}

std::int32_t TransferJunkList(StaticFunctionTag*, BGSListForm* Items, TESObjectREFR* fromContainer, TESObjectREFR* toContainer, std::int32_t a_count = -1) {
	SKSE::log::info(" ");
	SKSE::log::info("---- Transferring Junk ----");

	std::int32_t transferCount = 0;

	ItemList* itemListMenu = GetItemListMenu();
	if (itemListMenu) {
		SKSE::log::info("Pausing ItemListMenu");
		itemListMenu->view->SetPause(true);
	}

	// For each form in the Items BGSListForm, get the count of items in the fromContainer and removeItem from fromContainer and addItem to toContainer
	Items->ForEachForm([&](TESForm& form) {
		TESBoundObject* item = form.As<TESBoundObject>();
		if (!item) {
			return BSContainer::ForEachResult::kContinue;
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

		return BSContainer::ForEachResult::kContinue;
	});

	if (itemListMenu) {
		SKSE::log::info("Resuming ItemListMenu and Updating");
		itemListMenu->view->SetPause(false);
		itemListMenu->Update();
	}
	
	return transferCount;
}

BGSListForm* GetTransferFormList(StaticFunctionTag*) {
	SKSE::log::info(" ");
	SKSE::log::info("---- Finding Transferrable Junk ----");

	BGSListForm* JunkList = JunkIt::Settings::GetJunkList();
	BGSListForm* transferList = JunkIt::Settings::GetTransferList();
	transferList->ClearData();

	// Get access to the UI Menu so we can limit our transfer based on the current view and Equip/Favorite state
	const auto UI = UI::GetSingleton();
	GPtr<ContainerMenu> containerMenu = UI ? UI->GetMenu<ContainerMenu>() : nullptr;
	ItemList* itemListMenu = containerMenu ? containerMenu->GetRuntimeData().itemList : nullptr;
	if (!itemListMenu) {
		SKSE::log::error("No ItemListMenu found");
		return transferList;
	}

	// Get entry list from the ItemListMenu
	BSTArray<ItemList::Item*> listItems = itemListMenu->items;
	std::vector<InventoryEntryData*> sortFormData;
	
	SKSE::log::info("Processing Entry List for transferable junk items");
	for (std::uint32_t i = 0, size = listItems.size(); i < size; i++) {
		ItemList::Item* entryItem = listItems[i];
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
		std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
			return a->GetWeight() > b->GetWeight();
		});
	}
	else if (JunkIt::Settings::GetTransferPriority() == JunkIt::Settings::SortPriority::kWeightLowHigh)
	{
		SKSE::log::info("Applying Weight [Low > High] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
			return a->GetWeight() < b->GetWeight();
		});
	}
	else if (JunkIt::Settings::GetTransferPriority() == JunkIt::Settings::SortPriority::kValueHighLow)
	{
		SKSE::log::info("Applying Value [High > Low] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
			return a->GetValue() > b->GetValue();
		});
	}
	else if (JunkIt::Settings::GetTransferPriority() == JunkIt::Settings::SortPriority::kValueLowHigh)
	{
		SKSE::log::info("Applying Value [Low > High] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
			return a->GetValue() < b->GetValue();
		});
	}
	else if (JunkIt::Settings::GetTransferPriority() == JunkIt::Settings::SortPriority::kValueWeightHighLow)
	{
		SKSE::log::info("Applying Value/Weight [High > Low] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
			return (a->GetValue() / a->GetWeight()) > (b->GetValue() / b->GetWeight());
		});
	}
	else if (JunkIt::Settings::GetTransferPriority() == JunkIt::Settings::SortPriority::kValueWeightLowHigh)
	{
		SKSE::log::info("Applying Value/Weight [Low > High] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
			return (a->GetValue() / a->GetWeight()) < (b->GetValue() / b->GetWeight());
		});
	}

	// Add the sorted list of items to the transfer list
	SKSE::log::info("Final TransferList:");
	for (const InventoryEntryData* entryData : sortFormData) {
		const TESBoundObject* entryObject = entryData->GetObject();
		if (!entryObject) {
			continue;
		}
		if (!transferList->HasForm(entryObject)) {
			SKSE::log::info("     {} - Val({}) Weight({})", entryObject->GetName(), entryData->GetValue(), entryData->GetWeight());
			transferList->AddForm(TESForm::LookupByID(entryObject->GetFormID()));
		}
	}

	SKSE::log::info("---- Generated Junk Transfer FormList ----");
	SKSE::log::info(" ");
	return transferList;
}

BGSListForm* GetSellFormList(StaticFunctionTag*) {
	SKSE::log::info(" ");
	SKSE::log::info("---- Finding Sellable Junk ----");

	BGSListForm* JunkList = JunkIt::Settings::GetJunkList();
	BGSListForm* sellList = JunkIt::Settings::GetSellList();
	sellList->ClearData();

	// Get access to the UI Menu so we can limit our transfer based on the current view and Equip/Favorite state
	const auto UI = UI::GetSingleton();
	GPtr<BarterMenu> barterMenu = UI ? UI->GetMenu<BarterMenu>() : nullptr;
	ItemList* itemListMenu = barterMenu ? barterMenu->GetRuntimeData().itemList : nullptr;
	if (!itemListMenu) {
		SKSE::log::error("No ItemListMenu found");
		return sellList;
	}

	// Get entry list from the ItemListMenu
	BSTArray<ItemList::Item*> listItems = itemListMenu->items;
	std::vector<InventoryEntryData*> sortFormData;
	
	SKSE::log::info("Processing Entry List for sellable junk items");
	for (std::uint32_t i = 0, size = listItems.size(); i < size; i++) {
		ItemList::Item* entryItem = listItems[i];
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
		std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
			return a->GetWeight() > b->GetWeight();
		});
	}
	else if (JunkIt::Settings::GetSellPriority() == JunkIt::Settings::SortPriority::kWeightLowHigh)
	{
		SKSE::log::info("Applying Weight [Low > High] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
			return a->GetWeight() < b->GetWeight();
		});
	}
	else if (JunkIt::Settings::GetSellPriority() == JunkIt::Settings::SortPriority::kValueHighLow)
	{
		SKSE::log::info("Applying Value [High > Low] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
			return a->GetValue() > b->GetValue();
		});
	}
	else if (JunkIt::Settings::GetSellPriority() == JunkIt::Settings::SortPriority::kValueLowHigh)
	{
		SKSE::log::info("Applying Value [Low > High] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
			return a->GetValue() < b->GetValue();
		});
	}
	else if (JunkIt::Settings::GetSellPriority() == JunkIt::Settings::SortPriority::kValueWeightHighLow)
	{
		SKSE::log::info("Applying Value/Weight [High > Low] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
			return (a->GetValue() / a->GetWeight()) > (b->GetValue() / b->GetWeight());
		});
	}
	else if (JunkIt::Settings::GetSellPriority() == JunkIt::Settings::SortPriority::kValueWeightLowHigh)
	{
		SKSE::log::info("Applying Value/Weight [Low > High] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
			return (a->GetValue() / a->GetWeight()) < (b->GetValue() / b->GetWeight());
		});
	}

	// Add the sorted list of items to the transfer list
	SKSE::log::info("Final SellList:");
	for (const InventoryEntryData* entryData : sortFormData) {
		const TESBoundObject* entryObject = entryData->GetObject();
		if (!entryObject) {
			continue;
		}
		if (!sellList->HasForm(entryObject)) {
			SKSE::log::info("     {} - Val({}) Weight({})", entryObject->GetName(), entryData->GetValue(), entryData->GetWeight());
			sellList->AddForm(TESForm::LookupByID(entryObject->GetFormID()));
		}
	}

	SKSE::log::info("---- Generated Junk Sell FormList ----");
	SKSE::log::info(" ");
	return sellList;
}

std::int32_t GetMenuItemValue(StaticFunctionTag*, TESForm* a_form) {
	SKSE::log::info(" ");
	SKSE::log::info("---- Getting Item Value as it is listed in the open menu for {} [{}] ----", a_form->GetName(), a_form->GetFormID());
	std::int32_t goldValue = -1;

	// Get access to the UI Menu so we can limit our transfer based on the current view and Equip/Favorite state
	const auto UI = UI::GetSingleton();

	ItemList* itemListMenu = nullptr;
	if (UI && UI->IsMenuOpen("InventoryMenu")) {
		itemListMenu = UI->GetMenu<InventoryMenu>()->GetRuntimeData().itemList;
	} else if (UI && UI->IsMenuOpen("ContainerMenu")) {
		itemListMenu = UI->GetMenu<ContainerMenu>()->GetRuntimeData().itemList;
	} else if (UI && UI->IsMenuOpen("BarterMenu")) {
		itemListMenu = UI->GetMenu<BarterMenu>()->GetRuntimeData().itemList;
	} else	{
		SKSE::log::info("No open menu found");
		return goldValue;
	}

	if (!itemListMenu) {
		SKSE::log::error("No ItemListMenu found");
		return goldValue;
	}

	// Get item list from the ItemListMenu
	BSTArray<ItemList::Item*> listItems = itemListMenu->items;
	SKSE::log::info("Parsing ItemList[{}] for the matching enchanted item", listItems.size());

	// Loop through the entry list to find the selected index for the formId
	for (std::uint32_t i = 0, size = listItems.size(); i < size; i++) {
		ItemList::Item* entryItem = listItems[i];
		if (!entryItem) {
			continue;
		}

		InventoryEntryData* entryData = entryItem->data.objDesc;
		if (!entryData) {
			continue;
		}

		TESBoundObject* entryObject = entryData->GetObject();
		if (!entryObject) {
			continue;
		}

		FormID formId = entryObject->GetFormID();
		if (formId == a_form->GetFormID()) {
			goldValue = entryData->GetValue();
			SKSE::log::info("{} - EntryData->GetValue() {}", entryObject->GetName(), goldValue);
			break;
		}
	}

	return goldValue;
}

void SaveJunkListToFile(StaticFunctionTag*) {
	JunkIt::Settings::SaveJunkListToFile();
}

BGSListForm* LoadJunkListFromFile(StaticFunctionTag*) {
	return JunkIt::Settings::LoadJunkListFromFile();
}

void UpdateItemKeywords(StaticFunctionTag*) {
	JunkIt::JunkHandler::UpdateItemKeywords();
}

/** EXPERIMENTAL */

std::int32_t ProcessItemListTransfer(StaticFunctionTag*, BGSListForm* a_itemList, TESObjectREFR* a_fromContainer, TESObjectREFR* a_toContainer, std::int32_t a_isBarter = 0) {
	using Count = std::int32_t;
	using InventoryItemMap = std::map<TESBoundObject*, std::pair<Count, std::unique_ptr<InventoryEntryData>>>;
	
	SKSE::log::info(" ");
	SKSE::log::info("---- Initiating Item Transfer using FormList ----");
	
	if (a_itemList->scriptAddedFormCount == 0) {
		SKSE::log::info("Provided FormList is empty");
	}

	ITEM_REMOVE_REASON reason = ITEM_REMOVE_REASON::kStoreInContainer;
	if (a_isBarter == 1) {
		reason = ITEM_REMOVE_REASON::kSelling;
		SKSE::log::info("Item Removal Reason set to ITEM_REMOVE_REASON::kSelling");
	} else if (a_toContainer->GetFormType() == FormType::ActorCharacter) {
		reason = ITEM_REMOVE_REASON::kStoreInTeammate;
		SKSE::log::info("Item Removal Reason set to ITEM_REMOVE_REASON::kStoreInTeammate");
	} else {
		SKSE::log::info("Item Removal Reason set to ITEM_REMOVE_REASON::kStoreInContainer");
	}

	// Get InventoryMap filtered by the item list 
	InventoryItemMap filteredInventoryMap = a_fromContainer->GetInventory([&](TESBoundObject& obj) {
		return a_itemList->HasForm(obj.GetFormID());
	});

	Count totalTransferred = 0;

	// Traverse the filtered InventoryItemMap and transfer each item
	for(auto const& [item, inventoryData] : filteredInventoryMap) {
		// SKSE::log::info(" ");
        // SKSE::log::info("==================== Item Transfer ====================");
        // SKSE::log::info(" ");

		Count itemCount = inventoryData.first;
		InventoryEntryData* invData = inventoryData.second.get();
		JunkIt::JunkHandler::TransferItem(item, a_fromContainer, a_toContainer, reason, itemCount, invData);
		totalTransferred += itemCount;

		// SKSE::log::info(" ");
        // SKSE::log::info("=======================================================");
        // SKSE::log::info(" ");
	}

	RefreshItemListUI();

	SKSE::log::info("---- ItemList Transfer Completed ----");
	SKSE::log::info(" ");

	return totalTransferred;
}

bool BindPapyrusFunctions(BSScript::IVirtualMachine* vm) {
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
	vm->RegisterFunction("FreezeItemListUI", "JunkIt_MCM", FreezeItemListUI);
	vm->RegisterFunction("ThawItemListUI", "JunkIt_MCM", ThawItemListUI);
	vm->RegisterFunction("TransferJunkList", "JunkIt_MCM", TransferJunkList);
	
	vm->RegisterFunction("RefreshDllSettings", "JunkIt_MCM", RefreshDllSettings);

	vm->RegisterFunction("AddJunkKeyword", "JunkIt_MCM", AddJunkKeyword);
	vm->RegisterFunction("RemoveJunkKeyword", "JunkIt_MCM", RemoveJunkKeyword);

	vm->RegisterFunction("SaveJunkListToFile", "JunkIt_MCM", SaveJunkListToFile);
	vm->RegisterFunction("LoadJunkListFromFile", "JunkIt_MCM", LoadJunkListFromFile);

	vm->RegisterFunction("ProcessItemListTransfer", "JunkIt_MCM", ProcessItemListTransfer);
	
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