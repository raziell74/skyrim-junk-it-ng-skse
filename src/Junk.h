#pragma once 
#include "settings.h"
using namespace RE;

namespace JunkIt {
    class JunkHandler {
        public: 
            static void UpdateItemKeywords() {
                BGSKeyword* isJunkKYWD = JunkIt::Settings::GetIsJunkKYWD();

                BGSListForm* JustList = Settings::GetJunkList();
                JustList->ForEachForm([&](TESForm& form) {
                    BGSKeywordForm* keywordForm = nullptr;
                    
                    if (form.GetFormType() == FormType::Ammo) {
                        // Ammo has to be treated differently as it does not inherit from BGSKeywordForm
                        TESAmmo* ammo = form.As<TESAmmo>();
                        keywordForm = ammo->AsKeywordForm();
                    } else {
                        // Generalized handling for all other form types
                        keywordForm = form.As<BGSKeywordForm>();
                    }

                    if (keywordForm && !keywordForm->HasKeyword(isJunkKYWD)) {
                        // SKSE::log::info("[OnGameLoad] keyword correction: Adding IsJunk keyword on {}", form.GetName());
                        keywordForm->AddKeyword(isJunkKYWD);
                    }

                    return BSContainer::ForEachResult::kContinue;
                });

                BGSListForm* UnjunkedList = Settings::GetUnjunkedList();
                UnjunkedList->ForEachForm([&](TESForm& form) {
                    BGSKeywordForm* keywordForm = nullptr;
                    
                    if (form.GetFormType() == FormType::Ammo) {
                        // Ammo has to be treated differently as it does not inherit from BGSKeywordForm
                        TESAmmo* ammo = form.As<TESAmmo>();
                        keywordForm = ammo->AsKeywordForm();
                    } else {
                        // Generalized handling for all other form types
                        keywordForm = form.As<BGSKeywordForm>();
                    }

                    if (keywordForm && keywordForm->HasKeyword(isJunkKYWD)) {
                        // SKSE::log::info("[OnGameLoad] keyword correction: Removing IsJunk keyword on {}", form.GetName());
                        keywordForm->RemoveKeyword(isJunkKYWD);
                    }

                    return BSContainer::ForEachResult::kContinue;
                });
            }

            static ItemList* GetItemListMenu();

            static void TransferItem(TESBoundObject* a_item, TESObjectREFR* a_fromContainer, TESObjectREFR* a_toContainer, ITEM_REMOVE_REASON a_reason, std::int32_t a_count, InventoryEntryData* a_invData);

        private:

            static inline ItemList* ItemListMenu;
            static inline std::string MenuName;
            
