#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "Generation/CityWallGenerator.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "CityGateGenerator.generated.h"

UENUM(BlueprintType)
enum class EHutongCityGateTower : uint8
{
	None   UMETA(DisplayName = "None", ToolTip = "The passage alone, the walk running over it."),
	Zhalou UMETA(DisplayName = "Barbican Gate Tower (閘樓)", ToolTip = "A small tower with two rows of arrow windows over the barbican's gate."),
	Chenglou UMETA(DisplayName = "Gate Tower (城樓)", ToolTip = "The two-storey gate tower over a city's main gate, in a colonnade under a double-eaved hip-and-gable roof."),
};

// 城門: a length of city wall with an arched passage through it and a tower over it; the legs either side abut its
// ends. Built along +X with the outside on -Y, as a leg is; the component turns it onto its front.
USTRUCT(BlueprintType)
struct FHutongCityGateParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(HutongBasic, DisplayName="City", ToolTip="Whose wall the gate is built in, and the legs either side joining it."))
	EHutongCityWallRank Rank = EHutongCityWallRank::Inner;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(HutongBasic, DisplayName="Tower", ToolTip="What stands over the passage."))
	EHutongCityGateTower Tower = EHutongCityGateTower::Zhalou;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Length", UIMin="800", UIMax="6000", ClampMin="300", Units="cm", ToolTip="Length of the gate along the wall, in cm."))
	double Length = HutongCanon::CityGate::LengthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate", meta=(DisplayName="Inner Extension", UIMin="0", UIMax="3000", ClampMin="0", Units="cm", ToolTip="How far the gate's platform reaches into the city past the wall's inner face, in cm; a gate tower widens it as it needs."))
	double InnerExtension = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage (門洞)", meta=(DisplayName="Passage Width", UIMin="300", UIMax="1200", ClampMin="150", Units="cm", ToolTip="Width of the passage through the wall, in cm."))
	double PassageWidth = HutongCanon::CityGate::PassageWidthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Passage (門洞)", meta=(DisplayName="Passage Height", UIMin="300", UIMax="1500", ClampMin="200", Units="cm", ToolTip="Height of the passage to the crown of its arch, in cm."))
	double PassageHeight = HutongCanon::CityGate::PassageHeightCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tower", meta=(DisplayName="Tower Width", UIMin="500", UIMax="3000", ClampMin="200", Units="cm", ToolTip="Width of the tower along the wall, in cm."))
	double TowerWidth = HutongCanon::CityGate::TowerWidthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tower", meta=(DisplayName="Tower Depth", UIMin="300", UIMax="2000", ClampMin="200", Units="cm", ToolTip="Depth of the tower across the wall, in cm."))
	double TowerDepth = HutongCanon::CityGate::TowerDepthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tower", meta=(DisplayName="Tower Height", UIMin="300", UIMax="1500", ClampMin="150", Units="cm", ToolTip="Height of the tower from the walk to its eave, in cm."))
	double TowerHeight = HutongCanon::CityGate::TowerHeightCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tower", meta=(DisplayName="Windows Across the Front", UIMin="0", UIMax="12", ClampMin="0", ToolTip="Arrow windows in each row across the front."))
	int32 FrontWindows = HutongCanon::CityGate::TowerFrontWindows;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tower", meta=(DisplayName="Rows of Windows", UIMin="0", UIMax="4", ClampMin="0", ToolTip="Rows of arrow windows on the tower."))
	int32 Rows = HutongCanon::CityGate::TowerRows;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Tower (城樓)", meta=(DisplayName="Gate Tower Width", EditCondition="Tower == EHutongCityGateTower::Chenglou", UIMin="1000", UIMax="5000", ClampMin="500", Units="cm", ToolTip="Width of the gate tower along the wall, to its column line, in cm."))
	double GateTowerWidth = HutongCanon::CityGate::GateTowerWidthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Tower (城樓)", meta=(DisplayName="Gate Tower Depth", EditCondition="Tower == EHutongCityGateTower::Chenglou", UIMin="600", UIMax="3000", ClampMin="400", Units="cm", ToolTip="Depth of the gate tower across the wall, to its column line, in cm."))
	double GateTowerDepth = HutongCanon::CityGate::GateTowerDepthCm;

	FHutongCityWallParams GetWall() const { FHutongCityWallParams W; W.ChooseRank(Rank); return W; }
	// The platform's top holds the gate tower with a margin.
	double GetTowerNeedX() const { return GateTowerWidth + 2.0 * (GetWall().GetBatter() + HutongCanon::CityGate::GateTowerPlatformMarginCm); }
	double GetTowerNeedY() const { return GateTowerDepth + 2.0 * (GetWall().GetBatter() + HutongCanon::CityGate::GateTowerPlatformMarginCm); }
	double GetLength() const
	{
		const double L = FMath::Max(Length, GetPassageWidth() + 200.0);
		return Tower == EHutongCityGateTower::Chenglou ? FMath::Max(L, GetTowerNeedX()) : L;
	}
	double GetDepth() const
	{
		const double B = GetWall().GetBaseWidth();
		const double D = B + FMath::Max(InnerExtension, 0.0);
		return Tower == EHutongCityGateTower::Chenglou ? FMath::Max(D, GetTowerNeedY()) : D;
	}
	double GetPassageWidth() const { return FMath::Max(PassageWidth, 150.0); }
	// The crown under the walk by at least the arch ring; the springing a half width under the crown.
	double GetPassageHeight() const
	{
		const FHutongCityWallParams W = GetWall();
		return FMath::Clamp(PassageHeight, 0.5 * GetPassageWidth() + 100.0, W.GetHeight() - HutongCanon::CityGate::ArchRingCm - 10.0);
	}
};

namespace HutongGen
{
	void BuildCityGate(UE::Geometry::FDynamicMesh3& Mesh, const FHutongCityGateParams& P, bool bDetail);
}
