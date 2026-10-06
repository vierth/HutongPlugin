#include "Generation/HutongBuildingComponent.h"
#include "Generation/HutongPlanOutlineComponent.h"
#include "Generation/ScreenWallGenerator.h"
#include "Generation/PathGenerator.h"
#include "Generation/FlowerBedGenerator.h"
#include "Generation/WaterJarGenerator.h"
#include "Generation/PavilionGenerator.h"
#include "Generation/ShopfrontGenerator.h"
#include "Generation/StoreyGenerator.h"
#include "Generation/InnerGateGenerator.h"
#include "Generation/CorridorGenerator.h"
#include "Generation/HutongMeshUtils.h"
#include "Tools/HutongPresets.h"
#include "Engine/StaticMeshActor.h"

using UE::Geometry::FDynamicMesh3;

int32 UHutongBuildingComponent::BuildLODs(TArray<FDynamicMesh3>& OutLODs) const
{
	const FVector2D Footprint = GetFootprintSize();
	// The seam every rebuild shares: generator builds the rectangle, then the corner offsets warp
	// the mesh, before normals and box UVs.
	const bool bSkew = HasFootprintSkew();
	const FHutongFootprintSkew Skew = GetFootprintSkew();
	return HutongGen::Detail::BuildPlacementLODs(bPlanOnly, Footprint.X, Footprint.Y,
		DetailLevel, bBuildLODChain,
		[this, Footprint, bSkew, &Skew](FDynamicMesh3& Mesh, EHutongDetail Level)
		{
			BuildMesh(Mesh, Level);
			if (bSkew) HutongMeshUtils::WarpFootprint(Mesh, Footprint.X, Footprint.Y, Skew);
		},
		OutLODs);
}

FHutongFootprintSkew UHutongBuildingComponent::WithEndBays(FHutongFootprintSkew Skew) const
{
	Skew.EndBayReach[0] = Skew.EndBayReach[1] = 0.0;
	EHutongBaySide Side;
	const FVector2D Size = GetFootprintSize();
	const bool bRunX = HutongFootprint::RunAlongX(Size);
	if (Skew.Mode != EHutongSkewMode::Ends || !GetFacade(Side) || ArePlanBaysAlongX() != bRunX) return Skew;
	FHutongPlanBays Bays;
	GetPlanBays(Bays);
	// One bay has no inner column to stop at.
	if (Bays.Boundaries.Num() < 3) return Skew;
	Bays.Boundaries.Sort();
	const double L = bRunX ? Size.X : Size.Y;
	// Seam on the end-side face of the first inner column, so the column stands as built.
	Skew.EndBayReach[0] = Bays.Boundaries[1] - Bays.ColumnRadius;
	Skew.EndBayReach[1] = L - Bays.Boundaries[Bays.Boundaries.Num() - 2] - Bays.ColumnRadius;
	return Skew;
}

void UHutongBuildingComponent::OnComponentCreated()
{
	Super::OnComponentCreated();
	// Not in the constructor: it also runs for the CDO, giving every building the same id.
	if (!BuildingId.IsValid()) { BuildingId = FGuid::NewGuid(); }
}

bool UHutongBuildingComponent::EnsureBuildingId()
{
	if (BuildingId.IsValid()) return false;
	BuildingId = FGuid::NewGuid();
	return true;
}

void UHutongBuildingComponent::Rebuild()
{
	AStaticMeshActor* Actor = Cast<AStaticMeshActor>(GetOwner());
	if (!Actor) return;

	if (!HasGeometry()) bPlanOnly = true;
	if (bPlanOnly)
	{
		// Plan only: remove the built mesh, add the outline.
		Actor->Modify();
		if (UStaticMeshComponent* SMC = Actor->GetStaticMeshComponent())
		{
			SMC->Modify();
			SMC->SetStaticMesh(nullptr);
		}
		ApplyPlanOutline();
		return;
	}

	TArray<FDynamicMesh3> LODs;
	const int32 CollisionLOD = BuildLODs(LODs);
	if (LODs.Num() == 0 || LODs[0].TriangleCount() == 0) return;

	Actor->Modify();
	HutongGen::BuildAndAssignStaticMesh(Actor, LODs, GetBuiltPalette(), CollisionLOD);
	ApplyPlanOutline();
}

void UHutongBuildingComponent::ApplyDragDerived(const UHutongBuildingComponent& Placed)
{
	// Another type takes its own line; within one, the line it was snapped to (or set by hand) stays.
	if (Placed.GetClass() != GetClass()) return;
	const double Top = Placed.GetBaseCourseTop();
	if (Top > 0.0 && GetBaseCourseTop() > 0.0 && !FMath::IsNearlyEqual(Top, GetBaseCourseTop(), 0.5)) SetBaseCourseTop(Top);
}

