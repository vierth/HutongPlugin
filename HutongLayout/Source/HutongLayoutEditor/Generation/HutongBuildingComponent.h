#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongActorSpawn.h"
#include "Generation/BaySide.h"
#include "Generation/HutongPlanOutlineComponent.h"
#include "Generation/WallGenerator.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/GateHouseGenerator.h"
#include "Generation/HallGenerator.h"
#include "Generation/CorridorGenerator.h"
#include "Generation/InnerGateGenerator.h"
#include "Generation/PaifangGenerator.h"
#include "Generation/PathGenerator.h"
#include "Generation/PassageGenerator.h"
#include "Generation/FlowerBedGenerator.h"
#include "Generation/WaterJarGenerator.h"
#include "Generation/FrameGenerator.h"
#include "Generation/PavilionGenerator.h"
#include "Generation/ScreenWallGenerator.h"
#include "Generation/ShopfrontGenerator.h"
#include "Generation/StoreyGenerator.h"
#include "Generation/EarPassageGenerator.h"
#include "Generation/CityWallGenerator.h"
#include "Generation/ArrowTowerGenerator.h"
#include "Generation/CityGateGenerator.h"
#include "Generation/SmallBuildingGenerator.h"
#include "Generation/HutongDetail.h"
#include "Generation/HutongMetadata.h"
#include "Generation/HutongGateRow.h"
#include "Generation/HutongRidge.h"
#include "Generation/HutongFootprint.h"
#include "HutongBuildingComponent.generated.h"

class AStaticMeshActor;

struct FHutongPlanOpening
{
	double Centre = 0.0;
	double Width = 0.0;
};

namespace HutongGen { struct FMeshInstances; }

