#include "Generation/HutongDoor.h"
#include "Generation/HutongMeshUtils.h"
#include "Math/Quat.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
	void AppendDrumStone(
		FDynamicMesh3& Mesh,
		double X0, double X1,
		double Y0, double Y1,
		double BaseZ,
		double Height,
		double DrumFraction,
		int32 Sides)
	{
		using namespace HutongMeshUtils;

		const double W = X1 - X0;
		const double Dp = Y1 - Y0;
		if (W <= 0.0 || Dp <= 0.0 || Height <= 0.0)
		{
			return;
		}

		// The drum overhangs the base it stands on, in the two directions its faces look.
		double R = 0.5 * FMath::Clamp(DrumFraction, 0.2, 0.9) * Height;
		R = FMath::Min(R, 0.42 * Height);

		// Its top is the stone's top, and the plinth rises past the drum's underside.
		const double DrumCz = Height - R;
		const double PlinthH = FMath::Max(Height - 1.75 * R, 0.1 * Height);

		AppendBox(Mesh, FVector3d(X0, Y0, BaseZ), FVector3d(X1, Y1, BaseZ + PlinthH));

		// Inset in X rather than flush with the plinth.
		const double Inset = 0.06 * W;
		const FQuat LayAcross = FQuat::FindBetweenNormals(FVector::UpVector, FVector::XAxisVector);
		const FVector Centre(0.0, 0.5 * (Y0 + Y1), BaseZ + DrumCz);

		const TArray<FVector2d> Disc = MakeCircleProfile(R, FMath::Max(Sides, 8));
		const TArray<FTransform> Stations = {
			FTransform(LayAcross, Centre + FVector(X0 + Inset, 0.0, 0.0)),
			FTransform(LayAcross, Centre + FVector(X1 - Inset, 0.0, 0.0)),
		};
		AppendSweptProfile(Mesh, Disc, Stations);
	}

	double DoorStoneReveal(double JambThickness, double DoorWidth)
	{
		return FMath::Min(0.3 * FMath::Max(JambThickness, 1.0), 0.05 * FMath::Max(DoorWidth, 1.0));
	}

	void AppendDoorStonePair(
		FDynamicMesh3& Mesh,
		const FHutongDoorStoneParams& Stones,
		double ClearX0, double ClearX1,
		double FrontY, double BackY,
		double BaseZ,
		double JambThickness,
		double MaxOutward,
		double MinProjection)
	{
		using namespace HutongMeshUtils;

		const double H = FMath::Max(Stones.GetHeight(), 0.0);
		const double DoorW = ClearX1 - ClearX0;
		if (!Stones.bEnabled || H <= 0.0 || DoorW <= 1.0) return;

		const double JambT = FMath::Max(JambThickness, 1.0);
		// Held inside whatever room the caller says there is.
		const double W = FMath::Clamp(FMath::Max(JambT * 2.0, 26.0), 4.0, FMath::Max(MaxOutward, 4.0));
		const double Reveal = DoorStoneReveal(JambT, DoorW);
		const double Proj = FMath::Max(Stones.Projection, MinProjection);

		// A 門枕石 is two things: a small 枕 running in under the leaf carrying the pivot socket, and the 門墩 proper, the mass you see, sitting wholly on the threshold in front of the doors.
		const double PlaneFront = FMath::Min(FrontY, BackY);
		const double PlaneBack = FMath::Max(FrontY, BackY);
		const double Y0 = PlaneFront - Proj;
		const double Y1 = PlaneFront + 0.55 * (PlaneBack - PlaneFront);
		const double LX0 = ClearX0 - W,      LX1 = ClearX0 + Reveal;
		const double RX0 = ClearX1 - Reveal, RX1 = ClearX1 + W;

		if (Stones.Style == EHutongDoorStone::Drum)
		{
			AppendDrumStone(Mesh, LX0, LX1, Y0, Y1, BaseZ, H, Stones.DrumFraction);
			AppendDrumStone(Mesh, RX0, RX1, Y0, Y1, BaseZ, H, Stones.DrumFraction);
		}
		else
		{
			AppendBox(Mesh, FVector3d(LX0, Y0, BaseZ), FVector3d(LX1, Y1, BaseZ + H));
			AppendBox(Mesh, FVector3d(RX0, Y0, BaseZ), FVector3d(RX1, Y1, BaseZ + H));
		}
	}

	void AppendDoorAssembly(FDynamicMesh3& Mesh, const FHutongDoorAssembly& D,
		int32* OutLeafFirstTriangle)
	{
		using namespace HutongMeshUtils;

		const double X0 = FMath::Min(D.OpeningX0, D.OpeningX1);
		const double X1 = FMath::Max(D.OpeningX0, D.OpeningX1);
		const double DoorW = X1 - X0;
		if (DoorW <= 1.0)
		{
			return;
		}

		const double Y0 = FMath::Min(D.FrontY, D.BackY);
		const double Y1 = FMath::Max(D.FrontY, D.BackY);
		const double Depth = FMath::Max(Y1 - Y0, 1.0);

		const double FrameT = FMath::Clamp(D.FrameThickness, 1.0, FMath::Max(Depth, 1.0));
		const double Thresh = D.BottomZ + FMath::Clamp(D.ThresholdHeight, 0.0, 40.0);

		// The head is lifted to whatever the character needs before anything is built and the jambs follow it up.
		double LeafTop = FMath::Clamp(D.LeafTopZ, D.BottomZ + 10.0,
			FMath::Max(D.BottomZ + 20.0, D.JambTopZ));
		LeafTop = FMath::Max(LeafTop, Thresh + FMath::Max(D.MinClearHeight, 0.0));
		const double JambTop = FMath::Max(D.JambTopZ, LeafTop + FrameT);

		// Jambs either side of the opening, and the head across the top of the leaves.
		AppendBox(Mesh, FVector3d(X0 - FrameT, Y0, D.BottomZ), FVector3d(X0, Y1, JambTop));
		AppendBox(Mesh, FVector3d(X1, Y0, D.BottomZ), FVector3d(X1 + FrameT, Y1, JambTop));
		AppendBox(Mesh, FVector3d(X0, Y0, LeafTop), FVector3d(X1, Y1, LeafTop + FrameT));

		// 門檻: stepped over, not walked through.
		if (Thresh > D.BottomZ)
		{
			AppendBox(Mesh, FVector3d(X0, Y0, D.BottomZ), FVector3d(X1, Y1, Thresh));
		}

		// 門簪 pin the head, so they are centred on the member.
		if (D.PegCount > 0)
		{
			const double PegR = FMath::Max(0.55 * FrameT, 4.0);
			const double PegZ = LeafTop + 0.5 * FrameT;
			// Starts at mid-depth so the inner end stays buried and only the face shows.
			const double PegLen = 0.5 * Depth + 2.0 * PegR;
			const FQuat LayDown = FQuat::FindBetweenNormals(FVector::UpVector, FVector(0.0, -1.0, 0.0));

			const int32 Pegs = FMath::Clamp(D.PegCount, 0, 6);
			for (int32 i = 1; i <= Pegs; ++i)
			{
				const double PX = X0 + DoorW * i / double(Pegs + 1);
				const int32 PegFirstVert = Mesh.MaxVertexID();
				AppendCylinder(Mesh, FVector3d::Zero(), PegR, PegLen, 6);
				TransformVerticesFrom(Mesh, PegFirstVert,
					FTransform(LayDown, FVector(PX, Y0 + 0.5 * Depth, PegZ)));
			}
		}

		// Everything from here on is leaf rather than frame.
		if (OutLeafFirstTriangle) *OutLeafFirstTriangle = Mesh.MaxTriangleID();

		if (LeafTop <= Thresh)
		{
			return;
		}

		// Rails and stiles stop just short of the leaf's far face.
		const double LeafThick = FMath::Max(0.25 * Depth, 3.0);
		const double Inset = FMath::Min(1.0, 0.25 * LeafThick);
		const double LeafW = 0.5 * DoorW;

		// The pivot stands inboard of the leaf's edge by the stone's overshoot plus enough of the leaf's own thickness for the swept slab to miss it.
		const double PivotIn = (D.StoneReveal > 0.0) ? D.StoneReveal + 0.6 * LeafThick : 0.0;

		if (D.bUseLeafAngles)
		{
			// Each leaf is built shut and then swung about its own jamb.
			const double LeafY0 = Y0 + 0.3 * Depth;
			const double LeafY1 = FMath::Min(Y1, LeafY0 + LeafThick);
			const double PivotY = 0.5 * (LeafY0 + LeafY1);

			auto AppendHingedLeaf = [&](double HingeX, double Direction, double AngleDeg)
			{
				const int32 LeafFirstVert = Mesh.MaxVertexID();

				const double LX0 = FMath::Min(HingeX, HingeX + Direction * LeafW);
				const double LX1 = FMath::Max(HingeX, HingeX + Direction * LeafW);

				AppendBox(Mesh, FVector3d(LX0, LeafY0, Thresh),
					FVector3d(LX1, LeafY1, LeafTop));

				// Rails proud of the outer face, stopping short of the inner one so nothing ends up coplanar with the leaf's own faces.
				for (int32 j = 1; j <= 2; ++j)
				{
					const double CZ = Thresh + (LeafTop - Thresh) * j / 3.0;
					AppendBox(Mesh,
						FVector3d(LX0 + Inset, LeafY0 - 0.2 * FrameT, CZ - 0.5 * FrameT),
						FVector3d(LX1 - Inset, LeafY1 - Inset,        CZ + 0.5 * FrameT));
				}

				// Negated against Direction so a positive angle swings both leaves the same way.
				YawVerticesFrom(Mesh, LeafFirstVert,
					FVector2d(HingeX + Direction * PivotIn, PivotY), -Direction * AngleDeg);
			};

			AppendHingedLeaf(X0, 1.0, D.LeftLeafAngleDeg);
			AppendHingedLeaf(X1, -1.0, D.RightLeafAngleDeg);
			return;
		}

		if (D.bLeavesOpen)
		{
			// A small standoff keeps the leaf's back face off the wall.
			const double Standoff = 2.0;
			const double LeafBackY = Y0 - Standoff;
			const double LeafFrontY = LeafBackY - LeafThick;

			// The hinge is at the jamb, so the free edge sits SwingClearance away from whatever it has to miss when the leaf lies flat.
			const double Clear = FMath::Max(D.SwingClearance, 0.0);
			const double SwingDeg = FMath::RadiansToDegrees(
				FMath::Acos(FMath::Clamp(Clear / LeafW, 0.0, 1.0)));

			auto AppendLeaf = [&](double HingeX, double Direction)
			{
				// Built lying flat, then swung about the hinge — boxes are axis-aligned, so the rotation has to come afterwards.
				const int32 LeafFirstVert = Mesh.MaxVertexID();

				const double LX0 = FMath::Min(HingeX, HingeX + Direction * LeafW);
				const double LX1 = FMath::Max(HingeX, HingeX + Direction * LeafW);

				AppendBox(Mesh, FVector3d(LX0, LeafFrontY, Thresh),
					FVector3d(LX1, LeafBackY, LeafTop));

				for (int32 j = 1; j <= 2; ++j)
				{
					const double CZ = Thresh + (LeafTop - Thresh) * j / 3.0;
					AppendBox(Mesh,
						FVector3d(LX0 + Inset, LeafFrontY - 0.2 * FrameT, CZ - 0.5 * FrameT),
						FVector3d(LX1 - Inset, LeafBackY - Inset,         CZ + 0.5 * FrameT));
				}

				// Negated against Direction so both leaves swing outward rather than one of them back through the doorway.
				YawVerticesFrom(Mesh, LeafFirstVert,
					FVector2d(HingeX, LeafBackY), -Direction * SwingDeg);
			};

			AppendLeaf(X0, -1.0);
			AppendLeaf(X1, 1.0);
			return;
		}

		// Shut: recessed behind the frame plane so the jambs read as a surround rather than as more of the same slab.
		const double LeafY0 = Y0 + 0.3 * Depth;
		const double LeafY1 = FMath::Min(Y1, LeafY0 + LeafThick);

		AppendBox(Mesh, FVector3d(X0, LeafY0, Thresh), FVector3d(X1, LeafY1, LeafTop));

		// Centre stile and two rails, each proud of the leaf by a different amount so that nothing ends up flush where they cross.
		const double MidX = 0.5 * (X0 + X1);
		AppendBox(Mesh,
			FVector3d(MidX - 0.5 * FrameT, LeafY0 - 0.4 * FrameT, Thresh),
			FVector3d(MidX + 0.5 * FrameT, LeafY1 - Inset,        LeafTop));

		for (int32 j = 1; j <= 2; ++j)
		{
			const double CZ = Thresh + (LeafTop - Thresh) * j / 3.0;
			AppendBox(Mesh,
				FVector3d(X0, LeafY0 - 0.2 * FrameT, CZ - 0.5 * FrameT),
				FVector3d(X1, LeafY1 - Inset,        CZ + 0.5 * FrameT));
		}
	}
}
