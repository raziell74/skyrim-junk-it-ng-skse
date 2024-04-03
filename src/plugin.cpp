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
		SKSE::log::info("No item selected in ItemListMenu. Updating UI and trying again");

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
	if (!inventoryEntry) {
		SKSE::log::error("Error getting InventoryEntryData for {}", selectedItem->data.objDesc->GetDisplayName());
		DebugNotification("JunkIt - Failed to mark item as junk!");
		return nullptr;
	}
	
	TESBoundObject* itemObject = inventoryEntry->GetObject();
	if (!itemObject) {
		SKSE::log::error("Error getting item as object for {}", inventoryEntry->GetDisplayName());
		DebugNotification("JunkIt - Failed to mark item as junk!");
		return nullptr;
	}

	TESForm* itemForm = itemObject->As<TESForm>();
	if (!itemForm) {
		SKSE::log::error("Error getting item as form for {}", inventoryEntry->GetDisplayName());
		DebugNotification("JunkIt - Failed to mark item as junk!");
		return nullptr;
	}

	std::string itemName = itemForm->GetName();
	std::string hexFormId = FormUtil::Form::GetFormConfigString(itemForm);
	BGSKeyword* isJunkKYWD = JunkIt::Settings::GetIsJunkKYWD();
	BGSKeywordForm* keywordForm = nullptr;

	// Send notification if the item is FromType light since lights (like torches) cannot get keywords
	if (itemForm->GetFormType() == FormType::Light) {
		DebugNotification("JunkIt - Lights cannot be marked as Junk");
		return nullptr;
	}
	
	// Ammo forms do not inherit from BGSKeywordForm, but have their own keyword management structure
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
	SKSE::log::info(" ");
	SKSE::log::info("Getting Container target data ----");
	TESObjectREFR* container = nullptr;

	const auto UI = UI::GetSingleton();
	const auto menu = UI ? UI->GetMenu<ContainerMenu>() : nullptr;
	if (menu) {
		const auto           refHandle = menu->GetTargetRefHandle();
		TESObjectREFRPtr refr;
		LookupReferenceByHandle(refHandle, refr);

		container = refr.get();
	}

	if (!container) {
		SKSE::log::info("     No container target found");
		return nullptr;
	}

	SKSE::log::info("     Container target {} [{}]", container->GetName(), FormUtil::Form::GetFormConfigString(container));
	return container;
}

TESObjectREFR* GetBarterMenuContainer(StaticFunctionTag*) {
	SKSE::log::info(" ");
	SKSE::log::info("Getting Vendor data ----");
	TESObjectREFR* container = UIUtil::Menu::GetBarterMenuTargetRef();
	
	if (!container) {
		SKSE::log::info("     No merchant actor container found");
		return nullptr;
	}

	SKSE::log::info("     Vendor actor {} [{}]", container->GetName(), FormUtil::Form::GetFormConfigString(container));
	return container;
}

