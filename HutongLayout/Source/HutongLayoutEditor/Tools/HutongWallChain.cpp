#include "Tools/HutongWallChain.h"

namespace
{
	double Cross(const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; }

	// Local +Y for a run along Dir: yaw + 90°, what the actor's rotation makes of the rect's Y.
	FVector2D LeftOf(const FVector2D& Dir) { return FVector2D(-Dir.Y, Dir.X); }

	// Past this turn the bisector cut runs along the wall and the end folds over the body.
	constexpr double MaxMiterTurnDeg = 150.0;
	// Nearer parallel, a flush cut runs metres along the wall.
	constexpr double MinFaceCross = 0.2;
}

namespace HutongWallChain
{
	double TurnDeg(const FVector2D& From, const FVector2D& To)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Cross(From, To), FVector2D::DotProduct(From, To)));
	}

	void CornerEnd(const FVector2D& SegmentDir, bool bAtStart, double Yaw1Deg, double Yaw2Deg,
		const FVector2D& Inward, const FVector2D& Corner, double Thickness, double& OutDrawnY, FEndFace& OutFace)
	{
		const FVector2D D = SegmentDir.GetSafeNormal();
		const FVector2D Away = bAtStart ? D : -D;
		auto Dir = [](double Yaw) { const double R = FMath::DegreesToRadians(Yaw); return FVector2D(FMath::Cos(R), FMath::Sin(R)); };
		FVector2D Face = Dir(Yaw1Deg);
		if (Yaw2Deg > -900.0 && FMath::Abs(Cross(Away, Dir(Yaw2Deg))) > FMath::Abs(Cross(Away, Face))) Face = Dir(Yaw2Deg);
		if (FVector2D::DotProduct(Face, Inward) < 0.0) Face = -Face;
		// Body toward it: on the left of travel, the drawn (outer) line is local Y = 0.
		OutDrawnY = Cross(D, Face) > 0.0 ? 0.0 : Thickness;
		OutFace.bSet = true;
		OutFace.Point = Corner;
		OutFace.Dir = Face;
	}

	bool SideAlongFace(const FVector2D& SegmentDir, double EdgeYawDeg, double EdgeYaw2Deg,
		const FVector2D& Inward, double Thickness, double& OutDrawnY, double ToleranceDeg, double Setback)
	{
		const FVector2D D = SegmentDir.GetSafeNormal();
		if (D.IsNearlyZero() || Inward.IsNearlyZero()) return false;
		double BestDot = 0.0;
		FVector2D Edge = FVector2D::ZeroVector;
		for (const double Yaw : { EdgeYawDeg, EdgeYaw2Deg })
		{
			if (Yaw < -900.0) continue;
			const double R = FMath::DegreesToRadians(Yaw);
			const FVector2D E(FMath::Cos(R), FMath::Sin(R));
			const double Dot = FMath::Abs(FVector2D::DotProduct(D, E));
			if (Dot > BestDot) { BestDot = Dot; Edge = E; }
		}
		if (BestDot < FMath::Cos(FMath::DegreesToRadians(ToleranceDeg))) return false;
		FVector2D Perp(-Edge.Y, Edge.X);
		if (FVector2D::DotProduct(Perp, Inward) < 0.0) Perp = -Perp;
		const FVector2D Left = LeftOf(D);
		const double S = FMath::Max(Setback, 0.0);
		OutDrawnY = FVector2D::DotProduct(Left, Perp) > 0.0 ? -S : Thickness + S;
		return true;
	}

	bool Build(const TArray<FVector2D>& InPoints, double Thickness, double DrawnY,
		const FEndFace& StartFace, const FEndFace& EndFace, TArray<FSegment>& OutSegments,
		double MinLength)
	{
		OutSegments.Reset();
		const double T = FMath::Max(Thickness, 1.0);

		TArray<FVector2D> Points;
		for (const FVector2D& P : InPoints)
		{
			if (Points.Num() == 0 || FVector2D::Distance(P, Points.Last()) >= MinLength) Points.Add(P);
		}
		const int32 N = Points.Num() - 1;
		if (N < 1) return false;

		TArray<FVector2D> Dir, Left;
		Dir.SetNum(N); Left.SetNum(N);
		for (int32 i = 0; i < N; ++i)
		{
			Dir[i] = (Points[i + 1] - Points[i]).GetSafeNormal();
			Left[i] = LeftOf(Dir[i]);
		}

		const double Shift = 0.5 * T - DrawnY;
		TArray<FVector2D> Centre;
		Centre.SetNum(N + 1);
		Centre[0] = Points[0] + Left[0] * Shift;
		Centre[N] = Points[N] + Left[N - 1] * Shift;
		for (int32 i = 1; i < N; ++i)
		{
			const double Denominator = 1.0 + FVector2D::DotProduct(Left[i - 1], Left[i]);
			Centre[i] = (Denominator > 0.1)
				? Points[i] + (Left[i - 1] + Left[i]) * (Shift / Denominator)
				: Points[i] + Left[i] * Shift;
		}

		OutSegments.SetNum(N);
		for (int32 i = 0; i < N; ++i)
		{
			FSegment& S = OutSegments[i];
			S.Origin = Centre[i] - Left[i] * (0.5 * T);
			S.YawDeg = FMath::RadiansToDegrees(FMath::Atan2(Dir[i].Y, Dir[i].X));
			S.Length = FVector2D::Distance(Centre[i], Centre[i + 1]);
		}

		// Joins cut on the bisector: outer corner reaches, inner gives way, by half-thickness * tan(half turn).
		for (int32 i = 0; i + 1 < N; ++i)
		{
			const double Turn = TurnDeg(Dir[i], Dir[i + 1]);
			if (FMath::Abs(Turn) < 0.05 || FMath::Abs(Turn) > MaxMiterTurnDeg) continue;
			const double K = 0.5 * T * FMath::Tan(FMath::DegreesToRadians(0.5 * Turn));
			OutSegments[i].Skew.Corner10 = FVector2D(K, 0.0);
			OutSegments[i].Skew.Corner11 = FVector2D(-K, 0.0);
			OutSegments[i + 1].Skew.Corner00 = FVector2D(-K, 0.0);
			OutSegments[i + 1].Skew.Corner01 = FVector2D(K, 0.0);
		}

		auto CutOnFace = [&](FSegment& S, const FVector2D& Vertex, const FVector2D& D, const FVector2D& L, const FEndFace& Face, bool bStart)
		{
			if (!Face.bSet) return;
			const FVector2D E = Face.Dir.GetSafeNormal();
			const double DE = Cross(D, E);
			if (FMath::Abs(DE) < MinFaceCross) return;
			const double VE = Cross(Vertex - Face.Point, E);
			const double LE = Cross(L, E);
			double Offsets[2];
			for (int32 k = 0; k < 2; ++k)
			{
				const double Y = k == 0 ? 0.0 : T;
				Offsets[k] = -(VE + (Y - 0.5 * T) * LE) / DE;
				if (FMath::Abs(Offsets[k]) > 2.0 * T) return;
			}
			if (bStart) { S.Skew.Corner00 = FVector2D(Offsets[0], 0.0); S.Skew.Corner01 = FVector2D(Offsets[1], 0.0); }
			else        { S.Skew.Corner10 = FVector2D(Offsets[0], 0.0); S.Skew.Corner11 = FVector2D(Offsets[1], 0.0); }
		};
		CutOnFace(OutSegments[0], Centre[0], Dir[0], Left[0], StartFace, true);
		CutOnFace(OutSegments[N - 1], Centre[N], Dir[N - 1], Left[N - 1], EndFace, false);

		// Skip a cut the warp would refuse (a short segment between two sharp turns).
		for (FSegment& S : OutSegments)
		{
			if (!S.Skew.IsZero() && !HutongFootprint::IsSkewValid(FVector2D(S.Length, T), S.Skew))
			{
				S.Skew = FHutongFootprintSkew();
			}
		}
		return true;
	}

	void Corners(const FSegment& Segment, double Thickness, FVector2D OutCorners[4])
	{
		const double Yaw = FMath::DegreesToRadians(Segment.YawDeg);
		const FVector2D D(FMath::Cos(Yaw), FMath::Sin(Yaw));
		const FVector2D L = LeftOf(D);
		FVector2D Local[4];
		HutongFootprint::Corners(FVector2D(Segment.Length, Thickness), Segment.Skew, Local);
		for (int32 i = 0; i < 4; ++i)
		{
			OutCorners[i] = Segment.Origin + D * Local[i].X + L * Local[i].Y;
		}
	}

	FHutongFootprintSkew CarryJoinSkew(const FHutongFootprintSkew& Existing, bool bJoinAtStart, const FHutongFootprintSkew& FromFar)
	{
		FHutongFootprintSkew Skew = Existing;
		Skew.Mode = EHutongSkewMode::Ends;
		if (bJoinAtStart)
		{
			Skew.Set(0, -FromFar.Get(2));
			Skew.Set(3, -FromFar.Get(1));
		}
		else
		{
			Skew.Set(1, FromFar.Get(1));
			Skew.Set(2, FromFar.Get(2));
		}
		return Skew;
	}

	FHutongFootprintSkew Reversed(const FHutongFootprintSkew& Skew)
	{
		FHutongFootprintSkew Out = Skew;
		Out.Set(0, -Skew.Get(2));
		Out.Set(1, -Skew.Get(3));
		Out.Set(2, -Skew.Get(0));
		Out.Set(3, -Skew.Get(1));
		return Out;
	}
}
