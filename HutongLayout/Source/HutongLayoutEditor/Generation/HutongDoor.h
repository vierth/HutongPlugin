#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongDoorStone.h"

namespace HutongGen
{
	namespace Passage
	{
		inline constexpr double MinClearHeight = HutongCanon::Openings::MinClearHeightCm;

		inline double MinHeadZ(double FloorZ, double ThresholdHeight)
		{
			return FloorZ + FMath::Clamp(ThresholdHeight, 0.0, 40.0) + MinClearHeight;
		}
	}

	struct FHutongDoorAssembly
	{
		double OpeningX0 = 0.0;
		double OpeningX1 = 150.0;

		double FrontY = 0.0;
		double BackY = 30.0;

		double BottomZ = 0.0;
		double LeafTopZ = 210.0;
		double JambTopZ = 240.0;

		double FrameThickness = 9.0;
		double ThresholdHeight = 12.0;

		double StoneReveal = 0.0;

		double MinClearHeight = Passage::MinClearHeight;

		// Mesh is its own collision: shut leaves block the doorway.
		bool bLeavesOpen = true;

		double SwingClearance = 1.0e6;

		int32 PegCount = 0;

		bool bUseLeafAngles = false;
		double LeftLeafAngleDeg = 0.0;
		double RightLeafAngleDeg = -85.0;
	};

	// Frame in whatever slot the caller's scope gives; leaves tag themselves 門漆.
	void AppendDoorAssembly(UE::Geometry::FDynamicMesh3& Mesh, const FHutongDoorAssembly& D);

	double LeafClearWidth(const FHutongDoorAssembly& D);

	void AppendDrumStone(
		UE::Geometry::FDynamicMesh3& Mesh,
		double X0, double X1,          // across the wall
		double Y0, double Y1,          // through it
		double BaseZ,                  // 0 for a wall, 臺基 top for a gate house
		double Height,
		double DrumFraction,           // drum diameter as a fraction of Height
		int32 Sides = 16);

	double DoorStoneReveal(double JambThickness, double DoorWidth);

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
