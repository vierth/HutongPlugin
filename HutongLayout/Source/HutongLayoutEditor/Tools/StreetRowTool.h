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

// What the ordinary bays of a street row are built as. Houses are the 倒座房 kind: a facade of
// windows and one door to the lane. Shops open the whole bay.
UENUM()
enum class EHutongStreetRowKind : uint8
{
	Houses UMETA(DisplayName = "Houses (房)"),
	Shops UMETA(DisplayName = "Shops (鋪面房)"),
};

UCLASS()
class UHutongStreetRowToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UHutongStreetRowToolProperties();

	UPROPERTY(EditAnywhere, Category="Row", meta=(ToolTip="What the bays that are not gates are built as."))
	EHutongStreetRowKind Kind = EHutongStreetRowKind::Houses;

	UPROPERTY(EditAnywhere, Category="Row", meta=(DisplayName="Bay Count Override", UIMin="0", UIMax="32", ClampMin="0", ClampMax="32", ToolTip="Forces the number of bays the row is divided into; zero derives it from the length and the building's bay width limits."))
	int32 BayCountOverride = 0;

	UPROPERTY(EditAnywhere, Category="Row", meta=(DisplayName="Gate (大門) Ridge Above Row", UIMin="0", UIMax="120", ClampMin="0", Units="cm", ToolTip="How far the ridge of each gate stands above the row's ridge, in cm; zero leaves the gate at its own height."))
	double GateRidgeClearance = HutongCanon::Gate::RidgeAboveRowCm;

	UPROPERTY(EditAnywhere, Category="Buildings|House (房)", meta=(ToolTip="Parameters of the houses (房) built on the ordinary bays."))
	FHutongSiheyuanParams House;

	UPROPERTY(EditAnywhere, Category="Buildings|Shop (鋪面房)", meta=(ToolTip="Parameters of the shops (鋪面房) built on the ordinary bays."))
	FHutongShopfrontParams Shop;

	UPROPERTY(EditAnywhere, Category="Buildings|Gate (大門)", meta=(ToolTip="Parameters of the gate house (大門) built on each gate bay. Its footprint is the bay's, whatever the style's band says."))
	FHutongGateHouseParams Gate;
};

// A street row from one drag: houses or shops along it, with a gate house on the bays picked
// for one. The buildings and the gates face independently, since a gate to a courtyard behind
// a row of shops looks the way the shops look, and one beside houses often does not.
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

	// The clicks after the rectangle, in order.
	enum class EExtra : uint8 { BuildingsFace, GateBays, GateFaces };
	EExtra Extra = EExtra::BuildingsFace;

	// The run is the longer extent; the facades are the two sides across it.
	bool IsRunAlongX() const;
	double RunLength() const;
	double RowDepth() const;
	int32 GetBayCount() const;
	// Along the run from the row's start, for a world point; the bay it lands in.
	int32 BayUnder(const FVector& World) const;
	HutongGen::EBaySide SideUnder(const FVector& World) const;

	// One piece as a local-rect footprint.
	void PieceRect(double From, double To, double& OutMinX, double& OutMinY, double& OutMaxX, double& OutMaxY) const;

	UPROPERTY()
	TObjectPtr<UHutongStreetRowToolProperties> Settings;

	UPROPERTY()
	TObjectPtr<UHutongPresetProperties> HousePresets;

	HutongGen::EBaySide BuildingSide = HutongGen::EBaySide::MinusY;
	HutongGen::EBaySide GateSide = HutongGen::EBaySide::MinusY;
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
