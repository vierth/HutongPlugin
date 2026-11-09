#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "Generation/CityWallGenerator.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "ArrowTowerGenerator.generated.h"

// 箭樓: the arrow tower on its 城台 at the front of a barbican. Built with the front (outside the city) on -Y, the
// platform's min corner at the origin; the component turns it onto its facade.
USTRUCT(BlueprintType)
struct FHutongArrowTowerParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Platform", meta=(HutongBasic, DisplayName="City", ToolTip="Whose wall the platform and the barbican walls joining it are built to."))
	EHutongCityWallRank Rank = EHutongCityWallRank::Inner;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Platform", meta=(DisplayName="Projection", UIMin="0", UIMax="1500", ClampMin="0", Units="cm", ToolTip="How far the platform stands ahead of the barbican wall's outer face, in cm."))
	double Projection = HutongCanon::ArrowTower::ProjectionCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tower", meta=(DisplayName="Tower Width", UIMin="1000", UIMax="5000", ClampMin="500", Units="cm", ToolTip="Width of the tower across its front, in cm."))
	double TowerWidth = HutongCanon::ArrowTower::TowerWidthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tower", meta=(DisplayName="Tower Depth", UIMin="500", UIMax="2500", ClampMin="300", Units="cm", ToolTip="Depth of the tower, front to back, in cm."))
	double TowerDepth = HutongCanon::ArrowTower::TowerDepthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tower", meta=(DisplayName="Lower Storey Height", UIMin="400", UIMax="2000", ClampMin="200", Units="cm", ToolTip="Height of the tower from the platform to the lower eave (腰檐), in cm."))
	double LowerHeight = HutongCanon::ArrowTower::LowerHeightCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tower", meta=(DisplayName="Upper Storey Height", UIMin="200", UIMax="1000", ClampMin="100", Units="cm", ToolTip="Height of the upper storey from the lower eave to the upper eave, in cm."))
	double UpperHeight = HutongCanon::ArrowTower::UpperHeightCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arrow Windows (箭窗)", meta=(HutongBasic, DisplayName="Windows Across the Front", UIMin="1", UIMax="20", ClampMin="0", ToolTip="Arrow windows in each row across the front."))
	int32 FrontWindows = HutongCanon::ArrowTower::FrontWindows;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arrow Windows (箭窗)", meta=(DisplayName="Windows on Each Side", UIMin="0", UIMax="10", ClampMin="0", ToolTip="Arrow windows in each row on each side."))
	int32 SideWindows = HutongCanon::ArrowTower::SideWindows;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arrow Windows (箭窗)", meta=(DisplayName="Rows Below the Lower Eave", UIMin="1", UIMax="6", ClampMin="0", ToolTip="Rows of arrow windows below the lower eave; one more row above it."))
	int32 LowerRows = HutongCanon::ArrowTower::LowerRows;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Arrow Windows (箭窗)", meta=(DisplayName="Window Size", UIMin="30", UIMax="150", ClampMin="10", Units="cm", ToolTip="Width and height of each arrow window, in cm."))
	double WindowSize = HutongCanon::ArrowTower::WindowSizeCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rear Annex (抱廈)", meta=(HutongBasic, DisplayName="Rear Annex (抱廈)", ToolTip="Builds the lower annex against the back of the tower."))
	bool bAnnex = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rear Annex (抱廈)", meta=(DisplayName="Annex Width", EditCondition="bAnnex", UIMin="500", UIMax="5000", ClampMin="200", Units="cm", ToolTip="Width of the rear annex, in cm."))
	double AnnexWidth = HutongCanon::ArrowTower::AnnexWidthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rear Annex (抱廈)", meta=(DisplayName="Annex Depth", EditCondition="bAnnex", UIMin="200", UIMax="1500", ClampMin="100", Units="cm", ToolTip="Depth of the rear annex, in cm."))
	double AnnexDepth = HutongCanon::ArrowTower::AnnexDepthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Rear Annex (抱廈)", meta=(DisplayName="Annex Height", EditCondition="bAnnex", UIMin="300", UIMax="1500", ClampMin="150", Units="cm", ToolTip="Height of the rear annex to its eave, in cm."))
	double AnnexHeight = HutongCanon::ArrowTower::AnnexHeightCm;

	FHutongCityWallParams GetWall() const { FHutongCityWallParams W; W.ChooseRank(Rank); return W; }
	double GetPlatformHeight() const { return GetWall().GetHeight(); }
	double GetBatter() const { return GetWall().GetBatter(); }
	double GetLegBaseWidth() const { return GetWall().GetBaseWidth(); }
	double GetPlatformWidth() const
	{
		return FMath::Max(TowerWidth, AnnexWidth) + 2.0 * (GetBatter() + HutongCanon::ArrowTower::PlatformMarginCm);
	}
	double GetPlatformDepth() const
	{
		const double Need = TowerDepth + (bAnnex ? AnnexDepth : 0.0) + 2.0 * (GetBatter() + HutongCanon::ArrowTower::PlatformMarginCm);
		return FMath::Max3(Need, HutongCanon::ArrowTower::PlatformRearCm, FMath::Max(Projection, 0.0) + GetLegBaseWidth() + 1.0);
	}
	// Where the tower's front stands, and the leg band at each side.
	double GetTowerFrontY() const { return GetBatter() + HutongCanon::ArrowTower::PlatformMarginCm; }
	double GetLegOuterY() const { return FMath::Clamp(Projection, 0.0, GetPlatformDepth() - GetLegBaseWidth()); }
};

namespace HutongGen
{
	void BuildArrowTower(UE::Geometry::FDynamicMesh3& Mesh, const FHutongArrowTowerParams& P, bool bWindows, bool bTileRuns);

	// A storey of 箭窗: Front windows a row on Y0, Rear on Y1, SideStart on the X0 end and SideEnd on the X1 end,
	// one row at each of Rows' heights.
	void AppendArrowWindowStorey(UE::Geometry::FDynamicMesh3& Mesh, double X0, double X1, double Y0, double Y1,
		double Z0, double Z1, int32 Front, int32 SideStart, int32 SideEnd, int32 Rear, const TArray<double>& Rows,
		double Size, double Recess, bool bWindows);

	// 角箭樓: an L of two 箭樓 arms on a corner platform PlatformSize square, outer faces on X = 0 and Y = 0, standing
	// on its walk at Z; Margin from each outer face of the platform's foot.
	void AppendCornerArrowTower(UE::Geometry::FDynamicMesh3& Mesh, double PlatformSize, double Margin, double Z, bool bWindows);

	// 紫禁城角樓 centred at (Cx, Cy), standing on the walk at Z.
	void AppendPalaceCornerTower(UE::Geometry::FDynamicMesh3& Mesh, double Cx, double Cy, double Z, bool bDetail);
	// Its cross's width, side to side.
	double PalaceCornerTowerSpan();

	// 歇山 over a W × D plan from (X0, Y0), its eave at EaveZ, ridge along X.
	void AppendTowerXieshanRoof(UE::Geometry::FDynamicMesh3& Mesh, double X0, double Y0, double W, double D,
		double EaveZ, double Overhang, bool bHipStart = false, bool bHipEnd = false);
}
