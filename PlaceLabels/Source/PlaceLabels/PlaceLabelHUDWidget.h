#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PlaceLabelTypes.h"
#include "PlaceLabelHUDWidget.generated.h"

// Base class for the on-screen place readout.
UCLASS(Abstract, BlueprintType, Blueprintable)
class UPlaceLabelHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Innermost region first, then its parents outward. Empty when the player is in no region.
	UPROPERTY(BlueprintReadOnly, Category = "Place Labels")
	TArray<FPlaceLabelEntry> Chain;

	// Chain[0], without the IsValidIndex check every graph would otherwise need.
	UFUNCTION(BlueprintPure, Category = "Place Labels")
	bool GetInnermost(FPlaceLabelEntry& OutEntry) const;

	// Fires only when the chain actually changes, never per tick. Chain is already populated.
	UFUNCTION(BlueprintImplementableEvent, Category = "Place Labels",
		meta = (DisplayName = "On Place Chain Changed"))
	void OnPlaceChainChanged();

	// Fires when the chain goes from non-empty to empty.
	UFUNCTION(BlueprintImplementableEvent, Category = "Place Labels",
		meta = (DisplayName = "On Left All Regions"))
	void OnLeftAllRegions();

	// Called by UPlaceLabelSubsystem. Not Blueprint-callable: this widget is a view.
	virtual void SetChain(const TArray<FPlaceLabelEntry>& InChain);
};

// A plain readout so the plugin shows something before anyone has authored a widget Blueprint.
UCLASS()
class UPlaceLabelDefaultHUDWidget : public UPlaceLabelHUDWidget
{
	GENERATED_BODY()

public:
	virtual void SetChain(const TArray<FPlaceLabelEntry>& InChain) override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	void RebuildLines();

	TSharedPtr<class SVerticalBox> LinesBox;
};