bool UHutongBuildingComponent::ApplyPresetParams(const FString& Name)
{
	if (Name.IsEmpty()) return false;
	const UScriptStruct* Type = nullptr;
	void* Data = nullptr;
	UHutongPresetLibrary* Library = UHutongPresetLibrary::Get();
	if (!Library || !GetParamsForPreset(Type, Data)) return false;
	return Library->LoadPreset(GetPresetKey(), Name, Type, Data);
}

FName UHutongBuildingComponent::GetPresetKey() const
{
	// UHutongSiheyuanBuildingComponent -> "Siheyuan", the house tool's preset key.
	FString Name = GetClass()->GetName();
	Name.RemoveFromStart(TEXT("UHutong"));
	Name.RemoveFromStart(TEXT("Hutong"));
	Name.RemoveFromEnd(TEXT("BuildingComponent"));
	return Name.IsEmpty() ? NAME_None : FName(*Name);
}

bool UHutongBuildingComponent::GetParamsForPreset(const UScriptStruct*& OutType, void*& OutData)
{
	FStructProperty* Prop = FindFProperty<FStructProperty>(GetClass(), TEXT("Params"));
	if (!Prop) return false;
	OutType = Prop->Struct;
	OutData = Prop->ContainerPtrToValuePtr<void>(this);
	return true;
}

TArray<FString> UHutongBuildingComponent::GetPresetOptions() const
{
	TArray<FString> Names;
	if (const UHutongPresetLibrary* Library = UHutongPresetLibrary::Get())
	{
		Names = Library->GetPresetNames(GetPresetKey());
	}
	// Empty = parameters match no preset.
	Names.Insert(FString(), 0);
	return Names;
}

bool UHutongBuildingComponent::ArePlanBaysAlongX() const
{
	EHutongBaySide Side = EHutongBaySide::MinusY;
	if (GetFacade(Side)) return HutongGen::BaySide::IsAlongX(Side);
	return !IsRunAlongY();
}

void UHutongBuildingComponent::ApplyPlanOutline()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	// **One outline; extras destroyed here.** Undo can restore an old outline beside a new one;
	// only the first is found again, so the second would draw a stale footprint forever.
	TArray<UHutongPlanOutlineComponent*> Outlines;
	Owner->GetComponents(Outlines);
	for (int32 i = bPlanOnly ? 1 : 0; i < Outlines.Num(); ++i)
	{
		Owner->Modify();
		Outlines[i]->DestroyComponent();
	}

	if (!bPlanOnly)
	{
		return;
	}

	UHutongPlanOutlineComponent* Outline = Outlines.Num() > 0 ? Outlines[0] : nullptr;
	if (!Outline)
	{
		Owner->Modify();
		Outline = NewObject<UHutongPlanOutlineComponent>(Owner, NAME_None, RF_Transactional);
		Outline->SetupAttachment(Owner->GetRootComponent());
		// AddInstanceComponent too, or it is neither saved nor listed on the actor.
		Owner->AddInstanceComponent(Outline);
		Outline->RegisterComponent();
	}
	EHutongBaySide Side = EHutongBaySide::MinusY;
	const bool bHasFacade = GetFacade(Side);
	TArray<FHutongPlanOpening> Openings;
	GetPlanOpenings(Openings);
	TArray<FVector2D> Marks;
	for (const FHutongPlanOpening& O : Openings) Marks.Add(FVector2D(O.Centre, O.Width));
	FHutongPlanBays Bays;
	GetPlanBays(Bays);
	Outline->ArrowMarks.Reset();
	GetPlanArrows(Outline->ArrowMarks);
	Outline->SetPlan(GetFootprintSize(), GetFootprintSkew(), bHasFacade, Side, Marks, IsRunAlongY(),
		Bays, ArePlanBaysAlongX(), GetPlanColour());
}

void UHutongBuildingComponent::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject)) SetFlags(RF_Transactional);
}

#if WITH_EDITOR
void UHutongBuildingComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Skip the stream of updates a slider drag produces.
	if (PropertyChangedEvent.ChangeType == EPropertyChangeType::Interactive) return;

	// A picked preset replaces only the parameters; footprint, facing and transform stay. Its
	// suggested frontage only affects a *new* drag.
	const FName Changed = PropertyChangedEvent.GetPropertyName();
	if (Changed == GET_MEMBER_NAME_CHECKED(UHutongBuildingComponent, Preset) && !Preset.IsEmpty())
	{
		ApplyPresetParams(Preset);
	}

	// A corner past the opposite edge folds the quad and would invert the warp; the panel shows the
	// rectangle built instead.
	{
		if (HasFootprintSkew() && !HutongFootprint::IsSkewValid(GetFootprintSize(), GetFootprintSkew()))
		{
			UE_LOG(LogTemp, Warning, TEXT("Hutong: corner offsets on %s fold the footprint; cleared."),
				*GetPathName());
			FootprintSkew = FHutongFootprintSkew();
		}
	}

	Rebuild();
}

