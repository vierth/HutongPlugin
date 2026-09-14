#pragma once

#include "CoreMinimal.h"
#include "Algo/Reverse.h"
#include "Components/PrimitiveComponent.h"
#include "Generation/BaySide.h"
#include "Generation/HutongFootprint.h"
#include "HutongPlanOutlineComponent.generated.h"

// What each kind of piece is drawn in while it is a plan. Grouped the way the palette tabs are —
// houses warm, gates hot, enclosure cool, garden green — so a street of outlines reads as its
// arrangement rather than as a field of identical rectangles. Colour is the only thing telling one
// laid-out footprint from another: none of them has any geometry yet.
namespace HutongPlanColours
{
	// Buildings.
	inline const FLinearColor House(1.00f, 0.85f, 0.20f, 1.0f);      // 正房 and its family
	inline const FLinearColor Shopfront(1.00f, 0.70f, 0.30f, 1.0f);  // 鋪面房
	// 樓, the shop with a storey over it: the shopfront's hue carried warmer, since on a plan the
	// two are the same rectangle and the difference is what stands on it.
	inline const FLinearColor Storey(1.00f, 0.55f, 0.15f, 1.0f);     // 樓
	inline const FLinearColor Hall(0.78f, 0.55f, 1.00f, 1.0f);       // 殿
	inline const FLinearColor Pavilion(0.62f, 0.68f, 1.00f, 1.0f);   // 亭
	// 耳房 with its 過道: the house's yellow pulled toward the passage's green.
	inline const FLinearColor EarPassage(0.88f, 0.90f, 0.35f, 1.0f); // 耳房過道

	// Gates and screens.
	inline const FLinearColor Gate(1.00f, 0.45f, 0.20f, 1.0f);       // 大門
	inline const FLinearColor InnerGate(1.00f, 0.60f, 0.50f, 1.0f);  // 垂花門
	inline const FLinearColor Paifang(0.95f, 0.40f, 0.80f, 1.0f);    // 牌坊
	inline const FLinearColor Screen(0.25f, 0.85f, 0.80f, 1.0f);     // 影壁

	// Enclosure.
	inline const FLinearColor Wall(0.55f, 0.72f, 0.95f, 1.0f);       // 院牆, onto the lane
	// A 隔牆 is the same generator and a different building: on a plan the two are told apart by
	// nothing else, since a run of wall is a rectangle whatever it bounds. Deeper and greener than
	// the boundary wall's blue, so the enclosure still reads as one family.
	inline const FLinearColor CourtWall(0.35f, 0.85f, 0.88f, 1.0f);  // 隔牆, inside the compound
	inline const FLinearColor Corridor(0.40f, 0.90f, 0.55f, 1.0f);   // 遊廊
	inline const FLinearColor Passage(0.60f, 0.85f, 0.45f, 1.0f);    // 過道
	inline const FLinearColor Path(0.78f, 0.78f, 0.72f, 1.0f);       // 甬路

	// Garden.
	inline const FLinearColor FlowerBed(0.35f, 0.80f, 0.35f, 1.0f);  // 花池
	inline const FLinearColor WaterJar(0.30f, 0.72f, 1.00f, 1.0f);   // 魚缸

	// A type that has not said.
	inline const FLinearColor Building(1.00f, 0.85f, 0.20f, 1.0f);
}

// What the plan knows about a building's bays: where the columns divide the frontage, along the
// bay axis in footprint coordinates with both ends included, so the number of bays is one less
// than the number of boundaries. Read out of the generators' own BayBoundary, or a plan draws a
// division the building will not build.
struct FHutongPlanBays
{
	TArray<double> Boundaries;
	// The bay the front door is in, as an index into the spans between boundaries.
	int32 DoorBay = INDEX_NONE;
};

namespace HutongGen::PlanBays
{
	// A facade generator builds with the front on -Y and the tool turns the finished mesh, so the
	// bay boundaries turn with it: mapped through the same RotateVertex, and reversed when the turn
	// reverses their order — which takes the door bay's index with it.
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

// Whether laid-out buildings draw their footprints at all. One switch for the whole editor, not a
// property on each placement: what it answers is "am I working on the plans right now", and the
// polygons are in the way of anything else drawn on the same ground — PlaceLabels' regions above
// all. Mirrored by the console variable hutong.ShowPlanOutlines, which is how another plugin
// offers the switch without linking to this one, and persisted in the editor's per-project ini.
//
// Hidden takes the fill's hit proxy with it, so a laid-out building cannot be clicked either. That
// is the point rather than a side effect: an outline that still swallows clicks is still in the way.
namespace HutongPlanOutline
{
	bool ArePlansVisible();
	void SetPlansVisible(bool bVisible);

	// Called once from the module's startup, since the console variable's own default is on.
	void LoadVisibilityFromConfig();
}

// The drawing of a building that has been laid out but not built.
UCLASS(ClassGroup=Hutong, meta=(DisplayName="Hutong Plan Outline"))
class UHutongPlanOutlineComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UHutongPlanOutlineComponent();

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

	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(ToolTip="Colour the outline is drawn in, which says what kind of piece this is."))
	FLinearColor Colour = HutongPlanColours::Building;


	void SetPlan(const FVector2D& InFootprint, const FHutongFootprintSkew& InSkew, bool bInHasFacade, EHutongBaySide InFacade,
		const TArray<FVector2D>& InOpenings, bool bInRunAlongY,
		const FHutongPlanBays& InBays, bool bInBaysAlongX, const FLinearColor& InColour);

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
};
