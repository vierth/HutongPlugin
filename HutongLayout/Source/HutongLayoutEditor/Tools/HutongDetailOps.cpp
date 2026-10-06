#include "Tools/HutongDetailOps.h"
#include "Tools/HutongSnap.h"
#include "Generation/BaySide.h"
#include "Tools/HutongPresets.h"

#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongActorSpawn.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/StaticMeshActor.h"
#include "UObject/UObjectIterator.h"
#include "EngineUtils.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Misc/ScopedSlowTask.h"
#include "ScopedTransaction.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionHandle.h"
#include "WorldPartition/WorldPartitionHelpers.h"

#define LOCTEXT_NAMESPACE "HutongDetailOps"

namespace HutongDetailOps
{

TArray<UHutongBuildingComponent*> CollectSelected()
{
	TArray<UHutongBuildingComponent*> Out;
	if (!GEditor) return Out;

	USelection* Selected = GEditor->GetSelectedActors();
	if (!Selected) return Out;

	for (FSelectionIterator It(*Selected); It; ++It)
	{
		AActor* Actor = Cast<AActor>(*It);
		if (!Actor) continue;
		if (UHutongBuildingComponent* B = Actor->FindComponentByClass<UHutongBuildingComponent>())
		{
			Out.Add(B);
		}
	}
	return Out;
}

namespace
{
	TArray<FWorldPartitionReference>& PinnedBuildings()
	{
		static TArray<FWorldPartitionReference> Pinned;
		static const FDelegateHandle Cleanup = FWorldDelegates::OnWorldCleanup.AddLambda(
			[](UWorld*, bool, bool) { Pinned.Empty(); });
		return Pinned;
	}
}

TArray<UHutongBuildingComponent*> CollectAll(UWorld* World)
{
	if (UWorldPartition* Partition = World ? World->GetWorldPartition() : nullptr)
	{
		TArray<FWorldPartitionReference>& Pinned = PinnedBuildings();
		FWorldPartitionHelpers::FForEachActorWithLoadingParams Params;
		Params.ActorClasses = { AStaticMeshActor::StaticClass() };
		FScopedSlowTask Task(0.0f, LOCTEXT("LoadingAll", "Loading every building in the level…"));
		Task.MakeDialog();
		FWorldPartitionHelpers::ForEachActorWithLoading(Partition, [&](const FWorldPartitionActorDescInstance* Desc)
		{
			const AActor* Actor = Desc ? Desc->GetActor() : nullptr;
			if (Actor && Actor->FindComponentByClass<UHutongBuildingComponent>()) Pinned.Emplace(Partition, Desc->GetGuid());
			return true;
		}, Params);
	}
	return CollectLoaded(World);
}

TArray<UHutongBuildingComponent*> CollectLoaded(UWorld* World)
{
	TArray<UHutongBuildingComponent*> Out;
	if (!World) return Out;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (UHutongBuildingComponent* B = It->FindComponentByClass<UHutongBuildingComponent>())
		{
			Out.Add(B);
		}
	}
	return Out;
}

int32 SetLevel(const TArray<UHutongBuildingComponent*>& Buildings, EHutongDetail Level)
{
	// Count first: slow-task total is right, and a no-op run opens no transaction.
	TArray<UHutongBuildingComponent*> Work;
	Work.Reserve(Buildings.Num());
	for (UHutongBuildingComponent* B : Buildings)
	{
		if (B && B->DetailLevel != Level) Work.Add(B);
	}
	if (Work.Num() == 0) return 0;

	const FScopedTransaction Transaction(LOCTEXT("SetDetailLevel", "Set Hutong Detail Level"));

	FScopedSlowTask Task((float)Work.Num(), LOCTEXT("Rebaking", "Rebuilding at the new detail…"));
	Task.MakeDialog();

	for (UHutongBuildingComponent* B : Work)
	{
		const AActor* Owner = B->GetOwner();
		Task.EnterProgressFrame(1.0f,
			FText::FromString(Owner ? Owner->GetActorNameOrLabel() : TEXT("building")));

		B->Modify();
		B->DetailLevel = Level;
		// Same seam as every parameter edit.
		B->Rebuild();

		// Rebuild does not apply attachments (lights, plan outline); compound and gallery call this too.
		B->ApplyPlacementAttachments();
	}
	// Footprints may have changed; no tool is active to invalidate the snap cache.
	HutongSnap::Invalidate();
	return Work.Num();
}

int32 Rebuild(const TArray<UHutongBuildingComponent*>& Buildings)
{
	TArray<UHutongBuildingComponent*> Work;
	Work.Reserve(Buildings.Num());
	for (UHutongBuildingComponent* B : Buildings)
	{
		if (B) Work.Add(B);
	}
	if (Work.Num() == 0) return 0;

	const FScopedTransaction Transaction(LOCTEXT("RebuildBuildings", "Rebuild Hutong Buildings"));

	FScopedSlowTask Task((float)Work.Num(), LOCTEXT("RebuildingAll", "Rebuilding from parameters…"));
	Task.MakeDialog();

	for (UHutongBuildingComponent* B : Work)
	{
		const AActor* Owner = B->GetOwner();
		Task.EnterProgressFrame(1.0f,
			FText::FromString(Owner ? Owner->GetActorNameOrLabel() : TEXT("building")));

		B->Modify();
		B->Rebuild();
		B->ApplyPlacementAttachments();
	}
	// Footprints may have changed; no tool is active to invalidate the snap cache.
	HutongSnap::Invalidate();
	return Work.Num();
}