UCLASS(Abstract, ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, PrioritizeCategories="Preset"))
class UHutongBuildingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Picking another replaces the parameters in place; footprint, facing and position stay, so a
	// 正房 becomes a 廂房 without redrawing. Empty = tuned away from any preset.
	UPROPERTY(EditAnywhere, Category="Preset", meta=(DisplayName="Type / Preset", GetOptions="GetPresetOptions", ToolTip="Preset supplying this building's parameters."))
	FString Preset;

	UFUNCTION()
	TArray<FString> GetPresetOptions() const;

	// Renaming the class orphans its presets.
	FName GetPresetKey() const;

	bool GetParamsForPreset(const UScriptStruct*& OutType, void*& OutData);

	bool ApplyPresetParams(const FString& Name);

	// Evidence metadata: geometry cannot tell a traced polygon from a gap-filler, so the placement records it. Exported with the scene.
	UPROPERTY(EditAnywhere, Category="Metadata", meta=(DisplayName="Confidence", ToolTip="How far this placement is attested on the map, 5 to 1."))
	EHutongConfidence Confidence = EHutongConfidence::Attested;

	UPROPERTY(EditAnywhere, Category="Metadata", meta=(DisplayName="Notes", MultiLine=true, ToolTip="Free text about this placement."))
	FString Notes;

	UPROPERTY(EditAnywhere, Category="Appearance", meta=(HutongAdvanced, ShowOnlyInnerProperties, ToolTip="Colours and materials for each surface of this building."))
	FHutongPalette Palette;

	UPROPERTY(EditAnywhere, Category="Detail", meta=(HutongAdvanced, DisplayName="Detail Level", ToolTip="How much of the building's geometry is built."))
	EHutongDetail DetailLevel = EHutongDetail::Near;

	UPROPERTY(EditAnywhere, Category="Detail", meta=(HutongAdvanced, DisplayName="Unique Mesh", ToolTip="Keeps a mesh of its own instead of sharing one with every identical building."))
	bool bBespokeMesh = false;

	UPROPERTY(EditAnywhere, Category="Detail", meta=(HutongAdvanced, DisplayName="Build LOD Chain", ToolTip="Bakes the cheaper detail levels as the mesh's LODs."))
	bool bBuildLODChain = true;

	UPROPERTY(EditAnywhere, Category="Detail", meta=(DisplayName="Plan Only (outline, no geometry)", ToolTip="Draws only the footprint outline on the ground and builds no geometry."))
	bool bPlanOnly = false;

	UPROPERTY(VisibleAnywhere, Category="Identity", meta=(DisplayName="Building Id", ToolTip="Unique identifier of this placement."))
	FGuid BuildingId;

	// Hand-finished work (materials, models placed inside, a tuned mesh) kept from every automatic change:
	// no rebuild, revert, generate, detail change, conversion, preset, bay or facing key, heights, sync,
	// resize or neighbour join touches it. Moving and turning it by hand still work; metadata stays editable.
	UPROPERTY(EditAnywhere, Category="Lock", meta=(DisplayName="Lock This Building (保留)", ToolTip="Keeps this building exactly as it is: nothing rebuilds, reverts, converts or resizes it until unlocked. It can still be moved and turned by hand."))
	bool bLocked = false;

	// Gives this building a copy of its mesh of its own, so editing the mesh (in the Static Mesh Editor)
	// changes no other building; its materials stay. Locking does it too.
	UFUNCTION(CallInEditor, Category="Lock", meta=(DisplayName="Make Mesh Unique", ToolTip="Gives this building its own copy of its mesh, so editing the mesh changes no other building."))
	void MakeMeshUnique();

	// Generators build the rectangle; BuildLODs warps each LOD to these corners. Under Footprint so
	// a layout-only export carries it.
	UPROPERTY(EditAnywhere, Category="Footprint", meta=(HutongAdvanced, DisplayName="Corner Offsets (角偏移)", ToolTip="Offset of each footprint corner from the rectangle, in cm."))
	FHutongFootprintSkew FootprintSkew;

	// Anticlockwise from the origin.
	void GetFootprintCorners(FVector2D OutCorners[4]) const
	{
		HutongFootprint::Corners(GetFootprintSize(), FootprintSkew, OutCorners);
	}

	bool HasFootprintSkew() const { return !FootprintSkew.IsZero(); }

	// Courtyard unit this building belongs to, for setting heights together; empty = none. A wall may
	// stand between two courts and list both, separated by ';' (CanShareCourts); anything else has one.
	UPROPERTY(EditAnywhere, Category="Metadata", meta=(DisplayName="Court (院落)", ToolTip="Name of the courtyard unit this building belongs to; a wall may list several, separated by ;."))
	FString Court;

	virtual bool CanShareCourts() const { return false; }
	TArray<FString> GetCourts() const
	{
		TArray<FString> Out;
		Court.ParseIntoArray(Out, TEXT(";"));
		for (FString& Name : Out) Name.TrimStartAndEndInline();
		Out.RemoveAll([](const FString& Name) { return Name.IsEmpty(); });
		return Out;
	}
	bool IsInCourt(const FString& Name) const { return GetCourts().Contains(Name); }
	void RemoveCourt(const FString& Name)
	{
		TArray<FString> Courts = GetCourts();
		Courts.Remove(Name);
		Court = FString::Join(Courts, TEXT("; "));
	}
	void AssignCourt(const FString& Name)
	{
		TArray<FString> Courts = CanShareCourts() ? GetCourts() : TArray<FString>();
		if (Name.IsEmpty()) Courts.Reset();
		else Courts.AddUnique(Name);
		Court = FString::Join(Courts, TEXT("; "));
	}

	// Rank in its court; Auto reads it from the type and preset.
	UPROPERTY(EditAnywhere, Category="Metadata", meta=(DisplayName="Court Role", ToolTip="Role of this building in its courtyard; Auto picks it from the type and preset."))
	EHutongCourtRole CourtRole = EHutongCourtRole::Auto;

	EHutongCourtRole GetCourtRole() const { return CourtRole != EHutongCourtRole::Auto ? CourtRole : InferCourtRole(); }
	virtual EHutongCourtRole InferCourtRole() const { return EHutongCourtRole::Other; }

	// The caller rebuilds after SetEditHeight.
	virtual double GetEditHeight() const { return GetEaveHeight(); }
	virtual bool SetEditHeight(double Cm) { return false; }
	virtual bool CanSetEditHeight() const { return false; }

	virtual double EaveForRidge(double TargetRidge, const FString& PresetName) const { return -1.0; }

	// The run's axis for corner offsets where the type knows it (0 = X, 1 = Y); -1 judges by the longer side.
	virtual int32 GetSkewRunAxis() const { return -1; }

	// Under Ends each end zone is its end bay, so a slid corner moves that bay alone.
	FHutongFootprintSkew GetFootprintSkew() const { return WithEndBays(FootprintSkew); }
	FHutongFootprintSkew WithEndBays(FHutongFootprintSkew Skew) const;

	virtual double GetBaseCourseTop() const { return -1.0; }
	virtual void SetBaseCourseTop(double TopAboveGround) {}

	// Laid over freshly loaded preset values, so a preset or type change lands as if drawn there.
	virtual void ApplyDragDerived(const UHutongBuildingComponent& Placed);

	// Footprint corners for callers using reflection (PlaceLabels).
	UFUNCTION(BlueprintPure, Category="Hutong|Footprint")
	void GetFootprintCornersLocal(FVector2D& C0, FVector2D& C1, FVector2D& C2, FVector2D& C3) const
	{
		FVector2D C[4];
		GetFootprintCorners(C);
		C0 = C[0]; C1 = C[1]; C2 = C[2]; C3 = C[3];
	}

	virtual void OnComponentCreated() override;

	bool EnsureBuildingId();

	void Rebuild();

	// Returns the collision LOD.
	int32 BuildLODs(TArray<UE::Geometry::FDynamicMesh3>& OutLODs, HutongGen::FMeshTurn* OutTurn = nullptr) const;

	virtual bool BuildInstances(TArray<HutongGen::FMeshInstances>& OutGroups) const { return false; }
	virtual bool BuildsInstanced() const { return false; }

	// Footprint extent in the actor's local XY; the mesh origin is the min corner.
	UFUNCTION(BlueprintPure, Category="Hutong|Footprint")
	virtual FVector2D GetFootprintSize() const { return FVector2D::ZeroVector; }

	virtual void SetFootprintSize(const FVector2D& Size) {}

	virtual double GetEaveHeight() const { return 0.0; }

	virtual double GetRidgeHeight() const { return 0.0; }

	virtual FText GetTypeLabel() const { return NSLOCTEXT("Hutong", "TypeBuilding", "Building"); }

	virtual FLinearColor GetPlanColour() const { return HutongPlanColours::Building; }

	virtual bool HasGeometry() const { return true; }

	FHutongPalette GetBuiltPalette() const
	{
		FHutongPalette P = Palette;
		AdjustPalette(P);
		return P;
	}
	virtual void AdjustPalette(FHutongPalette& InOut) const {}

	virtual void ApplyPlacementAttachments() { ApplyPlanOutline(); }

	virtual bool GetFacade(EHutongBaySide& OutSide) const { return false; }
	virtual bool SetFacade(EHutongBaySide Side) { return false; }
	virtual int32 FacadeTurnStep() const { return 1; }

	virtual int32 GetMaxBayCount() const { return 32; }
	// MoveDoorBay: +1 = one bay left seen from the front.
	virtual bool CanMoveDoorBay() const { return false; }
	virtual bool MoveDoorBay(int32 Delta) { return false; }
	virtual bool CanStepDoorBay(int32 Delta) const { return false; }
	virtual int32 GetDrawnBayCount() const
	{
		FHutongPlanBays Bays;
		GetPlanBays(Bays);
		return Bays.Boundaries.Num() - 1;
	}
	virtual bool CanDivideOrFuse() const { return true; }

	// Overrides read the generator's BayBoundary so plan and mesh agree.
	virtual void GetPlanBays(FHutongPlanBays& Out) const {}

	bool ArePlanBaysAlongX() const;

	// Centre X, Y; direction X, Y; footprint coords.
	virtual void GetPlanArrows(TArray<FVector4>& Out) const {}

	// Where a city wall leg may abut this piece, square to its face. World space: the leg's centre line at the face,
	// the way the leg leaves, a point on the wall's outer side, the leg's base width.
	struct FWallPort { FVector Point; FVector2D Out; FVector Outside; double BaseWidth; };
	virtual void GetWallPorts(TArray<FWallPort>& Out) const {}

	virtual void GetPlanOpenings(TArray<FHutongPlanOpening>& Out) const {}
	virtual void SetPlanOpeningCentre(int32 Index, double CentreCm) {}
	virtual bool IsRunAlongY() const { return false; }

	// Needed by layout-only import before reading a footprint.
	virtual void SetRunAlongY(bool bAlongY) {}

	virtual FName GetTypeVariant() const { return NAME_None; }
	virtual void SetTypeVariant(FName Variant) {}

	virtual void GetTypeVariants(TArray<FName>& Out) const {}

	void ApplyPlanOutline();

	// Sets RF_Transactional so Modify() records a bare NewObject for undo.
	virtual void PostInitProperties() override;
	// Editor-module class: cook strips the component, keeps the baked mesh.
	virtual bool IsEditorOnly() const override { return true; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditUndo() override;
#endif

protected:
	// Subclass contract: fill the mesh in local space with its origin at the footprint's min corner.
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const {}

};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Wall", PrioritizeCategories="Preset Openings Footprint"))
class UHutongWallBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual void ApplyPlacementAttachments() override;
	virtual bool BuildInstances(TArray<HutongGen::FMeshInstances>& OutGroups) const override;
	virtual bool BuildsInstanced() const override;

	// Along Y the frame is a turn, so the run counts back from its far end.
	struct FPieceRun
	{
		FHutongWallParams Params;
		double Length = 0.0;
		double Thickness = 0.0;
		double StartExtend = 0.0;
		double EndExtend = 0.0;
		FHutongFootprintSkew Skew;
		FTransform Frame = FTransform::Identity;
	};
	FPieceRun GetPieceRun() const;

	virtual bool CanShareCourts() const override { return true; }

	virtual void ApplyDragDerived(const UHutongBuildingComponent& Placed) override
	{
		Super::ApplyDragDerived(Placed);
		const UHutongWallBuildingComponent* Wall = Cast<UHutongWallBuildingComponent>(&Placed);
		if (Wall && !Wall->Params.bHasGate && Wall->Params.Doorway == EHutongWallDoorway::None)
		{
			Params.bHasGate = false;
			Params.Doorway = EHutongWallDoorway::None;
		}
	}
	virtual double GetEditHeight() const override { return Params.GetHeight(); }
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.bDeriveFromRole = false; Params.Height = Cm; return true; }
	virtual EHutongCourtRole InferCourtRole() const override
	{
		switch (Params.Role)
		{
		case EHutongWallRole::Courtyard:    return EHutongCourtRole::CourtWall;
		case EHutongWallRole::ImperialCity: return EHutongCourtRole::Other;
		default:                            return EHutongCourtRole::LaneWall;
		}
	}
	virtual FText GetTypeLabel() const override
	{
		switch (Params.Role)
		{
		case EHutongWallRole::Courtyard:    return NSLOCTEXT("Hutong", "TypeCourtWall", "Court Wall (隔牆)");
		case EHutongWallRole::ImperialCity: return NSLOCTEXT("Hutong", "TypeImperialWall", "Imperial City Wall (皇城牆)");
		default:                            return NSLOCTEXT("Hutong", "TypeLaneWall", "Lane Wall (院牆)");
		}
	}

	virtual FLinearColor GetPlanColour() const override
	{
		switch (Params.Role)
		{
		case EHutongWallRole::Courtyard:    return HutongPlanColours::CourtWall;
		case EHutongWallRole::ImperialCity: return HutongPlanColours::ImperialWall;
		default:                            return HutongPlanColours::Wall;
		}
	}

	UPROPERTY(EditAnywhere, Category="Wall", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the wall generator."))
	FHutongWallParams Params;

	virtual double GetBaseCourseTop() const override { return Params.GetBaseCourseHeight(); }
	virtual void SetBaseCourseTop(double TopAboveGround) override { Params.BaseCourseHeight = FMath::Clamp(TopAboveGround, 10.0, 0.6 * Params.GetHeight()); }

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="10", ClampMin="10", Units="cm", ToolTip="Length of the wall run, in cm."))
	double Length = 500.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Runs the wall along the actor's local Y axis instead of X."))
	bool bLengthAlongY = false;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(HutongAdvanced, DisplayName="Start Miter", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="How far the start of the run extends past the rectangle, in cm."))
	double StartExtend = 0.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(HutongAdvanced, DisplayName="End Miter", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="How far the end of the run extends past the rectangle, in cm."))
	double EndExtend = 0.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(HutongAdvanced, DisplayName="Footprint Thickness", UIMin="0", ClampMin="0", Units="cm", ToolTip="Maximum wall thickness, in cm; zero for no limit."))
	double FootprintThickness = 0.0;


	static void BuildWallMesh(const FHutongWallParams& InParams, double InLength, double InThickness,
		bool bAlongY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	// Params.FootprintThickness is set only on BuildWallMesh's local copy.
	double GetBuiltThickness() const
	{
		return (FootprintThickness > 0.0)
			? FMath::Min(Params.GetThickness(), FootprintThickness)
			: Params.GetThickness();
	}
	virtual FVector2D GetFootprintSize() const override
	{
		const double T = GetBuiltThickness();
		return FVector2D(bLengthAlongY ? T : Length, bLengthAlongY ? Length : T);
	}
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		Length = FMath::Max(bLengthAlongY ? S.Y : S.X, 10.0);
		// A narrower cross extent caps the thickness, at most the role's figure; widening back clears the cap.
		const double Cross = bLengthAlongY ? S.X : S.Y;
		FootprintThickness = (Cross > 1.0 && Cross < Params.GetThickness() - 0.01) ? Cross : 0.0;
	}
	virtual bool IsRunAlongY() const override { return bLengthAlongY; }
	virtual void SetRunAlongY(bool bAlongY) override { bLengthAlongY = bAlongY; }

	virtual FName GetTypeVariant() const override
	{
		return FName(*StaticEnum<EHutongWallRole>()->GetNameStringByValue((int64)Params.Role));
	}
	virtual void SetTypeVariant(FName Variant) override
	{
		const int64 Value = StaticEnum<EHutongWallRole>()->GetValueByNameString(Variant.ToString());
		if (Value != INDEX_NONE) Params.Role = (EHutongWallRole)Value;
	}

	virtual void GetTypeVariants(TArray<FName>& Out) const override
	{
		const UEnum* Roles = StaticEnum<EHutongWallRole>();
		for (int32 i = 0; i < Roles->NumEnums() - 1; ++i)
		{
			Out.Add(FName(*Roles->GetNameStringByIndex(i)));
		}
	}

	virtual void GetPlanOpenings(TArray<FHutongPlanOpening>& Out) const override
	{
		// Gates in list order, so a slider dragged past another keeps its own gate.
		if (Params.bHasGate)
		{
			Out.Add({ Params.GatePosition * Length, Params.GateWidth });
			for (const double Position : Params.MoreGatePositions) Out.Add({ Position * Length, Params.GateWidth });
		}
		if (Params.Doorway != EHutongWallDoorway::None && Params.HasDecorativeDoorway(Length))
		{
			Out.Add({ Params.DoorwayPosition * Length, Params.GetDoorwayWidth() });
		}
	}
	virtual void SetPlanOpeningCentre(int32 Index, double CentreCm) override
	{
		TArray<FHutongPlanOpening> Openings;
		GetPlanOpenings(Openings);
		if (!Openings.IsValidIndex(Index) || Length <= 1.0) return;
		// Keep the opening inside the run with a brick each side.
		const double Half = 0.5 * Openings[Index].Width + 10.0;
		const double Fraction = FMath::Clamp(CentreCm, Half, FMath::Max(Length - Half, Half)) / Length;
		const int32 Gates = Params.bHasGate ? 1 + Params.MoreGatePositions.Num() : 0;
		if (Index == 0 && Gates > 0) Params.GatePosition = Fraction;
		else if (Index < Gates) Params.MoreGatePositions[Index - 1] = Fraction;
		else Params.DoorwayPosition = Fraction;
	}

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Siheyuan", PrioritizeCategories="Preset Footprint"))
class UHutongSiheyuanBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypeHouse", "House (房)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::House; }

	UPROPERTY(EditAnywhere, Category="Siheyuan", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the house generator."))
	FHutongSiheyuanParams Params;

	virtual double GetBaseCourseTop() const override { return Params.GetFloorHeight() + Params.GetBaseCourseHeight(); }
	virtual void SetBaseCourseTop(double TopAboveGround) override { Params.BaseCourseHeight = FMath::Max(TopAboveGround - Params.GetFloorHeight(), 25.0); }

	// Params.Width is the struct default until filled from the footprint.
	virtual double GetEaveHeight() const override { return ParamsForFootprint().GetEaveHeight(); }
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.bDeriveEaveFromBays = false; Params.EaveHeight = Cm; return true; }
	virtual EHutongCourtRole InferCourtRole() const override;
	virtual double GetRidgeHeight() const override
	{
		const FHutongSiheyuanParams P = ParamsForFootprint();
		return HutongGen::Ridge::House(P, P.Width, P.Depth);
	}
	FHutongSiheyuanParams ParamsForFootprint() const
	{
		FHutongSiheyuanParams P = Params;
		const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
		P.Width = bAlongX ? FootprintX : FootprintY;
		P.Depth = bAlongX ? FootprintY : FootprintX;
		P.BayCountOverride = BayCountOverride;
		return P;
	}

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="50", ClampMin="10", Units="cm", ToolTip="Extent of the footprint along the actor's local X, in cm."))
	double FootprintX = 800.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="50", ClampMin="10", Units="cm", ToolTip="Extent of the footprint along the actor's local Y, in cm."))
	double FootprintY = 500.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Which side of the footprint carries the bay facade and door."))
	EHutongBaySide BaySide = EHutongBaySide::MinusY;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Bay Count Override", UIMin="0", UIMax="32", ClampMin="0", ClampMax="32", ToolTip="Number of bays; zero chooses automatically."))
	int32 BayCountOverride = 0;

	static void BuildSiheyuanMesh(const FHutongSiheyuanParams& InParams, EHutongBaySide Side,
		int32 InBayCountOverride, double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual bool GetFacade(EHutongBaySide& OutSide) const override { OutSide = BaySide; return true; }
	virtual bool SetFacade(EHutongBaySide Side) override { BaySide = Side; return true; }

	virtual bool CanMoveDoorBay() const override { return Params.bHasFrontDoorCenter && ParamsForFootprint().GetBayCount() > 1; }
	virtual bool MoveDoorBay(int32 Delta) override { return Params.StepDoorBay(Delta, ParamsForFootprint().GetBayCount()); }
	virtual bool CanStepDoorBay(int32 Delta) const override { return Params.CanStepDoorBay(Delta, ParamsForFootprint().GetBayCount()); }

	virtual void GetPlanBays(FHutongPlanBays& Out) const override
	{
		FHutongSiheyuanParams P = Params;
		const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
		P.Width = bAlongX ? FootprintX : FootprintY;
		P.Depth = bAlongX ? FootprintY : FootprintX;
		P.BayCountOverride = BayCountOverride;

		const int32 N = P.GetBayCount();
		const double ColR = P.GetColumnRadius();
		for (int32 i = 0; i <= N; ++i) Out.Boundaries.Add(P.GetBayBoundary(i, N, P.Width, ColR));
		if (P.bHasFrontDoorCenter) Out.DoorBay = P.GetDoorBayIndex(N);
		Out.ColumnRadius = ColR;
		HutongGen::PlanBays::OntoFacade(Out, BaySide, FootprintX, FootprintY);
	}

	virtual FVector2D GetFootprintSize() const override
	{
		return FVector2D(FootprintX, FootprintY);
	}
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		FootprintX = FMath::Max(S.X, 10.0);
		FootprintY = FMath::Max(S.Y, 10.0);
	}

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Gate House", PrioritizeCategories="Preset Gate Footprint"))
class UHutongGateHouseBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypeGateHouse", "Gate House (大門)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Gate; }

	UPROPERTY(EditAnywhere, Category="Gate House", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the gate house generator."))
	FHutongGateHouseParams Params;

	virtual double GetBaseCourseTop() const override { return FMath::Max(Params.FloorHeight, 0.0) + Params.GetBaseCourseHeight(); }
	virtual void SetBaseCourseTop(double TopAboveGround) override { Params.BaseCourseHeight = FMath::Max(TopAboveGround - FMath::Max(Params.FloorHeight, 0.0), 25.0); }
	virtual double GetEaveHeight() const override { return Params.GetEaveHeight(); }
	virtual double EaveForRidge(double TargetRidge, const FString& PresetName) const override;
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.EaveHeight = Cm; return true; }
	virtual EHutongCourtRole InferCourtRole() const override { return EHutongCourtRole::Gate; }
	virtual double GetRidgeHeight() const override
	{
		return HutongGen::Ridge::Gate(Params,
			HutongGen::BaySide::IsAlongX(BaySide) ? FootprintY : FootprintX);
	}

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="50", ClampMin="10", Units="cm", ToolTip="Extent of the footprint along the actor's local X, in cm."))
	double FootprintX = 400.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="50", ClampMin="10", Units="cm", ToolTip="Extent of the footprint along the actor's local Y, in cm."))
	double FootprintY = 400.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Which side of the footprint the gate faces."))
	EHutongBaySide BaySide = EHutongBaySide::MinusY;
	virtual bool GetFacade(EHutongBaySide& OutSide) const override { OutSide = BaySide; return true; }
	virtual bool SetFacade(EHutongBaySide Side) override { BaySide = Side; return true; }
	virtual int32 FacadeTurnStep() const override { return 2; }

	static void BuildGateHouseMesh(const FHutongGateHouseParams& InParams, EHutongBaySide Side,
		double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override
	{
		return FVector2D(FootprintX, FootprintY);
	}
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		FootprintX = FMath::Max(S.X, 10.0);
		FootprintY = FMath::Max(S.Y, 10.0);
	}

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Paifang", PrioritizeCategories="Preset Footprint"))
class UHutongPaifangBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypePaifang", "Memorial Arch (牌坊)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Paifang; }

	UPROPERTY(EditAnywhere, Category="Paifang", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the paifang generator."))
	FHutongPaifangParams Params;
	// The frame's eave is the central 樓's, on the architrave above the columns.
	virtual double GetEaveHeight() const override { return Params.GetRoofEaveZ(); }
	virtual double GetRidgeHeight() const override { return HutongGen::Ridge::Paifang(Params); }

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="100", ClampMin="20", Units="cm", ToolTip="Span of the archway, in cm."))
	double Length = 900.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="20", ClampMin="10", Units="cm", ToolTip="Cross extent of the footprint, in cm."))
	double Depth = 200.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Runs the span along the actor's local Y axis instead of X."))
	bool bLengthAlongY = false;

	virtual void GetPlanBays(FHutongPlanBays& Out) const override
	{
		const int32 N = Params.GetBayCount();
		const double ColR = Params.GetColumnRadiusFor(Depth);
		for (int32 i = 0; i <= N; ++i) Out.Boundaries.Add(Params.GetBayBoundary(i, N, Length, ColR));
	}

	static void BuildPaifangMesh(const FHutongPaifangParams& InParams,
		double InLength, double InDepth, bool bAlongY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override
	{
		return FVector2D(bLengthAlongY ? Depth : Length, bLengthAlongY ? Length : Depth);
	}
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		Length = FMath::Max(bLengthAlongY ? S.Y : S.X, 10.0);
		Depth = FMath::Max(bLengthAlongY ? S.X : S.Y, 10.0);
	}
	virtual bool IsRunAlongY() const override { return bLengthAlongY; }
	virtual void SetRunAlongY(bool bAlongY) override { bLengthAlongY = bAlongY; }

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Screen Wall", PrioritizeCategories="Preset Footprint"))
class UHutongScreenWallBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypeScreen", "Screen Wall (影壁)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Screen; }

	UPROPERTY(EditAnywhere, Category="Screen Wall", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the screen wall generator."))
	FHutongScreenWallParams Params;

	virtual double GetBaseCourseTop() const override { return FMath::Max(Params.PlinthHeight, 0.0) + Params.GetBaseCourseHeight(); }
	virtual void SetBaseCourseTop(double TopAboveGround) override { Params.BaseCourseHeight = FMath::Max(TopAboveGround - FMath::Max(Params.PlinthHeight, 0.0), 25.0); }
	virtual double GetEaveHeight() const override { return Params.GetEaveHeight(); }
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.Height = Cm; return true; }
	virtual double GetRidgeHeight() const override { return HutongGen::Ridge::ScreenWall(Params); }

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="60", ClampMin="20", Units="cm", ToolTip="Length of the screen wall, in cm."))
	double Length = 400.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Runs the screen along the actor's local Y axis instead of X."))
	bool bLengthAlongY = false;

	static void BuildScreenWallMesh(const FHutongScreenWallParams& InParams,
		double InLength, bool bAlongY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override
	{
		const double Dep = Params.GetFootprintDepth();
		return FVector2D(bLengthAlongY ? Dep : Length, bLengthAlongY ? Length : Dep);
	}
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		Length = FMath::Max(bLengthAlongY ? S.Y : S.X, 20.0);
	}
	virtual bool IsRunAlongY() const override { return bLengthAlongY; }
	virtual void SetRunAlongY(bool bAlongY) override { bLengthAlongY = bAlongY; }

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Corridor", PrioritizeCategories="Preset Footprint"))
class UHutongCorridorBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypeCorridor", "Covered Corridor (遊廊)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Corridor; }

	UPROPERTY(EditAnywhere, Category="Corridor", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the corridor generator."))
	FHutongCorridorParams Params;
	virtual double GetEaveHeight() const override { return Params.GetEaveHeight(); }
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.EaveHeight = Cm; return true; }
	virtual EHutongCourtRole InferCourtRole() const override { return EHutongCourtRole::Corridor; }
	virtual double GetRidgeHeight() const override { return HutongGen::Ridge::Corridor(Params); }

	virtual void ApplyDragDerived(const UHutongBuildingComponent& Placed) override
	{
		Super::ApplyDragDerived(Placed);
		const FVector2D S = Placed.GetFootprintSize();
		Params.Width = Params.WalkWidthFromFootprint(FMath::Min(S.X, S.Y));
		Width = Params.Width;
	}

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="100", ClampMin="40", Units="cm", ToolTip="Length of the corridor run, in cm."))
	double Length = 800.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="90", ClampMin="60", Units="cm", ToolTip="Clear width of the walk, in cm."))
	double Width = 150.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Runs the corridor along the actor's local Y axis instead of X."))
	bool bLengthAlongY = false;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Open Side Flipped", ToolTip="Turns the run end-for-end so it opens onto the other side."))
	bool bFlipOpenSide = false;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(HutongAdvanced, DisplayName="Bench Gap At", UIMin="-1", UIMax="1", ClampMin="-1", ClampMax="1", ToolTip="Where the bench breaks, as a fraction of the length; negative leaves it unbroken."))
	double BenchGapAt = -1.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(HutongAdvanced, DisplayName="No Post At Low End", ToolTip="Omits the post at the run's low end."))
	bool bOmitLowEndPost = false;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(HutongAdvanced, DisplayName="No Post At High End", ToolTip="Omits the post at the run's high end."))
	bool bOmitHighEndPost = false;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(HutongAdvanced, DisplayName="No Bench At Low End", ToolTip="Omits the bench from the end bay at the run's low end."))
	bool bNoBenchAtLowEnd = false;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(HutongAdvanced, DisplayName="No Bench At High End", ToolTip="Omits the bench from the end bay at the run's high end."))
	bool bNoBenchAtHighEnd = false;

	virtual void GetPlanBays(FHutongPlanBays& Out) const override
	{
		const int32 N = Params.GetBayCount(Length);
		for (int32 i = 0; i <= N; ++i) Out.Boundaries.Add(Params.GetBayBoundary(i, N, Length));
	}

	static void BuildCorridorMesh(const FHutongCorridorParams& InParams,
		double InLength, bool bAlongY, bool bFlip, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override
	{
		const double Dep = Width + Params.GetCrossExtras();
		return FVector2D(bLengthAlongY ? Dep : Length, bLengthAlongY ? Length : Dep);
	}
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		Length = FMath::Max(bLengthAlongY ? S.Y : S.X, 40.0);
		Width = FMath::Max((bLengthAlongY ? S.X : S.Y) - Params.GetCrossExtras(), 60.0);
	}
	virtual bool IsRunAlongY() const override { return bLengthAlongY; }
	virtual void SetRunAlongY(bool bAlongY) override { bLengthAlongY = bAlongY; }

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Inner Gate", PrioritizeCategories="Preset Footprint"))
class UHutongInnerGateBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypeInnerGate", "Inner Gate (垂花門)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::InnerGate; }

	UPROPERTY(EditAnywhere, Category="Inner Gate", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the inner gate generator."))
	FHutongInnerGateParams Params;
	virtual double GetEaveHeight() const override { return Params.GetEaveHeight(); }
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.EaveHeight = Cm; return true; }
	virtual EHutongCourtRole InferCourtRole() const override { return EHutongCourtRole::InnerGate; }
	virtual double GetRidgeHeight() const override { return HutongGen::Ridge::InnerGate(Params); }

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="150", ClampMin="60", Units="cm", ToolTip="Frontage of the gate, in cm."))
	double Width = 320.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="120", ClampMin="60", Units="cm", ToolTip="Depth of the gate, in cm."))
	double Depth = 260.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Which side of the footprint the gate faces."))
	EHutongBaySide BaySide = EHutongBaySide::MinusY;
	virtual bool GetFacade(EHutongBaySide& OutSide) const override { OutSide = BaySide; return true; }
	virtual bool SetFacade(EHutongBaySide Side) override
	{
		if (HutongGen::BaySide::IsAlongX(BaySide) != HutongGen::BaySide::IsAlongX(Side)) Swap(Width, Depth);
		BaySide = Side;
		return true;
	}
	virtual int32 FacadeTurnStep() const override { return 2; }

	static void BuildInnerGateMesh(const FHutongInnerGateParams& InParams, EHutongBaySide Side,
		double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override
	{
		const bool bX = HutongGen::BaySide::IsAlongX(BaySide);
		return FVector2D(bX ? Width : Depth, bX ? Depth : Width);
	}
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		const bool bX = HutongGen::BaySide::IsAlongX(BaySide);
		Width = FMath::Max(bX ? S.X : S.Y, 60.0);
		Depth = FMath::Max(bX ? S.Y : S.X, 60.0);
	}

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Shopfront", PrioritizeCategories="Preset Footprint"))
class UHutongShopfrontBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypeShopfront", "Shopfront (鋪面房)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Shopfront; }

	virtual void AdjustPalette(FHutongPalette& InOut) const override;
	// Every placement path bakes with its own palette, then attaches: dress the mesh in the scheme here.
	virtual void ApplyPlacementAttachments() override;

	UPROPERTY(EditAnywhere, Category="Shopfront", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the shopfront generator."))
	FHutongShopfrontParams Params;

	virtual double GetBaseCourseTop() const override { return Params.GetFloorHeight() + Params.GetBaseCourseHeight(); }
	virtual void SetBaseCourseTop(double TopAboveGround) override { Params.BaseCourseHeight = FMath::Max(TopAboveGround - Params.GetFloorHeight(), 25.0); }
	virtual double GetEaveHeight() const override { return Params.GetEaveHeight(); }
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.EaveHeight = Cm; return true; }
	virtual double GetRidgeHeight() const override { return HutongGen::Ridge::Shop(Params); }

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="200", ClampMin="80", Units="cm", ToolTip="Extent of the footprint along the actor's local X, in cm."))
	double FootprintX = 900.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="200", ClampMin="80", Units="cm", ToolTip="Extent of the footprint along the actor's local Y, in cm."))
	double FootprintY = 500.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Which side of the footprint faces the street."))
	EHutongBaySide BaySide = EHutongBaySide::MinusY;
	virtual bool GetFacade(EHutongBaySide& OutSide) const override { OutSide = BaySide; return true; }
	virtual bool SetFacade(EHutongBaySide Side) override { BaySide = Side; return true; }

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Bay Count Override", UIMin="0", UIMax="32", ClampMin="0", ClampMax="32", ToolTip="Number of bays; zero chooses automatically."))
	int32 BayCountOverride = 0;

	virtual void GetPlanBays(FHutongPlanBays& Out) const override
	{
		const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
		const double W = bAlongX ? FootprintX : FootprintY;
		const double D = bAlongX ? FootprintY : FootprintX;
		const int32 N = (BayCountOverride > 0)
			? BayCountOverride
			: HutongGen::ComputeBayCount(W, Params.MinBayWidth, Params.MaxBayWidth);
		const double ColR = Params.GetColumnRadiusFor(W, D);
		for (int32 i = 0; i <= N; ++i) Out.Boundaries.Add(Params.GetBayBoundary(i, N, W, ColR));
		HutongGen::PlanBays::OntoFacade(Out, BaySide, FootprintX, FootprintY);
	}

	static void BuildShopfrontMesh(const FHutongShopfrontParams& InParams, EHutongBaySide Side,
		int32 InBayCountOverride, double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override
	{
		return FVector2D(FootprintX, FootprintY);
	}
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		FootprintX = FMath::Max(S.X, 10.0);
		FootprintY = FMath::Max(S.Y, 10.0);
	}

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Multi-Story Building", PrioritizeCategories="Preset Footprint"))
class UHutongStoreyBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypeStorey", "Multi-Story Building (樓)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Storey; }

	UPROPERTY(EditAnywhere, Category="Story", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the multi-story building (樓) generator."))
	FHutongStoreyParams Params;

	virtual double GetBaseCourseTop() const override { return FMath::Max(Params.FloorHeight, 0.0) + Params.GetBaseCourseHeight(); }
	virtual void SetBaseCourseTop(double TopAboveGround) override { Params.BaseCourseHeight = FMath::Max(TopAboveGround - FMath::Max(Params.FloorHeight, 0.0), 25.0); }
	virtual double GetEaveHeight() const override { return Params.GetEaveHeight(); }
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.UpperStoreyHeight = Cm - Params.GetStoreyLineHeight(); return true; }
	virtual double GetRidgeHeight() const override { return HutongGen::Ridge::Storey(Params); }

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="200", ClampMin="80", Units="cm", ToolTip="Extent of the footprint along the actor's local X, in cm."))
	double FootprintX = 900.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="200", ClampMin="80", Units="cm", ToolTip="Extent of the footprint along the actor's local Y, in cm."))
	double FootprintY = 620.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Which side of the footprint faces the street."))
	EHutongBaySide BaySide = EHutongBaySide::MinusY;
	virtual bool GetFacade(EHutongBaySide& OutSide) const override { OutSide = BaySide; return true; }
	virtual bool SetFacade(EHutongBaySide Side) override { BaySide = Side; return true; }

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Bay Count Override", UIMin="0", UIMax="32", ClampMin="0", ClampMax="32", ToolTip="Number of bays; zero chooses automatically."))
	int32 BayCountOverride = 0;

	virtual void GetPlanBays(FHutongPlanBays& Out) const override
	{
		const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
		const double W = bAlongX ? FootprintX : FootprintY;
		const double D = bAlongX ? FootprintY : FootprintX;
		const int32 N = (BayCountOverride > 0)
			? BayCountOverride
			: HutongGen::ComputeBayCount(W, Params.MinBayWidth, Params.MaxBayWidth);
		const double ColR = Params.GetColumnRadiusFor(W, D);
		for (int32 i = 0; i <= N; ++i) Out.Boundaries.Add(Params.GetBayBoundary(i, N, W, ColR));
		HutongGen::PlanBays::OntoFacade(Out, BaySide, FootprintX, FootprintY);
	}

	static void BuildStoreyMesh(const FHutongStoreyParams& InParams, EHutongBaySide Side,
		int32 InBayCountOverride, double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override
	{
		return FVector2D(FootprintX, FootprintY);
	}
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		FootprintX = FMath::Max(S.X, 60.0);
		FootprintY = FMath::Max(S.Y, 60.0);
	}

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Ear Room", PrioritizeCategories="Preset Footprint"))
class UHutongEarPassageBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		// One label for every kind: it keys the conversion list; the preset says which kind.
		return NSLOCTEXT("Hutong", "TypeEarRoom", "Ear Room (耳房)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::EarPassage; }

	UPROPERTY(EditAnywhere, Category="Ear Room", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the ear room (耳房) and any covered passage (過道) beside or through it."))
	FHutongEarPassageParams Params;

	virtual double GetBaseCourseTop() const override { return Params.Room.GetFloorHeight() + Params.Room.GetBaseCourseHeight(); }
	virtual void SetBaseCourseTop(double TopAboveGround) override { Params.Room.BaseCourseHeight = FMath::Max(TopAboveGround - Params.Room.GetFloorHeight(), 25.0); }

	virtual double GetEaveHeight() const override { return ParamsForFootprint().GetEaveHeight(); }
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.Room.bDeriveEaveFromBays = false; Params.Room.EaveHeight = Cm; return true; }
	virtual EHutongCourtRole InferCourtRole() const override { return EHutongCourtRole::EarRoom; }
	virtual double GetRidgeHeight() const override
	{
		const FHutongEarPassageParams P = ParamsForFootprint();
		return HutongGen::Ridge::EarPassage(P, P.Width, P.Depth);
	}

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Bay Count Override", UIMin="0", UIMax="3", ClampMin="0", ClampMax="3", ToolTip="Number of room bays, 1 to 3; 0 for automatic (one or two)."))
	int32 BayCountOverride = 0;

	virtual int32 GetMaxBayCount() const override { return FHutongEarPassageParams::MaxRoomBays; }
	virtual int32 GetDrawnBayCount() const override { return ParamsForFootprint().RoomParams().GetBayCount(); }
	virtual bool CanMoveDoorBay() const override { return Params.HasRoom() && Params.Room.bHasFrontDoorCenter && GetDrawnBayCount() > 1; }
	virtual bool MoveDoorBay(int32 Delta) override { return Params.HasRoom() && Params.Room.StepDoorBay(Delta, GetDrawnBayCount()); }
	virtual bool CanStepDoorBay(int32 Delta) const override { return Params.HasRoom() && Params.Room.CanStepDoorBay(Delta, GetDrawnBayCount()); }
	virtual bool CanDivideOrFuse() const override { return false; }

	FHutongEarPassageParams ParamsForFootprint() const
	{
		FHutongEarPassageParams P = Params;
		P.BayCountOverride = BayCountOverride;
		const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
		P.Width = bAlongX ? FootprintX : FootprintY;
		P.Depth = bAlongX ? FootprintY : FootprintX;
		return P;
	}

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="50", ClampMin="10", Units="cm", ToolTip="Extent of the footprint along the actor's local X, in cm."))
	double FootprintX = 740.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="50", ClampMin="10", Units="cm", ToolTip="Extent of the footprint along the actor's local Y, in cm."))
	double FootprintY = 340.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Side of the footprint carrying the bay facade and the passage doorway."))
	EHutongBaySide BaySide = EHutongBaySide::MinusY;

	static void BuildEarPassageMesh(const FHutongEarPassageParams& InParams, EHutongBaySide Side,
		double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual bool GetFacade(EHutongBaySide& OutSide) const override { OutSide = BaySide; return true; }
	virtual bool SetFacade(EHutongBaySide Side) override { BaySide = Side; return true; }

	// The strip line replaces the end column so the division sits at the gable face.
	static FHutongPlanBays PlanBaysInBuildFrame(const FHutongEarPassageParams& P)
	{
		FHutongPlanBays Out;
		if (!P.HasRoom())
		{
			Out.Boundaries = { 0.0, P.GetWidth() };
			if (P.HasClosingDoorway()) Out.DoorBay = 0;
			return Out;
		}
		const FHutongSiheyuanParams R = P.RoomParams();
		const int32 N = R.GetBayCount();
		const double ColR = R.GetColumnRadius();
		const double X0 = P.GetRoomX0();
		TArray<double> Room;
		for (int32 i = 0; i <= N; ++i) Room.Add(X0 + R.GetBayBoundary(i, N, R.Width, ColR));
		if (!P.HasPassage())
		{
			Out.Boundaries = Room;
			Out.ColumnRadius = ColR;
			if (R.bHasFrontDoorCenter) Out.DoorBay = R.GetDoorBayIndex(N);
		}
		else if (P.bPassageAtFarEnd)
		{
			Out.Boundaries = Room;
			Out.Boundaries.Last() = P.GetWidth() - P.GetStripWidth();
			Out.Boundaries.Add(P.GetWidth());
			if (R.bHasFrontDoorCenter) Out.DoorBay = R.GetDoorBayIndex(N);
		}
		else
		{
			Out.Boundaries.Add(0.0);
			Out.Boundaries.Append(Room);
			Out.Boundaries[1] = P.GetStripWidth();
			if (R.bHasFrontDoorCenter) Out.DoorBay = R.GetDoorBayIndex(N) + 1;
		}
		return Out;
	}
	virtual void GetPlanBays(FHutongPlanBays& Out) const override
	{
		Out = PlanBaysInBuildFrame(ParamsForFootprint());
		HutongGen::PlanBays::OntoFacade(Out, BaySide, FootprintX, FootprintY);
	}

	virtual FVector2D GetFootprintSize() const override { return FVector2D(FootprintX, FootprintY); }
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		FootprintX = FMath::Max(S.X, 10.0);
		FootprintY = FMath::Max(S.Y, 10.0);
	}

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Pavilion", PrioritizeCategories="Preset Footprint"))
class UHutongPavilionBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypePavilion", "Pavilion (亭)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Pavilion; }

	UPROPERTY(EditAnywhere, Category="Pavilion", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the pavilion generator."))
	FHutongPavilionParams Params;
	// The eave derives from the bay, so from this component's footprint.
	virtual double GetEaveHeight() const override
	{
		FHutongPavilionParams Sized = Params;
		Sized.Width = FMath::Max(Width, 1.0);
		Sized.Depth = FMath::Max(Depth, 1.0);
		return Sized.GetEaveHeight();
	}
	virtual double GetRidgeHeight() const override { return HutongGen::Ridge::Pavilion(Params, Width, Depth); }

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="150", ClampMin="80", Units="cm", ToolTip="Extent of the pavilion along the actor's local X, in cm."))
	double Width = 300.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="150", ClampMin="80", Units="cm", ToolTip="Extent of the pavilion along the actor's local Y, in cm."))
	double Depth = 300.0;

	static void BuildPavilionMesh(const FHutongPavilionParams& InParams,
		double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override
	{
		return FVector2D(Width, Depth);
	}
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		Width = FMath::Max(S.X, 10.0);
		Depth = FMath::Max(S.Y, 10.0);
	}

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Path", PrioritizeCategories="Preset Footprint"))
class UHutongPathBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypePath", "Paved Path (甬路)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Path; }

	UPROPERTY(EditAnywhere, Category="Path", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the path generator."))
	FHutongPathParams Params;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="100", ClampMin="40", Units="cm", ToolTip="Length of the path, in cm."))
	double Length = 600.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="60", ClampMin="40", Units="cm", ToolTip="Width of the path, in cm."))
	double Width = 130.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Runs the path along the actor's local Y axis instead of X."))
	bool bLengthAlongY = false;

	static void BuildPathMesh(const FHutongPathParams& InParams,
		double InLength, double InWidth, bool bAlongY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override
	{
		const double Dep = Width + 2.0 * Params.GetKerbWidth();
		return FVector2D(bLengthAlongY ? Dep : Length, bLengthAlongY ? Length : Dep);
	}
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		Length = FMath::Max(bLengthAlongY ? S.Y : S.X, 40.0);
		Width = FMath::Max((bLengthAlongY ? S.X : S.Y) - 2.0 * Params.GetKerbWidth(), 40.0);
	}
	virtual bool IsRunAlongY() const override { return bLengthAlongY; }
	virtual void SetRunAlongY(bool bAlongY) override { bLengthAlongY = bAlongY; }

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Passage", PrioritizeCategories="Preset Footprint"))
class UHutongPassageBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypePassage", "Covered Passage (過道)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Passage; }

	UPROPERTY(EditAnywhere, Category="Passage", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the passage generator."))
	FHutongPassageParams Params;
	virtual double GetEaveHeight() const override { return Params.GetEaveHeight(); }
	virtual double GetRidgeHeight() const override
	{
		FHutongPassageParams P = Params;
		P.Width = Width;
		return HutongGen::Ridge::Passage(P);
	}

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="100", ClampMin="40", Units="cm", ToolTip="Length of the passage, in cm."))
	double Length = 300.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="80", ClampMin="60", Units="cm", ToolTip="Clear width of the passage from wall face to wall face, in cm."))
	double Width = 200.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Runs the passage along the actor's local Y axis instead of X."))
	bool bLengthAlongY = false;

	static void BuildPassageMesh(const FHutongPassageParams& InParams,
		double InLength, double InWidth, bool bAlongY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override
	{
		FHutongPassageParams P = Params;
		P.Width = Width;
		const double Dep = P.GetRoofSpan();
		return FVector2D(bLengthAlongY ? Dep : Length, bLengthAlongY ? Length : Dep);
	}
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		Length = FMath::Max(bLengthAlongY ? S.Y : S.X, 40.0);
	}
	virtual bool IsRunAlongY() const override { return bLengthAlongY; }
	virtual void SetRunAlongY(bool bAlongY) override { bLengthAlongY = bAlongY; }

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Flower Bed", PrioritizeCategories="Preset Footprint"))
class UHutongFlowerBedBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypeFlowerBed", "Flower Bed (花池)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::FlowerBed; }

	UPROPERTY(EditAnywhere, Category="Bed", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the flower bed generator."))
	FHutongFlowerBedParams Params;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="60", ClampMin="30", Units="cm", ToolTip="Extent of the footprint along the actor's local X, in cm."))
	double FootprintX = 200.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="60", ClampMin="30", Units="cm", ToolTip="Extent of the footprint along the actor's local Y, in cm."))
	double FootprintY = 140.0;

	static void BuildFlowerBedMesh(const FHutongFlowerBedParams& InParams,
		double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override
	{
		return FVector2D(FootprintX, FootprintY);
	}
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		FootprintX = FMath::Max(S.X, 10.0);
		FootprintY = FMath::Max(S.Y, 10.0);
	}

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Water Jar", PrioritizeCategories="Preset Footprint"))
class UHutongWaterJarBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypeWaterJar", "Water Jar (魚缸)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::WaterJar; }

	UPROPERTY(EditAnywhere, Category="Jar", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the water jar generator."))
	FHutongWaterJarParams Params;

	virtual void ApplyDragDerived(const UHutongBuildingComponent& Placed) override
	{
		Super::ApplyDragDerived(Placed);
		const FVector2D S = Placed.GetFootprintSize();
		const UHutongWaterJarBuildingComponent* Jar = Cast<UHutongWaterJarBuildingComponent>(&Placed);
		Params.BellyDiameter = Jar ? Jar->Params.BellyDiameter : FMath::Clamp(FMath::Max(S.X, S.Y), 20.0, 140.0);
	}

	static void BuildWaterJarMesh(const FHutongWaterJarParams& InParams,
		UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override
	{
		const double S = Params.GetFootprint();
		return FVector2D(S, S);
	}

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

// 城角: the corner of a city or palace wall, a square platform its two legs abut, the corner tower's base.
UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Wall Corner", PrioritizeCategories="Preset Footprint"))
class UHutongWallCornerBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		if (Shape == EHutongWallCornerShape::Rounded) return NSLOCTEXT("Hutong", "TypeRoundedCorner", "Rounded Wall Corner (甕城角)");
		return Params.Rank == EHutongCityWallRank::Palace
			? NSLOCTEXT("Hutong", "TypePalaceCorner", "Palace Wall Corner (紫禁城城角)")
			: NSLOCTEXT("Hutong", "TypeWallCorner", "City Wall Corner (城角)");
	}
	virtual FLinearColor GetPlanColour() const override
	{
		return Params.Rank == EHutongCityWallRank::Palace ? HutongPlanColours::PalaceWall : HutongPlanColours::CityWall;
	}
	virtual bool CanDivideOrFuse() const override { return false; }

	UPROPERTY(EditAnywhere, Category="City Wall", meta=(ShowOnlyInnerProperties, ToolTip="The wall's section, which the corner carries round."))
	FHutongCityWallParams Params;

	UPROPERTY(EditAnywhere, Category="City Wall", meta=(HutongBasic, DisplayName="Shape", ToolTip="A square platform under a corner tower, or the wall carried round a curve."))
	EHutongWallCornerShape Shape = EHutongWallCornerShape::Platform;

	UPROPERTY(EditAnywhere, Category="City Wall", meta=(HutongBasic, DisplayName="Tower", EditCondition="Shape == EHutongWallCornerShape::Platform", ToolTip="What stands on a square corner platform."))
	EHutongCornerTower Tower = EHutongCornerTower::None;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Platform Size", UIMin="0", UIMax="6000", ClampMin="0", Units="cm", ToolTip="Width of the corner's square footprint, in cm; zero takes the city's."))
	double PlatformSize = 0.0;

	// 0..3 anticlockwise from the actor's origin, as GetFootprintCorners counts them.
	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Outer Corner", ClampMin="0", ClampMax="3", ToolTip="Which corner of the footprint is the outside corner of the wall."))
	int32 OuterCorner = 0;

	double GetPlatformSize() const { return SizeFor(Params, Shape, PlatformSize); }
	static double SizeFor(const FHutongCityWallParams& InParams, EHutongWallCornerShape InShape, double Asked)
	{
		const bool bRounded = InShape == EHutongWallCornerShape::Rounded;
		const double S = Asked > 0.0 ? Asked : (bRounded ? InParams.GetRoundedCorner() : InParams.GetCornerPlatform());
		return FMath::Max(S, InParams.GetBaseWidth() + (bRounded ? 10.0 : 2.0 * InParams.GetBatter() + 10.0));
	}

	virtual double GetEditHeight() const override { return Params.GetHeight(); }
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.PinFromRank(); Params.Height = Cm; return true; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	virtual void GetWallPorts(TArray<FWallPort>& Out) const override;
	FVector GetOuterCornerWorld() const;

	static void BuildCornerMesh(const FHutongCityWallParams& InParams, EHutongWallCornerShape InShape, double InPlatform,
		int32 InOuterCorner, UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Detail = EHutongDetail::Near,
		EHutongCornerTower InTower = EHutongCornerTower::None);

	virtual FVector2D GetFootprintSize() const override { const double S = GetPlatformSize(); return FVector2D(S, S); }
	virtual void SetFootprintSize(const FVector2D& S) override { PlatformSize = FMath::Max(S.X, S.Y); }

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

// 箭樓: the arrow tower on its platform at the front of a barbican; the barbican's legs abut its sides.
UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Arrow Tower", PrioritizeCategories="Preset Footprint"))
class UHutongArrowTowerBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override { return NSLOCTEXT("Hutong", "TypeArrowTower", "Arrow Tower (箭樓)"); }
	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::CityWall; }
	virtual bool CanDivideOrFuse() const override { return false; }

	UPROPERTY(EditAnywhere, Category="Arrow Tower", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the arrow tower generator."))
	FHutongArrowTowerParams Params;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Front", ToolTip="Which side of the footprint faces out of the city."))
	EHutongBaySide Facade = EHutongBaySide::MinusY;

	virtual bool GetFacade(EHutongBaySide& OutSide) const override { OutSide = Facade; return true; }
	virtual bool SetFacade(EHutongBaySide Side) override { Facade = Side; return true; }

	virtual double GetEditHeight() const override { return Params.LowerHeight; }
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.LowerHeight = Cm; return true; }

	virtual void GetWallPorts(TArray<FWallPort>& Out) const override;

	static FVector2D FootprintFor(const FHutongArrowTowerParams& InParams, EHutongBaySide Side);
	static void BuildArrowTowerMesh(const FHutongArrowTowerParams& InParams, EHutongBaySide Side,
		UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override { return FootprintFor(Params, Facade); }

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

// 城門: a length of city wall with an arched passage and a tower over it; the legs either side abut its ends.
UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong City Gate", PrioritizeCategories="Preset Footprint"))
class UHutongCityGateBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		switch (Params.Tower)
		{
		case EHutongCityGateTower::Zhalou:   return NSLOCTEXT("Hutong", "TypeBarbicanGate", "Barbican Gate (閘樓)");
		case EHutongCityGateTower::Chenglou: return NSLOCTEXT("Hutong", "TypeGateTower", "City Gate Tower (城樓)");
		default:                             return NSLOCTEXT("Hutong", "TypeCityGate", "City Gate Passage (門洞)");
		}
	}
	virtual FLinearColor GetPlanColour() const override
	{
		return Params.Rank == EHutongCityWallRank::Palace ? HutongPlanColours::PalaceWall : HutongPlanColours::CityWall;
	}
	virtual bool CanDivideOrFuse() const override { return false; }

	UPROPERTY(EditAnywhere, Category="Gate", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the city gate generator."))
	FHutongCityGateParams Params;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Front", ToolTip="Which side of the footprint faces out of the city."))
	EHutongBaySide Facade = EHutongBaySide::MinusY;

	virtual bool GetFacade(EHutongBaySide& OutSide) const override { OutSide = Facade; return true; }
	virtual bool SetFacade(EHutongBaySide Side) override { Facade = Side; return true; }

	virtual void GetWallPorts(TArray<FWallPort>& Out) const override;

	static FVector2D FootprintFor(const FHutongCityGateParams& InParams, EHutongBaySide Side);
	static void BuildCityGateMesh(const FHutongCityGateParams& InParams, EHutongBaySide Side,
		UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override { return FootprintFor(Params, Facade); }

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

// 城牆: one straight leg of a city wall; the tool lays a run as legs mitred where they meet.
UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong City Wall", PrioritizeCategories="Preset Footprint"))
class UHutongCityWallBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return Params.Rank == EHutongCityWallRank::Palace
			? NSLOCTEXT("Hutong", "TypePalaceWall", "Palace Wall (紫禁城城牆)")
			: NSLOCTEXT("Hutong", "TypeCityWall", "City Wall (城牆)");
	}

	virtual FLinearColor GetPlanColour() const override
	{
		return Params.Rank == EHutongCityWallRank::Palace ? HutongPlanColours::PalaceWall : HutongPlanColours::CityWall;
	}
	virtual bool CanDivideOrFuse() const override { return false; }

	UPROPERTY(EditAnywhere, Category="City Wall", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the city wall generator."))
	FHutongCityWallParams Params;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="500", ClampMin="100", Units="cm", ToolTip="Length of this leg of the wall, in cm."))
	double Length = 3000.0;

	// The outer face, where the crenellated parapet stands: one of the two long sides.
	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Outer Face", ToolTip="Which long side of the footprint faces out of the city and carries the crenellated parapet."))
	EHutongBaySide OuterSide = EHutongBaySide::MinusY;

	virtual double GetEditHeight() const override { return Params.GetHeight(); }
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.PinFromRank(); Params.Height = Cm; return true; }

	virtual bool GetFacade(EHutongBaySide& OutSide) const override { OutSide = OuterSide; return true; }
	virtual bool SetFacade(EHutongBaySide Side) override
	{
		if (Side != EHutongBaySide::MinusY && Side != EHutongBaySide::PlusY) return false;
		OuterSide = Side;
		return true;
	}
	virtual int32 FacadeTurnStep() const override { return 2; }

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// A leg runs along X however short: the chain's mitres are along-X offsets.
	virtual int32 GetSkewRunAxis() const override { return 0; }

	virtual void GetPlanArrows(TArray<FVector4>& Out) const override;

	static void BuildCityWallMesh(const FHutongCityWallParams& InParams, double InLength, EHutongBaySide Outer,
		UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Detail = EHutongDetail::Near);

	// In the leg's own frame: origin at the footprint's min corner, the leg along +X.
	static bool GetRampOutline(const FHutongCityWallParams& InParams, double InLength, EHutongBaySide Outer,
		FVector2D OutCorners[4], FVector2D& OutUpFrom, FVector2D& OutUpTo, FVector2D OutGap[2]);

	virtual FVector2D GetFootprintSize() const override { return FVector2D(Length, Params.GetBaseWidth()); }
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		Length = FMath::Max(S.X, 100.0);
		const double Delta = FMath::Max(S.Y, 200.0) - Params.GetBaseWidth();
		if (FMath::Abs(Delta) < 0.5) return;
		Params.PinFromRank();
		Params.BaseWidth = Params.GetBaseWidth() + Delta;
		Params.TopWidth = FMath::Max(Params.GetTopWidth() + Delta, 150.0);
	}

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

