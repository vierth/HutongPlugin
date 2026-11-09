#pragma once

#include "CoreMinimal.h"
#include "Algo/Reverse.h"
#include "Components/PrimitiveComponent.h"
#include "Generation/BaySide.h"
#include "Generation/HutongFootprint.h"
#include "UObject/ObjectKey.h"
#include "HutongPlanOutlineComponent.generated.h"

// Dark and saturated to read on the light paper map.
namespace HutongPlanColours
{
	inline const FLinearColor Selected(1.00f, 1.00f, 1.00f, 1.0f);

	inline const FLinearColor House(0.04f, 0.09f, 0.36f, 1.0f);
	inline const FLinearColor Shopfront(0.80f, 0.38f, 0.00f, 1.0f);
	inline const FLinearColor Storey(0.60f, 0.22f, 0.00f, 1.0f);
	inline const FLinearColor Hall(0.42f, 0.12f, 0.62f, 1.0f);
	inline const FLinearColor Pavilion(0.25f, 0.25f, 0.72f, 1.0f);
	inline const FLinearColor Frame(0.42f, 0.27f, 0.12f, 1.0f);
	inline const FLinearColor EarPassage(0.45f, 0.45f, 0.00f, 1.0f);

	inline const FLinearColor Gate(0.85f, 0.00f, 0.00f, 1.0f);
	inline const FLinearColor InnerGate(0.72f, 0.00f, 0.36f, 1.0f);
	inline const FLinearColor Paifang(0.58f, 0.00f, 0.58f, 1.0f);
	inline const FLinearColor Screen(0.00f, 0.40f, 0.36f, 1.0f);

	inline const FLinearColor Wall(0.10f, 0.22f, 0.62f, 1.0f);
	inline const FLinearColor CourtWall(0.00f, 0.42f, 0.68f, 1.0f);
	inline const FLinearColor CityWall(0.20f, 0.24f, 0.40f, 1.0f);
	inline const FLinearColor PalaceWall(0.48f, 0.08f, 0.16f, 1.0f);
	inline const FLinearColor ImperialWall(0.62f, 0.20f, 0.06f, 1.0f);
	inline const FLinearColor Corridor(0.00f, 0.45f, 0.22f, 1.0f);
	inline const FLinearColor Passage(0.30f, 0.45f, 0.00f, 1.0f);
	inline const FLinearColor Path(0.33f, 0.31f, 0.28f, 1.0f);

	inline const FLinearColor FlowerBed(0.15f, 0.55f, 0.00f, 1.0f);
	inline const FLinearColor WaterJar(0.00f, 0.30f, 0.80f, 1.0f);

	inline const FLinearColor SmallBuilding(0.20f, 0.26f, 0.44f, 1.0f);

	inline const FLinearColor Unknown(0.28f, 0.28f, 0.33f, 1.0f);

	inline const FLinearColor Building(0.60f, 0.50f, 0.00f, 1.0f);

	// Plans of two types on one spot are drawn apart in height by this order, bottom first.
	inline const FLinearColor* const Layers[] = {
		&Path, &Unknown, &CityWall, &PalaceWall, &ImperialWall, &Hall, &House, &Storey, &Shopfront, &Frame, &SmallBuilding, &Pavilion, &Corridor, &EarPassage, &Passage,
		&Gate, &InnerGate, &Paifang, &Screen, &Wall, &CourtWall, &FlowerBed, &WaterJar, &Building };
	inline constexpr int32 LayerCount = UE_ARRAY_COUNT(Layers);

	inline int32 LayerOf(const FLinearColor& Colour)
	{
		for (int32 i = 0; i < LayerCount; ++i)
		{
			if (*Layers[i] == Colour) return i;
		}
		return LayerCount;
	}
}

// Bay boundaries along the bay axis in footprint coords, both ends included (bays = boundaries - 1).
struct FHutongPlanBays
{
	TArray<double> Boundaries;
	int32 DoorBay = INDEX_NONE;
	// Column radius: the end-bay skew seam stops at the first inner column's face.
	double ColumnRadius = 0.0;
};