int32 GeneratePlanned(const TArray<UHutongBuildingComponent*>& Buildings)
{
	TArray<UHutongBuildingComponent*> Work;
	Work.Reserve(Buildings.Num());
	for (UHutongBuildingComponent* B : Buildings)
	{
		if (B && B->bPlanOnly && B->HasGeometry()) Work.Add(B);
	}
	if (Work.Num() == 0) return 0;

	const FScopedTransaction Transaction(LOCTEXT("GenerateGeometry", "Generate Hutong Geometry"));

	FScopedSlowTask Task((float)Work.Num(), LOCTEXT("Generating", "Generating geometry…"));
	Task.MakeDialog();

	for (UHutongBuildingComponent* B : Work)
	{
		const AActor* Owner = B->GetOwner();
		Task.EnterProgressFrame(1.0f,
			FText::FromString(Owner ? Owner->GetActorNameOrLabel() : TEXT("building")));

		B->Modify();
		B->bPlanOnly = false;
		B->Rebuild();
		B->ApplyPlacementAttachments();
	}
	// Footprints may have changed; no tool is active to invalidate the snap cache.
	HutongSnap::Invalidate();
	return Work.Num();
}

int32 RevertToPlan(const TArray<UHutongBuildingComponent*>& Buildings)
{
	TArray<UHutongBuildingComponent*> Work;
	Work.Reserve(Buildings.Num());
	for (UHutongBuildingComponent* B : Buildings)
	{
		if (B && !B->bPlanOnly) Work.Add(B);
	}
	if (Work.Num() == 0) return 0;

	const FScopedTransaction Transaction(LOCTEXT("RevertToPlan", "Revert Hutong Buildings To Layout"));

	FScopedSlowTask Task((float)Work.Num(), LOCTEXT("Reverting", "Reverting to layout…"));
	Task.MakeDialog();

	for (UHutongBuildingComponent* B : Work)
	{
		const AActor* Owner = B->GetOwner();
		Task.EnterProgressFrame(1.0f,
			FText::FromString(Owner ? Owner->GetActorNameOrLabel() : TEXT("building")));

		B->Modify();
		B->bPlanOnly = true;
		B->Rebuild();
		B->ApplyPlacementAttachments();
	}
	HutongSnap::Invalidate();
	return Work.Num();
}

const TArray<FConvertTarget>& ConvertTargets()
{
	// Built once from the component classes: a new type joins automatically.
	static TArray<FConvertTarget> Targets;
	if (Targets.Num() > 0) return Targets;

	TArray<UClass*> Classes;
	GetDerivedClasses(UHutongBuildingComponent::StaticClass(), Classes, /*bRecursive*/ true);
	for (UClass* Class : Classes)
	{
		if (!Class || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)) continue;

		// Only the component knows its variant labels: read them off a throwaway instance.
		UHutongBuildingComponent* Probe = NewObject<UHutongBuildingComponent>(GetTransientPackage(), Class);
		if (!Probe) continue;

		TArray<FName> Variants;
		Probe->GetTypeVariants(Variants);
		if (Variants.Num() == 0) Variants.Add(NAME_None);

		for (const FName& Variant : Variants)
		{
			if (!Variant.IsNone()) Probe->SetTypeVariant(Variant);
			Targets.Add({ Probe->GetTypeLabel().ToString(), Class, Variant });
		}
	}

	// Kin next to each other, so stepping through with T moves between near choices: buildings with
	// rooms, then gates, walls, courtyard pieces, the temple. Variants keep their own order.
	// Family: what a building may turn into from the menu (user, 2026-10-03) — roofed buildings, gates,
	// the temple and the pavilion; walls; ways (corridor, passage, path); furnishings.
	struct FRank { UClass* Class; int32 Group; int32 Family; };
	static const TArray<FRank> Order = {
		{ UHutongSiheyuanBuildingComponent::StaticClass(), 0, 0 }, { UHutongEarPassageBuildingComponent::StaticClass(), 0, 0 },
		{ UHutongShopfrontBuildingComponent::StaticClass(), 0, 0 }, { UHutongStoreyBuildingComponent::StaticClass(), 0, 0 },
		{ UHutongFrameBuildingComponent::StaticClass(), 0, 0 },
		{ UHutongGateHouseBuildingComponent::StaticClass(), 1, 0 }, { UHutongInnerGateBuildingComponent::StaticClass(), 1, 0 },
		{ UHutongPaifangBuildingComponent::StaticClass(), 1, 0 },
		{ UHutongHallBuildingComponent::StaticClass(), 4, 0 }, { UHutongPavilionBuildingComponent::StaticClass(), 4, 0 },
		{ UHutongWallBuildingComponent::StaticClass(), 2, 1 }, { UHutongScreenWallBuildingComponent::StaticClass(), 2, 1 },
		// City scale: a lane wall's footprint would make a sliver of it, so it turns only into Unknown.
		{ UHutongCityWallBuildingComponent::StaticClass(), 2, 4 },
		{ UHutongCorridorBuildingComponent::StaticClass(), 3, 2 }, { UHutongPassageBuildingComponent::StaticClass(), 3, 2 },
		{ UHutongPathBuildingComponent::StaticClass(), 3, 2 },
		{ UHutongFlowerBedBuildingComponent::StaticClass(), 5, 3 }, { UHutongWaterJarBuildingComponent::StaticClass(), 5, 3 },
		{ UHutongUnknownBuildingComponent::StaticClass(), 6, UnknownFamily } };
	auto RankOf = [](const UClass* Class) { return Order.IndexOfByPredicate([Class](const FRank& R) { return R.Class == Class; }); };
	for (FConvertTarget& T : Targets)
	{
		const int32 R = RankOf(T.Class);
		T.Group = R == INDEX_NONE ? 7 : Order[R].Group;
		T.Family = R == INDEX_NONE ? INDEX_NONE : Order[R].Family;
	}
	// A class missing from the list goes last, by label.
	Targets.StableSort([&](const FConvertTarget& A, const FConvertTarget& B)
	{
		const int32 RA = RankOf(A.Class), RB = RankOf(B.Class);
		const int32 KA = RA == INDEX_NONE ? Order.Num() : RA, KB = RB == INDEX_NONE ? Order.Num() : RB;
		if (KA != KB) return KA < KB;
		return KA == Order.Num() && A.Label < B.Label;
	});
	return Targets;
}

