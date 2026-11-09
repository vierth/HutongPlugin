#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongCanon.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "CityWallGenerator.generated.h"

UENUM(BlueprintType)
enum class EHutongCityWallRank : uint8
{
	Inner UMETA(DisplayName = "Inner City (內城)", ToolTip = "The inner city's wall: about 12 m high and 20 m thick at the foot."),
	Outer UMETA(DisplayName = "Outer City (外城)", ToolTip = "The outer city's lower, thinner wall: about 6.4 m high and 6.4 m at the foot."),
	Palace UMETA(DisplayName = "Palace City (紫禁城)", ToolTip = "The Forbidden City's wall: 7.9 m high, 8.62 m at the foot, 6.66 m at the top, red on the side facing the palace."),
};

UENUM(BlueprintType)
enum class EHutongWallCornerShape : uint8
{
	Platform UMETA(DisplayName = "Square Platform (城角)", ToolTip = "A square platform under a corner tower, battered round the outside."),
	Rounded  UMETA(DisplayName = "Rounded (甕城)", ToolTip = "The wall carried round a curve, as at a barbican's outer corners."),
};

UENUM(BlueprintType)
enum class EHutongCornerTower : uint8
{
	None        UMETA(DisplayName = "None", ToolTip = "The platform alone."),
	ArrowTower  UMETA(DisplayName = "Corner Arrow Tower (角箭樓)", ToolTip = "An L-shaped arrow tower wrapping the corner, as on the inner city's corners."),
	PalaceTower UMETA(DisplayName = "Palace Corner Tower (紫禁城角樓)", ToolTip = "The Forbidden City's corner tower: a cross of halls under three eaves and a cross-ridged roof, in yellow glazed tile."),
};

