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
	// The preset this piece's parameters came from, and the way to change what a laid-out polygon
	// will build. Pick another and the parameters are replaced where they stand: the footprint,
	// the facing and the position are the plan's, so a rectangle drawn as a 正房 becomes a 廂房
	// without being drawn again. Empty means the parameters have been tuned away from any preset.
	UPROPERTY(EditAnywhere, Category="Preset", meta=(DisplayName="Type / Preset", GetOptions="GetPresetOptions", ToolTip="Preset this building's parameters are taken from; changing it rebuilds the building in place, keeping its footprint and facing."))
	FString Preset;

	UFUNCTION()
	TArray<FString> GetPresetOptions() const;

	// The preset list this type reads, which is the tool's own key: a house's presets are the
	// house tool's. Derived from the class name (UHutong<Key>BuildingComponent), so a type gets
	// its presets with no per-type code — and renaming a component class orphans them, exactly as
	// renaming a tool's key would.
	FName GetPresetKey() const;

	// The params struct, by reflection, so one implementation loads a preset into any of the
	// fourteen types. The same lookup UHutongPresetProperties makes on a tool's property set.
	bool GetParamsForPreset(const UScriptStruct*& OutType, void*& OutData);

	// Takes a named preset's parameters, which is what picking one in the dropdown does. Also the
	// two ends of an export: the reference a record's parameters are measured against, and the
	// base the recorded differences are applied to.
	bool ApplyPresetParams(const FString& Name);

	// **What the placement is worth as evidence, and what somebody wants to say about it.** A
	// baked mesh remembers nothing and neither does a rectangle on the ground: a polygon traced
	// off the map and one put there to close a gap in a street are the same rectangle, and only
	// the placement can say which it is. Both travel in the scene file with everything else the
	// placement decided.
	UPROPERTY(EditAnywhere, Category="Metadata", meta=(DisplayName="Confidence", ToolTip="How far this placement is attested on the map and how far it is inferred; 5 is drawn on the map, 1 is not present in any source."))
	EHutongConfidence Confidence = EHutongConfidence::Attested;

	UPROPERTY(EditAnywhere, Category="Metadata", meta=(DisplayName="Notes", MultiLine=true, ToolTip="Free text about this placement: what it was read from, what is uncertain about it, what to come back to."))
	FString Notes;

	UPROPERTY(EditAnywhere, Category="Appearance", meta=(ShowOnlyInnerProperties, ToolTip="Colours and materials for each surface of this building."))
	FHutongPalette Palette;

	UPROPERTY(EditAnywhere, Category="Detail", meta=(DisplayName="Detail Level", ToolTip="How much of the building's geometry is built, from massing block to full detail."))
	EHutongDetail DetailLevel = EHutongDetail::Near;

	UPROPERTY(EditAnywhere, Category="Detail", meta=(DisplayName="Bespoke Mesh", ToolTip="Keeps a mesh of its own for this placement instead of a shared library mesh."))
	bool bBespokeMesh = false;

	UPROPERTY(EditAnywhere, Category="Detail", meta=(DisplayName="Build LOD Chain", ToolTip="Bakes the cheaper detail levels below the placed one as the mesh's own LODs."))
	bool bBuildLODChain = true;

	UPROPERTY(EditAnywhere, Category="Detail", meta=(DisplayName="Plan Only (outline, no geometry)", ToolTip="Draws only the footprint outline on the ground and builds no geometry."))
	bool bPlanOnly = false;

	UPROPERTY(VisibleAnywhere, Category="Identity", meta=(DisplayName="Building Id", ToolTip="Unique identifier of this placement, used to match it on scene import."))
	FGuid BuildingId;

	// The footprint off square. The generators only ever build the rectangle; the built mesh is
	// warped to these corners afterwards, once per LOD, in BuildLODs. Under Footprint so a
	// layout-only export carries it, and on the base so every type has it without a line of its own.
	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Corner Offsets (角偏移)", ToolTip="Moves each corner of the footprint off the rectangle so the plan can be an angled quadrilateral; the built mesh is warped to fit. Zero on all four keeps the rectangle."))
	FHutongFootprintSkew FootprintSkew;

	// The four local corners the placement actually occupies, rectangle plus offsets, from the origin anticlockwise.
	void GetFootprintCorners(FVector2D OutCorners[4]) const
	{
		HutongFootprint::Corners(GetFootprintSize(), FootprintSkew, OutCorners);
	}

	bool HasFootprintSkew() const { return !FootprintSkew.IsZero(); }

	// Where this building's 下鹼 tops out above the ground, or negative for a kind without one;
	// and setting it, so a placement snapped to a neighbour can take the neighbour's line.
	virtual double GetBaseCourseTop() const { return -1.0; }
	virtual void SetBaseCourseTop(double TopAboveGround) {}

	// The same for a caller that reaches this class by reflection, as PlaceLabels does.
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

	// The chain this placement bakes, LOD0 first.
	void BuildLODs(TArray<UE::Geometry::FDynamicMesh3>& OutLODs) const;

	// The footprint in the actor's local XY, which with the mesh origin at the rect's min corner is all anything needs to reconstruct the four world corners.
	UFUNCTION(BlueprintPure, Category="Hutong|Footprint")
	virtual FVector2D GetFootprintSize() const { return FVector2D::ZeroVector; }

	// The inverse, for the plan outline's handles.
	virtual void SetFootprintSize(const FVector2D& Size) {}

	// The eave above the ground, or zero for a piece that has none a gate could stand beside: a
	// wall's cap and a path are not the row a gate is set into.
	virtual double GetEaveHeight() const { return 0.0; }

	// The ridge above the ground, or zero as for the eave. What a gate set into this row has to
	// stand clear of.
	virtual double GetRidgeHeight() const { return 0.0; }

	// What this piece is, for the mode's hover readout.
	virtual FText GetTypeLabel() const { return NSLOCTEXT("Hutong", "TypeBuilding", "Building"); }

	// What a laid-out one is drawn in. A street of plans is a dozen rectangles on the ground and
	// the only thing distinguishing them is colour, so it goes beside the label: both answer the
	// same question about a piece that has no geometry yet.
	virtual FLinearColor GetPlanColour() const { return HutongPlanColours::Building; }

	// Whatever the component hangs on the actor besides the baked mesh — the lights, at present.
	virtual void ApplyPlacementAttachments() { ApplyPlanOutline(); }

	// Which edge of the footprint is the facade, for the plan outline's hatching.
	virtual bool GetFacade(EHutongBaySide& OutSide) const { return false; }
	// Turns the facade to another side of the same footprint, for a building placed facing the wrong way.
	virtual bool SetFacade(EHutongBaySide Side) { return false; }
	// How many quarter turns one press of [ or ] moves the facade.
	virtual int32 FacadeTurnStep() const { return 1; }

	// Where the columns divide this building's frontage, for the plan's bay divisions and the
	// hover readout's count. Empty on a type with no bays to report — a wall, a path, or a piece
	// that is one bay by construction. Every override reads the generator's own BayBoundary, so a
	// plan cannot draw a division the mesh will not build.
	virtual void GetPlanBays(FHutongPlanBays& Out) const {}

	// Which footprint axis those boundaries are measured along: the facade edge where the type has
	// one, the run otherwise.
	bool ArePlanBaysAlongX() const;

	// The openings a plan can slide along the run — a wall's 牆垣式門 and garden doorway.
	virtual void GetPlanOpenings(TArray<FHutongPlanOpening>& Out) const {}
	virtual void SetPlanOpeningCentre(int32 Index, double CentreCm) {}
	// Which footprint axis the run lies along, for a type with a run.
	virtual bool IsRunAlongY() const { return false; }

	// The other half of it, which a layout-only import needs before it can read a footprint: a
	// line-like piece takes its length off whichever extent is the run.
	virtual void SetRunAlongY(bool bAlongY) {}

	// What kind of thing this is *within* its class, where a class carries more than one: the
	// wall's role is drawn by two different tools and decides height, thickness, cap and what
	// openings are allowed, so it is type rather than parameter and a layout-only record has to
	// carry it. NAME_None on a class with only one kind in it.
	virtual FName GetTypeVariant() const { return NAME_None; }
	virtual void SetTypeVariant(FName Variant) {}

	// The variants this class can be converted into, so the conversion list can offer a 院牆 and a
	// 隔牆 as two things to turn a building into rather than one wall and a follow-up edit. Empty
	// on a class with only one kind in it.
	virtual void GetTypeVariants(TArray<FName>& Out) const {}

	// Creates, updates or removes the plan outline to match bPlanOnly.
	void ApplyPlanOutline();

	// A component that is not RF_Transactional is silently left out of every transaction: the
	// placement tools make one with a bare NewObject, and a corner dragged on it could not be undone.
	virtual void PostInitProperties() override;

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
	// The role is what the run is, so the readout and the plan say it: 院牆 and 隔牆 are the two
	// tools that draw walls, and a rectangle on the ground carries no other sign of which it is.
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

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Start Miter", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="How far the start of the run extends past the drawn rectangle to fill a corner, in cm."))
	double StartExtend = 0.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="End Miter", UIMin="0", UIMax="60", ClampMin="0", Units="cm", ToolTip="How far the end of the run extends past the drawn rectangle to fill a corner, in cm."))
	double EndExtend = 0.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Footprint Thickness", UIMin="0", ClampMin="0", Units="cm", ToolTip="Cross extent of the footprint that caps the wall's thickness, in cm; zero applies no cap."))
	double FootprintThickness = 0.0;


	// Shared by the tool's preview-time build and the component's rebuild.
	static void BuildWallMesh(const FHutongWallParams& InParams, double InLength, double InThickness,
		bool bAlongY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	// The thickness this run is actually built at. The component's own FootprintThickness is what
	// BuildMesh caps with — Params.FootprintThickness is only ever set on the local copy inside
	// BuildWallMesh — so reading the params here reported the role's figure for a compound wall
	// built on a plotted 24, to snapping, the hover outline, the plan outline and the exchange alike.
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
		// A run laid on a plotted width — a compound's cross wall on its own 24 — is thinner than
		// its role asks for, and that is a fact about the footprint. Taken only when it narrows,
		// so a footprint never widens a wall past what its role says — and dropped when the
		// footprint reaches the role's figure again, or a cap set once stood for ever and a
		// re-widened run kept reporting the narrow one to snapping, the outline and the exchange.
		const double Cross = bLengthAlongY ? S.X : S.Y;
		FootprintThickness = (Cross > 1.0 && Cross < Params.GetThickness() - 0.01) ? Cross : 0.0;
	}
	virtual bool IsRunAlongY() const override { return bLengthAlongY; }
	virtual void SetRunAlongY(bool bAlongY) override { bLengthAlongY = bAlongY; }

	// 院牆 or 隔牆 — the two wall tools' own answer, and not a parameter: it decides the height,
	// the thickness, the cap and which openings the run may carry.
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
		// The gate first, then the garden doorway, so the slider indices are stable whichever is present.
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
		// Held so the whole opening stays inside the run, with a brick either side of it.
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

	// Filled in from the footprint first: the eave derives from the bay width, and Params.Width
	// answers the struct default until then.
	virtual double GetEaveHeight() const override { return ParamsForFootprint().GetEaveHeight(); }
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
		// Params.Width is tool-driven and answers the struct's own default until the footprint
		// fills it in, and every derived figure below hangs off it — the same fill
		// BuildSiheyuanMesh does at spawn.
		FHutongSiheyuanParams P = Params;
		const bool bAlongX = HutongGen::BaySide::IsAlongX(BaySide);
		P.Width = bAlongX ? FootprintX : FootprintY;
		P.Depth = bAlongX ? FootprintY : FootprintX;
		P.BayCountOverride = BayCountOverride;

		const int32 N = P.GetBayCount();
		const double ColR = P.GetColumnRadius();
		for (int32 i = 0; i <= N; ++i) Out.Boundaries.Add(P.GetBayBoundary(i, N, P.Width, ColR));
		if (P.bHasFrontDoorCenter) Out.DoorBay = P.GetDoorBayIndex(N);
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
	virtual double GetRidgeHeight() const override { return HutongGen::Ridge::Corridor(Params); }

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="100", ClampMin="40", Units="cm", ToolTip="Length of the corridor run, in cm."))
	double Length = 800.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(UIMin="90", ClampMin="60", Units="cm", ToolTip="Clear width of the walk, in cm."))
	double Width = 150.0;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Runs the corridor along the actor's local Y axis instead of X."))
	bool bLengthAlongY = false;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Open Side Flipped", ToolTip="Turns the run end-for-end so it opens onto the other side."))
	bool bFlipOpenSide = false;

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(DisplayName="Bench Gap At", UIMin="-1", UIMax="1", ClampMin="-1", ClampMax="1", ToolTip="Where the bench breaks for a way onto the walk, as a fraction of the length; negative leaves it unbroken."))
	double BenchGapAt = -1.0;

	virtual void GetPlanBays(FHutongPlanBays& Out) const override
	{
		// A corridor's bays are not counted but fall out of the run against the 步 spacing.
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
		// A shop has no door bay: the boards come out of whichever bays are open, counted from
		// the middle, and marking one of them as the way in would say the wrong thing.
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
		// Like the shop below it, a 樓 has no door bay: the boards come out of whichever bays are open.
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

	UPROPERTY(EditAnywhere, Category="Footprint", meta=(ToolTip="Which side of the footprint carries the room's bay facade and the passage's doorway."))
	EHutongBaySide BaySide = EHutongBaySide::MinusY;

	static void BuildEarPassageMesh(const FHutongEarPassageParams& InParams, EHutongBaySide Side,
		double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Detail = EHutongDetail::Near);

	virtual bool GetFacade(EHutongBaySide& OutSide) const override { OutSide = BaySide; return true; }
	virtual bool SetFacade(EHutongBaySide Side) override { BaySide = Side; return true; }

	// The room's bay lines, plus the line where the passage strip meets it, so the plan shows the
	// way through — in the build frame, facade on -Y, the strip's end line standing in for the
	// room's end column so the division is at the gable face rather than the column's centre.
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
	virtual double GetEaveHeight() const override { return Params.GetEaveHeight(); }
	virtual double GetRidgeHeight() const override { return HutongGen::Ridge::Pavilion(Params, Depth); }

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
	virtual double GetEaveHeight() const override { return Params.GetEaveHeight(); }
	virtual double GetRidgeHeight() const override
	{
		return HutongGen::Ridge::Hall(Params,
			HutongGen::BaySide::IsAlongX(BaySide) ? FootprintY : FootprintX);
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