void UHutongBuildingComponent::PostEditUndo()
{
	Super::PostEditUndo();

	// Undoing a conversion also lands here for the removed component, reset to defaults; building
	// from it would bake a default building. Only a component the actor still carries may build.
	const AActor* Owner = GetOwner();
	if (!IsValid(this) || !Owner || !Owner->GetComponents().Contains(this)) return;

	// Undo restores the parameters and the previous mesh reference independently.
	Rebuild();
}
#endif

void UHutongWallBuildingComponent::BuildWallMesh(const FHutongWallParams& InParams, double InLength,
	double InThickness, bool bAlongY, FDynamicMesh3& OutMesh,
	EHutongDetail Detail)
{
	FHutongWallParams P = InParams;
	P.Length = FMath::Max(InLength, 1.0);
	// The footprint caps the thickness.
	P.FootprintThickness = FMath::Max(InThickness, 1.0);

	HutongGen::Detail::Apply(Detail, P);

	// 牆 has no block form: a run is already boxes.
	HutongGen::BuildWall(OutMesh, P);

	// BuildWall lays the length along X.
	if (bAlongY)
	{
		for (int32 vid : OutMesh.VertexIndicesItr())
		{
			const FVector3d V = OutMesh.GetVertex(vid);
			OutMesh.SetVertex(vid, FVector3d(V.Y, V.X, V.Z));
		}
		OutMesh.ReverseOrientation();
	}
}

void UHutongWallBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	FHutongWallParams P = Params;
	P.StartExtend = StartExtend;
	P.EndExtend = EndExtend;
	// Same answer as GetBuiltThickness.
	const double Cross = (FootprintThickness > 0.0) ? FootprintThickness : P.GetThickness();
	BuildWallMesh(P, Length, Cross, bLengthAlongY, OutMesh, Level);
}

void UHutongSiheyuanBuildingComponent::BuildSiheyuanMesh(const FHutongSiheyuanParams& InParams,
	EHutongBaySide Side, int32 InBayCountOverride, double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Detail)
{
	const bool bAlongX = HutongGen::BaySide::IsAlongX(Side);

	FHutongSiheyuanParams P = InParams;
	P.Width = bAlongX ? SizeX : SizeY;
	P.Depth = bAlongX ? SizeY : SizeX;
	P.BayCountOverride = InBayCountOverride;

	HutongGen::Detail::Apply(Detail, P);

	if (HutongGen::Detail::IsMassing(Detail))
	{
		HutongGen::Massing::AppendBlock(OutMesh, HutongGen::Massing::From(P));
	}
	else
	{
		HutongGen::BuildSiheyuan(OutMesh, P);
	}

	// BuildSiheyuan always puts the facade on -Y; rotate the result onto the chosen side.
	if (Side == EHutongBaySide::MinusY) return;

	for (int32 vid : OutMesh.VertexIndicesItr())
	{
		OutMesh.SetVertex(vid,
			HutongGen::BaySide::RotateVertex(Side, OutMesh.GetVertex(vid), SizeX, SizeY));
	}

}

void UHutongSiheyuanBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildSiheyuanMesh(Params, BaySide, BayCountOverride, FootprintX, FootprintY, OutMesh, Level);
}

void UHutongEarPassageBuildingComponent::BuildEarPassageMesh(const FHutongEarPassageParams& InParams,
	EHutongBaySide Side, double SizeX, double SizeY, FDynamicMesh3& OutMesh, EHutongDetail Detail)
{
	const bool bAlongX = HutongGen::BaySide::IsAlongX(Side);
	FHutongEarPassageParams P = InParams;
	P.Width = bAlongX ? SizeX : SizeY;
	P.Depth = bAlongX ? SizeY : SizeX;

	// Detail level applied inside, per part.
	HutongGen::BuildEarPassage(OutMesh, P, Detail);

	if (Side == EHutongBaySide::MinusY) return;
	for (int32 vid : OutMesh.VertexIndicesItr())
	{
		OutMesh.SetVertex(vid, HutongGen::BaySide::RotateVertex(Side, OutMesh.GetVertex(vid), SizeX, SizeY));
	}
}

void UHutongEarPassageBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	FHutongEarPassageParams P = Params;
	P.BayCountOverride = BayCountOverride;
	BuildEarPassageMesh(P, BaySide, FootprintX, FootprintY, OutMesh, Level);
}

void UHutongGateHouseBuildingComponent::BuildGateHouseMesh(
	const FHutongGateHouseParams& InParams, EHutongBaySide Side,
	double SizeX, double SizeY, FDynamicMesh3& OutMesh, EHutongDetail Detail)
{
	// BuildGateHouse always faces -Y, so the facade span is X and the depth is Y.
	FHutongGateHouseParams P = InParams;
	const bool bAlongX = HutongGen::BaySide::IsAlongX(Side);
	P.Width = bAlongX ? SizeX : SizeY;
	P.Depth = bAlongX ? SizeY : SizeX;

	HutongGen::Detail::Apply(Detail, P);

	if (HutongGen::Detail::IsMassing(Detail))
	{
		HutongGen::Massing::AppendBlock(OutMesh, HutongGen::Massing::From(P));
	}
	else
	{
		HutongGen::BuildGateHouse(OutMesh, P);
	}

	if (Side != EHutongBaySide::MinusY)
	{
		for (int32 vid : OutMesh.VertexIndicesItr())
		{
			OutMesh.SetVertex(vid,
				HutongGen::BaySide::RotateVertex(Side, OutMesh.GetVertex(vid), SizeX, SizeY));
		}
	}
}

void UHutongGateHouseBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildGateHouseMesh(Params, BaySide, FootprintX, FootprintY, OutMesh, Level);
}

void UHutongPaifangBuildingComponent::BuildPaifangMesh(
	const FHutongPaifangParams& InParams, double InLength, double InDepth,
	bool bAlongY, FDynamicMesh3& OutMesh, EHutongDetail Detail)
{
	FHutongPaifangParams P = InParams;
	P.Length = InLength;
	P.Depth = InDepth;

	HutongGen::Detail::Apply(Detail, P);

	if (HutongGen::Detail::IsMassing(Detail))
	{
		HutongGen::Massing::AppendBlock(OutMesh, HutongGen::Massing::From(P));
	}
	else
	{
		HutongGen::BuildPaifang(OutMesh, P);
	}

	// BuildPaifang lays the span along X.
	if (bAlongY)
	{
		HutongMeshUtils::YawVerticesFrom(OutMesh, 0,
			FVector2d(0.5 * InDepth, 0.5 * InDepth), 90.0);
	}
}

void UHutongPaifangBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildPaifangMesh(Params, Length, Depth, bLengthAlongY, OutMesh, Level);
}

// --- 影壁 ---

void UHutongScreenWallBuildingComponent::BuildScreenWallMesh(
	const FHutongScreenWallParams& InParams, double InLength, bool bAlongY, FDynamicMesh3& OutMesh,
	EHutongDetail Detail)
{
	FHutongScreenWallParams P = InParams;
	P.Length = InLength;

	HutongGen::Detail::Apply(Detail, P);

	if (HutongGen::Detail::IsMassing(Detail))
	{
		HutongGen::Massing::AppendBlock(OutMesh, HutongGen::Massing::From(P));
	}
	else
	{
		HutongGen::BuildScreenWall(OutMesh, P);
	}

	// Built along X.
	if (bAlongY)
	{
		const double Dep = P.GetFootprintDepth();
		HutongMeshUtils::YawVerticesFrom(OutMesh, 0, FVector2d(0.5 * Dep, 0.5 * Dep), 90.0);
	}
}

void UHutongScreenWallBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildScreenWallMesh(Params, Length, bLengthAlongY, OutMesh, Level);
}

// --- 遊廊 ---

void UHutongCorridorBuildingComponent::BuildCorridorMesh(
	const FHutongCorridorParams& InParams, double InLength, bool bAlongY, bool bFlip,
	FDynamicMesh3& OutMesh, EHutongDetail Detail)
{
	FHutongCorridorParams P = InParams;
	P.Length = InLength;

	// The flip below reverses the run's own X.
	if (bFlip && P.BenchGapAt >= 0.0)
	{
		P.BenchGapAt = 1.0 - FMath::Clamp(P.BenchGapAt, 0.0, 1.0);
	}
	if (bFlip)
	{
		Swap(P.bOmitLowEndPost, P.bOmitHighEndPost);
		Swap(P.bNoBenchAtLowEnd, P.bNoBenchAtHighEnd);
	}

	HutongGen::Detail::Apply(Detail, P);

	if (HutongGen::Detail::IsMassing(Detail))
	{
		HutongGen::Massing::AppendBlock(OutMesh, HutongGen::Massing::From(P));
	}
	else
	{
		HutongGen::BuildCorridor(OutMesh, P);
	}

	const double Dep = P.GetFootprintDepth();

	// Open side flips by a 180° yaw about the centre, not a mirror.
	if (bFlip)
	{
		HutongMeshUtils::YawVerticesFrom(OutMesh, 0,
			FVector2d(0.5 * InLength, 0.5 * Dep), 180.0);
	}

	if (bAlongY)
	{
		HutongMeshUtils::YawVerticesFrom(OutMesh, 0, FVector2d(0.5 * Dep, 0.5 * Dep), 90.0);
	}
}

void UHutongCorridorBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	FHutongCorridorParams P = Params;
	P.Width = Width;
	P.BenchGapAt = BenchGapAt;
	P.bOmitLowEndPost = bOmitLowEndPost;
	P.bOmitHighEndPost = bOmitHighEndPost;
	P.bNoBenchAtLowEnd = bNoBenchAtLowEnd;
	P.bNoBenchAtHighEnd = bNoBenchAtHighEnd;
	BuildCorridorMesh(P, Length, bLengthAlongY, bFlipOpenSide, OutMesh, Level);
}

