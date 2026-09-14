#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PlaceLabelSettings.generated.h"

class UPlaceLabelHUDWidget;

// Project Settings → Plugins → Place Labels.
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Place Labels"))
class UPlaceLabelSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPlaceLabelSettings();

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("Place Labels"); }

	// Turn off if your own HUD owns widget lifetime; the subsystem still resolves the chain and broadcasts On Chain Changed.
	UPROPERTY(config, EditAnywhere, Category = "HUD")
	bool bAutoCreateHUDWidget = true;

	// Soft class pointer on purpose.
	UPROPERTY(config, EditAnywhere, Category = "HUD",
		meta = (EditCondition = "bAutoCreateHUDWidget"))
	TSoftClassPtr<UPlaceLabelHUDWidget> HUDWidgetClass;

	UPROPERTY(config, EditAnywhere, Category = "HUD",
		meta = (EditCondition = "bAutoCreateHUDWidget"))
	int32 HUDZOrder = 0;

	// How often the player's position is tested against the regions.
	UPROPERTY(config, EditAnywhere, Category = "Query",
		meta = (ClampMin = "0.0", UIMax = "1.0", Units = "s"))
	float QueryInterval = 0.2f;

	// Standing still costs one distance compare per poll instead of a full region sweep.
	UPROPERTY(config, EditAnywhere, Category = "Query",
		meta = (ClampMin = "0.0", Units = "cm"))
	float MinMoveToRequery = 50.0f;

	UPROPERTY(config, EditAnywhere, Category = "Query", meta = (ClampMin = "0"))
	int32 PlayerIndex = 0;

	// How far up the parent chain the readout is allowed to walk.
	UPROPERTY(config, EditAnywhere, Category = "Query", meta = (ClampMin = "1", UIMax = "16"))
	int32 MaxChainDepth = 16;
};
