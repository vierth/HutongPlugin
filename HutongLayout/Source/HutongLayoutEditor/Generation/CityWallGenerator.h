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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Merlon Width (垛)", UIMin="80", UIMax="300", ClampMin="30", Units="cm", ToolTip="Width of each merlon between two crenels, in cm; fitted to the run."))
	double MerlonWidth = HutongCanon::CityWall::MerlonWidthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Crenel Width (垛口)", UIMin="20", UIMax="120", ClampMin="10", Units="cm", ToolTip="Width of each crenel, in cm."))
	double CrenelWidth = HutongCanon::CityWall::CrenelWidthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Crenel Depth", UIMin="20", UIMax="150", ClampMin="0", Units="cm", ToolTip="How far each crenel is cut down from the parapet top, in cm."))
	double CrenelDepth = HutongCanon::CityWall::CrenelDepthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Loopholes (射眼)", ToolTip="Cuts a square loophole through each merlon."))
	bool bLoopholes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battlements", meta=(DisplayName="Loophole Size", EditCondition="bLoopholes", UIMin="10", UIMax="50", ClampMin="5", Units="cm", ToolTip="Width and height of each loophole, in cm."))
	double LoopholeSize = HutongCanon::CityWall::LoopholeSizeCm;

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

	// Rank figures, or the fields once pinned.
	struct FRankDefaults
	{
		double Height, BaseWidth, TopWidth, ParapetHeight, ParapetThickness, BastionWidth, BastionProjection;
	};
	FRankDefaults GetRankDefaults() const
	{
		using namespace HutongCanon::CityWall;
		return Rank == EHutongCityWallRank::Outer
			? FRankDefaults{ OuterHeightCm, OuterBaseWidthCm, OuterTopWidthCm, OuterParapetHeightCm, OuterParapetThicknessCm, OuterBastionWidthCm, OuterBastionProjectionCm }
			: FRankDefaults{ HeightCm, BaseWidthCm, TopWidthCm, ParapetHeightCm, ParapetThicknessCm, BastionWidthCm, BastionProjectionCm };
	}
	// Choosing a city takes its figures, over any pinned by hand (a pinned wall once ignored the choice).
	void ChooseRank(EHutongCityWallRank InRank)
	{
		Rank = InRank;
		bDeriveFromRank = true;
	}

	// Writes the rank's figures into the fields and stops deriving, so one can be changed by hand.
	void PinFromRank()
	{
		if (!bDeriveFromRank) return;
		const FRankDefaults R = GetRankDefaults();
		Height = R.Height; BaseWidth = R.BaseWidth; TopWidth = R.TopWidth;
		ParapetHeight = R.ParapetHeight; ParapetThickness = R.ParapetThickness;
		BastionWidth = R.BastionWidth; BastionProjection = R.BastionProjection;
		bDeriveFromRank = false;
	}

	double GetHeight() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().Height : Height, 200.0); }
	double GetBaseWidth() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().BaseWidth : BaseWidth, 200.0); }
	// Never wider than the base (no overhang) and never so narrow the parapets meet.
	double GetTopWidth() const
	{
		const double MinTop = GetParapetThickness() + (bInnerParapet ? GetInnerParapetThickness() : 0.0) + 100.0;
		return FMath::Clamp(bDeriveFromRank ? GetRankDefaults().TopWidth : TopWidth, FMath::Min(MinTop, GetBaseWidth()), GetBaseWidth());
	}
	// Horizontal set-in of each face from foot to top.
	double GetBatter() const { return 0.5 * (GetBaseWidth() - GetTopWidth()); }
	double GetFootingHeight() const { return FMath::Clamp(FootingHeight, 0.0, 0.5 * GetHeight()); }
	double GetParapetHeight() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().ParapetHeight : ParapetHeight, 40.0); }
	double GetParapetThickness() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().ParapetThickness : ParapetThickness, 10.0); }
	double GetBastionWidth() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().BastionWidth : BastionWidth, 200.0); }
	double GetBastionProjection() const { return FMath::Max(bDeriveFromRank ? GetRankDefaults().BastionProjection : BastionProjection, 100.0); }
	// Height the ramp climbs: a centimetre under the walk, so its top never shares the walk's plane.
	double GetRampRise() const { return GetHeight() - 1.0; }
	double GetRampRun() const { return GetRampRise() / FMath::Tan(FMath::DegreesToRadians(FMath::Clamp(RampSlopeDeg, 4.0, 35.0))); }
	double GetCrenelDepth() const { return FMath::Clamp(CrenelDepth, 0.0, GetParapetHeight() - 20.0); }
	double GetInnerParapetHeight() const { return FMath::Max(InnerParapetHeight, 20.0); }
	double GetInnerParapetThickness() const { return FMath::Max(InnerParapetThickness, 10.0); }
	// Top of everything: the merlons.
	double GetOverallHeight() const { return GetHeight() + GetParapetHeight(); }

	// Merlons on a run: a merlon at each end, crenels between, widths fitted to the length.
	int32 GetMerlonCount(double RunLength) const
	{
		const double Mw = FMath::Max(MerlonWidth, 30.0), Cw = FMath::Max(CrenelWidth, 10.0);
		return FMath::Max(1, FMath::RoundToInt32((RunLength + Cw) / (Mw + Cw)));
	}

	// Set by the component from its footprint.
	double Length = 2000.0;
};

namespace HutongGen
{
	// Where the 馬道 lies along its leg, built frame (outer face on -Y): the 馬道門's outer face, the
	// incline's foot, the landing's start and the top end. False when the leg is too short for it.
	struct FCityWallRamp
	{
		double XGate = 0.0, XFoot = 0.0, XLand = 0.0, XTop = 0.0;
		// +1 rising toward +X, -1 toward 0.
		double Dir = 1.0;
	};
	bool CityWallRampLayout(const FHutongCityWallParams& P, FCityWallRamp& Out);

	// Detail: Massing builds the body alone; Far adds footing and battlements; Near cuts the loopholes.
	void BuildCityWall(UE::Geometry::FDynamicMesh3& Mesh, const FHutongCityWallParams& P, bool bBattlements, bool bLoopholes);
}
