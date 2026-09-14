#pragma once

#include "CoreMinimal.h"
#include "Generation/HutongJiajia.h"
#include "Generation/HutongRoofTile.h"
#include "DynamicMesh/DynamicMesh3.h"

namespace HutongGen
{
	// The masonry envelope every 硬山 building shares: 臺基, 下鹼, 墀頭 and the gable roof.
	namespace Shell
	{
		// 臺基, projecting in front of the facade only; sides and rear stay flush.
		void AppendPlatform(
			UE::Geometry::FDynamicMesh3& Mesh,
			double Width, double Depth, double FloorHeight,
			double FrontOverhang, double SideProjection,
			int32 StepCount, double StepTread, double StepX0, double StepX1);

		// 散水 outside the rectangle [X0,Y0]-[X1,Y1]: four runs tiling the way 下鹼's boxes do.
		void AppendApron(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double Y0, double X1, double Y1,
			double FrontWidth, double RearWidth, double SideWidth, double ApronThickness);

		// 踏跺 falling away from EdgeY: Direction -1 for -Y, +1 for +Y.
		void AppendSteps(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double EdgeY, double Direction,
			int32 StepCount, double StepTread, double FloorHeight);

		// 下鹼 round the left, right and rear walls.
		void AppendBaseCourseU(
			UE::Geometry::FDynamicMesh3& Mesh,
			double Width, double Depth, double WallThickness,
			double FloorHeight, double CourseHeight, double Projection,
			bool bIncludeRear = true, bool bToGround = false);

		// 墀頭: a corbelled pier at the front corner of each gable wall.
		void AppendChitou(
			UE::Geometry::FDynamicMesh3& Mesh,
			double Width, double WallThickness,
			double FloorHeight, double EaveHeight,
			double Projection, int32 CorbelSteps);

		struct FRoofParams
		{
			double FrontOverhang = 60.0;
			// 封護檐: a rear eave onto the lane barely projects. Not a mirror of the front.
			double RearOverhang = 0.0;

			// How much of the rear slope is cut away, so it terminates over the wall rather than out in the lane.
			double RearSlopeTrim = 0.0;

			// 懸山 throw past each gable end. Zero is 硬山, the flush gable a hutong house has.
			double GableOverhang = 0.0;

			// 舉架.
			HutongGen::FHutongRoofSection Section;

			// Ridge above the eave.
			double Rise = 0.0;

			// Only subdivides a 捲棚 roll; the straight steps need no facets beyond their own.
			int32 SlopeSegments = 8;

			// The eave course, without which the roof ends in a knife edge.
			double FasciaDepth = 8.0;
			double FasciaWidth = 12.0;

			// 合瓦 unless the building is entitled to better — a rank rule, see EHutongRoofTile.
			EHutongRoofTile Tile = EHutongRoofTile::He;
			double TileRowSpacing = HutongGen::RoofTile::DefaultRowSpacing;

			// 椽頭 showing in a row under the eave tiles. Zero section turns them off.
			double RafterSection = 7.0;
			double RafterSpacing = 24.0;

			// 飛椽, the square outer course on the 檐椽's ends. Off leaves the round 檐椽 at the eave.
			bool bFlyingRafters = true;

			// 正脊. Turn ApexRoll to zero alongside it: a ridge sits on a fold, not a roll.
			bool bHasRidgeCourse = false;
			double RidgeCourseHeight = 0.0;
			double RidgeCourseWidth = 0.0;
			double RidgeEndKick = 0.0;
		};

		// Gable roof, eave course and ridge. Owns its own material tagging.
		void AppendGableRoof(
			UE::Geometry::FDynamicMesh3& Mesh,
			double Width, double Depth, double EaveHeight,
			const FRoofParams& Roof);

		// How far above the eave line a trimmed rear slope terminates.
		double RearEaveLift(const FRoofParams& Roof, double Depth);

		double RoofRise(const FRoofParams& Roof);

		// 檐椽頭: the round lower rafter ends, set back and down from the square 飛椽 above them.
		void AppendRoundRafterEnds(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double Y0, double Y1,
			double CentreZ, double Diameter, double Spacing, int32 Sides = 6);

		// 飛椽頭 spanning X0..X1 across Y0..Y1.
		void AppendRafterEnds(
			UE::Geometry::FDynamicMesh3& Mesh,
			double X0, double X1, double Y0, double Y1,
			double TopZ, double Section, double Spacing);
	}
}