            static inline ExtraDataType GetExtraDataTypeByIndex(std::int32_t i) {
                switch (i) {
                    case 0: return ExtraDataType::kNone;
                    case 1: return ExtraDataType::kCell3D;
                    case 2: return ExtraDataType::kOpenCloseActivateRef;
                    case 3: return ExtraDataType::kAmmo;
                    case 4: return ExtraDataType::kPatrolRefData;
                    case 5: return ExtraDataType::kPackageData;
                    case 6: return ExtraDataType::kOcclusionShape;
                    case 7: return ExtraDataType::kCollisionData;
                    case 8: return ExtraDataType::kFollower;
                    case 9: return ExtraDataType::kLevCreaModifier;
                    case 10: return ExtraDataType::kGhost;
                    case 11: return ExtraDataType::kOriginalReference;
                    case 12: return ExtraDataType::kOwnership;
                    case 13: return ExtraDataType::kGlobal;
                    case 14: return ExtraDataType::kRank;
                    case 15: return ExtraDataType::kCount;
                    case 16: return ExtraDataType::kHealth;
                    case 17: return ExtraDataType::kUnk26;
                    case 18: return ExtraDataType::kTimeLeft;
                    case 19: return ExtraDataType::kCharge;
                    case 20: return ExtraDataType::kLight;
                    case 21: return ExtraDataType::kLock;
                    case 22: return ExtraDataType::kTeleport;
                    case 23: return ExtraDataType::kMapMarker;
                    case 24: return ExtraDataType::kLeveledCreature;
                    case 25: return ExtraDataType::kLeveledItem;
                    case 26: return ExtraDataType::kScale;
                    case 27: return ExtraDataType::kMissingLinkedRefIDs;
                    case 28: return ExtraDataType::kMagicCaster;
                    case 29: return ExtraDataType::kNonActorMagicTarget;
                    case 30: return ExtraDataType::kUnk33;
                    case 31: return ExtraDataType::kPlayerCrimeList;
                    case 32: return ExtraDataType::kUnk35;
                    case 33: return ExtraDataType::kEnableStateParent;
                    case 34: return ExtraDataType::kEnableStateChildren;
                    case 35: return ExtraDataType::kItemDropper;
                    case 36: return ExtraDataType::kDroppedItemList;
                    case 37: return ExtraDataType::kRandomTeleportMarker;
                    case 38: return ExtraDataType::kUnk3B;
                    case 39: return ExtraDataType::kSavedHavokData;
                    case 40: return ExtraDataType::kCannotWear;
                    case 41: return ExtraDataType::kPoison;
                    case 42: return ExtraDataType::kMagicLight;
                    case 43: return ExtraDataType::kLastFinishedSequence;
                    case 44: return ExtraDataType::kSavedAnimation;
                    case 45: return ExtraDataType::kNorthRotation;
                    case 46: return ExtraDataType::kSpawnContainer;
                    case 47: return ExtraDataType::kFriendHits;
                    case 48: return ExtraDataType::kHeadingTarget;
                    case 49: return ExtraDataType::kUnk46;
                    case 50: return ExtraDataType::kRefractionProperty;
                    case 51: return ExtraDataType::kStartingWorldOrCell;
                    case 52: return ExtraDataType::kHotkey;
                    case 53: return ExtraDataType::kEditorRef3DData;
                    case 54: return ExtraDataType::kEditorRefMoveData;
                    case 55: return ExtraDataType::kInfoGeneralTopic;
                    case 56: return ExtraDataType::kHasNoRumors;
                    case 57: return ExtraDataType::kSound;
                    case 58: return ExtraDataType::kTerminalState;
                    case 59: return ExtraDataType::kLinkedRef;
                    case 60: return ExtraDataType::kLinkedRefChildren;
                    case 61: return ExtraDataType::kActivateRef;
                    case 62: return ExtraDataType::kActivateRefChildren;
                    case 63: return ExtraDataType::kCanTalkToPlayer;
                    case 64: return ExtraDataType::kObjectHealth;
                    case 65: return ExtraDataType::kCellImageSpace;
                    case 66: return ExtraDataType::kNavMeshPortal;
                    case 67: return ExtraDataType::kModelSwap;
                    case 68: return ExtraDataType::kRadius;
                    case 69: return ExtraDataType::kUnk5A;
                    case 70: return ExtraDataType::kFactionChanges;
                    case 71: return ExtraDataType::kDismemberedLimbs;
                    case 72: return ExtraDataType::kActorCause;
                    case 73: return ExtraDataType::kMultiBound;
                    case 74: return ExtraDataType::kMultiBoundMarkerData;
                    case 75: return ExtraDataType::kMultiBoundRef;
                    case 76: return ExtraDataType::kReflectedRefs;
                    case 77: return ExtraDataType::kReflectorRefs;
                    case 78: return ExtraDataType::kEmittanceSource;
                    case 79: return ExtraDataType::kUnk64;
                    case 80: return ExtraDataType::kCombatStyle;
                    case 81: return ExtraDataType::kUnk66;
                    case 82: return ExtraDataType::kPrimitive;
                    case 83: return ExtraDataType::kOpenCloseActivateRef;
                    case 84: return ExtraDataType::kAnimNoteReceiver;
                    case 85: return ExtraDataType::kAmmo;
                    case 86: return ExtraDataType::kPatrolRefData;
                    case 87: return ExtraDataType::kPackageData;
                    case 88: return ExtraDataType::kOcclusionShape;
                    case 89: return ExtraDataType::kCollisionData;
                    case 90: return ExtraDataType::kSayTopicInfoOnceADay;
                    case 91: return ExtraDataType::kEncounterZone;
                    case 92: return ExtraDataType::kSayTopicInfo;
                    case 93: return ExtraDataType::kOcclusionPlaneRefData;
                    case 94: return ExtraDataType::kPortalRefData;
                    case 95: return ExtraDataType::kPortal;
                    case 96: return ExtraDataType::kRoom;
                    case 97: return ExtraDataType::kHealthPerc;
                    case 98: return ExtraDataType::kRoomRefData;
                    case 99: return ExtraDataType::kGuardedRefData;
                    case 100: return ExtraDataType::kCreatureAwakeSound;
                    case 101: return ExtraDataType::kUnk7A;
                    case 102: return ExtraDataType::kHorse;
                    case 103: return ExtraDataType::kIgnoredBySandbox;
                    case 104: return ExtraDataType::kCellAcousticSpace;
                    case 105: return ExtraDataType::kReservedMarkers;
                    case 106: return ExtraDataType::kWeaponIdleSound;
                    case 107: return ExtraDataType::kWaterLightRefs;
                    case 108: return ExtraDataType::kLitWaterRefs;
                    case 109: return ExtraDataType::kWeaponAttackSound;
                    case 110: return ExtraDataType::kActivateLoopSound;
                    case 111: return ExtraDataType::kPatrolRefInUseData;
                    case 112: return ExtraDataType::kAshPileRef;
                    case 113: return ExtraDataType::kCreatureMovementSound;
                    case 114: return ExtraDataType::kFollowerSwimBreadcrumbs;
                    case 115: return ExtraDataType::kAliasInstanceArray;
                    case 116: return ExtraDataType::kLocation;
                    case 117: return ExtraDataType::kUnk8A;
                    case 118: return ExtraDataType::kLocationRefType;
                    case 119: return ExtraDataType::kPromotedRef;
                    case 120: return ExtraDataType::kAnimationSequencer;
                    case 121: return ExtraDataType::kOutfitItem;
                    case 122: return ExtraDataType::kUnk8F;
                    case 123: return ExtraDataType::kLeveledItemBase;
                    case 124: return ExtraDataType::kLightData;
                    case 125: return ExtraDataType::kSceneData;
                    case 126: return ExtraDataType::kBadPosition;
                    case 127: return ExtraDataType::kHeadTrackingWeight;
                    case 128: return ExtraDataType::kFromAlias;
                    case 129: return ExtraDataType::kShouldWear;
                    case 130: return ExtraDataType::kFavorCost;
                    case 131: return ExtraDataType::kAttachedArrows3D;
                    case 132: return ExtraDataType::kTextDisplayData;
                    case 133: return ExtraDataType::kAlphaCutoff;
                    case 134: return ExtraDataType::kEnchantment;
                    case 135: return ExtraDataType::kSoul;
                    case 136: return ExtraDataType::kForcedTarget;
                    case 137: return ExtraDataType::kUnk9E;
                    case 138: return ExtraDataType::kUniqueID;
                    case 139: return ExtraDataType::kFlags;
                    case 140: return ExtraDataType::kRefrPath;
                    case 141: return ExtraDataType::kDecalGroup;
                    case 142: return ExtraDataType::kLockList;
                    case 143: return ExtraDataType::kForcedLandingMarker;
                    case 144: return ExtraDataType::kLargeRefOwnerCells;
                    case 145: return ExtraDataType::kCellWaterEnvMap;
                    case 146: return ExtraDataType::kCellGrassData;
                    case 147: return ExtraDataType::kTeleportName;
                    case 148: return ExtraDataType::kInteraction;
                    case 149: return ExtraDataType::kWaterData;
                    case 150: return ExtraDataType::kWaterCurrentZoneData;
                    case 151: return ExtraDataType::kAttachRef;
                    case 152: return ExtraDataType::kAttachRefChildren;
                    case 153: return ExtraDataType::kGroupConstraint;
                    case 154: return ExtraDataType::kScriptedAnimDependence;
                    case 155: return ExtraDataType::kCachedScale;
                    case 156: return ExtraDataType::kRaceData;
                    case 157: return ExtraDataType::kGIDBuffer;
                    case 158: return ExtraDataType::kMissingRefIDs;
                    case 159: return ExtraDataType::kUnkB4;
                    case 160: return ExtraDataType::kResourcesPreload;
                    case 161: return ExtraDataType::kUnkB6;
                    case 162: return ExtraDataType::kUnkB7;
                    case 163: return ExtraDataType::kUnkB8;
                    case 164: return ExtraDataType::kUnkB9;
                    case 165: return ExtraDataType::kUnkBA;
                    case 166: return ExtraDataType::kUnkBB;
                    case 167: return ExtraDataType::kUnkBC;
                    case 168: return ExtraDataType::kUnkBD;
                    case 169: return ExtraDataType::kUnkBE;
                    case 170: return ExtraDataType::kUnkBF;
                    default: return ExtraDataType::kNone;
                }
            }
            
