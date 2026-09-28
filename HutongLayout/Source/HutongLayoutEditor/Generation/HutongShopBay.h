#pragma once

#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "Templates/Function.h"

namespace HutongGen
{
	// Shop street facade: 排板門 in shut bays, 櫃檯 across open ones. Shared by 鋪面房 and 樓 so the
	// two never disagree on a boarded bay.
	namespace ShopBay
	{
		struct FFront
		{
			// Facade plane of the boards; the counter straddles it.
			double FaceY = 0.0;

			double BoardWidth = 26.0;
			double BoardThickness = 5.0;

			double SillZ = 0.0;
			double HeadZ = 300.0;

			// Counted outward from the middle bay; zero boards up the whole front.
			int32 OpenBayCount = 1;

			bool bHasCounter = true;
			double CounterHeight = 88.0;
			double CounterDepth = 52.0;
		};

		// Open bays as the half-open range the boards skip.
		void OpenBayRange(int32 BayCount, int32 OpenBayCount, int32& OutLo, int32& OutHi);

		// Boards and counters across every bay. Caller tags materials: woodwork amid masonry.
		void AppendFront(
			UE::Geometry::FDynamicMesh3& Mesh,
			const TFunctionRef<double(int32)>& BoundaryX,
			int32 BayCount, double ColumnRadius, const FFront& F);
	}
}
