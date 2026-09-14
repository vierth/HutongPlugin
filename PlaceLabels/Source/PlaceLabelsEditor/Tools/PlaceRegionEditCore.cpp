#include "Tools/PlaceRegionEditCore.h"

#include "PlaceRegionComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "SceneManagement.h"

namespace PlaceLabelsEdit
{

void DrawLine(FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B,
	const FLinearColor& Color, float Thickness)
{
	if (!PDI)
	{
		return;
	}
	static const FLinearColor Backing(0.02f, 0.02f, 0.02f, 1.0f);
	PDI->DrawLine(A, B, Backing, SDPG_Foreground, Thickness + 4.0f);
	PDI->DrawLine(A, B, Color, SDPG_Foreground, Thickness);
}

void DrawDashedLine(FPrimitiveDrawInterface* PDI, const FVector& A, const FVector& B,
	const FLinearColor& Color, float Thickness, double DashLength)
{
	const FVector Delta = B - A;
	const double Length = Delta.Size();
	if (Length < UE_KINDA_SMALL_NUMBER)
	{
		return;
	}

	// An odd number of half-periods puts a dash on both ends of the line.
	const int32 Steps = FMath::Max(3, 2 * FMath::RoundToInt32(Length / (2.0 * DashLength)) + 1);
	const FVector Step = Delta / Steps;
	for (int32 i = 0; i < Steps; i += 2)
	{
		DrawLine(PDI, A + Step * i, A + Step * (i + 1), Color, Thickness);
	}
}

void DrawCrossHandle(FPrimitiveDrawInterface* PDI, const FVector& P, const FLinearColor& Color,
	double Size, float Thickness)
{
	const double H = Size * 0.5;
	DrawLine(PDI, P - FVector(H, 0, 0), P + FVector(H, 0, 0), Color, Thickness);
	DrawLine(PDI, P - FVector(0, H, 0), P + FVector(0, H, 0), Color, Thickness);
}

void DrawDiamondHandle(FPrimitiveDrawInterface* PDI, const FVector& P, const FLinearColor& Color,
	double Size, float Thickness)
{
	const double H = Size * 0.5;
	const FVector N = P + FVector(0, H, 0);
	const FVector E = P + FVector(H, 0, 0);
	const FVector S = P - FVector(0, H, 0);
	const FVector W = P - FVector(H, 0, 0);
	DrawLine(PDI, N, E, Color, Thickness);
	DrawLine(PDI, E, S, Color, Thickness);
	DrawLine(PDI, S, W, Color, Thickness);
	DrawLine(PDI, W, N, Color, Thickness);
}

void DrawCrossOutHandle(FPrimitiveDrawInterface* PDI, const FVector& P, const FLinearColor& Color,
	double Size, float Thickness)
{
	// Rotated 45 degrees off DrawCrossHandle, so a removal never reads as a corner at a glance.
	const double H = Size * 0.5 * UE_INV_SQRT_2;
	DrawLine(PDI, P + FVector(-H, -H, 0), P + FVector(H, H, 0), Color, Thickness);
	DrawLine(PDI, P + FVector(-H, H, 0), P + FVector(H, -H, 0), Color, Thickness);
}

bool TraceGround(UWorld* World, const FVector& RayOrigin, const FVector& RayDirection,
	double FallbackPlaneZ, FVector& OutHit)
{
	if (World)
	{
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(PlaceLabelEditGround), /*bTraceComplex*/ true);
		if (World->LineTraceSingleByChannel(
				Hit, RayOrigin, RayOrigin + RayDirection * 1.0e7, ECC_Visibility, Params))
		{
			OutHit = Hit.ImpactPoint;
			return true;
		}
	}

	// Nothing under the cursor: fall back to a horizontal plane.
	if (FMath::IsNearlyZero(RayDirection.Z))
	{
		return false;
	}
	const double T = (FallbackPlaneZ - RayOrigin.Z) / RayDirection.Z;
	if (T <= 0.0)
	{
		return false;
	}
	OutHit = RayOrigin + RayDirection * T;
	return true;
}

int32 ClosestVertex(const TArray<FVector2D>& Poly, const FVector2D& P, double& OutDistSq)
{
	OutDistSq = TNumericLimits<double>::Max();
	int32 Best = INDEX_NONE;
	for (int32 i = 0; i < Poly.Num(); ++i)
	{
		const double DistSq = FVector2D::DistSquared(Poly[i], P);
		if (DistSq < OutDistSq)
		{
			OutDistSq = DistSq;
			Best = i;
		}
	}
	return Best;
}

