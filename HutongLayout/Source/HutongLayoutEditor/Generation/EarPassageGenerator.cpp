#include "Generation/EarPassageGenerator.h"
#include "Generation/HutongCanon.h"
#include "Generation/HutongMeshUtils.h"
#include "Tools/HutongPresetDefaults.h"

using UE::Geometry::FDynamicMesh3;

FHutongEarPassageParams::FHutongEarPassageParams()
{
	Room = HutongPresets::MakeHouse(HutongCanon::House::EarRoom);
	// Both walls are the compound's 隔牆: the passage is an inside way, not the plot's edge.
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
		const double Tw = P.GetOuterWallThickness();
		const double ClearX0 = P.GetClearX0();
		const bool bBlock = Detail::IsMassing(Detail);

		// The room, at its own end of the frontage.
		{
			FHutongSiheyuanParams R = P.RoomParams();
			Detail::Apply(Detail, R);
			const int32 V0 = Mesh.MaxVertexID();
			if (bBlock) Massing::AppendBlock(Mesh, Massing::From(R));
			else BuildSiheyuan(Mesh, R);
			HutongMeshUtils::TransformVerticesFrom(Mesh, V0, FTransform(FVector(P.GetRoomX0(), 0.0, 0.0)));
		}

		// The wall the passage runs along, the full depth, on the outer side.
		if (P.bHasOuterWall)
		{
			FHutongWallParams Wp = P.OuterWall;
			Wp.Length = D;
			Wp.FootprintThickness = Tw;
			Wp.bHasGate = false;
			Wp.Doorway = EHutongWallDoorway::None;
			Detail::Apply(Detail, Wp);
			const int32 V0 = Mesh.MaxVertexID();
			BuildWall(Mesh, Wp);
			// Built along X; turned onto Y it lands at x in [-T, 0], then slid to the outer edge.
			HutongMeshUtils::YawVerticesFrom(Mesh, V0, FVector2d::ZeroVector, 90.0);
			HutongMeshUtils::TransformVerticesFrom(Mesh, V0, FTransform(FVector(P.GetOuterWallX0() + Tw, 0.0, 0.0)));
		}

		// The 隔牆 across the front of the way through, with the doorway it was given.
		if (P.bHasClosingWall)
		{
			FHutongWallParams Cw = P.ClosingWallParams();
			Detail::Apply(Detail, Cw);
			const int32 V0 = Mesh.MaxVertexID();
			BuildWall(Mesh, Cw);
			HutongMeshUtils::TransformVerticesFrom(Mesh, V0, FTransform(FVector(ClearX0, 0.0, 0.0)));
		}

		// The roof over the way through: run along the depth, bearing into the wall and the gable.
		{
			FHutongPassageParams Pass = P.PassageParams();
			Detail::Apply(Detail, Pass);
			const double Span = Pass.GetRoofSpan();
			const double Bear = FMath::Max(Pass.Bearing, 0.0);
			const int32 V0 = Mesh.MaxVertexID();
			if (bBlock) Massing::AppendBlock(Mesh, Massing::From(Pass));
			else BuildPassage(Mesh, Pass);
			// Built with the run along X and the span along Y; turned onto Y the span lands at x in [-Span, 0].
			HutongMeshUtils::YawVerticesFrom(Mesh, V0, FVector2d::ZeroVector, 90.0);
			HutongMeshUtils::TransformVerticesFrom(Mesh, V0, FTransform(FVector(ClearX0 - Bear + Span, 0.0, 0.0)));
		}
	}
}
