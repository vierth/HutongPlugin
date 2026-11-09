#pragma once

#include "CoreMinimal.h"
#include "Generation/CityGateGenerator.h"
#include "Generation/ArrowTowerGenerator.h"
#include "Generation/CityWallGenerator.h"
#include "Generation/BaySide.h"
#include "Generation/HutongFootprint.h"
#include "GateComplexLayout.generated.h"

UENUM(BlueprintType)
enum class EHutongCompassSide : uint8
{
	North UMETA(DisplayName = "North (北)", ToolTip = "Facing north."),
	East  UMETA(DisplayName = "East (東)", ToolTip = "Facing east."),
	South UMETA(DisplayName = "South (南)", ToolTip = "Facing south."),
	West  UMETA(DisplayName = "West (西)", ToolTip = "Facing west."),
};

UENUM(BlueprintType)
enum class EHutongBarbicanGateSide : uint8
{
	East UMETA(DisplayName = "East (德勝門)", ToolTip = "The barbican's gate in its east side wall."),
	West UMETA(DisplayName = "West", ToolTip = "The barbican's gate in its west side wall."),
};

// 德勝門 laid out at once: the gate and 城樓 in the city wall, the 甕城 in front with rounded outer corners, the
// 箭樓 at its front, the 閘樓 in one side wall, and a stretch of city wall either side.
USTRUCT(BlueprintType)
struct FHutongGateComplexParams
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Complex", meta=(HutongBasic, DisplayName="City", ToolTip="Whose wall every piece is built to."))
	EHutongCityWallRank Rank = EHutongCityWallRank::Inner;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Complex", meta=(DisplayName="Barbican Width", UIMin="6000", UIMax="20000", ClampMin="3000", Units="cm", ToolTip="Width of the barbican (甕城) along the city wall, outside face to outside face, in cm."))
	double BarbicanWidth = HutongCanon::GateComplex::BarbicanWidthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Complex", meta=(DisplayName="Barbican Depth", UIMin="4000", UIMax="15000", ClampMin="2000", Units="cm", ToolTip="How far the barbican reaches out from the city wall to its front wall's outer face, in cm."))
	double BarbicanDepth = HutongCanon::GateComplex::BarbicanDepthCm;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Complex", meta=(HutongBasic, DisplayName="Barbican Gate Side", ToolTip="Which side wall of the barbican carries its gate and gate tower (閘樓)."))
	EHutongBarbicanGateSide SideGate = EHutongBarbicanGateSide::East;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Gate Complex", meta=(DisplayName="City Wall Either Side", UIMin="0", UIMax="10000", ClampMin="0", Units="cm", ToolTip="Length of city wall laid either side of the gate, to carry on from with the City Wall tool, in cm; zero lays none."))
	double WallStub = HutongCanon::GateComplex::WallStubCm;
};

namespace HutongGen
{
	// One placed piece of the complex. Origin and yaw are its actor's in the complex's frame: the city wall's outer face
	// on Y = 0, outside the city toward -Y, the gate's middle at X = 0.
	struct FGateComplexPiece
	{
		enum class EKind : uint8 { Gate, ArrowTower, Corner, Wall };
		EKind Kind = EKind::Wall;
		FVector2D Origin = FVector2D::ZeroVector;
		double YawDeg = 0.0;
		FVector2D Size = FVector2D::ZeroVector;
		FHutongCityGateParams Gate;
		FHutongArrowTowerParams Tower;
		FHutongCityWallParams Wall;
		EHutongBaySide Facade = EHutongBaySide::MinusY;
		int32 OuterCorner = 0;
		double CornerSize = 0.0;
		double Length = 0.0;
		EHutongBaySide OuterSide = EHutongBaySide::MinusY;
		FHutongFootprintSkew Skew;
	};

	void LayOutGateComplex(const FHutongGateComplexParams& P, TArray<FGateComplexPiece>& Out);

	// The piece's footprint corners in the complex's frame.
	void GateComplexPieceCorners(const FGateComplexPiece& Piece, FVector2D OutCorners[4]);
}
