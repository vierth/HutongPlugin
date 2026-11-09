#pragma once

#include "CoreMinimal.h"

class AActor;
class UHutongBuildingComponent;

namespace HutongOverlaps
{
	inline constexpr double OfferShare = 0.6;

	double OverlapShare(const TArray<FVector2D>& A, const TArray<FVector2D>& B);

	TArray<FVector2D> WorldFootprint(const UHutongBuildingComponent* Building);

	struct FPair
	{
		TWeakObjectPtr<UHutongBuildingComponent> First;
		TWeakObjectPtr<UHutongBuildingComponent> Second;
		double Share = 0.0;
	};

	// First is the one already there when only Second is a candidate.
	TArray<FPair> Find(const TArray<UHutongBuildingComponent*>& Candidates,
		const TArray<UHutongBuildingComponent*>& Against);

	void TransferMetadata(const UHutongBuildingComponent* From, UHutongBuildingComponent* To);

	void OpenWindow(const TArray<FPair>& Pairs);

	void CheckAfterImport(const TArray<TWeakObjectPtr<AActor>>& Placed);
}