// 小房: a small freestanding building — guard post, shed, lone room, wayside shrine. PROVISIONAL (open-todos).
UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Small Building", PrioritizeCategories="Preset Footprint"))
class UHutongSmallBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypeSmallBuilding", "Small Building (小房)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::SmallBuilding; }

	UPROPERTY(EditAnywhere, Category="Small Building", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the small building generator."))
	FHutongSmallBuildingParams Params;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="100", ClampMin="60", Units="cm", ToolTip="Extent of the footprint along the actor's local X, in cm."))
	double FootprintX = 400.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="100", ClampMin="60", Units="cm", ToolTip="Extent of the footprint along the actor's local Y, in cm."))
	double FootprintY = 360.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Which side of the footprint is the front."))
	EHutongBaySide BaySide = EHutongBaySide::MinusY;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Bay Count Override", UIMin="0", UIMax="5", ClampMin="0", ClampMax="8", ToolTip="Number of bays; zero chooses automatically."))
	int32 BayCountOverride = 0;

	static FHutongSmallBuildingParams Sized(FHutongSmallBuildingParams P, EHutongBaySide Side, int32 InBayCountOverride,
		double SizeX, double SizeY)
	{
		const bool bAlongX = HutongGen::BaySide::IsAlongX(Side);
		P.Width = bAlongX ? SizeX : SizeY;
		P.Depth = bAlongX ? SizeY : SizeX;
		P.BayCountOverride = InBayCountOverride;
		return P;
	}
	FHutongSmallBuildingParams SizedParams() const { return Sized(Params, BaySide, BayCountOverride, FootprintX, FootprintY); }

	virtual double GetEaveHeight() const override { return Params.GetEaveHeight(); }
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.EaveHeight = Cm; return true; }
	virtual double GetRidgeHeight() const override { return HutongGen::Ridge::Small(SizedParams()); }

	virtual bool GetFacade(EHutongBaySide& OutSide) const override { OutSide = BaySide; return true; }
	virtual bool SetFacade(EHutongBaySide Side) override { BaySide = Side; return true; }
	virtual int32 GetMaxBayCount() const override { return 8; }

	virtual void GetPlanBays(FHutongPlanBays& Out) const override
	{
		const FHutongSmallBuildingParams P = SizedParams();
		const int32 N = P.GetBayCount();
		for (int32 i = 0; i <= N; ++i) Out.Boundaries.Add(P.GetBayBoundary(i, N));
		Out.DoorBay = P.Front == EHutongSmallFront::Door ? P.GetDoorBay(N) : INDEX_NONE;
		HutongGen::PlanBays::OntoFacade(Out, BaySide, FootprintX, FootprintY);
	}

	static void BuildSmallBuildingMesh(const FHutongSmallBuildingParams& InParams, EHutongBaySide Side,
		int32 InBayCountOverride, double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override { return FVector2D(FootprintX, FootprintY); }
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		FootprintX = FMath::Max(S.X, 60.0);
		FootprintY = FMath::Max(S.Y, 60.0);
	}

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