FText ConvertGroupName(int32 Group)
{
	switch (Group)
	{
	case 0:  return LOCTEXT("GroupBuildings", "Buildings (房屋)");
	case 1:  return LOCTEXT("GroupGates", "Gates (門)");
	case 2:  return LOCTEXT("GroupWalls", "Walls (牆, 城牆)");
	case 3:  return LOCTEXT("GroupWays", "Walks and Passages (廊, 過道, 甬路)");
	case 4:  return LOCTEXT("GroupTemple", "Temple and Pavilion (殿, 亭)");
	case 5:  return LOCTEXT("GroupFurnishing", "Courtyard Furnishings (花池, 魚缸)");
	case 6:  return LOCTEXT("GroupUnknown", "Unknown (未知)");
	default: return LOCTEXT("GroupOther", "Other");
	}
}

bool CanExchange(const FConvertTarget& From, const FConvertTarget& To)
{
	// A type outside every family turns only into itself.
	// A traced footprint of unknown type may become anything, and anything may go back to it.
	if (From.Family == UnknownFamily || To.Family == UnknownFamily) return true;
	return From.Class == To.Class || (From.Family != INDEX_NONE && From.Family == To.Family);
}

bool CanAllBecome(const TArray<UHutongBuildingComponent*>& Buildings, const FConvertTarget& To)
{
	const TArray<FConvertTarget>& Targets = ConvertTargets();
	return !Buildings.ContainsByPredicate([&](const UHutongBuildingComponent* B)
	{
		const int32 Index = FindConvertTargetIndex(B);
		return Index == INDEX_NONE || !CanExchange(Targets[Index], To);
	});
}

FConvertTarget FindConvertTarget(const FString& Label)
{
	for (const FConvertTarget& Target : ConvertTargets())
	{
		if (Target.Label == Label) return Target;
	}
	return FConvertTarget();
}

int32 FindConvertTargetIndex(const UHutongBuildingComponent* Building)
{
	if (!Building) return INDEX_NONE;
	const TArray<FConvertTarget>& Targets = ConvertTargets();
	// The label names the variant (院牆 or 隔牆); the class alone is the fallback.
	const FString Label = Building->GetTypeLabel().ToString();
	const int32 ByLabel = Targets.IndexOfByPredicate([&](const FConvertTarget& T) { return T.Label == Label; });
	if (ByLabel != INDEX_NONE) return ByLabel;
	return Targets.IndexOfByPredicate([&](const FConvertTarget& T) { return T.Class == Building->GetClass(); });
}

FName PresetKeyOf(const FConvertTarget& Target)
{
	const UHutongBuildingComponent* Default = Target.Class ? Target.Class->GetDefaultObject<UHutongBuildingComponent>() : nullptr;
	return Default ? Default->GetPresetKey() : NAME_None;
}

namespace
{
	// Editable fields that differ, nested structs walked so a field is named rather than its group.
	void CollectDifferences(const UStruct* Struct, const void* Mine, const void* Theirs, const FString& Prefix, TArray<FString>& Out)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			const FProperty* Prop = *It;
			if (!Prop->HasAnyPropertyFlags(CPF_Edit)) continue;
			const void* A = Prop->ContainerPtrToValuePtr<void>(Mine);
			const void* B = Prop->ContainerPtrToValuePtr<void>(Theirs);
			if (Prop->Identical(A, B, PPF_None)) continue;
			const FString Name = Prefix + Prop->GetDisplayNameText().ToString();
			if (const FStructProperty* Inner = CastField<FStructProperty>(Prop))
			{
				const int32 Before = Out.Num();
				CollectDifferences(Inner->Struct, A, B, Name + TEXT(" › "), Out);
				if (Out.Num() > Before) continue;
			}
			Out.Add(Name);
		}
	}
}

UHutongBuildingComponent* MakeAsDrawn(const UHutongBuildingComponent& Placed, UClass* Class, FName Variant,
	const FString& Preset)
{
	UHutongBuildingComponent* Drawn = Class ? NewObject<UHutongBuildingComponent>(
		GetTransientPackage(), Class, NAME_None, RF_Transient) : nullptr;
	if (!Drawn) return nullptr;
	// Order as a conversion: kind, then preset, then what the drag writes.
	if (!Variant.IsNone()) Drawn->SetTypeVariant(Variant);
	if (!Preset.IsEmpty()) Drawn->ApplyPresetParams(Preset);
	Drawn->ApplyDragDerived(Placed);
	return Drawn;
}

namespace
{
	bool CopyParams(const UHutongBuildingComponent& From, UHutongBuildingComponent& To)
	{
		const UScriptStruct* FromType = nullptr;
		const UScriptStruct* ToType = nullptr;
		void* Src = nullptr;
		void* Dst = nullptr;
		if (!const_cast<UHutongBuildingComponent&>(From).GetParamsForPreset(FromType, Src)
			|| !To.GetParamsForPreset(ToType, Dst) || FromType != ToType) return false;
		ToType->CopyScriptStruct(Dst, Src);
		return true;
	}

