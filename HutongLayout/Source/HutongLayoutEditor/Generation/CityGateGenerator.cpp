#include "Generation/CityGateGenerator.h"
#include "Generation/ArrowTowerGenerator.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongFrame.h"
#include "Generation/HutongJiajia.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"

using UE::Geometry::FDynamicMesh3;

namespace
{
	// Pre-bake normals point inward: up is Z <= -0.9.
	void CityGateTagUp(FDynamicMesh3& Mesh, int32 FirstTri, int32 EndTri, int32 Slot)
	{
		HutongMeshUtils::FSlotScope::FlushCurrent(Mesh);
		UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs = Mesh.Attributes()->GetMaterialID();
		for (int32 T = FirstTri; T < EndTri; ++T)
		{
			if (Mesh.IsTriangle(T) && Mesh.GetTriNormal(T).Z <= -0.9) MatIDs->SetValue(T, Slot);
		}
	}
}

namespace HutongGen
{
	void AppendGateTower(FDynamicMesh3& Mesh, const FHutongCityGateParams& P, double Cx, double Cy, double Z, bool bDetail)
	{
		using namespace HutongMeshUtils;
		using namespace HutongCanon::CityGate;
		const double W = P.GateTowerWidth, Dt = P.GateTowerDepth;
		const double X0 = Cx - 0.5 * W, X1 = Cx + 0.5 * W, Y0 = Cy - 0.5 * Dt, Y1 = Cy + 0.5 * Dt;
		const double ColR = 0.5 * ColumnDiameterCm;

		{
			FSlotScope Tag(Mesh, MatSlot_Stone);
			AppendBox(Mesh, FVector3d(X0 - 2.0 * ColR, Y0 - 2.0 * ColR, Z), FVector3d(X1 + 2.0 * ColR, Y1 + 2.0 * ColR, Z + PlinthCm));
		}
		const double Floor = Z + PlinthCm, Eave1 = Floor + LowerColumnCm;

		// The hall inside its 周圍廊, red-walled; a column at every bay line round the column line.
		{
			FSlotScope Tag(Mesh, MatSlot_RedWall);
			AppendBox(Mesh, FVector3d(X0 + VerandaCm, Y0 + VerandaCm, Floor), FVector3d(X1 - VerandaCm, Y1 - VerandaCm, Eave1));
		}
		auto Columns = [&](double A0, double A1, double B0, double B1, int32 Bays, int32 SideBays, double Bottom, double Top)
		{
			FSlotScope Tag(Mesh, MatSlot_Wood);
			for (int32 i = 0; i <= Bays; ++i)
			{
				const double X = A0 + (A1 - A0) * i / Bays;
				Frame::AppendColumn(Mesh, X, B0, ColR, ColR, Bottom, Top, 1.0, bDetail ? 12 : 6);
				Frame::AppendColumn(Mesh, X, B1, ColR, ColR, Bottom, Top, 1.0, bDetail ? 12 : 6);
			}
			for (int32 j = 1; j < SideBays; ++j)
			{
				const double Y = B0 + (B1 - B0) * j / SideBays;
				Frame::AppendColumn(Mesh, A0, Y, ColR, ColR, Bottom, Top, 1.0, bDetail ? 12 : 6);
				Frame::AppendColumn(Mesh, A1, Y, ColR, ColR, Bottom, Top, 1.0, bDetail ? 12 : 6);
			}
			Tag.Close();
			// 額枋 round the column line, buried in the columns and the eave above.
			FSlotScope Paint(Mesh, MatSlot_Paint);
			const double S = ColumnDiameterCm, Zb = Top - S;
			AppendBox(Mesh, FVector3d(A0, B0 - 0.4 * S, Zb), FVector3d(A1, B0 + 0.4 * S, Top + 1.0));
			AppendBox(Mesh, FVector3d(A0, B1 - 0.4 * S, Zb), FVector3d(A1, B1 + 0.4 * S, Top + 1.0));
			AppendBox(Mesh, FVector3d(A0 - 0.4 * S, B0 + 0.4 * S, Zb), FVector3d(A0 + 0.4 * S, B1 - 0.4 * S, Top + 1.0));
			AppendBox(Mesh, FVector3d(A1 - 0.4 * S, B0 + 0.4 * S, Zb), FVector3d(A1 + 0.4 * S, B1 - 0.4 * S, Top + 1.0));
		};
		Columns(X0, X1, Y0, Y1, P.Tower == EHutongCityGateTower::Chenglou ? GateTowerBays : 3, GateTowerSideBays, Floor, Eave1);

		// A hipped skirt whose top stands inside the storey above it.
		auto Skirt = [&](double A0, double A1, double B0, double B1, double EaveZ, double Overhang, double MaxRise)
		{
			FSlotScope Tag(Mesh, MatSlot_Roof);
			FHipRoofSpec Hip;
			Hip.Width = (A1 - A0) + 2.0 * Overhang;
			Hip.Depth = (B1 - B0) + 2.0 * Overhang;
			Hip.RidgeLength = FMath::Max(Hip.Width - Hip.Depth, 0.0);
			Hip.Section = Jiajia::MakeSection(EHutongPurlins::Seven, 0.5 * (B1 - B0), Overhang, 0.0);
			Hip.Rise = FMath::Min(Hip.Section.Rise(), MaxRise);
			Hip.EaveOverhang = Overhang;
			Hip.bEaveCaps = true;
			Hip.FlareRise = 45.0;
			Hip.FlareRun = 35.0;
			Hip.FlareLength = 0.3 * FMath::Min(Hip.Width, Hip.Depth);
			Hip.FasciaDrop = 22.0;
			Hip.TileRowSpacing = 22.0;
			AppendHippedRoof(Mesh, FVector3d(A0 - Overhang, B0 - Overhang, EaveZ), Hip);
		};

		const double U0x = X0 + UpperInsetCm, U1x = X1 - UpperInsetCm, U0y = Y0 + UpperInsetCm, U1y = Y1 - UpperInsetCm;
		const double Deck = Eave1 + BalconyRiseCm, Eave2 = Deck + UpperWallCm;
		Skirt(X0, X1, Y0, Y1, Eave1, LowerEaveOverhangCm, BalconyRiseCm + UpperWallCm - 60.0);

		// 平座: a deck round the upper hall over the skirt, a railing at its edge.
		{
			FSlotScope Tag(Mesh, MatSlot_Wood);
			const double E = BalconyCm, T = 12.0;
			AppendBox(Mesh, FVector3d(U0x - E, U0y - E, Deck - 20.0), FVector3d(U1x + E, U1y + E, Deck));
			const double Rz = Deck + RailingCm;
			AppendBox(Mesh, FVector3d(U0x - E, U0y - E, Deck), FVector3d(U1x + E, U0y - E + T, Rz));
			AppendBox(Mesh, FVector3d(U0x - E, U1y + E - T, Deck), FVector3d(U1x + E, U1y + E, Rz));
			AppendBox(Mesh, FVector3d(U0x - E, U0y - E + T, Deck), FVector3d(U0x - E + T, U1y + E - T, Rz));
			AppendBox(Mesh, FVector3d(U1x + E - T, U0y - E + T, Deck), FVector3d(U1x + E, U1y + E - T, Rz));
		}

		// The upper hall: red walls behind its own columns.
		{
			FSlotScope Tag(Mesh, MatSlot_RedWall);
			AppendBox(Mesh, FVector3d(U0x + ColR, U0y + ColR, Deck - 20.0), FVector3d(U1x - ColR, U1y - ColR, Eave2));
		}
		Columns(U0x, U1x, U0y, U1y, P.Tower == EHutongCityGateTower::Chenglou ? GateTowerBays : 3, GateTowerSideBays, Deck, Eave2);

		// 重檐: the lower eave over the upper hall, a short wall band, then the 歇山.
		const double C0x = U0x + ClerestoryInsetCm, C1x = U1x - ClerestoryInsetCm, C0y = U0y + ClerestoryInsetCm, C1y = U1y - ClerestoryInsetCm;
		const double Eave3 = Eave2 + ClerestoryCm;
		Skirt(U0x, U1x, U0y, U1y, Eave2, MiddleEaveOverhangCm, ClerestoryCm - 30.0);
		{
			FSlotScope Tag(Mesh, MatSlot_Paint);
			AppendBox(Mesh, FVector3d(C0x, C0y, Eave2 - 10.0), FVector3d(C1x, C1y, Eave3));
		}
		AppendTowerXieshanRoof(Mesh, C0x, C0y, C1x - C0x, C1y - C0y, Eave3, TopEaveOverhangCm);
	}

