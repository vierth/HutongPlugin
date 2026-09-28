#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRoofTile.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongRearEave.h"

namespace HutongGen
{
	// Shared 硬山 envelope: 臺基, 下鹼, 墀頭, gable roof.
	namespace Shell
	{
		// 臺基, projecting at the front only.
		void AppendPlatform(
			UE::Geometry::FDynamicMesh3& Mesh,
			double Width, double Depth, double FloorHeight,
			double FrontOverhang, double SideProjection,
			int32 StepCount, double StepTread, double StepX0, double StepX1, bool bPaved = true);

		// 階條石 band round a paved 臺明 (about 1.3 柱徑 of a house's).
		inline constexpr double PlatformEdgeCm = 32.0;

		// 踏跺 descending from EdgeY; Direction ±1 = ±Y.
		void AppendSteps(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double EdgeY, double Direction,
			int32 StepCount, double StepTread, double FloorHeight);

		// 下鹼 on left, right, rear walls; side runs gapped over SideGapY0..SideGapY1 if non-empty.
		void AppendBaseCourseU(
			UE::Geometry::FDynamicMesh3& Mesh,
			double Width, double Depth, double WallThickness,
			double FloorHeight, double CourseHeight, double Projection,
			bool bIncludeRear = true, bool bToGround = false,
			double SideGapY0 = 0.0, double SideGapY1 = 0.0,
			// The back wall's thickness where it differs from the side walls'; negative: the same.
			double RearWallThickness = -1.0);

		struct FRoofParams;

		// 墀頭 at each gable's front corner: 上身 pier, 盤頭 corbel, 戧檐 out to the 連檐, closing the eave
		// corner under the 博縫. Also at a rear eave showing rafters. Depth = building's, for the overhangs.
		void AppendChitou(
			UE::Geometry::FDynamicMesh3& Mesh,
			double Width, double Depth, double WallThickness,
			double FloorHeight, double EaveHeight,
			double Projection, int32 CorbelSteps, const FRoofParams& Roof,
			// The pier stands on the ground, its foot in the wall's 下鹼 (this high above the floor, this proud).
			double BaseCourseHeight = 0.0, double BaseCourseProjection = 0.0);

		// 上身 projection past the wall end: max(half the overhang, Projection); 盤頭 and 戧檐 finish the reach.
		double ChitouBodyProjection(double Projection, double Overhang);

		struct FRoofParams
		{
			double FrontOverhang = 60.0;
			// 封護檐: a lane-side rear eave barely projects.
			double RearOverhang = 0.0;

			// Rear slope cut back to end over the wall, not in the lane.
			double RearSlopeTrim = 0.0;
			// The brick cornice under a 封護檐 drip course.
			EHutongSealedCornice SealedCornice = EHutongSealedCornice::IceTray;
			// Lowest the cornice may reach down the back wall (above any 高窗); unset keeps it above the eave line.
			double SealedCorniceFloorZ = -1.0e9;

			// 懸山 overhang past each gable; zero = 硬山.
			double GableOverhang = 0.0;

			// Over rafters: the ceiling at the column line, above the roof's base (where the rafters rest on
			// the 檐檁). HutongGen::Proportions::UndersideRise.
			double UndersideRise = 0.0;

			// 徹上明造: the roof a shell this thick (plumb) over rafters that run on up to the ridge, the attic
			// triangle closed by the 山牆 (GableWallThickness) or a board on a 懸山. Zero: flat ceiling.
			double ShellCover = 0.0;
			double GableWallThickness = 0.0;

			// Behind a 前廊, the depth of the wall face on the 金柱 line from the front column line: the 檐椽 run
			// open over the 廊 up to the 金檁, the underside rising with the slope (FGableUnderside::VerandaEnd).
			// Zero: the level ceiling runs out to the eave columns.
			double VerandaEnd = 0.0;

			HutongGen::FHutongRoofSection Section;

			// Ridge above eave.
			double Rise = 0.0;

			// Subdivides only a 捲棚 roll.
			int32 SlopeSegments = 8;

			// Eave course; without it the roof ends in a knife edge.
			double FasciaDepth = 8.0;
			double FasciaWidth = 12.0;

			// 合瓦 unless rank allows 筒瓦 (EHutongRoofTile).
			EHutongRoofTile Tile = EHutongRoofTile::He;
			double TileRowSpacing = HutongGen::RoofTile::DefaultRowSpacing;

			// 椽頭 row under the eave tiles; zero = off.
			double RafterSection = 7.0;
			double RafterSpacing = 24.0;

			// 椽頭 inset from each closed end, where the 墀頭 stands.
			double RafterEndInset = 0.0;

			// 飛椽 on the 檐椽 ends; off leaves the round 檐椽.
			bool bFlyingRafters = true;

			// 博縫 (brick band proud of the 山牆) and 排山勾滴 on it, each 硬山 gable. Zero = off; 懸山 has neither.
			double RakeDepth = 26.0;
			double RakeProjection = 6.0;

			// 壟 as geometry: 筒瓦 or 蓋瓦 course swept over the ridge per row at the 勾頭 pitch; slope texture aligned under them.
			bool bTileRuns = true;

			// Low end continues into another roof (耳房過道): no rake, pediment or ridge tail; ridge and eave run to the end.
			bool bOpenLowEnd = false;
			bool bOpenHighEnd = false;