	int32 PlanBayCount(const UHutongBuildingComponent* Building)
	{
		return Building->GetDrawnBayCount();
	}

	// The bays are what the map attests: the count drawn before stays. Forced stays forced; a derived
	// count is forced only where the new values would derive another.
	void CarryBayCount(UHutongBuildingComponent* Building, int32 DrawnBays, int32 Forced)
	{
		if (DrawnBays < 1 || GetBayCountOverride(Building) == INDEX_NONE) return;
		const int32 Bays = FMath::Min(DrawnBays, Building->GetMaxBayCount());
		SetBayCountOverride(Building, 0);
		if (Forced > 0 || PlanBayCount(Building) != Bays) SetBayCountOverride(Building, Bays);
	}

	FName OwnVariant(const UHutongBuildingComponent* Building)
	{
		const int32 Index = FindConvertTargetIndex(Building);
		return Index != INDEX_NONE ? ConvertTargets()[Index].Variant : NAME_None;
	}
}

TArray<FString> CustomizedFields(const UHutongBuildingComponent* Building)
{
	TArray<FString> Out;
	if (!Building) return Out;
	const UHutongBuildingComponent* Reference = MakeAsDrawn(*Building, Building->GetClass(), OwnVariant(Building), Building->Preset);
	const UScriptStruct* Type = nullptr;
	void* Mine = nullptr;
	void* Theirs = nullptr;
	if (!Reference || !const_cast<UHutongBuildingComponent*>(Building)->GetParamsForPreset(Type, Mine)
		|| !const_cast<UHutongBuildingComponent*>(Reference)->GetParamsForPreset(Type, Theirs)) return Out;
	CollectDifferences(Type, Mine, Theirs, FString(), Out);
	return Out;
}

bool ApplyPresetAsDrawn(UHutongBuildingComponent* Building, const FString& Preset)
{
	if (!Building || !UHutongPresetLibrary::Get()->GetPresetNames(Building->GetPresetKey()).Contains(Preset)) return false;
	const UHutongBuildingComponent* Drawn = MakeAsDrawn(*Building, Building->GetClass(), OwnVariant(Building), Preset);
	if (!Drawn) return false;
	// The outline stays where it was drawn; the new values may read it differently (a corridor's walk).
	const FVector2D Footprint = Building->GetFootprintSize();
	const int32 DrawnBays = PlanBayCount(Building);
	const int32 Forced = GetBayCountOverride(Building);
	if (!CopyParams(*Drawn, *Building)) return false;
	Building->Preset = Preset;
	Building->SetFootprintSize(Footprint);
	CarryBayCount(Building, DrawnBays, Forced);
	return true;
}

namespace
{
	// Hutong_Shopfront_a1b2c3 -> Hutong_Siheyuan_a1b2c3: keep the placement's guid tail, rename the type.
	void RelabelForType(AActor* Actor, const UHutongBuildingComponent* Building)
	{
		if (!Actor || !Building) return;
		const FString Old = Actor->GetActorLabel();
		FString Head, Tail;
		const FString Suffix = Old.Split(TEXT("_"), &Head, &Tail, ESearchCase::IgnoreCase, ESearchDir::FromEnd)
			? Tail : Building->BuildingId.ToString(EGuidFormats::Digits).Left(6);
		Actor->SetActorLabel(FString::Printf(TEXT("Hutong_%s_%s"),
			*Building->GetPresetKey().ToString(), *Suffix));
	}
}

UHutongBuildingComponent* ConvertBuilding(UHutongBuildingComponent* Old,
	const FConvertTarget& Target, const FString& Preset)
{
	if (!Old || !Target.IsValid()) return nullptr;
	AActor* Actor = Old->GetOwner();
	if (!Actor) return nullptr;

	// Only placement state survives a type change; per-type parameters do not.
	const FVector2D Footprint = Old->GetFootprintSize();
	const FHutongFootprintSkew Skew = Old->FootprintSkew;
	const bool bRunAlongY = Old->IsRunAlongY();
	EHutongBaySide Facade = EHutongBaySide::MinusY;
	const bool bHasFacade = Old->GetFacade(Facade);
	const EHutongDetail Level = Old->DetailLevel;
	const bool bPlanOnly = Old->bPlanOnly;
	const bool bBespoke = Old->bBespokeMesh;
	const bool bLODs = Old->bBuildLODChain;
	const FHutongPalette Palette = Old->Palette;
	const FGuid Id = Old->BuildingId;
	// Read before the old one goes: the new type's values as a drag over this footprint gives them.
	const UHutongBuildingComponent* Drawn = MakeAsDrawn(*Old, Target.Class, Target.Variant, Preset);
	const int32 DrawnBays = PlanBayCount(Old);
	const int32 Forced = GetBayCountOverride(Old);

	Actor->Modify();
	Old->Modify();
	Old->DestroyComponent();

	UHutongBuildingComponent* New = NewObject<UHutongBuildingComponent>(
		Actor, Target.Class, NAME_None, RF_Transactional);

	// Same id: a conversion is not a new placement; earlier exports still name it.
	New->BuildingId = Id;
	New->Palette = Palette;
	New->DetailLevel = Level;
	New->bPlanOnly = bPlanOnly || !New->HasGeometry();
	New->bBespokeMesh = bBespoke;
	New->bBuildLODChain = bLODs;

	// Order: kind, preset, footprint. Wall thickness is capped per role, so the role must be set first.
	if (!Target.Variant.IsNone()) New->SetTypeVariant(Target.Variant);
	if (!Preset.IsEmpty() && New->ApplyPresetParams(Preset)) New->Preset = Preset;
	if (Drawn) CopyParams(*Drawn, *New);

	New->SetRunAlongY(bRunAlongY);
	New->SetFootprintSize(Footprint);
	if (bHasFacade) New->SetFacade(Facade);
	// Corner skew is placement state and carries over.
	New->FootprintSkew = Skew;
	CarryBayCount(New, DrawnBays, Forced);

	Actor->AddInstanceComponent(New);
	New->RegisterComponent();

	RelabelForType(Actor, New);

	New->Rebuild();
	New->ApplyPlacementAttachments();
	return New;
}

