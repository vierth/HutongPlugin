#pragma once

#include "CoreMinimal.h"

class AActor;
class UHutongBuildingComponent;

// Two buildings standing mostly on one footprint under different ids: the same building traced twice,
// or imported beside itself. Found, listed in a window, and resolved there by keeping one.
namespace HutongOverlaps
{
	// Share of the smaller footprint the other covers above which the pair is offered.
	inline constexpr double OfferShare = 0.6;

	// Overlap of two convex polygons as a share of the smaller one's area (0 to 1).
	double OverlapShare(const TArray<FVector2D>& A, const TArray<FVector2D>& B);

	// A building's footprint in world XY.
	TArray<FVector2D> WorldFootprint(const UHutongBuildingComponent* Building);

	struct FPair
	{
		TWeakObjectPtr<UHutongBuildingComponent> First;
		TWeakObjectPtr<UHutongBuildingComponent> Second;
		double Share = 0.0;
	};

	// Pairs above OfferShare with at least one member in Candidates, the other in Against (either may be
	// the same list). First is the one already there when only Second is a candidate.
	TArray<FPair> Find(const TArray<UHutongBuildingComponent*>& Candidates,
		const TArray<UHutongBuildingComponent*>& Against);

	// Notes appended, the higher confidence, and court and role where the keeper has none.
	void TransferMetadata(const UHutongBuildingComponent* From, UHutongBuildingComponent* To);

	// The cleanup window for these pairs; nothing opens for none.
	void OpenWindow(const TArray<FPair>& Pairs);

	// After an import: the placed actors against everything loaded; opens the window if any overlap.
	void CheckAfterImport(const TArray<TWeakObjectPtr<AActor>>& Placed);
}
