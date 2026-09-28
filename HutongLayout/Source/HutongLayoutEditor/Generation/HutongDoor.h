#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongDoorStone.h"

namespace HutongGen
{
	namespace Passage
	{
		// Clear height, 門檻 top to head underside.
		inline constexpr double MinClearHeight = HutongCanon::Openings::MinClearHeightCm;

		// Lowest allowed head underside.
		inline double MinHeadZ(double FloorZ, double ThresholdHeight)
		{
			return FloorZ + FMath::Clamp(ThresholdHeight, 0.0, 40.0) + MinClearHeight;
		}
	}

	// Doorway woodwork: 抱框 jambs, head, 門檻, leaves.
	struct FHutongDoorAssembly
	{
		// Clear opening; jambs go outside it.
		double OpeningX0 = 0.0;
		double OpeningX1 = 150.0;

		// Wall faces. Leaves swing toward FrontY.
		double FrontY = 0.0;
		double BackY = 30.0;

		double BottomZ = 0.0;
		// Leaf top and jamb top; the caller fills the transom between.
		double LeafTopZ = 210.0;
		double JambTopZ = 240.0;

		double FrameThickness = 9.0;
		double ThresholdHeight = 12.0;

		// 門枕石 reach into the opening past the jamb.
		double StoneReveal = 0.0;

		double MinClearHeight = Passage::MinClearHeight;

		// Mesh is its own collision: shut leaves block the doorway.
		bool bLeavesOpen = true;

		// Clear wall each leaf can fold back onto.
		double SwingClearance = 1.0e6;

		// 門簪 pegs through the head.
		int32 PegCount = 0;

		// Use explicit leaf angles instead of folding flat.
		bool bUseLeafAngles = false;
		double LeftLeafAngleDeg = 0.0;
		double RightLeafAngleDeg = -85.0;
	};

	// Frame in whatever slot the caller's scope gives; leaves tag themselves 門漆.
	void AppendDoorAssembly(UE::Geometry::FDynamicMesh3& Mesh, const FHutongDoorAssembly& D);

	// Free width a body has through the doorway as AppendDoorAssembly builds it: between the leaves at their
	// angles (bUseLeafAngles) — thickness, rails and the pivot's set-in counted — and the 門枕石 reveal.
	double LeafClearWidth(const FHutongDoorAssembly& D);

	// 抱鼓石: drum on a plinth at a gate jamb.
	void AppendDrumStone(
		UE::Geometry::FDynamicMesh3& Mesh,
		double X0, double X1,          // across the wall
		double Y0, double Y1,          // through it
		double BaseZ,                  // 0 for a wall, 臺基 top for a gate house
		double Height,
		double DrumFraction,           // drum diameter as a fraction of Height
		int32 Sides = 16);

	// 門墩 overshoot into the opening past its jamb.
	double DoorStoneReveal(double JambThickness, double DoorWidth);

	// 門墩 pair at a doorway's jambs, either form.
	void AppendDoorStonePair(
		UE::Geometry::FDynamicMesh3& Mesh,
		const FHutongDoorStoneParams& Stones,
		double ClearX0, double ClearX1,
		double FrontY, double BackY,
		double BaseZ,
		double JambThickness,
		double MaxOutward,
		double MinProjection = 0.0);
}