void RebuildEdited(UHutongBuildingComponent* Building)
{
	Building->Rebuild();
	Building->ApplyPlacementAttachments();
}

TArray<UHutongBuildingComponent*> NeedingChange(const TArray<UHutongBuildingComponent*>& Buildings,
	int32 TypeIndex, const FString& Preset)
{
	// One already of the chosen type and preset loses nothing and is left alone.
	TArray<UHutongBuildingComponent*> Out;
	for (UHutongBuildingComponent* B : Buildings)
	{
		const bool bSameType = TypeIndex == INDEX_NONE || TypeIndex == FindConvertTargetIndex(B);
		if (!bSameType || B->Preset != Preset) Out.Add(B);
	}
	return Out;
}

TArray<FString> CustomizedAcross(const TArray<UHutongBuildingComponent*>& Buildings)
{
	TArray<FString> Out;
	for (const UHutongBuildingComponent* B : Buildings)
	{
		for (const FString& Field : CustomizedFields(B)) Out.AddUnique(Field);
	}
	return Out;
}

int32 ChangeTypeOrPreset(const TArray<UHutongBuildingComponent*>& Buildings, int32 TypeIndex, const FString& Preset)
{
	const TArray<FConvertTarget>& Targets = ConvertTargets();
	const FConvertTarget Target = Targets.IsValidIndex(TypeIndex) ? Targets[TypeIndex] : FConvertTarget();
	int32 Changed = 0;
	bool bConverted = false;
	{
		const FScopedTransaction Transaction(LOCTEXT("ChangeTypeOrPreset", "Change Building Type or Preset"));
		for (UHutongBuildingComponent* B : NeedingChange(Buildings, TypeIndex, Preset))
		{
			if (Target.IsValid() && TypeIndex != FindConvertTargetIndex(B))
			{
				// The keys and the menu change a building only within its family; the Scene tab converts freely.
				if (!CanAllBecome({ B }, Target)) continue;
				if (ConvertBuilding(B, Target, Preset) == nullptr) continue;
				bConverted = true;
				++Changed;
				continue;
			}
			if (B->GetOwner()) B->GetOwner()->Modify();
			B->Modify();
			// As if drawn here with the new preset: the drag's own values come out the same.
			if (!ApplyPresetAsDrawn(B, Preset)) continue;
			RebuildEdited(B);
			++Changed;
		}
	}
	// Footprints may have changed with the type.
	if (bConverted) HutongSnap::Invalidate();
	return Changed;
}

bool AdjustBays(const TArray<UHutongBuildingComponent*>& Buildings, int32 Delta)
{
	TArray<UHutongBuildingComponent*> Work = Buildings;
	Work.RemoveAll([](const UHutongBuildingComponent* B) { return GetBayCountOverride(B) == INDEX_NONE; });
	if (Work.Num() == 0 || Delta == 0) return false;

	const FScopedTransaction Transaction(LOCTEXT("AdjustBays", "Change Bay Count"));
	for (UHutongBuildingComponent* B : Work)
	{
		// A derived count is seeded from the bays drawn, so the first press steps from what is seen.
		const int32 Forced = GetBayCountOverride(B);
		const int32 Current = Forced > 0 ? Forced : FMath::Max(1, B->GetDrawnBayCount());
		const int32 Next = FMath::Clamp(Current + Delta, 1, B->GetMaxBayCount());
		if (Next == Current) continue;
		if (B->GetOwner()) B->GetOwner()->Modify();
		B->Modify();
		SetBayCountOverride(B, Next);
		RebuildEdited(B);
	}
	return true;
}

bool SetFacing(const TArray<UHutongBuildingComponent*>& Buildings, const FText& Title,
	TFunctionRef<EHutongBaySide(const UHutongBuildingComponent&, EHutongBaySide)> NextSide)
{
	TArray<UHutongBuildingComponent*> Work;
	for (UHutongBuildingComponent* B : Buildings)
	{
		EHutongBaySide Side;
		if (B && B->GetFacade(Side)) Work.Add(B);
	}
	if (Work.Num() == 0) return false;

	const FScopedTransaction Transaction(Title);
	for (UHutongBuildingComponent* B : Work)
	{
		EHutongBaySide Side = EHutongBaySide::MinusY;
		B->GetFacade(Side);
		if (B->GetOwner()) B->GetOwner()->Modify();
		B->Modify();
		if (!B->SetFacade(NextSide(*B, Side))) continue;
		RebuildEdited(B);
	}
	return true;
}

bool TurnFacing(const TArray<UHutongBuildingComponent*>& Buildings, int32 Delta)
{
	if (Delta == 0) return false;
	// Sides in turning order (−Y, +X, +Y, −X): a step is an addition.
	return SetFacing(Buildings, LOCTEXT("TurnFacing", "Turn Building Facade"),
		[Delta](const UHutongBuildingComponent& B, EHutongBaySide Side)
		{
			return (EHutongBaySide)((((int32)Side + Delta * B.FacadeTurnStep()) % 4 + 4) % 4);
		});
}

bool FlipFacing(const TArray<UHutongBuildingComponent*>& Buildings)
{
	return SetFacing(Buildings, LOCTEXT("FlipFacing", "Flip Building Facade"),
		[](const UHutongBuildingComponent&, EHutongBaySide Side) { return HutongGen::BaySide::Opposite(Side); });
}

