#include "Generation/CityWallGenerator.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Tools/HutongWallChain.h"
#include "DynamicMeshEditor.h"

using UE::Geometry::FDynamicMesh3;

namespace
{
	// The walk is the body's top face, retagged: a slab on it would z-fight.
	// Pre-bake winding: GetTriNormal points inward, so up is Z ≤ -0.9.
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

	// Brick facing into the corner's city (+X or +Y), in a range. Pre-bake normals point inward.
	void CityWallTagFacingIn(FDynamicMesh3& Mesh, int32 FirstTri, int32 EndTri, int32 Slot)
	{
		HutongMeshUtils::FSlotScope::FlushCurrent(Mesh);
		UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs = Mesh.Attributes()->GetMaterialID();
		for (int32 T = FirstTri; T < EndTri; ++T)
		{
			if (!Mesh.IsTriangle(T) || MatIDs->GetValue(T) != HutongGen::MatSlot_CityBrick) continue;
			const FVector3d N = -Mesh.GetTriNormal(T);
			if (N.X > 0.5 || N.Y > 0.5) MatIDs->SetValue(T, Slot);
		}
	}

	// Brick facing +Y beyond the wall's centre line: the inner face, 宇牆, ramp and its gate. Pre-bake normals point inward.
	void CityWallTagInnerFace(FDynamicMesh3& Mesh, double CentreY, int32 Slot)
	{
		HutongMeshUtils::FSlotScope::FlushCurrent(Mesh);
		UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs = Mesh.Attributes()->GetMaterialID();
		for (int32 T : Mesh.TriangleIndicesItr())
		{
			if (MatIDs->GetValue(T) != HutongGen::MatSlot_CityBrick || Mesh.GetTriNormal(T).Y > -0.5) continue;
			if (Mesh.GetTriCentroid(T).Y > CentreY) MatIDs->SetValue(T, Slot);
		}
	}

