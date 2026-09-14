#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Templates/Function.h"

namespace HutongGen
{
	// The street facade a shop presents: 排板門 dropped into the bays that are shut and a 櫃檯
	// across the ones that are open. Shared by the 鋪面房 and the 樓 over it — the shop below a
	// two-storey building is the same shop, and a second copy of this loop is how the two would
	// start disagreeing about what a boarded bay looks like.
	namespace ShopBay
	{
		struct FFront
		{
			// The facade plane the boards stand in; the counter straddles it.
			double FaceY = 0.0;

			double BoardWidth = 26.0;
			double BoardThickness = 5.0;

			double SillZ = 0.0;
			double HeadZ = 300.0;

			// Counted outward from the middle bay; zero boards the whole front up.
			int32 OpenBayCount = 1;

			bool bHasCounter = true;
			double CounterHeight = 88.0;
			double CounterDepth = 52.0;
		};

		// Which bays are open, as the half-open range the boards skip.
		void OpenBayRange(int32 BayCount, int32 OpenBayCount, int32& OutLo, int32& OutHi);

		// Boards and counters across every bay. Material tagging is the caller's: this is woodwork
		// in the middle of a facade that has masonry on either side of it.
		void AppendFront(
			UE::Geometry::FDynamicMesh3& Mesh,
			const TFunctionRef<double(int32)>& BoundaryX,
			int32 BayCount, double ColumnRadius, const FFront& F);
	}
}
