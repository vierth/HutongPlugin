#pragma once

#include "CoreMinimal.h"
#include "VectorTypes.h"
#include "BaySide.generated.h"

// Which side of the footprint carries the bay facade.
UENUM()
enum class EHutongBaySide : uint8
{
	MinusY = 0 UMETA(ToolTip="The facade is on the footprint's -Y side."),
	PlusX  = 1 UMETA(ToolTip="The facade is on the footprint's +X side."),
	PlusY  = 2 UMETA(ToolTip="The facade is on the footprint's +Y side."),
	MinusX = 3 UMETA(ToolTip="The facade is on the footprint's -X side."),
};

namespace HutongGen
{
	using EBaySide = EHutongBaySide;

	namespace BaySide
	{
		EBaySide DefaultFromCameraDelta(double Dx, double Dy);

		EBaySide ClosestToPoint(double LocalX, double LocalY,
			double MinX, double MinY, double MaxX, double MaxY);

		// The nearer of the two sides whose facade runs along the given axis; the other pair is not offered.
		EBaySide ClosestOnAxis(bool bFacadeAlongX, double LocalX, double LocalY,
			double MinX, double MinY, double MaxX, double MaxY);

		bool IsAlongX(EBaySide Side);

		// The other side of the same axis.
		inline EBaySide Opposite(EBaySide Side) { return (EBaySide)((uint8)Side ^ 2); }

		struct FEdge
		{
			bool      bAlongX;
			double    FixedCoord;
			FVector2D OutDir;
		};
		FEdge GetEdge(EBaySide Side, double MinX, double MinY, double MaxX, double MaxY);

		FVector3d RotateVertex(EBaySide Side, const FVector3d& V, double SizeX, double SizeY);
	}
}
