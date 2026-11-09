#include "Generation/ArrowTowerGenerator.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"
#include "Generation/HutongShell.h"
#include "Generation/HutongJiajia.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"

using UE::Geometry::FDynamicMesh3;

namespace
{
	using namespace HutongMeshUtils;
	using namespace HutongGen;

	// One face's 箭窗 skin: a wall Recess deep over the core, the windows left open onto it. U along the face,
	// the skin between Outer and Inner across (either way round).
	void AppendArrowWindowSkin(FDynamicMesh3& Mesh, bool bAlongY, double U0, double U1, double Outer, double Inner,
		double Z0, double Z1, int32 Count, const TArray<double>& RowCentres, double Size)
	{
		const double C0 = FMath::Min(Outer, Inner), C1 = FMath::Max(Outer, Inner);
		auto Box = [&](double A0, double A1, double ZA, double ZB)
		{
			if (A1 - A0 < 0.1 || ZB - ZA < 0.1) return;
			if (bAlongY) AppendBox(Mesh, FVector3d(C0, A0, ZA), FVector3d(C1, A1, ZB));
			else AppendBox(Mesh, FVector3d(A0, C0, ZA), FVector3d(A1, C1, ZB));
		};
		const double L = U1 - U0;
		const double Half = 0.5 * FMath::Min(Size, Count > 0 ? 0.6 * L / Count : Size);
		double Z = Z0;
		for (const double Zc : RowCentres)
		{
			const double Za = Zc - Half, Zb = Zc + Half;
			if (Count <= 0 || Za <= Z || Zb >= Z1) continue;
			Box(U0, U1, Z, Za);
			double U = U0;
			for (int32 i = 0; i < Count; ++i)
			{
				const double Uc = U0 + (i + 0.5) * L / Count;
				Box(U, Uc - Half, Za, Zb);
				U = Uc + Half;
			}
			Box(U, U1, Za, Zb);
			Z = Zb;
		}
		Box(U0, U1, Z, Z1);
	}

	template <typename TSpec>
	void ArrowTowerFlare(TSpec& Spec, double W, double D)
	{
		Spec.FlareRise = 45.0;
		Spec.FlareRun = 35.0;
		Spec.FlareLength = 0.3 * FMath::Min(W, D);
		Spec.FasciaDrop = 22.0;
		Spec.TileRowSpacing = 22.0;
	}

	// Pre-bake normals point inward: up is Z <= -0.9.
	void ArrowTowerTagUp(FDynamicMesh3& Mesh, int32 FirstTri, int32 EndTri, int32 Slot)
	{
		FSlotScope::FlushCurrent(Mesh);
		UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs = Mesh.Attributes()->GetMaterialID();
		for (int32 T = FirstTri; T < EndTri; ++T)
		{
			if (Mesh.IsTriangle(T) && Mesh.GetTriNormal(T).Z <= -0.9) MatIDs->SetValue(T, Slot);
		}
	}
}

namespace HutongGen
{
	// A storey: a core painted where the windows show it, a skin on each face. Front on Y0.
	void AppendArrowWindowStorey(FDynamicMesh3& Mesh, double X0, double X1, double Y0, double Y1, double Z0, double Z1,
		int32 Front, int32 SideStart, int32 SideEnd, int32 Rear, const TArray<double>& Rows, double Size, double Recess, bool bWindows)
	{
		if (!bWindows)
		{
			FSlotScope Tag(Mesh, MatSlot_Body);
			AppendBox(Mesh, FVector3d(X0, Y0, Z0), FVector3d(X1, Y1, Z1));
			return;
		}
		{
			FSlotScope Tag(Mesh, MatSlot_DoorPaint);
			AppendBox(Mesh, FVector3d(X0 + Recess, Y0 + Recess, Z0), FVector3d(X1 - Recess, Y1 - Recess, Z1));
		}
		FSlotScope Tag(Mesh, MatSlot_Body);
		AppendArrowWindowSkin(Mesh, false, X0, X1, Y0, Y0 + Recess, Z0, Z1, Front, Rows, Size);
		AppendArrowWindowSkin(Mesh, false, X0, X1, Y1, Y1 - Recess, Z0, Z1, Rear, Rows, Size);
		AppendArrowWindowSkin(Mesh, true, Y0 + Recess, Y1 - Recess, X0, X0 + Recess, Z0, Z1, SideStart, Rows, Size);
		AppendArrowWindowSkin(Mesh, true, Y0 + Recess, Y1 - Recess, X1, X1 - Recess, Z0, Z1, SideEnd, Rows, Size);
	}


