#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PlaceRegionActor.generated.h"

class UPlaceRegionComponent;

// What the pen tool spawns: an actor whose root is a UPlaceRegionComponent.
UCLASS(PrioritizeCategories = "Place Label")
class PLACELABELS_API APlaceRegionActor : public AActor
{
	GENERATED_BODY()

public:
	APlaceRegionActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Place Label")
	TObjectPtr<UPlaceRegionComponent> Region;

#if WITH_EDITORONLY_DATA
	// A clickable icon at the region's centre.
	UPROPERTY()
	TObjectPtr<class UBillboardComponent> SpriteComponent;
#endif

#if WITH_EDITOR
	// Re-runs the type-driven parenting rule for this region alone.
	UFUNCTION(CallInEditor, Category = "Place Label|Hierarchy",
		meta = (DisplayName = "Recompute Parent"))
	void RecomputeParent();

	// Clears Explicit Parent so auto-parenting applies again.
	UFUNCTION(CallInEditor, Category = "Place Label|Hierarchy",
		meta = (DisplayName = "Clear Explicit Parent"))
	void ClearExplicitParent();

	virtual void PostEditMove(bool bFinished) override;
	virtual FString GetDefaultActorLabel() const override;
#endif
};
