#include "Tools/HutongSnap.h"
#include "Generation/HutongBuildingComponent.h"
#include "Tools/HutongDetailOps.h"
#include "GameFramework/Actor.h"

namespace
{
	// A run and its reverse are the same line, so directions fold into [0, 180).
	double NormalizeYaw(double Deg)
	{
		double Y = FMath::Fmod(Deg, 180.0);
		if (Y < 0.0) Y += 180.0;
		return Y;
	}

	// Calls Fn with each cached footprint and its four world-space corners, in order.
	template <typename FnType>
	void ForEachFootprint(const TArray<HutongSnap::FFootprint>& Footprints,
		const AActor* Ignore, FnType&& Fn)
	{
		for (const HutongSnap::FFootprint& F : Footprints)
		{
			const AActor* Actor = F.Actor.Get();
			if (!Actor || Actor == Ignore) continue;
			Fn(F, F.Corners);
		}
	}
}

namespace HutongSnap
{
	FFootprintCache& Cache()
	{
		static FFootprintCache Instance;
		return Instance;
	}

	void FFootprintCache::Refresh(UWorld* World)
	{
		Items.Reset();
		GatheredFrom = World;
		LastRefreshSeconds = FPlatformTime::Seconds();
		if (!World) return;

		// The same walk the promote, rebuild and export paths make, rather than a second answer to
		// which actors are buildings.
		for (UHutongBuildingComponent* Building : HutongDetailOps::CollectLoaded(World))
		{
			AActor* Actor = Building ? Building->GetOwner() : nullptr;
			if (!Actor) continue;

			FFootprint F;
			F.Building = Building;
			F.Actor = Actor;
			F.ActorToWorld = Actor->GetActorTransform();
			F.Size = Building->GetFootprintSize();
			F.bPlanOnly = Building->bPlanOnly;
			// The mesh is built from the actor's origin; the corners are the placement's own, which
			// with a skew are not the rectangle's — never derive them from Size here.
			FVector2D Local[4];
			Building->GetFootprintCorners(Local);
			for (int32 i = 0; i < 4; ++i)
			{
				F.Corners[i] = F.ActorToWorld.TransformPosition(FVector(Local[i].X, Local[i].Y, 0.0));
			}
			// Dropped here rather than tested in every query: a footprint of nothing is not a
			// neighbour anything can snap to.
			if (F.Size.X > 1.0 && F.Size.Y > 1.0) Items.Add(MoveTemp(F));
		}
	}

	const TArray<FFootprint>& FFootprintCache::Get(UWorld* World, double MaxAgeSeconds)
	{
		const bool bStale = (LastRefreshSeconds < 0.0)
			|| (GatheredFrom.Get() != World)
			|| (MaxAgeSeconds >= 0.0 && FPlatformTime::Seconds() - LastRefreshSeconds > MaxAgeSeconds);
		if (bStale) Refresh(World);
		return Items;
	}