	void AppendTowerXieshanRoof(FDynamicMesh3& Mesh, double X0, double Y0, double W, double D, double EaveZ, double Overhang,
		bool bHipStart, bool bHipEnd)
	{
		const double O = Overhang;
		FSlotScope RoofTag(Mesh, MatSlot_Roof);
		FXieshanRoofSpec Xie;
		Xie.Width = W + 2.0 * O;
		Xie.Depth = D + 2.0 * O;
		Xie.Section = Jiajia::MakeSection(EHutongPurlins::Seven, 0.5 * D, O, 0.0);
		Xie.Rise = Xie.Section.Rise();
		Xie.ShouInset = 0.15 * Xie.Depth;
		Xie.EaveOverhang = O;
		Xie.bEaveCaps = true;
		Xie.RidgeWidth = FMath::Clamp(0.03 * Xie.Depth, 20.0, 40.0);
		Xie.RidgeHeight = FMath::Clamp(0.05 * Xie.Depth, 30.0, 70.0);
		Xie.BargeThickness = 8.0;
		Xie.BargeDepth = FMath::Clamp(0.04 * Xie.Depth, 25.0, 50.0);
		Xie.bHipStart = bHipStart;
		Xie.bHipEnd = bHipEnd;
		ArrowTowerFlare(Xie, Xie.Width, Xie.Depth);
		TArray<UE::Geometry::FIndex2i> Timber;
		AppendXieshanRoof(Mesh, FVector3d(X0 - O, Y0 - O, EaveZ), Xie, nullptr, &Timber);
		RoofTag.Close();
		for (const UE::Geometry::FIndex2i& R : Timber) SetMaterialIDForTriangleRange(Mesh, R.A, R.B, MatSlot_Wood);
	}