namespace HutongGen::PlanBays
{
	// Boundaries take the mesh's RotateVertex turn, reversed with the door index when it flips their order.
	inline void OntoFacade(FHutongPlanBays& Bays, EHutongBaySide Side, double SizeX, double SizeY)
	{
		const bool bAlongX = BaySide::IsAlongX(Side);
		for (double& T : Bays.Boundaries)
		{
			const FVector3d V = BaySide::RotateVertex(Side, FVector3d(T, 0.0, 0.0), SizeX, SizeY);
			T = bAlongX ? V.X : V.Y;
		}
		if (Bays.Boundaries.Num() >= 2 && Bays.Boundaries[0] > Bays.Boundaries.Last())
		{
			Algo::Reverse(Bays.Boundaries);
			if (Bays.DoorBay != INDEX_NONE)
			{
				Bays.DoorBay = Bays.Boundaries.Num() - 2 - Bays.DoorBay;
			}
		}
	}
}

// Mirrored by the cvar hutong.ShowPlanOutlines so other plugins toggle it without linking.
namespace HutongPlanOutline
{
	bool ArePlansVisible();
	void SetPlansVisible(bool bVisible);

	void LoadVisibilityFromConfig();

	bool ArePlansOverBuildings();
	void SetPlansOverBuildings(bool bOver);

	void StartLayers();
	void StopLayers();

	UMaterial* GetPlanMaterial();
	bool IsLayerCurrent(const UWorld* World);
}

// A building that has been laid out but not built. Its own proxy draws only while selected (the layer then
// hides it) and takes the clicks then; every other plan in the world is drawn, and hit-tested, by the world's
// one UHutongPlanLayerComponent — a drawing proxy per plan took the frame on a city map from 8 to 67 ms, all
// render and RHI thread (user, 2026-10-06), and one hit-proxy mesh per plan drew 10k of them on every mouse move.
UCLASS(ClassGroup=Hutong, meta=(DisplayName="Hutong Plan Outline"))
class UHutongPlanOutlineComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UHutongPlanOutlineComponent();

	virtual bool IsEditorOnly() const override { return true; }

	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(ToolTip="Size of the footprint in the actor's local frame, in cm."))
	FVector2D Footprint = FVector2D(100.0, 100.0);

	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(DisplayName="Corner Offsets (角偏移)", ToolTip="How far each corner of the footprint is moved off the rectangle, in cm."))
	FHutongFootprintSkew Skew;

	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(ToolTip="Whether the outline marks a facade edge."))
	bool bHasFacade = false;

	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(ToolTip="Which side of the footprint the facade is on."))
	EHutongBaySide Facade = EHutongBaySide::MinusY;

	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(ToolTip="Openings along the run, each as centre and width in cm."))
	TArray<FVector2D> Openings;

	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(ToolTip="Whether the run lies along the actor's local Y rather than X."))
	bool bRunAlongY = false;

	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(ToolTip="Where the columns divide the frontage, in cm along the bay axis."))
	TArray<double> BayBoundaries;

	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(ToolTip="Whether the bays are counted along the footprint's local X rather than Y."))
	bool bBaysAlongX = true;

	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(ToolTip="Which bay carries the front door; -1 where the type has none."))
	int32 DoorBay = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(ToolTip="Colour the outline is drawn in."))
	FLinearColor Colour = HutongPlanColours::Building;

	// Square marks with an arrow (a city wall's ramp, pointing up it): centre X, Y and the arrow's direction
	// X, Y in footprint coords. Set before SetPlan, which redraws.
	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(ToolTip="Marks with an arrow, such as a ramp's direction up the wall."))
	TArray<FVector4> ArrowMarks;


	void SetPlan(const FVector2D& InFootprint, const FHutongFootprintSkew& InSkew, bool bInHasFacade, EHutongBaySide InFacade,
		const TArray<FVector2D>& InOpenings, bool bInRunAlongY,
		const FHutongPlanBays& InBays, bool bInBaysAlongX, const FLinearColor& InColour);

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	// A mesh batch whose material the proxy did not declare is dropped (with an ensure).
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;

	void GetWorldQuad(FVector OutCorners[4]) const;
	int32 GetStackLevel() const;
	// A shared edge is drawn once, in the winner's colour.
	TArray<TPair<FVector, FVector>> CollectWinningNeighbourEdges() const;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport) override;
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
	virtual void DestroyRenderState_Concurrent() override;
};

// Every plan of one world in one proxy: fills and lines in one vertex buffer, built when a plan changes
// and drawn as one batch. Lines are quads widened to their pixel width in the vertex shader, so the
// buffer holds at any zoom. Transient, owned by no actor, registered with the world like its line batcher.
UCLASS(Transient)
class UHutongPlanLayerComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UHutongPlanLayerComponent();

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	// A mesh batch whose material the proxy did not declare is dropped (with an ensure).
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const override;

	void SendSelection();

	TMap<TObjectKey<UHutongPlanOutlineComponent>, int32> PlanIndices;
};
