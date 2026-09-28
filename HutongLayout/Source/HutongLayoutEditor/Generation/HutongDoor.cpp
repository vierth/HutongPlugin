#include "Generation/HutongDoor.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
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
		if (W <= 0.0 || Y1 <= Y0 || Height <= 0.0)
		{
			return;
		}

		// Drum overhangs its plinth along both face directions.
		double R = 0.5 * FMath::Clamp(DrumFraction, 0.2, 0.9) * Height;
		R = FMath::Min(R, 0.42 * Height);

		// Drum top = stone top; plinth rises past the drum's underside.
		const double DrumCz = Height - R;
		const double PlinthH = FMath::Max(Height - 1.75 * R, 0.1 * Height);

		// A deep drum grows forward only; centred, it poked through the 餘塞板 behind.
		const double DrumY = FMath::Min(0.5 * (Y0 + Y1), Y1 - R);
		const double FrontY = FMath::Min(Y0, DrumY - R);

		AppendBox(Mesh, FVector3d(X0, FrontY, BaseZ), FVector3d(X1, Y1, BaseZ + PlinthH));

		// Inset in X, not flush with the plinth.
		const double Inset = 0.06 * W;
		const FQuat LayAcross = FQuat::FindBetweenNormals(FVector::UpVector, FVector::XAxisVector);
		const FVector Centre(0.0, DrumY, BaseZ + DrumCz);

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
		// Clamped to the caller's room.
		const double W = FMath::Clamp(FMath::Max(JambT * 2.0, 26.0), 4.0, FMath::Max(MaxOutward, 4.0));
		const double Reveal = DoorStoneReveal(JambT, DoorW);
		const double Proj = FMath::Max(Stones.Projection, MinProjection);

		// 門枕石 = a small 枕 under the leaf (pivot socket) + the visible 門墩 wholly in front of the doors.
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

	double LeafClearWidth(const FHutongDoorAssembly& D)
	{
		// The same figures AppendDoorAssembly lays its hinged leaves out with.
		const double X0 = FMath::Min(D.OpeningX0, D.OpeningX1);
		const double X1 = FMath::Max(D.OpeningX0, D.OpeningX1);
		const double DoorW = X1 - X0;
		const double Y0 = FMath::Min(D.FrontY, D.BackY);
		const double Y1 = FMath::Max(D.FrontY, D.BackY);
		const double Depth = FMath::Max(Y1 - Y0, 1.0);
		const double FrameT = FMath::Clamp(D.FrameThickness, 1.0, Depth);
		const double Reveal = FMath::Max(D.StoneReveal, 0.0);
		if (!D.bUseLeafAngles) return DoorW - 2.0 * Reveal;

		const double LeafThick = FMath::Max(0.25 * Depth, 3.0);
		const double LeafW = 0.5 * DoorW;
		const double PivotIn = (Reveal > 0.0) ? Reveal + 0.6 * LeafThick : 0.0;
		const double LeafY0 = Y0 + 0.3 * Depth;
		const double LeafY1 = FMath::Min(Y1, LeafY0 + LeafThick);
		const double PivotY = 0.5 * (LeafY0 + LeafY1);

		// How far into the opening a leaf's footprint (rails included) reaches, swung as YawVerticesFrom swings it.
		auto Reach = [&](double HingeX, double Direction, double AngleDeg)
		{
			const double Rad = FMath::DegreesToRadians(-Direction * AngleDeg);
			const double C = FMath::Cos(Rad), S = FMath::Sin(Rad);
			const double PX = HingeX + Direction * PivotIn;
			double Most = HingeX;
			for (const double X : { HingeX, HingeX + Direction * LeafW })
			{
				for (const double Y : { LeafY0 - 0.2 * FrameT, LeafY1 })
				{
					const double RX = PX + (X - PX) * C - (Y - PivotY) * S;
					Most = (Direction > 0.0) ? FMath::Max(Most, RX) : FMath::Min(Most, RX);
				}
			}
			return Most;
		};
		const double Left = FMath::Max(X0 + Reveal, Reach(X0, 1.0, D.LeftLeafAngleDeg));
		const double Right = FMath::Min(X1 - Reveal, Reach(X1, -1.0, D.RightLeafAngleDeg));
		return Right - Left;
	}

	void AppendDoorAssembly(FDynamicMesh3& Mesh, const FHutongDoorAssembly& D)
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

		// Lift the head to clear height first; jambs follow.
		double LeafTop = FMath::Clamp(D.LeafTopZ, D.BottomZ + 10.0,
			FMath::Max(D.BottomZ + 20.0, D.JambTopZ));
		LeafTop = FMath::Max(LeafTop, Thresh + FMath::Max(D.MinClearHeight, 0.0));
		const double JambTop = FMath::Max(D.JambTopZ, LeafTop + FrameT);

		AppendBox(Mesh, FVector3d(X0 - FrameT, Y0, D.BottomZ), FVector3d(X0, Y1, JambTop));
		AppendBox(Mesh, FVector3d(X1, Y0, D.BottomZ), FVector3d(X1 + FrameT, Y1, JambTop));
		AppendBox(Mesh, FVector3d(X0, Y0, LeafTop), FVector3d(X1, Y1, LeafTop + FrameT));

		// 門檻: stepped over, not walked through.
		if (Thresh > D.BottomZ)
		{
			AppendBox(Mesh, FVector3d(X0, Y0, D.BottomZ), FVector3d(X1, Y1, Thresh));
		}

		// 門簪 centred on the head.
		if (D.PegCount > 0)
		{
			// 表十三: a ninth of the 門口 across (no deeper than the head it goes through), a seventh long.
			namespace J = HutongCanon::Joinery;
			const double PegR = FMath::Max(FMath::Min(0.5 * J::PegDiameterOfDoorway * DoorW, 0.55 * FrameT), 4.0);
			const double PegZ = LeafTop + 0.5 * FrameT;
			// Starts at mid-depth so the inner end stays buried.
			const double PegLen = 0.5 * Depth + FMath::Max(J::PegLengthOfDoorway * DoorW, 2.0 * PegR);
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

		// Leaves from here, in 門漆 whatever the frame is.
		FSlotScope LeafTag(Mesh, HutongGen::MatSlot_DoorPaint);

		if (LeafTop <= Thresh)
		{
			return;
		}

		// Rails and stiles stop short of the leaf's far face (no coplanar faces).
		const double LeafThick = FMath::Max(0.25 * Depth, 3.0);
		const double Inset = FMath::Min(1.0, 0.25 * LeafThick);
		const double LeafW = 0.5 * DoorW;

		// Pivot inboard of the leaf edge by the stone reveal plus enough for the swinging slab to miss the stone.
		const double PivotIn = (D.StoneReveal > 0.0) ? D.StoneReveal + 0.6 * LeafThick : 0.0;

		if (D.bUseLeafAngles)
		{
			// Build shut, then swing about its jamb.
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

				// Rails proud of the outer face, short of the inner: no coplanar faces.
				for (int32 j = 1; j <= 2; ++j)
				{
					const double CZ = Thresh + (LeafTop - Thresh) * j / 3.0;
					AppendBox(Mesh,
						FVector3d(LX0 + Inset, LeafY0 - 0.2 * FrameT, CZ - 0.5 * FrameT),
						FVector3d(LX1 - Inset, LeafY1 - Inset,        CZ + 0.5 * FrameT));
				}

				// Negated by Direction so a positive angle swings both leaves the same way.
				YawVerticesFrom(Mesh, LeafFirstVert,
					FVector2d(HingeX + Direction * PivotIn, PivotY), -Direction * AngleDeg);
			};

			AppendHingedLeaf(X0, 1.0, D.LeftLeafAngleDeg);
			AppendHingedLeaf(X1, -1.0, D.RightLeafAngleDeg);
			return;
		}

		if (D.bLeavesOpen)
		{
			// Standoff keeps the leaf's back off the wall.
			const double Standoff = 2.0;
			const double LeafBackY = Y0 - Standoff;
			const double LeafFrontY = LeafBackY - LeafThick;

			// Hinge at the jamb; flat leaf's free edge limited by SwingClearance.
			const double Clear = FMath::Max(D.SwingClearance, 0.0);
			const double SwingDeg = FMath::RadiansToDegrees(
				FMath::Acos(FMath::Clamp(Clear / LeafW, 0.0, 1.0)));

			auto AppendLeaf = [&](double HingeX, double Direction)
			{
				// Boxes are axis-aligned: build, then yaw about the hinge.
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

				// Negated by Direction so both leaves swing outward.
				YawVerticesFrom(Mesh, LeafFirstVert,
					FVector2d(HingeX, LeafBackY), -Direction * SwingDeg);
			};

			AppendLeaf(X0, -1.0);
			AppendLeaf(X1, 1.0);
			return;
		}

		// Shut: recessed behind the frame plane so the jambs read as a surround.
		const double LeafY0 = Y0 + 0.3 * Depth;
		const double LeafY1 = FMath::Min(Y1, LeafY0 + LeafThick);

		AppendBox(Mesh, FVector3d(X0, LeafY0, Thresh), FVector3d(X1, LeafY1, LeafTop));

		// Centre stile and rails at different projections: nothing flush where they cross.
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