	void BuildCityGate(FDynamicMesh3& Mesh, const FHutongCityGateParams& P, bool bDetail)
	{
		using namespace HutongMeshUtils;
		using namespace HutongCanon::CityGate;
		const FHutongCityWallParams Wall = P.GetWall();
		const double L = P.GetLength(), H = Wall.GetHeight(), B = Wall.GetBaseWidth(), Bat = Wall.GetBatter();
		const double D = P.GetDepth();
		const double Foot = bDetail ? Wall.GetFootingHeight() : 0.0;
		const double Pw = P.GetPassageWidth(), Crown = P.GetPassageHeight();
		const double Rad = 0.5 * Pw, Spring = Crown - Rad, RingTop = Crown + ArchRingCm;
		const double Xa = 0.5 * (L - Pw), Xb = Xa + Pw, Xm = 0.5 * L;
		auto In = [&](double Z) { return Bat * Z / H; };
		// Plans at height Z, battered front and back and, past the legs, at the ends; a leg abuts each end between
		// 0 and B, its inner face running on into the extension (a point at the foot).
		auto LeftPier = [&](double Z)
		{
			const double I = In(Z);
			return TArray<FVector3d>{ {0.0, I, Z}, {Xa, I, Z}, {Xa, D - I, Z}, {I, D - I, Z}, {I, B - I, Z}, {0.0, B - I, Z} };
		};
		auto RightPier = [&](double Z)
		{
			const double I = In(Z);
			return TArray<FVector3d>{ {Xb, I, Z}, {L, I, Z}, {L, B - I, Z}, {L - I, B - I, Z}, {L - I, D - I, Z}, {Xb, D - I, Z} };
		};
		auto Whole = [&](double Z)
		{
			const double I = In(Z);
			return TArray<FVector3d>{ {0.0, I, Z}, {L, I, Z}, {L, B - I, Z}, {L - I, B - I, Z}, {L - I, D - I, Z},
				{I, D - I, Z}, {I, B - I, Z}, {0.0, B - I, Z} };
		};
		auto Between = [&](const TArray<FVector3d>& R, double X0, double X1)
		{
			TArray<FVector3d> Out;
			for (const FVector3d& V : R) Out.Add({ FMath::Clamp(V.X, X0, X1), V.Y, V.Z });
			return Out;
		};

		// Piers either side of the passage, the stone footing under them.
		if (Foot > 1.0)
		{
			FSlotScope Tag(Mesh, MatSlot_Stone);
			AppendLoft(Mesh, LeftPier(0.0), LeftPier(Foot));
			AppendLoft(Mesh, RightPier(0.0), RightPier(Foot));
		}
		const int32 BodyFirst = Mesh.MaxTriangleID();
		{
			FSlotScope Tag(Mesh, MatSlot_CityBrick);
			AppendLoft(Mesh, LeftPier(Foot), LeftPier(H));
			AppendLoft(Mesh, RightPier(Foot), RightPier(H));
			AppendLoft(Mesh, Between(Whole(RingTop), Xa, Xb), Between(Whole(H), Xa, Xb));

			// The arch and its spandrels, through the wall at the ring's top: the portal stands in a shallow recess.
			const int32 Steps = bDetail ? 16 : 6;
			TArray<FVector2d> Profile = { { Xb, Spring }, { Xb, RingTop }, { Xa, RingTop }, { Xa, Spring } };
			for (int32 i = 1; i < Steps; ++i)
			{
				const double A = UE_DOUBLE_PI * (1.0 - double(i) / Steps);
				Profile.Add({ Xm + Rad * FMath::Cos(A), Spring + Rad * FMath::Sin(A) });
			}
			const double Y0 = In(RingTop), Y1 = D - In(RingTop);
			const FVector X(1.0, 0.0, 0.0), Up(0.0, 0.0, 1.0), Back(0.0, -1.0, 0.0);
			AppendSweptProfile(Mesh, Profile, { FTransform(X, Up, Back, FVector(0.0, Y1, 0.0)), FTransform(X, Up, Back, FVector(0.0, Y0, 0.0)) });
			Tag.Close();
			CityGateTagUp(Mesh, BodyFirst, Mesh.MaxTriangleID(), MatSlot_Floor);
		}
		if (Wall.HasRedInnerFace())
		{
			FSlotScope::FlushCurrent(Mesh);
			UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs = Mesh.Attributes()->GetMaterialID();
			for (int32 T = BodyFirst; T < Mesh.MaxTriangleID(); ++T)
			{
				if (Mesh.IsTriangle(T) && MatIDs->GetValue(T) == MatSlot_CityBrick && Mesh.GetTriNormal(T).Y < -0.5) MatIDs->SetValue(T, int32(MatSlot_RedWall));
			}
		}

		if (P.Tower == EHutongCityGateTower::Zhalou)
		{
			// Its front over the wall's outer face at the walk, a little in from the gate's ends.
			const double Tw = FMath::Min(P.TowerWidth, L - 2.0 * Bat), Td = FMath::Min(P.TowerDepth, B - 2.0 * Bat);
			const double X0 = 0.5 * (L - Tw), Y0 = Bat + 20.0, Ht = FMath::Max(P.TowerHeight, 150.0);
			if (Y0 + Td > D - Bat) return;
			TArray<double> RowZ;
			for (int32 r = 0; r < P.Rows; ++r) RowZ.Add(H + Ht * (r + 0.6) / (P.Rows + 0.4));
			AppendArrowWindowStorey(Mesh, X0, X0 + Tw, Y0, Y0 + Td, H, H + Ht, P.FrontWindows, TowerSideWindows, TowerSideWindows, 0, RowZ,
				TowerWindowCm, HutongCanon::ArrowTower::WindowRecessCm, bDetail);
			AppendTowerXieshanRoof(Mesh, X0, Y0, Tw, Td, H + Ht, TowerOverhangCm);
		}
		else if (P.Tower == EHutongCityGateTower::Chenglou)
		{
			AppendGateTower(Mesh, P, 0.5 * L, 0.5 * D, H, bDetail);
		}
	}
}
