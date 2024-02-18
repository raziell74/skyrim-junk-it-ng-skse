#include "log.h"
#include "settings.h"

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
	SKSE::log::info("---- Transfer Initiated ----");

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
		if (equipped) {
			SKSE::log::info("Junk Item Equipped - Skipping {}", entryItem->data.objDesc->GetObject()->GetName());
			continue;
		}
		std::uint32_t favorite = entryItem->data.objDesc->IsFavorited();
		if (favorite) {
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

	SKSE::log::info("---- Transfer Complete ----");
	SKSE::log::info(" ");
	return transferList;
}

void TransferJunk(RE::StaticFunctionTag*) {
	// Coming Soon
}

void RetrieveJunk(RE::StaticFunctionTag*) {
	// Coming Soon
}

void SellJunk(RE::StaticFunctionTag*) {
	// Coming Soon
}

bool BindPapyrusFunctions(RE::BSScript::IVirtualMachine* vm) {
	vm->RegisterFunction("RefreshUIIcons", "JunkIt_MCM", RefreshUIIcons);
	vm->RegisterFunction("TransferJunk", "JunkIt_MCM", TransferJunk);
	vm->RegisterFunction("RetrieveJunk", "JunkIt_MCM", RetrieveJunk);
	vm->RegisterFunction("SellJunk", "JunkIt_MCM", SellJunk);

	vm->RegisterFunction("GetContainerMode", "JunkIt_MCM", GetContainerMode);
	vm->RegisterFunction("GetTransferFormList", "JunkIt_MCM", GetTransferFormList);
	vm->RegisterFunction("GetRetrievalFormList", "JunkIt_MCM", GetTransferFormList);
	vm->RegisterFunction("GetSellFormList", "JunkIt_MCM", GetTransferFormList);

	vm->RegisterFunction("RefreshDllSettings", "JunkIt_MCM", RefreshDllSettings);
	
	SKSE::log::info("Registered Native Papyrus Functions");
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