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
#include "Generation/HutongDetail.h"
#include "Generation/HutongMetadata.h"
#include "Generation/HutongGateRow.h"
#include "Generation/HutongRidge.h"
#include "Generation/HutongFootprint.h"
#include "HutongBuildingComponent.generated.h"

class AStaticMeshActor;

// An opening in a line-like piece, for the plan's slider.
struct FHutongPlanOpening
{
	double Centre = 0.0;
	double Width = 0.0;
};

UCLASS(Abstract, ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, PrioritizeCategories="Preset"))
class UHutongBuildingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Picking another replaces the parameters in place; footprint, facing and position stay, so a
	// 正房 becomes a 廂房 without redrawing. Empty = tuned away from any preset.
	UPROPERTY(EditAnywhere, Category="Preset", meta=(DisplayName="Type / Preset", GetOptions="GetPresetOptions", ToolTip="Preset this building's parameters come from; changing it rebuilds in place."))
	FString Preset;

	UFUNCTION()
	TArray<FString> GetPresetOptions() const;

	// The tool's preset key, derived from the class name (UHutong<Key>BuildingComponent).
	// Renaming the class orphans its presets.
	FName GetPresetKey() const;

	// The params struct by reflection; same lookup as UHutongPresetProperties.
	bool GetParamsForPreset(const UScriptStruct*& OutType, void*& OutData);

	// Loads a named preset's parameters. Also the export baseline that recorded diffs are measured against and applied to.
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

	UPROPERTY(EditAnywhere, Category="Detail", meta=(HutongAdvanced, DisplayName="Bespoke Mesh", ToolTip="Keeps a mesh of its own instead of a shared library mesh."))
	bool bBespokeMesh = false;

	UPROPERTY(EditAnywhere, Category="Detail", meta=(HutongAdvanced, DisplayName="Build LOD Chain", ToolTip="Bakes the cheaper detail levels as the mesh's LODs."))
	bool bBuildLODChain = true;

	UPROPERTY(EditAnywhere, Category="Detail", meta=(DisplayName="Plan Only (outline, no geometry)", ToolTip="Draws only the footprint outline on the ground and builds no geometry."))
	bool bPlanOnly = false;

	UPROPERTY(VisibleAnywhere, Category="Identity", meta=(DisplayName="Building Id", ToolTip="Unique identifier of this placement."))
	FGuid BuildingId;

	// Generators build the rectangle; BuildLODs warps each LOD to these corners. Under Footprint so
	// a layout-only export carries it.
	UPROPERTY(EditAnywhere, Category="Footprint", meta=(HutongAdvanced, DisplayName="Corner Offsets (角偏移)", ToolTip="Moves each footprint corner off the rectangle; zero keeps it."))
	FHutongFootprintSkew FootprintSkew;

	// Local corners, rectangle plus offsets, anticlockwise from the origin.
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
	// Takes the building out of one court (a wall keeps any other).
	void RemoveCourt(const FString& Name)
	{
		TArray<FString> Courts = GetCourts();
		Courts.Remove(Name);
		Court = FString::Join(Courts, TEXT("; "));
	}
	// Puts the building in a court: a wall adds it to those it stands between, anything else moves
	// to it. Empty clears every court.
	void AssignCourt(const FString& Name)
	{
		TArray<FString> Courts = CanShareCourts() ? GetCourts() : TArray<FString>();
		if (Name.IsEmpty()) Courts.Reset();
		else Courts.AddUnique(Name);
		Court = FString::Join(Courts, TEXT("; "));
	}

	// Rank in its court; Auto reads it from the type and preset.
	UPROPERTY(EditAnywhere, Category="Metadata", meta=(DisplayName="Court Role", ToolTip="What this building is in its courtyard; Auto reads it from the type and preset."))
	EHutongCourtRole CourtRole = EHutongCourtRole::Auto;

	EHutongCourtRole GetCourtRole() const { return CourtRole != EHutongCourtRole::Auto ? CourtRole : InferCourtRole(); }
	virtual EHutongCourtRole InferCourtRole() const { return EHutongCourtRole::Other; }

	// The height the heights tool edits: the eave (檐柱 top) of a roofed type, the body top of a wall.
	// Set overrides whatever derived it; false where the type's height is not one number (亭, the
	// 大式 殿, 牌坊). The caller rebuilds; GetEditHeight answers what is built, the doorway floor included.
	virtual double GetEditHeight() const { return GetEaveHeight(); }
	virtual bool SetEditHeight(double Cm) { return false; }
	virtual bool CanSetEditHeight() const { return false; }

	// A gate's eave whose ridge stands at TargetRidge, under the named preset (empty = its own
	// parameters); negative for every other type.
	virtual double EaveForRidge(double TargetRidge, const FString& PresetName) const { return -1.0; }

	// The offsets as the warp, outline and handles read them: under Ends on a building with bays
	// along its run, each end zone is its end bay, so a slid corner moves that bay alone.
	FHutongFootprintSkew GetFootprintSkew() const { return WithEndBays(FootprintSkew); }
	// Any candidate offsets (a drag's) with this building's end bays.
	FHutongFootprintSkew WithEndBays(FHutongFootprintSkew Skew) const;

	// 下鹼 top above ground, negative if none. Setter lets a snapped placement match its neighbour.
	virtual double GetBaseCourseTop() const { return -1.0; }
	virtual void SetBaseCourseTop(double TopAboveGround) {}

	// Footprint corners for callers using reflection (PlaceLabels).
	UFUNCTION(BlueprintPure, Category="Hutong|Footprint")
	void GetFootprintCornersLocal(FVector2D& C0, FVector2D& C1, FVector2D& C2, FVector2D& C3) const
	{
		FVector2D C[4];
		GetFootprintCorners(C);
		C0 = C[0]; C1 = C[1]; C2 = C[2]; C3 = C[3];
	}

	virtual void OnComponentCreated() override;

	// Mints the id if this placement predates the field.
	bool EnsureBuildingId();

	// Regenerates the owning actor's static mesh from the current parameters.
	void Rebuild();

	// The baked LOD chain, LOD0 first. Returns the collision LOD.
	int32 BuildLODs(TArray<UE::Geometry::FDynamicMesh3>& OutLODs) const;

	// Footprint extent in the actor's local XY; the mesh origin is the min corner.
	UFUNCTION(BlueprintPure, Category="Hutong|Footprint")
	virtual FVector2D GetFootprintSize() const { return FVector2D::ZeroVector; }

	// The inverse, for the plan outline's handles.
	virtual void SetFootprintSize(const FVector2D& Size) {}

	// Eave above ground; zero for pieces a gate cannot stand beside (walls, paths).
	virtual double GetEaveHeight() const { return 0.0; }

	// Ridge above ground, zero as for the eave; a gate in this row must clear it.
	virtual double GetRidgeHeight() const { return 0.0; }

	// What this piece is, for the mode's hover readout.
	virtual FText GetTypeLabel() const { return NSLOCTEXT("Hutong", "TypeBuilding", "Building"); }

	// Plan-only outline colour; tells plan rectangles apart by type.
	virtual FLinearColor GetPlanColour() const { return HutongPlanColours::Building; }

	// Attachments besides the baked mesh (currently lights).
	virtual void ApplyPlacementAttachments() { ApplyPlanOutline(); }

	// Which edge of the footprint is the facade, for the plan outline's hatching.
	virtual bool GetFacade(EHutongBaySide& OutSide) const { return false; }
	// Turns the facade to another side of the same footprint.
	virtual bool SetFacade(EHutongBaySide Side) { return false; }
	// How many quarter turns one press of [ or ] moves the facade.
	virtual int32 FacadeTurnStep() const { return 1; }

	// Column divisions of the frontage for the plan and readout; empty when no bays. Overrides read
	// the generator's BayBoundary so plan and mesh agree.
	virtual void GetPlanBays(FHutongPlanBays& Out) const {}

	// Axis of the boundaries: facade edge if any, else the run.
	bool ArePlanBaysAlongX() const;

	// The openings a plan can slide along the run — a wall's 牆垣式門 and garden doorway.
	virtual void GetPlanOpenings(TArray<FHutongPlanOpening>& Out) const {}
	virtual void SetPlanOpeningCentre(int32 Index, double CentreCm) {}
	// Which footprint axis the run lies along, for a type with a run.
	virtual bool IsRunAlongY() const { return false; }

	// Needed by layout-only import before reading a footprint: length comes off the run extent.
	virtual void SetRunAlongY(bool bAlongY) {}

	// Kind within the class (e.g. wall role), carried by layout-only records; NAME_None if the class has one kind.
	virtual FName GetTypeVariant() const { return NAME_None; }
	virtual void SetTypeVariant(FName Variant) {}

	// Variants offered separately in the conversion list (院牆, 隔牆); empty if the class has one kind.
	virtual void GetTypeVariants(TArray<FName>& Out) const {}

	// Creates, updates or removes the plan outline to match bPlanOnly.
	void ApplyPlanOutline();

	// Sets RF_Transactional: without it, Modify() records nothing and a bare NewObject is not undoable.
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
	// A wall may stand between two courts.
	virtual bool CanShareCourts() const override { return true; }
	// The heights tool edits the body top; the cap rises over it.
	virtual double GetEditHeight() const override { return Params.GetHeight(); }
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.bDeriveFromRole = false; Params.Height = Cm; return true; }
	virtual EHutongCourtRole InferCourtRole() const override
	{
		return Params.Role == EHutongWallRole::Courtyard ? EHutongCourtRole::CourtWall : EHutongCourtRole::LaneWall;
	}
	// Role labels the run, as nothing else distinguishes 院牆 from 隔牆.
	virtual FText GetTypeLabel() const override
	{
		return (Params.Role == EHutongWallRole::Courtyard)
			? NSLOCTEXT("Hutong", "TypeCourtWall", "court wall (隔牆)")
			: NSLOCTEXT("Hutong", "TypeLaneWall", "lane wall (院牆)");
	}

	virtual FLinearColor GetPlanColour() const override
	{
		return (Params.Role == EHutongWallRole::Courtyard)
			? HutongPlanColours::CourtWall : HutongPlanColours::Wall;
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

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(HutongAdvanced, DisplayName="Footprint Thickness", UIMin="0", ClampMin="0", Units="cm", ToolTip="Cross extent capping the wall's thickness, in cm; zero applies no cap."))
	double FootprintThickness = 0.0;


	// Shared by the tool's preview-time build and the component's rebuild.
	static void BuildWallMesh(const FHutongWallParams& InParams, double InLength, double InThickness,
		bool bAlongY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	// Built thickness: capped by the component's FootprintThickness (Params.FootprintThickness is
	// only set on BuildWallMesh's local copy).
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
		// A narrower cross extent caps the thickness; widening back to the role's figure clears the
		// cap. Never widens past the role.
		const double Cross = bLengthAlongY ? S.X : S.Y;
		FootprintThickness = (Cross > 1.0 && Cross < Params.GetThickness() - 0.01) ? Cross : 0.0;
	}
	virtual bool IsRunAlongY() const override { return bLengthAlongY; }
	virtual void SetRunAlongY(bool bAlongY) override { bLengthAlongY = bAlongY; }

	// 院牆 or 隔牆: a type, not a parameter; decides height, thickness, cap and allowed openings.
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
		// Gate first, then garden doorway: stable slider indices.
		if (Params.bHasGate)
		{
			Out.Add({ Params.GatePosition * Length, Params.GateWidth });
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
		const bool bGate = Params.bHasGate && Index == 0;
		if (bGate) Params.GatePosition = Fraction;
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
		return NSLOCTEXT("Hutong", "TypeHouse", "house (房)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::House; }

	UPROPERTY(EditAnywhere, Category="Siheyuan", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the house generator."))
	FHutongSiheyuanParams Params;

	virtual double GetBaseCourseTop() const override { return Params.GetFloorHeight() + Params.GetBaseCourseHeight(); }
	virtual void SetBaseCourseTop(double TopAboveGround) override { Params.BaseCourseHeight = FMath::Max(TopAboveGround - Params.GetFloorHeight(), 25.0); }

	// Footprint first: eave derives from bay width, and Params.Width is the struct default until filled.
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

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Bay Count Override", UIMin="0", UIMax="32", ClampMin="0", ClampMax="32", ToolTip="Forces the number of bays; zero derives it from the bay width limits."))
	int32 BayCountOverride = 0;

	// Shared by the tool's preview-time build and the component's rebuild.
	static void BuildSiheyuanMesh(const FHutongSiheyuanParams& InParams, EHutongBaySide Side,
		int32 InBayCountOverride, double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual bool GetFacade(EHutongBaySide& OutSide) const override { OutSide = BaySide; return true; }
	virtual bool SetFacade(EHutongBaySide Side) override { BaySide = Side; return true; }

	virtual void GetPlanBays(FHutongPlanBays& Out) const override
	{
		// Fill Width/Depth from the footprint as BuildSiheyuanMesh does; Params.Width is the struct default until then.
		FHutongSiheyuanParams P = Params;
		const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
		P.Width = bAlongX ? FootprintX : FootprintY;
		P.Depth = bAlongX ? FootprintY : FootprintX;
		P.BayCountOverride = BayCountOverride;

		const int32 N = P.GetBayCount();
		const double ColR = P.GetColumnRadius();
		for (int32 i = 0; i <= N; ++i) Out.Boundaries.Add(P.GetBayBoundary(i, N, P.Width, ColR));
		if (P.bHasFrontDoorCenter) Out.DoorBay = P.GetDoorBayIndex(N);

		// Rows: 檐柱 at the front edge, facade a 廊步 in under a 前廊, rear 金柱 a 廊步 inside the
		// 後檐柱 in the back wall.
		double FY, RY;
		P.GetBuiltVerandaDepths(P.Depth, FMath::Clamp(P.WallThickness, 1.0, FMath::Min(P.Width, P.Depth) * 0.2), FY, RY);
		if (FY > 0.0) Out.ColumnRows.Add(0.0);
		Out.ColumnRows.Add(FY);
		if (RY > 0.0) Out.ColumnRows.Add(P.Depth - RY);
		Out.ColumnRows.Add(P.Depth);
		Out.ColumnRadius = ColR;
		Out.FootingSize = HutongCanon::Frame::BaseStoneSide * 2.0 * ColR;
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
		return NSLOCTEXT("Hutong", "TypeGateHouse", "gate house (大門)");
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

	// Shared by the tool's preview-time build and the component's rebuild.
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
		return NSLOCTEXT("Hutong", "TypePaifang", "memorial arch (牌坊)");
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
		return NSLOCTEXT("Hutong", "TypeScreen", "screen wall (影壁)");
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
		return NSLOCTEXT("Hutong", "TypeCorridor", "covered corridor (遊廊)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Corridor; }

	UPROPERTY(EditAnywhere, Category="Corridor", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the corridor generator."))
	FHutongCorridorParams Params;
	virtual double GetEaveHeight() const override { return Params.GetEaveHeight(); }
	virtual bool CanSetEditHeight() const override { return true; }
	virtual bool SetEditHeight(double Cm) override { Params.EaveHeight = Cm; return true; }
	virtual EHutongCourtRole InferCourtRole() const override { return EHutongCourtRole::Corridor; }
	virtual double GetRidgeHeight() const override { return HutongGen::Ridge::Corridor(Params); }

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

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(HutongAdvanced, DisplayName="No Post At Low End", ToolTip="Leaves out the post at the run's low end, where another run's post stands at the same corner."))
	bool bOmitLowEndPost = false;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(HutongAdvanced, DisplayName="No Post At High End", ToolTip="Leaves out the post at the run's high end, where another run's post stands at the same corner."))
	bool bOmitHighEndPost = false;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(HutongAdvanced, DisplayName="No Bench At Low End", ToolTip="Leaves the bench out of the end bay at the run's low end, where the walk turns through a doorway."))
	bool bNoBenchAtLowEnd = false;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(HutongAdvanced, DisplayName="No Bench At High End", ToolTip="Leaves the bench out of the end bay at the run's high end, where the walk turns through a doorway."))
	bool bNoBenchAtHighEnd = false;

	virtual void GetPlanBays(FHutongPlanBays& Out) const override
	{
		// Bays follow from run length and 步 spacing.
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
		return NSLOCTEXT("Hutong", "TypeInnerGate", "inner gate (垂花門)");
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
		// Width and Depth are the frontage and the run, whichever axis they lie on.
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
		return NSLOCTEXT("Hutong", "TypeShopfront", "shopfront (鋪面房)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Shopfront; }

	UPROPERTY(EditAnywhere, Category="Shopfront", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the shopfront generator."))
	FHutongShopfrontParams Params;

	virtual double GetBaseCourseTop() const override { return FMath::Max(Params.FloorHeight, 0.0) + Params.GetBaseCourseHeight(); }
	virtual void SetBaseCourseTop(double TopAboveGround) override { Params.BaseCourseHeight = FMath::Max(TopAboveGround - FMath::Max(Params.FloorHeight, 0.0), 25.0); }
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

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Bay Count Override", UIMin="0", UIMax="32", ClampMin="0", ClampMax="32", ToolTip="Forces the number of bays; zero derives it from the frontage."))
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
		// No door bay: boards come out of whichever bays are open, counted from the middle.
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
		return NSLOCTEXT("Hutong", "TypeStorey", "multi-story building (樓)");
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

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Bay Count Override", UIMin="0", UIMax="32", ClampMin="0", ClampMax="32", ToolTip="Forces the number of bays; zero derives it from the frontage."))
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
		// No door bay, as for the shop.
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

UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Ear Room With Passage", PrioritizeCategories="Preset Footprint"))
class UHutongEarPassageBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypeEarPassage", "ear room with passage (耳房過道)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::EarPassage; }

	UPROPERTY(EditAnywhere, Category="Ear Room With Passage", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the ear room and the covered passage beside it."))
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

	FHutongEarPassageParams ParamsForFootprint() const
	{
		FHutongEarPassageParams P = Params;
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

	// Room bay lines plus the passage-strip line, in the build frame (facade on -Y). The strip line
	// replaces the end column so the division sits at the gable face.
	static FHutongPlanBays PlanBaysInBuildFrame(const FHutongEarPassageParams& P)
	{
		FHutongPlanBays Out;
		const FHutongSiheyuanParams R = P.RoomParams();
		const int32 N = R.GetBayCount();
		const double ColR = R.GetColumnRadius();
		const double X0 = P.GetRoomX0();
		TArray<double> Room;
		for (int32 i = 0; i <= N; ++i) Room.Add(X0 + R.GetBayBoundary(i, N, R.Width, ColR));
		if (P.bPassageAtFarEnd)
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
		return NSLOCTEXT("Hutong", "TypePavilion", "pavilion (亭)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Pavilion; }

	UPROPERTY(EditAnywhere, Category="Pavilion", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the pavilion generator."))
	FHutongPavilionParams Params;
	// The eave derives from the bay, so from this component's footprint, not the params' own Width/Depth.
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
		return NSLOCTEXT("Hutong", "TypePath", "paved path (甬路)");
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
		return NSLOCTEXT("Hutong", "TypePassage", "covered passage (過道)");
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
		return NSLOCTEXT("Hutong", "TypeFlowerBed", "flower bed (花池)");
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
		return NSLOCTEXT("Hutong", "TypeWaterJar", "water jar (魚缸)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::WaterJar; }

	UPROPERTY(EditAnywhere, Category="Jar", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the water jar generator."))
	FHutongWaterJarParams Params;

	static void BuildWaterJarMesh(const FHutongWaterJarParams& InParams,
		UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	// Square, and derived rather than stored.
	virtual FVector2D GetFootprintSize() const override
	{
		const double S = Params.GetFootprint();
		return FVector2D(S, S);
	}

protected:
	virtual void BuildMesh(UE::Geometry::FDynamicMesh3& OutMesh, EHutongDetail Level) const override;
};

// 構架: a house's frame alone, for showing how it is built.
UCLASS(ClassGroup=Hutong, meta=(BlueprintSpawnableComponent, DisplayName="Hutong Timber Frame", PrioritizeCategories="Preset Footprint"))
class UHutongFrameBuildingComponent : public UHutongBuildingComponent
{
	GENERATED_BODY()

public:
	virtual FText GetTypeLabel() const override
	{
		return NSLOCTEXT("Hutong", "TypeFrame", "timber frame (構架)");
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

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Bay Count Override", UIMin="0", UIMax="32", ClampMin="0", ClampMax="32", ToolTip="Forces the number of bays; zero derives it from the bay width limits."))
	int32 BayCountOverride = 0;

	// Params' house with the footprint filled in.
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

		const HutongGen::FrameLayout::FLayout L = HutongGen::FrameLayout::Make(H);
		Out.ColumnRows.Add(L.Y[0]);
		if (L.bFrontVeranda) Out.ColumnRows.Add(L.Y[L.Front]);
		if (L.bRearVeranda) Out.ColumnRows.Add(L.Y[L.Rear]);
		Out.ColumnRows.Add(L.Y.Last());
		Out.ColumnRadius = 0.5 * L.D;
		Out.FootingSize = HutongCanon::Frame::BaseStoneSide * L.D;
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
		return NSLOCTEXT("Hutong", "TypeHall", "temple hall (殿)");
	}

	virtual FLinearColor GetPlanColour() const override { return HutongPlanColours::Hall; }

	UPROPERTY(EditAnywhere, Category="Hall", meta=(ShowOnlyInnerProperties, ToolTip="Parameters of the temple hall generator."))
	FHutongHallParams Params;

	virtual double GetBaseCourseTop() const override { return FMath::Max(Params.FloorHeight, 0.0) + Params.GetBaseCourseHeight(); }
	virtual void SetBaseCourseTop(double TopAboveGround) override { Params.BaseCourseHeight = FMath::Max(TopAboveGround - FMath::Max(Params.FloorHeight, 0.0), 25.0); }
	// Params with the footprint filled: the 大式 hall is sized from it.
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

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Bay Count Override", UIMin="0", UIMax="32", ClampMin="0", ClampMax="32", ToolTip="Forces the number of bays; zero derives it from the frontage."))
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
		// A temple front is symmetrical: the doors are in the middle bay.
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