// --- 垂花門 ---

void UHutongInnerGateBuildingComponent::BuildInnerGateMesh(
	const FHutongInnerGateParams& InParams, EHutongBaySide Side,
	double SizeX, double SizeY, FDynamicMesh3& OutMesh, EHutongDetail Detail)
{
	// Built facing -Y and rotated into the chosen side in place.
	FHutongInnerGateParams P = InParams;
	const bool bAlongX = HutongGen::BaySide::IsAlongX(Side);
	P.Width = bAlongX ? SizeX : SizeY;
	P.Depth = bAlongX ? SizeY : SizeX;

	HutongGen::Detail::Apply(Detail, P);

	if (HutongGen::Detail::IsMassing(Detail))
	{
		HutongGen::Massing::AppendBlock(OutMesh, HutongGen::Massing::From(P));
	}
	else
	{
		HutongGen::BuildInnerGate(OutMesh, P);
	}

	if (Side != EHutongBaySide::MinusY)
	{
		for (int32 vid : OutMesh.VertexIndicesItr())
		{
			OutMesh.SetVertex(vid,
				HutongGen::BaySide::RotateVertex(Side, OutMesh.GetVertex(vid), SizeX, SizeY));
		}
	}
}

void UHutongInnerGateBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
	BuildInnerGateMesh(Params, BaySide,
		bAlongX ? Width : Depth, bAlongX ? Depth : Width, OutMesh, Level);
}

// --- 鋪面房 ---

namespace
{
	struct FShopColours { FColor Wood, Boards, Paint, Plaque; };

	// sRGB. Order matches EHutongShopScheme after Auto.
	const FShopColours ShopSchemeColours[] = {
		{ FColor(150, 38, 28),  FColor(122, 40, 30),  FColor(44, 96, 98),   FColor(28, 26, 25) },   // 朱紅
		{ FColor(44, 84, 58),   FColor(52, 78, 58),   FColor(150, 40, 30),  FColor(34, 56, 96) },   // 綠
		{ FColor(30, 27, 25),   FColor(36, 31, 28),   FColor(196, 156, 64), FColor(30, 27, 25) },   // 黑金
		{ FColor(150, 38, 28),  FColor(118, 86, 58),  FColor(46, 92, 132),  FColor(30, 28, 27) },   // 青綠
		{ FColor(104, 70, 44),  FColor(120, 86, 56),  FColor(88, 62, 42),   FColor(30, 28, 27) },   // 本色
	};
	constexpr int32 ShopSchemeCount = UE_ARRAY_COUNT(ShopSchemeColours);
	static_assert(ShopSchemeCount == (int32)EHutongShopScheme::Natural, "one colour set per scheme");
}

void UHutongShopfrontBuildingComponent::AdjustPalette(FHutongPalette& InOut) const
{
	int32 Index = (int32)Params.Scheme - 1;
	if (Params.Scheme == EHutongShopScheme::Auto)
	{
		Index = BuildingId.IsValid() ? int32(GetTypeHash(BuildingId) % uint32(ShopSchemeCount)) : 0;
	}
	if (Index < 0 || Index >= ShopSchemeCount) return;
	const FShopColours& C = ShopSchemeColours[Index];
	const FHutongPalette Default;
	// A colour chosen by hand stays.
	auto Lay = [](FLinearColor& Slot, const FLinearColor& Was, const FColor& To)
	{
		if (Slot.Equals(Was, 1.0e-4f)) Slot = FLinearColor::FromSRGBColor(To);
	};
	Lay(InOut.Wood, Default.Wood, C.Wood);
	Lay(InOut.DoorPaint, Default.DoorPaint, C.Boards);
	Lay(InOut.Paint, Default.Paint, C.Paint);
	Lay(InOut.Plaque, Default.Plaque, C.Plaque);
}

void UHutongShopfrontBuildingComponent::ApplyPlacementAttachments()
{
	Super::ApplyPlacementAttachments();
	if (!bPlanOnly) HutongGen::AssignPaletteMaterials(Cast<AStaticMeshActor>(GetOwner()), GetBuiltPalette());
}

void UHutongShopfrontBuildingComponent::BuildShopfrontMesh(
	const FHutongShopfrontParams& InParams, EHutongBaySide Side, int32 InBayCountOverride,
	double SizeX, double SizeY, FDynamicMesh3& OutMesh, EHutongDetail Detail)
{
	FHutongShopfrontParams P = InParams;
	const bool bAlongX = HutongGen::BaySide::IsAlongX(Side);
	P.Width = bAlongX ? SizeX : SizeY;
	P.Depth = bAlongX ? SizeY : SizeX;
	P.BayCountOverride = InBayCountOverride;

	HutongGen::Detail::Apply(Detail, P);

	if (HutongGen::Detail::IsMassing(Detail))
	{
		HutongGen::Massing::AppendBlock(OutMesh, HutongGen::Massing::From(P));
	}
	else
	{
		HutongGen::BuildShopfront(OutMesh, P);
	}

	if (Side != EHutongBaySide::MinusY)
	{
		for (int32 vid : OutMesh.VertexIndicesItr())
		{
			OutMesh.SetVertex(vid,
				HutongGen::BaySide::RotateVertex(Side, OutMesh.GetVertex(vid), SizeX, SizeY));
		}
	}
}

void UHutongShopfrontBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildShopfrontMesh(Params, BaySide, BayCountOverride, FootprintX, FootprintY, OutMesh, Level);
}

// --- 樓 ---

void UHutongStoreyBuildingComponent::BuildStoreyMesh(
	const FHutongStoreyParams& InParams, EHutongBaySide Side, int32 InBayCountOverride,
	double SizeX, double SizeY, FDynamicMesh3& OutMesh, EHutongDetail Detail)
{
	FHutongStoreyParams P = InParams;
	const bool bAlongX = HutongGen::BaySide::IsAlongX(Side);
	P.Width = bAlongX ? SizeX : SizeY;
	P.Depth = bAlongX ? SizeY : SizeX;
	P.BayCountOverride = InBayCountOverride;

	HutongGen::Detail::Apply(Detail, P);

	if (HutongGen::Detail::IsMassing(Detail))
	{
		HutongGen::Massing::AppendBlock(OutMesh, HutongGen::Massing::From(P));
	}
	else
	{
		HutongGen::BuildStorey(OutMesh, P);
	}

	if (Side != EHutongBaySide::MinusY)
	{
		for (int32 vid : OutMesh.VertexIndicesItr())
		{
			OutMesh.SetVertex(vid,
				HutongGen::BaySide::RotateVertex(Side, OutMesh.GetVertex(vid), SizeX, SizeY));
		}
	}
}

void UHutongStoreyBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildStoreyMesh(Params, BaySide, BayCountOverride, FootprintX, FootprintY, OutMesh, Level);
}

// --- 亭 ---

void UHutongPavilionBuildingComponent::BuildPavilionMesh(
	const FHutongPavilionParams& InParams, double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Detail)
{
	// No facing side and no rotation.
	FHutongPavilionParams P = InParams;
	P.Width = SizeX;
	P.Depth = SizeY;
	HutongGen::Detail::Apply(Detail, P);

	if (HutongGen::Detail::IsMassing(Detail))
	{
		HutongGen::Massing::AppendBlock(OutMesh, HutongGen::Massing::From(P));
	}
	else
	{
		HutongGen::BuildPavilion(OutMesh, P);
	}
}

void UHutongPavilionBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildPavilionMesh(Params, Width, Depth, OutMesh, Level);
}

// --- 甬路 ---

void UHutongPathBuildingComponent::BuildPathMesh(
	const FHutongPathParams& InParams, double InLength, double InWidth, bool bAlongY,
	FDynamicMesh3& OutMesh, EHutongDetail Detail)
{
	FHutongPathParams P = InParams;
	P.Length = InLength;
	P.Width = InWidth;

	HutongGen::Detail::Apply(Detail, P);

	// 甬路 has no block form: detail level only thins its courses.
	HutongGen::BuildPath(OutMesh, P);

	// Built along X and swung onto Y by a real rotation, not an X/Y swap.
	if (bAlongY)
	{
		const double Dep = P.GetFootprintDepth();
		HutongMeshUtils::YawVerticesFrom(OutMesh, 0, FVector2d(0.5 * Dep, 0.5 * Dep), 90.0);
	}
}

void UHutongPathBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildPathMesh(Params, Length, Width, bLengthAlongY, OutMesh, Level);
}

// --- 殿 ---

void UHutongHallBuildingComponent::BuildHallMesh(
	const FHutongHallParams& InParams, EHutongBaySide Side,
	int32 BayCountOverride, double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Detail)
{
	FHutongHallParams P = InParams;
	const bool bAlongX = HutongGen::BaySide::IsAlongX(Side);
	P.Width = bAlongX ? SizeX : SizeY;
	P.Depth = bAlongX ? SizeY : SizeX;
	P.BayCountOverride = BayCountOverride;

	HutongGen::Detail::Apply(Detail, P);

	if (HutongGen::Detail::IsMassing(Detail))
	{
		HutongGen::Massing::AppendBlock(OutMesh, HutongGen::Massing::From(P));
	}
	else
	{
		HutongGen::BuildHall(OutMesh, P);
	}

	// Built facade on -Y, rotated into place.
	if (Side != EHutongBaySide::MinusY)
	{
		for (int32 vid : OutMesh.VertexIndicesItr())
		{
			OutMesh.SetVertex(vid,
				HutongGen::BaySide::RotateVertex(Side, OutMesh.GetVertex(vid), SizeX, SizeY));
		}
	}
}

void UHutongHallBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildHallMesh(Params, BaySide, BayCountOverride, FootprintX, FootprintY, OutMesh,
		Level);
}

