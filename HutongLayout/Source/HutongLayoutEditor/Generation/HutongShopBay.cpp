#include "Generation/HutongShopBay.h"
#include "Generation/HutongMeshUtils.h"
#include "Generation/HutongPalette.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
namespace ShopBay
{

void OpenBayRange(int32 BayCount, int32 OpenBayCount, int32& OutLo, int32& OutHi)
{
	const int32 Open = FMath::Clamp(OpenBayCount, 0, BayCount);
	const int32 Mid = BayCount / 2;
	// Outward from the middle, so a half-open shop opens at its centre; held inside the frontage (an even
	// count centred on the upper middle bay ran one bay off the end and left bay 0 boarded).
	OutLo = FMath::Clamp(Mid - (Open - 1) / 2, 0, FMath::Max(BayCount - Open, 0));
	OutHi = OutLo + Open - 1;
}

void AppendFront(FDynamicMesh3& Mesh, const TFunctionRef<double(int32)>& BoundaryX,
	int32 BayCount, double ColumnRadius, const FFront& F)
{
	using namespace HutongMeshUtils;

	const double BoardT = FMath::Max(F.BoardThickness, 1.0);
	const double BoardW = FMath::Max(F.BoardWidth, 6.0);
	if (F.HeadZ - F.SillZ < 10.0) return;

	int32 OpenLo = 0, OpenHi = -1;
	OpenBayRange(BayCount, F.OpenBayCount, OpenLo, OpenHi);
	const bool bAnyOpen = FMath::Clamp(F.OpenBayCount, 0, BayCount) > 0;

	for (int32 i = 0; i < BayCount; ++i)
	{
		const double X0 = BoundaryX(i) + 0.5 * ColumnRadius;
		const double X1 = BoundaryX(i + 1) - 0.5 * ColumnRadius;
		if (X1 - X0 < 2.0 * BoardW) continue;

		if (bAnyOpen && i >= OpenLo && i <= OpenHi) continue;

		// 排板門: loose boards in a grooved sill, every second set deeper; the stagger reads as boards, not a panel.
		// Lacquered like door leaves, not the columns' wood.
		FSlotScope BoardTag(Mesh, MatSlot_DoorPaint);
		const double Span = X1 - X0;
		const int32 Boards = FMath::Max(FMath::RoundToInt32(Span / BoardW), 2);
		for (int32 b = 0; b < Boards; ++b)
		{
			const double BX0 = X0 + Span * b / double(Boards);
			const double BX1 = X0 + Span * (b + 1) / double(Boards);
			const double Back = (b % 2 == 0) ? 0.0 : 0.35 * BoardT;
			AppendBox(Mesh,
				FVector3d(BX0, F.FaceY + Back,          F.SillZ),
				FVector3d(BX1, F.FaceY + Back + BoardT, F.HeadZ));
		}
	}

	// 櫃檯 over the open bays as one run, the way in left at the low-X end.
	if (!bAnyOpen || F.Counter == FFront::ECounter::None) return;
	const double OX0 = BoundaryX(OpenLo) + 0.5 * ColumnRadius;
	const double OX1 = BoundaryX(OpenHi + 1) - 0.5 * ColumnRadius;
	const double Span = OX1 - OX0;
	const double CH = FMath::Clamp(F.CounterHeight, 20.0, F.HeadZ - F.SillZ - 30.0);
	const double CD = FMath::Max(F.CounterDepth, 10.0);
	const bool bArm = Span >= WalkGapCm + 40.0;
	if (F.Counter == FFront::ECounter::Street)
	{
		if (bArm)
		{
			AppendBox(Mesh, FVector3d(OX0 + WalkGapCm, F.FaceY - 0.55 * CD, F.SillZ),
				FVector3d(OX1, F.FaceY + 0.45 * CD, F.SillZ + CH));
		}
		return;
	}

	// 曲尺櫃台: an arm parallel to the front behind the customers' floor, a return running back along the
	// high-X side, the shopkeeper's side behind both, reached past the arm's open end.
	const double Y0 = F.FaceY + InsideSetbackCm;
	const double Back = F.FaceY + F.InsideDepth - WalkGapCm;
	if (Y0 + CD > Back) return;
	if (bArm) AppendBox(Mesh, FVector3d(OX0 + WalkGapCm, Y0, F.SillZ), FVector3d(OX1, Y0 + CD, F.SillZ + CH));
	const double ReturnEnd = FMath::Min(Y0 + CD + FMath::Clamp(0.5 * Span, 60.0, 200.0), Back);
	if (ReturnEnd - (Y0 + CD) >= 40.0)
	{
		AppendBox(Mesh, FVector3d(OX1 - CD, Y0 + CD, F.SillZ), FVector3d(OX1, ReturnEnd, F.SillZ + CH));
	}
}

} // namespace ShopBay
} // namespace HutongGen
