#include "Generation/HutongDetail.h"

#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongShell.h"
#include "Generation/HutongRoofTile.h"

#include "Generation/WallGenerator.h"
#include "Generation/SiheyuanGenerator.h"
#include "Generation/GateHouseGenerator.h"
#include "Generation/HallGenerator.h"
#include "Generation/CorridorGenerator.h"
#include "Generation/InnerGateGenerator.h"
#include "Generation/PaifangGenerator.h"
#include "Generation/PathGenerator.h"
#include "Generation/PassageGenerator.h"
#include "Generation/FlowerBedGenerator.h"
#include "Generation/WaterJarGenerator.h"
#include "Generation/FrameGenerator.h"
#include "Generation/PavilionGenerator.h"
#include "Generation/ScreenWallGenerator.h"
#include "Generation/ShopfrontGenerator.h"
#include "Generation/StoreyGenerator.h"

using UE::Geometry::FDynamicMesh3;
using namespace HutongMeshUtils;
using namespace HutongGen;

namespace
{
	int32 Coarser(int32 N) { return FMath::Max(N / 2, 1); }
	int32 Finer(int32 N)   { return FMath::CeilToInt32(1.5 * (double)FMath::Max(N, 1)); }

	double CoarserTileSpacing(double Spacing)
	{
		return 2.0 * FMath::Max(Spacing, HutongGen::RoofTile::DefaultRowSpacing);
	}
}

