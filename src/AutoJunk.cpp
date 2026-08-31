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
#include <windows.h>

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

        constexpr const char* const kKnownItemMaterials[] = {
            "Hide",
            "Wood",
            "Iron",
            "Iron Banded",
            "Studded",
            "Leather",
            "Imperial",
            "Stormcloak",
            "Forsworn",
            "Draugr",
            "Draugr Honed",
            "Falmer",
            "Falmer Honed",
            "Silver",
            "Steel",
            "Steel Plate",
            "Scaled",
            "Hunter",
            "Vampire",
            "Bonemold",
            "Chitin",
            "Morag Tong",
            "Elven",
            "Elven Gilded",
            "Dwarven",
            "Nordic",
            "Orcish",
            "Glass",
            "Falmer Hardened",
            "Dawnguard",
            "Ebony",
            "Stalhrim",
            "Dragonscale",
            "Dragonplate",
            "Dragonbone",
            "Daedric",
            "Deathbrand",
            "Aetherium"
        };

        constexpr RE::FormID kLocalIDMask = 0x00FFFFFF;
        constexpr RE::FormID kWeapPickaxe = 0x0E3C16;
        constexpr RE::FormID kSSDRocksplinterPickaxe = 0x06A707;
        constexpr RE::FormID kDunVolunruudPickaxe = 0x1019D4;
        constexpr RE::FormID kAxe01 = 0x02F2F4;
        constexpr RE::FormID kDunHaltedStreamPoachersAxe = 0x0AE086;
        constexpr RE::FormID kDLC1ClothesVampireLordArmor = 0x011A84;
        constexpr RE::FormID kDLC2RieklingSpearThrown = 0x017720;
        constexpr RE::FormID kDaedricArrow = 0x0139C0;
        constexpr RE::FormID kEbonyArrow = 0x0139BF;
        constexpr RE::FormID kGlassArrow = 0x0139BE;
        constexpr RE::FormID kElvenArrow = 0x0139BD;
        constexpr RE::FormID kDLC1ElvenArrowBlessed = 0x0098A1;
        constexpr RE::FormID kDLC1ElvenArrowBlood = 0x0098A0;
        constexpr RE::FormID kDwarvenArrow = 0x0139BC;
        constexpr RE::FormID kDwarvenSphereArrow = 0x07B932;
        constexpr RE::FormID kDwarvenSphereBolt01 = 0x07B935;
        constexpr RE::FormID kDwarvenSphereBolt02 = 0x10EC8C;
        constexpr RE::FormID kDLC2DwarvenBallistaBolt = 0x0339A1;
        constexpr RE::FormID kOrcishArrow = 0x0139BB;
        constexpr RE::FormID kNordHeroArrow = 0x0EAFDF;
        constexpr RE::FormID kDraugrArrow = 0x034182;
        constexpr RE::FormID kFalmerArrow = 0x038341;
        constexpr RE::FormID kSteelArrow = 0x01397F;
        constexpr RE::FormID kMQ101SteelArrow = 0x105EE7;
        constexpr RE::FormID kIronArrow = 0x01397D;
        constexpr RE::FormID kCWArrow = 0x020DDF;
        constexpr RE::FormID kCWArrowShort = 0x020F02;
        constexpr RE::FormID kTrapDart = 0x0236DD;
        constexpr RE::FormID kDunArcherPracticeArrow = 0x0CAB52;
        constexpr RE::FormID kDunGeirmundSigdisArrowsIllusion = 0x0E738A;
        constexpr RE::FormID kFollowerIronArrow = 0x10E2DE;
        constexpr RE::FormID kTestDLC1Bolt = 0x00590C;
        constexpr RE::FormID kForswornArrow = 0x0CEE9E;
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

        RE::BGSKeywordForm* KeywordFormOf(RE::TESBoundObject* object) {
            if (!object) {
                return nullptr;
            }
            if (auto* weap = object->As<RE::TESObjectWEAP>()) {
                return weap;
            }
            if (auto* armo = object->As<RE::TESObjectARMO>()) {
                return armo;
            }
            if (auto* ammo = object->As<RE::TESAmmo>()) {
                return ammo->AsKeywordForm();
            }
            return nullptr;
        }

        RE::BGSKeywordForm* KeywordFormOfAny(RE::TESBoundObject* object) {
            if (auto* form = KeywordFormOf(object)) {
                return form;
            }
            if (!object) {
                return nullptr;
            }
            if (auto* book = object->As<RE::TESObjectBOOK>()) {
                return book;
            }
            if (auto* misc = object->As<RE::TESObjectMISC>()) {
                return misc;
            }
            if (auto* magic = object->As<RE::MagicItem>()) {
                return magic;
            }
            return nullptr;
        }

        bool KeywordFormHasEditorId(RE::BGSKeywordForm* keywordForm, std::string_view editorId, bool contains) {
            if (!keywordForm || editorId.empty()) {
                return false;
            }
            bool found = false;
            keywordForm->ForEachKeyword([&](RE::BGSKeyword* keyword) {
                if (!keyword) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                const char* id = keyword->GetFormEditorID();
                if (!id || !*id) {
                    return RE::BSContainer::ForEachResult::kContinue;
                }
                const bool match = contains ? Util::String::iContains(id, editorId) : Util::String::iEquals(id, editorId);
                if (match) {
                    found = true;
                    return RE::BSContainer::ForEachResult::kStop;
                }
                return RE::BSContainer::ForEachResult::kContinue;
            });
            return found;
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

        std::string_view KeywordMaterialLabel(RE::BGSKeywordForm* keywords) {
            if (!keywords) {
                return "Other";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialDaedric") || HasEditorKeyword(keywords, "WeapMaterialDaedric")) {
                return "Daedric";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialDragonplate")) {
                return "Dragonplate";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialDragonscale")) {
                return "Dragonscale";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialDwarven") || HasEditorKeyword(keywords, "WeapMaterialDwarven")) {
                return "Dwarven";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialEbony") || HasEditorKeyword(keywords, "WeapMaterialEbony")) {
                return "Ebony";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialElven") || HasEditorKeyword(keywords, "WeapMaterialElven")) {
                return "Elven";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialElvenGilded")) {
                return "Elven Gilded";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialGlass") || HasEditorKeyword(keywords, "WeapMaterialGlass")) {
                return "Glass";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialHide")) {
                return "Hide";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialImperialHeavy") ||
                HasEditorKeyword(keywords, "ArmorMaterialImperialLight") ||
                HasEditorKeyword(keywords, "WeapMaterialImperial")) {
                return "Imperial";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialImperialStudded")) {
                return "Studded";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialIron") || HasEditorKeyword(keywords, "WeapMaterialIron")) {
                return "Iron";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialIronBanded")) {
                return "Iron Banded";
            }
            if (HasEditorKeyword(keywords, "DLC1ArmorMaterialVampire")) {
                return "Vampire";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialLeather")) {
                return "Leather";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialOrcish") || HasEditorKeyword(keywords, "WeapMaterialOrcish")) {
                return "Orcish";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialScaled")) {
                return "Scaled";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialSteel") || HasEditorKeyword(keywords, "WeapMaterialSteel")) {
                return "Steel";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialSteelPlate")) {
                return "Steel Plate";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialStormcloak")) {
                return "Stormcloak";
            }
            if (HasEditorKeyword(keywords, "ArmorMaterialStudded")) {
                return "Studded";
            }
            if (HasEditorKeyword(keywords, "DLC1ArmorMaterialDawnguard")) {
                return "Dawnguard";
            }
            if (HasEditorKeyword(keywords, "DLC1ArmorMaterialFalmerHardened") ||
                HasEditorKeyword(keywords, "DLC1ArmorMaterialFalmerHeavy")) {
                return "Falmer Hardened";
            }
            if (HasEditorKeyword(keywords, "DLC1ArmorMaterialHunter")) {
                return "Hunter";
            }
            if (HasEditorKeyword(keywords, "DLC1LD_CraftingMaterialAetherium")) {
                return "Aetherium";
            }
            if (HasEditorKeyword(keywords, "DLC1WeapMaterialDragonbone")) {
                return "Dragonbone";
            }
            if (HasEditorKeyword(keywords, "DLC2ArmorMaterialBonemoldHeavy") ||
                HasEditorKeyword(keywords, "DLC2ArmorMaterialBonemoldLight")) {
                return "Bonemold";
            }
            if (HasEditorKeyword(keywords, "DLC2ArmorMaterialChitinHeavy") ||
                HasEditorKeyword(keywords, "DLC2ArmorMaterialChitinLight")) {
                return "Chitin";
            }
            if (HasEditorKeyword(keywords, "DLC2ArmorMaterialMoragTong")) {
                return "Morag Tong";
            }
            if (HasEditorKeyword(keywords, "DLC2ArmorMaterialNordicHeavy") ||
                HasEditorKeyword(keywords, "DLC2ArmorMaterialNordicLight") ||
                HasEditorKeyword(keywords, "DLC2WeaponMaterialNordic")) {
                return "Nordic";
            }
            if (HasEditorKeyword(keywords, "DLC2ArmorMaterialStalhrimHeavy") ||
                HasEditorKeyword(keywords, "DLC2ArmorMaterialStalhrimLight") ||
                HasEditorKeyword(keywords, "DLC2WeaponMaterialStalhrim")) {
                if (HasEditorKeyword(keywords, "DLC2dunHaknirArmor")) {
                    return "Deathbrand";
                }
                return "Stalhrim";
            }
            if (HasEditorKeyword(keywords, "WeapMaterialDraugr")) {
                return "Draugr";
            }
            if (HasEditorKeyword(keywords, "WeapMaterialDraugrHoned")) {
                return "Draugr Honed";
            }
            if (HasEditorKeyword(keywords, "WeapMaterialFalmer") || HasEditorKeyword(keywords, "ArmorMaterialFalmer")) {
                return "Falmer";
            }
            if (HasEditorKeyword(keywords, "WeapMaterialFalmerHoned")) {
                return "Falmer Honed";
            }
            if (HasEditorKeyword(keywords, "WeapMaterialSilver")) {
                return "Silver";
            }
            if (HasEditorKeyword(keywords, "WeapMaterialWood")) {
                return "Wood";
            }
            return "Other";
        }

        std::string_view AmmoBaseIdMaterial(RE::TESBoundObject* object) {
            switch (LocalFormID(object)) {
                case kDaedricArrow:
                    return "Daedric";
                case kEbonyArrow:
                    return "Ebony";
                case kGlassArrow:
                    return "Glass";
                case kElvenArrow:
                case kDLC1ElvenArrowBlessed:
                case kDLC1ElvenArrowBlood:
                    return "Elven";
                case kDwarvenArrow:
                case kDwarvenSphereArrow:
                case kDwarvenSphereBolt01:
                case kDwarvenSphereBolt02:
                case kDLC2DwarvenBallistaBolt:
                    return "Dwarven";
                case kOrcishArrow:
                    return "Orcish";
                case kNordHeroArrow:
                    return "Nordic";
                case kDraugrArrow:
                    return "Draugr";
                case kFalmerArrow:
                    return "Falmer";
                case kSteelArrow:
                case kMQ101SteelArrow:
                    return "Steel";
                case kIronArrow:
                case kCWArrow:
                case kCWArrowShort:
                case kTrapDart:
                case kDunArcherPracticeArrow:
                case kDunGeirmundSigdisArrowsIllusion:
                case kFollowerIronArrow:
                case kTestDLC1Bolt:
                    return "Iron";
                case kForswornArrow:
                    return "Forsworn";
                case kDLC2RieklingSpearThrown:
                    return "Wood";
                default:
                    return {};
            }
        }

        std::string GetSkyUIMaterialLabel(RE::TESBoundObject* object) {
            if (!object) {
                return {};
            }
            const auto formType = object->GetFormType();
            if (formType != RE::FormType::Weapon && formType != RE::FormType::Armor && formType != RE::FormType::Ammo) {
                return {};
            }
            if (formType == RE::FormType::Ammo) {
                if (const auto ammoMaterial = AmmoBaseIdMaterial(object); !ammoMaterial.empty()) {
                    return std::string(ammoMaterial);
                }
            }
            return std::string(KeywordMaterialLabel(KeywordFormOf(object)));
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

        std::string GFxValueToUtf8(const RE::GFxValue& value) {
            if (value.IsString()) {
                const char* text = value.GetString();
                return (text && *text) ? std::string(text) : std::string{};
            }
            if (value.IsStringW()) {
                const wchar_t* text = value.GetStringW();
                if (!text || !*text) {
                    return {};
                }
                const int utf8Size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
                if (utf8Size <= 1) {
                    return {};
                }
                std::string utf8(static_cast<std::size_t>(utf8Size - 1), '\0');
                WideCharToMultiByte(CP_UTF8, 0, text, -1, utf8.data(), utf8Size, nullptr, nullptr);
                return utf8;
            }
            return {};
        }

        std::string GetOpenGFxString(RE::InventoryEntryData* entry, const char* member, bool skipJunkOverride) {
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
                if (!item->obj.GetMember(member, &value)) {
                    continue;
                }
                const std::string text = GFxValueToUtf8(value);
                if (text.empty()) {
                    continue;
                }
                if (skipJunkOverride && IsJunkOverrideType(text)) {
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

        bool ConfiguredListMatches(
            const std::vector<std::string>& configured,
            std::string_view nativeLabel,
            std::string_view gfxLabel) {
            if (configured.empty()) {
                return false;
            }

            std::vector<std::string> configuredNormalized;
            configuredNormalized.reserve(configured.size());
            for (const auto& value : configured) {
                configuredNormalized.push_back(NormalizeType(value));
            }

            return LabelMatchesConfigured(nativeLabel, configuredNormalized) ||
                LabelMatchesConfigured(gfxLabel, configuredNormalized);
        }

        bool MaterialKeywordsContainConfigured(RE::TESBoundObject* object, const std::vector<std::string>& configured) {
            if (configured.empty()) {
                return false;
            }
            auto* keywords = KeywordFormOf(object);
            if (!keywords) {
                return false;
            }
            for (const auto& value : configured) {
                if (KeywordFormHasEditorId(keywords, value, true)) {
                    return true;
                }
            }
            return false;
        }

        bool ConfiguredKeywordsMatch(RE::TESBoundObject* object, const std::vector<std::string>& configured) {
            if (configured.empty()) {
                return false;
            }
            auto* keywords = KeywordFormOfAny(object);
            if (!keywords) {
                return false;
            }
            for (const auto& value : configured) {
                if (KeywordFormHasEditorId(keywords, value, false)) {
                    return true;
                }
            }
            return false;
        }

        bool AutoJunkListsEmpty() {
            return Settings::GetAutoJunkTypes().empty() &&
                Settings::GetAutoJunkMaterials().empty() &&
                Settings::GetAutoJunkKeywords().empty();
        }

        bool AutoJunkMatches(RE::InventoryEntryData* entry) {
            if (!entry || !entry->object) {
                return false;
            }
            if (ConfiguredListMatches(
                    Settings::GetAutoJunkTypes(),
                    GetSkyUITypeLabel(entry->object),
                    GetOpenGFxString(entry, "subTypeDisplay", true))) {
                return true;
            }
            const auto& materials = Settings::GetAutoJunkMaterials();
            if (ConfiguredListMatches(
                    materials,
                    GetSkyUIMaterialLabel(entry->object),
                    GetOpenGFxString(entry, "materialDisplay", false)) ||
                MaterialKeywordsContainConfigured(entry->object, materials)) {
                return true;
            }
            return ConfiguredKeywordsMatch(entry->object, Settings::GetAutoJunkKeywords());
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
            if (AutoJunkListsEmpty()) {
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
            if (!AutoJunkMatches(entry)) {
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
                if (!Settings::GetAutoJunkOnMenuOpen() || AutoJunkListsEmpty()) {
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
            if (!Settings::GetAutoJunkOnPickup() || AutoJunkListsEmpty()) {
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
                if (!Settings::GetAutoJunkOnMenuOpen() || AutoJunkListsEmpty()) {
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
                if (!Settings::GetAutoJunkOnPickup() || AutoJunkListsEmpty()) {
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

    std::span<const char* const> AutoJunk::KnownItemMaterials() {
        return std::span<const char* const>(kKnownItemMaterials);
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
