#include "AutoJunk.h"

#include "I4Integration.h"
#include "JunkData.h"
#include "SkyPromptIntegration.h"
#include "junk.h"
#include "settings.h"
#include "util.h"

#include "RE/E/ExtraUniqueID.h"

#include <cctype>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace JunkIt {
    namespace {
        constexpr const char* const kKnownItemTypes[] = {
            "Melee",
            "Sword",
            "Dagger",
            "War Axe",
            "Mace",
            "Greatsword",
            "Battleaxe",
            "Warhammer",
            "Bow",
            "Staff",
            "Crossbow",
            "Pickaxe",
            "Wood Axe",
            "Head",
            "Body",
            "Hands",
            "Forearms",
            "Amulet",
            "Ring",
            "Feet",
            "Calves",
            "Shield",
            "Circlet",
            "Ears",
            "Tail",
            "Arrow",
            "Bolt",
            "Spear",
            "Potion",
            "Food",
            "Drink",
            "Poison",
            "Health",
            "Magicka",
            "Stamina",
            "Book",
            "Note",
            "Recipe",
            "Spell Tome",
            "Scroll",
            "Ingredient",
            "Torch",
            "Key",
            "Soul Gem",
            "Misc",
            "Clothing",
            "Toy",
            "House Part",
            "Artifact",
            "Gem",
            "Hide",
            "Tool",
            "Remains",
            "Ingot",
            "Clutter",
            "Firewood",
            "Claw",
            "Lockpick",
            "Gold",
            "Leather",
            "Strips"
        };

        constexpr RE::FormID kLocalIDMask = 0x00FFFFFF;
        constexpr RE::FormID kWeapPickaxe = 0x0E3C16;
        constexpr RE::FormID kSSDRocksplinterPickaxe = 0x06A707;
        constexpr RE::FormID kDunVolunruudPickaxe = 0x1019D4;
        constexpr RE::FormID kAxe01 = 0x02F2F4;
        constexpr RE::FormID kDunHaltedStreamPoachersAxe = 0x0AE086;
        constexpr RE::FormID kDLC1ClothesVampireLordArmor = 0x011A84;
        constexpr RE::FormID kDLC2RieklingSpearThrown = 0x017720;
        constexpr RE::FormID kLockpick = 0x00000A;
        constexpr RE::FormID kGold001 = 0x00000F;
        constexpr RE::FormID kLeather01 = 0x0DB5D2;
        constexpr RE::FormID kLeatherStrips = 0x0800E4;
        constexpr RE::FormID kGemAmethystFlawless = 0x06851E;
        constexpr RE::FormID kRubyDragonClaw = 0x04B56C;
        constexpr RE::FormID kIvoryDragonClaw = 0x0AB7BB;
        constexpr RE::FormID kGlassClaw = 0x07C260;
        constexpr RE::FormID kEbonyClaw = 0x05AF48;
        constexpr RE::FormID kEmeraldDragonClaw = 0x0ED417;
        constexpr RE::FormID kDiamondClaw = 0x0AB375;
        constexpr RE::FormID kIronClaw = 0x08CDFA;
        constexpr RE::FormID kCoralDragonClaw = 0x0B634C;
        constexpr RE::FormID kE3GoldenClaw = 0x0999E7;
        constexpr RE::FormID kSapphireDragonClaw = 0x0663D7;
        constexpr RE::FormID kMS13GoldenClaw = 0x039647;
        constexpr RE::FormID kITMPotionUse = 0x000B6435;

        using BipedSlot = RE::BGSBipedObjectForm::BipedObjectSlot;
        constexpr BipedSlot kArmorSlotPrecedence[] = {
            BipedSlot::kBody,
            BipedSlot::kHair,
            BipedSlot::kHands,
            BipedSlot::kForearms,
            BipedSlot::kFeet,
            BipedSlot::kCalves,
            BipedSlot::kShield,
            BipedSlot::kAmulet,
            BipedSlot::kRing,
            BipedSlot::kLongHair,
            BipedSlot::kEars,
            BipedSlot::kHead,
            BipedSlot::kCirclet,
            BipedSlot::kTail
        };

        std::string NormalizeType(std::string_view text) {
            std::string normalized;
            normalized.reserve(text.size());
            for (unsigned char ch : text) {
                if (ch == ' ' || ch == '_' || ch == '-') {
                    continue;
                }
                normalized.push_back(static_cast<char>(std::tolower(ch)));
            }
            return normalized;
        }

        RE::FormID LocalFormID(const RE::TESForm* form) {
            return form ? (form->GetFormID() & kLocalIDMask) : 0;
        }

        bool LocalFormIDIs(const RE::TESForm* form, RE::FormID localID) {
            return LocalFormID(form) == localID;
        }

        bool HasEditorKeyword(RE::BGSKeywordForm* keywordForm, std::string_view editorId) {
            return keywordForm && keywordForm->HasKeywordString(editorId);
        }

        std::string_view WeaponTypeLabel(RE::TESObjectWEAP* weap) {
            if (!weap) {
                return "Weapon";
            }
            if (weap->IsHandToHandMelee()) {
                return "Melee";
            }
            if (weap->IsOneHandedSword()) {
                return "Sword";
            }
            if (weap->IsOneHandedDagger()) {
                return "Dagger";
            }
            if (weap->IsOneHandedAxe()) {
                return "War Axe";
            }
            if (weap->IsOneHandedMace()) {
                return "Mace";
            }
            if (weap->IsTwoHandedSword()) {
                return "Greatsword";
            }
            if (weap->IsTwoHandedAxe()) {
                if (weap->HasKeywordString("WeapTypeWarhammer")) {
                    return "Warhammer";
                }
                return "Battleaxe";
            }
            if (weap->IsBow()) {
                return "Bow";
            }
            if (weap->IsStaff()) {
                return "Staff";
            }
            if (weap->IsCrossbow()) {
                return "Crossbow";
            }
            return "Weapon";
        }

        std::string_view ArmorTypeLabel(RE::TESObjectARMO* armo) {
            if (!armo) {
                return {};
            }
            if (LocalFormIDIs(armo, kDLC1ClothesVampireLordArmor)) {
                return "Body";
            }
            for (const auto slot : kArmorSlotPrecedence) {
                if (!armo->HasPartOf(slot)) {
                    continue;
                }
                switch (slot) {
                    case BipedSlot::kHead:
                    case BipedSlot::kHair:
                    case BipedSlot::kLongHair:
                        return "Head";
                    case BipedSlot::kBody:
                        return "Body";
                    case BipedSlot::kHands:
                        return "Hands";
                    case BipedSlot::kForearms:
                        return "Forearms";
                    case BipedSlot::kAmulet:
                        return "Amulet";
                    case BipedSlot::kRing:
                        return "Ring";
                    case BipedSlot::kFeet:
                        return "Feet";
                    case BipedSlot::kCalves:
                        return "Calves";
                    case BipedSlot::kShield:
                        return "Shield";
                    case BipedSlot::kCirclet:
                        return "Circlet";
                    case BipedSlot::kEars:
                        return "Ears";
                    case BipedSlot::kTail:
                        return "Tail";
                    default:
                        break;
                }
            }
            return {};
        }

        std::string_view PotionTypeLabel(RE::AlchemyItem* alch) {
            if (!alch) {
                return "Potion";
            }
            if (alch->IsFood()) {
                if (alch->data.consumptionSound && alch->data.consumptionSound->GetFormID() == kITMPotionUse) {
                    return "Drink";
                }
                return "Food";
            }
            if (alch->IsPoison()) {
                return "Poison";
            }
            if (auto* effect = alch->GetCostliestEffectItem()) {
                if (effect->baseEffect) {
                    switch (effect->baseEffect->data.primaryAV) {
                        case RE::ActorValue::kHealth:
                        case RE::ActorValue::kHealRate:
                        case RE::ActorValue::kHealRateMult:
                            return "Health";
                        case RE::ActorValue::kMagicka:
                        case RE::ActorValue::kMagickaRate:
                        case RE::ActorValue::kMagickaRateMult:
                            return "Magicka";
                        case RE::ActorValue::kStamina:
                        case RE::ActorValue::kStaminaRate:
                        case RE::ActorValue::kStaminaRateMult:
                            return "Stamina";
                        default:
                            break;
                    }
                }
            }
            return "Potion";
        }

        std::string_view BookTypeLabel(RE::TESObjectBOOK* book) {
            if (!book) {
                return "Book";
            }
            std::string_view label = "Book";
            if (book->IsNoteScroll()) {
                label = "Note";
            }
            if (HasEditorKeyword(book, "VendorItemRecipe")) {
                return "Recipe";
            }
            if (HasEditorKeyword(book, "VendorItemSpellTome")) {
                return "Spell Tome";
            }
            return label;
        }

        std::string_view MiscTypeLabel(RE::TESObjectMISC* misc) {
            if (!misc) {
                return "Misc";
            }
            const auto localID = LocalFormID(misc);
            switch (localID) {
                case kGemAmethystFlawless:
                    return "Gem";
                case kRubyDragonClaw:
                case kIvoryDragonClaw:
                case kGlassClaw:
                case kEbonyClaw:
                case kEmeraldDragonClaw:
                case kDiamondClaw:
                case kIronClaw:
                case kCoralDragonClaw:
                case kE3GoldenClaw:
                case kSapphireDragonClaw:
                case kMS13GoldenClaw:
                    return "Claw";
                case kLockpick:
                    return "Lockpick";
                case kGold001:
                    return "Gold";
                case kLeather01:
                    return "Leather";
                case kLeatherStrips:
                    return "Strips";
                default:
                    break;
            }
            if (HasEditorKeyword(misc, "BYOHAdoptionClothesKeyword")) {
                return "Clothing";
            }
            if (HasEditorKeyword(misc, "BYOHAdoptionToyKeyword")) {
                return "Toy";
            }
            if (HasEditorKeyword(misc, "BYOHHouseCraftingCategoryWeaponRacks") ||
                HasEditorKeyword(misc, "BYOHHouseCraftingCategoryShelf") ||
                HasEditorKeyword(misc, "BYOHHouseCraftingCategoryFurniture") ||
                HasEditorKeyword(misc, "BYOHHouseCraftingCategoryExterior") ||
                HasEditorKeyword(misc, "BYOHHouseCraftingCategoryContainers") ||
                HasEditorKeyword(misc, "BYOHHouseCraftingCategoryBuilding") ||
                HasEditorKeyword(misc, "BYOHHouseCraftingCategorySmithing")) {
                return "House Part";
            }
            if (HasEditorKeyword(misc, "VendorItemDaedricArtifact")) {
                return "Artifact";
            }
            if (HasEditorKeyword(misc, "VendorItemGem")) {
                return "Gem";
            }
            if (HasEditorKeyword(misc, "VendorItemAnimalHide")) {
                return "Hide";
            }
            if (HasEditorKeyword(misc, "VendorItemTool")) {
                return "Tool";
            }
            if (HasEditorKeyword(misc, "VendorItemAnimalPart")) {
                return "Remains";
            }
            if (HasEditorKeyword(misc, "VendorItemOreIngot")) {
                return "Ingot";
            }
            if (HasEditorKeyword(misc, "VendorItemClutter")) {
                return "Clutter";
            }
            if (HasEditorKeyword(misc, "VendorItemFirewood")) {
                return "Firewood";
            }
            return "Misc";
        }

        std::string GetSkyUITypeLabel(RE::TESBoundObject* object) {
            if (!object) {
                return {};
            }
            switch (object->GetFormType()) {
                case RE::FormType::Weapon:
                    if (auto* weap = object->As<RE::TESObjectWEAP>()) {
                        const auto localID = LocalFormID(weap);
                        if (localID == kWeapPickaxe || localID == kSSDRocksplinterPickaxe || localID == kDunVolunruudPickaxe) {
                            return "Pickaxe";
                        }
                        if (localID == kAxe01 || localID == kDunHaltedStreamPoachersAxe) {
                            return "Wood Axe";
                        }
                        return std::string(WeaponTypeLabel(weap));
                    }
                    return "Weapon";
                case RE::FormType::Armor:
                    return std::string(ArmorTypeLabel(object->As<RE::TESObjectARMO>()));
                case RE::FormType::AlchemyItem:
                    return std::string(PotionTypeLabel(object->As<RE::AlchemyItem>()));
                case RE::FormType::Ingredient:
                    return "Ingredient";
                case RE::FormType::Book:
                    return std::string(BookTypeLabel(object->As<RE::TESObjectBOOK>()));
                case RE::FormType::Scroll:
                    return "Scroll";
                case RE::FormType::KeyMaster:
                    return "Key";
                case RE::FormType::SoulGem:
                    return "Soul Gem";
                case RE::FormType::Ammo:
                    if (LocalFormIDIs(object, kDLC2RieklingSpearThrown)) {
                        return "Spear";
                    }
                    if (auto* ammo = object->As<RE::TESAmmo>(); ammo && ammo->IsBolt()) {
                        return "Bolt";
                    }
                    return "Arrow";
                case RE::FormType::Misc:
                    return std::string(MiscTypeLabel(object->As<RE::TESObjectMISC>()));
                case RE::FormType::Light:
                    return "Torch";
                default:
                    return {};
            }
        }

        bool IsJunkOverrideType(std::string_view label) {
            const auto& configured = I4JunkConfig::GetSingleton().subTypeDisplay;
            const std::string_view overrideLabel = configured.empty() ? "Junk" : configured;
            return NormalizeType(label) == NormalizeType(overrideLabel);
        }

        bool LabelMatchesConfigured(std::string_view label, const std::vector<std::string>& configuredNormalized) {
            if (label.empty()) {
                return false;
            }
            const auto normalized = NormalizeType(label);
            for (const auto& configuredType : configuredNormalized) {
                if (normalized == configuredType) {
                    return true;
                }
            }
            return false;
        }

        std::string GetOpenSubTypeDisplay(RE::InventoryEntryData* entry) {
            auto* itemList = UIUtil::ItemList::GetOpenList();
            if (!itemList || !entry || !entry->object) {
                return {};
            }

            std::string fromObject;
            for (std::uint32_t i = 0, size = itemList->items.size(); i < size; ++i) {
                auto* item = itemList->items[i];
                if (!item || !item->data.objDesc || !item->obj.IsObject()) {
                    continue;
                }
                if (item->data.objDesc != entry && item->data.objDesc->object != entry->object) {
                    continue;
                }

                RE::GFxValue value;
                if (!item->obj.GetMember("subTypeDisplay", &value) || !value.IsString()) {
                    continue;
                }
                const char* text = value.GetString();
                if (!text || !*text || IsJunkOverrideType(text)) {
                    continue;
                }
                if (item->data.objDesc == entry) {
                    return text;
                }
                if (fromObject.empty()) {
                    fromObject = text;
                }
            }
            return fromObject;
        }

        bool TypeMatches(RE::InventoryEntryData* entry) {
            const auto& configured = Settings::GetAutoJunkTypes();
            if (!entry || !entry->object || configured.empty()) {
                return false;
            }

            std::vector<std::string> configuredNormalized;
            configuredNormalized.reserve(configured.size());
            for (const auto& type : configured) {
                configuredNormalized.push_back(NormalizeType(type));
            }

            if (LabelMatchesConfigured(GetSkyUITypeLabel(entry->object), configuredNormalized)) {
                return true;
            }
            return LabelMatchesConfigured(GetOpenSubTypeDisplay(entry), configuredNormalized);
        }

        bool ExtraListMatchesUniqueID(const RE::ExtraDataList* extraList, std::uint16_t uniqueID) {
            if (!extraList) {
                return false;
            }
            const auto* extraId = extraList->GetByType<RE::ExtraUniqueID>();
            return extraId && extraId->uniqueID == uniqueID;
        }

        bool ContainerOrBarterOpen() {
            auto* ui = RE::UI::GetSingleton();
            if (!ui) {
                return false;
            }
            return ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME) || ui->IsMenuOpen(RE::BarterMenu::MENU_NAME);
        }

        bool TryMarkExtra(RE::InventoryEntryData* entry, RE::ExtraDataList* extraList) {
            if (!entry || !entry->object) {
                return false;
            }
            if (Settings::GetAutoJunkTypes().empty()) {
                return false;
            }
            if (JunkHandler::operationInProgress.load()) {
                return false;
            }
            if (entry->IsQuestObject()) {
                return false;
            }
            if (auto* gold = Settings::GetGold001(); gold && entry->object == gold) {
                return false;
            }
            if (Settings::ProtectEquipped() && entry->IsWorn()) {
                return false;
            }
            if (Settings::ProtectFavorites() && entry->IsFavorited()) {
                return false;
            }
            if (!TypeMatches(entry)) {
                return false;
            }

            const auto identity = JunkDataManager::BuildIdentityForEntry(entry, extraList);
            auto& manager = JunkDataManager::GetSingleton();
            if (manager.IsNoAutoJunk(identity) || manager.IsJunk(identity)) {
                return false;
            }
            return manager.AddJunkIdentity(identity, true).has_value();
        }

        bool ApplyToInventoryMap(auto& inventory) {
            bool marked = false;
            for (auto& [object, data] : inventory) {
                if (!object || data.first <= 0 || !data.second) {
                    continue;
                }
                if (AutoJunk::TryMarkEntry(data.second.get())) {
                    marked = true;
                }
            }
            return marked;
        }

        void RefreshOpenItemList() {
            if (auto* itemList = UIUtil::ItemList::GetOpenList()) {
                itemList->Update();
            }
        }

        void RecaptureAfterAutoJunk(bool marked) {
            if (marked) {
                SkyPromptIntegration::GetSingleton().RecapturePreviews();
                RefreshOpenItemList();
            }
        }

        void ScheduleMenuScanRetry() {
            auto* tasks = SKSE::GetTaskInterface();
            if (!tasks) {
                return;
            }
            tasks->AddUITask([]() {
                if (!Settings::GetAutoJunkOnMenuOpen() || Settings::GetAutoJunkTypes().empty()) {
                    return;
                }
                RecaptureAfterAutoJunk(AutoJunk::ApplyToOpenMenus());
            });
        }

        void TryMarkPickup(RE::FormID baseObj, std::uint16_t uniqueID, bool isRetry);

        void SchedulePickupRetry(RE::FormID baseObj, std::uint16_t uniqueID) {
            auto* tasks = SKSE::GetTaskInterface();
            if (!tasks) {
                return;
            }
            tasks->AddUITask([baseObj, uniqueID]() {
                TryMarkPickup(baseObj, uniqueID, true);
            });
        }

        void TryMarkPickup(RE::FormID baseObj, std::uint16_t uniqueID, bool isRetry) {
            if (!Settings::GetAutoJunkOnPickup() || Settings::GetAutoJunkTypes().empty()) {
                return;
            }
            if (ContainerOrBarterOpen()) {
                return;
            }

            auto* player = RE::PlayerCharacter::GetSingleton();
            auto* form = RE::TESForm::LookupByID(baseObj);
            auto* object = form ? form->As<RE::TESBoundObject>() : nullptr;
            if (!player || !object) {
                return;
            }

            auto inventory = player->GetInventory();
            auto it = inventory.find(object);
            if (it == inventory.end() || !it->second.second) {
                if (!isRetry) {
                    SchedulePickupRetry(baseObj, uniqueID);
                }
                return;
            }

            auto* entry = it->second.second.get();
            if (uniqueID == 0) {
                AutoJunk::TryMarkEntry(entry);
                return;
            }

            bool marked = false;
            if (entry->extraLists) {
                for (auto* extraList : *entry->extraLists) {
                    if (ExtraListMatchesUniqueID(extraList, uniqueID)) {
                        if (TryMarkExtra(entry, extraList)) {
                            marked = true;
                        }
                    }
                }
            }
            if (!marked) {
                AutoJunk::TryMarkEntry(entry);
            }
        }

        class AutoJunkEventHandler :
            public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
            public RE::BSTEventSink<RE::TESContainerChangedEvent> {
        public:
            static AutoJunkEventHandler* GetSingleton() {
                static AutoJunkEventHandler singleton;
                return &singleton;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent* a_event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override {
                if (!a_event || !a_event->opening) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                if (a_event->menuName != RE::ContainerMenu::MENU_NAME &&
                    a_event->menuName != RE::BarterMenu::MENU_NAME) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                if (!Settings::GetAutoJunkOnMenuOpen() || Settings::GetAutoJunkTypes().empty()) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                RecaptureAfterAutoJunk(AutoJunk::ApplyToOpenMenus());
                ScheduleMenuScanRetry();
                return RE::BSEventNotifyControl::kContinue;
            }

            RE::BSEventNotifyControl ProcessEvent(
                const RE::TESContainerChangedEvent* a_event,
                RE::BSTEventSource<RE::TESContainerChangedEvent>*) override {
                if (!a_event || a_event->itemCount <= 0) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                if (!Settings::GetAutoJunkOnPickup() || Settings::GetAutoJunkTypes().empty()) {
                    return RE::BSEventNotifyControl::kContinue;
                }
                if (JunkHandler::operationInProgress.load() || ContainerOrBarterOpen()) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                auto* player = RE::PlayerCharacter::GetSingleton();
                if (!player || a_event->newContainer != player->GetFormID()) {
                    return RE::BSEventNotifyControl::kContinue;
                }

                TryMarkPickup(a_event->baseObj, a_event->uniqueID, false);
                return RE::BSEventNotifyControl::kContinue;
            }

        private:
            AutoJunkEventHandler() = default;
        };
    }

    std::span<const char* const> AutoJunk::KnownItemTypes() {
        return std::span<const char* const>(kKnownItemTypes);
    }

    bool AutoJunk::TryMarkEntry(RE::InventoryEntryData* entry) {
        if (!entry || !entry->object) {
            return false;
        }

        bool marked = false;
        if (!entry->extraLists || entry->extraLists->empty()) {
            return TryMarkExtra(entry, nullptr);
        }

        for (auto* extraList : *entry->extraLists) {
            if (TryMarkExtra(entry, extraList)) {
                marked = true;
            }
        }
        return marked;
    }

    bool AutoJunk::ApplyToPlayerInventory() {
        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player) {
            return false;
        }
        auto inventory = player->GetInventory();
        return ApplyToInventoryMap(inventory);
    }

    bool AutoJunk::ApplyToReferenceInventory(RE::TESObjectREFR* container) {
        if (!container) {
            return false;
        }
        auto inventory = container->GetInventory();
        return ApplyToInventoryMap(inventory);
    }

    bool AutoJunk::ApplyToOpenMenus() {
        bool marked = ApplyToPlayerInventory();

        auto* ui = RE::UI::GetSingleton();
        if (!ui) {
            return marked;
        }

        if (ui->IsMenuOpen(RE::ContainerMenu::MENU_NAME)) {
            if (ApplyToReferenceInventory(JunkHandler::GetContainerMenuContainer())) {
                marked = true;
            }
        }

        if (ui->IsMenuOpen(RE::BarterMenu::MENU_NAME)) {
            if (ApplyToReferenceInventory(JunkHandler::GetBarterMenuContainer())) {
                marked = true;
            }
            if (ApplyToReferenceInventory(JunkHandler::GetBarterMenuMerchantContainer())) {
                marked = true;
            }
        }

        return marked;
    }

    void AutoJunk::Install() {
        auto* handler = AutoJunkEventHandler::GetSingleton();
        auto* ui = RE::UI::GetSingleton();
        if (ui) {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(handler);
        } else {
            SKSE::log::error("UI singleton unavailable; Auto Junk menu sink not registered");
        }

        if (auto* holder = RE::ScriptEventSourceHolder::GetSingleton()) {
            holder->AddEventSink<RE::TESContainerChangedEvent>(handler);
        } else {
            SKSE::log::error("ScriptEventSourceHolder unavailable; Auto Junk pickup sink not registered");
        }

        SKSE::log::info("Registered Auto Junk event sinks");
    }
}