namespace HutongGen
{
namespace Detail
{

void Apply(EHutongDetail D, FHutongWallParams& P)
{
	if (D == EHutongDetail::Far || D == EHutongDetail::Massing)
	{
		P.WindowOutlineSteps = 4;
		P.DoorwayOutlineSteps = 6;
		P.WindowLatticeBars = 0;
		P.bDoorwayChuihua = false;
		P.GatePegCount = 0;
		P.bHasTileRuns = false;
	}
	else if (D == EHutongDetail::Hero)
	{
		P.WindowOutlineSteps = Finer(P.WindowOutlineSteps);
		P.DoorwayOutlineSteps = Finer(P.DoorwayOutlineSteps);
	}
}

void Apply(EHutongDetail D, FHutongSiheyuanParams& P)
{
	if (D == EHutongDetail::Far)
	{
		P.bExposedFrame = false;
		P.bHasTileRuns = false;
		// 椽頭 (檐椽 + 飛椽 ends): over half a 廂房's cost per survey.
		P.RafterEndSection = 0.0;
		P.bHasFlyingRafters = false;

		// 合瓦 has no 勾頭, so this is the eave-cap row as well as the tile.
		P.RoofTile = EHutongRoofTile::He;
		P.TileRowSpacing = CoarserTileSpacing(P.TileRowSpacing);

		P.bHasWindowLattice = false;
		P.WindowMullions = 0;
		P.WindowRails = 0;
		P.bHasCurtainFrame = false;
		P.TransomPanes = 1;

		P.bHasInteriorPlaster = false;
		P.bHasInteriorPartitions = false;
		P.bHasChitou = false;

		P.RidgeEndKick = 0.0;
		P.RoofSlopeSegments = Coarser(P.RoofSlopeSegments);
	}
	else if (D == EHutongDetail::Hero)
	{
		P.RoofSlopeSegments = Finer(P.RoofSlopeSegments);
	}
}

void Apply(EHutongDetail D, FHutongGateHouseParams& P)
{
	if (D == EHutongDetail::Far)
	{
		P.bExposedFrame = false;
		P.bHasTileRuns = false;
		P.RafterEndSection = 0.0;
		// The tile is *derived* from the style here.
		P.bPlainTileForDetail = true;
		P.TileRowSpacing = CoarserTileSpacing(P.TileRowSpacing);
		P.RidgeEndKick = 0.0;
		P.bHasChitou = false;
		P.DoorPegCount = 0;
	}
}

void Apply(EHutongDetail D, FHutongPaifangParams& P)
{
	if (D == EHutongDetail::Far)
	{
		P.bPlainTileForDetail = true;
		P.bHasTileRuns = false;
		P.RafterEndSection = 0.0;
		P.TileRowSpacing = CoarserTileSpacing(P.TileRowSpacing);
		P.RoofEaveSegments = Coarser(P.RoofEaveSegments);
		P.RoofSlopeSegments = Coarser(P.RoofSlopeSegments);
	}
	else if (D == EHutongDetail::Hero)
	{
		P.RoofEaveSegments = Finer(P.RoofEaveSegments);
		P.RoofSlopeSegments = Finer(P.RoofSlopeSegments);
	}
}

void Apply(EHutongDetail D, FHutongScreenWallParams& P)
{
	if (D == EHutongDetail::Far)
	{
		P.bHasTileRuns = false;
		P.RafterEndSection = 0.0;
		P.RidgeEndKick = 0.0;
		P.bHasPanel = false;
	}
}

void Apply(EHutongDetail D, FHutongCorridorParams& P)
{
	if (D == EHutongDetail::Far)
	{
		P.bExposedFrame = false;
		P.bHasTileRuns = false;
		P.RafterEndSection = 0.0;
		// 倒掛楣子: six bars a bay, the heaviest courtyard piece.
		P.bHasFrieze = false;
		P.FriezeBars = 0;
		P.bHasBenchLattice = false;
	}
}

void Apply(EHutongDetail D, FHutongInnerGateParams& P)
{
	if (D == EHutongDetail::Far)
	{
		P.bExposedFrame = false;
		P.bHasTileRuns = false;
		P.RafterEndSection = 0.0;
		P.RidgeEndKick = 0.0;
		P.bHasBrackets = false;
	}
}

void Apply(EHutongDetail D, FHutongShopfrontParams& P)
{
	if (D == EHutongDetail::Far)
	{
		P.bHasTileRuns = false;
		P.RafterEndSection = 0.0;
		P.RoofTile = EHutongRoofTile::He;
		P.TileRowSpacing = CoarserTileSpacing(P.TileRowSpacing);
		P.RidgeEndKick = 0.0;
		P.bHasChitou = false;
		P.bHasSpandrels = false;
	}
}

void Apply(EHutongDetail D, FHutongStoreyParams& P)
{
	if (D == EHutongDetail::Far)
	{
		P.bHasTileRuns = false;
		P.RafterEndSection = 0.0;
		P.bHasSkirtRafters = false;
		P.RoofTile = EHutongRoofTile::He;
		P.TileRowSpacing = CoarserTileSpacing(P.TileRowSpacing);
		P.RidgeEndKick = 0.0;
		P.bHasWindowLattice = false;
		P.BalusterSpacing = FMath::Max(P.BalusterSpacing * 2.0, 12.0);
	}
}

void Apply(EHutongDetail D, FHutongPavilionParams& P)
{
	if (D == EHutongDetail::Far)
	{
		P.FreezeProportions();
		P.bExposedFrame = false;
		P.RoofTile = EHutongRoofTile::He;
		P.TileRowSpacing = CoarserTileSpacing(P.TileRowSpacing);
		P.RoofEaveSegments = Coarser(P.RoofEaveSegments);
		P.RoofSlopeSegments = Coarser(P.RoofSlopeSegments);
		P.bHasFrieze = false;
		P.FriezeBars = 0;
		P.bHasTileRuns = false;
		P.RafterEndSection = 0.0;
	}
	else if (D == EHutongDetail::Hero)
	{
		P.RoofEaveSegments = Finer(P.RoofEaveSegments);
		P.RoofSlopeSegments = Finer(P.RoofSlopeSegments);
	}
}

void Apply(EHutongDetail D, FHutongPathParams& P)
{
	// Course count reads as texture at any distance.
	if (D == EHutongDetail::Far || D == EHutongDetail::Massing)
	{
		P.CourseSpacing = 2.0 * FMath::Max(P.CourseSpacing, 1.0);
	}
}

void Apply(EHutongDetail D, FHutongPassageParams& P)
{
	if (D == EHutongDetail::Far || D == EHutongDetail::Massing) P.bHasTileRuns = false;
}

void Apply(EHutongDetail, FHutongFlowerBedParams&)
{
	// Already massing: four kerbs and earth.
}

void Apply(EHutongDetail D, FHutongWaterJarParams& P)
{
	if (D == EHutongDetail::Far || D == EHutongDetail::Massing)
	{
		P.Sides = FMath::Min(P.Sides, 8);
	}
	else if (D == EHutongDetail::Hero)
	{
		P.Sides = FMath::Min(Finer(P.Sides), 48);
	}
}

void Apply(EHutongDetail D, FHutongFrameParams& P)
{
	// 塊 reads as 遠: posts, beams, purlins.
	if (D == EHutongDetail::Far || D == EHutongDetail::Massing)
	{
		P.bHasRafters = false;
		P.bHasRidgeBraces = false;
	}
}

void Apply(EHutongDetail D, FHutongHallParams& P)
{
	if (D == EHutongDetail::Far)
	{
		// 筒瓦 on 歇山 is the heaviest thing the plugin builds.
		P.RoofTile = EHutongRoofTile::He;
		P.TileRowSpacing = CoarserTileSpacing(P.TileRowSpacing);
		P.RoofEaveSegments = Coarser(P.RoofEaveSegments);
		P.RoofSlopeSegments = Coarser(P.RoofSlopeSegments);
		P.LatticeMullions = 0;
		P.LatticeRails = 0;
		P.bHasBrackets = false;
		P.bExposedFrame = false;
		// At a distance 斗栱 sets are texture.
		P.bHasDougong = false;
		P.bHasTileRuns = false;
		P.RafterEndSection = 0.0;
	}
	else if (D == EHutongDetail::Hero)
	{
		P.RoofEaveSegments = Finer(P.RoofEaveSegments);
		P.RoofSlopeSegments = Finer(P.RoofSlopeSegments);
	}
}

TArray<EHutongDetail> LODChain(EHutongDetail Placed)
{
	TArray<EHutongDetail> Chain;
	for (int32 L = (int32)Placed; L >= (int32)EHutongDetail::Massing; --L)
	{
		Chain.Add((EHutongDetail)L);
	}
	return Chain;
}

float LODScreenSize(int32 LODIndex)
{
	static const float Sizes[] = { 1.0f, 0.25f, 0.0625f, 0.015f };
	return Sizes[FMath::Clamp(LODIndex, 0, (int32)UE_ARRAY_COUNT(Sizes) - 1)];
}

int32 BuildLODChain(
	EHutongDetail Placed,
	bool bChain,
	TFunctionRef<void(FDynamicMesh3&, EHutongDetail)> Build,
	TArray<FDynamicMesh3>& OutLODs,
	FMeshTurn* OutTurn)
{
	OutLODs.Reset();
	if (OutTurn) *OutTurn = FMeshTurn();

	TArray<EHutongDetail> Chain;
	if (bChain) Chain = LODChain(Placed);
	else Chain.Add(Placed);

	int32 CollisionLOD = 0;
	bool bTurnKnown = false;
	for (const EHutongDetail Level : Chain)
	{
		FDynamicMesh3 Mesh;
		{
			FMeshTurnRecorder Turn;
			Build(Mesh, Level);
			if (OutTurn)
			{
				// Every level of a building is turned alike; the component carries one turn.
				ensureMsgf(!bTurnKnown || Turn.Get() == *OutTurn, TEXT("LODs of one building built with different turns"));
				if (!bTurnKnown) *OutTurn = Turn.Get();
				bTurnKnown = true;
				UnturnAndSnap(Mesh, *OutTurn);
			}
		}

		// LOD0 is whatever the placement asked for, empty or not.
		if (OutLODs.Num() > 0
			&& (Mesh.TriangleCount() == 0
				|| Mesh.TriangleCount() >= OutLODs.Last().TriangleCount()))
		{
			continue;
		}
		// Collision from 遠 or above; 塊 collides only as the sole LOD.
		if (Level >= EHutongDetail::Far) CollisionLOD = OutLODs.Num();
		OutLODs.Emplace(MoveTemp(Mesh));
	}
	return CollisionLOD;
}

int32 BuildPlacementLODs(
	bool bPlanOnly,
	double SizeX,
	double SizeY,
	EHutongDetail Placed,
	bool bChain,
	TFunctionRef<void(FDynamicMesh3&, EHutongDetail)> Build,
	TArray<FDynamicMesh3>& OutLODs,
	FMeshTurn* OutTurn)
{
	if (!bPlanOnly)
	{
		return BuildLODChain(Placed, bChain, Build, OutLODs, OutTurn);
	}
	if (OutTurn) *OutTurn = FMeshTurn();
	OutLODs.Reset();
	return 0;
}

} // namespace Detail
} // namespace HutongGen