// --- 過道 ---

void UHutongPassageBuildingComponent::BuildPassageMesh(
	const FHutongPassageParams& InParams, double InLength, double InWidth, bool bAlongY,
	FDynamicMesh3& OutMesh, EHutongDetail Detail)
{
	FHutongPassageParams P = InParams;
	P.Length = FMath::Max(InLength, 1.0);
	P.Width = FMath::Max(InWidth, 1.0);
	HutongGen::Detail::Apply(Detail, P);

	if (HutongGen::Detail::IsMassing(Detail))
	{
		HutongGen::Massing::AppendBlock(OutMesh, HutongGen::Massing::From(P));
	}
	else
	{
		HutongGen::BuildPassage(OutMesh, P);
	}

	// Built with the run along X, then swung onto Y.
	if (bAlongY)
	{
		const double Dep = P.GetRoofSpan();
		HutongMeshUtils::YawVerticesFrom(OutMesh, 0, FVector2d(0.5 * Dep, 0.5 * Dep), 90.0);
	}
}

void UHutongPassageBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildPassageMesh(Params, Length, Width, bLengthAlongY, OutMesh, Level);
}

// --- 花池 ---

void UHutongFlowerBedBuildingComponent::BuildFlowerBedMesh(
	const FHutongFlowerBedParams& InParams, double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Detail)
{
	FHutongFlowerBedParams P = InParams;
	P.SizeX = FMath::Max(SizeX, 1.0);
	P.SizeY = FMath::Max(SizeY, 1.0);
	HutongGen::Detail::Apply(Detail, P);
	HutongGen::BuildFlowerBed(OutMesh, P);
}

void UHutongFlowerBedBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildFlowerBedMesh(Params, FootprintX, FootprintY, OutMesh, Level);
}

// --- 城牆 ---

namespace
{
	// The params the generator builds with: it builds the outer face on -Y and the half turn puts it on +Y,
	// which also reverses the run — so the ramp's position and direction are flipped first and stay the
	// leg's own whichever side the battlements are on.
	FHutongCityWallParams CityWallBuiltParams(const FHutongCityWallParams& InParams, double InLength, EHutongBaySide Outer)
	{
		FHutongCityWallParams P = InParams;
		P.Length = FMath::Max(InLength, 10.0);
		if (Outer == EHutongBaySide::PlusY)
		{
			P.bRampRisesTowardStart = !P.bRampRisesTowardStart;
			P.RampPosition = 1.0 - P.RampPosition;
		}
		return P;
	}
}

void UHutongCityWallBuildingComponent::GetPlanArrows(TArray<FVector4>& Out) const
{
	FVector2D C[4], From, To, Gap[2];
	if (!GetRampOutline(Params, Length, OuterSide, C, From, To, Gap)) return;
	// The square stands inside the footprint (the ramp itself is outside it), against the inner edge.
	const double B = Params.GetBaseWidth();
	const double Inset = 0.5 * FMath::Min(300.0, 0.3 * B) + 40.0;
	const double Y = OuterSide == EHutongBaySide::PlusY ? Inset : B - Inset;
	Out.Add(FVector4(0.5 * (From.X + To.X), Y, To.X > From.X ? 1.0 : -1.0, 0.0));
}

bool UHutongCityWallBuildingComponent::GetRampOutline(const FHutongCityWallParams& InParams, double InLength,
	EHutongBaySide Outer, FVector2D OutCorners[4], FVector2D& OutUpFrom, FVector2D& OutUpTo, FVector2D OutGap[2])
{
	const FHutongCityWallParams P = CityWallBuiltParams(InParams, InLength, Outer);
	HutongGen::FCityWallRamp R;
	if (!HutongGen::CityWallRampLayout(P, R)) return false;
	const double B = P.GetBaseWidth();
	const double RW = FMath::Max(P.RampWidth, 150.0);
	const double YGap = B - P.GetBatter() - 0.5 * P.GetInnerParapetThickness();
	// Built frame, then the same half turn as the mesh.
	auto Leg = [&](double X, double Y)
	{
		return Outer == EHutongBaySide::PlusY ? FVector2D(P.Length - X, B - Y) : FVector2D(X, Y);
	};
	OutCorners[0] = Leg(R.XGate, B);
	OutCorners[1] = Leg(R.XTop, B);
	OutCorners[2] = Leg(R.XTop, B + RW);
	OutCorners[3] = Leg(R.XGate, B + RW);
	OutUpFrom = Leg(R.XFoot, B + 0.5 * RW);
	OutUpTo = Leg(R.XLand, B + 0.5 * RW);
	OutGap[0] = Leg(R.XLand, YGap);
	OutGap[1] = Leg(R.XTop - R.Dir * HutongCanon::CityWall::RampParapetThicknessCm, YGap);
	return true;
}

