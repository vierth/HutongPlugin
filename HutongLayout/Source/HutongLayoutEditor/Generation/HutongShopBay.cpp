#include "Generation/HutongShopBay.h"
#include "Generation/HutongMeshUtils.h"

using UE::Geometry::FDynamicMesh3;

namespace HutongGen
{
namespace ShopBay
{

void OpenBayRange(int32 BayCount, int32 OpenBayCount, int32& OutLo, int32& OutHi)
{
	const int32 Open = FMath::Clamp(OpenBayCount, 0, BayCount);
	const int32 Mid = BayCount / 2;
	// Counted outward from the middle, so a half-open shop opens at its centre.
	OutLo = Mid - (Open - 1) / 2;
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

		if (bAnyOpen && i >= OpenLo && i <= OpenHi)
		{
			// 櫃檯 across the mouth of the bay.
			if (F.bHasCounter)
			{
				const double CH = FMath::Clamp(F.CounterHeight, 20.0, F.HeadZ - F.SillZ - 30.0);
				const double CD = FMath::Max(F.CounterDepth, 10.0);
				AppendBox(Mesh,
					FVector3d(X0, F.FaceY - 0.55 * CD, F.SillZ),
					FVector3d(X1, F.FaceY + 0.45 * CD, F.SillZ + CH));
			}
			continue;
		}

		// 排板門: loose boards dropped into a grooved sill, every second one set a little deeper —
		// the stagger is what reads as boards rather than a panel.
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
}

} // namespace ShopBay
} // namespace HutongGen