namespace HutongGen
{
namespace Massing
{

void AppendBlock(FDynamicMesh3& Mesh, const FBlock& B)
{
	const double W = FMath::Max(B.Width, 1.0);
	const double D = FMath::Max(B.Depth, 1.0);
	const double Floor = FMath::Max(B.FloorHeight, 0.0);
	const double Eave = FMath::Max(B.EaveHeight, Floor + 1.0);

	auto SlideY = [&Mesh, &B](int32 FirstVert)
	{
		if (FMath::IsNearlyZero(B.OriginY)) return;
		TransformVerticesFrom(Mesh, FirstVert,
			FTransform(FQuat::Identity, FVector(0.0, B.OriginY, 0.0)));
	};

	if (Floor > 0.0)
	{
		const int32 FirstVert = Mesh.MaxVertexID();
		Shell::AppendPlatform(Mesh, W, D, Floor,
			FMath::Max(B.PlatformOverhang, 0.0), 0.0, 0, 0.0, 0.0, 0.0, /*bPaved*/ false);
		SlideY(FirstVert);
	}

	{
		const int32 FirstVert = Mesh.MaxVertexID();
		const int32 FirstTri = Mesh.MaxTriangleID();

		if (!B.bOpen)
		{
			AppendBox(Mesh, FVector3d(0.0, 0.0, Floor), FVector3d(W, D, Eave));
		}
		else if (B.ColumnSize > 0.0)
		{
			const double C = FMath::Max(B.ColumnSize, 1.0);
			const int32 NX = FMath::Max(B.ColumnsAlongWidth, 2);
			const int32 NR = FMath::Clamp(B.ColumnRows, 1, 2);
			for (int32 ix = 0; ix < NX; ++ix)
			{
				const double CentreX = FMath::Lerp(0.5 * C, W - 0.5 * C, (double)ix / (NX - 1));
				for (int32 iy = 0; iy < NR; ++iy)
				{
					const double CentreY = (NR == 1)
						? 0.5 * D
						: FMath::Lerp(0.5 * C, D - 0.5 * C, (double)iy);
					AppendBox(Mesh,
						FVector3d(CentreX - 0.5 * C, CentreY - 0.5 * C, Floor),
						FVector3d(CentreX + 0.5 * C, CentreY + 0.5 * C, Eave));
				}
			}
		}

		SetMaterialIDForTriangleRange(Mesh, FirstTri, Mesh.MaxTriangleID(), MatSlot_Body);
		SlideY(FirstVert);
	}

	if (B.Roof == ERoof::None) return;

	const int32 RoofFirstVert = Mesh.MaxVertexID();

	if (B.Roof == ERoof::Gable)
	{
		Shell::FRoofParams Roof;
		Roof.FrontOverhang = FMath::Max(B.FrontOverhang, 0.0);
		Roof.RearOverhang = FMath::Max(B.RearOverhang, 0.0);
		Roof.RearSlopeTrim = FMath::Max(B.RearSlopeTrim, 0.0);
		Roof.GableOverhang = FMath::Max(B.GableOverhang, 0.0);
		Roof.Section = B.Section;
		Roof.Rise = B.Rise;
		Roof.SlopeSegments = 2;
		Roof.FasciaDepth = 0.0;
		Roof.FasciaWidth = 0.0;
		Roof.RafterSection = 0.0;
		Roof.bFlyingRafters = false;
		Roof.Tile = EHutongRoofTile::He;
		Roof.bHasRidgeCourse = false;
		Roof.RakeDepth = 0.0;
		Roof.bTileRuns = false;
		Shell::AppendGableRoof(Mesh, W, D, Eave, Roof);
	}
	else
	{
		const double O = FMath::Max(B.FrontOverhang, 0.0);
		const double RoofW = W + 2.0 * O;
		const double RoofD = D + 2.0 * O;
		const double Rise = (B.Rise > 0.0) ? B.Rise : FMath::Max(B.Section.Rise(), 20.0);
		const FVector3d RoofMin(-O, -O, Eave);
		FSlotScope RoofTag(Mesh, MatSlot_Roof);

		if (B.Roof == ERoof::Xieshan)
		{
			FXieshanRoofSpec Xie;
			Xie.Width = RoofW;
			Xie.Depth = RoofD;
			Xie.Rise = Rise;
			Xie.Section = B.Section;
			Xie.ShouInset = FMath::Max(B.ShouInset, 1.0);
			Xie.EaveSegments = 4;
			Xie.SlopeSegments = 3;
			AppendXieshanRoof(Mesh, RoofMin, Xie);
		}
		else
		{
			FHipRoofSpec Hip;
			Hip.Width = RoofW;
			Hip.Depth = RoofD;
			Hip.RidgeLength = FMath::Max(B.RidgeLength, 0.0);
			Hip.Rise = Rise;
			Hip.Section = B.Section;
			Hip.EaveSegments = 4;
			Hip.SlopeSegments = 2;
			AppendHippedRoof(Mesh, RoofMin, Hip);
		}
	}

	SlideY(RoofFirstVert);
}

FBlock From(const FHutongSiheyuanParams& P)
{
	FBlock B;
	B.Width = P.Width;
	B.Depth = P.Depth;
	B.FloorHeight = P.GetFloorHeight();
	B.PlatformOverhang = P.PlatformOverhang;
	B.EaveHeight = P.GetRoofBaseHeight();
	B.FrontOverhang = P.GetRoofOverhang();
	B.RearOverhang = P.GetRearRoofOverhang();
	// The same arithmetic MakeRoofParams does.
	B.RearSlopeTrim = (P.RearEave == EHutongRearEave::Lane)
		? FMath::Max(B.FrontOverhang - B.RearOverhang, 0.0) : 0.0;
	B.Section = P.GetRoofSection();
	B.Rise = P.bDeriveProportions ? 0.0 : P.GetRoofRise();
	return B;
}

FBlock From(const FHutongGateHouseParams& P)
{
	FBlock B;
	B.Width = P.Width;
	B.Depth = P.Depth;
	B.FloorHeight = P.FloorHeight;
	B.PlatformOverhang = P.PlatformOverhang;
	B.EaveHeight = P.GetRoofBaseHeight();
	B.FrontOverhang = P.GetRoofOverhang();
	B.RearOverhang = B.FrontOverhang;
	B.Section = Jiajia::MakeSection(
		P.GetPurlins(), 0.5 * FMath::Max(P.Depth, 1.0), B.FrontOverhang, P.RoofApexRoll);
	B.Rise = P.GetRoofRise(P.Depth);
	return B;
}

FBlock From(const FHutongShopfrontParams& P)
{
	FBlock B;
	B.Width = P.Width;
	B.Depth = P.Depth;
	B.FloorHeight = P.FloorHeight;
	B.PlatformOverhang = P.PlatformOverhang;
	B.EaveHeight = P.GetRoofBaseHeight();
	B.FrontOverhang = FMath::Max(P.RoofOverhang, 0.0);
	B.RearOverhang = P.GetRearRoofOverhang();
	B.RearSlopeTrim = (P.RearEave == EHutongRearEave::Lane)
		? FMath::Max(B.FrontOverhang - B.RearOverhang, 0.0) : 0.0;
	B.Section = Jiajia::MakeSection(
		EHutongPurlins::Five, 0.5 * FMath::Max(P.Depth, 1.0), B.FrontOverhang, P.RoofApexRoll);
	B.Rise = P.GetRoofRise();
	return B;
}

FBlock From(const FHutongStoreyParams& P)
{
	FBlock B;
	B.Width = P.Width;
	B.Depth = P.Depth;
	B.FloorHeight = FMath::Clamp(P.FloorHeight, 0.0, 120.0);
	B.PlatformOverhang = P.PlatformOverhang;
	// Both storeys: a 樓's height is the point of its block.
	B.EaveHeight = P.GetRoofBaseHeight();
	B.FrontOverhang = FMath::Max(P.RoofOverhang, 0.0);
	B.RearOverhang = P.GetRearRoofOverhang();
	B.RearSlopeTrim = (P.RearEave == EHutongRearEave::Lane)
		? FMath::Max(B.FrontOverhang - B.RearOverhang, 0.0) : 0.0;
	B.Section = Jiajia::MakeSection(
		EHutongPurlins::Five, 0.5 * FMath::Max(P.Depth, 1.0), B.FrontOverhang, P.RoofApexRoll);
	B.Rise = P.GetRoofRise();
	return B;
}

FBlock From(const FHutongScreenWallParams& P)
{
	FBlock B;
	B.Width = P.Length;
	B.Depth = FMath::Max(P.Thickness, 1.0);
	// The wall stands in the middle of its footprint: the 須彌座 projects either side of it.
	B.OriginY = FMath::Max(P.PlinthProjection, 0.0);
	B.EaveHeight = P.GetEaveHeight();
	B.FrontOverhang = FMath::Max(P.RoofOverhang, 0.0);
	B.RearOverhang = B.FrontOverhang;
	B.GableOverhang = FMath::Max(P.GableOverhang, 0.0);
	B.Section = Jiajia::MakeSection(
		EHutongPurlins::Three, 0.5 * B.Depth, B.FrontOverhang, P.RoofApexRoll);
	B.Rise = P.GetRoofRise();
	return B;
}

FBlock From(const FHutongPassageParams& P)
{
	FBlock B;
	B.Width = FMath::Max(P.Length, 1.0);
	B.Depth = P.GetRoofSpan();
	B.EaveHeight = P.GetEaveHeight();
	// Walls belong to the 耳房 and 院牆, so the block is just the roof.
	B.bOpen = true;
	B.ColumnSize = 0.0;
	B.Section = Jiajia::MakeSection(EHutongPurlins::Three, 0.5 * B.Depth, 0.0, 0.0);
	B.Rise = P.GetRoofRise(B.Depth);
	return B;
}

FBlock From(const FHutongInnerGateParams& P)
{
	FBlock B;
	B.Width = P.Width;
	B.Depth = P.Depth;
	B.FloorHeight = P.FloorHeight;
	B.PlatformOverhang = P.PlatformOverhang;
	B.EaveHeight = P.GetRoofBaseHeight();
	// 獨立柱擔梁式: one column pair in the middle of the depth, carrying everything.
	B.bOpen = true;
	B.ColumnSize = FMath::Max(P.ColumnDiameter, 4.0);
	B.ColumnsAlongWidth = 2;
	B.ColumnRows = 1;
	B.FrontOverhang = FMath::Max(P.RoofOverhang, 0.0);
	B.RearOverhang = B.FrontOverhang;
	B.GableOverhang = FMath::Max(P.GableOverhang, 0.0);
	B.Section = Jiajia::MakeSection(
		EHutongPurlins::Three, 0.5 * FMath::Max(P.Depth, 1.0), B.FrontOverhang, P.IsHallAndRoll() ? 0.0 : P.RoofApexRoll);
	B.Rise = P.GetRoofRise();
	// 一殿一卷: two column rows, and one roof reads the pair at a distance.
	if (P.IsHallAndRoll()) B.ColumnRows = 2;
	return B;
}

FBlock From(const FHutongCorridorParams& P)
{
	FBlock B;
	B.Width = FMath::Max(P.Length, 1.0);
	B.Depth = P.GetFootprintDepth();
	B.FloorHeight = FMath::Max(P.FloorHeight, 0.0);
	B.EaveHeight = P.GetRoofBaseHeight();
	B.bOpen = true;
	B.ColumnSize = FMath::Max(P.ColumnDiameter, 4.0);
	B.ColumnsAlongWidth = FMath::Clamp(
		FMath::RoundToInt32(B.Width / FMath::Max(P.BaySpacing, 40.0)) + 1, 2, 64);
	B.ColumnRows = 2;
	B.FrontOverhang = FMath::Max(P.RoofOverhang, 0.0);
	B.RearOverhang = B.FrontOverhang;
	B.GableOverhang = FMath::Max(P.GableOverhang, 0.0);
	B.Section = Jiajia::MakeSection(
		EHutongPurlins::Three, 0.5 * B.Depth, B.FrontOverhang, P.GetRoofRoll());
	B.Rise = P.GetRoofRise();
	return B;
}

FBlock From(const FHutongPaifangParams& P)
{
	FBlock B;
	B.Width = FMath::Max(P.Length, 1.0);
	B.Depth = FMath::Max(P.Depth, 1.0);
	B.EaveHeight = P.GetRoofEaveZ();
	// Cart opening: a solid block would wall off the street.
	B.bOpen = true;
	B.ColumnSize = FMath::Max(P.ColumnDiameter, 4.0);
	B.ColumnsAlongWidth = P.GetBayCount() + 1;
	B.ColumnRows = 1;
	// 樓 are 廡殿: one hipped roof over the span at the central bay's height stands in for them.
	B.Roof = P.bHasRoofs ? ERoof::Hipped : ERoof::None;
	B.RidgeLength = FMath::Max(B.Width - B.Depth, 0.0);
	B.FrontOverhang = FMath::Max(P.RoofOverhang, 0.0);
	B.RearOverhang = B.FrontOverhang;
	B.Section = Jiajia::MakeSection(EHutongPurlins::Three, 0.5 * B.Depth, B.FrontOverhang, 0.0);
	B.Rise = P.GetRoofRise();
	return B;
}

FBlock From(const FHutongPavilionParams& P)
{
	FBlock B;
	B.Width = P.Width;
	B.Depth = P.Depth;
	B.FloorHeight = P.GetFloorHeight();
	B.PlatformOverhang = P.GetPlatformOverhang();
	B.EaveHeight = P.GetRoofBaseHeight();
	B.bOpen = true;
	B.ColumnSize = FMath::Max(P.GetColumnDiameter(), 4.0);
	B.ColumnsAlongWidth = 2;
	B.ColumnRows = 2;
	B.Roof = (P.RoofType == EHutongRoofType::Xieshan) ? ERoof::Xieshan : ERoof::Hipped;
	B.ShouInset = P.ShouInset;
	B.RidgeLength = (P.RoofType == EHutongRoofType::Wudian)
		? FMath::Max(P.Width - P.Depth, 0.0) : 0.0;
	B.FrontOverhang = P.GetRoofOverhang();
	B.Section = P.GetRoofSection();
	// Set explicitly: the generator reads the field.
	B.Rise = P.GetRoofRise();
	return B;
}

FBlock From(const FHutongHallParams& P)
{
	FBlock B;
	namespace GH = HutongCanon::GrandHall;
	B.Width = P.Width;
	B.Depth = P.Depth;
	B.FloorHeight = P.IsGrand() ? GH::PlatformHeight * P.GetGrandScale() : P.FloorHeight;
	B.PlatformOverhang = P.IsGrand() ? GH::PlatformReach * P.GetGrandScale() : P.PlatformOverhang;
	B.EaveHeight = P.GetRoofBaseHeight();
	B.Roof = (P.IsGrand() || P.RoofType == EHutongRoofType::Xieshan) ? ERoof::Xieshan : ERoof::Hipped;
	B.ShouInset = P.IsGrand() ? GH::ShouInset * P.GetGrandScale() : P.ShouInset;
	B.FrontOverhang = P.GetRoofOverhangBuilt();
	B.Section = P.GetRoofSection(P.Depth);
	// RidgeLength is on the roof rectangle, which oversails the body.
	B.RidgeLength = (!P.IsGrand() && P.RoofType == EHutongRoofType::Wudian)
		? FMath::Max(P.Width - P.Depth, 0.0) : 0.0;
	B.Rise = P.GetRoofRise(P.Depth);
	return B;
}

} // namespace Massing
} // namespace HutongGen
