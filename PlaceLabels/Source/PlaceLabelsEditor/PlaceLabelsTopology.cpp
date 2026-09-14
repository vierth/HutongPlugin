#include "PlaceLabelsTopology.h"

#include "PlaceLabelGeometry.h"
#include "PlaceRegionComponent.h"
#include "Tools/PlaceRegionEditCore.h"

namespace PlaceLabelsTopology
{

namespace
{
	// Distance from a point to a closed polygon's boundary.
	double DistanceToBoundary(const TArray<FVector2D>& Poly, const FVector2D& P)
	{
		const PlaceLabelsEdit::FEdgeHit Hit =
			PlaceLabelsEdit::ClosestEdge(Poly, P, /*bClosed*/ true);
		return Hit.EdgeIndex == INDEX_NONE ? TNumericLimits<double>::Max()
										   : FMath::Sqrt(Hit.DistSq);
	}

	// Minimal union-find.
	struct FUnionFind
	{
		TArray<int32> Parent;

		explicit FUnionFind(int32 Count)
		{
			Parent.SetNumUninitialized(Count);
			for (int32 i = 0; i < Count; ++i)
			{
				Parent[i] = i;
			}
		}

		int32 Find(int32 X)
		{
			while (Parent[X] != X)
			{
				Parent[X] = Parent[Parent[X]];
				X = Parent[X];
			}
			return X;
		}

		void Union(int32 A, int32 B)
		{
			const int32 RootA = Find(A);
			const int32 RootB = Find(B);
			if (RootA != RootB)
			{
				Parent[RootB] = RootA;
			}
		}
	};

	struct FVertexRef
	{
		int32 Polygon = 0;
		int32 Vertex = 0;
	};

	// Pass 1, corner to corner: every vertex within tolerance of a vertex belonging to a different polygon joins one cluster, and the cluster collapses to its centroid.
	void WeldCorners(TArray<TArray<FVector2D>>& Polygons, TArray<bool>& Changed, double Tol,
		FWeldReport& OutReport)
	{
		TArray<FVertexRef> Refs;
		TArray<FVector2D> Positions;
		for (int32 r = 0; r < Polygons.Num(); ++r)
		{
			for (int32 v = 0; v < Polygons[r].Num(); ++v)
			{
				Refs.Add({ r, v });
				Positions.Add(Polygons[r][v]);
			}
		}
		if (Positions.Num() < 2)
		{
			return;
		}

		// Cells the size of the tolerance, so every candidate partner is in the 3x3 neighbourhood.
		const double CellSize = FMath::Max(Tol, UE_KINDA_SMALL_NUMBER);
		TMap<FIntPoint, TArray<int32>> Grid;
		for (int32 i = 0; i < Positions.Num(); ++i)
		{
			const FIntPoint Cell(FMath::FloorToInt32(Positions[i].X / CellSize),
				FMath::FloorToInt32(Positions[i].Y / CellSize));
			Grid.FindOrAdd(Cell).Add(i);
		}

		FUnionFind Sets(Positions.Num());
		const double TolSq = Tol * Tol;

		for (int32 i = 0; i < Positions.Num(); ++i)
		{
			const FIntPoint Cell(FMath::FloorToInt32(Positions[i].X / CellSize),
				FMath::FloorToInt32(Positions[i].Y / CellSize));

			for (int32 dx = -1; dx <= 1; ++dx)
			{
				for (int32 dy = -1; dy <= 1; ++dy)
				{
					const TArray<int32>* Bucket = Grid.Find(FIntPoint(Cell.X + dx, Cell.Y + dy));
					if (!Bucket)
					{
						continue;
					}
					for (int32 j : *Bucket)
					{
						if (j <= i)
						{
							continue;
						}
						if (FVector2D::DistSquared(Positions[i], Positions[j]) <= TolSq)
						{
							Sets.Union(i, j);
						}
					}
				}
			}
		}

		TMap<int32, TArray<int32>> Clusters;
		for (int32 i = 0; i < Positions.Num(); ++i)
		{
			Clusters.FindOrAdd(Sets.Find(i)).Add(i);
		}

		for (const TPair<int32, TArray<int32>>& Cluster : Clusters)
		{
			const TArray<int32>& Members = Cluster.Value;
			if (Members.Num() < 2)
			{
				continue;
			}

			// A cluster confined to one polygon is two corners of the same outline sitting on top of each other.
			TMap<int32, int32> CornersPerPolygon;
			for (int32 Index : Members)
			{
				++CornersPerPolygon.FindOrAdd(Refs[Index].Polygon);
			}
			if (CornersPerPolygon.Num() < 2)
			{
				continue;
			}

			// A polygon contributing more than one corner has those corners collapsed onto each
			// other as well as onto its neighbour's, and the cleanup pass will only take a polygon
			// down to three. So a cluster that would leave one below three is refused whole: a
			// triangle with two corners in it comes out with zero area, stops containing the player
			// and stops triangulating, and the weld reported success.
			bool bWouldDegenerate = false;
			for (const TPair<int32, int32>& Pair : CornersPerPolygon)
			{
				if (Pair.Value > 1 && Polygons[Pair.Key].Num() - (Pair.Value - 1) < 3)
				{
					bWouldDegenerate = true;
					break;
				}
			}
			if (bWouldDegenerate)
			{
				++OutReport.ClustersRefused;
				continue;
			}

			// Proximity is transitive and distance is not.
			FBox2D Bounds(ForceInit);
			for (int32 Index : Members)
			{
				Bounds += Positions[Index];
			}
			if (Bounds.GetSize().Size() > Tol * 3.0)
			{
				++OutReport.ClustersRefused;
				continue;
			}

			FVector2D Centroid = FVector2D::ZeroVector;
			for (int32 Index : Members)
			{
				Centroid += Positions[Index];
			}
			Centroid /= static_cast<double>(Members.Num());

			for (int32 Index : Members)
			{
				const FVertexRef& Ref = Refs[Index];
				if (!Polygons[Ref.Polygon][Ref.Vertex].Equals(Centroid, CoincidentEpsilonCm))
				{
					Polygons[Ref.Polygon][Ref.Vertex] = Centroid;
					Changed[Ref.Polygon] = true;
					++OutReport.CornersMoved;
				}
			}
		}
	}