			// Timber 懸山 (垂花門): 山花 boarding and a 梅花釘 博縫板 down each rake.
			bool bWoodenGable = false;
			double BargeboardDepth = 26.0;
			double BargeboardThickness = 5.0;

			// X a tile row must hit, to continue another roof's rows. Unset: centred between closed ends, or
			// laid from the one closed end.
			bool bHasTileRowPhase = false;
			double TileRowPhase = 0.0;

			// 正脊. Set ApexRoll to zero with it: a ridge sits on a fold.
			bool bHasRidgeCourse = false;
			double RidgeCourseHeight = 0.0;
			double RidgeCourseWidth = 0.0;
			double RidgeEndKick = 0.0;
		};

		// Gable roof, eave course, ridge. Tags its own materials.
		void AppendGableRoof(
			UE::Geometry::FDynamicMesh3& Mesh,
			double Width, double Depth, double EaveHeight,
			const FRoofParams& Roof);

		// Height above eave where a trimmed rear slope ends.
		double RearEaveLift(const FRoofParams& Roof, double Depth);

		double RoofRise(const FRoofParams& Roof);

		// Where the gable roof's underside stands at depth Y (0 = front column line): the level ceiling, over a
		// 前廊 (VerandaEnd) the slope less the level ceiling's depth under it at the column line, or under a
		// 徹上明造 shell the slope less its cover. For a wall on an interior line to meet it.
		double UndersideAt(const FRoofParams& Roof, double Depth, double RoofBase, double Y);

		// A wall on an interior line under a 徹上明造 shell carried up from BottomZ to meet it, its top
		// following the slope across the wall's thickness (flat, it would stand through the thin shell).
		// Nothing without a shell or a 前廊's open underside. Caller's material scope.
		void AppendWallToUnderside(UE::Geometry::FDynamicMesh3& Mesh, const FRoofParams& Roof, double Depth,
			double RoofBase, double X0, double X1, double Y0, double Y1, double BottomZ);

		// 檐椽: round rafters across Y0..Y1, centre at CentreZ at Y0, climbing Rise by Y1.
		void AppendRoundRafterEnds(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double Y0, double Y1,
			double CentreZ, double Diameter, double Spacing, int32 Sides = 6, double Rise = 0.0);

		// 飛椽頭 spanning X0..X1 across Y0..Y1.
		void AppendRafterEnds(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double Y0, double Y1,
			double TopZ, double Section, double Spacing);

		// What goes on a roof whatever its shape: 壟 courses square to each eave, stopped at the hips;
		// 椽頭 under the eave course, fanning into a 翼角; 垂脊 and 正脊 on a hipped roof.
		// Stations up a dressed course.
		inline constexpr int32 CourseStations = 12;
		// About one station per 45 cm of slope, 4 to CourseStations: a 牌樓's 1.5 m slope needs 4, a 殿's 12
		// (more only rode between the facets it lies on, and looked the same).
		inline int32 CourseStationsFor(double SlopeLength)
		{
			return FMath::Clamp(FMath::CeilToInt32(SlopeLength / 45.0), 4, CourseStations);
		}

		struct FRoofDressing
		{
			EHutongRoofTile Tile = EHutongRoofTile::He;
			double TileRowSpacing = HutongGen::RoofTile::DefaultRowSpacing;
			// How far the eave course hangs below the eave line.
			double FasciaDrop = 8.0;
			bool bTileRuns = true;
			double RafterSection = 0.0;   // zero: no 椽頭
			double RafterSpacing = 22.0;
			bool bFlyingRafters = true;
			// 椽頭 evenly along each whole panel, no margin kept for a 角梁: a round roof's panels meet without hips.
			bool bNoCorners = false;
			// Eave to column line; the 檐椽 run up the soffit to it. Zero: stubs under a flat soffit.
			double EaveOverhang = 0.0;
			// 垂脊 up the four hips and 正脊 along the top; zero: none (a 歇山 builds its own).
			double RidgeWidth = 0.0;
			double RidgeHeight = 0.0;
		};

		// A dressed roof's surface fine enough for its courses: facets coarser than the courses bulge over
		// them in a 翼角 and break the eave line. Eave segments of about two rows; as many rows up as a
		// course has stations.
		inline void DressedRoofSegments(int32& EaveSegments, int32& SlopeSegments, double LongEave, double SlopeLength, double Pitch)
		{
			EaveSegments = FMath::Max(EaveSegments, FMath::CeilToInt32(LongEave / (2.0 * FMath::Max(Pitch, 4.0))));
			SlopeSegments = FMath::Max(SlopeSegments, CourseStationsFor(SlopeLength));
		}

		// Tags its own parts: courses tile, 椽頭 彩畫, ridges 正脊, and on 筒瓦 a 勾頭 at each course's foot
		// (the roof itself lays none then). Call after the roof's own tagging.
		void AppendRoofDressing(
			UE::Geometry::FDynamicMesh3& Mesh,
			const TArray<HutongMeshUtils::FRoofPanel>& Panels,
			const FRoofDressing& Dressing);

		// 圓攢尖's 壟: one course per slope line, eave to StopRadius (under the 寶頂), each tapering with the
		// circle so every one reaches the top, a 勾頭 at its foot. Tags itself tile.
		void AppendRoundRoofCourses(
			UE::Geometry::FDynamicMesh3& Mesh,
			const FVector3d& Centre,
			const HutongMeshUtils::FRoundRoofSpec& Spec,
			EHutongRoofTile Tile, double FasciaDrop, double StopRadius);
	}
}