	FResult FindSnap(const TArray<FFootprint>& Footprints, const FVector& Query, double Radius,
		const AActor* IgnoreActor)
	{
		FResult Best;
		double BestScore = TNumericLimits<double>::Max();
		const double R2 = Radius * Radius;

		ForEachFootprint(Footprints, IgnoreActor, [&](const FFootprint& F, const FVector C[4])
		{
			const FVector2D Centre(
				0.25 * (C[0].X + C[1].X + C[2].X + C[3].X),
				0.25 * (C[0].Y + C[1].Y + C[2].Y + C[3].Y));
			for (int32 i = 0; i < 4; ++i)
			{
				const FVector A = C[i];
				const FVector B = C[(i + 1) % 4];

				// Corner.
				const double DC = FVector::Dist2D(Query, A);
				if (DC * DC <= R2 && DC * 0.66 < BestScore)
				{
					BestScore = DC * 0.66;
					Best.bSnapped = true;
					Best.Point = FVector(A.X, A.Y, Query.Z);
					// The two edges meeting here; the run being drawn is usually continuing one of them.
					Best.EdgeYawDeg = NormalizeYaw(
						FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X)));
					const FVector P = C[(i + 3) % 4];
					Best.EdgeYaw2Deg = NormalizeYaw(
						FMath::RadiansToDegrees(FMath::Atan2(A.Y - P.Y, A.X - P.X)));
					Best.Inward = (Centre - FVector2D(A.X, A.Y)).GetSafeNormal();
					Best.Building = F.Building;
				}

				// Nearest point along the edge, clamped to the segment.
				const FVector2D AB(B.X - A.X, B.Y - A.Y);
				const double LenSq = AB.SizeSquared();
				if (LenSq < 1.0) continue;
				const FVector2D AQ(Query.X - A.X, Query.Y - A.Y);
				const double T = FMath::Clamp(FVector2D::DotProduct(AQ, AB) / LenSq, 0.0, 1.0);
				const FVector P(A.X + AB.X * T, A.Y + AB.Y * T, Query.Z);
				const double DE = FVector::Dist2D(Query, P);
				if (DE * DE <= R2 && DE < BestScore)
				{
					BestScore = DE;
					Best.bSnapped = true;
					Best.Point = P;
					Best.EdgeYawDeg = NormalizeYaw(FMath::RadiansToDegrees(FMath::Atan2(AB.Y, AB.X)));
					Best.EdgeYaw2Deg = -1000.0;
					// The perpendicular that points at the centre: the footprint's winding is not something to assume.
					FVector2D N(-AB.Y, AB.X);
					N.Normalize();
					if (FVector2D::DotProduct(N, Centre - FVector2D(P.X, P.Y)) < 0.0) N = -N;
					Best.Inward = N;
					Best.Building = F.Building;
				}
			}
		});

		return Best;
	}

	TArray<double> GatherEdgeYaws(const TArray<FFootprint>& Footprints, const FVector& Query,
		double Radius, const AActor* IgnoreActor)
	{
		TArray<double> Yaws;
		const double R2 = Radius * Radius;

		ForEachFootprint(Footprints, IgnoreActor, [&](const FFootprint& F, const FVector C[4])
		{
			// All four edges: on a skewed footprint they are four bearings, and AddUnique folds the
			// parallel pairs of a rectangle back to two.
			bool bNear = false;
			for (int32 i = 0; i < 4 && !bNear; ++i)
			{
				bNear = FVector::DistSquared2D(Query, C[i]) <= R2;
			}
			if (!bNear) return;

			for (int32 i = 0; i < 4; ++i)
			{
				const FVector A = C[i], B = C[(i + 1) % 4];
				const double Y = NormalizeYaw(
					FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X)));
				Yaws.AddUnique(FMath::RoundToDouble(Y * 100.0) / 100.0);
			}
		});

		return Yaws;
	}

	FResult FindSnapAlongLine(const TArray<FFootprint>& Footprints, const FVector& Origin,
		const FVector2D& Dir, const FVector& Query, double Radius, const AActor* IgnoreActor)
	{
		FResult Best;
		const FVector2D D = Dir.GetSafeNormal();
		if (D.IsNearlyZero()) return Best;
		const FVector2D O(Origin.X, Origin.Y);
		const double QueryAlong = FVector2D::DotProduct(FVector2D(Query.X, Query.Y) - O, D);
		double BestDistance = Radius;

		ForEachFootprint(Footprints, IgnoreActor, [&](const FFootprint& F, const FVector C[4])
		{
			const FVector2D Centre(
				0.25 * (C[0].X + C[1].X + C[2].X + C[3].X),
				0.25 * (C[0].Y + C[1].Y + C[2].Y + C[3].Y));
			for (int32 i = 0; i < 4; ++i)
			{
				const FVector2D A(C[i].X, C[i].Y);
				const FVector2D B(C[(i + 1) % 4].X, C[(i + 1) % 4].Y);
				const FVector2D AB = B - A;
				const double Len = AB.Size();
				if (Len < 1.0) continue;
				const FVector2D E = AB / Len;
				// Nearly parallel lines meet nowhere worth snapping to.
				const double Denominator = D.X * E.Y - D.Y * E.X;
				if (FMath::Abs(Denominator) < 0.05) continue;
				// O + D*Along == A + E*OnEdge.
				const FVector2D AO = O - A;
				const double Along = (AO.Y * E.X - AO.X * E.Y) / Denominator;
				const double OnEdge = (AO.Y * D.X - AO.X * D.Y) / Denominator;
				if (OnEdge < -Radius || OnEdge > Len + Radius) continue;
				if (FMath::Abs(Along) <= 2.0) continue;
				const double Distance = FMath::Abs(Along - QueryAlong);
				if (Distance > BestDistance) continue;
				BestDistance = Distance;
				const FVector2D P = O + D * Along;
				Best.bSnapped = true;
				Best.Point = FVector(P.X, P.Y, Query.Z);
				Best.EdgeYawDeg = NormalizeYaw(FMath::RadiansToDegrees(FMath::Atan2(E.Y, E.X)));
				FVector2D N(-E.Y, E.X);
				if (FVector2D::DotProduct(N, Centre - P) < 0.0) N = -N;
				Best.Inward = N;
				Best.Building = F.Building;
			}
		});
		return Best;
	}

	double SnapYaw(double YawDeg, const TArray<double>& CandidatesDeg, double ToleranceDeg)
	{
		double Best = YawDeg;
		double BestDelta = ToleranceDeg;

		for (const double Cand : CandidatesDeg)
		{
			// Every quarter turn off a neighbour's line is a candidate too.
			for (int32 Q = 0; Q < 4; ++Q)
			{
				const double Target = Cand + 90.0 * Q;
				double Delta = FMath::Fmod(FMath::Abs(YawDeg - Target), 360.0);
				if (Delta > 180.0) Delta = 360.0 - Delta;
				if (Delta < BestDelta)
				{
					BestDelta = Delta;
					// Keep the caller's turn count: snapping must not spin the rect a half turn.
					Best = YawDeg + (Target - YawDeg > 180.0 ? Target - 360.0 - YawDeg
						: (Target - YawDeg < -180.0 ? Target + 360.0 - YawDeg : Target - YawDeg));
				}
			}
		}
		return Best;
	}
}