// A traced footprint whose type is not known: laid out, never built; carries metadata only.
UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Unknown", PrioritizeCategories="Metadata Footprint"))
class UHutongUnknownBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	UHutongUnknownBuildingComponent() { bPlanOnly = true; }

	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypeUnknown", "Unknown (未知)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Unknown; }
	virtual bool HasGeometry() const override { return false; }
	virtual bool CanDivideOrFuse() const override { return false; }

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="50", ClampMin="10", Units="cm", ToolTip="Extent of the footprint along the actor's local X, in cm."))
	double FootprintX = 600.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="50", ClampMin="10", Units="cm", ToolTip="Extent of the footprint along the actor's local Y, in cm."))
	double FootprintY = 400.0;

	virtual FVector2D GetFootprintSize() const override { return FVector2D(FootprintX, FootprintY); }
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		FootprintX = FMath::Max(S.X, 10.0);
		FootprintY = FMath::Max(S.Y, 10.0);
	}
};

// 構架: a house's frame alone, for showing how it is built.
UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Timber Frame", PrioritizeCategories="Preset Footprint"))
class UHutongFrameBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypeFrame", "Timber Frame (構架)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Frame; }

	UPROPERTY(EditAnywhere, Category="Frame", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the timber frame generator."))
	FHutongFrameParams Params;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="50", ClampMin="10", Units="cm", ToolTip="Extent of the footprint along the actor's local X, in cm."))
	double FootprintX = 1060.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="50", ClampMin="10", Units="cm", ToolTip="Extent of the footprint along the actor's local Y, in cm."))
	double FootprintY = 700.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Which side of the footprint is the front of the frame."))
	EHutongBaySide BaySide = EHutongBaySide::MinusY;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Bay Count Override", UIMin="0", UIMax="32", ClampMin="0", ClampMax="32", ToolTip="Number of bays; zero chooses automatically."))
	int32 BayCountOverride = 0;

	FHutongSiheyuanParams HouseForFootprint() const
	{
		FHutongSiheyuanParams H = Params.House;
		const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
		H.Width = bAlongX ? FootprintX : FootprintY;
		H.Depth = bAlongX ? FootprintY : FootprintX;
		H.BayCountOverride = BayCountOverride;
		return H;
	}

	virtual double GetEaveHeight() const override { return HouseForFootprint().GetEaveHeight(); }
	virtual bool CanMoveDoorBay() const override { return Params.House.bHasFrontDoorCenter && HouseForFootprint().GetBayCount() > 1; }
	virtual bool MoveDoorBay(int32 Delta) override { return Params.House.StepDoorBay(Delta, HouseForFootprint().GetBayCount()); }
	virtual bool CanStepDoorBay(int32 Delta) const override { return Params.House.CanStepDoorBay(Delta, HouseForFootprint().GetBayCount()); }
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.House.bDeriveEaveFromBays = false; Params.House.EaveHeight = Cm; return true; }
	virtual double GetRidgeHeight() const override
	{
		const HutongGen::FrameLayout::FLayout L = HutongGen::FrameLayout::Make(HouseForFootprint());
		return L.PurlinTop(L.Ridge());
	}

	static void BuildFrameMesh(const FHutongFrameParams& InParams, EHutongBaySide Side,
		int32 InBayCountOverride, double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual bool GetFacade(EHutongBaySide& OutSide) const override { OutSide = BaySide; return true; }
	virtual bool SetFacade(EHutongBaySide Side) override { BaySide = Side; return true; }

	virtual void GetPlanBays(FHutongPlanBays& Out) const override
	{
		const FHutongSiheyuanParams H = HouseForFootprint();
		const int32 N = H.GetBayCount();
		const double ColR = H.GetColumnRadius();
		for (int32 i = 0; i <= N; ++i) Out.Boundaries.Add(H.GetBayBoundary(i, N, H.Width, ColR));
		Out.DoorBay = H.GetDoorBayIndex(N);
		Out.ColumnRadius = HutongGen::FrameLayout::Make(H).D * 0.5;
		HutongGen::PlanBays::OntoFacade(Out, BaySide, FootprintX, FootprintY);
	}

	virtual FVector2D GetFootprintSize() const override { return FVector2D(FootprintX, FootprintY); }
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		FootprintX = FMath::Max(S.X, 10.0);
		FootprintY = FMath::Max(S.Y, 10.0);
	}

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Hall", PrioritizeCategories="Preset Footprint"))
class UHutongHallBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypeHall", "Temple Hall (殿)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Hall; }

	UPROPERTY(EditAnywhere, Category="Hall", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the temple hall generator."))
	FHutongHallParams Params;

	virtual double GetBaseCourseTop() const override { return FMath::Max(Params.FloorHeight, 0.0) + Params.GetBaseCourseHeight(); }
	virtual void SetBaseCourseTop(double TopAboveGround) override { Params.BaseCourseHeight = FMath::Max(TopAboveGround - FMath::Max(Params.FloorHeight, 0.0), 25.0); }
	FHutongHallParams SizedParams() const
	{
		FHutongHallParams P = Params;
		const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
		P.Width = bAlongX ? FootprintX : FootprintY;
		P.Depth = bAlongX ? FootprintY : FootprintX;
		return P;
	}
	virtual double GetEaveHeight() const override { return SizedParams().GetEaveHeight(); }
	virtual bool CanSetEditHeight() const override { return !Params.IsGrand(); }
	virtual bool SetEditHeight(double Cm) override { if (Params.IsGrand()) return false; Params.EaveHeight = Cm; return true; }
	virtual double GetRidgeHeight() const override
	{
		const FHutongHallParams P = SizedParams();
		return HutongGen::Ridge::Hall(P, P.Width, P.Depth);
	}

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="300", ClampMin="120", Units="cm", ToolTip="Extent of the footprint along the actor's local X, in cm."))
	double FootprintX = 900.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="250", ClampMin="120", Units="cm", ToolTip="Extent of the footprint along the actor's local Y, in cm."))
	double FootprintY = 620.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Which side of the footprint carries the facade."))
	EHutongBaySide BaySide = EHutongBaySide::MinusY;
	virtual bool GetFacade(EHutongBaySide& OutSide) const override { OutSide = BaySide; return true; }
	virtual bool SetFacade(EHutongBaySide Side) override { BaySide = Side; return true; }

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Bay Count Override", UIMin="0", UIMax="32", ClampMin="0", ClampMax="32", ToolTip="Number of bays; zero chooses automatically."))
	int32 BayCountOverride = 0;

	virtual void GetPlanBays(FHutongPlanBays& Out) const override
	{
		const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
		const double W = bAlongX ? FootprintX : FootprintY;
		const double D = bAlongX ? FootprintY : FootprintX;
		const int32 N = (BayCountOverride > 0)
			? BayCountOverride
			: HutongGen::ComputeBayCount(W, Params.MinBayWidth, Params.MaxBayWidth);
		const double ColR = Params.GetColumnRadiusFor(W, D);
		for (int32 i = 0; i <= N; ++i) Out.Boundaries.Add(Params.GetBayBoundary(i, N, W, ColR));
		Out.DoorBay = N / 2;
		HutongGen::PlanBays::OntoFacade(Out, BaySide, FootprintX, FootprintY);
	}

	static void BuildHallMesh(const FHutongHallParams& InParams, EHutongBaySide Side,
		int32 BayCountOverride, double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual FVector2D GetFootprintSize() const override
	{
		return FVector2D(FootprintX, FootprintY);
	}
	virtual void SetFootprintSize(const FVector2D& S) override
	{
		FootprintX = FMath::Max(S.X, 10.0);
		FootprintY = FMath::Max(S.Y, 10.0);
	}

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};