bool ToggleGate(const TArray<UHutongBuildingComponent*>& Buildings)
{
	TArray<UHutongWallBuildingComponent*> Walls;
	for (UHutongBuildingComponent* B : Buildings)
	{
		if (UHutongWallBuildingComponent* W = Cast<UHutongWallBuildingComponent>(B)) Walls.Add(W);
	}
	if (Walls.Num() == 0) return false;

	const FScopedTransaction Transaction(LOCTEXT("ToggleGate", "Toggle Wall Gate"));
	for (UHutongWallBuildingComponent* W : Walls)
	{
		if (W->GetOwner()) W->GetOwner()->Modify();
		W->Modify();
		// Any opening off; none, a gate (牆垣門) on.
		const bool bOpen = W->Params.bHasGate || W->Params.Doorway != EHutongWallDoorway::None;
		W->Params.bHasGate = !bOpen;
		if (bOpen) W->Params.Doorway = EHutongWallDoorway::None;
		RebuildEdited(W);
	}
	return true;
}

int32 Convert(const TArray<UHutongBuildingComponent*>& Buildings, const FConvertTarget& Target,
	const FString& Preset)
{
	if (!Target.IsValid()) return 0;

	TArray<UHutongBuildingComponent*> Work;
	TArray<UHutongBuildingComponent*> PresetOnly;
	Work.Reserve(Buildings.Num());
	for (UHutongBuildingComponent* B : Buildings)
	{
		if (!B || !B->GetOwner()) continue;
		// Already this type and variant: only a new preset to load (rebuild is a separate button).
		if (B->GetClass() == Target.Class && B->GetTypeVariant() == Target.Variant)
		{
			if (!Preset.IsEmpty() && B->Preset != Preset) PresetOnly.Add(B);
			continue;
		}
		Work.Add(B);
	}
	if (Work.Num() == 0 && PresetOnly.Num() == 0) return 0;

	const FScopedTransaction Transaction(LOCTEXT("ConvertBuildings", "Convert Hutong Buildings"));

	FScopedSlowTask Task((float)Work.Num(), LOCTEXT("Converting", "Converting…"));
	Task.MakeDialog();

	int32 Converted = 0;
	for (UHutongBuildingComponent* Old : Work)
	{
		Task.EnterProgressFrame(1.0f, FText::FromString(Old->GetOwner()->GetActorNameOrLabel()));
		if (ConvertBuilding(Old, Target, Preset)) ++Converted;
	}
	for (UHutongBuildingComponent* B : PresetOnly)
	{
		B->GetOwner()->Modify();
		B->Modify();
		if (!ApplyPresetAsDrawn(B, Preset)) continue;
		RebuildEdited(B);
		++Converted;
	}
	// Footprints may have changed; no tool is active to invalidate the snap cache.
	HutongSnap::Invalidate();
	return Converted;
}

namespace
{
	// Bay line positions along the run, actor-local, sorted.
	TArray<double> BayLines(const UHutongBuildingComponent* B)
	{
		FHutongPlanBays Bays;
		B->GetPlanBays(Bays);
		Bays.Boundaries.Sort();
		return Bays.Boundaries;
	}

	// Skew kept by one end after a cut: its two corners stay, the cut corners zero.
	// bAlongX = run axis; bStart = the end at the origin.
	FHutongFootprintSkew EndSkew(const FHutongFootprintSkew& Skew, bool bAlongX, bool bStart)
	{
		FHutongFootprintSkew Out;
		Out.Mode = Skew.Mode;
		int32 A, B;
		HutongFootprint::EndCorners(bAlongX ? FVector2D(2.0, 1.0) : FVector2D(1.0, 2.0), bStart, A, B);
		Out.Set(A, Skew.Get(A));
		Out.Set(B, Skew.Get(B));
		return Out;
	}

	// Either corner at this end is skewed.
	bool EndHasSkew(const FHutongFootprintSkew& Skew, bool bAlongX, bool bStart)
	{
		int32 A, B;
		HutongFootprint::EndCorners(bAlongX ? FVector2D(2.0, 1.0) : FVector2D(1.0, 2.0), bStart, A, B);
		return !Skew.Get(A).IsNearlyZero() || !Skew.Get(B).IsNearlyZero();
	}

	// Hand-placed joins are inexact.
	constexpr double JoinToleranceCm = 5.0;
	constexpr double JoinToleranceDeg = 0.5;
}

int32 GetBayCountOverride(const UHutongBuildingComponent* Building)
{
	if (!Building) return INDEX_NONE;
	const FIntProperty* P = CastField<FIntProperty>(Building->GetClass()->FindPropertyByName(TEXT("BayCountOverride")));
	return P ? P->GetPropertyValue_InContainer(Building) : INDEX_NONE;
}

bool SetBayCountOverride(UHutongBuildingComponent* Building, int32 Count)
{
	if (!Building) return false;
	const FIntProperty* P = CastField<FIntProperty>(Building->GetClass()->FindPropertyByName(TEXT("BayCountOverride")));
	if (!P) return false;
	P->SetPropertyValue_InContainer(Building, FMath::Max(Count, 0));
	return true;
}

bool CanDivide(const UHutongBuildingComponent* Building)
{
	if (!Building || !Building->GetOwner() || GetBayCountOverride(Building) == INDEX_NONE || !Building->CanDivideOrFuse()) return false;
	return BayLines(Building).Num() >= 3;
}

