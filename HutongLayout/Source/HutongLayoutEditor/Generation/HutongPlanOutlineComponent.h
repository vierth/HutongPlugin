#pragma once

#include "CoreMinimal.h"
#include "Algo/Reverse.h"
#include "Components/PrimitiveComponent.h"
#include "Generation/BaySide.h"
#include "Generation/HutongFootprint.h"
#include "HutongPlanOutlineComponent.generated.h"

// Plan outline colour per type, the only thing telling plans apart. Grouped like the palette
// tabs: houses warm, gates hot, enclosure cool, garden green.
namespace HutongPlanColours
{
	// Selection, drawn under the type colour's outline: white is no type's hue.
	inline const FLinearColor Selected(1.00f, 1.00f, 1.00f, 1.0f);

	// Buildings.
	inline const FLinearColor House(0.05f, 0.12f, 0.45f, 1.0f);      // 正房 and its family: dark blue, reads on the paper
	inline const FLinearColor Shopfront(1.00f, 0.70f, 0.30f, 1.0f);  // 鋪面房
	// Shopfront hue, warmer: on a plan the two are the same rectangle.
	inline const FLinearColor Storey(1.00f, 0.55f, 0.15f, 1.0f);     // 樓
	inline const FLinearColor Hall(0.78f, 0.55f, 1.00f, 1.0f);       // 殿
	inline const FLinearColor Pavilion(0.62f, 0.68f, 1.00f, 1.0f);   // 亭
	// Bare timber.
	inline const FLinearColor Frame(0.80f, 0.62f, 0.40f, 1.0f);      // 構架
	// Yellow toward the passage's green.
	inline const FLinearColor EarPassage(0.88f, 0.90f, 0.35f, 1.0f); // 耳房過道

	// Gates and screens.
	inline const FLinearColor Gate(1.00f, 0.45f, 0.20f, 1.0f);       // 大門
	inline const FLinearColor InnerGate(1.00f, 0.60f, 0.50f, 1.0f);  // 垂花門
	inline const FLinearColor Paifang(0.95f, 0.40f, 0.80f, 1.0f);    // 牌坊
	inline const FLinearColor Screen(0.25f, 0.85f, 0.80f, 1.0f);     // 影壁

	// Enclosure.
	inline const FLinearColor Wall(0.55f, 0.72f, 0.95f, 1.0f);       // 院牆, onto the lane
	// Same generator as 院牆, told apart only by colour; greener than its blue, same family.
	inline const FLinearColor CourtWall(0.35f, 0.85f, 0.88f, 1.0f);  // 隔牆, inside the compound
	// 城牆: the lane wall's blue, darker and greyer — the same family at city scale.
	inline const FLinearColor CityWall(0.40f, 0.48f, 0.78f, 1.0f);   // 城牆
	inline const FLinearColor Corridor(0.40f, 0.90f, 0.55f, 1.0f);   // 遊廊
	inline const FLinearColor Passage(0.60f, 0.85f, 0.45f, 1.0f);    // 過道
	inline const FLinearColor Path(0.78f, 0.78f, 0.72f, 1.0f);       // 甬路

	// Garden.
	inline const FLinearColor FlowerBed(0.35f, 0.80f, 0.35f, 1.0f);  // 花池
	inline const FLinearColor WaterJar(0.30f, 0.72f, 1.00f, 1.0f);   // 魚缸

	// 小房: the house's blue, greyed — a building, but a minor one.
	inline const FLinearColor SmallBuilding(0.45f, 0.55f, 0.72f, 1.0f); // 小房

	// Traced, type not known: cool grey, no family's hue, cooler than the path.
	inline const FLinearColor Unknown(0.60f, 0.60f, 0.68f, 1.0f);   // 未知

	// Fallback for a type with no colour.
	inline const FLinearColor Building(1.00f, 0.85f, 0.20f, 1.0f);

	// Drawing layer per type, bottom first: plans of two types on one spot are drawn apart in height,
	// so depth decides, not the engine's draw order. Like types share a colour, a tie there is not seen.
	// Ground first, then buildings large to small, enclosure and garden on top.
	inline const FLinearColor* const Layers[] = {
		&Path, &Unknown, &CityWall, &Hall, &House, &Storey, &Shopfront, &Frame, &SmallBuilding, &Pavilion, &Corridor, &EarPassage, &Passage,
		&Gate, &InnerGate, &Paifang, &Screen, &Wall, &CourtWall, &FlowerBed, &WaterJar, &Building };
	inline constexpr int32 LayerCount = UE_ARRAY_COUNT(Layers);

	// A colour on no list draws on top.
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
// Read from the generators' BayBoundary so the plan matches the build.
struct FHutongPlanBays
{
	TArray<double> Boundaries;
	// The bay the front door is in, as an index into the spans between boundaries.
	int32 DoorBay = INDEX_NONE;
	// Column radius: the end-bay skew seam stops at the first inner column's face.
	double ColumnRadius = 0.0;
};

namespace HutongGen::PlanBays
{
	// Generators build front on -Y and the mesh is turned; boundaries go through the same
	// RotateVertex, reversed (with the door index) when the turn reverses their order.
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

// Editor-wide switch for plan outlines, not per placement. Mirrored by the cvar
// hutong.ShowPlanOutlines (lets other plugins toggle it without linking); saved in the per-project ini.
// Hidden also removes the hit proxy, deliberately: a hidden outline must not swallow clicks.
namespace HutongPlanOutline
{
	bool ArePlansVisible();
	void SetPlansVisible(bool bVisible);

	// Called once at module startup; the cvar defaults to on.
	void LoadVisibilityFromConfig();

	// Off (default): plans draw in the world's depth, so a built building hides those behind it.
	bool ArePlansOverBuildings();
	void SetPlansOverBuildings(bool bOver);

	// Module startup and shutdown: the per-world plan layers (UHutongPlanLayerComponent) follow the plans.
	void StartLayers();
	void StopLayers();

	// The layers' material, made on first use; null under -nullrhi.
	UMaterial* GetPlanMaterial();
	// Whether World's layer is up and has drawn every change so far (tests).
	bool IsLayerCurrent(const UWorld* World);
}

// A building that has been laid out but not built. Its own proxy draws only while selected (the layer then
// leaves it out) and in the hit-proxy pass (a click selects the building); every other plan in the world
// is drawn by the world's one UHutongPlanLayerComponent — a drawing proxy per plan took the frame on a city map
// from 8 to 67 ms, all render and RHI thread (user, 2026-10-06).
UCLASS(ClassGroup=Hutong, meta=(DisplayName="Hutong Plan Outline"))
class UHutongPlanOutlineComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UHutongPlanOutlineComponent();

	// Editor-module class: stripped at cook, like the building component.
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

	// This plan's footprint in world space, corners in order.
	void GetWorldQuad(FVector OutCorners[4]) const;
	// Drawing height step: the type's layer (HutongPlanColours::Layers).
	int32 GetStackLevel() const;
	// Neighbours' edges that win over this plan's where they coincide: a shared edge is drawn once,
	// in the winner's colour.
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
};
