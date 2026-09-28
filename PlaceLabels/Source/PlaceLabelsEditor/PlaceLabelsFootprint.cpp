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

	// Shared with HutongLayout's panel: hidden plans withdraw the trace offer. Unregistered without
	// HutongLayout; the class lookup then answers.
	bool ArePlansShown()
	{
		static IConsoleVariable* Var =
			IConsoleManager::Get().FindConsoleVariable(TEXT("hutong.ShowPlanOutlines"));
		return Var == nullptr || Var->GetBool();
	}

	UClass* ResolveBuildingComponentClass()
	{
		// Static: without HutongLayout, one failed lookup, not one per hover tick.
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

	// Mirrors `void GetFootprintCornersLocal(FVector2D& C0, FVector2D& C1, FVector2D& C2, FVector2D& C3) const`:
	// skewed corners. Missing on older HutongLayout → rectangle.
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

		// Mesh origin at the footprint's min corner; corners include any skew.
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
