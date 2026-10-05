#include "Generation/EarPassageGenerator.h"
#include "Generation/HutongCanon.h"
#include "Generation/HutongMeshUtils.h"
#include "Tools/HutongPresetDefaults.h"

using UE::Geometry::FDynamicMesh3;

FHutongEarPassageParams::FHutongEarPassageParams()
{
	Room = HutongPresets::MakeHouse(HutongCanon::House::EarRoom);
	// Both walls are 隔牆: the passage is an inner way, not the plot edge.
	OuterWall.Role = EHutongWallRole::Courtyard;
	OuterWall.bHasWindows = false;
	ClosingWall.Role = EHutongWallRole::Courtyard;
	ClosingWall.bHasWindows = false;
	ClosingWall.Doorway = EHutongWallDoorway::Rect;
	ClosingWall.DoorwayPosition = 0.5;
}

namespace HutongGen
{
	void BuildEarPassage(FDynamicMesh3& Mesh, const FHutongEarPassageParams& P, EHutongDetail Detail)
	{
		const double D = FMath::Max(P.Depth, 1.0);
		const double Tw = P.OuterWall.GetThickness();
		const double ClearX0 = P.GetClearX0();
		const bool bBlock = Detail::IsMassing(Detail);

		// One roof over room and passage reads as one block at a distance.
		if (bBlock && P.bRoofOverPassage)
		{
			FHutongSiheyuanParams Whole = P.RoomParams();
			Whole.Width = P.GetWidth();
			Whole.bRoofRunsOnLow = Whole.bRoofRunsOnHigh = false;
			Detail::Apply(Detail, Whole);
			Massing::AppendBlock(Mesh, Massing::From(Whole));
			return;
		}

		// Room, at its own end of the frontage.
		if (P.HasRoom())
		{
			FHutongSiheyuanParams R = P.RoomParams();
			Detail::Apply(Detail, R);
			const int32 V0 = Mesh.MaxVertexID();
			if (bBlock) Massing::AppendBlock(Mesh, Massing::From(R));
			else BuildSiheyuan(Mesh, R);
			HutongMeshUtils::TransformVerticesFrom(Mesh, V0, FTransform(FVector(P.GetRoomX0(), 0.0, 0.0)));
		}
		if (!P.HasPassage()) return;

		// Walls along the passage, full depth: the outer one, and over the whole frontage the inner one.
		auto AppendSideWall = [&](double WallX0)
		{
			FHutongWallParams Wp = P.OuterWall;
			if (P.bRoofOverPassage)
			{
				// The roof end closes over it as the gable: wall stops at the eave, capless.
				Wp.PinHeight(P.GetPassageEaveHeight());
				Wp.CapSlabHeight = 0.0;
				Wp.CapRidgeHeight = 0.0;
			}
			Wp.Length = D;
			Wp.FootprintThickness = Tw;
			Wp.bHasGate = false;
			Wp.Doorway = EHutongWallDoorway::None;
			Detail::Apply(Detail, Wp);
			const int32 V0 = Mesh.MaxVertexID();
			BuildWall(Mesh, Wp);
			// Built along X; yawed onto Y it lands at x in [-T, 0], then slid to its edge.
			HutongMeshUtils::YawVerticesFrom(Mesh, V0, FVector2d::ZeroVector, 90.0);
			HutongMeshUtils::TransformVerticesFrom(Mesh, V0, FTransform(FVector(WallX0 + Tw, 0.0, 0.0)));
		};
		if (P.bHasOuterWall) AppendSideWall(P.GetOuterWallX0());
		if (P.GetInnerWallThickness() > 0.0) AppendSideWall(P.GetInnerWallX0());

		// 隔牆 across the front of the way, with its doorway.
		if (P.bHasClosingWall)
		{
			FHutongWallParams Cw = P.ClosingWallParams();
			Detail::Apply(Detail, Cw);
			const int32 V0 = Mesh.MaxVertexID();
			BuildWall(Mesh, Cw);
			HutongMeshUtils::TransformVerticesFrom(Mesh, V0, FTransform(FVector(ClearX0, 0.0, 0.0)));
		}

		// Room roof carried over the way, tile rows in step with the room's.
		if (P.bRoofOverPassage)
		{
			FHutongSiheyuanParams R = P.RoomParams();
			Detail::Apply(Detail, R);
			const double Pitch = FMath::Max(R.TileRowSpacing, 4.0);
			const double RoomX0 = P.GetRoomX0();
			const double SX0 = P.GetStripX0();
			if (!P.HasRoom())
			{
				// The passage is the whole building: closed at both gables, a row on the middle.
				AppendHouseRoofRun(Mesh, R, SX0, SX0 + P.GetStripWidth(), false, false, SX0 + 0.5 * P.GetStripWidth());
				return;
			}
			// The room lays rows from its closed gable; the passage continues from there.
			const double Phase = P.bPassageAtFarEnd ? RoomX0 + 0.5 * Pitch : RoomX0 + P.GetRoomWidth() - 0.5 * Pitch;
			AppendHouseRoofRun(Mesh, R, SX0, SX0 + P.GetStripWidth(),
				/*bOpenLow*/ P.bPassageAtFarEnd, /*bOpenHigh*/ !P.bPassageAtFarEnd, Phase);
			return;
		}

		// Passage roof along the depth, bearing into wall and gable.
		{
			FHutongPassageParams Pass = P.PassageParams();
			Detail::Apply(Detail, Pass);
			const double Span = Pass.GetRoofSpan();
			const double Bear = FMath::Max(Pass.Bearing, 0.0);
			const int32 V0 = Mesh.MaxVertexID();
			if (bBlock) Massing::AppendBlock(Mesh, Massing::From(Pass));
			else BuildPassage(Mesh, Pass);
			// Built run along X, span along Y; yawed onto Y the span lands at x in [-Span, 0].
			HutongMeshUtils::YawVerticesFrom(Mesh, V0, FVector2d::ZeroVector, 90.0);
			HutongMeshUtils::TransformVerticesFrom(Mesh, V0, FTransform(FVector(ClearX0 - Bear + Span, 0.0, 0.0)));
		}
	}
}
