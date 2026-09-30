#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "Tools/RectDragToolBase.h"
#include "Tools/HutongPresets.h"
#include "Generation/BaySide.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/ShopfrontGenerator.h"
#include "Generation/GateHouseGenerator.h"
#include "Generation/HutongCanon.h"
#include "StreetRowTool.generated.h"

// Ordinary bays of a street row: Houses (倒座房, windows and one lane door) or Shops (bay fully open).
UENUM()
enum class EHutongStreetRowKind : uint8
{
	Houses UMETA(DisplayName = "Houses (房)"),
	Shops UMETA(DisplayName = "Shops (鋪面房)"),
};

// Where a row of houses stands in its plots: lining the lane at the front, or closing them at the back.
UENUM()
enum class EHutongStreetRowPosition : uint8
{
	Front UMETA(DisplayName = "Front Row (倒座房)"),
	Rear UMETA(DisplayName = "Rear Row (後罩房)"),
};

UCLASS()
class UHutongStreetRowToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UHutongStreetRowToolProperties();

	UPROPERTY(EditAnywhere, Category="Row", meta=(ToolTip="What the bays that are not gates are built as."))
	EHutongStreetRowKind Kind = EHutongStreetRowKind::Houses;

	UPROPERTY(EditAnywhere, Category="Row", meta=(HutongBasic, DisplayName="Row Position", EditCondition="Kind == EHutongStreetRowKind::Houses", EditConditionHides, ToolTip="Whether the houses are the plots' front row (倒座房) or rear row (後罩房); picks their preset."))
	EHutongStreetRowPosition Position = EHutongStreetRowPosition::Front;

	UPROPERTY(EditAnywhere, Category="Row", meta=(DisplayName="Bay Count Override", UIMin="0", UIMax="32", ClampMin="0", ClampMax="32", ToolTip="Forces the number of bays; zero derives it from the length."))
	int32 BayCountOverride = 0;

	UPROPERTY(EditAnywhere, Category="Row", meta=(DisplayName="Gate (大門) Ridge Above Row", UIMin="0", UIMax="120", ClampMin="0", Units="cm", ToolTip="Height of each gate's ridge above the row's, in cm; zero leaves it."))
	double GateRidgeClearance = HutongCanon::Gate::RidgeAboveRowCm;

	UPROPERTY(EditAnywhere, Category="Buildings|House (房)", meta=(ToolTip="Parameters of the houses (房) built on the ordinary bays."))
	FHutongSiheyuanParams House;

	UPROPERTY(EditAnywhere, Category="Buildings|Shop (鋪面房)", meta=(ToolTip="Parameters of the shops (鋪面房) built on the ordinary bays."))
	FHutongShopfrontParams Shop;

	UPROPERTY(EditAnywhere, Category="Buildings|Gate (大門)", meta=(ToolTip="Parameters of the gate house (大門) on each gate bay."))
	FHutongGateHouseParams Gate;
};

// Street row from one drag: houses or shops, gate houses on picked bays. Gates face the buildings' way
// unless F turns them (a gate behind shops looks like the shops; beside houses often not).
UCLASS()
class UHutongStreetRowTool : public URectDragToolBase
{
	GENERATED_BODY()
public:
	virtual void AdjustHeight(double DeltaCm) override;
	virtual void AdjustBracketValue(int32 Delta, bool bFine, bool bCoarse) override;
	virtual double GetPreviewHeight() const override;
	virtual FText GetStagePromptText() const override;
	virtual TArray<FText> GetStageNames() const override;
	virtual int32 GetStageIndex() const override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual TArray<FText> GetToolHelpLines() const override;
	virtual void CancelPlacement() override;
	virtual void FlipFacing() override;
	virtual FText GetKeyHintText() const override;
	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

protected:
	virtual void RegisterToolSettings() override;
	virtual void BuildMeshForRect(double SizeX, double SizeY, UE::Geometry::FDynamicMesh3& OutMesh,
		EHutongDetail Level) override {}
	virtual void SpawnFinalActor() override;
	virtual FString GetActorNameBase() const override { return TEXT("Hutong_Row"); }
	virtual FString GetPlacementDetail() const override;

	virtual void OnPlacementStarted(const FVector& HitWorld) override;
	virtual bool OnRectCommitted(const FVector& HitWorld) override;
	virtual bool OnExtraStageClicked(const FVector& HitWorld) override;
	virtual void OnPlacementHover(const FVector& HitWorld) override;

	// Clicks after the rectangle, in order.
	enum class EExtra : uint8 { BuildingsFace, GateBays };
	EExtra Extra = EExtra::BuildingsFace;

	// Run is the longer extent; facades are the two long sides.
	bool IsRunAlongX() const;
	double RunLength() const;
	double RowDepth() const;
	int32 GetBayCount() const;
	// Bay a world point lands in, measured along the run from its start.
	int32 BayUnder(const FVector& World) const;
	HutongGen::EBaySide SideUnder(const FVector& World) const;

	// One piece as a local-rect footprint.
	void PieceRect(double From, double To, double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const;

	UPROPERTY()
	TObjectPtr<UHutongStreetRowToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> HousePresets;

	HutongGen::EBaySide BuildingSide = HutongGen::EBaySide::MinusY;
	// Relative to the buildings' side, so gates follow when that side flips.
	bool bGatesFaceBack = false;
	HutongGen::EBaySide GateSide() const
	{
		return bGatesFaceBack ? HutongGen::BaySide::Opposite(BuildingSide) : BuildingSide;
	}
	TSet<int32> GateBays;
	int32 HoverBay = INDEX_NONE;
};

UCLASS()
class UHutongStreetRowToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override { return true; }
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};