TESObjectREFR* GetBarterMenuMerchantContainer(StaticFunctionTag*) {
	TESObjectREFR* merchantRef = UIUtil::Menu::GetBarterMenuTargetRef();
	if (!merchantRef) {
		SKSE::log::error("     Vendor Ref is required to get the merchant container. Exiting with error.");
		return nullptr;
	}

	TESFaction* merchantFaction = merchantRef->As<Actor>()->GetVendorFaction();
	if (!merchantFaction) {
		SKSE::log::error("     No merchant faction found - using vendor actor as container");
		return merchantRef;
	}
	
	SKSE::log::info("     Merchant faction found with id {} - looking up faction->merchantContainer", FormUtil::Form::GetFormConfigString(merchantFaction));
	TESObjectREFR* container = merchantFaction->vendorData.merchantContainer;
	if (!container) {
		SKSE::log::info("     Merchant container not found for faction - using vendor actor as merchantContainer");
		return merchantRef;
	}

	SKSE::log::info("     Merchant Container identified with Reference FormID {}", FormUtil::Form::GetFormConfigString(container));
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
			std::float_t aValWeight = a->GetValue() / a->GetWeight();
			std::float_t bValWeight = b->GetValue() / b->GetWeight();
			if (aValWeight == INFINITY) aValWeight = 0;
			if (bValWeight == INFINITY) bValWeight = 0;
			return aValWeight > bValWeight;
		});
	}
	else if (JunkIt::Settings::GetTransferPriority() == JunkIt::Settings::SortPriority::kValueWeightLowHigh)
	{
		SKSE::log::info("Applying Value/Weight [Low > High] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
			std::float_t aValWeight = a->GetValue() / a->GetWeight();
			std::float_t bValWeight = b->GetValue() / b->GetWeight();
			if (aValWeight == INFINITY) aValWeight = 0;
			if (bValWeight == INFINITY) bValWeight = 0;
			return aValWeight < bValWeight;
		});
	}

	// Add the sorted list of items to the transfer list
	SKSE::log::info("Finalized TransferList:");
	for (const InventoryEntryData* entryData : sortFormData) {
		const TESBoundObject* entryObject = entryData->GetObject();
		if (!entryObject) {
			continue;
		}
		if (!transferList->HasForm(entryObject->GetFormID())) {
			TESForm* itemForm = entryData->object->As<TESForm>();
			std::string sortString = "";
			std::float_t valWeight = entryData->GetValue() / entryData->GetWeight();
			if (valWeight == INFINITY) valWeight = 0;
			switch (JunkIt::Settings::GetTransferPriority()) {
				case JunkIt::Settings::SortPriority::kWeightHighLow:
				case JunkIt::Settings::SortPriority::kWeightLowHigh:
					sortString = fmt::format("Weight({})", entryData->GetWeight());
					break;
				case JunkIt::Settings::SortPriority::kValueHighLow:
				case JunkIt::Settings::SortPriority::kValueLowHigh:
					sortString = fmt::format("Val({})", entryData->GetValue());
					break;
				case JunkIt::Settings::SortPriority::kValueWeightHighLow:
				case JunkIt::Settings::SortPriority::kValueWeightLowHigh:
					sortString = fmt::format("Val/Weight({:.2f})", valWeight);
					break;
				default:
					sortString = fmt::format("Val({}) Weight({}) Val/Weight({:.2f})", 
						entryData->GetValue(), 
						entryData->GetWeight(), 
						valWeight
					);
					break;
			}
			SKSE::log::info("     {} - {} [{}]", 
				sortString,
				entryObject->GetName(),
				FormUtil::Form::GetFormConfigString(itemForm)
			);
			transferList->AddForm(itemForm);
		}
	}

	SKSE::log::info("---- Completed Junk Transfer List Generation ----");
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
			std::float_t aValWeight = a->GetValue() / a->GetWeight();
			std::float_t bValWeight = b->GetValue() / b->GetWeight();
			if (aValWeight == INFINITY) aValWeight = 0;
			if (bValWeight == INFINITY) bValWeight = 0;
			return aValWeight > bValWeight;
		});
	}
	else if (JunkIt::Settings::GetSellPriority() == JunkIt::Settings::SortPriority::kValueWeightLowHigh)
	{
		SKSE::log::info("Applying Value/Weight [Low > High] Sort Priority");
		std::sort(sortFormData.begin(), sortFormData.end(), [](const InventoryEntryData* a, const InventoryEntryData* b) {
			std::float_t aValWeight = a->GetValue() / a->GetWeight();
			std::float_t bValWeight = b->GetValue() / b->GetWeight();
			if (aValWeight == INFINITY) aValWeight = 0;
			if (bValWeight == INFINITY) bValWeight = 0;
			return aValWeight < bValWeight;
		});
	}

	// Add the sorted list of items to the transfer list
	SKSE::log::info("Finalized SellList:");
	for (const InventoryEntryData* entryData : sortFormData) {
		const TESBoundObject* entryObject = entryData->GetObject();
		if (!entryObject) {
			continue;
		}
		if (!sellList->HasForm(entryObject->GetFormID())) {
			TESForm* itemForm = entryData->object->As<TESForm>();
			std::string sortString = "";
			std::float_t valWeight = entryData->GetValue() / entryData->GetWeight();
			if (valWeight == INFINITY) valWeight = 0;
			switch (JunkIt::Settings::GetSellPriority()) {
				case JunkIt::Settings::SortPriority::kWeightHighLow:
				case JunkIt::Settings::SortPriority::kWeightLowHigh:
					sortString = fmt::format("Weight({})", entryData->GetWeight());
					break;
				case JunkIt::Settings::SortPriority::kValueHighLow:
				case JunkIt::Settings::SortPriority::kValueLowHigh:
					sortString = fmt::format("Val({})", entryData->GetValue());
					break;
				case JunkIt::Settings::SortPriority::kValueWeightHighLow:
				case JunkIt::Settings::SortPriority::kValueWeightLowHigh:
					sortString = fmt::format("Val/Weight({:.2f})", valWeight);
					break;
				default:
					sortString = fmt::format("Val({}) Weight({}) Val/Weight({:.2f})", 
						entryData->GetValue(), 
						entryData->GetWeight(), 
						valWeight
					);
					break;
			}
			SKSE::log::info("     {} - {} [{}]", 
				sortString,
				entryObject->GetName(),
				FormUtil::Form::GetFormConfigString(itemForm)
			);
			sellList->AddForm(itemForm);
		}
	}

	SKSE::log::info("---- Generated Junk Sell FormList ----");
	SKSE::log::info(" ");
	return sellList;
}