	void BuildArrowTower(FDynamicMesh3& Mesh, const FHutongArrowTowerParams& P, bool bWindows, bool bTileRuns)
	{
		using namespace HutongCanon::ArrowTower;
		const FHutongCityWallParams Wall = P.GetWall();
		const double Hp = Wall.GetHeight(), Bat = Wall.GetBatter(), B = Wall.GetBaseWidth();
		const double Wp = P.GetPlatformWidth(), Dp = P.GetPlatformDepth(), Yo = P.GetLegOuterY();
		const double Foot = bWindows ? Wall.GetFootingHeight() : 0.0;

		// The platform: battered all round, the barbican's legs abutting its sides between Yo and Yo + B, their
		// battered faces running on into it.
		auto Ring = [&](double Z)
		{
			const double I = Bat * Z / Hp;
			return TArray<FVector3d>{ {I, I, Z}, {Wp - I, I, Z}, {Wp - I, Yo + I, Z}, {Wp, Yo + I, Z}, {Wp, Yo + B - I, Z},
				{Wp - I, Yo + B - I, Z}, {Wp - I, Dp - I, Z}, {I, Dp - I, Z}, {I, Yo + B - I, Z}, {0.0, Yo + B - I, Z},
				{0.0, Yo + I, Z}, {I, Yo + I, Z} };
		};
		if (Foot > 1.0)
		{
			FSlotScope Tag(Mesh, MatSlot_Stone);
			AppendLoft(Mesh, Ring(0.0), Ring(Foot));
		}
		{
			FSlotScope Tag(Mesh, MatSlot_CityBrick);
			const int32 First = Mesh.MaxTriangleID();
			AppendLoft(Mesh, Ring(Foot), Ring(Hp));
			Tag.Close();
			ArrowTowerTagUp(Mesh, First, Mesh.MaxTriangleID(), MatSlot_Floor);
		}

		const double Wt = FMath::Min(P.TowerWidth, Wp - 2.0 * Bat), Dt = P.TowerDepth;
		const double X0 = 0.5 * (Wp - Wt), X1 = X0 + Wt;
		const double Y0 = P.GetTowerFrontY(), Y1 = Y0 + Dt;
		const double Zl = Hp + FMath::Max(P.LowerHeight, 200.0);
		const double Zu = Zl + FMath::Max(P.UpperHeight, 100.0);
		const double Ui = UpperInsetCm;
		const double Size = FMath::Max(P.WindowSize, 10.0);
		const int32 LowerRows = FMath::Max(P.LowerRows, 0);

		TArray<double> Lower;
		for (int32 r = 0; r < LowerRows; ++r) Lower.Add(Hp + (Zl - Hp) * (r + 0.6) / (LowerRows + 0.4));
		AppendArrowWindowStorey(Mesh, X0, X1, Y0, Y1, Hp, Zl, P.FrontWindows, P.SideWindows, P.SideWindows, P.bAnnex ? 0 : 3, Lower, Size,
			WindowRecessCm, bWindows);
		const TArray<double> Upper = { 0.5 * (Zl + Zu) };
		AppendArrowWindowStorey(Mesh, X0 + Ui, X1 - Ui, Y0 + Ui, Y1 - Ui, Zl, Zu, P.FrontWindows, P.SideWindows, P.SideWindows, 3, Upper,
			Size, WindowRecessCm, bWindows);

		FSlotScope RoofTag(Mesh, MatSlot_Roof);

		// 腰檐: a hipped skirt whose upper part stands inside the upper storey.
		{
			const double O = LowerEaveOverhangCm;
			FHipRoofSpec Hip;
			Hip.Width = Wt + 2.0 * O;
			Hip.Depth = Dt + 2.0 * O;
			Hip.RidgeLength = FMath::Max(Hip.Width - Hip.Depth, 0.0);
			Hip.Section = Jiajia::MakeSection(EHutongPurlins::Seven, 0.5 * Dt, O, 0.0);
			Hip.Rise = FMath::Min(Hip.Section.Rise(), FMath::Max(P.UpperHeight, 100.0) - 40.0);
			Hip.EaveOverhang = O;
			Hip.bEaveCaps = true;
			ArrowTowerFlare(Hip, Hip.Width, Hip.Depth);
			AppendHippedRoof(Mesh, FVector3d(X0 - O, Y0 - O, Zl), Hip);
		}

		// The upper roof: 歇山 over the upper storey.
		RoofTag.Close();
		AppendTowerXieshanRoof(Mesh, X0 + Ui, Y0 + Ui, Wt - 2.0 * Ui, Dt - 2.0 * Ui, Zu, UpperEaveOverhangCm);

		// 抱廈: a lower block against the back, its roof running into the tower's rear wall.
		if (P.bAnnex)
		{
			const double Wa = FMath::Min(P.AnnexWidth, Wt), Da = P.AnnexDepth;
			const double Za = Hp + FMath::Min(P.AnnexHeight, P.LowerHeight - 100.0);
			const double Ax0 = 0.5 * (Wp - Wa);
			{
				FSlotScope Tag(Mesh, MatSlot_Body);
				AppendBox(Mesh, FVector3d(Ax0, Y1 - 1.0, Hp), FVector3d(Ax0 + Wa, Y1 + Da, Za));
			}
			Shell::FRoofParams Roof;
			const double O = 90.0;
			Roof.FrontOverhang = O;
			Roof.RearOverhang = O;
			Roof.GableOverhang = 30.0;
			Roof.Section = Jiajia::MakeSection(EHutongPurlins::Five, 0.5 * Da, O, 0.0);
			Roof.Tile = EHutongRoofTile::Tong;
			Roof.bTileRuns = bTileRuns;
			Roof.RafterSection = 0.0;
			const int32 Mark = Mesh.MaxVertexID();
			Shell::AppendGableRoof(Mesh, Wa, Da, Za, Roof);
			TransformVerticesFrom(Mesh, Mark, FTransform(FVector(Ax0, Y1, 0.0)));
		}
	}