UHutongBuildingComponent* DivideBuilding(UHutongBuildingComponent* Building, int32 BayLine, FText& OutWhyNot)
{
	AActor* Owner = Building ? Building->GetOwner() : nullptr;
	UWorld* World = Owner ? Owner->GetWorld() : nullptr;
	if (!World || !CanDivide(Building))
	{
		OutWhyNot = LOCTEXT("DivideNoBays", "This building has no bay lines to divide at.");
		return nullptr;
	}

	const TArray<double> Lines = BayLines(Building);
	const int32 Bays = Lines.Num() - 1;
	if (BayLine < 1 || BayLine >= Bays)
	{
		OutWhyNot = LOCTEXT("DivideNoSuchLine", "Pick a bay line inside the building, not its end.");
		return nullptr;
	}

	const bool bAlongX = Building->ArePlanBaysAlongX();
	const FVector2D Size = Building->GetFootprintSize();
	const double Run = bAlongX ? Size.X : Size.Y;
	const double Cut = Lines[BayLine];
	if (Cut <= 10.0 || Run - Cut <= 10.0)
	{
		OutWhyNot = LOCTEXT("DivideTooThin", "The cut would leave a piece too thin to stand.");
		return nullptr;
	}
	if (Building->FootprintSkew.Mode == EHutongSkewMode::Whole && Building->HasFootprintSkew())
	{
		OutWhyNot = LOCTEXT("DivideWholeSkew", "A footprint angled as a whole cannot be divided; set its corner offsets to Ends only first.");
		return nullptr;
	}

	// Second piece starts at the cut, same line and facing, keeps the far end's corners.
	const FTransform Xf = Owner->GetActorTransform();
	FTransform NewXf = Xf;
	NewXf.SetLocation(Xf.TransformPosition(bAlongX ? FVector(Cut, 0.0, 0.0) : FVector(0.0, Cut, 0.0)));
	const FVector2D KeptSize = bAlongX ? FVector2D(Cut, Size.Y) : FVector2D(Size.X, Cut);
	const FVector2D NewSize = bAlongX ? FVector2D(Run - Cut, Size.Y) : FVector2D(Size.X, Run - Cut);

	if (ULevel* Level = Owner->GetLevel()) Level->Modify();
	const FString Label = Owner->GetActorLabel();
	FString Head, Tail;
	const FString NameBase = Label.Split(TEXT("_"), &Head, &Tail, ESearchCase::IgnoreCase, ESearchDir::FromEnd)
		? Head : Label;
	AStaticMeshActor* NewActor = HutongGen::SpawnEmptyActor(World, NewXf, NameBase);
	if (!NewActor)
	{
		OutWhyNot = LOCTEXT("DivideSpawnFailed", "Could not place the second building.");
		return nullptr;
	}
	NewActor->SetFolderPath(Owner->GetFolderPath());

	// Duplicate keeps type and parameters with no per-type code; placement state is overwritten after.
	UHutongBuildingComponent* New = DuplicateObject<UHutongBuildingComponent>(Building, NewActor);
	New->SetFlags(RF_Transactional);
	New->BuildingId = FGuid::NewGuid();
	New->SetFootprintSize(NewSize);
	New->FootprintSkew = EndSkew(Building->FootprintSkew, bAlongX, /*bStart*/ false);
	SetBayCountOverride(New, Bays - BayLine);
	NewActor->AddInstanceComponent(New);
	New->RegisterComponent();

	Owner->Modify();
	Building->Modify();
	Building->SetFootprintSize(KeptSize);
	Building->FootprintSkew = EndSkew(Building->FootprintSkew, bAlongX, /*bStart*/ true);
	SetBayCountOverride(Building, BayLine);

	Building->Rebuild();
	Building->ApplyPlacementAttachments();
	New->Rebuild();
	New->ApplyPlacementAttachments();

	HutongSnap::Invalidate();
	return New;
}