	struct FPendingInsert
	{
		int32 EdgeIndex = 0;
		double Param = 0.0;
		FVector2D Point = FVector2D::ZeroVector;
	};

	// Pass 2, T-junctions: a corner landing in the middle of a neighbour's edge is projected exactly onto that edge, and the neighbour gains a corner there.
	void WeldTJunctions(TArray<TArray<FVector2D>>& Polygons, TArray<bool>& Changed, double Tol,
		FWeldReport& OutReport)
	{
		TArray<TArray<FPendingInsert>> Inserts;
		Inserts.SetNum(Polygons.Num());

		const double TolSq = Tol * Tol;

		for (int32 r = 0; r < Polygons.Num(); ++r)
		{
			for (int32 s = 0; s < Polygons.Num(); ++s)
			{
				if (r == s || Polygons[r].Num() < 3 || Polygons[s].Num() < 3)
				{
					continue;
				}

				for (int32 v = 0; v < Polygons[r].Num(); ++v)
				{
					const FVector2D P = Polygons[r][v];

					const PlaceLabelsEdit::FEdgeHit Hit =
						PlaceLabelsEdit::ClosestEdge(Polygons[s], P, /*bClosed*/ true);
					if (Hit.EdgeIndex == INDEX_NONE || Hit.DistSq > TolSq)
					{
						continue;
					}

					// Already has a partner corner, from pass 1 or from being drawn on it.
					double NearestVertexSq = TNumericLimits<double>::Max();
					PlaceLabelsEdit::ClosestVertex(Polygons[s], P, NearestVertexSq);
					if (NearestVertexSq <= TolSq)
					{
						continue;
					}

					// Or from an insertion queued earlier this pass, which is not in the polygon yet.
					bool bQueued = false;
					for (const FPendingInsert& Queued : Inserts[s])
					{
						if (FVector2D::DistSquared(Queued.Point, P) <= TolSq)
						{
							bQueued = true;
							break;
						}
					}
					if (bQueued)
					{
						continue;
					}

					const FVector2D& A = Polygons[s][Hit.EdgeIndex];
					const FVector2D& B = Polygons[s][(Hit.EdgeIndex + 1) % Polygons[s].Num()];
					const FVector2D Edge = B - A;
					const double LengthSq = Edge.SizeSquared();
					if (LengthSq < UE_KINDA_SMALL_NUMBER)
					{
						continue;
					}
					const double Param = FVector2D::DotProduct(P - A, Edge) / LengthSq;

					// The endpoints are corners, and a corner that wanted one of those should have found it in pass 1.
					if (Param <= UE_KINDA_SMALL_NUMBER || Param >= 1.0 - UE_KINDA_SMALL_NUMBER)
					{
						continue;
					}

					if (!Polygons[r][v].Equals(Hit.Point, CoincidentEpsilonCm))
					{
						Polygons[r][v] = Hit.Point;
						Changed[r] = true;
						++OutReport.CornersMoved;
					}

					Inserts[s].Add({ Hit.EdgeIndex, Param, Hit.Point });
				}
			}
		}

		for (int32 s = 0; s < Polygons.Num(); ++s)
		{
			if (Inserts[s].Num() == 0)
			{
				continue;
			}

			// Descending, so an insertion never shifts the index of one still to be applied.
			Inserts[s].Sort([](const FPendingInsert& A, const FPendingInsert& B)
			{
				return A.EdgeIndex != B.EdgeIndex ? A.EdgeIndex > B.EdgeIndex : A.Param > B.Param;
			});

			for (const FPendingInsert& Insert : Inserts[s])
			{
				Polygons[s].Insert(Insert.Point, Insert.EdgeIndex + 1);
				++OutReport.CornersInserted;
			}
			Changed[s] = true;
		}
	}

