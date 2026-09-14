#include "PlaceLabelGeometry.h"

namespace PlaceLabelsGeo
{
	bool PointInPolygon2D(const TArray<FVector2D>& Poly, const FVector2D& P)
	{
		const int32 N = Poly.Num();
		if (N < 3)
		{
			return false;
		}

		bool bInside = false;
		for (int32 i = 0, j = N - 1; i < N; j = i++)
		{
			const FVector2D& A = Poly[i];
			const FVector2D& B = Poly[j];

			// Half-open in Y: an edge owns its lower endpoint and not its upper one.
			const bool bStraddles = (A.Y > P.Y) != (B.Y > P.Y);
			if (!bStraddles)
			{
				continue;
			}

			const double T = (P.Y - A.Y) / (B.Y - A.Y);
			if (P.X < A.X + T * (B.X - A.X))
			{
				bInside = !bInside;
			}
		}
		return bInside;
	}

	double SignedArea2D(const TArray<FVector2D>& Poly)
	{
		const int32 N = Poly.Num();
		if (N < 3)
		{
			return 0.0;
		}

		double Twice = 0.0;
		for (int32 i = 0, j = N - 1; i < N; j = i++)
		{
			Twice += (Poly[j].X * Poly[i].Y) - (Poly[i].X * Poly[j].Y);
		}
		return Twice * 0.5;
	}

	FBox2D ComputeBounds2D(const TArray<FVector2D>& Poly)
	{
		FBox2D Box(ForceInit);
		for (const FVector2D& V : Poly)
		{
			Box += V;
		}
		return Box;
	}

	double Perimeter2D(const TArray<FVector2D>& Poly)
	{
		const int32 N = Poly.Num();
		if (N < 2)
		{
			return 0.0;
		}

		double Total = 0.0;
		for (int32 i = 0, j = N - 1; i < N; j = i++)
		{
			Total += FVector2D::Distance(Poly[j], Poly[i]);
		}
		return Total;
	}

	double DistancePointToPolygon2D(const TArray<FVector2D>& Poly, const FVector2D& P)
	{
		const int32 N = Poly.Num();
		if (N < 3)
		{
			return TNumericLimits<double>::Max();
		}
		if (PointInPolygon2D(Poly, P))
		{
			return 0.0;
		}

		double Best = TNumericLimits<double>::Max();
		for (int32 i = 0, j = N - 1; i < N; j = i++)
		{
			const FVector2D Closest = FMath::ClosestPointOnSegment2D(P, Poly[j], Poly[i]);
			Best = FMath::Min(Best, FVector2D::Distance(P, Closest));
		}
		return Best;
	}

	double DistancePolygonToPolygon2D(
		const TArray<FVector2D>& A, const TArray<FVector2D>& B, double MaxDistance)
	{
		if (A.Num() < 3 || B.Num() < 3)
		{
			return TNumericLimits<double>::Max();
		}

		// Bounds reject before any edge work.
		const FBox2D BoundsA = ComputeBounds2D(A).ExpandBy(MaxDistance);
		if (!BoundsA.Intersect(ComputeBounds2D(B)))
		{
			return TNumericLimits<double>::Max();
		}

		// Containment either way counts as touching.
		if (PointInPolygon2D(B, A[0]) || PointInPolygon2D(A, B[0]))
		{
			return 0.0;
		}

		auto MinVertexToEdge = [](const TArray<FVector2D>& Verts, const TArray<FVector2D>& Edges)
		{
			double Best = TNumericLimits<double>::Max();
			const int32 M = Edges.Num();
			for (const FVector2D& V : Verts)
			{
				for (int32 i = 0, j = M - 1; i < M; j = i++)
				{
					const FVector2D Closest = FMath::ClosestPointOnSegment2D(V, Edges[j], Edges[i]);
					Best = FMath::Min(Best, FVector2D::Distance(V, Closest));
				}
			}
			return Best;
		};

		// Both directions: a short edge poking at the middle of a long one is missed by either direction alone.
		return FMath::Min(MinVertexToEdge(A, B), MinVertexToEdge(B, A));
	}

	FVector2D RepresentativePoint2D(const TArray<FVector2D>& Poly)
	{
		const int32 N = Poly.Num();
		if (N == 0)
		{
			return FVector2D::ZeroVector;
		}
		if (N < 3)
		{
			return Poly[0];
		}

		// Area-weighted centroid. Degenerate (zero-area) polygons fall back to the vertex mean.
		const double Area = SignedArea2D(Poly);
		FVector2D Centroid = FVector2D::ZeroVector;
		if (!FMath::IsNearlyZero(Area))
		{
			for (int32 i = 0, j = N - 1; i < N; j = i++)
			{
				const double Cross = (Poly[j].X * Poly[i].Y) - (Poly[i].X * Poly[j].Y);
				Centroid += (Poly[j] + Poly[i]) * Cross;
			}
			Centroid /= (6.0 * Area);
		}
		else
		{
			for (const FVector2D& V : Poly)
			{
				Centroid += V;
			}
			Centroid /= static_cast<double>(N);
		}

		if (PointInPolygon2D(Poly, Centroid))
		{
			return Centroid;
		}

		// Concave shape whose centroid escaped it.
		int32 NearestIndex = 0;
		double NearestDistSq = TNumericLimits<double>::Max();
		for (int32 i = 0; i < N; ++i)
		{
			const double DistSq = FVector2D::DistSquared(Poly[i], Centroid);
			if (DistSq < NearestDistSq)
			{
				NearestDistSq = DistSq;
				NearestIndex = i;
			}
		}

		const FVector2D& V = Poly[NearestIndex];
		const FVector2D& Prev = Poly[(NearestIndex + N - 1) % N];
		const FVector2D& Next = Poly[(NearestIndex + 1) % N];
		const FVector2D Inward = ((Prev + Next) * 0.5 - V);
		const FVector2D Candidate = V + Inward * 0.5;

		return PointInPolygon2D(Poly, Candidate) ? Candidate : V;
	}

