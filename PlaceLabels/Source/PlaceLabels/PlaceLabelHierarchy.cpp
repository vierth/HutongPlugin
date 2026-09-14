#include "PlaceLabelHierarchy.h"

#include "PlaceLabelGeometry.h"
#include "PlaceRegionComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

namespace PlaceLabelsHierarchy
{
	namespace
	{
		// Deep enough to cross any plausible hierarchy, shallow enough to terminate on corrupt data.
		constexpr int32 MaxWalkDepth = 32;

		bool IsUsableCandidate(const UPlaceRegionComponent* Candidate, const UPlaceRegionComponent* Region)
		{
			return Candidate
				&& Candidate != Region
				&& Candidate->GetWorldPoints2D().Num() >= 3
				&& Candidate->GetOwner() != nullptr;
		}
	}

	void CollectRegions(UWorld* World, TArray<UPlaceRegionComponent*>& OutRegions)
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
				OutRegions.Add(Region);
			}
		}
	}

	bool WouldCreateCycle(const UPlaceRegionComponent* Child, const UPlaceRegionComponent* Proposed)
	{
		const UPlaceRegionComponent* Current = Proposed;
		for (int32 i = 0; Current && i < MaxWalkDepth; ++i)
		{
			if (Current == Child)
			{
				return true;
			}
			Current = Current->GetEffectiveParent();
		}
		// Ran off the top, or hit the cap on data that is already broken.
		return false;
	}

	UPlaceRegionComponent* ResolveParentFor(
		UPlaceRegionComponent* Region, const TArray<UPlaceRegionComponent*>& AllRegions)
	{
		if (!Region || Region->GetWorldPoints2D().Num() < 3)
		{
			return nullptr;
		}

		// An explicit parent is a statement that the geometry lies. Nothing is derived.
		if (Region->ExplicitParent)
		{
			return Region->ExplicitParent->FindComponentByClass<UPlaceRegionComponent>();
		}

		const TArray<FVector2D>& RegionPoints = Region->GetWorldPoints2D();
		const FVector2D Representative = PlaceLabelsGeo::RepresentativePoint2D(RegionPoints);

		const UPlaceLabelTypeAsset* Type = Region->Type;

		if (Type && Type->ParentTypes.Num() > 0)
		{
			// Ordered: the first type that yields any match wins outright.
			for (const FName& ParentTypeId : Type->ParentTypes)
			{
				if (ParentTypeId.IsNone())
				{
					continue;
				}

				UPlaceRegionComponent* Best = nullptr;
				double BestArea = TNumericLimits<double>::Max();
				double BestDistance = TNumericLimits<double>::Max();

				for (UPlaceRegionComponent* Candidate : AllRegions)
				{
					if (!IsUsableCandidate(Candidate, Region)
						|| Candidate->GetTypeId() != ParentTypeId
						|| WouldCreateCycle(Region, Candidate))
					{
						continue;
					}

					const double CandidateArea = Candidate->GetWorldArea();

					if (Type->ParentRelation == EPlaceParentRelation::Containing)
					{
						if (!Candidate->ContainsWorldPoint2D(Representative))
						{
							continue;
						}
						if (CandidateArea < BestArea)
						{
							Best = Candidate;
							BestArea = CandidateArea;
						}
					}
					else
					{
						// Adjacent: the compound-fronting-a-hutong case.
						const double Radius = FMath::Max(Type->ParentSearchRadius, 0.0);
						if (!Region->GetWorldBounds2D().ExpandBy(Radius)
								 .Intersect(Candidate->GetWorldBounds2D()))
						{
							continue;
						}

						const double Distance = PlaceLabelsGeo::DistancePolygonToPolygon2D(
							RegionPoints, Candidate->GetWorldPoints2D(), Radius);
						if (Distance > Radius)
						{
							continue;
						}

						// Nearest wins; a tie between two lanes goes to the smaller one.
						if (Distance < BestDistance
							|| (FMath::IsNearlyEqual(Distance, BestDistance) && CandidateArea < BestArea))
						{
							Best = Candidate;
							BestDistance = Distance;
							BestArea = CandidateArea;
						}
					}
				}

				if (Best)
				{
					return Best;
				}
			}
		}

		// Fallback for untyped regions and for types whose rule found nothing.
		UPlaceRegionComponent* Fallback = nullptr;
		double FallbackArea = TNumericLimits<double>::Max();

		for (UPlaceRegionComponent* Candidate : AllRegions)
		{
			if (!IsUsableCandidate(Candidate, Region)
				|| !Candidate->ContainsWorldPoint2D(Representative)
				|| WouldCreateCycle(Region, Candidate))
			{
				continue;
			}

			const double CandidateArea = Candidate->GetWorldArea();
			if (CandidateArea < FallbackArea)
			{
				Fallback = Candidate;
				FallbackArea = CandidateArea;
			}
		}

		return Fallback;
	}

	int32 RecomputeAll(UWorld* World)
	{
		TArray<UPlaceRegionComponent*> Regions;
		CollectRegions(World, Regions);
		if (Regions.Num() == 0)
		{
			return 0;
		}

		// Coarsest first.
		Regions.Sort([](const UPlaceRegionComponent& A, const UPlaceRegionComponent& B)
		{
			return A.GetDisplayPriority() < B.GetDisplayPriority();
		});

		int32 ChangedCount = 0;
		for (UPlaceRegionComponent* Region : Regions)
		{
			if (!Region || !Region->bAutoParent || Region->ExplicitParent)
			{
				continue;
			}
			if (Region->RecomputeDerivedParent())
			{
				++ChangedCount;
			}
		}
		return ChangedCount;
	}
}