	void AppendCornerArrowTower(FDynamicMesh3& Mesh, double PlatformSize, double Margin, double Z, bool bWindows)
	{
		using namespace HutongCanon::ArrowTower;
		using namespace HutongCanon::CornerTower;
		const double M = Margin, Dt = DepthCm;
		const double La = FMath::Max(PlatformSize - 2.0 * M, 2.0 * Dt + 100.0);
		const double Zl = Z + LowerHeightCm, Zu = Zl + UpperHeightCm, Ui = UpperInsetCm;
		TArray<double> Lower;
		for (int32 r = 0; r < LowerRows; ++r) Lower.Add(Z + (Zl - Z) * (r + 0.6) / (LowerRows + 0.4));
		const TArray<double> Upper = { 0.5 * (Zl + Zu) };

		// One arm in its own frame, front on Y = 0 along +X, then turned into place. The arm through the corner
		// carries the corner square; the other stops at its back, its upper storey and roofs running on over it.
		auto Arm = [&](double Len, int32 Front, int32 SideStart, int32 SideEnd, bool bThroughCorner, double Lift, const FTransform& Place)
		{
			const int32 Mark = Mesh.MaxVertexID();
			AppendArrowWindowStorey(Mesh, 0.0, Len, 0.0, Dt, Z, Zl, Front, SideStart, SideEnd, 0, Lower, WindowSizeCm, WindowRecessCm, bWindows);
			const double UpperEnd = bThroughCorner ? Len - Ui : Len + Ui;
			AppendArrowWindowStorey(Mesh, Ui, UpperEnd, Ui, Dt - Ui, Zl, Zu, Front, SideStart, SideEnd, 0,
				Upper, WindowSizeCm, WindowRecessCm, bWindows);
			FSlotScope RoofTag(Mesh, MatSlot_Roof);
			auto Hipped = [&](double X0, double X1, double Y0, double Y1, double EaveZ, double O, double MaxRise)
			{
				FHipRoofSpec Hip;
				Hip.Width = (X1 - X0) + 2.0 * O;
				Hip.Depth = (Y1 - Y0) + 2.0 * O;
				Hip.RidgeLength = FMath::Max(Hip.Width - Hip.Depth, 0.0);
				Hip.Section = Jiajia::MakeSection(EHutongPurlins::Seven, 0.5 * (Y1 - Y0), O, 0.0);
				Hip.Rise = FMath::Min(Hip.Section.Rise(), MaxRise);
				Hip.EaveOverhang = O;
				Hip.bEaveCaps = true;
				ArrowTowerFlare(Hip, Hip.Width, Hip.Depth);
				AppendHippedRoof(Mesh, FVector3d(X0 - O, Y0 - O, EaveZ), Hip);
			};
			// Both arms' roofs run the whole L; one sits a centimetre over the other so their shared corner never fights.
			// The top roof hips at the corner and carries its 山花 at the arm's far end.
			const double RoofEnd = bThroughCorner ? Len : Len + Dt;
			Hipped(0.0, RoofEnd, 0.0, Dt, Zl + Lift, LowerEaveOverhangCm, UpperHeightCm - 40.0);
			RoofTag.Close();
			AppendTowerXieshanRoof(Mesh, Ui, Ui, RoofEnd - 2.0 * Ui, Dt - 2.0 * Ui, Zu + Lift, UpperEaveOverhangCm,
				bThroughCorner, !bThroughCorner);
			TransformVerticesFrom(Mesh, Mark, Place);
		};
		// Arm A along the Y = 0 face through the corner; arm B along the X = 0 face from A's back, a quarter turn.
		const int32 FrontA = FrontWindowsPerFace;
		const int32 FrontB = FMath::Max(1, FMath::RoundToInt32(FrontWindowsPerFace * (La - Dt) / La));
		Arm(La, FrontA, EndWindows, EndWindows, true, 0.0, FTransform(FVector(M, M, 0.0)));
		Arm(La - Dt, FrontB, EndWindows, 0, false, 1.0, FTransform(FRotator(0.0, -90.0, 0.0), FVector(M, M + La, 0.0)));
	}

	double PalaceCornerTowerSpan()
	{
		using namespace HutongCanon::PalaceCornerTower;
		return CoreCm + 2.0 * ArmProjectionCm;
	}

