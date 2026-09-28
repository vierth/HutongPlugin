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
	// Buildings.
	inline const FLinearColor House(1.00f, 0.85f, 0.20f, 1.0f);      // 正房 and its family
	inline const FLinearColor Shopfront(1.00f, 0.70f, 0.30f, 1.0f);  // 鋪面房
	// Shopfront hue, warmer: on a plan the two are the same rectangle.
	inline const FLinearColor Storey(1.00f, 0.55f, 0.15f, 1.0f);     // 樓
	inline const FLinearColor Hall(0.78f, 0.55f, 1.00f, 1.0f);       // 殿
	inline const FLinearColor Pavilion(0.62f, 0.68f, 1.00f, 1.0f);   // 亭
	// House yellow toward bare timber.
	inline const FLinearColor Frame(0.80f, 0.62f, 0.40f, 1.0f);      // 構架
	// House yellow toward the passage's green.
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
	inline const FLinearColor Corridor(0.40f, 0.90f, 0.55f, 1.0f);   // 遊廊
	inline const FLinearColor Passage(0.60f, 0.85f, 0.45f, 1.0f);    // 過道
	inline const FLinearColor Path(0.78f, 0.78f, 0.72f, 1.0f);       // 甬路

	// Garden.
	inline const FLinearColor FlowerBed(0.35f, 0.80f, 0.35f, 1.0f);  // 花池
	inline const FLinearColor WaterJar(0.30f, 0.72f, 1.00f, 1.0f);   // 魚缸

	// Fallback for a type with no colour.
	inline const FLinearColor Building(1.00f, 0.85f, 0.20f, 1.0f);
}

// Bay boundaries along the bay axis in footprint coords, both ends included (bays = boundaries - 1).
// Read from the generators' BayBoundary so the plan matches the build.
struct FHutongPlanBays
{
	TArray<double> Boundaries;
	// The bay the front door is in, as an index into the spans between boundaries.
	int32 DoorBay = INDEX_NONE;
	// Column rows across the depth, footprint coords; a column at every boundary on each. Empty = none drawn.
	TArray<double> ColumnRows;
	double ColumnRadius = 0.0;
	// 柱頂石: the side of the square base stone under each column.
	double FootingSize = 0.0;
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
		for (double& R : Bays.ColumnRows)
		{
			const FVector3d V = BaySide::RotateVertex(Side, FVector3d(0.0, R, 0.0), SizeX, SizeY);
			R = bAlongX ? V.Y : V.X;
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
}

// The drawing of a building that has been laid out but not built.
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

	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(ToolTip="Column lines across the depth, in cm; a column stands at every bay boundary on each."))
	TArray<double> ColumnRows;

	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(ToolTip="Radius of the columns drawn on the plan, in cm."))
	double ColumnRadius = 0.0;

	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(ToolTip="Side of the square base stone (柱頂石) under each column, in cm."))
	double FootingSize = 0.0;

	UPROPERTY(VisibleAnywhere, Category="Plan", meta=(ToolTip="Colour the outline is drawn in."))
	FLinearColor Colour = HutongPlanColours::Building;


	void SetPlan(const FVector2D& InFootprint, const FHutongFootprintSkew& InSkew, bool bInHasFacade, EHutongBaySide InFacade,
		const TArray<FVector2D>& InOpenings, bool bInRunAlongY,
		const FHutongPlanBays& InBays, bool bInBaysAlongX, const FLinearColor& InColour);

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
};