std::int32_t GetMenuItemValue(StaticFunctionTag*, TESForm* a_form) {
	// SKSE::log::info("---- Getting Item Value as it is listed in the open menu for {} [0x{:X}] ----", a_form->GetName(), a_form->GetFormID());
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
	// SKSE::log::info("Parsing ItemList[{}] for the matching enchanted item", listItems.size());

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
			SKSE::log::info("          Value Per Item = {} gold", goldValue);
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

std::int32_t ProcessItemListTransfer(StaticFunctionTag*, BGSListForm* a_itemList, TESObjectREFR* a_fromContainer, TESObjectREFR* a_toContainer, std::int32_t a_isBarter = 0) {
	using Count = std::int32_t;
	using InventoryItemMap = std::map<TESBoundObject*, std::pair<Count, std::unique_ptr<InventoryEntryData>>>;
	
	SKSE::log::info(" ");
	SKSE::log::info("---- Initiating Item Transfer using FormList ----");

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
		Count itemCount = inventoryData.first;
		if (itemCount == 0) {
			continue;
		}

		InventoryEntryData* invData = inventoryData.second.get();
		JunkIt::JunkHandler::TransferItem(item, a_fromContainer, a_toContainer, reason, itemCount, invData);
		totalTransferred += itemCount;
	}

	RefreshItemListUI();

	SKSE::log::info("---- ItemList Transfer Completed ----");
	SKSE::log::info(" ");

	return totalTransferred;
}

std::int32_t GetContainerItemListCount(StaticFunctionTag*, TESObjectREFR* a_container, BGSListForm* a_itemList) {
	using Count = std::int32_t;
	using InventoryCountMap = std::map<TESBoundObject*, Count>;

	Count totalCount = 0;

	std::string containerName = a_container->GetName();
	if (containerName.empty()) {
		containerName = FormUtil::Form::GetFormConfigString(a_container);
	}

	// Get InventoryMap filtered by the item list 
	InventoryCountMap filteredInventoryMap = a_container->GetInventoryCounts([&](TESBoundObject& obj) {
		return a_itemList->HasForm(obj.GetFormID());
	});

	// Save this InventoryCountMap to a class variable for future use
	JunkIt::JunkHandler::SetContainerInventoryCountMap(filteredInventoryMap, a_container);

	// Traverse the filtered InventoryCountMap and transfer each item
	for(auto const& [item, count] : filteredInventoryMap) {
		totalCount += count;
	}

	SKSE::log::info("     {} Inventory FormList Count {}", 
		containerName, 
		totalCount
	);
	return totalCount;
}

std::int32_t GetContainerSingleItemCount(StaticFunctionTag*, TESObjectREFR* a_container, TESForm* a_item) {
	using Count = std::int32_t;
	using InventoryCountMap = std::map<TESBoundObject*, Count>;
	
	Count totalCount = 0;
	
	if (!a_item) {
		SKSE::log::error("     Item form is not valid. Item Count {}", totalCount);
		return totalCount;
	}

	// Get the previous List Filtered InventoryCountMap or Full InventoryCountMap if no filtered one for this container is found
	InventoryCountMap* invMap = JunkIt::JunkHandler::GetContainerInventoryCountMap(a_container);

	// Look up item from previous InventoryCountMap
	auto itemInvData = invMap->find(a_item->As<TESBoundObject>());
	if (itemInvData != invMap->end()) {
		totalCount = itemInvData->second;
	}

	if (totalCount) {
		SKSE::log::info("     {} {} [{}]", 
			totalCount, 
			a_item->GetName(), 
			FormUtil::Form::GetFormConfigString(a_item)
		);
	}
	return totalCount;
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