	// Pass 3, cleanup: a weld can pull two corners of one outline onto the same point, leaving a zero-length edge every downstream predicate then has to survive.
	void RemoveDuplicateCorners(TArray<TArray<FVector2D>>& Polygons, TArray<bool>& Changed,
		FWeldReport& OutReport)
	{
		for (int32 p = 0; p < Polygons.Num(); ++p)
		{
			TArray<FVector2D>& Poly = Polygons[p];
			for (int32 i = Poly.Num() - 1; i >= 0 && Poly.Num() > 3; --i)
			{
				const FVector2D& Here = Poly[i];
				const FVector2D& Prev = Poly[(i - 1 + Poly.Num()) % Poly.Num()];
				if (Here.Equals(Prev, CoincidentEpsilonCm))
				{
					Poly.RemoveAt(i);
					Changed[p] = true;
					++OutReport.CornersRemoved;
				}
			}
		}
	}

	FVector2D WorldToLocal2D(const UPlaceRegionComponent* Region, const FVector2D& World)
	{
		const FTransform& Xf = Region->GetComponentTransform();
		const double Z = Region->GetComponentLocation().Z;
		const FVector Local = Xf.InverseTransformPosition(FVector(World.X, World.Y, Z));
		return FVector2D(Local.X, Local.Y);
	}
}

void WeldPolygons(TArray<TArray<FVector2D>>& Polygons, double ToleranceCm,
	TArray<bool>& OutChanged, FWeldReport& OutReport)
{
	OutChanged.Init(false, Polygons.Num());
	if (Polygons.Num() < 2)
	{
		return;
	}

	const double Tol = FMath::Max(ToleranceCm, CoincidentEpsilonCm);

	WeldCorners(Polygons, OutChanged, Tol, OutReport);
	WeldTJunctions(Polygons, OutChanged, Tol, OutReport);
	RemoveDuplicateCorners(Polygons, OutChanged, OutReport);
}

void FindSeamMarks(const TArray<FVector2D>& A, const TArray<FVector2D>& B, double ToleranceCm,
	TArray<FSeamMark>& OutMarks)
{
	OutMarks.Reset();

	const int32 NA = A.Num();
	const int32 NB = B.Num();
	if (NA < 3 || NB < 3)
	{
		return;
	}

	const double Tol = FMath::Max(ToleranceCm, CoincidentEpsilonCm);

	// Every edge of A against every edge of B.
	for (int32 i = 0; i < NA; ++i)
	{
		const FVector A1(A[i].X, A[i].Y, 0.0);
		const FVector A2(A[(i + 1) % NA].X, A[(i + 1) % NA].Y, 0.0);

		for (int32 j = 0; j < NB; ++j)
		{
			const FVector B1(B[j].X, B[j].Y, 0.0);
			const FVector B2(B[(j + 1) % NB].X, B[(j + 1) % NB].Y, 0.0);

			FVector ClosestOnA;
			FVector ClosestOnB;
			FMath::SegmentDistToSegmentSafe(A1, A2, B1, B2, ClosestOnA, ClosestOnB);

			const double Distance = FVector::Dist(ClosestOnA, ClosestOnB);
			if (Distance <= CoincidentEpsilonCm || Distance > Tol)
			{
				continue;
			}

			FSeamMark Mark;
			Mark.From = FVector2D(ClosestOnA.X, ClosestOnA.Y);
			Mark.To = FVector2D(ClosestOnB.X, ClosestOnB.Y);
			Mark.DistanceCm = Distance;
			OutMarks.Add(Mark);
		}
	}
}

void FindPolygonSeams(const TArray<TArray<FVector2D>>& Polygons, double ToleranceCm,
	TArray<FPolygonSeam>& OutSeams)
{
	OutSeams.Reset();

	const double Tol = FMath::Max(ToleranceCm, CoincidentEpsilonCm);

	for (int32 i = 0; i < Polygons.Num(); ++i)
	{
		if (Polygons[i].Num() < 3)
		{
			continue;
		}
		const FBox2D BoundsA = PlaceLabelsGeo::ComputeBounds2D(Polygons[i]);

		for (int32 j = i + 1; j < Polygons.Num(); ++j)
		{
			if (Polygons[j].Num() < 3)
			{
				continue;
			}
			const FBox2D BoundsB = PlaceLabelsGeo::ComputeBounds2D(Polygons[j]);
			if (!BoundsA.ExpandBy(Tol).Intersect(BoundsB))
			{
				continue;
			}

			TArray<FSeamMark> Marks;
			FindSeamMarks(Polygons[i], Polygons[j], Tol, Marks);
			if (Marks.Num() == 0)
			{
				continue;
			}

			FPolygonSeam Seam;
			Seam.A = i;
			Seam.B = j;
			Seam.NearMissCount = Marks.Num();
			for (const FSeamMark& Mark : Marks)
			{
				Seam.WorstOffsetCm = FMath::Max(Seam.WorstOffsetCm, Mark.DistanceCm);
			}

			// Gap or overlap: the same fault and the same weld fixes both, but they read entirely differently to an author working out what they did.
			for (const FSeamMark& Mark : Marks)
			{
				const FVector2D Between = (Mark.From + Mark.To) * 0.5;
				if (PlaceLabelsGeo::PointInPolygon2D(Polygons[i], Between)
					&& PlaceLabelsGeo::PointInPolygon2D(Polygons[j], Between))
				{
					Seam.bOverlapping = true;
					break;
				}
			}

			OutSeams.Add(Seam);
		}
	}

	// Worst first: the biggest sliver is the one most likely to be a real mistake.
	OutSeams.Sort([](const FPolygonSeam& A, const FPolygonSeam& B)
	{
		return A.WorstOffsetCm > B.WorstOffsetCm;
	});
}

void FindSeamIssues(const TArray<TWeakObjectPtr<UPlaceRegionComponent>>& Regions,
	double ToleranceCm, TArray<FSeamIssue>& OutIssues)
{
	OutIssues.Reset();

	// Index-parallel with Polygons, so a seam's indices map straight back to components.
	TArray<TWeakObjectPtr<UPlaceRegionComponent>> Live;
	TArray<TArray<FVector2D>> Polygons;

	for (const TWeakObjectPtr<UPlaceRegionComponent>& Weak : Regions)
	{
		const UPlaceRegionComponent* Region = Weak.Get();
		if (!Region || Region->GetWorldPoints2D().Num() < 3)
		{
			continue;
		}
		Live.Add(Weak);
		Polygons.Add(Region->GetWorldPoints2D());
	}

	TArray<FPolygonSeam> Seams;
	FindPolygonSeams(Polygons, ToleranceCm, Seams);

	OutIssues.Reserve(Seams.Num());
	for (const FPolygonSeam& Seam : Seams)
	{
		FSeamIssue Issue;
		Issue.A = Live[Seam.A];
		Issue.B = Live[Seam.B];
		Issue.WorstOffsetCm = Seam.WorstOffsetCm;
		Issue.NearMissCount = Seam.NearMissCount;
		Issue.bOverlapping = Seam.bOverlapping;
		OutIssues.Add(Issue);
	}
}

void WeldBoundaries(const TArray<UPlaceRegionComponent*>& Regions, double ToleranceCm,
	FWeldReport& OutReport)
{
	OutReport = FWeldReport();

	TArray<UPlaceRegionComponent*> Live;
	TArray<TArray<FVector2D>> Polygons;

	for (UPlaceRegionComponent* Region : Regions)
	{
		if (!Region || Region->LocalPoints.Num() < 3)
		{
			continue;
		}
		Live.Add(Region);
		Polygons.Add(Region->GetWorldPoints2D());
	}
	if (Live.Num() < 2)
	{
		return;
	}

	TArray<bool> Changed;
	WeldPolygons(Polygons, ToleranceCm, Changed, OutReport);

	for (int32 i = 0; i < Live.Num(); ++i)
	{
		// Untouched regions must not be dirtied.
		if (!Changed[i])
		{
			continue;
		}

		UPlaceRegionComponent* Region = Live[i];
		Region->Modify();

		Region->LocalPoints.Reset(Polygons[i].Num());
		for (const FVector2D& P : Polygons[i])
		{
			Region->LocalPoints.Add(WorldToLocal2D(Region, P));
		}

		Region->RebuildCache();
		Region->UpdateBounds();
		++OutReport.RegionsChanged;
	}
}

void WeldPair(UPlaceRegionComponent* A, UPlaceRegionComponent* B, double ToleranceCm,
	FWeldReport& OutReport)
{
	TArray<UPlaceRegionComponent*> Pair;
	Pair.Add(A);
	Pair.Add(B);
	WeldBoundaries(Pair, ToleranceCm, OutReport);
}

} // namespace PlaceLabelsTopology
