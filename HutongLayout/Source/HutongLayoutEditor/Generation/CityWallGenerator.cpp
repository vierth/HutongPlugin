#include "Generation/CityWallGenerator.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"

using UE::Geometry::FDynamicMesh3;

namespace
{
	// The walk is the body's top face: retagged rather than laid as a slab on it, which would share
	// that face and z-fight. Pre-bake winding: GetTriNormal points inward, so up is Z ≤ -0.9.
	void CityWallTagWalk(FDynamicMesh3& Mesh, int32 FirstTri, int32 EndTri, int32 Slot)
	{
		HutongMeshUtils::FSlotScope::FlushCurrent(Mesh);
		Mesh.EnableAttributes();
		Mesh.Attributes()->EnableMaterialID();
		UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs = Mesh.Attributes()->GetMaterialID();
		for (int32 T = FirstTri; T < EndTri; ++T)
		{
			if (Mesh.IsTriangle(T) && Mesh.GetTriNormal(T).Z <= -0.9) MatIDs->SetValue(T, Slot);
		}
	}
}

namespace HutongGen
{
	bool CityWallRampLayout(const FHutongCityWallParams& P, FCityWallRamp& Out)
	{
		using namespace HutongCanon::CityWall;
		const double L = FMath::Max(P.Length, 10.0);
		const double Land = FMath::Max(FMath::Max(P.RampWidth, 150.0), 300.0);
		const double Run = P.GetRampRun();
		const double Total = RampGateThicknessCm + Run + Land;
		const double Margin = RampParapetThicknessCm + 20.0;
		if (!P.bRamp || L - 2.0 * Margin < Total) return false;
		Out.Dir = P.bRampRisesTowardStart ? -1.0 : 1.0;
		// Top end along the run, held so the whole ramp and its gateway stay on this length of wall.
		const double Lo = Out.Dir > 0.0 ? Total + Margin : Margin;
		const double Hi = Out.Dir > 0.0 ? L - Margin : L - Total - Margin;
		Out.XTop = FMath::Clamp(P.RampPosition * L, Lo, Hi);
		Out.XLand = Out.XTop - Out.Dir * Land;
		Out.XFoot = Out.XLand - Out.Dir * Run;
		Out.XGate = Out.XFoot - Out.Dir * RampGateThicknessCm;
		return true;
	}