// 城牆: battered brick wall over a rammed-earth core, crenellated 垛口牆 on the outer edge, plain 宇牆 on
// the inner, the 海墁 walk between. Built along +X with the outer face on -Y; the component turns it.
USTRUCT(BlueprintType)
struct FHutongCityWallParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall", meta=(HutongBasic, DisplayName="City", ToolTip="Which city's wall: the inner city's or the lower outer city's. Choosing one takes its height, widths and parapet."))
	EHutongCityWallRank Rank = EHutongCityWallRank::Inner;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall", meta=(DisplayName="Derive From City", ToolTip="Uses the chosen city's height, widths and parapet."))
	bool bDeriveFromRank = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall", meta=(EditCondition="!bDeriveFromRank", UIMin="400", UIMax="2000", ClampMin="200", Units="cm", ToolTip="Height of the walk on top of the wall above the ground, in cm."))
	double Height = HutongCanon::CityWall::HeightCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall", meta=(EditCondition="!bDeriveFromRank", UIMin="400", UIMax="3000", ClampMin="200", Units="cm", ToolTip="Width of the wall at the ground, face to face, in cm."))
	double BaseWidth = HutongCanon::CityWall::BaseWidthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall", meta=(EditCondition="!bDeriveFromRank", UIMin="300", UIMax="2500", ClampMin="150", Units="cm", ToolTip="Width of the wall's top, face to face, in cm; less than the base gives the battered faces."))
	double TopWidth = HutongCanon::CityWall::TopWidthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Wall", meta=(DisplayName="Stone Footing Height (城基)", UIMin="0", UIMax="300", ClampMin="0", Units="cm", ToolTip="Height of the dressed stone courses at the foot of the wall, in cm; zero omits them."))
	double FootingHeight = HutongCanon::CityWall::FootingHeightCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Parapet Height (垛口牆)", EditCondition="!bDeriveFromRank", UIMin="100", UIMax="300", ClampMin="40", Units="cm", ToolTip="Height of the crenellated outer parapet above the walk, in cm."))
	double ParapetHeight = HutongCanon::CityWall::ParapetHeightCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Parapet Thickness", EditCondition="!bDeriveFromRank", UIMin="30", UIMax="150", ClampMin="10", Units="cm", ToolTip="Thickness of the outer parapet, in cm."))
	double ParapetThickness = HutongCanon::CityWall::ParapetThicknessCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Merlon Width (垛)", EditCondition="!bDeriveFromRank", UIMin="60", UIMax="300", ClampMin="30", Units="cm", ToolTip="Width of each merlon between two crenels, in cm; fitted to the run."))
	double MerlonWidth = HutongCanon::CityWall::MerlonWidthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Crenel Width (垛口)", EditCondition="!bDeriveFromRank", UIMin="20", UIMax="120", ClampMin="10", Units="cm", ToolTip="Width of each crenel, in cm."))
	double CrenelWidth = HutongCanon::CityWall::CrenelWidthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Crenel Depth", EditCondition="!bDeriveFromRank", UIMin="20", UIMax="150", ClampMin="0", Units="cm", ToolTip="How far each crenel is cut down from the parapet top, in cm."))
	double CrenelDepth = HutongCanon::CityWall::CrenelDepthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Merlon Top Width", EditCondition="!bDeriveFromRank", UIMin="0", UIMax="200", ClampMin="0", Units="cm", ToolTip="Width of a narrower step on top of each merlon, in cm; zero for a plain merlon."))
	double MerlonTopWidth = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Merlon Top Height", EditCondition="!bDeriveFromRank && MerlonTopWidth > 0", UIMin="5", UIMax="80", ClampMin="1", Units="cm", ToolTip="Height of the narrower step on top of each merlon, in cm."))
	double MerlonTopHeight = HutongCanon::CityWall::PalaceMerlonStepCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Loopholes (射眼)", ToolTip="Cuts a square loophole through each merlon."))
	bool bLoopholes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Loophole Width", EditCondition="bLoopholes && !bDeriveFromRank", UIMin="10", UIMax="50", ClampMin="5", Units="cm", ToolTip="Width of each loophole, in cm."))
	double LoopholeSize = HutongCanon::CityWall::LoopholeSizeCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Loophole Height", EditCondition="bLoopholes && !bDeriveFromRank", UIMin="0", UIMax="80", ClampMin="0", Units="cm", ToolTip="Height of each loophole, in cm; zero makes it square."))
	double LoopholeHeight = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Inner Parapet (宇牆)", ToolTip="Builds the low plain parapet along the inner edge of the walk."))
	bool bInnerParapet = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Inner Parapet Height", EditCondition="bInnerParapet", UIMin="40", UIMax="200", ClampMin="20", Units="cm", ToolTip="Height of the inner parapet above the walk, in cm."))
	double InnerParapetHeight = HutongCanon::CityWall::InnerParapetHeightCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Inner Parapet Thickness", EditCondition="bInnerParapet", UIMin="20", UIMax="120", ClampMin="10", Units="cm", ToolTip="Thickness of the inner parapet, in cm."))
	double InnerParapetThickness = HutongCanon::CityWall::InnerParapetThicknessCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bastions (馬面)", meta=(HutongBasic, DisplayName="Bastions (馬面)", ToolTip="Projects bastions from the outer face at regular intervals, the battlements running round them."))
	bool bBastions = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bastions (馬面)", meta=(DisplayName="Bastion Spacing", EditCondition="bBastions", UIMin="3000", UIMax="20000", ClampMin="1000", Units="cm", ToolTip="Distance from one bastion's centre to the next, in cm."))
	double BastionSpacing = HutongCanon::CityWall::BastionSpacingCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bastions (馬面)", meta=(DisplayName="Bastion Width", EditCondition="bBastions && !bDeriveFromRank", UIMin="500", UIMax="3000", ClampMin="200", Units="cm", ToolTip="Width of each bastion at its foot along the wall, in cm."))
	double BastionWidth = HutongCanon::CityWall::BastionWidthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Bastions (馬面)", meta=(DisplayName="Bastion Projection", EditCondition="bBastions && !bDeriveFromRank", UIMin="300", UIMax="2000", ClampMin="100", Units="cm", ToolTip="How far each bastion stands out from the wall's foot, in cm."))
	double BastionProjection = HutongCanon::CityWall::BastionProjectionCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ramp (馬道)", meta=(HutongBasic, DisplayName="Ramp (馬道)", ToolTip="A ramp up the inner face to the walk, a gateway at its foot."))
	bool bRamp = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ramp (馬道)", meta=(DisplayName="Ramp Top Position", EditCondition="bRamp", UIMin="0", UIMax="1", ClampMin="0", ClampMax="1", ToolTip="Where the top of the ramp reaches the walk along this length of wall, 0 to 1."))
	double RampPosition = 0.8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ramp (馬道)", meta=(DisplayName="Rises Toward the Start", EditCondition="bRamp", ToolTip="Runs the ramp up toward the start of this length of wall instead of its end."))
	bool bRampRisesTowardStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ramp (馬道)", meta=(DisplayName="Ramp Width", EditCondition="bRamp", UIMin="250", UIMax="1000", ClampMin="150", Units="cm", ToolTip="Width of the ramp, in cm."))
	double RampWidth = HutongCanon::CityWall::RampWidthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ramp (馬道)", meta=(DisplayName="Ramp Slope", EditCondition="bRamp", UIMin="8", UIMax="25", ClampMin="4", ClampMax="35", Units="deg", ToolTip="Slope of the ramp, in degrees."))
	double RampSlopeDeg = HutongCanon::CityWall::RampSlopeDeg;

	struct FRankDefaults
	{
		double Height, BaseWidth, TopWidth, ParapetHeight, ParapetThickness, BastionWidth, BastionProjection;
		double MerlonWidth = HutongCanon::CityWall::MerlonWidthCm;
		double CrenelWidth = HutongCanon::CityWall::CrenelWidthCm;
		double CrenelDepth = HutongCanon::CityWall::CrenelDepthCm;
		double MerlonTopWidth = 0.0;
		double MerlonTopHeight = HutongCanon::CityWall::PalaceMerlonStepCm;
		double LoopholeSize = HutongCanon::CityWall::LoopholeSizeCm;
		double LoopholeHeight = 0.0;
	};
	FRankDefaults GetRankDefaults() const
	{
		using namespace HutongCanon::CityWall;
		switch (Rank)
		{
		case EHutongCityWallRank::Outer:
			return { OuterHeightCm, OuterBaseWidthCm, OuterTopWidthCm, OuterParapetHeightCm, OuterParapetThicknessCm, OuterBastionWidthCm, OuterBastionProjectionCm };
		case EHutongCityWallRank::Palace:
			return { PalaceHeightCm, PalaceBaseWidthCm, PalaceTopWidthCm, PalaceParapetHeightCm, PalaceParapetThicknessCm, PalaceBastionWidthCm, PalaceBastionProjectionCm,
				PalaceMerlonWidthCm, PalaceCrenelWidthCm, PalaceCrenelDepthCm, PalaceMerlonTopWidthCm, PalaceMerlonStepCm };
		default:
			return { HeightCm, BaseWidthCm, TopWidthCm, ParapetHeightCm, ParapetThicknessCm, BastionWidthCm, BastionProjectionCm };
		}
	}
	// JUDGEMENT — a barbican's rounded corner, the outer face's radius in base widths (德勝門's, from the user's photographs).
	double GetRoundedCorner() const { return 1.6 * GetBaseWidth(); }
	double GetCornerPlatform() const
	{
		using namespace HutongCanon::CityWall;
		switch (Rank)
		{
		case EHutongCityWallRank::Outer:  return OuterCornerPlatformCm;
		case EHutongCityWallRank::Palace: return PalaceCornerPlatformCm;
		default:                          return CornerPlatformCm;
		}
	}
	// The face toward the palace: the inner face and everything on it.
	bool HasRedInnerFace() const { return Rank == EHutongCityWallRank::Palace; }
	void ChooseRank(EHutongCityWallRank InRank)
	{
		Rank = InRank;
		bDeriveFromRank = true;
	}

	void PinFromRank()
	{
		if (!bDeriveFromRank) return;
		const FRankDefaults R = GetRankDefaults();
		Height = R.Height; BaseWidth = R.BaseWidth; TopWidth = R.TopWidth;
		ParapetHeight = R.ParapetHeight; ParapetThickness = R.ParapetThickness;
		BastionWidth = R.BastionWidth; BastionProjection = R.BastionProjection;
		MerlonWidth = R.MerlonWidth; CrenelWidth = R.CrenelWidth; CrenelDepth = R.CrenelDepth;
		MerlonTopWidth = R.MerlonTopWidth; MerlonTopHeight = R.MerlonTopHeight;
		LoopholeSize = R.LoopholeSize; LoopholeHeight = R.LoopholeHeight;
		bDeriveFromRank = false;
	}

	double GetHeight() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().Height : Height, 200.0); }
	double GetBaseWidth() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().BaseWidth : BaseWidth, 200.0); }
	double GetTopWidth() const
	{
		const double MinTop = GetParapetThickness() + (bInnerParapet ? GetInnerParapetThickness() : 0.0) + 100.0;
		return FMath::Clamp(bDeriveFromRank ? GetRankDefaults().TopWidth : TopWidth, FMath::Min(MinTop, GetBaseWidth()), GetBaseWidth());
	}
	double GetBatter() const { return 0.5 * (GetBaseWidth() - GetTopWidth()); }
	double GetFootingHeight() const { return FMath::Clamp(FootingHeight, 0.0, 0.5 * GetHeight()); }
	double GetParapetHeight() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().ParapetHeight : ParapetHeight, 40.0); }
	double GetParapetThickness() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().ParapetThickness : ParapetThickness, 10.0); }
	double GetBastionWidth() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().BastionWidth : BastionWidth, 200.0); }
	double GetBastionProjection() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().BastionProjection : BastionProjection, 100.0); }
	// A centimetre under the walk, so its top stays off the walk's plane.
	double GetRampRise() const { return GetHeight() - 1.0; }
	double GetRampRun() const { return GetRampRise() / FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(RampSlopeDeg, 4.0, 35.0))); }
	double GetMerlonWidth() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().MerlonWidth : MerlonWidth, 30.0); }
	double GetCrenelWidth() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().CrenelWidth : CrenelWidth, 10.0); }
	double GetCrenelDepth() const { return FMath::Clamp(bDeriveFromRank ? GetRankDefaults().CrenelDepth : CrenelDepth, 0.0, GetParapetHeight() - 20.0); }
	// Zero: a plain merlon. Clamped to the merlon as fitted to the run.
	double GetMerlonTopWidth() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().MerlonTopWidth : MerlonTopWidth, 0.0); }
	// Within the merlon above the crenel sill.
	double GetMerlonTopHeight() const
	{
		if (GetMerlonTopWidth() <= 0.0) return 0.0;
		return FMath::Clamp(bDeriveFromRank ? GetRankDefaults().MerlonTopHeight : MerlonTopHeight, 1.0, 0.5 * GetCrenelDepth());
	}
	double GetLoopholeWidth() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().LoopholeSize : LoopholeSize, 5.0); }
	double GetLoopholeHeight() const
	{
		const double Asked = bDeriveFromRank ? GetRankDefaults().LoopholeHeight : LoopholeHeight;
		return Asked > 0.0 ? Asked : GetLoopholeWidth();
	}
	double GetInnerParapetHeight() const { return FMath::Max(InnerParapetHeight, 20.0); }
	double GetInnerParapetThickness() const { return FMath::Max(InnerParapetThickness, 10.0); }
	double GetOverallHeight() const { return GetHeight() + GetParapetHeight(); }

	int32 GetMerlonCount(double RunLength) const
	{
		const double Mw = GetMerlonWidth(), Cw = GetCrenelWidth();
		return FMath::Max(1, FMath::RoundToInt32((RunLength + Cw) / (Mw + Cw)));
	}

	double Length = 2000.0;
};