	// One straight run of 垛口牆 from A0 to A1 along X (or Y), C0..C1 across, standing on the walk.
	void AppendCityBattlement(FDynamicMesh3& Mesh, const FHutongCityWallParams& P, bool bAlongY,
		double A0, double A1, double C0, double C1, bool bLoopholes)
	{
		using namespace HutongMeshUtils;
		const double H = P.GetHeight();
		const double Top = H + P.GetParapetHeight();
		const double Sill = Top - P.GetCrenelDepth();
		auto Box = [&](double X0, double X1, double Z0, double Z1)
		{
			if (bAlongY) AppendBox(Mesh, FVector3d(C0, X0, Z0), FVector3d(C1, X1, Z1));
			else AppendBox(Mesh, FVector3d(X0, C0, Z0), FVector3d(X1, C1, Z1));
		};
		const double Len = A1 - A0;
		if (Len < 10.0) return;
		const int32 N = P.GetMerlonCount(Len);
		const double Cw = N > 1 ? FMath::Min(P.GetCrenelWidth(), 0.5 * Len / (N - 1)) : 0.0;
		const double Mw = (Len - (N - 1) * Cw) / N;
		const double HoleW = FMath::Min(P.GetLoopholeWidth(), 0.5 * Mw);
		const double HoleH = FMath::Min(P.GetLoopholeHeight(), 0.6 * (Sill - H));
		const double HoleZ0 = H + 0.5 * (Sill - H) - 0.5 * HoleH;
		// The top step's underside stands on the merlon's top (opposite normals).
		const double StepH = P.GetMerlonTopHeight();
		const double StepW = FMath::Min(P.GetMerlonTopWidth(), Mw - 10.0);
		const double Body = StepW > 0.0 ? Top - StepH : Top;
		for (int32 i = 0; i < N; ++i)
		{
			const double X0 = A0 + i * (Mw + Cw), X1 = X0 + Mw;
			if (bLoopholes && HoleW > 4.0 && HoleH > 4.0)
			{
				const double HX0 = 0.5 * (X0 + X1) - 0.5 * HoleW, HX1 = HX0 + HoleW;
				Box(X0, HX0, H, Body);
				Box(HX1, X1, H, Body);
				Box(HX0, HX1, H, HoleZ0);
				Box(HX0, HX1, HoleZ0 + HoleH, Body);
			}
			else
			{
				Box(X0, X1, H, Body);
			}
			if (StepW > 0.0 && StepH > 0.0)
			{
				const double Mid = 0.5 * (X0 + X1);
				Box(Mid - 0.5 * StepW, Mid + 0.5 * StepW, Body, Top);
			}
			if (i + 1 < N) Box(X1, X1 + Cw, H, Sill);
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
		auto In = [&](double Z) { return Bat * Z / H; };

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

		// The 馬面's inner face lies on the wall's own face (opposite normals).
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

		double GapX0 = 0.0, GapX1 = 0.0;
		const double RW = FMath::Max(P.RampWidth, 150.0);
		const double Tr = RampParapetThicknessCm;
		FCityWallRamp Ramp;
		const bool bRamp = CityWallRampLayout(P, Ramp);
		const double D = Ramp.Dir;
		const double XT = Ramp.XTop, XLand = Ramp.XLand, XFoot = Ramp.XFoot, XGate = Ramp.XGate;
		const double Hr = P.GetRampRise();
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

			const double Tp = P.GetParapetThickness();
			const double Top = H + P.GetParapetHeight();
			const double Sill = Top - P.GetCrenelDepth();
			auto Battlement = [&](bool bAlongY, double A0, double A1, double C0, double C1)
			{
				AppendCityBattlement(Mesh, P, bAlongY, A0, A1, C0, C1, bLoopholes);
			};

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
				const double Hp = RampParapetHeightCm;
				const double Yp0 = Yo - Tr;
				const FVector3d Rail[8] = {
					{XFoot, Yp0, 2.0}, {XFoot, Yo, 2.0}, {XLand, Yo, Hr}, {XLand, Yp0, Hr},
					{XFoot, Yp0, 2.0 + Hp}, {XFoot, Yo, 2.0 + Hp}, {XLand, Yo, Hr + Hp}, {XLand, Yp0, Hr + Hp} };
				AppendHexahedron(Mesh, Rail);
				AppendBox(Mesh, FVector3d(FMath::Min(XLand, XT), Yp0, Hr), FVector3d(FMath::Max(XLand, XT), Yo, Hr + Hp));
				AppendBox(Mesh, FVector3d(FMath::Min(XT, XT - D * Tr), B - Bat - P.GetInnerParapetThickness(), Hr),
					FVector3d(FMath::Max(XT, XT - D * Tr), Yp0, Hr + Hp));

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

		if (P.HasRedInnerFace()) CityWallTagInnerFace(Mesh, 0.5 * B, MatSlot_RedWall);
	}

	void BuildWallCorner(FDynamicMesh3& Mesh, const FHutongCityWallParams& P, double PlatformSize, bool bBattlements, bool bLoopholes, bool bOuterParapet)
	{
		using namespace HutongMeshUtils;
		const double H = P.GetHeight();
		const double B = P.GetBaseWidth();
		const double Bat = P.GetBatter();
		const double S = FMath::Max(PlatformSize, B + 2.0 * Bat + 10.0);
		const double Foot = bBattlements ? P.GetFootingHeight() : 0.0;
		auto In = [&](double Z) { return Bat * Z / H; };
		// The plan at height Z: outer faces on X = 0 and Y = 0, ports at X = S and Y = S as wide as a leg, the platform's
		// inner faces battered; a leg's inner face runs on into the re-entrant corner (a point at the foot).
		auto Ring = [&](double Z)
		{
			const double I = In(Z);
			return TArray<FVector3d>{ {I, I, Z}, {S, I, Z}, {S, B - I, Z}, {S - I, B - I, Z}, {S - I, S - I, Z},
				{B - I, S - I, Z}, {B - I, S, Z}, {I, S, Z} };
		};
		if (Foot > 1.0)
		{
			FSlotScope Tag(Mesh, MatSlot_Stone);
			AppendLoft(Mesh, Ring(0.0), Ring(Foot));
		}
		const int32 BodyFirst = Mesh.MaxTriangleID();
		{
			FSlotScope Tag(Mesh, MatSlot_CityBrick);
			AppendLoft(Mesh, Ring(Foot), Ring(H));
			Tag.Close();
			CityWallTagWalk(Mesh, BodyFirst, Mesh.MaxTriangleID(), MatSlot_Floor);
		}
		const int32 BodyEnd = Mesh.MaxTriangleID();
		if (!bBattlements)
		{
			if (P.HasRedInnerFace()) CityWallTagFacingIn(Mesh, BodyFirst, BodyEnd, MatSlot_RedWall);
			return;
		}

		FSlotScope Tag(Mesh, MatSlot_CityBrick);
		const double Tp = P.GetParapetThickness();
		if (bOuterParapet)
		{
			AppendCityBattlement(Mesh, P, false, Bat, S, Bat, Bat + Tp, bLoopholes);
			AppendCityBattlement(Mesh, P, true, Bat + Tp, S, Bat, Bat + Tp, bLoopholes);
		}

		int32 ParapetFirst = Mesh.MaxTriangleID();
		if (P.bInnerParapet)
		{
			// Round the re-entrant edge of the walk, a thickness inside it.
			const double Ti = P.GetInnerParapetThickness(), Z1 = H + P.GetInnerParapetHeight();
			const double E = S - Bat, F = B - Bat;
			AppendBox(Mesh, FVector3d(E, F - Ti, H), FVector3d(S, F, Z1));
			AppendBox(Mesh, FVector3d(E - Ti, F - Ti, H), FVector3d(E, E, Z1));
			AppendBox(Mesh, FVector3d(F, E - Ti, H), FVector3d(E - Ti, E, Z1));
			AppendBox(Mesh, FVector3d(F - Ti, E - Ti, H), FVector3d(F, S, Z1));
		}
		const int32 ParapetEnd = Mesh.MaxTriangleID();
		Tag.Close();
		if (P.HasRedInnerFace())
		{
			CityWallTagFacingIn(Mesh, BodyFirst, BodyEnd, MatSlot_RedWall);
			CityWallTagFacingIn(Mesh, ParapetFirst, ParapetEnd, MatSlot_RedWall);
		}
	}

	void BuildRoundedCorner(FDynamicMesh3& Mesh, const FHutongCityWallParams& P, double Size, bool bBattlements, bool bLoopholes)
	{
		const double B = P.GetBaseWidth();
		const double S = FMath::Max(Size, B + 10.0);
		const double R = S - 0.5 * B;
		// About 4 m a leg, so a fitted merlon or two each; from the Y port round to the X port, the outside on the right.
		const int32 Legs = FMath::Clamp(FMath::CeilToInt32(0.5 * UE_DOUBLE_PI * R / 400.0), 3, 24);
		TArray<FVector2D> Points;
		for (int32 i = 0; i <= Legs; ++i)
		{
			const double A = UE_DOUBLE_PI * (1.0 + 0.5 * i / Legs);
			Points.Add(FVector2D(S + R * FMath::Cos(A), S + R * FMath::Sin(A)));
		}
		// The end legs cut on the port planes, where the straight legs abut square.
		HutongWallChain::FEndFace StartFace, EndFace;
		StartFace.bSet = EndFace.bSet = true;
		StartFace.Point = Points[0];
		StartFace.Dir = FVector2D(1.0, 0.0);
		EndFace.Point = Points.Last();
		EndFace.Dir = FVector2D(0.0, 1.0);
		TArray<HutongWallChain::FSegment> Segments;
		if (!HutongWallChain::Build(Points, B, 0.5 * B, StartFace, EndFace, Segments)) return;
		HutongMeshUtils::FSlotScope::FlushCurrent(Mesh);
		Mesh.EnableAttributes();
		Mesh.Attributes()->EnableMaterialID();
		for (const HutongWallChain::FSegment& Seg : Segments)
		{
			FHutongCityWallParams Q = P;
			Q.Length = Seg.Length;
			Q.bBastions = false;
			Q.bRamp = false;
			FDynamicMesh3 Leg;
			Leg.EnableAttributes();
			Leg.Attributes()->EnableMaterialID();
			BuildCityWall(Leg, Q, bBattlements, bLoopholes);
			FHutongFootprintSkew Skew = Seg.Skew;
			Skew.RunAxis = 0;
			if (!Skew.IsZero()) HutongMeshUtils::WarpFootprint(Leg, Seg.Length, B, Skew);
			const FTransform Xf(FRotator(0.0, Seg.YawDeg, 0.0), FVector(Seg.Origin.X, Seg.Origin.Y, 0.0));
			for (const int32 V : Leg.VertexIndicesItr()) Leg.SetVertex(V, FVector3d(Xf.TransformPosition(FVector(Leg.GetVertex(V)))));
			HutongMeshUtils::FSlotScope::FlushCurrent(Leg);
			UE::Geometry::FMeshIndexMappings Mappings;
			UE::Geometry::FDynamicMeshEditor(&Mesh).AppendMesh(&Leg, Mappings);
		}
	}
}