	bool TriangulatePolygon2D(const TArray<FVector2D>& Poly, TArray<int32>& OutIndices)
	{
		OutIndices.Reset();

		const int32 N = Poly.Num();
		if (N < 3)
		{
			return false;
		}

		// Work on a CCW copy of the index list; a clockwise polygon is just walked backwards.
		TArray<int32> Remaining;
		Remaining.Reserve(N);
		if (SignedArea2D(Poly) >= 0.0)
		{
			for (int32 i = 0; i < N; ++i)
			{
				Remaining.Add(i);
			}
		}
		else
		{
			for (int32 i = N - 1; i >= 0; --i)
			{
				Remaining.Add(i);
			}
		}

		auto Cross = [](const FVector2D& A, const FVector2D& B, const FVector2D& C)
		{
			return (B.X - A.X) * (C.Y - A.Y) - (B.Y - A.Y) * (C.X - A.X);
		};

		auto PointInTriangle = [&Cross](const FVector2D& P,
			const FVector2D& A, const FVector2D& B, const FVector2D& C)
		{
			const double D1 = Cross(A, B, P);
			const double D2 = Cross(B, C, P);
			const double D3 = Cross(C, A, P);
			// Strictly inside; touching an edge is not an obstruction to clipping the ear.
			return (D1 > 0.0 && D2 > 0.0 && D3 > 0.0) || (D1 < 0.0 && D2 < 0.0 && D3 < 0.0);
		};

		// Each pass must remove at least one ear or the polygon is not simple.
		int32 GuardCounter = 2 * Remaining.Num();

		while (Remaining.Num() > 3 && GuardCounter-- > 0)
		{
			bool bClipped = false;
			const int32 Count = Remaining.Num();

			for (int32 i = 0; i < Count; ++i)
			{
				const int32 IndexPrev = Remaining[(i + Count - 1) % Count];
				const int32 IndexCur = Remaining[i];
				const int32 IndexNext = Remaining[(i + 1) % Count];

				const FVector2D& A = Poly[IndexPrev];
				const FVector2D& B = Poly[IndexCur];
				const FVector2D& C = Poly[IndexNext];

				// Reflex corners are not ears.
				if (Cross(A, B, C) <= 0.0)
				{
					continue;
				}

				bool bContainsOther = false;
				for (int32 j = 0; j < Count; ++j)
				{
					const int32 Other = Remaining[j];
					if (Other == IndexPrev || Other == IndexCur || Other == IndexNext)
					{
						continue;
					}
					if (PointInTriangle(Poly[Other], A, B, C))
					{
						bContainsOther = true;
						break;
					}
				}
				if (bContainsOther)
				{
					continue;
				}

				OutIndices.Add(IndexPrev);
				OutIndices.Add(IndexCur);
				OutIndices.Add(IndexNext);
				Remaining.RemoveAt(i);
				bClipped = true;
				break;
			}

			if (!bClipped)
			{
				// No ear anywhere: the outline crosses itself.
				return false;
			}
		}

		if (Remaining.Num() == 3)
		{
			OutIndices.Append({ Remaining[0], Remaining[1], Remaining[2] });
			return true;
		}
		return false;
	}

	void FindSelfIntersections(
		const TArray<FVector2D>& Poly, TArray<TPair<int32, int32>>& OutCrossingEdges)
	{
		OutCrossingEdges.Reset();

		const int32 N = Poly.Num();
		if (N < 4)
		{
			return;
		}

		// SegmentIntersection2D is declared on FVector, not FVector2D.
		auto Lift = [](const FVector2D& V) { return FVector(V.X, V.Y, 0.0); };

		// O(n^2) over edges.
		for (int32 i = 0; i < N; ++i)
		{
			const FVector A0 = Lift(Poly[i]);
			const FVector A1 = Lift(Poly[(i + 1) % N]);

			for (int32 j = i + 1; j < N; ++j)
			{
				// Adjacent edges always share an endpoint; the wrap-around pair does too.
				if (j == i + 1 || (i == 0 && j == N - 1))
				{
					continue;
				}

				const FVector B0 = Lift(Poly[j]);
				const FVector B1 = Lift(Poly[(j + 1) % N]);

				FVector Unused;
				if (FMath::SegmentIntersection2D(A0, A1, B0, B1, Unused))
				{
					OutCrossingEdges.Emplace(i, j);
				}
			}
		}
	}
}