void UHutongCityWallBuildingComponent::BuildCityWallMesh(const FHutongCityWallParams& InParams, double InLength,
	EHutongBaySide Outer, FDynamicMesh3& OutMesh, EHutongDetail Detail)
{
	FHutongCityWallParams P = CityWallBuiltParams(InParams, InLength, Outer);
	HutongGen::BuildCityWall(OutMesh, P, Detail != EHutongDetail::Massing,
		P.bLoopholes && (Detail == EHutongDetail::Near || Detail == EHutongDetail::Hero));
	// Built outer face on -Y; a half turn about the centre, never a mirror, puts it on +Y.
	if (Outer == EHutongBaySide::PlusY)
	{
		HutongMeshUtils::YawVerticesFrom(OutMesh, 0, FVector2d(0.5 * P.Length, 0.5 * P.GetBaseWidth()), 180.0);
	}
}

void UHutongCityWallBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildCityWallMesh(Params, Length, OuterSide, OutMesh, Level);
}

// --- 魚缸 ---

void UHutongWaterJarBuildingComponent::BuildWaterJarMesh(
	const FHutongWaterJarParams& InParams, FDynamicMesh3& OutMesh, EHutongDetail Detail)
{
	FHutongWaterJarParams P = InParams;
	HutongGen::Detail::Apply(Detail, P);
	HutongGen::BuildWaterJar(OutMesh, P);
}

void UHutongWaterJarBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildWaterJarMesh(Params, OutMesh, Level);
}

// --- 構架 ---

void UHutongFrameBuildingComponent::BuildFrameMesh(const FHutongFrameParams& InParams,
	EHutongBaySide Side, int32 InBayCountOverride, double SizeX, double SizeY, FDynamicMesh3& OutMesh,
	EHutongDetail Detail)
{
	const bool bAlongX = HutongGen::BaySide::IsAlongX(Side);
	FHutongFrameParams P = InParams;
	P.House.Width = bAlongX ? SizeX : SizeY;
	P.House.Depth = bAlongX ? SizeY : SizeX;
	P.House.BayCountOverride = InBayCountOverride;
	HutongGen::Detail::Apply(Detail, P);
	HutongGen::BuildFrame(OutMesh, P);

	// Built front on -Y, rotated onto the chosen side.
	if (Side == EHutongBaySide::MinusY) return;
	for (int32 vid : OutMesh.VertexIndicesItr())
	{
		OutMesh.SetVertex(vid, HutongGen::BaySide::RotateVertex(Side, OutMesh.GetVertex(vid), SizeX, SizeY));
	}
}

void UHutongFrameBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildFrameMesh(Params, BaySide, BayCountOverride, FootprintX, FootprintY, OutMesh, Level);
}

EHutongCourtRole UHutongSiheyuanBuildingComponent::InferCourtRole() const
{
	// The preset names the type; a compound's pieces carry none, but their labels do.
	static const TPair<const TCHAR*, EHutongCourtRole> ByPreset[] = {
		{ TEXT("Main Hall"), EHutongCourtRole::MainHall }, { TEXT("Side House"), EHutongCourtRole::SideHouse },
		{ TEXT("Ear Room"), EHutongCourtRole::EarRoom }, { TEXT("Front Row"), EHutongCourtRole::FrontRow },
		{ TEXT("Rear Row"), EHutongCourtRole::RearRow } };
	for (const TPair<const TCHAR*, EHutongCourtRole>& It : ByPreset)
	{
		if (Preset.StartsWith(It.Key)) return It.Value;
	}
	static const TPair<const TCHAR*, EHutongCourtRole> ByLabel[] = {
		{ TEXT("Hutong_Zhengfang"), EHutongCourtRole::MainHall }, { TEXT("Hutong_Xiangfang"), EHutongCourtRole::SideHouse },
		{ TEXT("Hutong_Erfang"), EHutongCourtRole::EarRoom }, { TEXT("Hutong_Daozuofang"), EHutongCourtRole::FrontRow },
		{ TEXT("Hutong_Houzhaofang"), EHutongCourtRole::RearRow } };
	const FString Label = GetOwner() ? GetOwner()->GetActorNameOrLabel() : FString();
	for (const TPair<const TCHAR*, EHutongCourtRole>& It : ByLabel)
	{
		if (Label.StartsWith(It.Key)) return It.Value;
	}
	return EHutongCourtRole::Other;
}

double UHutongGateHouseBuildingComponent::EaveForRidge(double TargetRidge, const FString& PresetName) const
{
	FHutongGateHouseParams P = Params;
	if (!PresetName.IsEmpty() && PresetName != Preset)
	{
		if (UHutongPresetLibrary* Library = UHutongPresetLibrary::Get())
		{
			Library->LoadPreset(GetPresetKey(), PresetName, FHutongGateHouseParams::StaticStruct(), &P);
		}
	}
	return HutongGen::GateRow::EaveForRidge(P, HutongGen::BaySide::IsAlongX(BaySide) ? FootprintY : FootprintX, TargetRidge);
}
