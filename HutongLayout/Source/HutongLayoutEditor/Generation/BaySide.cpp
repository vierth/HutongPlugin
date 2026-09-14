#include "Generation/BaySide.h"

namespace HutongGen::BaySide
{
	EBaySide DefaultFromCameraDelta(double Dx, double Dy)
	{
		if (FMath::Abs(Dx) > FMath::Abs(Dy))
		{
			return (Dx > 0.0) ? EBaySide::PlusX : EBaySide::MinusX;
		}
		return (Dy > 0.0) ? EBaySide::PlusY : EBaySide::MinusY;
	}

	EBaySide ClosestToPoint(double LocalX, double LocalY,
		double MinX, double MinY, double MaxX, double MaxY)
	{
		const double dMinY = FMath::Abs(LocalY - MinY);
		const double dMaxX = FMath::Abs(LocalX - MaxX);
		const double dMaxY = FMath::Abs(LocalY - MaxY);
		const double dMinX = FMath::Abs(LocalX - MinX);
		double best = dMinY;
		EBaySide side = EBaySide::MinusY;
		if (dMaxX < best) { best = dMaxX; side = EBaySide::PlusX; }
		if (dMaxY < best) { best = dMaxY; side = EBaySide::PlusY; }
		if (dMinX < best) { best = dMinX; side = EBaySide::MinusX; }
		return side;
	}

	EBaySide ClosestOnAxis(bool bFacadeAlongX, double LocalX, double LocalY,
		double MinX, double MinY, double MaxX, double MaxY)
	{
		if (bFacadeAlongX)
		{
			return (LocalY < 0.5 * (MinY + MaxY)) ? EBaySide::MinusY : EBaySide::PlusY;
		}
		return (LocalX < 0.5 * (MinX + MaxX)) ? EBaySide::MinusX : EBaySide::PlusX;
	}

	bool IsAlongX(EBaySide Side)
	{
		return Side == EBaySide::MinusY || Side == EBaySide::PlusY;
	}

	FEdge GetEdge(EBaySide Side, double MinX, double MinY, double MaxX, double MaxY)
	{
		switch (Side)
		{
		case EBaySide::MinusY: return { true,  MinY, FVector2D( 0.0, -1.0) };
		case EBaySide::PlusY:  return { true,  MaxY, FVector2D( 0.0,  1.0) };
		case EBaySide::PlusX:  return { false, MaxX, FVector2D( 1.0,  0.0) };
		case EBaySide::MinusX: return { false, MinX, FVector2D(-1.0,  0.0) };
		}
		return { true, MinY, FVector2D(0.0, -1.0) };
	}

	FVector3d RotateVertex(EBaySide Side, const FVector3d& V, double SizeX, double SizeY)
	{
		switch (Side)
		{
		case EBaySide::MinusY: return V;
		case EBaySide::PlusX:  return FVector3d(-V.Y + SizeX,  V.X,           V.Z);
		case EBaySide::PlusY:  return FVector3d(-V.X + SizeX, -V.Y + SizeY,   V.Z);
		case EBaySide::MinusX: return FVector3d( V.Y,         -V.X + SizeY,   V.Z);
		}
		return V;
	}
}