	void BuildCityWall(FDynamicMesh3& Mesh, const FHutongCityWallParams& P, bool bBattlements, bool bLoopholes)
	{
		using namespace HutongMeshUtils;
		using namespace HutongCanon::CityWall;

		const double L = FMath::Max(P.Length, 10.0);
		const double H = P.GetHeight();
		const double B = P.GetBaseWidth();
		const double Bat = P.GetBatter();
		const double S = bBattlements ? P.GetFootingHeight() : 0.0;
		const double BatS = Bat * S / H;
		// Each face sets in by Bat over the height.
		auto In = [&](double Z) { return Bat * Z / H; };

		// Footing, then the brick body above it on the same batter: one section split at the stone line.
		if (S > 1.0)
		{
			FSlotScope Tag(Mesh, MatSlot_Stone);
			AppendYZPrism(Mesh, { {0.0, 0.0}, {B, 0.0}, {B - BatS, S}, {BatS, S} }, 0.0, L);
		}
		{
			FSlotScope Tag(Mesh, MatSlot_CityBrick);
			const int32 FirstTri = Mesh.MaxTriangleID();
			AppendYZPrism(Mesh, { {BatS, S}, {B - BatS, S}, {B - Bat, H}, {Bat, H} }, 0.0, L);
			Tag.Close();
			CityWallTagWalk(Mesh, FirstTri, Mesh.MaxTriangleID(), MatSlot_Floor);
		}

		// 馬面 along the outer face, evenly spaced and clear of each end (the mitre's zone). Battered on all three
		// faces; the inner face lies on the wall's own face (opposite normals), the top level with the walk.
		struct FBastion { double X0, X1; };
		TArray<FBastion> Bastions;
		const double Pj = P.GetBastionProjection();
		if (P.bBastions)
		{
			const double BW = FMath::Max(P.GetBastionWidth(), 2.0 * Bat + 2.0 * P.GetParapetThickness() + 100.0);
			const double Usable = L - 2.0 * B - BW;
			if (Usable >= 0.0)
			{
				const double Pitch = FMath::Max(P.BastionSpacing, BW + 100.0);
				const int32 Count = FMath::FloorToInt32(Usable / Pitch) + 1;
				const double First = 0.5 * L - 0.5 * (Count - 1) * Pitch;
				for (int32 i = 0; i < Count; ++i)
				{
					const double C = First + i * Pitch;
					Bastions.Add({ C - 0.5 * BW, C + 0.5 * BW });
				}
			}
		}
		for (const FBastion& Bs : Bastions)
		{
			auto Block = [&](double Z0, double Z1, int32 Slot)
			{
				FVector3d C[8];
				const double Zs[2] = { Z0, Z1 };
				for (int32 k = 0; k < 2; ++k)
				{
					const double Z = Zs[k];
					C[4 * k + 0] = FVector3d(Bs.X0 + In(Z), -Pj + In(Z), Z);
					C[4 * k + 1] = FVector3d(Bs.X1 - In(Z), -Pj + In(Z), Z);
					C[4 * k + 2] = FVector3d(Bs.X1 - In(Z), In(Z), Z);
					C[4 * k + 3] = FVector3d(Bs.X0 + In(Z), In(Z), Z);
				}
				FSlotScope Tag(Mesh, Slot);
				const int32 FirstTri = Mesh.MaxTriangleID();
				AppendHexahedron(Mesh, C);
				Tag.Close();
				if (Slot == MatSlot_CityBrick) CityWallTagWalk(Mesh, FirstTri, Mesh.MaxTriangleID(), MatSlot_Floor);
			};
			if (S > 1.0) Block(0.0, S, MatSlot_Stone);
			Block(S, H, MatSlot_CityBrick);
		}

		// 馬道 against the inner face: incline, a level landing at the top, the 宇牆 opened over the landing.
		double GapX0 = 0.0, GapX1 = 0.0;
		const double RW = FMath::Max(P.RampWidth, 150.0);
		const double Tr = RampParapetThicknessCm;
		FCityWallRamp Ramp;
		const bool bRamp = CityWallRampLayout(P, Ramp);
		const double D = Ramp.Dir;
		const double XT = Ramp.XTop, XLand = Ramp.XLand, XFoot = Ramp.XFoot, XGate = Ramp.XGate;
		const double Hr = P.GetRampRise();
		// The ramp's inner face on the wall's battered face, 5 cm into it.
		auto YIn = [&](double Z) { return B - In(Z) - 5.0; };
		const double Yo = B + RW;
		if (bRamp)
		{
			FSlotScope Tag(Mesh, MatSlot_CityBrick);
			const int32 FirstTri = Mesh.MaxTriangleID();
			const FVector3d Incline[8] = {
				{XFoot, YIn(0.0), 0.0}, {XFoot, Yo, 0.0}, {XLand, Yo, 0.0}, {XLand, YIn(0.0), 0.0},
				{XFoot, YIn(2.0), 2.0}, {XFoot, Yo, 2.0}, {XLand, Yo, Hr}, {XLand, YIn(Hr), Hr} };
			AppendHexahedron(Mesh, Incline);
			AppendYZPrism(Mesh, { {YIn(0.0), 0.0}, {Yo, 0.0}, {Yo, Hr}, {YIn(Hr), Hr} }, FMath::Min(XLand, XT), FMath::Max(XLand, XT));
			Tag.Close();
			CityWallTagWalk(Mesh, FirstTri, Mesh.MaxTriangleID(), MatSlot_Floor);
			GapX0 = FMath::Min(XLand, XT - D * Tr);
			GapX1 = FMath::Max(XLand, XT - D * Tr);
		}

		if (bBattlements)
		{
			FSlotScope Tag(Mesh, MatSlot_CityBrick);

			// 垛口牆 along one straight run (X or Y), Cross0..Cross1 across it: merlons (with a 射眼) and
			// crenels side by side, faces between them opposite, a merlon at each end.
			const double Tp = P.GetParapetThickness();
			const double Top = H + P.GetParapetHeight();
			const double Sill = Top - P.GetCrenelDepth();
			auto Box = [&](bool bAlongY, double A0, double A1, double C0, double C1, double Z0, double Z1)
			{
				if (bAlongY) AppendBox(Mesh, FVector3d(C0, A0, Z0), FVector3d(C1, A1, Z1));
				else AppendBox(Mesh, FVector3d(A0, C0, Z0), FVector3d(A1, C1, Z1));
			};
			auto Battlement = [&](bool bAlongY, double A0, double A1, double C0, double C1)
			{
				const double Len = A1 - A0;
				if (Len < 10.0) return;
				const int32 N = P.GetMerlonCount(Len);
				const double Cw = N > 1 ? FMath::Min(FMath::Max(P.CrenelWidth, 10.0), 0.5 * Len / (N - 1)) : 0.0;
				const double Mw = (Len - (N - 1) * Cw) / N;
				const double Hole = FMath::Min(FMath::Max(P.LoopholeSize, 5.0), 0.5 * FMath::Min(Mw, Sill - H));
				const double HoleZ0 = H + 0.5 * (Sill - H) - 0.5 * Hole;
				for (int32 i = 0; i < N; ++i)
				{
					const double X0 = A0 + i * (Mw + Cw), X1 = X0 + Mw;
					if (bLoopholes && Hole > 4.0)
					{
						const double HX0 = 0.5 * (X0 + X1) - 0.5 * Hole, HX1 = HX0 + Hole;
						Box(bAlongY, X0, HX0, C0, C1, H, Top);
						Box(bAlongY, HX1, X1, C0, C1, H, Top);
						Box(bAlongY, HX0, HX1, C0, C1, H, HoleZ0);
						Box(bAlongY, HX0, HX1, C0, C1, HoleZ0 + Hole, Top);
					}
					else
					{
						Box(bAlongY, X0, X1, C0, C1, H, Top);
					}
					if (i + 1 < N) Box(bAlongY, X1, X1 + Cw, C0, C1, H, Sill);
				}
			};

			// Along the outer edge, broken at each bastion's mouth; round each bastion's three sides.
			double From = 0.0;
			for (const FBastion& Bs : Bastions)
			{
				const double M0 = Bs.X0 + Bat, M1 = Bs.X1 - Bat;
				Battlement(false, From, M0, Bat, Bat + Tp);
				Battlement(false, M0, M1, -Pj + Bat, -Pj + Bat + Tp);
				Battlement(true, -Pj + Bat + Tp, Bat + Tp, M0, M0 + Tp);
				Battlement(true, -Pj + Bat + Tp, Bat + Tp, M1 - Tp, M1);
				From = M1;
			}
			Battlement(false, From, L, Bat, Bat + Tp);

			// 宇牆 on the inner edge, open where the ramp arrives.
			if (P.bInnerParapet)
			{
				const double Y0 = B - Bat - P.GetInnerParapetThickness(), Y1 = B - Bat, Z1 = H + P.GetInnerParapetHeight();
				if (bRamp)
				{
					AppendBox(Mesh, FVector3d(0.0, Y0, H), FVector3d(GapX0, Y1, Z1));
					AppendBox(Mesh, FVector3d(GapX1, Y0, H), FVector3d(L, Y1, Z1));
				}
				else
				{
					AppendBox(Mesh, FVector3d(0.0, Y0, H), FVector3d(L, Y1, Z1));
				}
			}

			if (bRamp)
			{
				// Parapet up the open side, along the landing and across its end to the 宇牆.
				const double Hp = RampParapetHeightCm;
				const double Yp0 = Yo - Tr;
				const FVector3d Rail[8] = {
					{XFoot, Yp0, 2.0}, {XFoot, Yo, 2.0}, {XLand, Yo, Hr}, {XLand, Yp0, Hr},
					{XFoot, Yp0, 2.0 + Hp}, {XFoot, Yo, 2.0 + Hp}, {XLand, Yo, Hr + Hp}, {XLand, Yp0, Hr + Hp} };
				AppendHexahedron(Mesh, Rail);
				AppendBox(Mesh, FVector3d(FMath::Min(XLand, XT), Yp0, Hr), FVector3d(FMath::Max(XLand, XT), Yo, Hr + Hp));
				AppendBox(Mesh, FVector3d(FMath::Min(XT, XT - D * Tr), B - Bat - P.GetInnerParapetThickness(), Hr),
					FVector3d(FMath::Max(XT, XT - D * Tr), Yp0, Hr + Hp));

				// 馬道門: a wall across the foot with a doorway, its inner pier on the battered face.
				const double Hg = RampGateHeightCm;
				const double Cg = 0.5 * (B + Yo);
				const double Ow = FMath::Min(RampGateOpeningWidthCm, RW - 60.0);
				const double Oh = FMath::Min(RampGateOpeningHeightCm, Hg - 60.0);
				const double GX0 = FMath::Min(XGate, XFoot), GX1 = FMath::Max(XGate, XFoot);
				const FVector3d Pier[8] = {
					{GX0, YIn(0.0), 0.0}, {GX1, YIn(0.0), 0.0}, {GX1, Cg - 0.5 * Ow, 0.0}, {GX0, Cg - 0.5 * Ow, 0.0},
					{GX0, YIn(Hg), Hg}, {GX1, YIn(Hg), Hg}, {GX1, Cg - 0.5 * Ow, Hg}, {GX0, Cg - 0.5 * Ow, Hg} };
				AppendHexahedron(Mesh, Pier);
				AppendBox(Mesh, FVector3d(GX0, Cg + 0.5 * Ow, 0.0), FVector3d(GX1, Yo, Hg));
				AppendBox(Mesh, FVector3d(GX0, Cg - 0.5 * Ow, Oh), FVector3d(GX1, Cg + 0.5 * Ow, Hg));
			}
		}
	}
}