bool CanFuse(const UHutongBuildingComponent* Building, const UHutongBuildingComponent* Other,
	bool& bOutAtEnd, FText* OutWhyNot)
{
	auto Refuse = [&](const FText& Why) { if (OutWhyNot) *OutWhyNot = Why; return false; };
	const AActor* Owner = Building ? Building->GetOwner() : nullptr;
	const AActor* OtherOwner = Other ? Other->GetOwner() : nullptr;
	if (!Owner || !OtherOwner || Building == Other) return Refuse(LOCTEXT("FuseNeedsTwo", "Two different buildings are needed."));
	if (Building->GetClass() != Other->GetClass()) return Refuse(LOCTEXT("FuseTypes", "Only buildings of the same type fuse."));
	if (GetBayCountOverride(Building) == INDEX_NONE || !Building->CanDivideOrFuse()) return Refuse(LOCTEXT("FuseNoBays", "This type has no bays to fuse."));

	EHutongBaySide SideA, SideB;
	if (!Building->GetFacade(SideA) || !Other->GetFacade(SideB) || SideA != SideB)
	{
		return Refuse(LOCTEXT("FuseFacing", "The two face different ways."));
	}
	if (Building->FootprintSkew.Mode == EHutongSkewMode::Whole && Building->HasFootprintSkew())
	{
		return Refuse(LOCTEXT("FuseWholeSkew", "A footprint angled as a whole cannot be fused."));
	}
	if (Other->FootprintSkew.Mode == EHutongSkewMode::Whole && Other->HasFootprintSkew())
	{
		return Refuse(LOCTEXT("FuseWholeSkewOther", "The neighbour's footprint is angled as a whole."));
	}

	const FTransform Xf = Owner->GetActorTransform();
	const FTransform OtherXf = OtherOwner->GetActorTransform();
	const double YawDelta = FMath::FindDeltaAngleDegrees(Xf.Rotator().Yaw, OtherXf.Rotator().Yaw);
	if (FMath::Abs(YawDelta) > JoinToleranceDeg) return Refuse(LOCTEXT("FuseAngle", "The two are not on one line; fusing at an angle is not supported yet."));
	if (FMath::Abs(Xf.GetLocation().Z - OtherXf.GetLocation().Z) > JoinToleranceCm) return Refuse(LOCTEXT("FuseHeight", "The two stand at different heights."));

	const bool bAlongX = Building->ArePlanBaysAlongX();
	const FVector2D Size = Building->GetFootprintSize();
	const FVector2D OtherSize = Other->GetFootprintSize();
	const double Depth = bAlongX ? Size.Y : Size.X;
	const double OtherDepth = bAlongX ? OtherSize.Y : OtherSize.X;
	if (FMath::Abs(Depth - OtherDepth) > JoinToleranceCm) return Refuse(LOCTEXT("FuseDepth", "The two are not the same depth."));

	const double Run = bAlongX ? Size.X : Size.Y;
	const double OtherRun = bAlongX ? OtherSize.X : OtherSize.Y;
	const FVector OtherLocal = Xf.InverseTransformPosition(OtherXf.GetLocation());
	const double Along = bAlongX ? OtherLocal.X : OtherLocal.Y;
	const double Across = bAlongX ? OtherLocal.Y : OtherLocal.X;
	if (FMath::Abs(Across) > JoinToleranceCm) return Refuse(LOCTEXT("FuseOffLine", "The two are not on one line."));

	if (FMath::Abs(Along - Run) <= JoinToleranceCm) bOutAtEnd = true;
	else if (FMath::Abs(Along + OtherRun) <= JoinToleranceCm) bOutAtEnd = false;
	else return Refuse(LOCTEXT("FuseApart", "The two do not stand end to end."));

	// Join corners must be square: a skewed end has nothing to meet.
	if (EndHasSkew(Building->FootprintSkew, bAlongX, !bOutAtEnd)
		|| EndHasSkew(Other->FootprintSkew, bAlongX, bOutAtEnd))
	{
		return Refuse(LOCTEXT("FuseJoinSkew", "The corners at the join are angled; square them first."));
	}
	return true;
}

bool FuseBuildings(UHutongBuildingComponent* Building, UHutongBuildingComponent* Other, FText& OutWhyNot)
{
	bool bAtEnd = false;
	if (!CanFuse(Building, Other, bAtEnd, &OutWhyNot)) return false;

	AActor* Owner = Building->GetOwner();
	AActor* OtherOwner = Other->GetOwner();
	UWorld* World = Owner->GetWorld();

	const bool bAlongX = Building->ArePlanBaysAlongX();
	const FVector2D Size = Building->GetFootprintSize();
	const FVector2D OtherSize = Other->GetFootprintSize();
	const double Run = bAlongX ? Size.X : Size.Y;
	const double OtherRun = bAlongX ? OtherSize.X : OtherSize.Y;

	// Fused bay count = sum of both sides' bays.
	const int32 BaysA = FMath::Max(BayLines(Building).Num() - 1, 1);
	const int32 BaysB = FMath::Max(BayLines(Other).Num() - 1, 1);

	const FTransform Xf = Owner->GetActorTransform();
	FTransform Fused = Xf;
	if (!bAtEnd)
	{
		// Neighbour comes first: fused building starts at its origin, on this one's line.
		Fused.SetLocation(Xf.TransformPosition(bAlongX ? FVector(-OtherRun, 0.0, 0.0) : FVector(0.0, -OtherRun, 0.0)));
	}
	const FVector2D FusedSize = bAlongX ? FVector2D(Run + OtherRun, Size.Y) : FVector2D(Size.X, Run + OtherRun);

	// Outer ends keep their skew; the join is square (checked above).
	const FHutongFootprintSkew& StartSkew = bAtEnd ? Building->FootprintSkew : Other->FootprintSkew;
	const FHutongFootprintSkew& EndSkewSrc = bAtEnd ? Other->FootprintSkew : Building->FootprintSkew;
	FHutongFootprintSkew Skew = EndSkew(StartSkew, bAlongX, /*bStart*/ true);
	const FHutongFootprintSkew Far = EndSkew(EndSkewSrc, bAlongX, /*bStart*/ false);
	int32 A, B;
	HutongFootprint::EndCorners(bAlongX ? FVector2D(2.0, 1.0) : FVector2D(1.0, 2.0), false, A, B);
	Skew.Set(A, Far.Get(A));
	Skew.Set(B, Far.Get(B));

	World->Modify();
	Owner->Modify();
	Building->Modify();
	Owner->SetActorTransform(Fused);
	Building->SetFootprintSize(FusedSize);
	Building->FootprintSkew = Skew;
	SetBayCountOverride(Building, BaysA + BaysB);

	OtherOwner->Modify();
	OtherOwner->Destroy();

	Building->Rebuild();
	Building->ApplyPlacementAttachments();

	HutongSnap::Invalidate();
	return true;
}

bool FuseSelected(FText& OutMessage)
{
	const TArray<UHutongBuildingComponent*> Picked = CollectSelected();
	if (Picked.Num() != 2)
	{
		OutMessage = LOCTEXT("FuseSelectTwo", "Select exactly two buildings to fuse.");
		return false;
	}
	const FScopedTransaction Transaction(LOCTEXT("FuseBuildings", "Fuse Hutong Buildings"));
	if (!FuseBuildings(Picked[0], Picked[1], OutMessage)) return false;
	OutMessage = LOCTEXT("FuseDone", "Fused into one building.");
	return true;
}

} // namespace HutongDetailOps

#undef LOCTEXT_NAMESPACE
