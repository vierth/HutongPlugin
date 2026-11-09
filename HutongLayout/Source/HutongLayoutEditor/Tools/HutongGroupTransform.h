#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongFootprint.h"

namespace HutongGroupTransform
{
	// Yaw the group box is drawn in: the members' shared bearing, quarter turns folded, else world axes.
	inline double FrameYaw(const TArray<double>& YawsDeg, double ToleranceDeg = 0.5)
	{
		if (YawsDeg.IsEmpty()) return 0.0;
		const double First = FMath::Fmod(FMath::Fmod(YawsDeg[0], 90.0) + 90.0, 90.0);
		for (const double Yaw : YawsDeg)
		{
			const double Folded = FMath::Fmod(FMath::Fmod(Yaw, 90.0) + 90.0, 90.0);
			const double Diff = FMath::Abs(Folded - First);
			if (FMath::Min(Diff, 90.0 - Diff) > ToleranceDeg) return 0.0;
		}
		return First;
	}

	inline FVector2D ToFrame(const FVector& World, double FrameYawDeg)
	{
		const FVector L = FRotator(0.0, -FrameYawDeg, 0.0).RotateVector(FVector(World.X, World.Y, 0.0));
		return FVector2D(L.X, L.Y);
	}

	inline FVector FromFrame(const FVector2D& Frame, double FrameYawDeg, double Z)
	{
		const FVector W = FRotator(0.0, FrameYawDeg, 0.0).RotateVector(FVector(Frame.X, Frame.Y, 0.0));
		return FVector(W.X, W.Y, Z);
	}

	inline FTransform RotateAbout(const FTransform& Start, const FVector& Pivot, double DeltaYawDeg)
	{
		const FRotator Delta(0.0, DeltaYawDeg, 0.0);
		const FVector Offset = Start.GetLocation() - Pivot;
		const FVector Turned = Delta.RotateVector(FVector(Offset.X, Offset.Y, 0.0));
		FTransform Out = Start;
		Out.SetLocation(FVector(Pivot.X + Turned.X, Pivot.Y + Turned.Y, Start.GetLocation().Z));
		Out.SetRotation((Delta.Quaternion() * Start.GetRotation()).GetNormalized());
		return Out;
	}

	// A run keeps its thickness: its short side keeps a factor of 1.
	inline FVector2D SideFactors(double MemberYawDeg, double FrameYawDeg, const FVector2D& Factors, const FVector2D& Size, bool bKeepShortSide)
	{
		const double Phi = FMath::DegreesToRadians(MemberYawDeg - FrameYawDeg);
		const FVector2D U(FMath::Cos(Phi), FMath::Sin(Phi)), V(-FMath::Sin(Phi), FMath::Cos(Phi));
		FVector2D Out(FVector2D(Factors.X * U.X, Factors.Y * U.Y).Size(), FVector2D(Factors.X * V.X, Factors.Y * V.Y).Size());
		if (bKeepShortSide) (Size.X < Size.Y ? Out.X : Out.Y) = 1.0;
		return Out;
	}

	inline FVector ScaledCentre(const FVector& Centre, double FrameYawDeg, const FVector2D& Anchor, const FVector2D& Factors)
	{
		const FVector2D C = ToFrame(Centre, FrameYawDeg);
		return FromFrame(Anchor + (C - Anchor) * Factors, FrameYawDeg, Centre.Z);
	}

	inline FVector FootprintCentre(const FTransform& Xf, const FVector2D& Size)
	{
		return Xf.TransformPosition(FVector(0.5 * Size.X, 0.5 * Size.Y, 0.0));
	}

	inline FVector OriginForCentre(const FTransform& Xf, const FVector& Centre, const FVector2D& Size)
	{
		const FVector Half = Xf.TransformVector(FVector(0.5 * Size.X, 0.5 * Size.Y, 0.0));
		return FVector(Centre.X - Half.X, Centre.Y - Half.Y, Xf.GetLocation().Z);
	}

	inline FHutongFootprintSkew ScaledSkew(const FHutongFootprintSkew& Skew, const FVector2D& SideFactors)
	{
		FHutongFootprintSkew Out = Skew;
		Out.Corner00 *= SideFactors;
		Out.Corner10 *= SideFactors;
		Out.Corner11 *= SideFactors;
		Out.Corner01 *= SideFactors;
		return Out;
	}
}
