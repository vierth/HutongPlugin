#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PlaceLabelTypes.h"
#include "PlaceLabelSubsystem.generated.h"

class UPlaceRegionComponent;
class UPlaceLabelHUDWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnPlaceLabelChainChanged, const TArray<FPlaceLabelEntry>&, Chain);

// Tracks which region the local player is standing in and hands the resulting chain of place names to the readout widget.
UCLASS()
class UPlaceLabelSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// Regions register themselves from BeginPlay.
	void RegisterRegion(UPlaceRegionComponent* Region);
	void UnregisterRegion(UPlaceRegionComponent* Region);

	// Fires only when the resolved chain actually changes, not on every poll.
	UPROPERTY(BlueprintAssignable, Category = "Place Labels")
	FOnPlaceLabelChainChanged OnChainChanged;

	UFUNCTION(BlueprintPure, Category = "Place Labels")
	const TArray<FPlaceLabelEntry>& GetCurrentChain() const { return CurrentChain; }

	// Public so a game can drive the query from its own pawn instead of the built-in poll.
	UFUNCTION(BlueprintCallable, Category = "Place Labels")
	void UpdateForLocation(const FVector& WorldLocation, bool bForce = false);

	// Highest display priority wins, smallest area breaks the tie.
	UPlaceRegionComponent* FindInnermostRegion(const FVector& WorldLocation) const;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(UPlaceLabelSubsystem, STATGROUP_Tickables);
	}
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	void BuildChain(UPlaceRegionComponent* Innermost, TArray<FPlaceLabelEntry>& OutChain) const;

	// True when the new chain differs from the current one by length or by any region.
	bool ChainDiffers(const TArray<FPlaceLabelEntry>& NewChain) const;

	bool TryGetPlayerLocation(FVector& OutLocation) const;
	void TryCreateHUD();

	UPROPERTY()
	TArray<TWeakObjectPtr<UPlaceRegionComponent>> Regions;

	UPROPERTY()
	TArray<FPlaceLabelEntry> CurrentChain;

	UPROPERTY()
	TObjectPtr<UPlaceLabelHUDWidget> HUDWidget;

	float Accumulator = 0.0f;
	FVector LastQueryLocation = FVector(TNumericLimits<double>::Max());
	bool bHasQueried = false;

	// The local player controller often does not exist yet at world BeginPlay.
	bool bHUDCreationPending = false;
};
