#include "Tools/HutongWallChain.h"

namespace
{
	double Cross(const FVector2D& A, const FVector2D& B) { return A.X * B.Y - A.Y * B.X; }

	// Local +Y for a run along Dir: the direction yaw + 90° points, which is what the actor's
	// rotation makes of the rectangle's own Y.
	FVector2D LeftOf(const FVector2D& Dir) { return FVector2D(-Dir.Y, Dir.X); }

	// A join is mitered only while the miter stays a sane length: past this turn the bisector cut
	// runs out along the wall, and past the skew's own validity the end would fold over the body.
	constexpr double MaxMiterTurnDeg = 150.0;
	// An outer end is cut flush only when the face is well off the run: nearer parallel than this
	// the cut would run metres along the wall, and a run meeting a face that nearly continues it
	// is the side-alignment case, not a flush end.
	constexpr double MinFaceCross = 0.2;
}

namespace HutongWallChain
{
	double TurnDeg(const FVector2D& From, const FVector2D& To)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Cross(From, To), FVector2D::DotProduct(From, To)));
	}

	bool SideAlongFace(const FVector2D& SegmentDir, double EdgeYawDeg, double EdgeYaw2Deg,
		const FVector2D& Inward, double Thickness, double& OutDrawnY, double ToleranceDeg, double Setback)
	{
		const FVector2D D = SegmentDir.GetSafeNormal();
		if (D.IsNearlyZero() || Inward.IsNearlyZero()) return false;
		// The edge most nearly along the segment, of the one or two on offer.
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
		// The edge's perpendicular that points into the neighbour, read against the wall's own +Y.
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

		// Too-short steps dropped, so a repeated click leaves no zero-length segment behind.
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

		// The centre line: the drawn line moved across by what puts it at half the thickness, the
		// interior vertices along their bisectors so the offset segments still meet.
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

		// Joins: both ends cut on the bisector of the turn, the outer corner reaching and the inner
		// one giving way by the same half-thickness times the tangent of half the turn.
		for (int32 i = 0; i + 1 < N; ++i)
		{
			const double Turn = TurnDeg(Dir[i], Dir[i + 1]);
			if (FMath::Abs(Turn) < 0.05 || FMath::Abs(Turn) > MaxMiterTurnDeg) continue;
			const double K = 0.5 * T * FMath::Tan(FMath::DegreesToRadians(0.5 * Turn));
			// This segment's end: local y = 0 reaches by K when the turn is towards +Y.
			OutSegments[i].Skew.Corner10 = FVector2D(K, 0.0);
			OutSegments[i].Skew.Corner11 = FVector2D(-K, 0.0);
			// The next one's start: the mirror, so the two faces are one line.
			OutSegments[i + 1].Skew.Corner00 = FVector2D(-K, 0.0);
			OutSegments[i + 1].Skew.Corner01 = FVector2D(K, 0.0);
		}

		// Outer ends: each corner slid along the run onto the face's line.
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

		// A cut the warp would refuse — a short segment between two sharp turns — is not made.
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
}
