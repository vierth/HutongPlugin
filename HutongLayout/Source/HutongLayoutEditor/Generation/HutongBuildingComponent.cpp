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

void UHutongBuildingComponent::BuildLODs(TArray<FDynamicMesh3>& OutLODs) const
{
	const FVector2D Footprint = GetFootprintSize();
	// The one seam every rebuild shares: the generator builds its rectangle, and the footprint's
	// corner offsets are then laid over the whole mesh, before normals and box UVs are taken.
	const bool bSkew = HasFootprintSkew();
	HutongGen::Detail::BuildPlacementLODs(bPlanOnly, Footprint.X, Footprint.Y,
		DetailLevel, bBuildLODChain,
		[this, Footprint, bSkew](FDynamicMesh3& Mesh, EHutongDetail Level)
		{
			BuildMesh(Mesh, Level);
			// Seen from below, a roof is its rafters and 望板, wood, not tiles.
			HutongMeshUtils::RetagDownwardFaces(Mesh, HutongGen::MatSlot_Roof, HutongGen::MatSlot_Wood);
			if (bSkew) HutongMeshUtils::WarpFootprint(Mesh, Footprint.X, Footprint.Y, FootprintSkew);
		},
		OutLODs);
}

void UHutongBuildingComponent::OnComponentCreated()
{
	Super::OnComponentCreated();
	// Here rather than in the constructor, which also runs for the class default object and would hand every building in the level the same id.
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

	if (bPlanOnly)
	{
		// Back to the plan: whatever was built comes off, and the outline goes on.
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
	BuildLODs(LODs);
	if (LODs.Num() == 0 || LODs[0].TriangleCount() == 0) return;

	Actor->Modify();
	HutongGen::BuildAndAssignStaticMesh(Actor, LODs, Palette);
	ApplyPlanOutline();
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
	// UHutongSiheyuanBuildingComponent -> "Siheyuan", which is the key the house tool saves under.
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
	// Empty is a real answer: parameters that match no preset any more.
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

	// **One outline, and any others are destroyed here.** An undo that restores a destroyed outline
	// onto an actor that has since made itself a new one leaves two, and only the first is ever
	// found again: the second stands there drawing the footprint as it was when it was made,
	// following the actor around with nothing left to update it. Which is what a resize looks like
	// when the old rectangle does not go away.
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
		// AddInstanceComponent as well as RegisterComponent, or it is neither saved with the actor nor listed on it.
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
	Outline->SetPlan(GetFootprintSize(), FootprintSkew, bHasFacade, Side, Marks, IsRunAlongY(),
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

	// A preset picked here replaces the parameters and nothing else: the footprint, the facing and
	// the transform are the placement's, so the polygon stays where it was drawn and builds
	// something else. The preset's own suggested frontage rides along in the params and is only
	// what a *new* drag would snap to.
	const FName Changed = PropertyChangedEvent.GetPropertyName();
	if (Changed == GET_MEMBER_NAME_CHECKED(UHutongBuildingComponent, Preset) && !Preset.IsEmpty())
	{
		ApplyPresetParams(Preset);
	}

	// A corner typed past the opposite edge folds the quadrilateral and the warp would turn the
	// building inside out; the panel then shows the rectangle it builds instead.
	{
		if (HasFootprintSkew() && !HutongFootprint::IsSkewValid(GetFootprintSize(), FootprintSkew))
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

	// An undo that takes a component *out* of existence still arrives here — undoing a conversion
	// un-creates the component it added — and that object is restored to its defaults before it
	// goes. Rebuilding from it bakes a default-parameters building onto the actor, which on a
	// laid-out plan is geometry appearing where the whole point was that there is none. Only the
	// component the actor is still carrying may build.
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

	// 牆 has no block form: a run is already thirty boxes and a block would be the same wall.
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
	// The footprint's own cross extent when there is one; GetBuiltThickness is the same answer.
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

	// The level is applied inside, part by part.
	HutongGen::BuildEarPassage(OutMesh, P, Detail);

	if (Side == EHutongBaySide::MinusY) return;
	for (int32 vid : OutMesh.VertexIndicesItr())
	{
		OutMesh.SetVertex(vid, HutongGen::BaySide::RotateVertex(Side, OutMesh.GetVertex(vid), SizeX, SizeY));
	}
}

void UHutongEarPassageBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildEarPassageMesh(Params, BaySide, FootprintX, FootprintY, OutMesh, Level);
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

	// Which side the colonnade opens onto is a 180 degree yaw about the footprint's centre, not a mirror.
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

	// 甬路 has no block form either: it is a slab with courses across it, and the courses are what the detail level thins.
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

	// Built with the facade on -Y and rotated into place, as every faced type here is.
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

// --- 魚缸 ---

void UHutongWaterJarBuildingComponent::BuildWaterJarMesh(
	const FHutongWaterJarParams& InParams, FDynamicMesh3& OutMesh, EHutongDetail Detail)
{
	// No footprint to thread through.
	FHutongWaterJarParams P = InParams;
	HutongGen::Detail::Apply(Detail, P);
	HutongGen::BuildWaterJar(OutMesh, P);
}

void UHutongWaterJarBuildingComponent::BuildMesh(FDynamicMesh3& OutMesh, EHutongDetail Level) const
{
	BuildWaterJarMesh(Params, OutMesh, Level);
}
