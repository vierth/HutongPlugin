#include "Tools/HutongDetailOps.h"
#include "Tools/HutongSnap.h"

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
		if (B && B->bPlanOnly) Work.Add(B);
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

	Targets.Sort([](const FConvertTarget& A, const FConvertTarget& B) { return A.Label < B.Label; });
	return Targets;
}

FConvertTarget FindConvertTarget(const FString& Label)
{
	for (const FConvertTarget& Target : ConvertTargets())
	{
		if (Target.Label == Label) return Target;
	}
	return FConvertTarget();
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

	Actor->Modify();
	Old->Modify();
	Old->DestroyComponent();

	UHutongBuildingComponent* New = NewObject<UHutongBuildingComponent>(
		Actor, Target.Class, NAME_None, RF_Transactional);

	// Same id: a conversion is not a new placement; earlier exports still name it.
	New->BuildingId = Id;
	New->Palette = Palette;
	New->DetailLevel = Level;
	New->bPlanOnly = bPlanOnly;
	New->bBespokeMesh = bBespoke;
	New->bBuildLODChain = bLODs;

	// Order: kind, preset, footprint. Wall thickness is capped per role, so the role must be set first.
	if (!Target.Variant.IsNone()) New->SetTypeVariant(Target.Variant);
	if (!Preset.IsEmpty() && New->ApplyPresetParams(Preset)) New->Preset = Preset;

	New->SetRunAlongY(bRunAlongY);
	New->SetFootprintSize(Footprint);
	if (bHasFacade) New->SetFacade(Facade);
	// Corner skew is placement state and carries over.
	New->FootprintSkew = Skew;

	Actor->AddInstanceComponent(New);
	New->RegisterComponent();

	RelabelForType(Actor, New);

	New->Rebuild();
	New->ApplyPlacementAttachments();
	return New;
}

int32 Convert(const TArray<UHutongBuildingComponent*>& Buildings, const FConvertTarget& Target,
	const FString& Preset)
{
	if (!Target.IsValid()) return 0;

	TArray<UHutongBuildingComponent*> Work;
	Work.Reserve(Buildings.Num());
	for (UHutongBuildingComponent* B : Buildings)
	{
		if (!B || !B->GetOwner()) continue;
		// Already this type and variant: nothing to convert (rebuild is a separate button).
		if (B->GetClass() == Target.Class && B->GetTypeVariant() == Target.Variant) continue;
		Work.Add(B);
	}
	if (Work.Num() == 0) return 0;

	const FScopedTransaction Transaction(LOCTEXT("ConvertBuildings", "Convert Hutong Buildings"));

	FScopedSlowTask Task((float)Work.Num(), LOCTEXT("Converting", "Converting…"));
	Task.MakeDialog();

	int32 Converted = 0;
	for (UHutongBuildingComponent* Old : Work)
	{
		Task.EnterProgressFrame(1.0f, FText::FromString(Old->GetOwner()->GetActorNameOrLabel()));
		if (ConvertBuilding(Old, Target, Preset)) ++Converted;
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
	if (!Building || !Building->GetOwner() || GetBayCountOverride(Building) == INDEX_NONE) return false;
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
	if (GetBayCountOverride(Building) == INDEX_NONE) return Refuse(LOCTEXT("FuseNoBays", "This type has no bays to fuse."));

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