namespace HutongGen
{
	struct FCityWallRamp
	{
		double XGate = 0.0, XFoot = 0.0, XLand = 0.0, XTop = 0.0;
		// +1 rising toward +X, -1 toward 0.
		double Dir = 1.0;
	};
	bool CityWallRampLayout(const FHutongCityWallParams& P, FCityWallRamp& Out);

	void BuildCityWall(UE::Geometry::FDynamicMesh3& Mesh, const FHutongCityWallParams& P, bool bBattlements, bool bLoopholes);

	// 城角: a square platform PlatformSize across with its outer corner at the origin, outer faces on X = 0 and Y = 0;
	// a leg of P's section abuts each port, the face at X = PlatformSize (Y 0..base) and Y = PlatformSize (X 0..base).
	// bOuterParapet false: the outside edges carry no 垛口牆 (a tower's walls rise from them).
	void BuildWallCorner(UE::Geometry::FDynamicMesh3& Mesh, const FHutongCityWallParams& P, double PlatformSize,
		bool bBattlements, bool bLoopholes, bool bOuterParapet = true);

	// The same ports, the wall between them carried round a quarter circle of short mitred legs, the outer face
	// reaching out to the arc of radius Size about (Size, Size).
	void BuildRoundedCorner(UE::Geometry::FDynamicMesh3& Mesh, const FHutongCityWallParams& P, double Size,
		bool bBattlements, bool bLoopholes);
}
