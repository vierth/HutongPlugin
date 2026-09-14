#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongDoorStone.h"

namespace HutongGen
{
	namespace Passage
	{
		// The clear height a doorway keeps, from the top of the 門檻 to the underside of the head.
		inline constexpr double MinClearHeight = HutongCanon::Openings::MinClearHeightCm;

		// Lowest the underside of a door head may sit, given its floor and its 門檻.
		inline double MinHeadZ(double FloorZ, double ThresholdHeight)
		{
			return FloorZ + FMath::Clamp(ThresholdHeight, 0.0, 40.0) + MinClearHeight;
		}
	}

	// A doorway's woodwork: 抱框 jambs, a head over the leaves, a 門檻 and the leaves themselves.
	struct FHutongDoorAssembly
	{
		// The clear doorway. Jambs are appended *outside* this, so it is what you walk through.
		double OpeningX0 = 0.0;
		double OpeningX1 = 150.0;

		// Faces of the wall the door sits in. Leaves swing out toward FrontY.
		double FrontY = 0.0;
		double BackY = 30.0;

		double BottomZ = 0.0;
		// Top of the leaves, and top of the jambs. Everything between is the caller's transom.
		double LeafTopZ = 210.0;
		double JambTopZ = 240.0;

		double FrameThickness = 9.0;
		double ThresholdHeight = 12.0;

		// How far the 門枕石 comes into the opening past the jamb.
		double StoneReveal = 0.0;

		// Clear height held under the head.
		double MinClearHeight = Passage::MinClearHeight;

		// The mesh is its own collision, so shut leaves are a wall the player cannot walk through.
		bool bLeavesOpen = true;

		// How much clear wall each leaf has to fold back onto.
		double SwingClearance = 1.0e6;

		// 門簪: the pegs through the head.
		int32 PegCount = 0;

		// Drives each leaf's angle directly instead of folding both flat.
		bool bUseLeafAngles = false;
		double LeftLeafAngleDeg = 0.0;
		double RightLeafAngleDeg = -85.0;
	};

	// OutLeafFirstTriangle receives the triangle the leaves start at — everything from there on is door rather than frame.
	void AppendDoorAssembly(UE::Geometry::FDynamicMesh3& Mesh, const FHutongDoorAssembly& D,
		int32* OutLeafFirstTriangle = nullptr);

	// 抱鼓石: a round drum on a plinth at the foot of a gate jamb.
	void AppendDrumStone(
		UE::Geometry::FDynamicMesh3& Mesh,
		double X0, double X1,          // across the wall
		double Y0, double Y1,          // through it
		double BaseZ,                  // what it stands on: 0 for a wall, the 臺基 for a gate house
		double Height,
		double DrumFraction,           // drum diameter as a fraction of Height
		int32 Sides = 16);

	// How far a 門墩 overshoots into the opening past its jamb.
	double DoorStoneReveal(double JambThickness, double DoorWidth);

	// The pair of 門墩 at the foot of a doorway's jambs, either form.
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
