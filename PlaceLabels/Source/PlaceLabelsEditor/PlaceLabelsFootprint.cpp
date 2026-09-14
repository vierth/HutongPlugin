#include "PlaceLabelsFootprint.h"

#include "PlaceLabelGeometry.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

namespace PlaceLabelsFootprint
{

namespace
{
	// Resolved once and cached.
	const TCHAR* BuildingComponentPath = TEXT("/Script/HutongLayoutEditor.HutongBuildingComponent");
	const TCHAR* FootprintFunctionName = TEXT("GetFootprintSize");

	// The switch HutongLayout's own panel and this plugin's checkbox both drive. With the plans
	// hidden the buildings are out of the way on purpose, so the offer to trace one is withdrawn
	// with them: an offer to click a rectangle nobody can see is worse than no offer at all.
	// Absent HutongLayout the variable is unregistered, and the class lookup below answers anyway.
	bool ArePlansShown()
	{
		static IConsoleVariable* Var =
			IConsoleManager::Get().FindConsoleVariable(TEXT("hutong.ShowPlanOutlines"));
		return Var == nullptr || Var->GetBool();
	}

	UClass* ResolveBuildingComponentClass()
	{
		// Static, so a project without HutongLayout pays one failed lookup rather than one per hover tick.
		static bool bResolved = false;
		static TWeakObjectPtr<UClass> Cached;

		if (!bResolved)
		{
			bResolved = true;
			Cached = FindObject<UClass>(nullptr, BuildingComponentPath);
		}
		return Cached.Get();
	}

	// Mirrors the UFunction's generated parameter struct for `FVector2D GetFootprintSize() const`.
	struct FGetFootprintSizeParams
	{
		FVector2D ReturnValue = FVector2D::ZeroVector;
	};

	// Mirrors `void GetFootprintCornersLocal(FVector2D& C0, FVector2D& C1, FVector2D& C2, FVector2D& C3) const`,
	// the placement's own four corners, which a skewed footprint moves off the rectangle. Absent
	// on a HutongLayout that predates it, and the rectangle is used instead.
	struct FGetFootprintCornersParams
	{
		FVector2D C0 = FVector2D::ZeroVector;
		FVector2D C1 = FVector2D::ZeroVector;
		FVector2D C2 = FVector2D::ZeroVector;
		FVector2D C3 = FVector2D::ZeroVector;
	};

	bool ReadFootprintCorners(UActorComponent* Component, const FVector2D& Size, FVector2D OutCorners[4])
	{
		OutCorners[0] = FVector2D(0.0, 0.0);
		OutCorners[1] = FVector2D(Size.X, 0.0);
		OutCorners[2] = FVector2D(Size.X, Size.Y);
		OutCorners[3] = FVector2D(0.0, Size.Y);
		UFunction* Function = Component->FindFunction(FName(TEXT("GetFootprintCornersLocal")));
		if (!Function || Function->ParmsSize != sizeof(FGetFootprintCornersParams)) return false;
		FGetFootprintCornersParams Params;
		Component->ProcessEvent(Function, &Params);
		OutCorners[0] = Params.C0; OutCorners[1] = Params.C1;
		OutCorners[2] = Params.C2; OutCorners[3] = Params.C3;
		return true;
	}

	bool ReadFootprintSize(UActorComponent* Component, FVector2D& OutSize)
	{
		UFunction* Function = Component->FindFunction(FName(FootprintFunctionName));
		if (!Function || Function->ParmsSize != sizeof(FGetFootprintSizeParams))
		{
			return false;
		}

		FGetFootprintSizeParams Params;
		Component->ProcessEvent(Function, &Params);
		OutSize = Params.ReturnValue;
		return OutSize.X > 1.0 && OutSize.Y > 1.0;
	}
}

bool IsAvailable()
{
	return ResolveBuildingComponentClass() != nullptr && ArePlansShown();
}

bool FindFootprintNear(UWorld* World, const FVector& WorldPoint, double SearchRadius,
	FFootprintHit& OutHit)
{
	UClass* ComponentClass = ResolveBuildingComponentClass();
	if (!World || !ComponentClass || !ArePlansShown())
	{
		return false;
	}

	const FVector2D QueryXY(WorldPoint.X, WorldPoint.Y);

	bool bFound = false;
	double BestScore = TNumericLimits<double>::Max();
	double BestArea = TNumericLimits<double>::Max();

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		UActorComponent* Component = Actor ? Actor->FindComponentByClass(ComponentClass) : nullptr;
		if (!Component)
		{
			continue;
		}

		FVector2D Size;
		if (!ReadFootprintSize(Component, Size))
		{
			continue;
		}

		// The generated mesh has its origin at the footprint's min corner; the corners are the
		// placement's own, off the rectangle where it has been angled.
		const FTransform& Xf = Actor->GetActorTransform();
		FVector2D Local[4];
		ReadFootprintCorners(Component, Size, Local);
		TArray<FVector> Corners;
		for (const FVector2D& C : Local) Corners.Add(Xf.TransformPosition(FVector(C.X, C.Y, 0.0)));

		TArray<FVector2D> Flat;
		Flat.Reserve(4);
		for (const FVector& C : Corners)
		{
			Flat.Emplace(C.X, C.Y);
		}

		const bool bInside = PlaceLabelsGeo::PointInPolygon2D(Flat, QueryXY);
		const double Distance = bInside ? 0.0
			: PlaceLabelsGeo::DistancePointToPolygon2D(Flat, QueryXY);

		if (!bInside && Distance > SearchRadius)
		{
			continue;
		}

		const double Area = FMath::Abs(PlaceLabelsGeo::SignedArea2D(Flat));

		// Containment beats proximity; among containing footprints the smallest wins.
		const double Score = bInside ? 0.0 : Distance;
		if (Score < BestScore || (FMath::IsNearlyEqual(Score, BestScore) && Area < BestArea))
		{
			BestScore = Score;
			BestArea = Area;
			OutHit.Corners = MoveTemp(Corners);
			OutHit.Label = Actor->GetActorLabel();
			OutHit.Distance = Distance;
			bFound = true;
		}
	}

	return bFound;
}

} // namespace PlaceLabelsFootprint