            static inline std::string ExtraDataTypeToString(std::int32_t t) {
                switch (t) {
                    case 0: return "kNone";
                    case 1: return "kCell3D";
                    case 2: return "kOpenCloseActivateRef";
                    case 3: return "kAmmo";
                    case 4: return "kPatrolRefData";
                    case 5: return "kPackageData";
                    case 6: return "kOcclusionShape";
                    case 7: return "kCollisionData";
                    case 8: return "kFollower";
                    case 9: return "kLevCreaModifier";
                    case 10: return "kGhost";
                    case 11: return "kOriginalReference";
                    case 12: return "kOwnership";
                    case 13: return "kGlobal";
                    case 14: return "kRank";
                    case 15: return "kCount";
                    case 16: return "kHealth";
                    case 17: return "kUnk26";
                    case 18: return "kTimeLeft";
                    case 19: return "kCharge";
                    case 20: return "kLight";
                    case 21: return "kLock";
                    case 22: return "kTeleport";
                    case 23: return "kMapMarker";
                    case 24: return "kLeveledCreature";
                    case 25: return "kLeveledItem";
                    case 26: return "kScale";
                    case 27: return "kMissingLinkedRefIDs";
                    case 28: return "kMagicCaster";
                    case 29: return "kNonActorMagicTarget";
                    case 30: return "kUnk33";
                    case 31: return "kPlayerCrimeList";
                    case 32: return "kUnk35";
                    case 33: return "kEnableStateParent";
                    case 34: return "kEnableStateChildren";
                    case 35: return "kItemDropper";
                    case 36: return "kDroppedItemList";
                    case 37: return "kRandomTeleportMarker";
                    case 38: return "kUnk3B";
                    case 39: return "kSavedHavokData";
                    case 40: return "kCannotWear";
                    case 41: return "kPoison";
                    case 42: return "kMagicLight";
                    case 43: return "kLastFinishedSequence";
                    case 44: return "kSavedAnimation";
                    case 45: return "kNorthRotation";
                    case 46: return "kSpawnContainer";
                    case 47: return "kFriendHits";
                    case 48: return "kHeadingTarget";
                    case 49: return "kUnk46";
                    case 50: return "kRefractionProperty";
                    case 51: return "kStartingWorldOrCell";
                    case 52: return "kHotkey";
                    case 53: return "kEditorRef3DData";
                    case 54: return "kEditorRefMoveData";
                    case 55: return "kInfoGeneralTopic";
                    case 56: return "kHasNoRumors";
                    case 57: return "kSound";
                    case 58: return "kTerminalState";
                    case 59: return "kLinkedRef";
                    case 60: return "kLinkedRefChildren";
                    case 61: return "kActivateRef";
                    case 62: return "kActivateRefChildren";
                    case 63: return "kCanTalkToPlayer";
                    case 64: return "kObjectHealth";
                    case 65: return "kCellImageSpace";
                    case 66: return "kNavMeshPortal";
                    case 67: return "kModelSwap";
                    case 68: return "kRadius";
                    case 69: return "kUnk5A";
                    case 70: return "kFactionChanges";
                    case 71: return "kDismemberedLimbs";
                    case 72: return "kActorCause";
                    case 73: return "kMultiBound";
                    case 74: return "kMultiBoundMarkerData";
                    case 75: return "kMultiBoundRef";
                    case 76: return "kReflectedRefs";
                    case 77: return "kReflectorRefs";
                    case 78: return "kEmittanceSource";
                    case 79: return "kUnk64";
                    case 80: return "kCombatStyle";
                    case 81: return "kUnk66";
                    case 82: return "kPrimitive";
                    case 83: return "kOpenCloseActivateRef";
                    case 84: return "kAnimNoteReceiver";
                    case 85: return "kAmmo";
                    case 86: return "kPatrolRefData";
                    case 87: return "kPackageData";
                    case 88: return "kOcclusionShape";
                    case 89: return "kCollisionData";
                    case 90: return "kSayTopicInfoOnceADay";
                    case 91: return "kEncounterZone";
                    case 92: return "kSayTopicInfo";
                    case 93: return "kOcclusionPlaneRefData";
                    case 94: return "kPortalRefData";
                    case 95: return "kPortal";
                    case 96: return "kRoom";
                    case 97: return "kHealthPerc";
                    case 98: return "kRoomRefData";
                    case 99: return "kGuardedRefData";
                    case 100: return "kCreatureAwakeSound";
                    case 101: return "kUnk7A";
                    case 102: return "kHorse";
                    case 103: return "kIgnoredBySandbox";
                    case 104: return "kCellAcousticSpace";
                    case 105: return "kReservedMarkers";
                    case 106: return "kWeaponIdleSound";
                    case 107: return "kWaterLightRefs";
                    case 108: return "kLitWaterRefs";
                    case 109: return "kWeaponAttackSound";
                    case 110: return "kActivateLoopSound";
                    case 111: return "kPatrolRefInUseData";
                    case 112: return "kAshPileRef";
                    case 113: return "kCreatureMovementSound";
                    case 114: return "kFollowerSwimBreadcrumbs";
                    case 115: return "kAliasInstanceArray";
                    case 116: return "kLocation";
                    case 117: return "kUnk8A";
                    case 118: return "kLocationRefType";
                    case 119: return "kPromotedRef";
                    case 120: return "kAnimationSequencer";
                    case 121: return "kOutfitItem";
                    case 122: return "kUnk8F";
                    case 123: return "kLeveledItemBase";
                    case 124: return "kLightData";
                    case 125: return "kSceneData";
                    case 126: return "kBadPosition";
                    case 127: return "kHeadTrackingWeight";
                    case 128: return "kFromAlias";
                    case 129: return "kShouldWear";
                    case 130: return "kFavorCost";
                    case 131: return "kAttachedArrows3D";
                    case 132: return "kTextDisplayData";
                    case 133: return "kAlphaCutoff";
                    case 134: return "kEnchantment";
                    case 135: return "kSoul";
                    case 136: return "kForcedTarget";
                    case 137: return "kUnk9E";
                    case 138: return "kUniqueID";
                    case 139: return "kFlags";
                    case 140: return "kRefrPath";
                    case 141: return "kDecalGroup";
                    case 142: return "kLockList";
                    case 143: return "kForcedLandingMarker";
                    case 144: return "kLargeRefOwnerCells";
                    case 145: return "kCellWaterEnvMap";
                    case 146: return "kCellGrassData";
                    case 147: return "kTeleportName";
                    case 148: return "kInteraction";
                    case 149: return "kWaterData";
                    case 150: return "kWaterCurrentZoneData";
                    case 151: return "kAttachRef";
                    case 152: return "kAttachRefChildren";
                    case 153: return "kGroupConstraint";
                    case 154: return "kScriptedAnimDependence";
                    case 155: return "kCachedScale";
                    case 156: return "kRaceData";
                    case 157: return "kGIDBuffer";
                    case 158: return "kMissingRefIDs";
                    case 159: return "kUnkB4";
                    case 160: return "kResourcesPreload";
                    case 161: return "kUnkB6";
                    case 162: return "kUnkB7";
                    case 163: return "kUnkB8";
                    case 164: return "kUnkB9";
                    case 165: return "kUnkBA";
                    case 166: return "kUnkBB";
                    case 167: return "kUnkBC";
                    case 168: return "kUnkBD";
                    case 169: return "kUnkBE";
                    case 170: return "kUnkBF";
                    default: return "kNone";
                }
            }
    };
}