	void AppendPalaceCornerTower(FDynamicMesh3& Mesh, double Cx, double Cy, double Z, bool bDetail)
	{
		using namespace HutongCanon::PalaceCornerTower;
		const int32 First = Mesh.MaxTriangleID();
		const double C = 0.5 * CoreCm, Aw = 0.5 * ArmWidthCm, Ap = ArmProjectionCm, In = UpperInsetCm;
		const double Floor = Z + PlinthCm, Z1 = Floor + LowerStoreyCm, Z2 = Z1 + ArmUpperCm, Z3 = Z2 + CoreAboveArmsCm;

		auto Hipped = [&](double X0, double X1, double Y0, double Y1, double EaveZ, double O, double MaxRise)
		{
			FSlotScope Tag(Mesh, MatSlot_Roof);
			FHipRoofSpec Hip;
			Hip.Width = (X1 - X0) + 2.0 * O;
			Hip.Depth = (Y1 - Y0) + 2.0 * O;
			Hip.RidgeLength = FMath::Max(Hip.Width - Hip.Depth, 0.0);
			const double Half = 0.5 * FMath::Min(X1 - X0, Y1 - Y0);
			Hip.Section = Jiajia::MakeSection(EHutongPurlins::Five, Half, O, 0.0);
			Hip.Rise = FMath::Min(Hip.Section.Rise(), MaxRise);
			Hip.EaveOverhang = O;
			Hip.bEaveCaps = bDetail;
			ArrowTowerFlare(Hip, Hip.Width, Hip.Depth);
			AppendHippedRoof(Mesh, FVector3d(X0 - O, Y0 - O, EaveZ), Hip);
		};
		// About the tower's centre, a quarter at a time.
		auto TurnFrom = [&](int32 Mark, int32 Quarters)
		{
			const FTransform Turn = FTransform(FVector(-Cx, -Cy, 0.0)) * FTransform(FRotator(0.0, 90.0 * Quarters, 0.0)) * FTransform(FVector(Cx, Cy, 0.0));
			TransformVerticesFrom(Mesh, Mark, Turn);
		};

		// The core, its lower eave and its storeys.
		{
			FSlotScope Tag(Mesh, MatSlot_Stone);
			AppendBox(Mesh, FVector3d(Cx - C - 30.0, Cy - C - 30.0, Z), FVector3d(Cx + C + 30.0, Cy + C + 30.0, Floor));
		}
		{
			FSlotScope Tag(Mesh, MatSlot_RedWall);
			AppendBox(Mesh, FVector3d(Cx - C, Cy - C, Floor), FVector3d(Cx + C, Cy + C, Z1));
			AppendBox(Mesh, FVector3d(Cx - C + In, Cy - C + In, Z1), FVector3d(Cx + C - In, Cy + C - In, Z3));
		}
		Hipped(Cx - C, Cx + C, Cy - C, Cy + C, Z1, LowerEaveOverhangCm, Z3 - Z1 - 60.0);

		// Each 抱廈, built east and turned round: its plinth, lower storey and skirt, its upper storey and 歇山, the
		// inner ends buried in the core.
		for (int32 Q = 0; Q < 4; ++Q)
		{
			const int32 Mark = Mesh.MaxVertexID();
			const double X0 = Cx + C, X1 = Cx + C + Ap;
			{
				FSlotScope Tag(Mesh, MatSlot_Stone);
				AppendBox(Mesh, FVector3d(X0 + 30.0, Cy - Aw - 30.0, Z), FVector3d(X1 + 30.0, Cy + Aw + 30.0, Floor));
			}
			{
				FSlotScope Tag(Mesh, MatSlot_RedWall);
				AppendBox(Mesh, FVector3d(X0, Cy - Aw, Floor), FVector3d(X1, Cy + Aw, Z1));
				AppendBox(Mesh, FVector3d(X0 - In, Cy - Aw + In, Z1), FVector3d(X1 - In, Cy + Aw - In, Z2));
			}
			Hipped(Cx, X1, Cy - Aw, Cy + Aw, Z1, LowerEaveOverhangCm, ArmUpperCm - 40.0);
			AppendTowerXieshanRoof(Mesh, Cx, Cy - Aw + In, X1 - In - Cx, 2.0 * (Aw - In), Z2, ArmEaveOverhangCm, true, false);
			if (Q > 0) TurnFrom(Mark, Q);
		}

		// 十字脊: two 歇山 crossing over the core, the second a centimetre higher; a 寶頂 at the crossing.
		const double Top = CoreCm - 2.0 * In;
		AppendTowerXieshanRoof(Mesh, Cx - 0.5 * Top, Cy - 0.5 * Top, Top, Top, Z3, TopEaveOverhangCm);
		{
			const int32 Mark = Mesh.MaxVertexID();
			AppendTowerXieshanRoof(Mesh, Cx - 0.5 * Top, Cy - 0.5 * Top, Top, Top, Z3 + 1.0, TopEaveOverhangCm);
			TurnFrom(Mark, 1);
		}
		{
			const double O = TopEaveOverhangCm;
			const double Ridge = Z3 + Jiajia::MakeSection(EHutongPurlins::Seven, 0.5 * Top, O, 0.0).Rise();
			FSlotScope Tag(Mesh, MatSlot_Finial);
			AppendCylinder(Mesh, FVector3d(Cx, Cy, Ridge - 10.0), FinialRadiusCm, FinialHeightCm, bDetail ? 16 : 8);
		}

		// Yellow glazed throughout.
		FSlotScope::FlushCurrent(Mesh);
		UE::Geometry::FDynamicMeshMaterialAttribute* MatIDs = Mesh.Attributes()->GetMaterialID();
		for (int32 T = First; T < Mesh.MaxTriangleID(); ++T)
		{
			if (!Mesh.IsTriangle(T)) continue;
			const int32 Slot = MatIDs->GetValue(T);
			if (Slot == MatSlot_Roof || Slot == MatSlot_Ridge || Slot == MatSlot_Finial) MatIDs->SetValue(T, int32(MatSlot_GlazedTile));
		}
	}
}