namespace HutongSnap
{
	FGap FindParallelGap(const TArray<FFootprint>& Footprints, const FVector& Query,
		double QueryYawDeg, double MaxDistance, double AngleToleranceDeg, const AActor* IgnoreActor)
	{
		FGap Best;
		double BestDist = MaxDistance;

		const bool bAnyYaw = (QueryYawDeg <= AnyYaw + 1.0);
		const double Want = NormalizeYaw(QueryYawDeg);
		const FVector2D Q(Query.X, Query.Y);

		ForEachFootprint(Footprints, IgnoreActor, [&](const FFootprint& F, const FVector C[4])
		{
			for (int32 i = 0; i < 4; ++i)
			{
				const FVector2D A(C[i].X, C[i].Y);
				const FVector2D B(C[(i + 1) % 4].X, C[(i + 1) % 4].Y);
				const FVector2D E = B - A;
				const double Len = E.Size();
				if (Len < 1.0) continue;

				const double Yaw = NormalizeYaw(FMath::RadiansToDegrees(FMath::Atan2(E.Y, E.X)));
				if (!bAnyYaw)
				{
					double Delta = FMath::Abs(Yaw - Want);
					if (Delta > 90.0) Delta = 180.0 - Delta;  // 0 and 180 are the same line
					if (Delta > AngleToleranceDeg) continue;
				}

				// Only where the query actually lies across from the edge.
				const FVector2D Dir = E / Len;
				const double Along = FVector2D::DotProduct(Q - A, Dir);
				if (Along < -0.25 * Len || Along > 1.25 * Len) continue;

				const FVector2D Foot = A + Dir * Along;
				const FVector2D Off = Foot - Q;
				const double Dist = Off.Size();
				if (Dist < 1.0 || Dist >= BestDist) continue;

				BestDist = Dist;
				Best.bFound = true;
				Best.DistanceCm = Dist;
				Best.Toward = Off / Dist;
				Best.EdgeYawDeg = Yaw;
			}
		});

		return Best;
	}
}
