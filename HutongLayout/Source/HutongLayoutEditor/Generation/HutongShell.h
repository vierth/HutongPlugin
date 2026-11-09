#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRoofTile.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongRearEave.h"

namespace HutongGen
{
	namespace Shell
	{
		void AppendPlatform(
			UE::Geometry::FDynamicMesh3& Mesh,
			double Width, double Depth, double FloorHeight,
			double FrontOverhang, double SideProjection,
			int32 StepCount, double StepTread, double StepX0, double StepX1, bool bPaved = true);

		// 階條石 band round a paved 臺明 (about 1.3 柱徑 of a house's).
		inline constexpr double PlatformEdgeCm = 32.0;

		// Direction ±1 = ±Y.
		void AppendSteps(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double EdgeY, double Direction,
			int32 StepCount, double StepTread, double FloorHeight);

		void AppendBaseCourseU(
			UE::Geometry::FDynamicMesh3& Mesh,
			double Width, double Depth, double WallThickness,
			double FloorHeight, double CourseHeight, double Projection,
			bool bIncludeRear = true, bool bToGround = false,
			double SideGapY0 = 0.0, double SideGapY1 = 0.0,
			// Negative: same as WallThickness.
			double RearWallThickness = -1.0);

		struct FRoofParams;

		void AppendChitou(
			UE::Geometry::FDynamicMesh3& Mesh,
			double Width, double Depth, double WallThickness,
			double FloorHeight, double EaveHeight,
			double Projection, int32 CorbelSteps, const FRoofParams& Roof,
			double BaseCourseHeight = 0.0, double BaseCourseProjection = 0.0);

		double ChitouBodyProjection(double Projection, double Overhang);

		struct FRoofParams
		{
			double FrontOverhang = 60.0;
			double RearOverhang = 0.0;

			double RearSlopeTrim = 0.0;
			EHutongSealedCornice SealedCornice = EHutongSealedCornice::IceTray;
			// Lowest Z the cornice may reach down the back wall (above any 高窗); the default keeps it above the eave line.
			double SealedCorniceFloorZ = -1.0e9;

			// Zero = 硬山.
			double GableOverhang = 0.0;

			// Ceiling at the column line above the roof's base (HutongGen::Proportions::UndersideRise).
			double UndersideRise = 0.0;

			double ShellCover = 0.0;
			double GableWallThickness = 0.0;

			double VerandaEnd = 0.0;

			HutongGen::FHutongRoofSection Section;

			double Rise = 0.0;

			int32 SlopeSegments = 8;

			double FasciaDepth = 8.0;
			double FasciaWidth = 12.0;

			EHutongRoofTile Tile = EHutongRoofTile::He;
			double TileRowSpacing = HutongGen::RoofTile::DefaultRowSpacing;

			double RafterSection = 7.0;
			double RafterSpacing = 24.0;

			double RafterEndInset = 0.0;

			bool bFlyingRafters = true;

			double RakeDepth = 26.0;
			double RakeProjection = 6.0;

			bool bTileRuns = true;

			bool bOpenLowEnd = false;
			bool bOpenHighEnd = false;

			bool bWoodenGable = false;
			double BargeboardDepth = 26.0;
			double BargeboardThickness = 5.0;

			// X a tile row must hit, to continue another roof's rows.
			bool bHasTileRowPhase = false;
			double TileRowPhase = 0.0;

			// Set ApexRoll to zero with it: a ridge sits on a fold.
			bool bHasRidgeCourse = false;
			double RidgeCourseHeight = 0.0;
			double RidgeCourseWidth = 0.0;
			double RidgeEndKick = 0.0;
		};

		// Tags its own materials.
		void AppendGableRoof(
			UE::Geometry::FDynamicMesh3& Mesh,
			double Width, double Depth, double EaveHeight,
			const FRoofParams& Roof);

		double RearEaveLift(const FRoofParams& Roof, double Depth);

		double RoofRise(const FRoofParams& Roof);

		double UndersideAt(const FRoofParams& Roof, double Depth, double RoofBase, double Y);

		// Caller's material scope.
		void AppendWallToUnderside(UE::Geometry::FDynamicMesh3& Mesh, const FRoofParams& Roof, double Depth,
			double RoofBase, double X0, double X1, double Y0, double Y1, double BottomZ);

		void AppendRoundRafterEnds(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double Y0, double Y1,
			double CentreZ, double Diameter, double Spacing, int32 Sides = 6, double Rise = 0.0);

		void AppendRafterEnds(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double Y0, double Y1,
			double TopZ, double Section, double Spacing);

		inline constexpr int32 CourseStations = 12;
		// About one station per 45 cm of slope: a 牌樓's 1.5 m slope needs 4, a 殿's 12.
		inline int32 CourseStationsFor(double SlopeLength)
		{
			return FMath::Clamp(FMath::CeilToInt32(SlopeLength / 45.0), 4, CourseStations);
		}

		struct FRoofDressing
		{
			EHutongRoofTile Tile = EHutongRoofTile::He;
			double TileRowSpacing = HutongGen::RoofTile::DefaultRowSpacing;
			double FasciaDrop = 8.0;
			bool bTileRuns = true;
			double RafterSection = 0.0;
			double RafterSpacing = 22.0;
			bool bFlyingRafters = true;
			bool bNoCorners = false;
			double EaveOverhang = 0.0;
			double RidgeWidth = 0.0;
			double RidgeHeight = 0.0;
		};

		// Facets coarser than the courses bulge over them in a 翼角 and break the eave line.
		inline void DressedRoofSegments(int32& EaveSegments, int32& SlopeSegments, double LongEave, double SlopeLength, double Pitch)
		{
			EaveSegments = FMath::Max(EaveSegments, FMath::CeilToInt32(LongEave / (2.0 * FMath::Max(Pitch, 4.0))));
			SlopeSegments = FMath::Max(SlopeSegments, CourseStationsFor(SlopeLength));
		}

		// Tags its own parts; call after the roof's own tagging.
		void AppendRoofDressing(
			UE::Geometry::FDynamicMesh3& Mesh,
			const TArray<HutongMeshUtils::FRoofPanel>& Panels,
			const FRoofDressing& Dressing);

		// Tags itself tile.
		void AppendRoundRoofCourses(
			UE::Geometry::FDynamicMesh3& Mesh,
			const FVector3d& Centre,
			const HutongMeshUtils::FRoundRoofSpec& Spec,
			EHutongRoofTile Tile, double FasciaDrop, double StopRadius);
	}
}