FEdgeHit ClosestEdge(const TArray<FVector2D>& Poly, const FVector2D& P, bool bClosed)
{
	FEdgeHit Result;

	const int32 N = Poly.Num();
	if (N < 2)
	{
		return Result;
	}

	const int32 EdgeCount = bClosed ? N : N - 1;
	for (int32 i = 0; i < EdgeCount; ++i)
	{
		const FVector2D& A = Poly[i];
		const FVector2D& B = Poly[(i + 1) % N];
		const FVector2D Closest = FMath::ClosestPointOnSegment2D(P, A, B);
		const double DistSq = FVector2D::DistSquared(Closest, P);
		if (DistSq < Result.DistSq)
		{
			Result.DistSq = DistSq;
			Result.EdgeIndex = i;
			Result.Point = Closest;
		}
	}
	return Result;
}

FSnapResult ResolveSnap(const FVector& TracedHit, const FSnapSettings& Settings,
	const FSnapQuery& Query)
{
	FSnapResult Result;
	Result.Point = TracedHit;

	const double Radius = FMath::Max(Settings.Radius, 0.0);
	if (Radius <= 0.0)
	{
		return Result;
	}

	double BestDistSq = Radius * Radius;
	const FVector2D HitXY(TracedHit.X, TracedHit.Y);

	// 1.
	if (Query.OwnPoints)
	{
		for (int32 i = 0; i < Query.OwnPoints->Num(); ++i)
		{
			if (i == Query.IgnoreOwnIndex)
			{
				continue;
			}
			const FVector& P = (*Query.OwnPoints)[i];
			const double DistSq = FVector2D::DistSquared(FVector2D(P.X, P.Y), HitXY);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Result.Point = FVector(P.X, P.Y, TracedHit.Z);
				Result.bSnapped = true;
				Result.Detail = FString::Printf(TEXT("corner %d"), i);
			}
		}
	}

	if (!Query.Regions)
	{
		return Result;
	}

	// 2. Vertices of regions already in the level.
	if (Settings.bToVertices)
	{
		for (const TWeakObjectPtr<UPlaceRegionComponent>& Weak : *Query.Regions)
		{
			const UPlaceRegionComponent* Region = Weak.Get();
			if (!Region || Region == Query.Exclude)
			{
				continue;
			}
			if (!Region->GetWorldBounds2D().ExpandBy(Radius).IsInside(HitXY))
			{
				continue;
			}

			const TArray<FVector2D>& Points = Region->GetWorldPoints2D();
			for (int32 i = 0; i < Points.Num(); ++i)
			{
				const double DistSq = FVector2D::DistSquared(Points[i], HitXY);
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					// Z from the trace, not the snap target: a snapped point still has to sit on the ground where it landed.
					Result.Point = FVector(Points[i].X, Points[i].Y, TracedHit.Z);
					Result.bSnapped = true;
					Result.Detail = FString::Printf(TEXT("%s corner %d"),
						*Region->Name.GetDisplayText().ToString(), i);
				}
			}
		}
	}

	// 3.
	if (!Result.bSnapped && Settings.bToEdges)
	{
		for (const TWeakObjectPtr<UPlaceRegionComponent>& Weak : *Query.Regions)
		{
			const UPlaceRegionComponent* Region = Weak.Get();
			if (!Region || Region == Query.Exclude)
			{
				continue;
			}
			if (!Region->GetWorldBounds2D().ExpandBy(Radius).IsInside(HitXY))
			{
				continue;
			}

			const FEdgeHit Hit = ClosestEdge(Region->GetWorldPoints2D(), HitXY, /*bClosed*/ true);
			if (Hit.EdgeIndex != INDEX_NONE && Hit.DistSq < BestDistSq)
			{
				BestDistSq = Hit.DistSq;
				Result.Point = FVector(Hit.Point.X, Hit.Point.Y, TracedHit.Z);
				Result.bSnapped = true;
				Result.Detail = FString::Printf(TEXT("%s edge %d"),
					*Region->Name.GetDisplayText().ToString(), Hit.EdgeIndex);
			}
		}
	}

	return Result;
}

void GatherRegions(UWorld* World, TArray<TWeakObjectPtr<UPlaceRegionComponent>>& OutRegions,
	bool bOnlyWithUsableOutline)
{
	OutRegions.Reset();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (UPlaceRegionComponent* Region = It->FindComponentByClass<UPlaceRegionComponent>())
		{
			if (bOnlyWithUsableOutline && Region->GetWorldPoints2D().Num() < 3)
			{
				continue;
			}
			OutRegions.Add(Region);
		}
	}
}

void GetRegionWorldPoints3D(const UPlaceRegionComponent* Region, TArray<FVector>& OutPoints)
{
	OutPoints.Reset();
	if (!Region)
	{
		return;
	}

	const FTransform& Xf = Region->GetComponentTransform();
	OutPoints.Reserve(Region->LocalPoints.Num());
	for (const FVector2D& P : Region->LocalPoints)
	{
		OutPoints.Add(Xf.TransformPosition(FVector(P.X, P.Y, 0.0)));
	}
}

} // namespace PlaceLabelsEdit
